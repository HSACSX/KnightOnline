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

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

struct ALCdevice;
struct ALCcontext;
struct LoadedSoundBuffer;
struct StreamedSoundBuffer;
class CN3SndObj;
class CN3SndMgr
{
	friend class CN3SndObj;

protected:
	CN3TableBase<__TABLE_SOUND>			m_Tbl_Source; // 사운드 소스 정보 테이블..

	bool								m_bSndEnable;
	std::list<CN3SndObj*>				m_SndObjStreams;	// 스트리밍 사운드..
	std::list<CN3SndObj*>				m_SndObjs;
	std::list<CN3SndObj*>				m_SndObjs_PlayOnceAndRelease;	// 한번만 플레이 하고 릴리즈 해야 하는 사운드들
	std::unordered_map<std::string, std::string> m_mp3ToWavFileMap;

	ALCdevice*	_alcDevice;
	ALCcontext*	_alcContext;

	std::list<uint32_t>				_unassignedSourceIds;
	std::list<uint32_t>				_unassignedStreamSourceIds;
	std::unordered_set<uint32_t>	_assignedSourceIds;
	std::unordered_set<uint32_t>	_assignedStreamSourceIds;
	std::mutex						_sourceIdMutex;

	std::unordered_map<std::string, std::shared_ptr<LoadedSoundBuffer>> _loadedSoundBufferByFilenameMap;
	std::mutex _loadedSoundBufferByFilenameMutex;

	std::unordered_map<std::string, std::shared_ptr<StreamedSoundBuffer>> _streamedSoundBufferByFilenameMap;
	std::mutex _streamedSoundBufferByFilenameMutex;

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

	CN3SndObj*	CreateObj(std::string szFN, e_SndType eType = SNDTYPE_3D);
	CN3SndObj*	CreateObj(int iID, e_SndType eType = SNDTYPE_3D);
	CN3SndObj*	CreateStreamObj(std::string szFN);
	CN3SndObj*	CreateStreamObj(int iID);

	CN3SndMgr();
	virtual ~CN3SndMgr();

private:
	bool PreprocessFilename(std::string& filename);
	bool DecodeMp3ToWav(std::string& filename);

protected:
	bool PullSourceFromPool(uint32_t* sourceId);
	void RestoreSourceToPool(uint32_t* sourceId);

	std::shared_ptr<LoadedSoundBuffer> GetLoadedSoundBuffer(const std::string& filename);
	void RemoveLoadedSoundBuffer(LoadedSoundBuffer* loadedSoundBuffer);

	void RemoveStreamedSoundBuffer(StreamedSoundBuffer* streamedSoundBuffer);
};

#endif // !defined(AFX_N3SNDMGR_H__9CB531B0_4FEB_4360_8141_D0BF61347BD7__INCLUDED_)
