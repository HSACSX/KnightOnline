// N3SndMgr.cpp: implementation of the CN3SndMgr class.
//
//////////////////////////////////////////////////////////////////////
#include "StdAfxBase.h"
#include "N3SndMgr.h"
#include "N3SndObj.h"
#include "N3Base.h"

#include <mpg123.h>
#include <filesystem>

#include <shlobj.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#endif

CN3SndMgr::CN3SndMgr()
{
	m_bSndEnable = false;
	m_bSndDuplicated = false;
}

CN3SndMgr::~CN3SndMgr()
{
	Release();
}

//
//	엔진 초기화..
//
void CN3SndMgr::Init(HWND hWnd)
{
	Release();
	m_bSndEnable = CN3SndObj::StaticInit(hWnd);
	m_Tbl_Source.LoadFromFile("Data\\sound.tbl");
}

CN3SndObj* CN3SndMgr::CreateObj(int iID, e_SndType eType)
{
	TABLE_SOUND* pTbl = m_Tbl_Source.Find(iID);
	if (pTbl == nullptr)
		return nullptr;

	return CreateObj(pTbl->szFN, eType);
}

CN3SndObj* CN3SndMgr::CreateObj(std::string szFN, e_SndType eType)
{
	if (!m_bSndEnable)
		return nullptr;

	if (eType == SNDTYPE_STREAM)
	{
		if (!CN3Base::s_Options.bSndBgmEnable)
			return nullptr;
	}
	else
	{
		if (!CN3Base::s_Options.bSndEffectEnable)
			return nullptr;
	}

	if (!PreprocessFilename(szFN))
		return nullptr;

	CN3SndObj* pObjSrc = nullptr;

	auto it = m_SndObjSrcs.find(szFN);

	// 못 찾았다... 새로 만들자..
	if (it == m_SndObjSrcs.end())
	{
		pObjSrc = new CN3SndObj();

		// 새로 로딩..
		if (!pObjSrc->Create(szFN, eType))
		{
			delete pObjSrc;
			return nullptr;
		}

		m_SndObjSrcs.insert(std::make_pair(szFN, pObjSrc)); // 맵에 추가한다..
	}
	else
	{
		pObjSrc = it->second;
	}

	if (!m_bSndDuplicated)
		return pObjSrc;

	if (pObjSrc == nullptr)
		return nullptr;

	CN3SndObj* pObjNew = new CN3SndObj();

	// Duplicate 처리..
	if (!pObjNew->Duplicate(pObjSrc, eType))
	{
		delete pObjNew;
		return nullptr;
	}

	if (pObjNew != nullptr)
		m_SndObjs_Duplicated.push_back(pObjNew); // 리스트에 넣는다...

	return pObjNew;
}

CN3SndObj* CN3SndMgr::CreateStreamObj(std::string szFN)
{
	if (!CN3Base::s_Options.bSndBgmEnable)
		return nullptr;

	if (!PreprocessFilename(szFN))
		return nullptr;

	CN3SndObj* pSndObj = new CN3SndObj();
	if (!pSndObj->Create(szFN, SNDTYPE_STREAM))
	{
		delete pSndObj;
		return nullptr;
	}

	m_SndObjStreams.push_back(pSndObj); // 리스트에 넣기..
	return pSndObj;
}

CN3SndObj* CN3SndMgr::CreateStreamObj(int iID)
{
	__TABLE_SOUND* pTbl = m_Tbl_Source.Find(iID);
	if (pTbl == nullptr)
		return nullptr;

	return CreateStreamObj(pTbl->szFN);
}

void CN3SndMgr::ReleaseStreamObj(CN3SndObj** ppObj)
{
	if (ppObj == nullptr || *ppObj == nullptr)
		return;

	auto it = m_SndObjStreams.begin(), itEnd = m_SndObjStreams.end();
	for (; it != itEnd; ++it)
	{
		if (*ppObj == *it)
		{
			delete *ppObj;
			*ppObj = nullptr;
			m_SndObjStreams.erase(it);
			break;
		}
	}
}


//
//	TickTick...^^
//
void CN3SndMgr::Tick()
{
	if (!m_bSndEnable)
		return;

	if (m_bSndDuplicated)
	{
		for (CN3SndObj* pSndObj : m_SndObjs_Duplicated)
			pSndObj->Tick();
	}
	else
	{
		for (auto& [_, pSndObj] : m_SndObjSrcs)
			pSndObj->Tick();
	}

	auto it = m_SndObjs_PlayOnceAndRelease.begin(), itEnd = m_SndObjs_PlayOnceAndRelease.end();
	while (it != itEnd)
	{
		CN3SndObj* pSndObj = *it;
		pSndObj->Tick();

		if (pSndObj->IsPlaying())
		{
			++it;
		}
		else
		{
			delete pSndObj;
			it = m_SndObjs_PlayOnceAndRelease.erase(it);
		}
	}

	for (CN3SndObj* pSndObj : m_SndObjStreams)
		pSndObj->Tick();

	CN3SndObj::StaticTick(); // CommitDeferredSetting...
}

//
//	Obj하나 무효화..
void CN3SndMgr::ReleaseObj(CN3SndObj** ppObj)
{
	if (ppObj == nullptr || *ppObj == nullptr)
		return;

	auto it = m_SndObjs_Duplicated.begin(), itEnd = m_SndObjs_Duplicated.end();
	for (; it != itEnd; ++it)
	{
		if (*ppObj == *it)
		{
			m_SndObjs_Duplicated.erase(it);

			// 객체 지우기..
			delete* ppObj;
			*ppObj = nullptr;
			return;
		}
	}

	it = m_SndObjs_PlayOnceAndRelease.begin();
	itEnd = m_SndObjs_PlayOnceAndRelease.end();
	for (; it != itEnd; ++it)
	{
		if (*ppObj == *it)
		{
			m_SndObjs_PlayOnceAndRelease.erase(it);

			// 객체 지우기..
			delete* ppObj;
			*ppObj = nullptr;
			return;
		}
	}

	*ppObj = nullptr; // 포인터만 널로 만들어 준다..
}

//
//	Release Whole Objects & Sound Sources & Sound Engine..
//
void CN3SndMgr::Release()
{
	if (!m_bSndEnable)
		return;

	for (auto& [_, pSndObj] : m_SndObjSrcs)
		delete pSndObj;
	m_SndObjSrcs.clear();

	for (CN3SndObj* pSndObj : m_SndObjs_Duplicated)
		delete pSndObj;
	m_SndObjs_Duplicated.clear();

	for (CN3SndObj* pSndObj : m_SndObjs_PlayOnceAndRelease)
		delete pSndObj;
	m_SndObjs_PlayOnceAndRelease.clear();

	for (CN3SndObj* pSndObj : m_SndObjStreams)
		delete pSndObj;
	m_SndObjStreams.clear();

	CN3SndObj::StaticRelease();
}

// 이 함수는 한번 플레이 하고 그 포인터를 다시 쓸수있게 ReleaseObj를 호출한다.
// 대신 위치는 처음 한번밖에 지정할 수 없다.
bool CN3SndMgr::PlayOnceAndRelease(int iSndID, const __Vector3* pPos)
{
	if (!m_bSndEnable)
		return false;

	if (!CN3Base::s_Options.bSndEffectEnable)
		return false;

	TABLE_SOUND* pTbl = m_Tbl_Source.Find(iSndID);
	if (pTbl == nullptr || pTbl->szFN.empty())
		return false;

	CN3SndObj* pObjSrc = nullptr;

	auto it = m_SndObjSrcs.find(pTbl->szFN);

	// 못 찾았다... 새로 만들자..
	if (it == m_SndObjSrcs.end())
	{
		pObjSrc = new CN3SndObj();

		// 새로 로딩..
		if (!pObjSrc->Create(pTbl->szFN, static_cast<e_SndType>(pTbl->iType)))
		{
			delete pObjSrc;
			return false;
		}

		m_SndObjSrcs.insert(std::make_pair(pTbl->szFN, pObjSrc)); // 맵에 추가한다..
		if (!m_bSndDuplicated)
			pObjSrc->Play(pPos);
	}
	else
	{
		pObjSrc = it->second;
	}

	if (pObjSrc == nullptr)
		return false;

	if (!m_bSndDuplicated)
	{
		pObjSrc->Play(pPos);
		return true;
	}

	CN3SndObj* pObj = new CN3SndObj();
	if (!pObj->Duplicate(pObjSrc, (e_SndType) pTbl->iType)) // Duplicate 처리..
	{
		delete pObj;
		return false;
	}

	// 리스트에 넣는다...noah
	if (pObj == nullptr)
		return false;

	m_SndObjs_PlayOnceAndRelease.push_back(pObj);
	pObj->Play(pPos);
	return true;
}

bool CN3SndMgr::PreprocessFilename(std::string& szFN)
{
	// Expect an extension (.mp3, .wav)
	if (szFN.length() < 4)
		return false;

	// Officially it has native support for MP3 decoding.
	// We decode back to WAV (one-time) and use the new filename instead.
	// Ideally this would just load it into memory directly, but that would require restructuring
	// a little.
	// This approach is fairly unintrusive and only incurs the performance hit once.
	// There's also very few mp3 files to actually convert, so space should not be an issue.
	// Finally, the game requires admin access, so it should have write access.
	if (_stricmp(&szFN[szFN.length() - 4], ".mp3") == 0)
	{
		if (!DecodeMp3ToWav(szFN))
			return false;
	}

	return true;
}

bool CN3SndMgr::DecodeMp3ToWav(std::string& filename)
{
	// We've seen this MP3 before. Replace its filename.
	auto itr = m_mp3ToWavFileMap.find(filename);
	if (itr != m_mp3ToWavFileMap.end())
	{
		filename = itr->second;
		return true;
	}

	PWSTR path = {};
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path);
	if (FAILED(hr))
	{
#ifdef _N3GAME
		CLogWriter::Write("Failed to fetch LocalAppData directory: {:X}", hr);
#endif
		return false;
	}

	std::error_code fsErrorCode = {};

	std::wstring audioDir = path;
	CoTaskMemFree(path);
	audioDir += L"\\OpenKO\\Snd";

	std::filesystem::path newPath(audioDir);
	std::filesystem::create_directories(newPath, fsErrorCode);

	std::filesystem::path oldPath(filename);
	newPath /= oldPath.filename();
	newPath.replace_extension(".wav");

	// If we've already converted this file, we can just use it immediately.
	if (std::filesystem::exists(newPath, fsErrorCode))
	{
		std::string newFilename = newPath.string();
		m_mp3ToWavFileMap.insert(std::make_pair(filename, newFilename));
		filename = std::move(newFilename);
		return true;
	}

	// We have yet to convert it, so we need to load it up and decode it.
	int error = MPG123_ERR;

	mpg123_handle* mpgHandle = mpg123_new(nullptr, &error);
	if (mpgHandle == nullptr)
	{
#ifdef _N3GAME
		CLogWriter::Write("Failed to allocate MP3 handle: {} ({})",
			filename, error);
#endif
		return false;
	}

	// Force output as 16-bit PCM - preserve sample rate and channel count.
	mpg123_format(mpgHandle, 0, 0, MPG123_ENC_SIGNED_16);

	error = mpg123_open(mpgHandle, filename.c_str());
	if (error != MPG123_OK)
	{
#ifdef _N3GAME
		CLogWriter::Write("Failed to open MP3: {} ({})",
			filename, error);
#endif
		mpg123_delete(mpgHandle);
		return false;
	}

	long rate = 0;
	int channels = 0, encoding = 0;

	error = mpg123_getformat(mpgHandle, &rate, &channels, &encoding);
	if (error != MPG123_OK)
	{
#ifdef _N3GAME
		CLogWriter::Write("Failed to get MP3 format: {} ({}: {})",
			filename, error, mpg123_strerror(mpgHandle));
#endif

		mpg123_delete(mpgHandle);
		return false;
	}

	off_t sampleCount = mpg123_length(mpgHandle);
	if (sampleCount < 0)
	{
#ifdef _N3GAME
		CLogWriter::Write("Failed to get total MP3 samples per channel: {}",
			filename);
#endif

		mpg123_delete(mpgHandle);
		return false;
	}

	std::string newFilename = newPath.string();

	// Open the new WAV file for writing.
	FILE* fp = fopen(newFilename.c_str(), "wb");
	if (fp == nullptr)
	{
#ifdef _N3GAME
		CLogWriter::Write("Failed to open file for writing decoded MP3 to: {}",
			newFilename);
#endif
		return false;
	}

	const int sampleSizeBytes = mpg123_encsize(encoding);

	// Initialise header to defaults
	WavFileHeader wavFileHeader = {};

	// Setup the file header.
	wavFileHeader.Format.AudioFormat = 1; // PCM
	wavFileHeader.Format.NumChannels = static_cast<uint16_t>(channels);
	wavFileHeader.Format.SampleRate = static_cast<uint16_t>(rate);
	wavFileHeader.Format.BitsPerSample = static_cast<uint16_t>(sampleSizeBytes * 8);
	wavFileHeader.Format.BytesPerBlock = wavFileHeader.Format.NumChannels * wavFileHeader.Format.BitsPerSample / 8;
	wavFileHeader.Format.BytesPerSec = wavFileHeader.Format.SampleRate * wavFileHeader.Format.BytesPerBlock;

	size_t done = 0, decodedBytes = 0;
	const size_t frameSize = mpg123_outblock(mpgHandle);
	std::vector<uint8_t> frameBlock(frameSize);

	// Skip the header - we'll write that at the end.
	fseek(fp, sizeof(WavFileHeader), SEEK_SET);

	error = mpg123_read(mpgHandle, &frameBlock[0], frameSize, &done);
	while (error == MPG123_OK)
	{
		decodedBytes += done;

		fwrite(&frameBlock[0], done, 1, fp);
		error = mpg123_read(mpgHandle, &frameBlock[0], frameSize, &done);
	}

	mpg123_delete(mpgHandle);

	if (error != MPG123_DONE)
	{
		fclose(fp);
		std::remove(newFilename.c_str());

#ifdef _N3GAME
		CLogWriter::Write("Failed to decode MP3: {} ({} - decoded {} bytes)",
			filename, error, decodedBytes);
#endif
		return false;
	}

	wavFileHeader.FileSize += static_cast<uint32_t>(decodedBytes);
	wavFileHeader.Data.Size = static_cast<uint32_t>(decodedBytes);

	// Write out the header.
	long endOfFileOffset = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	fwrite(&wavFileHeader, sizeof(WavFileHeader), 1, fp);

	// Seek back to the known end, before the file is closed and flushed.
	fseek(fp, endOfFileOffset, SEEK_SET);
	fclose(fp);

	m_mp3ToWavFileMap.insert(std::make_pair(filename, newFilename));
	filename = std::move(newFilename);

	return true;
}
