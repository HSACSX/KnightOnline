#ifndef SERVER_SHAREDSERVER__USER_DATA_H
#define SERVER_SHAREDSERVER__USER_DATA_H

#pragma once

#include <shared/globals.h>

struct _ITEM_DATA
{
	int32_t nNum            = 0; // item 번호
	int16_t sDuration       = 0; // item 내구력
	int16_t sCount          = 0; // item 갯수 or item 축복 속성에 해당 값
	uint8_t byFlag          = 0;
	uint16_t sTimeRemaining = 0;
	int64_t nSerialNum      = 0; // item serial code
};

struct _WAREHOUSE_ITEM_DATA
{
	int32_t nNum       = 0; // item 번호
	int16_t sDuration  = 0; // item 내구력
	int16_t sCount     = 0; // item 갯수 or item 축복 속성에 해당 값
	int64_t nSerialNum = 0; // item serial code
};

struct _USER_QUEST
{
	int16_t sQuestID     = 0;
	uint8_t byQuestState = QUEST_STATE_NOT_STARTED;
};

struct _USER_DATA
{
	char m_id[MAX_ID_SIZE + 1]                            = {};   // 유저 ID
	char m_Accountid[MAX_ID_SIZE + 1]                     = {};   // 계정 ID

	uint8_t m_bZone                                       = 0;    // 현재 Zone
	float m_curx                                          = 0.0f; // 현재 X 좌표
	float m_curz                                          = 0.0f; // 현재 Z 좌표
	float m_cury                                          = 0.0f; // 현재 Y 좌표

	uint8_t m_bNation                                     = 0;    // 소속국가
	uint8_t m_bRace                                       = 0;    // 종족
	int16_t m_sClass                                      = 0;    // 직업
	uint8_t m_bHairColor                                  = 0;    // 머리색깔
	uint8_t m_bRank                                       = 0;    // 작위
	uint8_t m_bTitle                                      = 0;    // 지위
	uint8_t m_bLevel                                      = 0;    // 레벨
	int32_t m_iExp                                        = 0;    // 경험치
	int32_t m_iLoyalty                                    = 0;    // 로열티
	int32_t m_iLoyaltyMonthly                             = 0;    // 로열티
	uint8_t m_bFace                                       = 0;    // 얼굴모양
	uint8_t m_bCity                                       = 0;    // 소속도시
	int16_t m_bKnights                                    = 0;    // 소속 기사단
	uint8_t m_bFame                                       = 0;    // 명성
	int16_t m_sHp                                         = 0;    // HP
	int16_t m_sMp                                         = 0;    // MP
	int16_t m_sSp                                         = 0;    // SP
	uint8_t m_bStr                                        = 0;    // 힘
	uint8_t m_bSta                                        = 0;    // 생명력
	uint8_t m_bDex                                        = 0;    // 공격, 회피율
	uint8_t m_bIntel                                      = 0;    // 지혜(?), 캐릭터 마법력 결정
	uint8_t m_bCha                                        = 0;    // 마법 성공률, 물건 가격 결정(?)
	uint8_t m_bAuthority                                  = 0;    // 유저 권한
	uint8_t m_bPoints                                     = 0;    // 보너스 포인트
	int32_t m_iGold                                       = 0;    // 캐릭이 지닌 돈(21억)
	int16_t m_sBind                                       = 0;    // Saved Bind Point
	int32_t m_iBank                                       = 0;    // 창고의 돈(21억)

	uint8_t m_bstrSkill[9]                                = {};   // 직업별 스킬
	_ITEM_DATA m_sItemArray[HAVE_MAX + SLOT_MAX]          = {};   // 42*8 bytes
	_WAREHOUSE_ITEM_DATA m_sWarehouseArray[WAREHOUSE_MAX] = {};   // 창고 아이템	192*8 bytes

	uint8_t m_bLogout                                     = 0;    // 로그아웃 플래그
	uint8_t m_bWarehouse                                  = 0;    // 창고 거래 했었나?
	uint32_t m_dwTime                                     = 0;    // 플레이 타임...

	uint8_t m_byPremiumType                               = 0;
	int16_t m_sPremiumTime                                = 0;
	int32_t m_iMannerPoint                                = 0;

	int16_t m_sQuestCount                                 = 0;
	_USER_QUEST m_quests[MAX_QUEST]                       = {};
};

constexpr int ALLOCATED_USER_DATA_BLOCK = 8000;
static_assert(sizeof(_USER_DATA) <= ALLOCATED_USER_DATA_BLOCK);

#endif // SERVER_SHAREDSERVER__USER_DATA_H
