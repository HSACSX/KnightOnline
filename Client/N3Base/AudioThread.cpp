#include "StdAfxBase.h"
#include "AudioThread.h"
#include "AudioHandle.h"
#include "al_wrapper.h"

#include <cassert>
#include <unordered_map>

using namespace std::chrono_literals;

void AudioThread::thread_loop()
{
	std::vector<QueueType> pendingQueue;
	std::unordered_map<uint32_t, std::shared_ptr<AudioHandle>> handles;

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

			pendingQueue.swap(_pendingQueue);
		}

		// Process queue
		if (!pendingQueue.empty())
		{
			for (auto& [type, handle, callback] : pendingQueue)
			{
				switch (type)
				{
					case AUDIO_QUEUE_ADD:
						reset(handle.get());
						handles[handle->SourceId] = std::move(handle);
						break;

					case AUDIO_QUEUE_CALLBACK:
						if (callback != nullptr)
							callback(handle.get());
						break;

					case AUDIO_QUEUE_REMOVE:
						alSourceStop(handle->SourceId);
						AL_CLEAR_ERROR_STATE();

						handles.erase(handle->SourceId);
						break;
				}
			}

			pendingQueue.clear();
		}
		
		for (auto& [_, handle] : handles)
			tick(handle.get());
	}
}

void AudioThread::Add(std::shared_ptr<AudioHandle> handle)
{
	assert(handle != nullptr);
	assert(handle->SourceId != INVALID_SOURCE_ID);

	std::lock_guard<std::mutex> lock(_mutex);
	_pendingQueue.push_back(std::make_tuple(AUDIO_QUEUE_ADD, std::move(handle), nullptr));
}

void AudioThread::QueueCallback(std::shared_ptr<AudioHandle> handle, AudioCallback callback)
{
	assert(handle != nullptr);
	assert(handle->SourceId != INVALID_SOURCE_ID);

	std::lock_guard<std::mutex> lock(_mutex);
	_pendingQueue.push_back(std::make_tuple(AUDIO_QUEUE_CALLBACK, std::move(handle), std::move(callback)));
}

void AudioThread::Remove(std::shared_ptr<AudioHandle> handle)
{
	assert(handle != nullptr);
	assert(handle->SourceId != INVALID_SOURCE_ID);

	std::lock_guard<std::mutex> lock(_mutex);
	_pendingQueue.push_back(std::make_tuple(AUDIO_QUEUE_REMOVE, std::move(handle), nullptr));
}

void AudioThread::reset(AudioHandle* handle)
{
	// TODO
	alSourceRewind(handle->SourceId);

	handle->StartedPlaying = false;
	handle->FinishedPlaying = false;

	// ResetStream(&stream, true);
}

void AudioThread::tick(AudioHandle* handle)
{
	if (handle->StartedPlaying
		&& !handle->FinishedPlaying)
	{
		ALint state = AL_INITIAL;
		alGetSourcei(handle->SourceId, AL_SOURCE_STATE, &state);
		if (!AL_CHECK_ERROR()
			&& state != AL_PLAYING)
			handle->FinishedPlaying = true;
	}
}
