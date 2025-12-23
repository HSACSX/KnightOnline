////////////////////////////////////////////////////////////////////////////////////////
//
//	N3SndDef.h
//	- 이것저것 Sound에 관련된 자료형정의, 상수정의...
//
//	By Donghoon..
//
////////////////////////////////////////////////////////////////////////////////////////

#ifndef __N3SNDDEF_H__
#define __N3SNDDEF_H__

#pragma once

#include <string>

struct __TABLE_SOUND // Sound 리소스 레코드...
{
	uint32_t	dwID;		// 고유 ID
	std::string	szFN;		// wave file name
	int			iType;		// 사운드 타입...
	int			iNumInst;	// 최대 사용할 수 있는 인스턴스의 갯수..
};

// 사운드 오브젝트 타입 정의..
enum e_SndType
{
	SNDTYPE_2D = 0,
	SNDTYPE_3D,
	SNDTYPE_STREAM,
	SNDTYPE_UNKNOWN
};

enum e_SndState
{
	SNDSTATE_STOP = 0,
	SNDSTATE_DELAY,
	SNDSTATE_FADEIN,
	SNDSTATE_PLAY,
	SNDSTATE_FADEOUT
};

#endif	//end of #ifndef __N3SNDDEF_H__
