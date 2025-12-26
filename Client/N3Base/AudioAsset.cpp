#include "StdAfxBase.h"
#include "AudioAsset.h"
#include "WaveFormat.h"
#include "al_wrapper.h"

#include <FileIO/FileReader.h>
#include <shared/StringUtils.h>

#include <cstring> // std::memcpy()

// We follow Frictional Games' implementation for scanning through RIFF chunks
// https://github.com/FrictionalGames/OALWrapper/blob/973659d5700720a87063789a591b811ce5af7dbe/sources/OAL_WAVSample.cpp

BufferedAudioAsset::BufferedAudioAsset()
{
	Type = AUDIO_ASSET_BUFFERED;
}

BufferedAudioAsset::~BufferedAudioAsset()
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

static bool ParseWAV(FileReader& file, ALenum* format, ALsizei* sampleRate, size_t* pcmChunkSize,
	const uint8_t** pcmDataBuffer, ALsizei* pcmDataSize)
{
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

	const uint8_t* pcmDataBuffer_ = ptr;
	const uint32_t pcmDataSize_ = waveData->Size;

	if (pcmDataSize_ > (end - pcmDataBuffer_))
		return false;

	ALenum format_;

	if (waveFormat->NumChannels == 2)
	{
		if (waveFormat->BitsPerSample == 8)
			format_ = AL_FORMAT_STEREO8;
		else if (waveFormat->BitsPerSample == 16)
			format_ = AL_FORMAT_STEREO16;
		else
			return false;
	}
	else if (waveFormat->NumChannels == 1)
	{
		if (waveFormat->BitsPerSample == 8)
			format_ = AL_FORMAT_MONO8;
		else if (waveFormat->BitsPerSample == 16)
			format_ = AL_FORMAT_MONO16;
		else
			return false;
	}
	else
	{
		return false;
	}

	constexpr double DurationSec = 0.418; // roughly the same duration as the MP3 chunks

	ALsizei bytesPerFrame = static_cast<ALint>(waveFormat->NumChannels * (waveFormat->BitsPerSample / 8));
	ALsizei chunkFrames = static_cast<ALint>(waveFormat->SampleRate * DurationSec);
	ALsizei pcmChunkSize_ = chunkFrames * bytesPerFrame;

	if (format != nullptr)
		*format = format_;

	if (sampleRate != nullptr)
		*sampleRate = static_cast<ALsizei>(waveFormat->SampleRate);

	if (pcmDataBuffer != nullptr)
		*pcmDataBuffer = pcmDataBuffer_;

	if (pcmDataSize != nullptr)
		*pcmDataSize = pcmDataSize_;

	if (pcmChunkSize != nullptr)
		*pcmChunkSize = pcmChunkSize_;

	return true;
}

bool BufferedAudioAsset::LoadFromFile(const std::string& filename)
{
	// Expect ".wav"
	if (filename.length() < 4
		|| strnicmp(filename.data() + filename.length() - 4, ".wav", 4) != 0)
		return false;

	FileReader file;
	if (!file.OpenExisting(filename))
		return false;

	ALenum pcmFormat;
	ALsizei sampleRate, pcmDataSize;
	size_t pcmChunkSize;
	const uint8_t* pcmDataBuffer = nullptr;
	if (!ParseWAV(file, &pcmFormat, &sampleRate, &pcmChunkSize, &pcmDataBuffer, &pcmDataSize))
		return false;

	alGenBuffers(1, &BufferId);
	if (AL_CHECK_ERROR())
		return false;

	alBufferData(BufferId, pcmFormat, pcmDataBuffer, pcmDataSize, sampleRate);
	if (AL_CHECK_ERROR())
		return false;

	Filename	= filename;
	PcmFormat	= pcmFormat;
	SampleRate	= static_cast<int32_t>(sampleRate);

	return true;
}

StreamedAudioAsset::StreamedAudioAsset()
{
	Type = AUDIO_ASSET_STREAMED;
}

bool StreamedAudioAsset::LoadFromFile(const std::string& filename)
{
	// Expect ".mp3"
	if (filename.length() < 4)
		return false;

	const char* extension = filename.data() + filename.length() - 4;

	bool isMp3 = false, isWav = false;
	if (strnicmp(extension, ".mp3", 4) == 0)
		isMp3 = true;
	else if (strnicmp(extension, ".wav", 4) == 0)
		isWav = true;
	else
		return false;

	auto file = std::make_unique<FileReader>();
	if (file == nullptr)
		return false;

	if (!file->OpenExisting(filename))
		return false;

	ALenum pcmFormat;
	ALsizei sampleRate;

	if (isWav)
	{
		size_t pcmChunkSize;
		ALsizei pcmDataSize;
		const uint8_t* pcmDataBuffer = nullptr;
		if (!ParseWAV(*file, &pcmFormat, &sampleRate, &pcmChunkSize, &pcmDataBuffer, &pcmDataSize))
			return false;

		DecoderType		= AUDIO_DECODER_PCM;
		PcmChunkSize	= pcmChunkSize;
		PcmDataSize		= static_cast<size_t>(pcmDataSize);
		PcmDataBuffer	= pcmDataBuffer;
	}
	else if (isMp3)
	{
		DecoderType		= AUDIO_DECODER_MP3;
		sampleRate		= 44100;
		pcmFormat		= AL_FORMAT_STEREO16;
	}
	else
	{
		return false;
	}

	File		= std::move(file);
	Filename	= filename;
	PcmFormat	= pcmFormat;
	SampleRate	= static_cast<int32_t>(sampleRate);

	return true;
}

StreamedAudioAsset::~StreamedAudioAsset()
{
}

