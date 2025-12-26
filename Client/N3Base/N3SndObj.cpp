// N3SndObj.cpp: implementation of the CN3SndObj class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfxBase.h"
#include "N3SndObj.h"
#include "N3Base.h"
#include "AudioAsset.h"
#include "AudioHandle.h"
#include "WaveFormat.h"
#include "al_wrapper.h"

#include <AL/alc.h>
#include <cassert>

#ifdef _N3GAME
#include "LogWriter.h"
#endif

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool AL_CHECK_ERROR_IMPL(const char* file, int line)
{
	ALenum error = alGetError();
	if (error == AL_NO_ERROR)
		return false;
	
#ifdef _N3GAME
	CLogWriter::Write("{}({}) - OpenAL error occurred: {:X}", file, line, error);
#endif
	return true;
}

CN3SndObj::CN3SndObj()
{
	_type = SNDTYPE_UNKNOWN;
	_state = SNDSTATE_STOP;

	_isStarted = false;
	_isLooping = false;

	_currentVolume = 0.0f;
	_maxVolume = 1.0f;

	_fadeInTime = 0;
	_fadeOutTime = 0;
	_startDelayTime = 0;
	_tmpSecPerFrm = 0;
}

CN3SndObj::~CN3SndObj()
{
	Release();
}

void CN3SndObj::Init()
{
	Release();

	_state = SNDSTATE_STOP;
	_isStarted = false;
	_isLooping = false;
	_currentVolume = 0.0f;
	_maxVolume = 1.0f;

	_startDelayTime = 0;
	_tmpSecPerFrm = 0;
	_fadeInTime = 0;
	_fadeOutTime = 0;
}

void CN3SndObj::Release()
{
	ReleaseHandle();

	if (_audioAsset != nullptr)
	{
		CN3Base::s_SndMgr.RemoveAudioAsset(_audioAsset.get());
		_audioAsset.reset();
	}
}

bool CN3SndObj::Create(const std::string& szFN, e_SndType eType)
{
	Init();

	if (eType == SNDTYPE_2D
		|| eType == SNDTYPE_3D)
	{
		_audioAsset = CN3Base::s_SndMgr.LoadBufferedAudioAsset(szFN);

		// Fallback to streaming if unsupported
		// Buffering is only supported for small WAV (PCM) files.
		// Streaming supports MP3.
		if (_audioAsset == nullptr)
			_audioAsset = CN3Base::s_SndMgr.LoadStreamedAudioAsset(szFN);
	}
	else if (eType == SNDTYPE_STREAM)
	{
		_audioAsset = CN3Base::s_SndMgr.LoadStreamedAudioAsset(szFN);
	}
	else
	{
		return false;
	}

	if (_audioAsset == nullptr)
		return false;

	_type = eType;
	return true;
}

//	SetVolume
//	range : [0.0f, 1.0f]
void CN3SndObj::SetVolume(float currentVolume)
{
	if (_handle == nullptr)
		return;

	_currentVolume = std::clamp(currentVolume, 0.0f, 1.0f);

	float gain = _currentVolume;
	CN3Base::s_SndMgr.QueueCallback(_handle, [=] (AudioHandle* handle)
	{
		alSourcef(handle->SourceId, AL_GAIN, _currentVolume);
		AL_CHECK_ERROR();
	});
}

const std::string& CN3SndObj::FileName() const
{
	if (_audioAsset != nullptr)
		return _audioAsset->Filename;

	static std::string empty;
	return empty;
}

bool CN3SndObj::IsFinished() const
{
	if (_handle == nullptr)
		return false;

	return _handle->FinishedPlaying;
}

void CN3SndObj::Tick()
{
	if (_handle == nullptr)
	{
		if (IsStarted())
			Stop();

		return;
	}

	if (GetState() == SNDSTATE_STOP)
		return;

	_tmpSecPerFrm += CN3Base::s_fSecPerFrm;

	if (GetState() == SNDSTATE_DELAY && _tmpSecPerFrm >= _startDelayTime)
	{
		_tmpSecPerFrm = 0;
		_state = SNDSTATE_FADEIN;
		PlayImpl();
	}

	if (GetState() == SNDSTATE_FADEIN)
	{
		if (_tmpSecPerFrm >= _fadeInTime)
		{
			_tmpSecPerFrm = 0;
			_state = SNDSTATE_PLAY;

			SetVolume(_maxVolume);
		}
		else
		{
			float vol = 0;
			if (_fadeInTime > 0.0f)
				vol = ((_tmpSecPerFrm / _fadeInTime) * _maxVolume);

			SetVolume(vol);
		}
	}

	if (GetState() == SNDSTATE_PLAY)
	{
		if (!_isLooping
			&& _handle->FinishedPlaying)
			_state = SNDSTATE_FADEOUT;
	}
	
	if (GetState() == SNDSTATE_FADEOUT)
	{
		if (_tmpSecPerFrm >= _fadeOutTime)
		{
			_tmpSecPerFrm = 0;

			SetVolume(0.0f);
			StopImpl();
		}
		else
		{
			// 볼륨 점점 작게....
			float vol = 0;
			if (_fadeOutTime > 0.0f)
				vol = (((_fadeOutTime - _tmpSecPerFrm) / _fadeOutTime) * _maxVolume);
			SetVolume(vol);
		}
	}
}

void CN3SndObj::Play(const __Vector3* pvPos, float delay, float fFadeInTime, bool bImmediately)
{
	if (_audioAsset == nullptr)
		return;

	if (bImmediately)
		Stop();

	if (_handle == nullptr)
	{
		if (_audioAsset->Type == AUDIO_ASSET_BUFFERED)
			_handle = BufferedAudioHandle::Create(_audioAsset);
		else if (_audioAsset->Type == AUDIO_ASSET_STREAMED)
			_handle = StreamedAudioHandle::Create(_audioAsset);

		if (_handle == nullptr)
			return;

		CN3Base::s_SndMgr.Add(_handle);
	}

	_handle->IsLooping = _isLooping;

	ALint isLooping = static_cast<ALint>(_isLooping);
	bool playImmediately = false;
	float gain = 0.0f;

	if (delay == 0.0f
		&& fFadeInTime == 0.0f)
	{
		_currentVolume = std::clamp(_maxVolume, 0.0f, 1.0f);

		gain = _currentVolume;
		playImmediately = true;
	}

	// Perform setup -- this is triggered after insertion, so the handle is added to the thread at this point
	// and otherwise setup, ready to play.
	if (GetType() == SNDTYPE_2D)
	{
		assert(_audioAsset->Type == AUDIO_ASSET_BUFFERED);

		if (_audioAsset->Type != AUDIO_ASSET_BUFFERED)
			return;

		auto audioAsset = static_pointer_cast<BufferedAudioAsset>(_audioAsset);
		if (audioAsset == nullptr)
			return;

		CN3Base::s_SndMgr.QueueCallback(_handle, [=] (AudioHandle* handle)
		{
			alSourcei(handle->SourceId, AL_BUFFER, audioAsset->BufferId);
			if (AL_CHECK_ERROR())
				return;

			alSourcei(handle->SourceId, AL_SOURCE_RELATIVE, AL_TRUE);
			AL_CHECK_ERROR();

			alSource3f(handle->SourceId, AL_POSITION, 0.0f, 0.0f, 0.0f);
			AL_CHECK_ERROR();

			alSourcef(handle->SourceId, AL_ROLLOFF_FACTOR, 0.0f);
			AL_CHECK_ERROR();

			alSourcei(handle->SourceId, AL_LOOPING, isLooping);
			AL_CHECK_ERROR();

			if (playImmediately)
			{
				alSourcef(handle->SourceId, AL_GAIN, gain);
				AL_CHECK_ERROR();

				alSourcePlay(handle->SourceId);
				AL_CHECK_ERROR();
			}
		});
	}
	else if (GetType() == SNDTYPE_3D)
	{
		assert(_audioAsset->Type == AUDIO_ASSET_BUFFERED);

		if (_audioAsset->Type != AUDIO_ASSET_BUFFERED)
			return;

		auto audioAsset = static_pointer_cast<BufferedAudioAsset>(_audioAsset);
		if (audioAsset == nullptr)
			return;

		bool hasPosition = false;

		__Vector3 position;
		if (pvPos != nullptr)
		{
			position = *pvPos;
			hasPosition = true;
		}

		CN3Base::s_SndMgr.QueueCallback(_handle, [=] (AudioHandle* handle)
		{
			alSourcei(handle->SourceId, AL_BUFFER, audioAsset->BufferId);
			if (AL_CHECK_ERROR())
				return;

			alSourcei(handle->SourceId, AL_SOURCE_RELATIVE, AL_FALSE);
			AL_CHECK_ERROR();

			if (hasPosition)
				alSource3f(handle->SourceId, AL_POSITION, position.x, position.y, position.z);
			else
				alSource3f(handle->SourceId, AL_POSITION, 0.0f, 0.0f, 0.0f);
			AL_CHECK_ERROR();

			alSourcef(handle->SourceId, AL_ROLLOFF_FACTOR, 0.5f);
			AL_CHECK_ERROR();

			alSourcei(handle->SourceId, AL_LOOPING, isLooping);
			AL_CHECK_ERROR();

			if (playImmediately)
			{
				alSourcef(handle->SourceId, AL_GAIN, gain);
				AL_CHECK_ERROR();

				alSourcePlay(handle->SourceId);
				AL_CHECK_ERROR();
			}
		});
	}
	else if (GetType() == SNDTYPE_STREAM)
	{
		CN3Base::s_SndMgr.QueueCallback(_handle, [=] (AudioHandle* handle)
		{
			alSourcei(handle->SourceId, AL_SOURCE_RELATIVE, AL_TRUE);
			AL_CHECK_ERROR();

			alSource3f(handle->SourceId, AL_POSITION, 0.0f, 0.0f, 0.0f);
			AL_CHECK_ERROR();

			alSourcef(handle->SourceId, AL_ROLLOFF_FACTOR, 0.0f);
			AL_CHECK_ERROR();

			alSourcei(handle->SourceId, AL_LOOPING, 0); // streams manually handle their looping
			AL_CHECK_ERROR();

			if (playImmediately)
			{
				alSourcef(handle->SourceId, AL_GAIN, gain);
				AL_CHECK_ERROR();

				alSourcePlay(handle->SourceId);
				AL_CHECK_ERROR();
			}
		});
	}
	else
	{
		return;
	}

	_isStarted = true;
	_fadeInTime = fFadeInTime;
	_fadeOutTime = 0;
	_startDelayTime = delay;
	_tmpSecPerFrm = 0;
	_state = SNDSTATE_DELAY;
}

void CN3SndObj::PlayImpl()
{
	if (_handle == nullptr)
		return;

	_isStarted = true;
	_state = SNDSTATE_PLAY;

	CN3Base::s_SndMgr.QueueCallback(_handle, [] (AudioHandle* handle)
	{
		handle->StartedPlaying = true;
		handle->FinishedPlaying = false;

		ALint state = AL_INITIAL;
		alGetSourcei(handle->SourceId, AL_SOURCE_STATE, &state);
		AL_CLEAR_ERROR_STATE();

		if (state != AL_PLAYING)
		{
			alSourcePlay(handle->SourceId);
			AL_CHECK_ERROR();
		}
	});
}

void CN3SndObj::Stop(float fFadeOutTime)
{
	_isStarted = false;

	if (_handle == nullptr)
		return;

	if (GetState() == SNDSTATE_FADEOUT
		|| GetState()  == SNDSTATE_STOP)
		return;

	_tmpSecPerFrm = 0;
	_fadeOutTime = fFadeOutTime;

	if (fFadeOutTime == 0.0f)
		StopImpl();
	else
		_state = SNDSTATE_FADEOUT;
}

void CN3SndObj::StopImpl()
{
	_isStarted = false;
	_state = SNDSTATE_STOP;

	ReleaseHandle();
}

void CN3SndObj::ReleaseHandle()
{
	if (_handle == nullptr)
		return;

	CN3Base::s_SndMgr.Remove(_handle);
	_handle.reset();
}

void CN3SndObj::SetPos(const __Vector3 vPos)
{
	if (_handle == nullptr
		|| GetType() != SNDTYPE_3D)
		return;

	CN3Base::s_SndMgr.QueueCallback(_handle, [=] (AudioHandle* handle)
	{
		alSource3f(handle->SourceId, AL_POSITION, vPos.x, vPos.y, vPos.z);
		AL_CLEAR_ERROR_STATE();
	});
}

void CN3SndObj::Looping(bool loop)
{
	_isLooping = loop;

	if (_handle == nullptr)
		return;

	_handle->IsLooping = true;

	// Streams need to manually handle their looping.
	if (_handle->HandleType == AUDIO_HANDLE_STREAMED)
		return;

	ALint isLooping = static_cast<ALint>(_isLooping);
	CN3Base::s_SndMgr.QueueCallback(_handle, [=] (AudioHandle* handle)
	{
		alSourcei(handle->SourceId, AL_LOOPING, isLooping);
		AL_CLEAR_ERROR_STATE();
	});
}

void CN3SndObj::SetListenerPos(const __Vector3& vPos)
{
	alListener3f(AL_POSITION, vPos.x, vPos.y, vPos.z);
	AL_CHECK_ERROR();
}

void CN3SndObj::SetListenerOrientation(const __Vector3& vAt, const __Vector3& vUp)
{
	ALfloat fv[6] =
	{
		vAt.x, vAt.y, vAt.z,
		vUp.x, vUp.y, vUp.z
	};

	alListenerfv(AL_ORIENTATION, fv);
	AL_CHECK_ERROR();
}
