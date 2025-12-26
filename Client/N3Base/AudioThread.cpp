#include "StdAfxBase.h"
#include "AudioThread.h"
#include "AudioDecoderThread.h"
#include "AudioAsset.h"
#include "AudioHandle.h"
#include "al_wrapper.h"

#include <cassert>
#include <unordered_map>

using namespace std::chrono_literals;

AudioThread::AudioThread()
{
}

AudioThread::~AudioThread()
{
}

void AudioThread::thread_loop()
{
	std::vector<QueueType> pendingQueue;
	std::unordered_map<uint32_t, std::shared_ptr<AudioHandle>> handleMap;
	StreamedAudioHandle::DecodedChunk tmpDecodedChunk; // keep a copy here so we aren't constantly reallocating

	_decoderThread = std::make_unique<AudioDecoderThread>();
	_decoderThread->start();

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
						reset(handle);
						handleMap[handle->SourceId] = std::move(handle);
						break;

					case AUDIO_QUEUE_CALLBACK:
						if (callback != nullptr)
							callback(handle.get());
						break;

					case AUDIO_QUEUE_REMOVE:
						if (handle->HandleType == AUDIO_HANDLE_STREAMED)
						{
							auto streamedAudioHandle = std::static_pointer_cast<StreamedAudioHandle>(handle);
							if (streamedAudioHandle != nullptr)
								_decoderThread->Remove(streamedAudioHandle);
						}

						alSourceStop(handle->SourceId);
						AL_CLEAR_ERROR_STATE();

						handleMap.erase(handle->SourceId);
						break;
				}
			}

			pendingQueue.clear();
		}
		
		for (auto& [_, handle] : handleMap)
			tick(handle, tmpDecodedChunk);
	}

	_decoderThread->shutdown();
	_decoderThread.reset();
}

void AudioThread::Add(std::shared_ptr<AudioHandle> handle)
{
	assert(handle != nullptr);
	assert(handle->SourceId != INVALID_SOURCE_ID);

	std::scoped_lock<std::mutex> lock(_mutex);
	_pendingQueue.push_back(std::make_tuple(AUDIO_QUEUE_ADD, std::move(handle), nullptr));
}

void AudioThread::QueueCallback(std::shared_ptr<AudioHandle> handle, AudioCallback callback)
{
	assert(handle != nullptr);
	assert(handle->SourceId != INVALID_SOURCE_ID);

	std::scoped_lock<std::mutex> lock(_mutex);
	_pendingQueue.push_back(std::make_tuple(AUDIO_QUEUE_CALLBACK, std::move(handle), std::move(callback)));
}

void AudioThread::Remove(std::shared_ptr<AudioHandle> handle)
{
	assert(handle != nullptr);
	assert(handle->SourceId != INVALID_SOURCE_ID);

	std::scoped_lock<std::mutex> lock(_mutex);
	_pendingQueue.push_back(std::make_tuple(AUDIO_QUEUE_REMOVE, std::move(handle), nullptr));
}

void AudioThread::reset(std::shared_ptr<AudioHandle>& handle)
{
	alSourceRewind(handle->SourceId);
	AL_CLEAR_ERROR_STATE();

	handle->StartedPlaying = false;
	handle->FinishedPlaying = false;

	if (handle->HandleType == AUDIO_HANDLE_STREAMED)
	{
		constexpr auto BUFFER_COUNT			= StreamedAudioHandle::BUFFER_COUNT;
		constexpr auto INVALID_BUFFER_ID	= StreamedAudioHandle::INVALID_BUFFER_ID;

		auto streamedAudioHandle = std::static_pointer_cast<StreamedAudioHandle>(handle);
		if (streamedAudioHandle != nullptr)
		{
			// Initialize the buffers
			for (size_t i = 0; i < BUFFER_COUNT; i++)
			{
				ALuint bufferId = INVALID_BUFFER_ID;

				alGenBuffers(1, &bufferId);
				if (!AL_CHECK_ERROR()
					&& bufferId != INVALID_BUFFER_ID)
					streamedAudioHandle->BufferIds.push(bufferId);
			}

			// Force initial decode now, so we can safely start playing.
			_decoderThread->InitialDecode(streamedAudioHandle.get());

			// Add to decoder for future decodes.
			_decoderThread->Add(std::move(streamedAudioHandle));

			// Force an initial tick to push our decoded data before play is triggered.
			StreamedAudioHandle::DecodedChunk tmpDecodedChunk = {};
			tick(handle, tmpDecodedChunk);
		}
	}
}

void AudioThread::tick(std::shared_ptr<AudioHandle>& handle, StreamedAudioHandle::DecodedChunk& tmpDecodedChunk)
{
	// Process streamed audio
	if (handle->HandleType == AUDIO_HANDLE_STREAMED)
	{
		StreamedAudioHandle* streamedAudioHandle = static_cast<StreamedAudioHandle*>(handle.get());

		// If any buffers have been processed, we should remove them and attempt to replace them
		// with any existing decoded chunks.
		ALint buffersProcessed = 0;
		alGetSourcei(handle->SourceId, AL_BUFFERS_PROCESSED, &buffersProcessed);

		if (!AL_CHECK_ERROR()
			&& buffersProcessed > 0)
		{
			// Unqueue the processed buffers so we can reuse their IDs for our new chunks
			for (int i = 0; i < buffersProcessed; i++)
			{
				ALuint bufferId = 0;
				alSourceUnqueueBuffers(handle->SourceId, 1, &bufferId);

				if (AL_CHECK_ERROR())
					continue;

				streamedAudioHandle->BufferIds.push(bufferId);
			}
		}

		while (!streamedAudioHandle->BufferIds.empty())
		{
			// Make sure we have a chunk to replace with first, before unqueueing.
			// This way we don't need additional tracking.
			{
				std::scoped_lock lock(_decoderThread->DecoderMutex());
				if (streamedAudioHandle->DecodedChunks.empty())
					break;

				tmpDecodedChunk = std::move(streamedAudioHandle->DecodedChunks.front());
				streamedAudioHandle->DecodedChunks.pop();
			}

			// < 0 indicates error
			// 0 indicates EOF
			if (tmpDecodedChunk.BytesDecoded <= 0)
				continue;

			// Buffer the new decoded data.
			ALuint bufferId = streamedAudioHandle->BufferIds.front();
			alBufferData(
				bufferId, streamedAudioHandle->Asset->PcmFormat,
				&tmpDecodedChunk.Data[0], tmpDecodedChunk.BytesDecoded,
				streamedAudioHandle->Asset->SampleRate);
			if (AL_CHECK_ERROR())
				continue;

			alSourceQueueBuffers(handle->SourceId, 1, &bufferId);
			if (AL_CHECK_ERROR())
				continue;

			streamedAudioHandle->BufferIds.pop();
		}
	}

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
