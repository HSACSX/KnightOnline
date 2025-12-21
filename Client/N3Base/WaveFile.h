//////////////////////////////////////////////////
//	Commented By : Lynus
//	Commented On 2001-04-12 오후 1:51:00
//
//	CWave Class (WaveFile.h)
//
//	End Of Comment (By Lynus On 2001-04-12 오후 1:51:00 )
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//	Coded By : Lynus
//	Coded On 2001-04-12 오후 1:50:14
//

#ifndef __WAVE_H_
#define __WAVE_H_

#include <Windows.h>
#include <stdint.h>
#include <inttypes.h>

#include <mmsystem.h>
#include <mmreg.h>
#include <dxerr.h>

#pragma pack(push, 1)
struct WavFileHeader
{
	struct WavFormat
	{
		uint32_t	ID				= mmioFOURCC('f', 'm', 't', ' ');

		// Chunk size minus 8 bytes
		uint32_t	Size			= sizeof(WavFormat) - 8;

		// Audio format (1: PCM integer, 3: IEEE 754 float)
		uint16_t	AudioFormat		= 1;

		// Number of channels
		uint16_t	NumChannels		= 1;

		// Sample rate (in hertz)
		uint32_t	SampleRate		= 44100;

		// SampleRate * BytesPerBlock
		uint32_t	BytesPerSec		= 0;

		// Number of bytes per block (NumChannels * (BitsPerSample / 8))
		uint16_t	BytesPerBlock	= 0;

		// Number of bits per sample
		uint16_t	BitsPerSample	= 0;
	};

	struct WavData
	{
		uint32_t	ID				= mmioFOURCC('d', 'a', 't', 'a');

		// NumSamples * NumChannels * (BitsPerSample / 8)
		uint32_t	Size			= 0;
	};

	// RIFF file identifier
	uint32_t		FileTypeID		= mmioFOURCC('R', 'I', 'F', 'F');

	// Overall file size minus 8 bytes (append the data size)
	uint32_t		FileSize		= sizeof(WavFileHeader) - 8;

	// WAV file format identifier
	uint32_t		FileFormatID	= mmioFOURCC('W', 'A', 'V', 'E');

	WavFormat		Format			= {};
	WavData			Data			= {};
};
#pragma pack(pop)

class CWaveFile
{
public:
	WAVEFORMATEX*	m_pwfx;		// Pointer to WAVEFORMATEX structure
	HMMIO			m_hmmio;	// MM I/O handle for the WAVE
	MMCKINFO		m_ck;		// Multimedia RIFF chunk
	MMCKINFO		m_ckRiff;	// Use in opening a WAVE file
	uint32_t		m_dwSize;	// The size of the wave file
	bool			m_bIsReadingFromMemory;
	uint8_t*		m_pbData;
	uint8_t*		m_pbDataCur;
	uint32_t		m_ulDataSize;

protected:
    HRESULT ReadMMIO();

public:
	CWaveFile();
	~CWaveFile();

	HRESULT Open(const char* strFileName);
	HRESULT OpenFromMemory(uint8_t* pbData, uint32_t ulDataSize);
	void Close();

	HRESULT Read(uint8_t* pBuffer, uint32_t dwSizeToRead, uint32_t* pdwSizeRead);

	uint32_t GetSize();
	HRESULT ResetFile();

	WAVEFORMATEX* GetFormat()
	{
		return m_pwfx;
	}
};

#endif // #ifndef __WAVE_H_
//	End Of Code (By Lynus On 2001-04-12 오후 1:50:14 )
//////////////////////////////////////////////////
