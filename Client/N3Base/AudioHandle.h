#ifndef CLIENT_N3BASE_AUDIOHANDLE_H
#define CLIENT_N3BASE_AUDIOHANDLE_H

#pragma once

#include <memory>

class AudioAsset;
class AudioHandle
{
public:
	enum e_AudioHandleType : uint8_t
	{
		AUDIO_HANDLE_BUFFERED = 0,
		AUDIO_HANDLE_STREAMED
	};

	static constexpr uint32_t INVALID_SOURCE_ID = ~0U;

	e_AudioHandleType			HandleType;
	bool						IsPlaying	= false;
	uint32_t					SourceId	= INVALID_SOURCE_ID;
	std::shared_ptr<AudioAsset>	Asset;

	virtual ~AudioHandle() {}
};

class BufferedAudioAsset;
class BufferedAudioHandle : public AudioHandle
{
public:
	static std::shared_ptr<BufferedAudioHandle> Create(std::shared_ptr<BufferedAudioAsset> asset);

	BufferedAudioHandle();
	~BufferedAudioHandle() override;
};

class StreamedAudioAsset;
class StreamedAudioHandle : public AudioHandle
{
public:
	static constexpr size_t		BUFFER_COUNT		= 4;
	static constexpr uint32_t	INVALID_BUFFER_ID	= ~0U;

	uint32_t BufferIds[BUFFER_COUNT];

	static std::shared_ptr<StreamedAudioHandle> Create(std::shared_ptr<StreamedAudioAsset> asset);

	StreamedAudioHandle();
	~StreamedAudioHandle() override;
};

#endif // CLIENT_N3BASE_AUDIOHANDLE_H
