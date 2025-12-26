#ifndef CLIENT_N3BASE_AUDIOTHREAD_H
#define CLIENT_N3BASE_AUDIOTHREAD_H

#pragma once

#include <shared/Thread.h>

#include <functional>
#include <memory>
#include <vector>

#include "AudioHandle.h"

enum e_AudioQueueType
{
	AUDIO_QUEUE_ADD = 0,
	AUDIO_QUEUE_REMOVE,
	AUDIO_QUEUE_CALLBACK
};

class AudioDecoderThread;
class AudioHandle;
class AudioThread : public Thread
{
public:
	using AudioCallback = std::function<void(AudioHandle*)>;
	using QueueType = std::tuple<e_AudioQueueType, std::shared_ptr<AudioHandle>, AudioCallback>;

	AudioThread();
	~AudioThread() override;

	void thread_loop() override;
	void Add(std::shared_ptr<AudioHandle> handle);
	void QueueCallback(std::shared_ptr<AudioHandle> handle, AudioCallback callback);
	void Remove(std::shared_ptr<AudioHandle> handle);

private:
	void reset(std::shared_ptr<AudioHandle>& handle);
	void tick(std::shared_ptr<AudioHandle>& handle, StreamedAudioHandle::DecodedChunk& tmpDecodedChunk);

protected:
	std::vector<QueueType> _pendingQueue;
	std::unique_ptr<AudioDecoderThread> _decoderThread;
};

#endif // CLIENT_N3BASE_AUDIOTHREAD_H
