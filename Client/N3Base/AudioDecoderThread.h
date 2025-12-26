#ifndef CLIENT_N3BASE_AUDIODECODERTHREAD_H
#define CLIENT_N3BASE_AUDIODECODERTHREAD_H

#pragma once

#include <shared/Thread.h>

class AudioDecoderThread : public Thread
{
public:
	void thread_loop() override;
};

#endif // CLIENT_N3BASE_AUDIODECODERTHREAD_H
