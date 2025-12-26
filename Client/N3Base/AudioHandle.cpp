#include "StdAfxBase.h"
#include "AudioHandle.h"
#include "N3Base.h"
#include "N3SndMgr.h"
#include "AudioAsset.h"

#include "al_wrapper.h"

AudioHandle::AudioHandle()
{
	SourceId		= INVALID_SOURCE_ID;
	StartedPlaying	= false;
	FinishedPlaying	= false;
}

AudioHandle::~AudioHandle()
{
}

std::shared_ptr<BufferedAudioHandle> BufferedAudioHandle::Create(std::shared_ptr<BufferedAudioAsset> asset)
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

std::shared_ptr<StreamedAudioHandle> StreamedAudioHandle::Create(std::shared_ptr<StreamedAudioAsset> asset)
{
	if (asset == nullptr)
		return nullptr;

	uint32_t sourceId;
	if (!CN3Base::s_SndMgr.PullStreamedSourceIdFromPool(&sourceId))
		return nullptr;

	auto handle = std::make_shared<StreamedAudioHandle>();
	if (handle == nullptr)
	{
		CN3Base::s_SndMgr.RestoreStreamedSourceIdToPool(&sourceId);
		return nullptr;
	}

	handle->SourceId = sourceId;
	handle->Asset = std::move(asset);

	return handle;
}

BufferedAudioHandle::BufferedAudioHandle()
{
	HandleType = AUDIO_HANDLE_BUFFERED;
}

BufferedAudioHandle::~BufferedAudioHandle()
{
	if (SourceId == INVALID_SOURCE_ID)
		return;

	alSourceStop(SourceId);
	AL_CLEAR_ERROR_STATE();

	alSourceRewind(SourceId);
	AL_CLEAR_ERROR_STATE();

	alSourcei(SourceId, AL_BUFFER, 0);
	AL_CLEAR_ERROR_STATE();

	CN3Base::s_SndMgr.RestoreBufferedSourceIdToPool(&SourceId);
}

StreamedAudioHandle::StreamedAudioHandle()
{
	HandleType = AUDIO_HANDLE_STREAMED;

	for (size_t i = 0; i < BUFFER_COUNT; i++)
		BufferIds[i] = INVALID_BUFFER_ID;
}

StreamedAudioHandle::~StreamedAudioHandle()
{
	if (SourceId == INVALID_SOURCE_ID)
		return;

	alSourceStop(SourceId);
	AL_CLEAR_ERROR_STATE();

	alSourceRewind(SourceId);
	AL_CLEAR_ERROR_STATE();

	alSourcei(SourceId, AL_BUFFER, 0);
	AL_CHECK_ERROR();

	for (int i = 0; i < BUFFER_COUNT; i++)
	{
		if (BufferIds[i] == INVALID_BUFFER_ID)
			continue;

		alDeleteBuffers(1, &BufferIds[i]);
		AL_CHECK_ERROR();
	}

	CN3Base::s_SndMgr.RestoreStreamedSourceIdToPool(&SourceId);
}
