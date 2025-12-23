#include "StdAfxBase.h"
#include "LoadedSoundBuffer.h"
#include "StreamedSoundBuffer.h"
#include "WaveFormat.h"
#include "al_wrapper.h"

#include <FileIO/FileReader.h>
#include <shared/StringUtils.h>

#include <cstring> // std::memcpy()

// We follow Frictional Games' implementation for scanning through RIFF chunks
// https://github.com/FrictionalGames/OALWrapper/blob/973659d5700720a87063789a591b811ce5af7dbe/sources/OAL_WAVSample.cpp

SoundBuffer::SoundBuffer()
{
}

SoundBuffer::~SoundBuffer()
{
	if (BufferId != INVALID_BUFFER_ID)
	{
		alDeleteBuffers(1, &BufferId);
		AL_CHECK_ERROR();
	}

	BufferId = INVALID_BUFFER_ID;
}

template <typename T>
static const uint8_t* find_chunk(const uint8_t* start, const uint8_t* end, const char* chunkID, const T** outStruct)
{
	constexpr size_t RequiredStructSize = std::max(sizeof(T), sizeof(RIFF_SubChunk));
	const uint8_t* ptr = start;
	while (ptr < (end - RequiredStructSize))
	{
		const RIFF_SubChunk* chunk = reinterpret_cast<const RIFF_SubChunk*>(ptr);

		if (chunk->SubChunkID[0] == chunkID[0]
			&& chunk->SubChunkID[1] == chunkID[1]
			&& chunk->SubChunkID[2] == chunkID[2]
			&& chunk->SubChunkID[3] == chunkID[3])
		{
			*outStruct = reinterpret_cast<const T*>(ptr);
			return ptr;
		}

		ptr += sizeof(RIFF_SubChunk);
		ptr += chunk->SubChunkSize;
	}

	return nullptr;
}

bool LoadedSoundBuffer::LoadFromFile(const std::string& filename)
{
	// Expect ".wav"
	if (filename.length() < 4
		|| strnicmp(filename.data() + filename.length() - 4, ".wav", 4) != 0)
		return false;

	FileReader file;
	if (!file.OpenExisting(filename))
		return false;

	const size_t fileSize = file.Size();
	if (fileSize < sizeof(RIFF_Header))
		return false;

	const uint8_t* start = static_cast<const uint8_t*>(file.Memory());
	const uint8_t* end = start + fileSize;
	const uint8_t* ptr = start;

	const RIFF_Header* riffHeader = reinterpret_cast<const RIFF_Header*>(ptr);
	const WAVE_Format* waveFormat = nullptr;
	const WAVE_Data* waveData = nullptr;

	if (riffHeader->FileTypeID[0] != 'R'
		|| riffHeader->FileTypeID[1] != 'I'
		|| riffHeader->FileTypeID[2] != 'F'
		|| riffHeader->FileTypeID[3] != 'F'
		|| riffHeader->FileFormatID[0] != 'W'
		|| riffHeader->FileFormatID[1] != 'A'
		|| riffHeader->FileFormatID[2] != 'V'
		|| riffHeader->FileFormatID[3] != 'E')
		return false;

	ptr += sizeof(RIFF_Header);

	ptr = find_chunk<WAVE_Format>(ptr, end, "fmt ", &waveFormat);
	if (ptr == nullptr)
		return false;

	// We expect PCM
	if (waveFormat->AudioFormat != 1)
		return false;

	ptr += sizeof(RIFF_SubChunk);
	ptr += waveFormat->Size;

	ptr = find_chunk<WAVE_Data>(ptr, end, "data", &waveData);
	if (ptr == nullptr)
		return false;

	ptr += sizeof(RIFF_SubChunk);

	const uint8_t* pcmDataBuffer = ptr;
	const uint32_t pcmDataSize = waveData->Size;
	if (pcmDataSize > (end - pcmDataBuffer))
		return false;

	alGenBuffers(1, &BufferId);
	if (AL_CHECK_ERROR())
		return false;

	ALenum format;
	
	if (waveFormat->NumChannels == 2)
	{
		if (waveFormat->BitsPerSample == 8)
			format = AL_FORMAT_STEREO8;
		else if (waveFormat->BitsPerSample == 16)
			format = AL_FORMAT_STEREO16;
		else
			return false;
	}
	else if (waveFormat->NumChannels == 1)
	{
		if (waveFormat->BitsPerSample == 8)
			format = AL_FORMAT_MONO8;
		else if (waveFormat->BitsPerSample == 16)
			format = AL_FORMAT_MONO16;
		else
			return false;
	}
	else
	{
		return false;
	}

	alBufferData(
		BufferId, format, pcmDataBuffer,
		static_cast<ALsizei>(pcmDataSize),
		static_cast<ALsizei>(waveFormat->SampleRate));

	if (AL_CHECK_ERROR())
		return false;

	Filename = filename;
	return true;
}

bool StreamedSoundBuffer::LoadFromFile(const std::string& filename)
{
	// Expect ".mp3" (TODO: or .wav)...
	if (filename.length() < 4)
		return false;

	if (strnicmp(filename.data() + filename.length() - 4, ".mp3", 4) != 0)
		return false;

	auto file = std::make_unique<FileReader>();
	if (file == nullptr)
		return false;

	if (!file->OpenExisting(filename))
		return false;

	File = std::move(file);
	Filename = filename;
	return true;
}
