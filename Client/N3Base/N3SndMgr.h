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
#include <unordered_map>

class CN3SndObj;
class CN3SndMgr
{
protected:
	CN3TableBase<__TABLE_SOUND>			m_Tbl_Source; // 사운드 소스 정보 테이블..

	bool								m_bSndEnable;
	bool								m_bSndDuplicated;
	std::map<std::string, CN3SndObj*>	m_SndObjSrcs;
	std::list<CN3SndObj*>				m_SndObjStreams;	// 스트리밍 사운드..
	std::list<CN3SndObj*>				m_SndObjs_Duplicated;
	std::list<CN3SndObj*>				m_SndObjs_PlayOnceAndRelease;	// 한번만 플레이 하고 릴리즈 해야 하는 사운드들
	std::unordered_map<std::string, std::string> m_mp3ToWavFileMap;
	
public:
	void SetDuplicated(bool bDuplicate)
	{
		m_bSndDuplicated = bDuplicate;
	}

	bool GetDuplicated() const
	{
		return m_bSndDuplicated;
	}

	void SetEnable(bool bEnable)
	{
		m_bSndEnable = bEnable;
	}

	void		ReleaseObj(CN3SndObj** ppObj);
	void		ReleaseStreamObj(CN3SndObj** ppObj);
	bool		PlayOnceAndRelease(int iSndID, const __Vector3* pPos = nullptr);
	void		Init(HWND hWnd);
	void		Release();
	void		Tick();	

	CN3SndObj*			CreateObj(std::string szFN, e_SndType eType = SNDTYPE_3D);
	CN3SndObj*			CreateObj(int iID, e_SndType eType = SNDTYPE_3D);
	CN3SndObj*			CreateStreamObj(std::string szFN);
	CN3SndObj*			CreateStreamObj(int iID);

	CN3SndMgr();
	virtual ~CN3SndMgr();

private:
	bool PreprocessFilename(std::string& filename);
	bool DecodeMp3ToWav(std::string& filename);
};

#endif // !defined(AFX_N3SNDMGR_H__9CB531B0_4FEB_4360_8141_D0BF61347BD7__INCLUDED_)
