#pragma once
#ifdef ENABLE_VIDEO

#include <Shared/Shared.hpp>
#include <Shared/Thread.hpp>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

namespace Graphics
{
	// Ring buffer for decoded video frames
	struct VideoFrame
	{
		Vector<uint8> data; // RGBA pixel data
		double pts = 0.0;   // Presentation timestamp in seconds
	};

	// Ring buffer for decoded audio samples
	struct AudioChunk
	{
		Vector<float> samples; // Interleaved stereo float samples
		double pts = 0.0;
	};

	class VideoDecoder
	{
	public:
		VideoDecoder();
		~VideoDecoder();

		bool Open(const String& path);
		void Close();

		// Video info
		int GetWidth() const { return m_width; }
		int GetHeight() const { return m_height; }
		double GetDuration() const { return m_duration; }
		double GetFrameRate() const { return m_frameRate; }
		bool IsOpen() const { return m_formatCtx != nullptr; }
		bool IsEof() const { return m_eof.load(); }
		bool HasAudioTrack() const { return m_audioStreamIdx >= 0; }
		int GetAudioSampleRate() const { return m_audioSampleRate; }
		int GetAudioChannels() const { return m_audioChannels; }

		// Seek to a position in seconds
		bool Seek(double seconds);

		// Get next decoded video frame (returns false if none ready)
		bool GetVideoFrame(VideoFrame& out);
		bool HasPendingVideoFrames() const;

		// Get decoded audio samples (returns number of samples written)
		int GetAudioSamples(float* buffer, int maxSamples);

		// Decode thread entry point
		void DecodeLoop();

		// Control
		void StartDecoding();
		void StopDecoding();
		bool IsDecoding() const { return m_decoding.load(); }

	private:
		bool m_DecodePacket(AVPacket* packet);
		void m_FlushBuffers();

		// FFmpeg contexts
		AVFormatContext* m_formatCtx = nullptr;
		AVCodecContext* m_videoCodecCtx = nullptr;
		AVCodecContext* m_audioCodecCtx = nullptr;
		SwsContext* m_swsCtx = nullptr;
		SwrContext* m_swrCtx = nullptr;

		// Stream indices
		int m_videoStreamIdx = -1;
		int m_audioStreamIdx = -1;

		// Video info
		int m_width = 0;
		int m_height = 0;
		double m_duration = 0.0;
		double m_frameRate = 30.0;

		// Audio info
		int m_audioSampleRate = 44100;
		int m_audioChannels = 2;

		// Decoded frame ring buffers (protected by mutex)
		static constexpr int MAX_VIDEO_FRAMES = 4;
		static constexpr int MAX_AUDIO_CHUNKS = 8;

		mutable std::mutex m_videoMutex;
		std::deque<VideoFrame> m_videoFrames;

		std::mutex m_audioMutex;
		std::deque<AudioChunk> m_audioChunks;
		double m_lastVideoPts = 0.0;
		double m_lastAudioPts = 0.0;

		// Decode thread control
		Thread* m_decodeThread = nullptr;
		std::atomic<bool> m_decoding{false};
		std::atomic<bool> m_eof{false};
		std::atomic<bool> m_seekRequested{false};
		std::atomic<double> m_seekTarget{0.0};
		std::mutex m_decodeMutex;
		std::condition_variable m_decodeCV;
	};
}

#endif // ENABLE_VIDEO
