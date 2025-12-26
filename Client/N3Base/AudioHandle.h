#ifndef CLIENT_N3BASE_AUDIOHANDLE_H
#define CLIENT_N3BASE_AUDIOHANDLE_H

#pragma once

#include <memory>
#include <vector>
#include <queue>

enum e_AudioHandleType : uint8_t
{
	AUDIO_HANDLE_BUFFERED = 0,
	AUDIO_HANDLE_STREAMED,
	AUDIO_HANDLE_UNKNOWN
};

class AudioAsset;
class AudioHandle
{
public:
	static constexpr uint32_t INVALID_SOURCE_ID = ~0U;

	e_AudioHandleType			HandleType;
	bool						StartedPlaying;
	bool						FinishedPlaying;
	bool						IsLooping;
	uint32_t					SourceId;
	std::shared_ptr<AudioAsset>	Asset;

	AudioHandle();
	virtual ~AudioHandle();
};

class BufferedAudioHandle : public AudioHandle
{
public:
	static std::shared_ptr<BufferedAudioHandle> Create(std::shared_ptr<AudioAsset> asset);

	BufferedAudioHandle();
	~BufferedAudioHandle() override;
};

class FileReader;
struct FileReaderHandle
{
	FileReader* File = nullptr;
	size_t		Offset = 0;
};

struct mpg123_handle_struct;
class StreamedAudioHandle : public AudioHandle
{
public:
	static constexpr size_t		BUFFER_COUNT		= 4;
	static constexpr uint32_t	INVALID_BUFFER_ID	= ~0U;

	struct DecodedChunk
	{
		std::vector<uint8_t>	Data;
		int32_t					BytesDecoded = -1;
	};

	FileReaderHandle			FileReaderHandle;
	mpg123_handle_struct*		Mp3Handle;
	std::queue<uint32_t>		BufferIds;
	std::queue<DecodedChunk>	DecodedChunks;
	bool						FinishedDecoding;

	static std::shared_ptr<StreamedAudioHandle> Create(std::shared_ptr<AudioAsset> asset);

	StreamedAudioHandle();
	~StreamedAudioHandle() override;
};

#endif // CLIENT_N3BASE_AUDIOHANDLE_H
