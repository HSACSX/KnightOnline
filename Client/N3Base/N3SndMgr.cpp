// N3SndMgr.cpp: implementation of the CN3SndMgr class.
//
//////////////////////////////////////////////////////////////////////
#include "StdAfxBase.h"
#include "N3SndMgr.h"
#include "N3SndObj.h"
#include "N3Base.h"
#include "AudioThread.h"
#include "AudioDecoderThread.h"
#include "WaveFormat.h"
#include "AudioAsset.h"
#include "al_wrapper.h"

#include <shared/StringUtils.h>

#include <AL/alc.h>
#include <mpg123.h>

#include <cassert>
#include <filesystem>
#include <shlobj.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#endif

CN3SndMgr::CN3SndMgr()
{
	m_bSndEnable = false;

	_alcDevice = nullptr;
	_alcContext = nullptr;
}

CN3SndMgr::~CN3SndMgr()
{
	Release();
}

// Initialize the sound engine.
void CN3SndMgr::Init()
{
	Release();

	m_bSndEnable = InitOpenAL();
	m_Tbl_Source.LoadFromFile("Data\\sound.tbl");
}

bool CN3SndMgr::InitOpenAL()
{
	if (_alcDevice != nullptr)
		return false;

	const char* deviceName = nullptr;
	if (alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT"))
	{
		if (alcIsExtensionPresent(0, "ALC_ENUMERATE_ALL_EXT"))
			deviceName = alcGetString(0, ALC_DEFAULT_ALL_DEVICES_SPECIFIER);
		else
			deviceName = alcGetString(0, ALC_DEFAULT_DEVICE_SPECIFIER);
	}

	_alcDevice = alcOpenDevice(deviceName);
	if (_alcDevice == nullptr)
	{
		MessageBoxW(nullptr, L"Failed to open sound device for playback", L"Error", MB_OK);
		return false;
	}

	_alcContext = alcCreateContext(_alcDevice, nullptr);
	alcMakeContextCurrent(_alcContext);

	ALuint generatedSourceIds[MAX_SOURCE_IDS];
	int generatedSourceIdCount = 0;
	while (generatedSourceIdCount < MAX_SOURCE_IDS)
	{
		alGenSources(1, &generatedSourceIds[generatedSourceIdCount]);
		if (alGetError() != AL_NO_ERROR)
			break;

		++generatedSourceIdCount;
	}

	if (generatedSourceIdCount == 0)
	{
		alcMakeContextCurrent(nullptr);
		alcDestroyContext(_alcContext);
		alcCloseDevice(_alcDevice);

		_alcContext = nullptr;
		_alcDevice = nullptr;

		MessageBoxW(nullptr, L"Unable to generate sound sources.", L"Error", MB_OK);
		return false;
	}

	int maxRegularSourceCount = generatedSourceIdCount;
	int maxStreamCount = MAX_STREAM_SOURCES;

	// Not enough sources to allow for MAX_STREAM_SOURCES.
	if (generatedSourceIdCount < maxStreamCount)
	{
		// If we have at least 2 source IDs available, reserve just 1 for stream sources.
		if (generatedSourceIdCount >= 2)
			maxStreamCount = 1;
		// Otherwise, we'll only have 1 source ID available. It should go towards the regular sound,
		// rather than the streamed source (i.e. BGM).
		else
			maxStreamCount = 0;
	}

	maxRegularSourceCount -= maxStreamCount;

	{
		std::lock_guard<std::mutex> lock(_sourceIdMutex);

		int i = 0;
		while (i < maxRegularSourceCount)
			_unassignedSourceIds.push_back(generatedSourceIds[i++]);

		while (i < generatedSourceIdCount)
			_unassignedStreamSourceIds.push_back(generatedSourceIds[i++]);
	}

	_thread = std::make_unique<AudioThread>();
	_thread->start();

	_decoderThread = std::make_unique<AudioDecoderThread>();
	_decoderThread->start();

	return true;
}

CN3SndObj* CN3SndMgr::CreateObj(int iID, e_SndType eType)
{
	__TABLE_SOUND* pTbl = m_Tbl_Source.Find(iID);
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

	CN3SndObj* pSndObj = new CN3SndObj();
	if (!pSndObj->Create(szFN, eType))
	{
		delete pSndObj;
		return nullptr;
	}

	m_SndObjs.push_back(pSndObj);
	return pSndObj;
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

	m_SndObjStreams.push_back(pSndObj);
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

// Tick sound logic by 1 frame.
void CN3SndMgr::Tick()
{
	if (!m_bSndEnable)
		return;

	for (CN3SndObj* pSndObj : m_SndObjs)
		pSndObj->Tick();

	auto it = m_SndObjs_PlayOnceAndRelease.begin(), itEnd = m_SndObjs_PlayOnceAndRelease.end();
	while (it != itEnd)
	{
		CN3SndObj* pSndObj = *it;
		pSndObj->Tick();

		if (pSndObj->IsFinished())
		{
			delete pSndObj;
			it = m_SndObjs_PlayOnceAndRelease.erase(it);
		}
		else
		{
			++it;
		}
	}

	for (CN3SndObj* pSndObj : m_SndObjStreams)
		pSndObj->Tick();
}

// Release an individual sound object.
void CN3SndMgr::ReleaseObj(CN3SndObj** ppObj)
{
	if (ppObj == nullptr || *ppObj == nullptr)
		return;

	auto it = m_SndObjs.begin(), itEnd = m_SndObjs.end();
	for (; it != itEnd; ++it)
	{
		if (*ppObj == *it)
		{
			m_SndObjs.erase(it);

			delete *ppObj;
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

			delete *ppObj;
			*ppObj = nullptr;
			return;
		}
	}

	assert(!"Failed to find sound to free.");

	// Reset the pointer still as the caller will expect it to be nullptr.
	// Don't free it because it's not managed by us (as far as we can tell).
	*ppObj = nullptr;
}

// Release the entirety of the sound engine.
void CN3SndMgr::Release()
{
	for (CN3SndObj* pSndObj : m_SndObjs)
		delete pSndObj;
	m_SndObjs.clear();

	for (CN3SndObj* pSndObj : m_SndObjs_PlayOnceAndRelease)
		delete pSndObj;
	m_SndObjs_PlayOnceAndRelease.clear();

	for (CN3SndObj* pSndObj : m_SndObjStreams)
		delete pSndObj;
	m_SndObjStreams.clear();

	if (_decoderThread != nullptr)
	{
		_decoderThread->shutdown();
		_decoderThread.reset();
	}

	if (_thread != nullptr)
	{
		_thread->shutdown();
		_thread.reset();
	}

	ReleaseOpenAL();
}

void CN3SndMgr::ReleaseOpenAL()
{
	if (_alcDevice == nullptr)
		return;

	{
		std::lock_guard<std::mutex> lock(_sourceIdMutex);

		for (uint32_t sourceId : _unassignedSourceIds)
			alDeleteSources(1, &sourceId);

		for (uint32_t sourceId : _unassignedStreamSourceIds)
			alDeleteSources(1, &sourceId);

		assert(_assignedSourceIds.empty());
		assert(_assignedStreamSourceIds.empty());

		_unassignedSourceIds.clear();
		_unassignedStreamSourceIds.clear();
		_assignedSourceIds.clear();
		_assignedStreamSourceIds.clear();
	}

	alcMakeContextCurrent(nullptr);

	alcDestroyContext(_alcContext);
	_alcContext = nullptr;

	alcCloseDevice(_alcDevice);
	_alcDevice = nullptr;
}

// Sounds are played once and then automatically freed via ReleaseObj.
// The position can only be specified now, as the pointer to this temporary
// sound object cannot otherwise be exposed.
bool CN3SndMgr::PlayOnceAndRelease(int iSndID, const __Vector3* pPos)
{
	if (!m_bSndEnable)
		return false;

	if (!CN3Base::s_Options.bSndEffectEnable)
		return false;

	__TABLE_SOUND* pTbl = m_Tbl_Source.Find(iSndID);
	if (pTbl == nullptr || pTbl->szFN.empty())
		return false;

	CN3SndObj* pSndObj = new CN3SndObj();
	if (!pSndObj->Create(pTbl->szFN, static_cast<e_SndType>(pTbl->iType)))
	{
		delete pSndObj;
		return false;
	}

	m_SndObjs_PlayOnceAndRelease.push_back(pSndObj);

	pSndObj->Play(pPos);
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
	if (strnicmp(szFN.data() + szFN.length() - 4, ".mp3", 4) == 0)
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
	RIFF_Header_Out wavFileHeader = {};

	// Setup the file header.
	wavFileHeader.Format.AudioFormat	= 1; // PCM
	wavFileHeader.Format.NumChannels	= static_cast<uint16_t>(channels);
	wavFileHeader.Format.SampleRate		= static_cast<uint16_t>(rate);
	wavFileHeader.Format.BitsPerSample	= static_cast<uint16_t>(sampleSizeBytes * 8);
	wavFileHeader.Format.BytesPerBlock	= wavFileHeader.Format.NumChannels * wavFileHeader.Format.BitsPerSample / 8;
	wavFileHeader.Format.BytesPerSec	= wavFileHeader.Format.SampleRate * wavFileHeader.Format.BytesPerBlock;

	size_t done = 0, decodedBytes = 0;
	const size_t frameSize = mpg123_outblock(mpgHandle);
	std::vector<uint8_t> frameBlock(frameSize);

	// Skip the header - we'll write that at the end.
	fseek(fp, sizeof(RIFF_Header), SEEK_SET);

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

		std::error_code ec;
		std::filesystem::remove(newPath, ec);

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
	fwrite(&wavFileHeader, sizeof(RIFF_Header), 1, fp);

	// Seek back to the known end, before the file is closed and flushed.
	fseek(fp, endOfFileOffset, SEEK_SET);
	fclose(fp);

	m_mp3ToWavFileMap.insert(std::make_pair(filename, newFilename));
	filename = std::move(newFilename);

	return true;
}

bool CN3SndMgr::PullBufferedSourceIdFromPool(uint32_t* sourceId)
{
	if (sourceId == nullptr)
		return false;

	std::lock_guard<std::mutex> lock(_sourceIdMutex);

	// Too many already active.
	if (_unassignedSourceIds.empty())
		return INVALID_SOURCE_ID;

	*sourceId = _unassignedSourceIds.front();
	_unassignedSourceIds.pop_front();
	_assignedSourceIds.insert(*sourceId);
	return true;
}

void CN3SndMgr::RestoreBufferedSourceIdToPool(uint32_t* sourceId)
{
	if (sourceId == nullptr
		|| *sourceId == INVALID_SOURCE_ID)
		return;

	std::lock_guard<std::mutex> lock(_sourceIdMutex);

	auto itr = _assignedSourceIds.find(*sourceId);
	if (itr == _assignedSourceIds.end())
		return;

	_assignedSourceIds.erase(itr);
	_unassignedSourceIds.push_back(*sourceId);

	*sourceId = INVALID_SOURCE_ID;
}

bool CN3SndMgr::PullStreamedSourceIdFromPool(uint32_t* sourceId)
{
	if (sourceId == nullptr)
		return false;

	std::lock_guard<std::mutex> lock(_sourceIdMutex);

	// Too many already active.
	if (_unassignedStreamSourceIds.empty())
		return INVALID_SOURCE_ID;

	*sourceId = _unassignedStreamSourceIds.front();
	_unassignedStreamSourceIds.pop_front();
	_assignedStreamSourceIds.insert(*sourceId);
	return true;
}

void CN3SndMgr::RestoreStreamedSourceIdToPool(uint32_t* sourceId)
{
	if (sourceId == nullptr
		|| *sourceId == INVALID_SOURCE_ID)
		return;

	std::lock_guard<std::mutex> lock(_sourceIdMutex);

	auto itr = _assignedStreamSourceIds.find(*sourceId);
	if (itr == _assignedStreamSourceIds.end())
		return;

	_assignedStreamSourceIds.erase(itr);
	_unassignedStreamSourceIds.push_back(*sourceId);

	*sourceId = INVALID_SOURCE_ID;
}

std::shared_ptr<BufferedAudioAsset> CN3SndMgr::LoadBufferedAudioAsset(const std::string& filename)
{
	std::shared_ptr<BufferedAudioAsset> audioAsset;
	std::lock_guard<std::mutex> lock(_bufferedAudioAssetByFilenameMutex);

	auto itr = _bufferedAudioAssetByFilenameMap.find(filename);
	if (itr == _bufferedAudioAssetByFilenameMap.end())
	{
		audioAsset = std::make_shared<BufferedAudioAsset>();
		if (audioAsset == nullptr)
			return nullptr;

		if (!audioAsset->LoadFromFile(filename))
			return nullptr;

		_bufferedAudioAssetByFilenameMap.insert(std::make_pair(filename, audioAsset));
	}
	else
	{
		audioAsset = itr->second;
	}

	++audioAsset->RefCount;
	return audioAsset;
}

void CN3SndMgr::RemoveBufferedAudioAsset(BufferedAudioAsset* audioAsset)
{
	std::lock_guard lock(_bufferedAudioAssetByFilenameMutex);
	if (--audioAsset->RefCount == 0)
		_bufferedAudioAssetByFilenameMap.erase(audioAsset->Filename);
}

std::shared_ptr<StreamedAudioAsset> CN3SndMgr::LoadStreamedAudioAsset(const std::string& filename)
{
	std::shared_ptr<StreamedAudioAsset> audioAsset;
	std::lock_guard<std::mutex> lock(_streamedAudioAssetByFilenameMutex);

	auto itr = _streamedAudioAssetByFilenameMap.find(filename);
	if (itr == _streamedAudioAssetByFilenameMap.end())
	{
		audioAsset = std::make_shared<StreamedAudioAsset>();
		if (audioAsset == nullptr)
			return nullptr;

		if (!audioAsset->LoadFromFile(filename))
			return nullptr;

		_streamedAudioAssetByFilenameMap.insert(std::make_pair(filename, audioAsset));
	}
	else
	{
		audioAsset = itr->second;
	}

	++audioAsset->RefCount;
	return audioAsset;
}

void CN3SndMgr::RemoveStreamedAudioAsset(StreamedAudioAsset* audioAsset)
{
	std::lock_guard lock(_streamedAudioAssetByFilenameMutex);
	if (--audioAsset->RefCount == 0)
		_streamedAudioAssetByFilenameMap.erase(audioAsset->Filename);
}

void CN3SndMgr::Add(std::shared_ptr<AudioHandle> handle)
{
	if (_thread != nullptr)
		_thread->Add(std::move(handle));
}

void CN3SndMgr::QueueCallback(std::shared_ptr<AudioHandle> handle, AudioThread::AudioCallback callback)
{
	if (_thread != nullptr)
		_thread->QueueCallback(std::move(handle), std::move(callback));
}

void CN3SndMgr::Remove(std::shared_ptr<AudioHandle> handle)
{
	if (_thread != nullptr)
		_thread->Remove(std::move(handle));
}
