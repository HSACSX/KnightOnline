#ifndef CLIENT_N3BASE_SOUNDBUFFER_H
#define CLIENT_N3BASE_SOUNDBUFFER_H

#pragma once

#include <string>

struct SoundBuffer
{
protected:
	static constexpr uint32_t INVALID_BUFFER_ID = ~0U;

	SoundBuffer();
	~SoundBuffer();

public:
	uint32_t		BufferId = INVALID_BUFFER_ID;
	std::string		Filename;
	uint32_t		RefCount = 0;
};

#endif // CLIENT_N3BASE_SOUNDBUFFER_H
