#include "StdAfxBase.h"
#include "AudioDecoderThread.h"

using namespace std::chrono_literals;

void AudioDecoderThread::thread_loop()
{
	while (_canTick)
	{
		{
			std::unique_lock<std::mutex> lock(_mutex);
			std::cv_status status = _cv.wait_for(lock, 20ms);

			if (!_canTick)
				break;

			// Ignore spurious wakeups
			if (status != std::cv_status::timeout)
				continue;
		}
	}
}
