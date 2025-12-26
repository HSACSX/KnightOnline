#ifndef CLIENT_N3BASE_AUDIODECODERTHREAD_H
#define CLIENT_N3BASE_AUDIODECODERTHREAD_H

#pragma once

#include <shared/Thread.h>

#include <memory>
#include <mutex>

enum e_AudioDecoderQueueType
{
	AUDIO_DECODER_QUEUE_ADD = 0,
	AUDIO_DECODER_QUEUE_REMOVE
};

class StreamedAudioHandle;
class AudioDecoderThread : public Thread
{
public:
	using QueueType = std::tuple<e_AudioDecoderQueueType, std::shared_ptr<StreamedAudioHandle>>;

	std::mutex& DecoderMutex()
	{
		return _decoderMutex;
	}

	void thread_loop() override;
	void Add(std::shared_ptr<StreamedAudioHandle> handle);
	void InitialDecode(StreamedAudioHandle* handle);
	void Remove(std::shared_ptr<StreamedAudioHandle> handle);

protected:
	void decode_impl(StreamedAudioHandle* handle);

	std::mutex _decoderMutex;
	std::vector<QueueType> _pendingQueue;
};

#endif // CLIENT_N3BASE_AUDIODECODERTHREAD_H
