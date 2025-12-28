#ifndef CLIENT_N3BASE_WAVEFORMAT_H
#define CLIENT_N3BASE_WAVEFORMAT_H

#pragma once

#include <inttypes.h> // uint32_t, uint16_t

#pragma pack(push, 1)
struct WAVE_Data
{
	char		ID[4]	= { 'd', 'a', 't', 'a' };

	// NumSamples * NumChannels * (BitsPerSample / 8)
	uint32_t	Size	= 0;
};

struct WAVE_Format
{
	char		ID[4]			= { 'f', 'm', 't', ' ' };

	// Chunk size minus 8 bytes
	uint32_t	Size			= sizeof(WAVE_Format) - 8;

	// Audio format (1: PCM integer, 3: IEEE 754 float)
	uint16_t	AudioFormat		= 1;

	// Number of channels
	uint16_t	NumChannels		= 1;

	// Sample rate (in hertz)
	uint32_t	SampleRate		= 44100;

	// SampleRate * BytesPerBlock
	uint32_t	BytesPerSec		= 0;

	// Number of bytes per block (NumChannels * (BitsPerSample / 8))
	uint16_t	BytesPerBlock	= 0;

	// Number of bits per sample
	uint16_t	BitsPerSample	= 0;
};

struct RIFF_Header
{
	// RIFF file identifier
	char			FileTypeID[4];	// 'R', 'I', 'F', 'F'

	// Overall file size minus 8 bytes (append the data size)
	uint32_t		FileSize;

	// WAV file format identifier
	char			FileFormatID[4]; // 'W', 'A', 'V', 'E'
};

struct RIFF_SubChunk
{
	char			SubChunkID[4];
	uint32_t		SubChunkSize;
};
#pragma pack(pop)

#endif // CLIENT_N3BASE_WAVEFORMAT_H
