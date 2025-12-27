#include "StdAfxBase.h"
#include "AudioHandle.h"
#include "N3Base.h"
#include "N3SndMgr.h"
#include "AudioAsset.h"
#include "al_wrapper.h"

#include <FileIO/FileReader.h>
#include <mpg123.h>

mpg123_ssize_t mpg123_filereader_read(void* userData, void* dst, size_t bytes);
off_t mpg123_filereader_seek(void* userData, off_t offset, int whence);
void mpg123_filereader_cleanup(void* userData);

AudioHandle::AudioHandle()
{
	HandleType		= AUDIO_HANDLE_UNKNOWN;
	IsManaged		= false;
	StartedPlaying	= false;
	FinishedPlaying	= false;
	IsLooping		= false;
	SourceId		= INVALID_SOURCE_ID;

	State			= SNDSTATE_INITIAL;

	FadeInTime		= 0.0f;
	FadeOutTime		= 0.0f;
	StartDelayTime	= 0.0f;
	Timer			= 0.0f;
}

AudioHandle::~AudioHandle()
{
}

std::shared_ptr<BufferedAudioHandle> BufferedAudioHandle::Create(std::shared_ptr<AudioAsset> asset)
{
	if (asset == nullptr)
		return nullptr;

	uint32_t sourceId;
	if (!CN3Base::s_SndMgr.PullBufferedSourceIdFromPool(&sourceId))
		return nullptr;

	auto handle = std::make_shared<BufferedAudioHandle>();
	if (handle == nullptr)
	{
		CN3Base::s_SndMgr.RestoreBufferedSourceIdToPool(&sourceId);
		return nullptr;
	}

	handle->SourceId = sourceId;
	handle->Asset = std::move(asset);
	return handle;
}

std::shared_ptr<StreamedAudioHandle> StreamedAudioHandle::Create(std::shared_ptr<AudioAsset> asset)
{
	assert(asset != nullptr);

	if (asset == nullptr)
		return nullptr;

	assert(asset->Type == AUDIO_ASSET_STREAMED);

	if (asset->Type != AUDIO_ASSET_STREAMED)
		return nullptr;

	StreamedAudioAsset* streamedAudioAsset = static_cast<StreamedAudioAsset*>(asset.get());

	uint32_t sourceId;
	if (!CN3Base::s_SndMgr.PullStreamedSourceIdFromPool(&sourceId))
		return nullptr;

	auto handle = std::make_shared<StreamedAudioHandle>();
	if (handle == nullptr)
	{
		CN3Base::s_SndMgr.RestoreStreamedSourceIdToPool(&sourceId);
		return nullptr;
	}

	if (handle->Mp3Handle != nullptr)
	{
		mpg123_delete(handle->Mp3Handle);
		handle->Mp3Handle = nullptr;
	}

	if (asset->DecoderType == AUDIO_DECODER_MP3)
	{
		handle->Mp3Handle = mpg123_new(nullptr, nullptr);
		if (handle->Mp3Handle == nullptr)
		{
			CN3Base::s_SndMgr.RestoreStreamedSourceIdToPool(&sourceId);
			return nullptr;
		}

		int err = mpg123_format_none(handle->Mp3Handle);
		assert(err == MPG123_OK);

		err = mpg123_format(handle->Mp3Handle, asset->SampleRate, MPG123_STEREO, MPG123_ENC_SIGNED_16);
		assert(err == MPG123_OK);

		err = mpg123_replace_reader_handle(handle->Mp3Handle, mpg123_filereader_read, mpg123_filereader_seek, mpg123_filereader_cleanup);
		assert(err == MPG123_OK);

		// Position the reader handle at the start of the file.
		handle->FileReaderHandle.File	= streamedAudioAsset->File.get();
		handle->FileReaderHandle.Offset	= 0;
	
		err = mpg123_open_handle(handle->Mp3Handle, &handle->FileReaderHandle);
		assert(err == MPG123_OK);

		if (streamedAudioAsset->PcmChunkSize == 0)
			streamedAudioAsset->PcmChunkSize = mpg123_outblock(handle->Mp3Handle);
	}
	else if (asset->DecoderType == AUDIO_DECODER_PCM)
	{
		if (streamedAudioAsset->PcmDataBuffer == nullptr)
		{
			CN3Base::s_SndMgr.RestoreStreamedSourceIdToPool(&sourceId);
			return nullptr;
		}

		// Position the reader handle at the beginning of the data buffer.
		handle->FileReaderHandle.File	= streamedAudioAsset->File.get();
		handle->FileReaderHandle.Offset	= streamedAudioAsset->PcmDataBuffer
			- static_cast<const uint8_t*>(streamedAudioAsset->File->Memory());
	}
	else
	{
		CN3Base::s_SndMgr.RestoreStreamedSourceIdToPool(&sourceId);
		return nullptr;
	}

	handle->SourceId		= sourceId;
	handle->Asset			= std::move(asset);

	return handle;
}

BufferedAudioHandle::BufferedAudioHandle()
{
	HandleType = AUDIO_HANDLE_BUFFERED;
}

BufferedAudioHandle::~BufferedAudioHandle()
{
	if (SourceId != INVALID_SOURCE_ID)
	{
		alSourceStop(SourceId);
		AL_CLEAR_ERROR_STATE();

		alSourceRewind(SourceId);
		AL_CLEAR_ERROR_STATE();

		alSourcei(SourceId, AL_BUFFER, 0);
		AL_CLEAR_ERROR_STATE();

		CN3Base::s_SndMgr.RestoreBufferedSourceIdToPool(&SourceId);
	}
}

StreamedAudioHandle::StreamedAudioHandle()
{
	HandleType			= AUDIO_HANDLE_STREAMED;
	Mp3Handle			= nullptr;
	BuffersAllocated	= false;
	FinishedDecoding	= false;

	BufferIds.reserve(BUFFER_COUNT);
}

void StreamedAudioHandle::RewindFrame()
{
	StreamedAudioAsset* asset = static_cast<StreamedAudioAsset*>(Asset.get());

	if (asset->DecoderType == AUDIO_DECODER_MP3)
	{
		FileReaderHandle.Offset = 0;

		if (Mp3Handle != nullptr)
			mpg123_seek_frame(Mp3Handle, 0, SEEK_SET);
	}
	else
	{
		FileReaderHandle.Offset = asset->PcmDataBuffer - static_cast<const uint8_t*>(asset->File->Memory());
	}
}

StreamedAudioHandle::~StreamedAudioHandle()
{
	if (SourceId != INVALID_SOURCE_ID)
	{
		alSourceStop(SourceId);
		AL_CLEAR_ERROR_STATE();

		alSourceRewind(SourceId);
		AL_CLEAR_ERROR_STATE();

		alSourcei(SourceId, AL_BUFFER, 0);
		AL_CHECK_ERROR();

		for (uint32_t bufferId : BufferIds)
		{
			alDeleteBuffers(1, &bufferId);
			AL_CHECK_ERROR();
		}

		CN3Base::s_SndMgr.RestoreStreamedSourceIdToPool(&SourceId);
	}

	if (Mp3Handle != nullptr)
	{
		mpg123_delete(Mp3Handle);
		Mp3Handle = nullptr;
	}
}

mpg123_ssize_t mpg123_filereader_read(void* userData, void* dst, size_t bytes)
{
	FileReaderHandle* handle = static_cast<FileReaderHandle*>(userData);
	const FileReader* file = handle->File;
	const size_t size = static_cast<size_t>(file->Size());

	if (handle->Offset >= size)
		return MPG123_DONE; // EOF

	const size_t remainingBytes = size - handle->Offset;
	size_t bytesToRead = std::min(bytes, remainingBytes);

	std::memcpy(
		dst,
		static_cast<const uint8_t*>(file->Memory()) + handle->Offset,
		bytesToRead);

	handle->Offset += bytesToRead;
	return static_cast<mpg123_ssize_t>(bytesToRead);
}

off_t mpg123_filereader_seek(void* userData, off_t offset, int whence)
{
	FileReaderHandle* handle = static_cast<FileReaderHandle*>(userData);
	const FileReader* file = handle->File;
	const size_t size = static_cast<size_t>(file->Size());

	size_t newPos = 0;

	switch (whence)
	{
		case SEEK_SET:
			newPos = static_cast<size_t>(offset);
			break;

		case SEEK_CUR:
			newPos = handle->Offset + static_cast<size_t>(offset);
			break;

		case SEEK_END:
			newPos = size + static_cast<size_t>(offset);
			break;

		default:
			return -1; // unhandled type
	}

	if (newPos > size)
		return -1; // cannot seek past the end of the file

	handle->Offset = newPos;
	return static_cast<off_t>(handle->Offset);
}

void mpg123_filereader_cleanup(void* /*userData*/)
{
	/* do nothing, cleanup is managed externally */
}
