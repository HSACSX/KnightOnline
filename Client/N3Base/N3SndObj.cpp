// N3SndObj.cpp: implementation of the CN3SndObj class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfxBase.h"
#include "N3SndObj.h"
#include "N3Base.h"
#include "LoadedSoundBuffer.h"
#include "StreamedSoundBuffer.h"
#include "WaveFormat.h"
#include "al_wrapper.h"

#include <AL/alc.h>

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
	_sourceId = INVALID_SOURCE_ID;
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
	if (GetType() == SNDTYPE_2D
		|| GetType() == SNDTYPE_3D
		|| GetType() == SNDTYPE_STREAM) // TODO: Replace
	{
		RemoveSource();
	}
//	else if (GetSndType() == SNDTYPE_STREAM)
//	{
//		QueueStreamUpdate(SND_STREAM_UPDATE_RELEASE);
//	}

	if (_loadedSoundBuffer != nullptr)
	{
		CN3Base::s_SndMgr.RemoveLoadedSoundBuffer(_loadedSoundBuffer.get());
		_loadedSoundBuffer.reset();
	}

	if (_streamedSoundBuffer != nullptr)
	{
		CN3Base::s_SndMgr.RemoveStreamedSoundBuffer(_streamedSoundBuffer.get());
		_streamedSoundBuffer.reset();
	}
}

bool CN3SndObj::Create(const std::string& szFN, e_SndType eType)
{
	if (_sourceId != INVALID_SOURCE_ID)
		Init();

	if (eType == SNDTYPE_2D
		|| eType == SNDTYPE_3D
		|| eType == SNDTYPE_STREAM) // TODO: handle this separately
	{
		auto loadedSoundBuffer = CN3Base::s_SndMgr.GetLoadedSoundBuffer(szFN);
		if (loadedSoundBuffer == nullptr)
			return false;

		_loadedSoundBuffer = std::move(loadedSoundBuffer);
	}
	else
	{
		return false;
	}

	_type = eType;
	return true;
}

//	SetVolume
//	range : [0.0f, 1.0f]
void CN3SndObj::SetVolume(float currentVolume)
{
	if (_sourceId == INVALID_SOURCE_ID)
		return;

	_currentVolume = std::clamp(currentVolume, 0.0f, 1.0f);
	alSourcef(_sourceId, AL_GAIN, _currentVolume);
	AL_CHECK_ERROR();
}

const std::string& CN3SndObj::FileName() const
{
	if (_loadedSoundBuffer != nullptr)
		return _loadedSoundBuffer->Filename;

	if (_streamedSoundBuffer != nullptr)
		return _streamedSoundBuffer->Filename;

	static std::string empty;
	return empty;
}

bool CN3SndObj::IsPlaying() const
{
	if (_sourceId == INVALID_SOURCE_ID)
		return false;

	ALint state = 0;
	alGetSourcei(_sourceId, AL_SOURCE_STATE, &state);
	alGetError(); /* eat any errors, this is OK */

	return state == AL_PLAYING;
}

void CN3SndObj::Tick()
{
	if (_sourceId == INVALID_SOURCE_ID)
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
		if (!_isLooping)
			_state = SNDSTATE_STOP;
	}
	else if (GetState() == SNDSTATE_FADEOUT)
	{
		if (_tmpSecPerFrm >= _fadeOutTime)
		{
			_tmpSecPerFrm = 0;
			_state = SNDSTATE_STOP;
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
	if (pvPos != nullptr)
		SetPos(*pvPos);

	if (bImmediately)
		Stop();

	if (GetType() == SNDTYPE_3D
		|| GetType() == SNDTYPE_2D
		|| GetType() == SNDTYPE_STREAM) // TODO: replace this with streaming functionality
	{
		// This should've been setup by Create().
		if (_loadedSoundBuffer == nullptr)
			return;

		// Not yet loaded, we should assign a source now.
		if (_sourceId == INVALID_SOURCE_ID)
		{
			if (!CN3Base::s_SndMgr.PullSourceFromPool(&_sourceId))
				return;

			alSourcei(_sourceId, AL_BUFFER, _loadedSoundBuffer->BufferId);

			if (AL_CHECK_ERROR())
			{
				CN3Base::s_SndMgr.RestoreSourceToPool(&_sourceId);
				return;
			}

			if (GetType() == SNDTYPE_2D
				|| GetType() == SNDTYPE_STREAM)
			{
				alSourcei(_sourceId, AL_SOURCE_RELATIVE, AL_TRUE);
				alSource3f(_sourceId, AL_POSITION, 0.0f, 0.0f, 0.0f);
				alSourcef(_sourceId, AL_ROLLOFF_FACTOR, 0.0f);
			}
			else
			{
				SetRollOffFactor(0.5f);
				SetDopplerFactor(0.0f);
			}
		}

		alSourcei(_sourceId, AL_LOOPING, static_cast<ALint>(_isLooping));
		AL_CHECK_ERROR();
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

	if (delay == 0.0f
		&& fFadeInTime == 0.0f)
	{
		SetVolume(_maxVolume);
		PlayImpl();
	}
}

void CN3SndObj::PlayImpl()
{
	if (_sourceId == INVALID_SOURCE_ID)
		return;

	_isStarted = true;

	if (!IsPlaying())
	{
//		if (GetSndType() == SNDTYPE_STREAM)
//			QueueStreamUpdate(SND_STREAM_UPDATE_INIT);
//		else
			alSourcePlay(_sourceId);

		AL_CHECK_ERROR();
	}
}

void CN3SndObj::Stop(float fFadeOutTime)
{
	_isStarted = false;

	if (_sourceId == INVALID_SOURCE_ID)
		return;

	if (GetState() == SNDSTATE_FADEOUT
		|| GetState()  == SNDSTATE_STOP)
		return;

	_tmpSecPerFrm = 0;
	_fadeOutTime = fFadeOutTime;

	if (fFadeOutTime == 0.0f)
	{
		_state = SNDSTATE_STOP;
		StopImpl();
	}
	else
	{
		_state = SNDSTATE_FADEOUT;
	}
}

void CN3SndObj::StopImpl()
{
	_isStarted = false;

	if (_sourceId == INVALID_SOURCE_ID)
		return;

	RemoveSource();
}

void CN3SndObj::RemoveSource()
{
	//	if (GetType() == SNDTYPE_STREAM)
//		QueueStreamUpdate(SND_STREAM_UPDATE_ONLY_REMOVE);
//	else
	{
		if (_sourceId != INVALID_SOURCE_ID)
		{
			alSourceStop(_sourceId);
			alGetError(); /* ignore errors */

			alSourceRewind(_sourceId);
			alGetError(); /* ignore errors */

			alSourcei(_sourceId, AL_BUFFER, 0);
			alGetError(); /* ignore errors */

			CN3Base::s_SndMgr.RestoreSourceToPool(&_sourceId);
		}
	}
}

void CN3SndObj::SetPos(const __Vector3& vPos)
{
	if (_sourceId == INVALID_SOURCE_ID
		|| GetType() != SNDTYPE_3D)
		return;

	alSource3f(_sourceId, AL_POSITION, vPos.x, vPos.y, vPos.z);
	AL_CHECK_ERROR();
}

void CN3SndObj::SetMaxDistance(float max)
{
	if (_sourceId == INVALID_SOURCE_ID
		|| GetType() != SNDTYPE_3D)
		return;

	alSourcef(_sourceId, AL_MAX_DISTANCE, max);
	AL_CHECK_ERROR();
}

void CN3SndObj::SetMinDistance(float min)
{
	if (_sourceId == INVALID_SOURCE_ID
		|| GetType() != SNDTYPE_3D)
		return;

	alSourcef(_sourceId, AL_REFERENCE_DISTANCE, min);
	AL_CHECK_ERROR();
}

void CN3SndObj::SetConeOrientation(const __Vector3& vDir)
{
	if (_sourceId == INVALID_SOURCE_ID
		|| GetType() != SNDTYPE_3D)
		return;
		
	alSource3f(_sourceId, AL_DIRECTION, vDir.x, vDir.y, vDir.z);
	AL_CHECK_ERROR();
}

void CN3SndObj::SetRollOffFactor(float factor)
{
	if (_sourceId == INVALID_SOURCE_ID
		|| GetType() != SNDTYPE_3D)
		return;

	alSourcef(_sourceId, AL_ROLLOFF_FACTOR, factor);
	AL_CHECK_ERROR();
}

void CN3SndObj::SetDopplerFactor(float factor)
{
	if (_sourceId == INVALID_SOURCE_ID
		|| GetType() != SNDTYPE_3D)
		return;

	alSourcef(_sourceId, AL_DOPPLER_FACTOR, factor);
	AL_CHECK_ERROR();
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
