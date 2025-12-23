// N3SndObj.h: interface for the CN3SndObj class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_N3SndObj_H__64BCBFD5_FD77_438D_9BF4_DC9B7C5D5BB9__INCLUDED_)
#define AFX_N3SndObj_H__64BCBFD5_FD77_438D_9BF4_DC9B7C5D5BB9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "N3SndDef.h"

#include <memory>
#include <string>

struct LoadedSoundBuffer;
struct StreamedSoundBuffer;
class CN3SndObj
{
protected:
	uint32_t				_sourceId;
	e_SndType				_type;
	e_SndState				_state;

	bool					_isStarted;
	bool					_isLooping;
	float					_currentVolume;
	float					_maxVolume;
	
	float					_fadeInTime;
	float					_fadeOutTime;
	float					_startDelayTime;
	float					_tmpSecPerFrm;

	std::shared_ptr<LoadedSoundBuffer>		_loadedSoundBuffer;
	std::shared_ptr<StreamedSoundBuffer>	_streamedSoundBuffer;

public:
	e_SndType GetType() const
	{
		return _type;
	}

	e_SndState GetState() const
	{
		return _state;
	}

	// Determines if the sound is considered started/active.
	// It may not necessarily be playing, but it can tell us if we've started it.
	bool IsStarted() const
	{
		return _isStarted;
	}

	void Looping(bool loop)
	{
		_isLooping = loop;
	}

	bool IsLooping() const
	{
		return _isLooping;
	}

	float GetVolume() const
	{
		return _currentVolume;
	}

	void SetMaxVolume(float maxVolume)
	{
		_maxVolume = maxVolume;
	}

	float GetMaxVolume() const
	{
		return _maxVolume;
	}

public:
	static void SetListenerPos(const __Vector3& vPos);
	static void SetListenerOrientation(const __Vector3& vAt, const __Vector3& vUp);

protected:
	void PlayImpl();
	void StopImpl();
	void RemoveSource();

public:
	const std::string& FileName() const;

	// Determines if the sound is effectively playing.
	// This fetches the live status.
	// If it can be avoided, use IsStarted() instead, as this
	// tells us whether we started playing it or not.
	bool	IsPlaying() const;
	void	SetVolume(float volume);	// range : [0.0f, 1.0f]

	void	Init();
	void	Release(); // 참조 카운트를 리턴 해준다.. 사운드 매니저에서는 이 참조 카운트를 보고 맵에서 지운다..
	bool	Create(const std::string& szFN, e_SndType eType);

	void	Play(const __Vector3* pvPos = nullptr, float delay = 0.0f, float fFadeInTime = 0.0f, bool bImmediately = true);
	void	Stop(float fFadeOutTime = 0.0f);
	void	Tick();

	void	SetConeOrientation(const __Vector3& vDir);
	void	SetRollOffFactor(float factor);
	void	SetDopplerFactor(float factor);
	void	SetMaxDistance(float max);
	void	SetMinDistance(float min);
	void	SetPos(const __Vector3& vPos);

	CN3SndObj();
	virtual ~CN3SndObj();
};

#endif // !defined(AFX_N3SndObj_H__64BCBFD5_FD77_438D_9BF4_DC9B7C5D5BB9__INCLUDED_)
