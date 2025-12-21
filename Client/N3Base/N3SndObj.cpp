// N3SndObj.cpp: implementation of the CN3SndObj class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfxBase.h"
#include "N3SndObj.h"
#include "N3Base.h"
#include <math.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

LPDIRECTSOUND				CN3SndObj::s_lpDS = nullptr;
LPDIRECTSOUND3DLISTENER		CN3SndObj::s_lpDSListener = nullptr;	// 3D listener object
DS3DLISTENER				CN3SndObj::s_dsListenerParams;			// Listener properties
bool						CN3SndObj::s_bNeedDeferredTick = false;	// 3D Listener CommitDeferredSetting

CN3SndObj::CN3SndObj()
{
	m_lpDSBuff = nullptr;
	m_lpDS3DBuff = nullptr;
	m_bIsLoop = false;
	m_iVol = -1;

	m_fFadeInTime = 0;
	m_fFadeOutTime = 0;
	m_fStartDelayTime = 0;
	m_fTmpSecPerFrm = 0;
	m_ePlayState = SNDSTATE_STOP;

	m_iMaxVolume = 100;
}

CN3SndObj::~CN3SndObj()
{
	Release();
}

//
//	Initialize....
//
void CN3SndObj::Init()
{
	Release();

	m_bIsLoop = false;
	m_iVol = -1;
	m_szFileName.clear();

	m_fStartDelayTime = 0;
	m_fTmpSecPerFrm = 0;
	m_ePlayState = SNDSTATE_STOP;
	m_fFadeInTime = 0;
	m_fFadeOutTime = 0;

	m_iMaxVolume = 100;
}

//
//	Release...
//
void CN3SndObj::Release()
{
	if (m_lpDS3DBuff != nullptr)
	{
		m_lpDS3DBuff->Release();
		m_lpDS3DBuff = nullptr;
	}

	if (m_lpDSBuff != nullptr)
	{
		m_lpDSBuff->Stop();
		m_lpDSBuff->Release();
		m_lpDSBuff = nullptr;
	}
}

bool CN3SndObj::StaticInit(HWND  hWnd, DWORD dwCoopLevel, DWORD dwPrimaryChannels, DWORD dwPrimaryFreq, DWORD dwPrimaryBitRate)
{
	HRESULT hr;
	LPDIRECTSOUNDBUFFER lpDSBPrimary = nullptr;

	if (s_lpDS != nullptr)
	{
		s_lpDS->Release();
		s_lpDS = nullptr;
	}

	// Create IDirectSound using the primary sound device
	hr = DirectSoundCreate(nullptr, &s_lpDS, nullptr);
	if (FAILED(hr))
		return false;

	// Set DirectSound coop level 
	hr = s_lpDS->SetCooperativeLevel(hWnd, dwCoopLevel);
	if (FAILED(hr))
		return false;

	// Set primary buffer format
	// Get the primary buffer 
	DSBUFFERDESC dsbd = {};
	dsbd.dwSize = sizeof(DSBUFFERDESC);
	dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRL3D | DSBCAPS_MUTE3DATMAXDISTANCE;
	dsbd.dwBufferBytes = 0;
	dsbd.lpwfxFormat = nullptr;
	hr = s_lpDS->CreateSoundBuffer(&dsbd, &lpDSBPrimary, nullptr);
	if (FAILED(hr))
		return false;

	WAVEFORMATEX wfx = {};
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = (uint16_t) dwPrimaryChannels;
	wfx.nSamplesPerSec = dwPrimaryFreq;
	wfx.wBitsPerSample = (uint16_t) dwPrimaryBitRate;
	wfx.nBlockAlign = wfx.wBitsPerSample / 8 * wfx.nChannels;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	hr = lpDSBPrimary->SetFormat(&wfx);
	if (FAILED(hr))
		return false;

	if (lpDSBPrimary != nullptr)
	{
		lpDSBPrimary->Release();
		lpDSBPrimary = nullptr;
	}

	// Create listener
	// Obtain primary buffer, asking it for 3D control
	memset(&dsbd, 0, sizeof(dsbd));
	dsbd.dwSize = sizeof(DSBUFFERDESC);
	dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_MUTE3DATMAXDISTANCE | DSBCAPS_CTRL3D;
	hr = s_lpDS->CreateSoundBuffer(&dsbd, &lpDSBPrimary, nullptr);
	if (FAILED(hr))
		return false;

	hr = lpDSBPrimary->QueryInterface(IID_IDirectSound3DListener, (VOID**) (&s_lpDSListener));
	if (FAILED(hr))
	{
		if (lpDSBPrimary != nullptr)
		{
			lpDSBPrimary->Release();
			lpDSBPrimary = nullptr;
		}

		return false;
	}

	if (lpDSBPrimary != nullptr)
	{
		lpDSBPrimary->Release();
		lpDSBPrimary = nullptr;
	}

	// Set listener 
	s_dsListenerParams.dwSize = sizeof(DS3DLISTENER);
	s_lpDSListener->GetAllParameters(&s_dsListenerParams);
	s_lpDSListener->SetRolloffFactor(DS3D_DEFAULTROLLOFFFACTOR / 2, DS3D_IMMEDIATE);
	s_lpDSListener->SetDopplerFactor(0, DS3D_DEFERRED);

	s_bNeedDeferredTick = true;	// 3D Listener CommitDeferredSetting
	return true;
}

void CN3SndObj::StaticRelease()
{
	if (s_lpDSListener != nullptr)
	{
		s_lpDSListener->Release();
		s_lpDSListener = nullptr;
	}

	if (s_lpDS != nullptr)
	{
		s_lpDS->Release();
		s_lpDS = nullptr;
	}

	s_bNeedDeferredTick = false;	// 3D Listener CommitDeferredSetting
}

void CN3SndObj::StaticTick()
{
	if (!s_bNeedDeferredTick)
		return;

	// 3D Listener CommitDeferredSetting
	s_lpDSListener->CommitDeferredSettings();
	s_bNeedDeferredTick = false;
}

bool CN3SndObj::Create(const std::string& szFN, e_SndType eType)
{
	if (s_lpDS == nullptr)
		return false;

	if (m_lpDSBuff != nullptr)
		Init();

	CWaveFile WaveFile;
	HRESULT hr = WaveFile.Open(szFN.c_str(), nullptr, 1);	//#define WAVEFILE_READ   1
	if (FAILED(hr))
	{
#ifdef _N3GAME
		if (!szFN.empty())
			CLogWriter::Write("CN3SndObj::Create - WaveFile Open Failed.. ({})", szFN);
#endif
		return false;
	}

	DSBUFFERDESC dsbd = {};
	dsbd.dwSize = sizeof(DSBUFFERDESC);
	dsbd.dwBufferBytes = WaveFile.GetSize();
	dsbd.lpwfxFormat = WaveFile.m_pwfx;

	// 2D 음원
	if (eType == SNDTYPE_2D || eType == SNDTYPE_STREAM)
	{
		dsbd.dwFlags = DSBCAPS_CTRLVOLUME; // | DSBCAPS_STATIC;
	}
	// 3D 음원..
	else if (eType == SNDTYPE_3D)
	{
		dsbd.dwFlags = DSBCAPS_CTRL3D | DSBCAPS_MUTE3DATMAXDISTANCE; // | DSBCAPS_STATIC;
		dsbd.guid3DAlgorithm = DS3DALG_HRTF_LIGHT;
	}

	hr = s_lpDS->CreateSoundBuffer(&dsbd, &m_lpDSBuff, nullptr);
	if (FAILED(hr))
	{
#ifdef _N3GAME
		CLogWriter::Write("CN3SndObj::Create - CreateSoundBuffer Failed.. ({})", szFN);
#endif
		return false;
	}

	if (!FillBufferWithSound(&WaveFile))
	{
#ifdef _N3GAME
		CLogWriter::Write("CN3SndObj::Create - FillBufferWithSound Failed.. ({})", szFN);
#endif
		return false;
	}

	m_lpDSBuff->SetCurrentPosition(0);
	if (SNDTYPE_3D == eType)	//3D 음원..
	{
		if (S_OK != m_lpDSBuff->QueryInterface(IID_IDirectSound3DBuffer, (VOID**) (&m_lpDS3DBuff)))
			return false;
	}

	m_szFileName = szFN; // 파일 이름을 기록한다..

	s_bNeedDeferredTick = true;	// 3D Listener CommitDeferredSetting
	return true;
}

bool CN3SndObj::Duplicate(CN3SndObj* pSrc, e_SndType eType, __Vector3* pPos)
{
	if (s_lpDS == nullptr
		|| pSrc == nullptr
		|| pSrc->m_lpDSBuff == nullptr)
		return false;

	if (m_lpDSBuff != nullptr)
		Init();

	HRESULT hr = 0;
	if (DS_OK != s_lpDS->DuplicateSoundBuffer(pSrc->m_lpDSBuff, &m_lpDSBuff))
		return false;

	m_lpDSBuff->SetCurrentPosition(0);
	if (eType == SNDTYPE_3D)
	{
		if (S_OK != m_lpDSBuff->QueryInterface(IID_IDirectSound3DBuffer, (VOID**) (&m_lpDS3DBuff)))
			return false;

		if (pPos != nullptr)
			SetPos(*pPos);

		SetMaxDistance(100);

		__Vector3 vDir = { 0.0f, 1.0f, 0.0f };
		SetConeOrientation(vDir);
	}

	s_bNeedDeferredTick = true;	// 3D Listener CommitDeferredSetting
	m_szFileName = pSrc->m_szFileName;
	return true;
}

bool CN3SndObj::FillBufferWithSound(CWaveFile* pWaveFile)
{
	if (m_lpDSBuff == nullptr || pWaveFile == nullptr)
		return false; // 포인터들 점검..

	HRESULT hr;
	void* pDSLockedBuffer = nullptr; // Pointer to locked buffer memory
	DWORD   dwDSLockedBufferSize = 0;    // Size of the locked DirectSound buffer
	DWORD   dwWavDataRead = 0;    // Amount of data read from the wav file 

	DSBCAPS dsbc; dsbc.dwSize = sizeof(dsbc);
	m_lpDSBuff->GetCaps(&dsbc);
	if (dsbc.dwBufferBytes != pWaveFile->GetSize())
		return false; // 사이즈 점검..

	if (!RestoreBuffer())
		return false;

	// Lock the buffer down
	hr = m_lpDSBuff->Lock(0, dsbc.dwBufferBytes, &pDSLockedBuffer, &dwDSLockedBufferSize, nullptr, nullptr, 0L);
	if (FAILED(hr))
		return false;

	pWaveFile->ResetFile();

	hr = pWaveFile->Read((uint8_t*) pDSLockedBuffer, dwDSLockedBufferSize, &dwWavDataRead);
	if (FAILED(hr))
		return false;

	if (dwWavDataRead == 0)
	{
		// Wav is blank, so just fill with silence
		memset(pDSLockedBuffer, (uint8_t) (pWaveFile->m_pwfx->wBitsPerSample == 8 ? 128 : 0), dwDSLockedBufferSize);
	}
	else if (dwWavDataRead < dwDSLockedBufferSize)
	{
		// just fill in silence 
		memset((uint8_t*) pDSLockedBuffer + dwWavDataRead,
			(uint8_t) (pWaveFile->m_pwfx->wBitsPerSample == 8 ? 128 : 0),
			dwDSLockedBufferSize - dwWavDataRead);
	}

	// Unlock the buffer, we don't need it anymore.
	m_lpDSBuff->Unlock(pDSLockedBuffer, dwDSLockedBufferSize, nullptr, 0);

	return true;
}

bool CN3SndObj::RestoreBuffer()
{
	if (m_lpDSBuff == nullptr)
		return false;

	HRESULT hr;
	DWORD dwStatus;
	hr = m_lpDSBuff->GetStatus(&dwStatus);
	if (FAILED(hr))
		return false;

	if (dwStatus & DSBSTATUS_BUFFERLOST)
	{
		// Since the app could have just been activated, then
		// DirectSound may not be giving us control yet, so 
		// the restoring the buffer may fail.  
		// If it does, sleep until DirectSound gives us control.
		hr = m_lpDSBuff->Restore();
		while (FAILED(hr))
		{
			if (hr == DSERR_BUFFERLOST)
				Sleep(10);

			hr = m_lpDSBuff->Restore();
		}
	}

	return true;
}

//
//	SetVolume...
//	range : [0,100]
//
void CN3SndObj::SetVolume(int Vol)
{
	if (m_lpDSBuff == nullptr)
		return;

	// 3D Sound 일때는 소리 조절이 안된다..!!!
	if (m_lpDS3DBuff != nullptr)
		return;

	m_iVol = Vol;
	if (Vol == 0)
	{
		m_lpDSBuff->SetVolume(-10000);
		return;
	}

	float fVol = (float) (Vol) / 100.0f;
	long dwVol = (long) (log10(fVol) * 3000);	//데시벨 관련 소리조절식..
	m_lpDSBuff->SetVolume(dwVol);
}

bool CN3SndObj::IsPlaying()
{
	if (m_lpDSBuff == nullptr)
		return false;

	DWORD dwStatus = 0;
	m_lpDSBuff->GetStatus(&dwStatus);
	if (dwStatus & DSBSTATUS_PLAYING)
		return true;

	return (m_ePlayState != SNDSTATE_STOP);
}

void CN3SndObj::Tick()
{
	if (m_lpDSBuff == nullptr || m_ePlayState == SNDSTATE_STOP)
		return;

	m_fTmpSecPerFrm += CN3Base::s_fSecPerFrm;

	if (m_ePlayState == SNDSTATE_DELAY && m_fTmpSecPerFrm >= m_fStartDelayTime)
	{
		m_fTmpSecPerFrm = 0;
		m_ePlayState = SNDSTATE_FADEIN;
		RealPlay();
	}

	if (m_ePlayState == SNDSTATE_FADEIN)
	{
		if (m_fTmpSecPerFrm >= m_fFadeInTime)
		{
			m_fTmpSecPerFrm = 0;
			m_ePlayState = SNDSTATE_PLAY;
			SetVolume(m_iMaxVolume);
		}
		else
		{
			int vol = 0;
			if (m_fFadeInTime > 0.0f)
				vol = (int) ((m_fTmpSecPerFrm / m_fFadeInTime) * (float) m_iMaxVolume);

			SetVolume(vol);
		}
	}

	if (m_ePlayState == SNDSTATE_PLAY)
	{
		if (!m_bIsLoop)
			m_ePlayState = SNDSTATE_STOP;
	}
	else if (m_ePlayState == SNDSTATE_FADEOUT)
	{
		if (m_fTmpSecPerFrm >= m_fFadeOutTime)
		{
			m_fTmpSecPerFrm = 0;
			m_ePlayState = SNDSTATE_STOP;
			SetVolume(0);
			m_lpDSBuff->Stop();
		}
		else
		{
			// 볼륨 점점 작게....
			int vol = 0;
			if (m_fFadeOutTime > 0.0f)
				vol = (int) (((m_fFadeOutTime - m_fTmpSecPerFrm) / m_fFadeOutTime) * (float) m_iMaxVolume);
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

	m_fFadeInTime = fFadeInTime;
	m_fFadeOutTime = 0;
	m_fStartDelayTime = delay;
	m_fTmpSecPerFrm = 0;
	m_ePlayState = SNDSTATE_DELAY;

	// 3D 사운드일때에는 FadeIn 등이 필요 없구.. 볼륨이 먹지 않기 때문에 리턴..
	if (m_lpDS3DBuff != nullptr)
	{
		m_ePlayState = SNDSTATE_PLAY;

		if (m_lpDSBuff != nullptr)
		{
			if (m_bIsLoop)
				m_lpDSBuff->Play(0, 0, DSBPLAY_LOOPING);
			else
				m_lpDSBuff->Play(0, 0, 0);
		}
	}
}

void CN3SndObj::RealPlay()
{
	if (m_lpDSBuff == nullptr)
		return;

	DWORD dwStatus = 0;
	m_lpDSBuff->GetStatus(&dwStatus);
	if ((dwStatus & DSBSTATUS_PLAYING) == DSBSTATUS_PLAYING)
		return;

	m_lpDSBuff->SetCurrentPosition(0);

	if (m_bIsLoop)
		m_lpDSBuff->Play(0, 0, DSBPLAY_LOOPING);
	else
		m_lpDSBuff->Play(0, 0, 0);
}

void CN3SndObj::Stop(float fFadeOutTime)
{
	if (m_lpDSBuff == nullptr)
		return;

	if (m_ePlayState == SNDSTATE_FADEOUT || m_ePlayState == SNDSTATE_STOP)
		return;

	m_fTmpSecPerFrm = 0;
	m_fFadeOutTime = fFadeOutTime;

	if (fFadeOutTime == 0.0f)
	{
		m_ePlayState = SNDSTATE_STOP;
		m_lpDSBuff->Stop();
	}
	else
	{
		m_ePlayState = SNDSTATE_FADEOUT;
	}
}

void CN3SndObj::SetPos(const __Vector3& vPos)
{
	if (m_lpDS3DBuff != nullptr)
		m_lpDS3DBuff->SetPosition(vPos.x, vPos.y, vPos.z, DS3D_IMMEDIATE);
}

void CN3SndObj::SetMaxDistance(float max)
{
	if (m_lpDS3DBuff != nullptr)
		m_lpDS3DBuff->SetMaxDistance(max, DS3D_IMMEDIATE);
}

void CN3SndObj::SetMinDistance(float min)
{
	if (m_lpDS3DBuff != nullptr)
		m_lpDS3DBuff->SetMinDistance(min, DS3D_IMMEDIATE);
}

void CN3SndObj::SetConeOrientation(const __Vector3& vDir)
{
	if (m_lpDS3DBuff != nullptr)
		m_lpDS3DBuff->SetConeOrientation(vDir.x, vDir.y, vDir.z, DS3D_IMMEDIATE);
}

//
// static functions ....
//

void CN3SndObj::SetDopplerFactor(float factor)
{
	if (s_lpDSListener == nullptr)
		return;

	s_lpDSListener->SetDopplerFactor(factor, DS3D_DEFERRED);
	s_bNeedDeferredTick = true;	// 3D Listener CommitDeferredSetting
}

void CN3SndObj::SetListenerPos(const __Vector3& vPos, bool IsDeferred)
{
	if (s_lpDSListener == nullptr)
		return;

	DWORD dwParam = IsDeferred ? DS3D_DEFERRED : DS3D_IMMEDIATE;
	s_lpDSListener->SetPosition(vPos.x, vPos.y, vPos.z, dwParam);
	s_bNeedDeferredTick = true;	// 3D Listener CommitDeferredSetting
}

void CN3SndObj::SetListenerOrientation(const __Vector3& vAt, const __Vector3& vUp, bool IsDeferred)
{
	if (s_lpDSListener == nullptr)
		return;

	DWORD dwParam = IsDeferred ? DS3D_DEFERRED : DS3D_IMMEDIATE;
	s_lpDSListener->SetOrientation(vAt.x, vAt.y, vAt.z, vUp.x, vUp.y, vUp.z, dwParam);
	s_bNeedDeferredTick = true;	// 3D Listener CommitDeferredSetting
}
