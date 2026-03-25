#include "stdafx.h"
#ifdef ENABLE_VIDEO
#include "Graphics/VideoDecoder.hpp"

namespace Graphics
{
	namespace
	{
		double ClampNonNegative(double value)
		{
			return value < 0.0 ? 0.0 : value;
		}
	}

	VideoDecoder::VideoDecoder()
	{
	}

	VideoDecoder::~VideoDecoder()
	{
		Close();
	}

	bool VideoDecoder::Open(const String& path)
	{
		Close();
		m_eof.store(false);
		m_seekRequested.store(false);
		m_seekTarget.store(0.0);
		m_lastVideoPts = 0.0;
		m_lastAudioPts = 0.0;

		// Open input file
		if (avformat_open_input(&m_formatCtx, path.c_str(), nullptr, nullptr) < 0)
		{
			Logf("VideoDecoder: Failed to open '%s'", Logger::Severity::Error, path);
			return false;
		}

		if (avformat_find_stream_info(m_formatCtx, nullptr) < 0)
		{
			Logf("VideoDecoder: Failed to find stream info for '%s'", Logger::Severity::Error, path);
			Close();
			return false;
		}

		// Find video/audio streams
		m_videoStreamIdx = -1;
		m_audioStreamIdx = -1;
		for (unsigned int i = 0; i < m_formatCtx->nb_streams; i++)
		{
			if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && m_videoStreamIdx < 0)
				m_videoStreamIdx = i;
			else if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && m_audioStreamIdx < 0)
				m_audioStreamIdx = i;
		}

		if (m_videoStreamIdx < 0)
		{
			Logf("VideoDecoder: No video stream found in '%s'", Logger::Severity::Error, path);
			Close();
			return false;
		}

		// Open video codec
		{
			AVCodecParameters* codecpar = m_formatCtx->streams[m_videoStreamIdx]->codecpar;
			const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
			if (!codec)
			{
				Logf("VideoDecoder: Unsupported video codec", Logger::Severity::Error);
				Close();
				return false;
			}

			m_videoCodecCtx = avcodec_alloc_context3(codec);
			avcodec_parameters_to_context(m_videoCodecCtx, codecpar);

			if (avcodec_open2(m_videoCodecCtx, codec, nullptr) < 0)
			{
				Logf("VideoDecoder: Failed to open video codec", Logger::Severity::Error);
				Close();
				return false;
			}

			m_width = m_videoCodecCtx->width;
			m_height = m_videoCodecCtx->height;

			AVRational framerate = av_guess_frame_rate(m_formatCtx, m_formatCtx->streams[m_videoStreamIdx], nullptr);
			if (framerate.num > 0 && framerate.den > 0)
				m_frameRate = av_q2d(framerate);
			else
				m_frameRate = 30.0;

			// Create scaler to convert to RGBA
			m_swsCtx = sws_getContext(
				m_width, m_height, m_videoCodecCtx->pix_fmt,
				m_width, m_height, AV_PIX_FMT_RGBA,
				SWS_BILINEAR, nullptr, nullptr, nullptr
			);

			if (!m_swsCtx)
			{
				Logf("VideoDecoder: Failed to create scaler context", Logger::Severity::Error);
				Close();
				return false;
			}
		}

		// Audio decoding is intentionally disabled in the current video-only milestone.
		// This keeps playback silent and avoids touching stream-specific audio conversion paths.
		m_audioStreamIdx = -1;
		m_audioSampleRate = 44100;
		m_audioChannels = 2;

		if (m_formatCtx->duration != AV_NOPTS_VALUE)
			m_duration = m_formatCtx->duration / (double)AV_TIME_BASE;
		else
			m_duration = 0.0;

		return true;
	}

	void VideoDecoder::Close()
	{
		StopDecoding();

		if (m_swsCtx)
		{
			sws_freeContext(m_swsCtx);
			m_swsCtx = nullptr;
		}
		if (m_swrCtx)
		{
			swr_free(&m_swrCtx);
		}
		if (m_videoCodecCtx)
		{
			avcodec_free_context(&m_videoCodecCtx);
		}
		if (m_audioCodecCtx)
		{
			avcodec_free_context(&m_audioCodecCtx);
		}
		if (m_formatCtx)
		{
			avformat_close_input(&m_formatCtx);
		}

		m_videoStreamIdx = -1;
		m_audioStreamIdx = -1;
		m_width = 0;
		m_height = 0;
		m_duration = 0.0;
		m_lastVideoPts = 0.0;
		m_lastAudioPts = 0.0;
		m_eof.store(false);
		m_seekRequested.store(false);
		m_seekTarget.store(0.0);

		{
			std::lock_guard<std::mutex> lock(m_videoMutex);
			m_videoFrames.clear();
		}
		{
			std::lock_guard<std::mutex> lock(m_audioMutex);
			m_audioChunks.clear();
		}
	}

	bool VideoDecoder::Seek(double seconds)
	{
		if (!m_formatCtx || m_videoStreamIdx < 0)
			return false;

		m_seekTarget.store(seconds);
		m_seekRequested.store(true);
		m_decodeCV.notify_one();
		return true;
	}

	bool VideoDecoder::GetVideoFrame(VideoFrame& out)
	{
		std::lock_guard<std::mutex> lock(m_videoMutex);
		if (m_videoFrames.empty())
			return false;

		out = std::move(m_videoFrames.front());
		m_videoFrames.pop_front();
		m_decodeCV.notify_one();
		return true;
	}

	bool VideoDecoder::HasPendingVideoFrames() const
	{
		std::lock_guard<std::mutex> lock(m_videoMutex);
		return !m_videoFrames.empty();
	}

	int VideoDecoder::GetAudioSamples(float* buffer, int maxSamples)
	{
		std::lock_guard<std::mutex> lock(m_audioMutex);
		int written = 0;

		while (written < maxSamples && !m_audioChunks.empty())
		{
			AudioChunk& chunk = m_audioChunks.front();
			int available = (int)chunk.samples.size();
			int toWrite = std::min(available, maxSamples - written);
			memcpy(buffer + written, chunk.samples.data(), toWrite * sizeof(float));
			written += toWrite;

			if (toWrite < available)
			{
				chunk.samples.erase(chunk.samples.begin(), chunk.samples.begin() + toWrite);
			}
			else
			{
				m_audioChunks.pop_front();
			}
		}

		if (written > 0)
			m_decodeCV.notify_one();

		return written;
	}

	void VideoDecoder::StartDecoding()
	{
		if (m_decoding.load() || !m_formatCtx || !m_videoCodecCtx)
			return;

		m_decoding.store(true);
		m_eof.store(false);
		m_seekRequested.store(false);
		m_decodeThread = new Thread([this]() { DecodeLoop(); });
	}

	void VideoDecoder::StopDecoding()
	{
		if (!m_decoding.load())
			return;

		m_decoding.store(false);
		m_decodeCV.notify_all();

		if (m_decodeThread && m_decodeThread->joinable())
			m_decodeThread->join();

		delete m_decodeThread;
		m_decodeThread = nullptr;
	}

	bool VideoDecoder::m_DecodePacket(AVPacket* packet)
	{
		if (packet->stream_index == m_videoStreamIdx)
		{
			int ret = avcodec_send_packet(m_videoCodecCtx, packet);
			if (ret < 0 && ret != AVERROR(EAGAIN))
				return false;

			AVFrame* frame = av_frame_alloc();
			while (true)
			{
				ret = avcodec_receive_frame(m_videoCodecCtx, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
					break;
				if (ret < 0)
					break;

				VideoFrame vf;
				vf.data.resize(m_width * m_height * 4);

				uint8_t* dstData[1] = { vf.data.data() };
				int dstLinesize[1] = { m_width * 4 };

				sws_scale(m_swsCtx, frame->data, frame->linesize, 0, m_height, dstData, dstLinesize);

				const AVStream* stream = m_formatCtx->streams[m_videoStreamIdx];
				const int64_t bestPts = (frame->best_effort_timestamp != AV_NOPTS_VALUE)
					? frame->best_effort_timestamp
					: frame->pts;
				if (bestPts != AV_NOPTS_VALUE)
					vf.pts = bestPts * av_q2d(stream->time_base);
				else
					vf.pts = m_lastVideoPts + (1.0 / std::max(1.0, m_frameRate));

				vf.pts = ClampNonNegative(vf.pts);
				m_lastVideoPts = vf.pts;

				std::lock_guard<std::mutex> lock(m_videoMutex);
				while ((int)m_videoFrames.size() >= MAX_VIDEO_FRAMES)
				{
					m_videoFrames.pop_front();
				}
				m_videoFrames.push_back(std::move(vf));
			}
			av_frame_free(&frame);
			return true;
		}

		return false;
	}

	void VideoDecoder::m_FlushBuffers()
	{
		{
			std::lock_guard<std::mutex> lock(m_videoMutex);
			m_videoFrames.clear();
		}
		{
			std::lock_guard<std::mutex> lock(m_audioMutex);
			m_audioChunks.clear();
		}

		m_lastVideoPts = 0.0;
		m_lastAudioPts = 0.0;

		if (m_videoCodecCtx)
			avcodec_flush_buffers(m_videoCodecCtx);
		if (m_audioCodecCtx)
			avcodec_flush_buffers(m_audioCodecCtx);
	}

	void VideoDecoder::DecodeLoop()
	{
		AVPacket* packet = av_packet_alloc();
		if (!packet)
			return;

		while (m_decoding.load())
		{
			if (!m_formatCtx || !m_videoCodecCtx)
				break;

			if (m_seekRequested.exchange(false))
			{
				double target = ClampNonNegative(m_seekTarget.load());
				AVRational tb = m_formatCtx->streams[m_videoStreamIdx]->time_base;
				int64_t ts = av_rescale_q((int64_t)(target * AV_TIME_BASE), AV_TIME_BASE_Q, tb);
				if (av_seek_frame(m_formatCtx, m_videoStreamIdx, ts, AVSEEK_FLAG_BACKWARD) >= 0)
				{
					m_FlushBuffers();
					m_lastVideoPts = target;
					m_lastAudioPts = target;
					m_eof.store(false);
				}
			}

			bool videoFull = false;
			{
				std::lock_guard<std::mutex> lock(m_videoMutex);
				videoFull = (int)m_videoFrames.size() >= MAX_VIDEO_FRAMES;
			}
			if (videoFull)
			{
				std::unique_lock<std::mutex> waitLock(m_decodeMutex);
				m_decodeCV.wait_for(waitLock, std::chrono::milliseconds(5));
				continue;
			}

			av_packet_unref(packet);
			int ret = av_read_frame(m_formatCtx, packet);
			if (ret < 0)
			{
				if (ret == AVERROR_EOF)
				{
					m_eof.store(true);
					std::unique_lock<std::mutex> waitLock(m_decodeMutex);
					m_decodeCV.wait_for(waitLock, std::chrono::milliseconds(10));
				}
				else
				{
					std::unique_lock<std::mutex> waitLock(m_decodeMutex);
					m_decodeCV.wait_for(waitLock, std::chrono::milliseconds(2));
				}
				continue;
			}

			m_eof.store(false);
			m_DecodePacket(packet);
		}

		av_packet_unref(packet);
		av_packet_free(&packet);
	}
}

#endif // ENABLE_VIDEO
