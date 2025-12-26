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

class AudioAsset
{
protected:
	static constexpr uint32_t INVALID_BUFFER_ID = ~0U;

public:
	e_AudioAssetType	Type		= AUDIO_ASSET_UNKNOWN;
	std::string			Filename;
	uint32_t			RefCount	= 0;
	int32_t				PcmFormat	= -1;
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
	uint32_t		BufferId = INVALID_BUFFER_ID;
};

class FileReader;
class StreamedAudioAsset : public AudioAsset
{
public:
	std::unique_ptr<FileReader>	File;

public:
	StreamedAudioAsset();
	bool LoadFromFile(const std::string& filename) override;
	~StreamedAudioAsset() override;
};

#endif // CLIENT_N3BASE_AUDIOASSET_H
