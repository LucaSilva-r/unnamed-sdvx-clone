#include "stdafx.h"

#ifdef ENABLE_VIDEO
#include <Graphics/VideoDecoder.hpp>
#include <thread>
#include <chrono>
#endif

Test("VideoDecoder.StressOpenSeekClose")
{
#ifdef ENABLE_VIDEO
	const String videoPath = Path::Absolute("skins/LiqidWave-1.5.0/video/song_select.wmv");
	TestEnsure(Path::FileExists(videoPath));

	constexpr int kIterations = 30;
	for (int i = 0; i < kIterations; i++)
	{
		Graphics::VideoDecoder decoder;
		TestEnsure(decoder.Open(videoPath));
		TestEnsure(decoder.GetWidth() > 0);
		TestEnsure(decoder.GetHeight() > 0);

		decoder.StartDecoding();

		Graphics::VideoFrame frame;
		bool gotFrame = false;
		for (int poll = 0; poll < 600; poll++)
		{
			if (decoder.GetVideoFrame(frame))
			{
				gotFrame = true;
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		TestEnsure(gotFrame);
		TestEnsure(!frame.data.empty());

		const double duration = decoder.GetDuration();
		const double seekTarget = (duration > 0.5) ? (duration * 0.5) : 0.0;
		TestEnsure(decoder.Seek(seekTarget));

		bool gotSeekFrame = false;
		for (int poll = 0; poll < 600; poll++)
		{
			if (decoder.GetVideoFrame(frame))
			{
				gotSeekFrame = true;
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		TestEnsure(gotSeekFrame);
		TestEnsure(!frame.data.empty());

		decoder.StopDecoding();
		decoder.Close();
	}
#else
	TestEnsure(true);
#endif
}
