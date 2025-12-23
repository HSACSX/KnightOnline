#ifndef CLIENT_N3BASE_LOADEDSOUNDBUFFER_H
#define CLIENT_N3BASE_LOADEDSOUNDBUFFER_H

#pragma once

#include "SoundBuffer.h"

struct LoadedSoundBuffer : SoundBuffer
{
	bool LoadFromFile(const std::string& filename);
};

#endif // CLIENT_N3BASE_LOADEDSOUNDBUFFER_H
