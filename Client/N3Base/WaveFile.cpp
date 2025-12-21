//////////////////////////////////////////////////
//	Commented By : Lynus
//	Commented On 2001-04-12 오후 1:52:29
//
//	CWave class(wave.cpp)
//
//	End Of Comment (By Lynus On 2001-04-12 오후 1:52:29 )
//////////////////////////////////////////////////

//////////////////////////////////////////////////
//	Coded By : Lynus
//	Coded On 2001-04-12 오후 1:52:49
//

#include "StdAfxBase.h"
#include "WaveFile.h"

//-----------------------------------------------------------------------------
// Name: CWaveFile::CWaveFile()
// Desc: Constructs the class.  Call Open() to open a wave file for reading.  
//       Then call Read() as needed.  Calling the destructor or Close() 
//       will close the file.  
//-----------------------------------------------------------------------------
CWaveFile::CWaveFile()
{
	m_pwfx = nullptr;
	m_hmmio = nullptr;
	m_dwSize = 0;
	m_bIsReadingFromMemory = false;
}

//-----------------------------------------------------------------------------
// Name: CWaveFile::~CWaveFile()
// Desc: Destructs the class
//-----------------------------------------------------------------------------
CWaveFile::~CWaveFile()
{
	Close();

	delete[] m_pwfx;
	m_pwfx = nullptr;
}


//-----------------------------------------------------------------------------
// Name: CWaveFile::Open()
// Desc: Opens a wave file for reading
//-----------------------------------------------------------------------------
HRESULT CWaveFile::Open(const char* strFileName)
{
	if (strFileName == nullptr)
		return E_INVALIDARG;

	m_bIsReadingFromMemory = false;

	delete[] m_pwfx;
	m_pwfx = nullptr;

	m_hmmio = mmioOpen((LPSTR) strFileName, nullptr, MMIO_ALLOCBUF | MMIO_READ);
	if (m_hmmio == nullptr)
		return E_FAIL;

	HRESULT hr = ReadMMIO();
	if (FAILED(hr))
	{
		// ReadMMIO will fail if its an not a wave file
		mmioClose(m_hmmio, 0);
		return hr;
	}

	hr = ResetFile();
	if (FAILED(hr))
		return hr;

	// After the reset, the size of the wav file is m_ck.cksize so store it now
	m_dwSize = m_ck.cksize;

	return hr;
}

//-----------------------------------------------------------------------------
// Name: CWaveFile::OpenFromMemory()
// Desc: copy data to CWaveFile member variable from memory
//-----------------------------------------------------------------------------
HRESULT CWaveFile::OpenFromMemory(uint8_t* pbData, uint32_t ulDataSize)
{
	m_ulDataSize	= ulDataSize;
	m_pbData		= pbData;
	m_pbDataCur		= pbData;
	m_bIsReadingFromMemory = true;
	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: CWaveFile::ReadMMIO()
// Desc: Support function for reading from a multimedia I/O stream.
//       m_hmmio must be valid before calling.  This function uses it to
//       update m_ckRiff, and m_pwfx. 
//-----------------------------------------------------------------------------
HRESULT CWaveFile::ReadMMIO()
{
	MMCKINFO        ckIn;           // chunk info. for general use.
	PCMWAVEFORMAT   pcmWaveFormat;  // Temp PCM structure to load in.       

	m_pwfx = nullptr;

	if (0 != mmioDescend(m_hmmio, &m_ckRiff, nullptr, 0))
		return E_FAIL;

	// Check to make sure this is a valid wave file
	if ((m_ckRiff.ckid != FOURCC_RIFF)
		|| (m_ckRiff.fccType != mmioFOURCC('W', 'A', 'V', 'E')))
		return E_FAIL;

	// Search the input file for for the 'fmt ' chunk.
	ckIn.ckid = mmioFOURCC('f', 'm', 't', ' ');
	if (0 != mmioDescend(m_hmmio, &ckIn, &m_ckRiff, MMIO_FINDCHUNK))
		return E_FAIL;

	// Expect the 'fmt' chunk to be at least as large as <PCMWAVEFORMAT>;
	// if there are extra parameters at the end, we'll ignore them
	if (ckIn.cksize < (LONG) sizeof(PCMWAVEFORMAT))
		return E_FAIL;

	// Read the 'fmt ' chunk into <pcmWaveFormat>.
	if (mmioRead(m_hmmio, (HPSTR) &pcmWaveFormat,
		sizeof(pcmWaveFormat)) != sizeof(pcmWaveFormat))
		return E_FAIL;

	// Allocate the waveformatex, but if its not pcm format, read the next
	// uint16_t, and thats how many extra bytes to allocate.
	if (pcmWaveFormat.wf.wFormatTag == WAVE_FORMAT_PCM)
	{
		m_pwfx = (WAVEFORMATEX*)new CHAR[sizeof(WAVEFORMATEX)];
		if (nullptr == m_pwfx)
			return E_FAIL;

		// Copy the bytes from the pcm structure to the waveformatex structure
		memcpy(m_pwfx, &pcmWaveFormat, sizeof(pcmWaveFormat));
		m_pwfx->cbSize = 0;
	}
	else
	{
		// Read in length of extra bytes.
		uint16_t cbExtraBytes = 0L;
		if (mmioRead(m_hmmio, (CHAR*) &cbExtraBytes, sizeof(uint16_t)) != sizeof(uint16_t))
			return E_FAIL;

		m_pwfx = (WAVEFORMATEX*)new CHAR[sizeof(WAVEFORMATEX) + cbExtraBytes];
		if (nullptr == m_pwfx)
			return E_FAIL;

		// Copy the bytes from the pcm structure to the waveformatex structure
		memcpy(m_pwfx, &pcmWaveFormat, sizeof(pcmWaveFormat));
		m_pwfx->cbSize = cbExtraBytes;

		// Now, read those extra bytes into the structure, if cbExtraAlloc != 0.
		if (mmioRead(m_hmmio, (CHAR*) (((uint8_t*) &(m_pwfx->cbSize)) + sizeof(uint16_t)),
			cbExtraBytes) != cbExtraBytes)
		{
			delete[] m_pwfx;
			m_pwfx = nullptr;
			return E_FAIL;
		}
	}

	// Ascend the input file out of the 'fmt ' chunk.
	if (0 != mmioAscend(m_hmmio, &ckIn, 0))
	{
		delete[] m_pwfx;
		m_pwfx = nullptr;
		return E_FAIL;
	}

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: CWaveFile::GetSize()
// Desc: Retuns the size of the read access wave file 
//-----------------------------------------------------------------------------
uint32_t CWaveFile::GetSize()
{
	return m_dwSize;
}

//-----------------------------------------------------------------------------
// Name: CWaveFile::ResetFile()
// Desc: Resets the internal m_ck pointer so reading starts from the 
//       beginning of the file again 
//-----------------------------------------------------------------------------
HRESULT CWaveFile::ResetFile()
{
	if (m_bIsReadingFromMemory)
	{
		m_pbDataCur = m_pbData;
	}
	else
	{
		if (m_hmmio == nullptr)
			return CO_E_NOTINITIALIZED;

		// Seek to the data
		if (-1 == mmioSeek(m_hmmio, m_ckRiff.dwDataOffset + sizeof(FOURCC),
			SEEK_SET))
			return S_FALSE;//DXTRACE_ERR( TEXT("mmioSeek"), E_FAIL );

		// Search the input file for the 'data' chunk.
		m_ck.ckid = mmioFOURCC('d', 'a', 't', 'a');
		if (0 != mmioDescend(m_hmmio, &m_ck, &m_ckRiff, MMIO_FINDCHUNK))
			return S_FALSE;//DXTRACE_ERR( TEXT("mmioDescend"), E_FAIL );
	}

	return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CWaveFile::Read()
// Desc: Reads section of data from a wave file into pBuffer and returns 
//       how much read in pdwSizeRead, reading not more than dwSizeToRead.
//       This uses m_ck to determine where to start reading from.  So 
//       subsequent calls will be continue where the last left off unless 
//       Reset() is called.
//-----------------------------------------------------------------------------
HRESULT CWaveFile::Read(uint8_t* pBuffer, uint32_t dwSizeToRead, uint32_t* pdwSizeRead)
{
	if (m_bIsReadingFromMemory)
	{
		if (m_pbDataCur == nullptr)
			return CO_E_NOTINITIALIZED;

		if (pdwSizeRead != nullptr)
			*pdwSizeRead = 0;

		if ((uint8_t*) (m_pbDataCur + dwSizeToRead) > (uint8_t*) (m_pbData + m_ulDataSize))
			dwSizeToRead = m_ulDataSize - (uint32_t) (m_pbDataCur - m_pbData);

		memcpy(pBuffer, m_pbDataCur, dwSizeToRead);

		if (pdwSizeRead != nullptr)
			*pdwSizeRead = dwSizeToRead;

		return S_OK;
	}
	else
	{
		MMIOINFO mmioinfoIn; // current status of m_hmmio

		if (m_hmmio == nullptr)
			return CO_E_NOTINITIALIZED;

		if (pBuffer == nullptr || pdwSizeRead == nullptr)
			return E_INVALIDARG;

		if (pdwSizeRead != nullptr)
			*pdwSizeRead = 0;

		if (0 != mmioGetInfo(m_hmmio, &mmioinfoIn, 0))
			return S_FALSE;//DXTRACE_ERR( TEXT("mmioGetInfo"), E_FAIL );

		uint32_t cbDataIn = dwSizeToRead;
		if (cbDataIn > m_ck.cksize)
			cbDataIn = m_ck.cksize;

		m_ck.cksize -= cbDataIn;

		for (uint32_t cT = 0; cT < cbDataIn; cT++)
		{
			// Copy the bytes from the io to the buffer.
			if (mmioinfoIn.pchNext == mmioinfoIn.pchEndRead)
			{
				if (0 != mmioAdvance(m_hmmio, &mmioinfoIn, MMIO_READ))
					return S_FALSE;//DXTRACE_ERR( TEXT("mmioAdvance"), E_FAIL );

				if (mmioinfoIn.pchNext == mmioinfoIn.pchEndRead)
					return S_FALSE;//DXTRACE_ERR( TEXT("mmioinfoIn.pchNext"), E_FAIL );
			}

			// Actual copy.
			*((uint8_t*) pBuffer + cT) = *((uint8_t*) mmioinfoIn.pchNext);
			mmioinfoIn.pchNext++;
		}

		if (0 != mmioSetInfo(m_hmmio, &mmioinfoIn, 0))
			return S_FALSE;//DXTRACE_ERR( TEXT("mmioSetInfo"), E_FAIL );

		if (pdwSizeRead != nullptr)
			*pdwSizeRead = cbDataIn;

		return S_OK;
	}
}

//-----------------------------------------------------------------------------
// Name: CWaveFile::Close()
// Desc: Closes the wave file 
//-----------------------------------------------------------------------------
void CWaveFile::Close()
{
	mmioClose(m_hmmio, 0);
	m_hmmio = nullptr;
}
