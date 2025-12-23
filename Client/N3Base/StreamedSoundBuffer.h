#ifndef CLIENT_N3BASE_STREAMEDSOUNDBUFFER_H
#define CLIENT_N3BASE_STREAMEDSOUNDBUFFER_H

#pragma once

#include <memory>
#include "SoundBuffer.h"

class FileReader;
struct StreamedSoundBuffer : SoundBuffer
{
	std::unique_ptr<FileReader>	File;

	bool LoadFromFile(const std::string& filename);
};

#endif // CLIENT_N3BASE_STREAMEDSOUNDBUFFER_H
