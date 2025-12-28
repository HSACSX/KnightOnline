#ifndef CLIENT_N3BASE_AUDIOHANDLE_H
#define CLIENT_N3BASE_AUDIOHANDLE_H

#pragma once

#include "N3SndDef.h"

#include <atomic>	// std::atomic<>
#include <memory>	// std::shared_ptr<>
#include <vector>	// std::vector<>
#include <queue>	// std::queue<>

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
	e_AudioHandleType				HandleType;
	std::atomic<bool>				IsManaged;
	bool							StartedPlaying;
	bool							FinishedPlaying;
	bool							IsLooping;
	uint32_t						SourceId;
	std::shared_ptr<AudioAsset>		Asset;

	e_SndState						State;

	std::shared_ptr<SoundSettings>	Settings;

	float							FadeInTime;
	float							FadeOutTime;
	float							StartDelayTime;
	float							Timer;

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

struct AudioDecodedChunk
{
	std::vector<uint8_t>	Data;
	int32_t					BytesDecoded = -1;
};

struct mpg123_handle_struct;
class StreamedAudioHandle : public AudioHandle
{
	friend class AudioThread;
	friend class AudioDecoderThread;

protected:
	FileReaderHandle				FileReaderHandle;
	mpg123_handle_struct*			Mp3Handle;
	std::queue<uint32_t>			AvailableBufferIds;
	std::vector<uint32_t>			BufferIds;
	std::queue<AudioDecodedChunk>	DecodedChunks;
	bool							BuffersAllocated;
	bool							FinishedDecoding;

public:
	static std::shared_ptr<StreamedAudioHandle> Create(std::shared_ptr<AudioAsset> asset);

	StreamedAudioHandle();
	~StreamedAudioHandle() override;

protected:
	void RewindFrame();
};

#endif // CLIENT_N3BASE_AUDIOHANDLE_H
