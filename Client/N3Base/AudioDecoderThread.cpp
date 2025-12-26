#include "StdAfxBase.h"
#include "AudioDecoderThread.h"
#include "AudioHandle.h"
#include "AudioAsset.h"

#include <cassert>
#include <unordered_map>

#include <mpg123.h>

using namespace std::chrono_literals;

void AudioDecoderThread::thread_loop()
{
	std::vector<QueueType> pendingQueue;
	std::unordered_map<uint32_t, std::shared_ptr<StreamedAudioHandle>> handleMap;

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
			for (auto& [type, handle] : pendingQueue)
			{
				switch (type)
				{
					case AUDIO_DECODER_QUEUE_ADD:
						handleMap[handle->SourceId] = std::move(handle);
						break;

					case AUDIO_DECODER_QUEUE_REMOVE:
						handleMap.erase(handle->SourceId);
						break;
				}
			}

			pendingQueue.clear();
		}

		// Run decode for this tick
		{
			std::scoped_lock lock(_decoderMutex);
			for (auto& [_, handle] : handleMap)
				decode_impl(handle.get());
		}
	}
}

void AudioDecoderThread::Add(std::shared_ptr<StreamedAudioHandle> handle)
{
	std::scoped_lock lock(_mutex);
	_pendingQueue.push_back(std::make_tuple(AUDIO_DECODER_QUEUE_ADD, std::move(handle)));
}

void AudioDecoderThread::InitialDecode(StreamedAudioHandle* handle)
{
	// Force an initial decode *now* from the main audio thread
	std::scoped_lock lock(_decoderMutex);
	decode_impl(handle);
}

void AudioDecoderThread::Remove(std::shared_ptr<StreamedAudioHandle> handle)
{
	std::scoped_lock lock(_mutex);
	_pendingQueue.push_back(std::make_tuple(AUDIO_DECODER_QUEUE_REMOVE, std::move(handle)));
}

void AudioDecoderThread::decode_impl(StreamedAudioHandle* handle)
{
	constexpr auto BUFFER_COUNT = StreamedAudioHandle::BUFFER_COUNT;
	if (handle->DecodedChunks.size() >= BUFFER_COUNT)
		return;

	const size_t ChunksToDecode = BUFFER_COUNT - handle->DecodedChunks.size();

	StreamedAudioAsset* asset = static_cast<StreamedAudioAsset*>(handle->Asset.get());

	assert(handle->PcmFrameSize != 0);

	if (handle->Mp3Handle != nullptr)
	{
		size_t done = 0;
		int error;

		for (size_t i = 0; i < ChunksToDecode; i++)
		{
			StreamedAudioHandle::DecodedChunk decodedChunk = {};
			decodedChunk.Data.resize(handle->PcmFrameSize);

			do
			{
				error = mpg123_read(handle->Mp3Handle, &decodedChunk.Data[0], handle->PcmFrameSize, &done);

				// The first read will invoke MPG123_NEW_FORMAT.
				// This is where we'd fetch the format data, but we don't really care about it.
				// Retry the read so we can decode the chunk.
				if (error == MPG123_NEW_FORMAT)
					continue;

				// Decoded a chunk
				if (error == MPG123_OK)
				{
					decodedChunk.BytesDecoded = static_cast<int32_t>(done);
					decodedChunk.Data.resize(done);

					handle->DecodedChunks.push(std::move(decodedChunk));
					handle->FinishedDecoding = false;
				}
				// Finished decoding chunks. This contains no data.
				else if (error == MPG123_DONE)
				{
					if (handle->FinishedDecoding)
						break;

					decodedChunk.BytesDecoded = 0;
					decodedChunk.Data.clear();

					handle->DecodedChunks.push(std::move(decodedChunk));

					// If we're looping, we should reset back to the first frame
					// and start decoding from there.
					if (handle->IsLooping)
						mpg123_seek_frame(handle->Mp3Handle, 0, SEEK_SET);
					// Otherwise, we've finished decoding so we should stop here.
					else
						handle->FinishedDecoding = true;
				}
			}
			while (false);
		}
	}
	else
	{
		// TODO
	}
}
