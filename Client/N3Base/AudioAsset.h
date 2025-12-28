#ifndef CLIENT_N3BASE_AUDIOASSET_H
#define CLIENT_N3BASE_AUDIOASSET_H

#pragma once

#include <memory>
#include <string>

enum e_AudioAssetType : uint8_t
{
	AUDIO_ASSET_BUFFERED = 0,
	AUDIO_ASSET_STREAMED,
	AUDIO_ASSET_UNKNOWN
};

enum e_AudioDecoderType : uint8_t
{
	AUDIO_DECODER_MP3 = 0,	// .MP3
	AUDIO_DECODER_PCM,		// .WAV (raw PCM streams only)
	AUDIO_DECODER_UNKNOWN
};

class AudioAsset
{
public:
	e_AudioAssetType	Type		= AUDIO_ASSET_UNKNOWN;
	e_AudioDecoderType	DecoderType	= AUDIO_DECODER_UNKNOWN;
	std::string			Filename;
	uint32_t			RefCount	= 0;
	int32_t				AlFormat	= -1;
	int32_t				SampleRate	= 0;

public:
	virtual bool LoadFromFile(const std::string& filename) = 0;
	virtual ~AudioAsset() {}
};

class BufferedAudioAsset : public AudioAsset
{
public:
	BufferedAudioAsset();
	bool LoadFromFile(const std::string& filename) override;
	~BufferedAudioAsset() override;

public:
	uint32_t BufferId;
};

class FileReader;
class StreamedAudioAsset : public AudioAsset
{
public:
	std::unique_ptr<FileReader>	File;
	size_t						PcmDataSize;
	size_t						PcmChunkSize;
	const uint8_t*				PcmDataBuffer;

public:
	StreamedAudioAsset();
	bool LoadFromFile(const std::string& filename) override;
	~StreamedAudioAsset() override;
};

#endif // CLIENT_N3BASE_AUDIOASSET_H
