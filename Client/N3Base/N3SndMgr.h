// N3SndMgr.h: interface for the CN3SndMgr class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_N3SNDMGR_H__9CB531B0_4FEB_4360_8141_D0BF61347BD7__INCLUDED_)
#define AFX_N3SNDMGR_H__9CB531B0_4FEB_4360_8141_D0BF61347BD7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "N3SndDef.h"
#include "N3TableBase.h"

#include <list>				// std::list<>
#include <memory>			// std::unique_ptr<>
#include <mutex>			// std::mutex
#include <string>			// std::string
#include <unordered_map>	// std::unordered_map<>
#include <unordered_set>	// std::unordered_set<>

#include "AudioThread.h"

struct ALCcontext;
struct ALCdevice;
class AudioAsset;
class BufferedAudioAsset;
class CN3SndObj;
class StreamedAudioAsset;
class CN3SndMgr
{
	friend class BufferedAudioHandle;
	friend class StreamedAudioHandle;
	friend class CN3SndObj;

protected:
	CN3TableBase<__TABLE_SOUND>			m_Tbl_Source; // 사운드 소스 정보 테이블..

	bool								m_bSndEnable;
	std::list<CN3SndObj*>				m_SndObjStreams;	// 스트리밍 사운드..
	std::list<CN3SndObj*>				m_SndObjs;
	std::list<CN3SndObj*>				m_SndObjs_PlayOnceAndRelease;	// 한번만 플레이 하고 릴리즈 해야 하는 사운드들

	ALCdevice*	_alcDevice;
	ALCcontext*	_alcContext;

	std::list<uint32_t>				_unassignedSourceIds;
	std::list<uint32_t>				_unassignedStreamSourceIds;
	std::unordered_set<uint32_t>	_assignedSourceIds;
	std::unordered_set<uint32_t>	_assignedStreamSourceIds;
	std::mutex						_sourceIdMutex;

	std::unordered_map<std::string, std::shared_ptr<BufferedAudioAsset>> _bufferedAudioAssetByFilenameMap;
	std::mutex _bufferedAudioAssetByFilenameMutex;

	std::unordered_map<std::string, std::shared_ptr<StreamedAudioAsset>> _streamedAudioAssetByFilenameMap;
	std::mutex _streamedAudioAssetByFilenameMutex;

	std::unique_ptr<AudioThread> _thread;

public:
	void SetEnable(bool bEnable)
	{
		m_bSndEnable = bEnable;
	}

	void		ReleaseObj(CN3SndObj** ppObj);
	void		ReleaseStreamObj(CN3SndObj** ppObj);
	bool		PlayOnceAndRelease(int iSndID, const __Vector3* pPos = nullptr);
	void		Init();
	bool		InitOpenAL();
	void		Release();
	void		ReleaseOpenAL();
	void		Tick();	

	CN3SndObj*	CreateObj(const std::string& szFN, e_SndType eType = SNDTYPE_3D);
	CN3SndObj*	CreateObj(int iID, e_SndType eType = SNDTYPE_3D);
	CN3SndObj*	CreateStreamObj(const std::string& szFN);
	CN3SndObj*	CreateStreamObj(int iID);

	CN3SndMgr();
	virtual ~CN3SndMgr();

protected:
	bool PullBufferedSourceIdFromPool(uint32_t* sourceId);
	void RestoreBufferedSourceIdToPool(uint32_t* sourceId);

	bool PullStreamedSourceIdFromPool(uint32_t* sourceId);
	void RestoreStreamedSourceIdToPool(uint32_t* sourceId);

	std::shared_ptr<BufferedAudioAsset> LoadBufferedAudioAsset(const std::string& filename);
	std::shared_ptr<StreamedAudioAsset> LoadStreamedAudioAsset(const std::string& filename);
	void RemoveAudioAsset(AudioAsset* audioAsset);

	void Add(std::shared_ptr<AudioHandle> handle);
	void QueueCallback(std::shared_ptr<AudioHandle> handle, AudioThread::AudioCallback callback);
	void Remove(std::shared_ptr<AudioHandle> handle);
};

#endif // !defined(AFX_N3SNDMGR_H__9CB531B0_4FEB_4360_8141_D0BF61347BD7__INCLUDED_)
