// ������
// ��������Դϴ�.

#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

// Rebuild: single portable path (the old _WIN32 branch pulled in client-only
// ComInclude.h/ComStructure.h that don't exist server-side).
#include "Define.h"
#include "Struct.h"

#pragma pack(push,1)


//////////////////////////////////////////////////////////////////////////
// COMMAND
//////////////////////////////////////////////////////////////////////////
#define CM_CERTIFY_LOGIN		1000 
#define CM_INSTANT_LOGIN		1001 
#define CM_CERTIFY_EXIT			1002
#define CM_MEMBER_INFO			1100
#define CM_CHARACTER_INFO		1200
#define CM_CHARACTER_END		1201
#define CM_CREATE_CHARACTER		1202
#define CM_DELETE_CHARACTER		1203
#define CM_CHOICE_CHARACTER		1204



#define CM_SERVER_LIST			1300
#define CM_EXECUTE_TUTORIAL		1400
#define CM_EXECUTE_QUEST		2704	// 0xA90 - Aug-2010 server value (was stale 1401)

#define CM_GAME_LOGIN			2000
#define CM_GAME_EXIT			2001
#define CM_UDP_CONFIRM			2002
#define CM_NOTICE_LIST			2003 // �������� ��� (CYG)

#define CM_PLAYER_INFO			2100
#define CM_ITEM_INFO			2101
#define CM_SKILL_INFO			2102
#define CM_TRAINING_INFO		2103
#define CM_CEREMONY_INFO		2104
#define CM_QUEST_INFO			2105
#define CM_SALE_LIST			2110

#define CM_ROOM_INFO			2200
#define CM_ROOM_LIST			2201
#define CM_LOBBY_LIST			2202
#define CM_CREATE_ROOM			2203
#define CM_SET_ROOM				2204
#define CM_CHOICE_ROOM			2205
#define CM_QUICK_ROOM			2206
#define CM_LEAVE_ROOM			2300
#define CM_CHANGE_PARENT		2301
#define CM_CHANGE_JANG			2302
#define CM_ATHLETE_INFO			2303
#define CM_ATHLETE_END			2304
#define CM_ROBOT_INFO			2305
#define CM_ROBOT_END			2306
#define CM_CHANGE_GROUND		2307
#define CM_CHANGE_BALL			2308
#define CM_FORCE_OUT			2321	// 0x0911 (was stale 2309; real wire value per CSCForceOut)
#define CM_INVITE_PLAYER		2310

#define CM_GAME_READY			2400
#define CM_GAME_START			2401
#define CM_GAME_COUNT			2402
#define CM_GAME_LOAD			2403
#define CM_GAME_PLAY			2404
#define CM_GAME_RESULT			2405
#define	CM_GAME_END				2406
#define CM_LEVEL_UP				2407

#define CM_CHANGE_TEAM			2501
#define CM_CHANGE_POSITION		2502

#define CM_CHANGE_MENT			2601
#define CM_GROWUP_CHARACTER		2701
#define CM_QUEST_REWARD		2702 // ����Ʈ�� ���� �������Ѵ� (CYG)



#define CM_SHOPITEM_LIST		3100
#define CM_UPDATE_ITEM			3101
#define CM_EQUIP_ITEM			3102
#define CM_DIVEST_ITEM			3103
#define CM_BUY_ITEM				3104
#define CM_GIFT_ITEM			3105
#define CM_EXCHANGE_ITEM		3106

#define CM_SHOPSKILL_LIST		3200
#define CM_UPDATE_SKILL			3201
#define CM_EQUIP_SKILL			3202
#define CM_DIVEST_SKILL			3203
#define CM_BUY_SKILL			3204

#define CM_SHOPTRAINING_LIST	3300
#define CM_UPDATE_TRAINING		3301
#define CM_BUY_TRAINING			3304

#define CM_SHOPCEREMONY_LIST	3400
#define CM_UPDATE_CEREMONY		3401
#define CM_EQUIP_CEREMONY		3402
#define CM_DIVEST_CEREMONY		3403
#define CM_BUY_CEREMONY			3404

#define CM_QUEST_LIST			3500
#define CM_UPDATE_QUEST			3501
#define CM_CREATE_QUEST			3502

#define CM_TCP_PING				5000
#define CM_SEND_MESSAGE			5001
#define CM_RAISE_FACULTY		5002
#define CM_CHANGE_SETTING		5003

#define CM_UDP_PUNCHING			9000
#define CM_UDP_PING				9001


#define CM_CHARACTER_SEARCH		1205 // ĳ���� ã�� (CYG)
#define CM_POST_ITEM			3107 // ������ ���� (�����ý���) (CYG)
#define CM_MISSION_REWARD		2703 // �̼ǿ� ���� ������ �Ѵ�. (CYG)
//--SOURCE_AUTO_INSERT_SCRIPT		


//////////////////////////////////////////////////////////////////////////
// Server To Server ��������
//////////////////////////////////////////////////////////////////////////
#define CM_STS_LOGIN			101


// NOTE (rebuild): the server's real CHeadPacket is the 3-int version defined in
// Struct.h ({int m_nCommand; int m_nBodySize; int m_nSequence;} - sequence drives
// CSecure). The old 2-int definition that lived here (from the client-era
// Protocol.Backup) is removed so every CCS*/CSC* packet below inherits the
// correct 12-byte header. Struct.h is already included above.
//////////////////////////////////////////////////////////////////////////
// CERTIFY PROTOCOL
//////////////////////////////////////////////////////////////////////////
// Matches the official client (oldSources/CLIENT_SHARE/PacketForm.h): id[31] +
// pass[65], NO version field -> body = 96 bytes. (The Aug-2010 binary added a
// trailing m_sVersion[20]@body+96, but the official 2008 client never sends it.)
class CCSCertifyLogin : public CHeadPacket
{ public:
	char			m_sID[ID_NAME_SIZE];		// [31] @ body+0
	char			m_sPass[LOGIN_PASS_SIZE];	// [65] @ body+31
};

class CSCCertifyLogin : public CHeadPacket
{ public:
	char			m_nResponse;
	int				m_nMemberSeq;
	CSCCertifyLogin() : CHeadPacket(CM_CERTIFY_LOGIN) { m_nBodySize = 5; }
};
//////////////////////////////////////////////////////////////////////////
class CCSInstantLogin : public CHeadPacket
{ public:
	int				m_nMemberSeq;
};

class CSCInstantLogin : public CHeadPacket
{ public:
	char			m_nResponse;
	CSCInstantLogin() : CHeadPacket(CM_INSTANT_LOGIN) { m_nBodySize = 1; }
};
//////////////////////////////////////////////////////////////////////////
class CCSCertifyExit : public CHeadPacket
{ public:
	char			m_nReason;
};

class CSCCertifyExit : public CHeadPacket
{ public:
	char			m_nResponse;
	char			m_nReason;
	CSCCertifyExit() : CHeadPacket(CM_CERTIFY_EXIT) { m_nBodySize = 2; }
};
//////////////////////////////////////////////////////////////////////////
class CCSMemberInfo : public CHeadPacket
{ public:
	int				m_nMemberSeq;
};

class CSCMemberInfo : public CHeadPacket
{ public:
	char			m_nResponse;
	int				m_nLastSeq;
	char			m_nCount;
	char			m_nTutorial;
	char			m_nQuest;
	CSetting		m_cSetting;
	CMoney			m_cMoney;
	int				m_nLoginDate;
	int				m_nDeleteDate;
	CSCMemberInfo() : CHeadPacket(CM_MEMBER_INFO) { m_nBodySize = 0x78; }
};

//////////////////////////////////////////////////////////////////////////
class CCharacterInfo
{ public:
	int				m_nPlayerSeq;
	short			m_nOrder;
	char			m_nPosition;
	char			m_nCondition;
	char			m_nAlias;
	char			m_nReserve0A;	// official client (XKick.exe sub_4BBD60): extra byte @ body+10, before name
	char			m_sName[PLAYER_NAME_SIZE];
	CLevel			m_cLevel;
	CShape			m_cShape;
	int				m_nEquipWear[MAX_EQUIP];
	int				m_nHomeWear[4];
	int				m_nAwayWear[4];
	char			m_nReserve8D;	// official client: trailing byte @ body+141
};

class CCSCharacterInfo : public CHeadPacket
{ public:
	int				m_nMemberSeq;
};

class CSCCharacterInfo : public CHeadPacket
{ public:
	char			m_nResponse;
	CCharacterInfo	m_cCharacterInfo;
	CSCCharacterInfo() : CHeadPacket(CM_CHARACTER_INFO) { m_nBodySize = 0x8e; }  // 142: official client body (was 0x8c/140)
};
//////////////////////////////////////////////////////////////////////////
class CCSCharacterEnd : public CHeadPacket
{ public:
};

class CSCCharacterEnd : public CHeadPacket
{ public:
	char				m_nResponse;
	CSCCharacterEnd() : CHeadPacket(CM_CHARACTER_END) { m_nBodySize = 1; }
};
//////////////////////////////////////////////////////////////////////////
class CCSCreateCharacter : public CHeadPacket
{ public:
	char				m_sName[PLAYER_NAME_SIZE];
	CShape				m_cShape;
	int					m_nEquip[5];
};

class CSCCreateCharacter : public CHeadPacket
{ public:
	char				m_nResponse;
	CSCCreateCharacter() : CHeadPacket(CM_CREATE_CHARACTER) { m_nBodySize = 1; }
};
//////////////////////////////////////////////////////////////////////////
class CCSDeleteCharacter : public CHeadPacket
{ public:
	int					m_nPlayerSeq;
	char				m_sJumin[20];
};

class CSCDeleteCharacter : public CHeadPacket
{ public:
	char				m_nResponse;
	int					m_nPlayerSeq;
	int					m_nDeleteDate;
	CSCDeleteCharacter() : CHeadPacket(CM_DELETE_CHARACTER) { m_nBodySize = 9; }
};
//////////////////////////////////////////////////////////////////////////
class CCSChoiceCharacter : public CHeadPacket
{ public:
	int					m_nPlayerSeq;
};

class CSCChoiceCharacter : public CHeadPacket
{ public:
	char				m_nResponse;
	int					m_nPlayerSeq;
	CSCChoiceCharacter() : CHeadPacket(CM_CHOICE_CHARACTER) { m_nBodySize = 5; }
};
//////////////////////////////////////////////////////////////////////////
// Rebuild: replaced with the binary's newer 70-byte layout (the live client
// matches the Aug-2010 server, not the stale Protocol.Backup). Verified field
// offsets under pack(1): m_nCode@0 ... m_sTitle@39, total 70 bytes.
class CServerData
{ public:
	int					m_nCode;					// 0
	char				m_nState;					// 4
	char				m_nChannel;					// 5
	char				m_nMatch;					// 6
	char				m_nJob;						// 7
	char				m_nFree;					// 8
	char				m_nSLevel;					// 9
	char				m_nELevel;					// 10
	short				m_nMax;						// 11
	short				m_nCurrent;					// 13
	CAddress			m_cAddress;					// 15 (TCP: m_sIP[20]@15 + m_nPort@35) - client PutGameLogin a1
	CAddress			m_cUDPAddress;				// 39 (UDP: m_sIP[20]@39 + m_nPort@59) - client PutGameLogin a2 (exit(1) if unset)
	char				m_sTitle[SERVER_NAME_SIZE];	// 63 (31 bytes) - client reads the name here
};

class CCSServerList : public CHeadPacket
{ public:
	char				m_nChannel;
};

class CSCServerList : public CHeadPacket
{ public:
	char				m_nResponse;
	char				m_nChannel;
	CServerData			m_cServerData[LIST10_SIZE];
	CSCServerList() : CHeadPacket(CM_SERVER_LIST) { m_nBodySize = 2 + LIST10_SIZE * 94; }  // 942: official client 94-byte entries (was 0x2be/70-byte)
};
//////////////////////////////////////////////////////////////////////////
class CCSExecuteTutorial : public CHeadPacket
{ public:
	char				m_nTutorial;
	char				m_nOrder;
};

class CSCExecuteTutorial : public CHeadPacket
{ public:
	char				m_nResponse;
	char				m_nTutorial;
	CSCExecuteTutorial() : CHeadPacket(CM_EXECUTE_TUTORIAL) { m_nBodySize = 2; }
};
//////////////////////////////////////////////////////////////////////////
class CCSExecuteQuest : public CHeadPacket
{ public:
	char				m_nQuest;
	char				m_nOrder;
};

class CSCExecuteQuest : public CHeadPacket
{ public:
	char				m_nResponse;
	char				m_nQuest;
	CMoney				m_cMoney;
	// Aug-2010 server uses command 0xA90 (= CM_EXECUTE_QUEST = 2704).
	CSCExecuteQuest() : CHeadPacket(CM_EXECUTE_QUEST) { m_nBodySize = 0x12; }
};
//////////////////////////////////////////////////////////////////////////
// GAME PROTOCOL
//////////////////////////////////////////////////////////////////////////
class CCSGameLogin : public CHeadPacket
{ public:
	int				m_nMemberSeq;
	int				m_nPlayerSeq;
};

class CSCGameLogin : public CHeadPacket
{ public:
	CSCGameLogin() : CHeadPacket(CM_GAME_LOGIN) { m_nBodySize = 1; }
	char			m_nResponse;
};
//////////////////////////////////////////////////////////////////////////
class CCSGameExit : public CHeadPacket
{ public:
	char			m_nReason;
};

class CSCGameExit : public CHeadPacket
{ public:
	char			m_nResponse;
	char			m_nReason;	// rebuild: binary CSCGameExit (offset 0x0d)
	// XKICK_Game1 CSCGameExit ctor: command 2001 (0x7d1, sent raw), body 2 bytes.
	CSCGameExit() : CHeadPacket(CM_GAME_EXIT) { m_nBodySize = 2; }
};
//////////////////////////////////////////////////////////////////////////
class CCSUDPConfirm : public CHeadPacket
{ public:
	int				m_nPlayerSeq;
};

class CSCUDPConfirm : public CHeadPacket
{ public:
	char			m_nResponse;
	// XKICK_Game1 CSCUDPConfirm ctor: command 2002, body 1 byte. Without this the
	// reply went out as command 0 and the client never confirmed UDP -> timeout.
	CSCUDPConfirm() : CHeadPacket(CM_UDP_CONFIRM) { m_nBodySize = 1; }
};
//////////////////////////////////////////////////////////////////////////
// �������� ��� (CYG)
class CNoticeData
{ public:
	int					m_nNoticeSeq;
	char				m_strText[TIP_SIZE];
};

class CCSNoticeList : public CHeadPacket
{ public:
	int m_nVersion; // �������� ����
};

// Rebuild: layout pinned to the binary (646 bytes). The newer server added an
// int m_nCount and shrank the array to LIST5_SIZE (was LIST10_SIZE here).
class CSCNoticeList : public CHeadPacket
{ public:
	char m_nResponse;
	int    m_nVersion;
	int    m_nCount;
	CNoticeData m_cNoticeList[LIST5_SIZE];
	CSCNoticeList() : CHeadPacket(CM_NOTICE_LIST) { m_nBodySize = 0x27a; }
};

//////////////////////////////////////////////////////////////////////////
// Byte-exact to XKICK_Game1 CPlayerInfo (748 bytes). Note vs the loose earlier
// version: m_nOperation IS sent (0x4), there is NO m_cItemFaculty on the wire
// (only Base/Raise/Training), the two rankings are 25 shorts each (50 bytes -
// the wire ranking count, independent of the wider internal CRanking), and the
// 344-byte item-option block trails. GetPlayerInfo (stubs.cpp) fills it.
class CPlayerInfo
{ public:
	int					m_nPlayerSeq;			// 0x00
	char				m_nOperation;			// 0x04
	char				m_sName[PLAYER_NAME_SIZE];	// 0x05  (15)
	char				m_sMent[PLAYER_MENT_SIZE];	// 0x14  (45)
	CLevel				m_cLevel;				// 0x41  (12)
	CFaculty			m_cBaseFaculty;			// 0x4d  (25)
	CFaculty			m_cRaiseFaculty;		// 0x66  (25)
	CFaculty			m_cTrainingFaculty;		// 0x7f  (25)
	CRecord				m_cTotalRecord;			// 0x98  (76)
	CRecord				m_cQuarterRecord;		// 0xe4  (76)
	short				m_cTotalRanking[25];	// 0x130 (50)
	short				m_cQuarterRanking[25];	// 0x162 (50)
	unsigned char		m_cItemOption[344];		// 0x194 (344, COption block)
};
static_assert(sizeof(CPlayerInfo) == 748, "CPlayerInfo must be 748 bytes (binary wire size)");

class CCSPlayerInfo: public CHeadPacket
{ public:
	int					m_nPlayerSeq;
};

class CSCPlayerInfo : public CHeadPacket
{ public:
	CSCPlayerInfo() : CHeadPacket(CM_PLAYER_INFO) { m_nBodySize = 749; }
	char				m_nResponse;
	CPlayerInfo			m_cPlayerInfo;
};
//////////////////////////////////////////////////////////////////////////
class CItemInfo
{ public:
	int					m_nItemSeq;
	int					m_nCode;
	char				m_nEquipKind;
	short				m_nAmount;
	char				m_nSlot;
	int					m_nOptionCode[OPTION_SIZE];
	int					m_nSlotCode[SLOT_SIZE];
};

class CCSItemInfo : public CHeadPacket
{ public:
	int					m_nPlayerSeq;
};

class CSCItemInfo : public CHeadPacket
{ public:
	CSCItemInfo() : CHeadPacket(CM_ITEM_INFO) { m_nBodySize = 1112; }
	char				m_nResponse;
	char				m_nCount;
	// rebuild: Game serializes up to 30 of the 37-byte wire item rows (CItemRow)
	// raw at +0xe with stride 0x25; opaque block keeps sizeof == 1124.
	unsigned char		m_cItemInfo[30 * 37];  //������Ŷ �� ���� ������ �ٸ� ������ �߰����� �� �� (CYG)
};
//////////////////////////////////////////////////////////////////////////
class CCSSkillInfo : public CHeadPacket
{ public:
	int					m_nPlayerSeq;
};

class CSCSkillInfo : public CHeadPacket
{ public:
	CSCSkillInfo() : CHeadPacket(CM_SKILL_INFO) { m_nBodySize = 502; }
	char				m_nResponse;
	char				m_nCount;
	CSkillInfo			m_cSkillInfo[MAX_SKILL_LIST]; // ������Ŷ �� ���� ������ �ٸ� ������ �߰����� �� �� (CYG)
};
//////////////////////////////////////////////////////////////////////////
class CCSTrainingInfo : public CHeadPacket
{ public:
	int					m_nPlayerSeq;
};

class CSCTrainingInfo : public CHeadPacket
{ public:
	CSCTrainingInfo() : CHeadPacket(CM_TRAINING_INFO) { m_nBodySize = 452; }
	char				m_nResponse;
	char				m_nCount;
	CTrainingInfo		m_cTrainingInfo[50]; // rebuild: binary CSCTrainingInfo holds 50 rows //������Ŷ �� ���� ������ �ٸ� ������ �߰����� �� �� (CYG)
};
//////////////////////////////////////////////////////////////////////////
class CCSCeremonyInfo : public CHeadPacket
{ public:
	int					m_nPlayerSeq;
};

class CSCCeremonyInfo : public CHeadPacket
{ public:
	CSCCeremonyInfo() : CHeadPacket(CM_CEREMONY_INFO) { m_nBodySize = 452; }
	char				m_nResponse;
	char				m_nCount;
	CCeremonyInfo		m_cCeremonyInfo[50];// rebuild: binary CSCCeremonyInfo holds 50 rows //������Ŷ �� ���� ������ �ٸ� ������ �߰����� �� �� (CYG)
};
//////////////////////////////////////////////////////////////////////////
class CQuestRow
{ public:
	int					m_nQuestSeq;
	int					m_nCode;
	short				m_nAmount;
	int					m_nPlayDate;
};

class CCSQuestInfo : public CHeadPacket
{ public:
	int					m_nPlayerSeq;
};

class CSCQuestInfo : public CHeadPacket
{ public:
	CSCQuestInfo() : CHeadPacket(CM_QUEST_INFO) { m_nBodySize = 142; }
	char				m_nResponse;
	char				m_nCount;
	CQuestRow			m_cQuestInfo[MAX_QUEST_LIST]; // ������Ŷ �� ���� ������ �ٸ� ������ �߰����� �� �� (CYG)
};
//////////////////////////////////////////////////////////////////////////
class CSaleList
{ public:
	char				m_nObjectKind;
	int					m_nCode;
	char				m_nSaleKind;
	int					m_nPrice;
	short				m_nAmount;
	int					m_nSaleDate;
};

class CCSSaleList : public CHeadPacket
{ public:
	char				m_nPeriod;
	char				m_nPage;
};

// [rebuild] removed stale game item struct CSCSaleList (binary-accurate version in game/GameProtocolItem.h)
//////////////////////////////////////////////////////////////////////////
class CRoomInfo
{ public:
	char				m_nState;
	char				m_nMode;
	char				m_nCource;
	int					m_nRoomSeq;
	char				m_nRoomJangTeam;
	int					m_nHomeJangSeq;
	int					m_nAwayJangSeq;

	char				m_sTitle[TITLE_NAME_SIZE];
	char				m_sPass[5];
	int					m_nQuestCode;
	char 				m_nGroundCode;
	char 				m_nBallCode;
	char				m_nTimeCode;
	char				m_nWeatherCode;
	char				m_nAttackCode;
	char 				m_nScaleCode;
	char				m_nAICode;
	short				m_nPointCode;	// rebuild: binary CRoomInfo (offset 0x4f)
	char				m_nAreaCode;	// rebuild: binary CRoomInfo (offset 0x51)
	char 				m_nStartLevel;
	char 				m_nEndLevel;
	char				m_nAttackTeam;
	char				m_nMaxCount;

	char 				m_nCheckClub;
	char 				m_nCheckTime;
	char 				m_nCheckWeather;
	char 				m_nCheckView;
	char 				m_nCheckViewChat;
	CReserveSeat		m_cHomeSeat;
	CReserveSeat		m_cAwaySeat;
	CReserveSeat		m_cViewSeat;
};

class CCSRoomInfo : public CHeadPacket
{ public:
	int					m_nPlayerSeq;
};

class CSCRoomInfo: public CHeadPacket
{ public:
	CSCRoomInfo() : CHeadPacket(CM_ROOM_INFO) { m_nBodySize = 488; }
	char				m_nResponse;
	CRoomInfo			m_cRoomInfo;
};
//////////////////////////////////////////////////////////////////////////
class CRoomData
{ public:
	char				m_nState;
	char				m_nMode;
	char				m_nCource;
	int					m_nRoomSeq;

	char				m_sTitle[TITLE_NAME_SIZE];
	char 				m_nScaleCode;
	char				m_nAICode;
	short				m_nPointCode;	// rebuild: binary CRoomData (offset 0x38)
	char				m_nAreaCode;	// rebuild: binary CRoomData (offset 0x3a)
	char 				m_nStartLevel;
	char 				m_nEndLevel;
	char 				m_nCheckClub;
	char 				m_nCheckView;
	char				m_nAthleteCount;
	char				m_nMaxCount;
	char				m_nViewCount;
	short				m_nRoomSpeed;	// rebuild: binary CRoomData (offset 0x42)
	CReserveSeat		m_cHomeSeat;
	CReserveSeat		m_cAwaySeat;
};

class CCSRoomList : public CHeadPacket
{ public:
	char				m_nListKind;
	char				m_nPage;
};

class CSCRoomList: public CHeadPacket
{ public:
	CSCRoomList() : CHeadPacket(CM_ROOM_LIST) { m_nBodySize = 1663; }
	char				m_nResponse;
	char				m_nPage;
	char				m_nTotalPage;	// rebuild: binary CSCRoomList (offset 0x0e)
	CRoomData			m_cRoomData[LIST5_SIZE];
};
//////////////////////////////////////////////////////////////////////////
class CLobbyData
{ public:
	char				m_nState;
	int					m_nPlayerSeq;	// rebuild: binary CLobbyData (int @ offset 0x01)
	char				m_nOperation;	// rebuild: binary CLobbyData (offset 0x05)
	char				m_nPosition;
	char				m_nLevel;
	char				m_sName[PLAYER_NAME_SIZE];
	char				m_sMent[PLAYER_MENT_SIZE];
};

class CCSLobbyList : public CHeadPacket
{ public:
	char				m_nPage;
};

class CSCLobbyList : public CHeadPacket
{ public:
	CSCLobbyList() : CHeadPacket(CM_LOBBY_LIST) { m_nBodySize = 682; }
	char				m_nResponse;
	char				m_nPage;
	CLobbyData			m_cLobbyData[LIST10_SIZE];
};
//////////////////////////////////////////////////////////////////////////
class CCSCreateRoom : public CHeadPacket   // XKICK_Game1 CCSCreateRoom: 92 bytes
{ public:
	CCSCreateRoom() : CHeadPacket(CM_CREATE_ROOM) { m_nBodySize = 0x50; }   // 92 - 12
	char				m_nState;			// 0x0c
	char				m_nMode;			// 0x0d
	char				m_sTitle[TITLE_NAME_SIZE];	// 0x0e
	char				m_sPass[5];			// 0x3d
	char				m_nAttackCode;		// 0x42
	char 				m_nScaleCode;		// 0x43
	char				m_nAICode;			// 0x44
	short				m_nPointCode;		// 0x45
	char				m_nAreaCode;		// 0x47
	char 				m_nStartLevel;		// 0x48
	char 				m_nEndLevel;		// 0x49
	char 				m_nCheckClub;		// 0x4a
	char 				m_nCheckTime;		// 0x4b
	char 				m_nCheckWeather;	// 0x4c
	char 				m_nCheckView;		// 0x4d
	char 				m_nCheckViewChat;	// 0x4e
	char				m_nMaxCount;		// 0x4f
	char				m_nHomePosition[TEAM_SIZE];	// 0x50
	char				m_nAwayPosition[TEAM_SIZE];	// 0x56
};

class CSCCreateRoom: public CHeadPacket
{ public:
	CSCCreateRoom() : CHeadPacket(CM_CREATE_ROOM) { m_nBodySize = 1; }
	char				m_nResponse;
};
//////////////////////////////////////////////////////////////////////////
class CCSSetRoom : public CHeadPacket   // XKICK_Game1 CCSSetRoom: 84 bytes
{ public:
	CCSSetRoom() : CHeadPacket(CM_SET_ROOM) { m_nBodySize = 0x48; }   // 84 - 12
	int					m_nRoomSeq;			// 0x0c
	char				m_nState;			// 0x10
	char				m_nMode;			// 0x11
	char				m_sTitle[TITLE_NAME_SIZE];	// 0x12
	char				m_sPass[5];			// 0x41
	char				m_nAttackCode;		// 0x46
	char 				m_nScaleCode;		// 0x47
	char				m_nAICode;			// 0x48
	short				m_nPointCode;		// 0x49
	char				m_nAreaCode;		// 0x4b
	char 				m_nStartLevel;		// 0x4c
	char 				m_nEndLevel;		// 0x4d
	char 				m_nCheckClub;		// 0x4e
	char 				m_nCheckTime;		// 0x4f
	char 				m_nCheckWeather;	// 0x50
	char 				m_nCheckView;		// 0x51
	char 				m_nCheckViewChat;	// 0x52
	char				m_nMaxCount;		// 0x53
};

class CSCSetRoom : public CHeadPacket
{ public:
	CSCSetRoom() : CHeadPacket(CM_SET_ROOM) { m_nBodySize = 1; }
	char				m_nResponse;	// rebuild: binary CSCSetRoom is response-only (13 bytes)
};
//////////////////////////////////////////////////////////////////////////
class CCSChoiceRoom : public CHeadPacket
{ public:
	int					m_nRoomSeq;
	char				m_nType;
	char				m_sPass[5];
};

class CSCChoiceRoom: public CHeadPacket
{ public:
	CSCChoiceRoom() : CHeadPacket(CM_CHOICE_ROOM) { m_nBodySize = 6; }
	char				m_nResponse;
	int					m_nRoomSeq;
	char				m_nTeam;
};
//////////////////////////////////////////////////////////////////////////
class CCSQuickRoom : public CHeadPacket
{ public:
	int					m_nPlayerSeq;
};

class CSCQuickRoom : public CHeadPacket
{ public:
	CSCQuickRoom() : CHeadPacket(CM_QUICK_ROOM) { m_nBodySize = 1; }
	char				m_nResponse;
};
//////////////////////////////////////////////////////////////////////////
class CCSLeaveRoom : public CHeadPacket
{ public:
	int					m_nRoomSeq;
};

class CSCLeaveRoom: public CHeadPacket
{ public:
	CSCLeaveRoom() : CHeadPacket(CM_LEAVE_ROOM) { m_nBodySize = 6; }
	char				m_nResponse;
	int					m_nLeavePlayerSeq;
	char				m_nLeaveTeam;
};
//////////////////////////////////////////////////////////////////////////
class CCSChangeParent : public CHeadPacket
{ public:
};

class CSCChangeParent: public CHeadPacket
{ public:
	CSCChangeParent() : CHeadPacket(CM_CHANGE_PARENT) { m_nBodySize = 29; }
	char				m_nResponse;
	int					m_nParentSeq;
	CAddress			m_cParentAddress;
};
//////////////////////////////////////////////////////////////////////////
class CCSChangeJang : public CHeadPacket
{ public:
	int					m_nPlayerSeq;
};

class CSCChangeJang: public CHeadPacket
{ public:
	CSCChangeJang() : CHeadPacket(CM_CHANGE_JANG) { m_nBodySize = 10; }
	char				m_nResponse;
	char				m_nRoomJangTeam;
	int					m_nHomeJangSeq;
	int					m_nAwayJangSeq;
};
//////////////////////////////////////////////////////////////////////////
class CAthleteInfo 
{ public:
	int					m_nPlayerSeq;
	char				m_nOperation;	// rebuild: binary CAthleteInfo (offset 0x04)
	char				m_nPosition;
	char				m_nTeam;
	char				m_nSeat;
	char				m_sName[PLAYER_NAME_SIZE];
	char				m_sMent[PLAYER_MENT_SIZE]; //��Ʈ �߰� (CYG)
	CLevel				m_cLevel;
	CShape				m_cShape;
	CAddress			m_cAddress;
	CFaculty			m_cBaseFaculty;
	CFaculty			m_cRaiseFaculty;
	CFaculty			m_cTrainingFaculty;	// rebuild: binary has NO m_cItemFaculty (only 3 faculties)
	int					m_nEquipWear[MAX_EQUIP];
	int					m_nHomeWear[4];
	int					m_nAwayWear[4];
	CSkillInfo			m_cSkillInfo[MAX_SKILL];
	CCeremonyInfo		m_cCeremonyInfo[MAX_CEREMONY];
	unsigned char		m_cItemOption[344];	// rebuild: COption block at binary offset 0x33b
};

class CCSAthleteInfo : public CHeadPacket
{ public:
	int					m_nPlayerSeq;
};

class CSCAthleteInfo : public CHeadPacket
{ public:
	CSCAthleteInfo() : CHeadPacket(CM_ATHLETE_INFO) { m_nBodySize = 1172; }
	char				m_nResponse;
	CAthleteInfo		m_cAthleteInfo;
};
//////////////////////////////////////////////////////////////////////////
class CCSAthleteEnd : public CHeadPacket
{ public:
};

class CSCAthleteEnd : public CHeadPacket
{ public:
	CSCAthleteEnd() : CHeadPacket(CM_ATHLETE_END) { m_nBodySize = 1; }
	char				m_nResponse;
};
//////////////////////////////////////////////////////////////////////////
class CRobotInfo
{ public:
	int					m_nRobotSeq;
	char				m_nTeam;
	char				m_nSeat;
	char				m_nLevel;
	char				m_nPosition;
	int					m_nCostume;
};

class CCSRobotInfo : public CHeadPacket
{ public:
	int					m_nRoomSeq;
};

class CSCRobotInfo : public CHeadPacket
{ public:
	CSCRobotInfo() : CHeadPacket(CM_ROBOT_INFO) { m_nBodySize = 13; }
	char				m_nResponse;
	CRobotInfo			m_cRobotInfo;
};
//////////////////////////////////////////////////////////////////////////
class CCSRobotEnd : public CHeadPacket
{ public:
};

class CSCRobotEnd : public CHeadPacket
{ public:
	CSCRobotEnd() : CHeadPacket(CM_ROBOT_END) { m_nBodySize = 1; }
	char				m_nResponse;
};
//////////////////////////////////////////////////////////////////////////
class CCSChangeGround : public CHeadPacket
{ public:
	int					m_nGroundCode;
	int					m_nTimeCode;	// rebuild: binary CCSChangeGround (offset 0x10) - time-of-day code
};
static_assert(offsetof(CCSChangeGround, m_nGroundCode) == 0xc, "CCSChangeGround.m_nGroundCode @0xc");
static_assert(offsetof(CCSChangeGround, m_nTimeCode)   == 0x10, "CCSChangeGround.m_nTimeCode @0x10");
static_assert(sizeof(CCSChangeGround) == 0x14, "CCSChangeGround wire size != 20");

class CSCChangeGround : public CHeadPacket
{ public:
	CSCChangeGround() : CHeadPacket(CM_CHANGE_GROUND) { m_nBodySize = 9; }
	char				m_nResponse;
	int					m_nGroundCode;
	int					m_nTimeCode;	// rebuild: binary CSCChangeGround (offset 0x11)
};
//////////////////////////////////////////////////////////////////////////
class CCSChangeBall : public CHeadPacket
{ public:
	int					m_nBallCode;
};

class CSCChangeBall : public CHeadPacket
{ public:
	CSCChangeBall() : CHeadPacket(CM_CHANGE_BALL) { m_nBodySize = 5; }
	char				m_nResponse;
	int					m_nBallCode;
};
//////////////////////////////////////////////////////////////////////////
class CCSForceOut : public CHeadPacket
{ public:
	int					m_nPlayerSeq;
};

class CSCForceOut : public CHeadPacket
{ public:
	CSCForceOut() : CHeadPacket(CM_FORCE_OUT) { m_nBodySize = 1; }
	char				m_nResponse;
};
//////////////////////////////////////////////////////////////////////////
class CCSInvitePlayer : public CHeadPacket
{ public:
	int  m_nPlayerSeq;
	char m_sMessage[MESSAGE_SIZE];
};

class CSCInvitePlayer : public CHeadPacket
{ public:
	CSCInvitePlayer() : CHeadPacket(CM_INVITE_PLAYER) { m_nBodySize = 146; }
	char m_nResponse;
	char m_sFromName[PLAYER_NAME_SIZE];
	char m_sMessage[121];	// rebuild: binary wire message is 121 bytes (not MESSAGE_SIZE)
	char m_sPass[5];
	int m_nRoomSeq;
};
//////////////////////////////////////////////////////////////////////////

class CCSGameReady : public CHeadPacket
{ public:
	char				m_nReady;
};

class CSCGameReady : public CHeadPacket
{ public:
	CSCGameReady() : CHeadPacket(CM_GAME_READY) { m_nBodySize = 3; }
	char				m_nResponse;
	char				m_nReady;
	char				m_nCancelTeam;
};
//////////////////////////////////////////////////////////////////////////
class CCSGameStart : public CHeadPacket
{ public:
	int					m_nRoomSeq;
};

class CSCGameStart : public CHeadPacket
{ public:
	CSCGameStart() : CHeadPacket(CM_GAME_START) { m_nBodySize = 56; }
	char				m_nResponse;
	int					m_nParentSeq;
	int					m_nWeatherCode;	// rebuild: binary CSCGameStart (offset 0x11)
	int					m_nTimeCode;	// rebuild: binary CSCGameStart (offset 0x15)
	short				m_nRandom;		// rebuild: binary CSCGameStart (offset 0x19)
	CAddress			m_cParentAddress;
	CMission			m_cMission; // �̼��ڵ�(CYG)
	char				m_nAttackTeam;	// rebuild: binary CSCGameStart (offset 0x43)
};
//////////////////////////////////////////////////////////////////////////
class CCSGameCount : public CHeadPacket
{ public:
	char				m_nCount;
};

class CSCGameCount : public CHeadPacket
{ public:
	CSCGameCount() : CHeadPacket(CM_GAME_COUNT) { m_nBodySize = 2; }
	char				m_nResponse;
	char				m_nCount;
};
//////////////////////////////////////////////////////////////////////////
class CCSGameLoad : public CHeadPacket
{ public:
	char				m_nStep;
};

class CSCGameLoad : public CHeadPacket
{ public:
	CSCGameLoad() : CHeadPacket(CM_GAME_LOAD) { m_nBodySize = 6; }
	char				m_nResponse;
	int					m_nPlayerSeq;
	char				m_nStep;
};
//////////////////////////////////////////////////////////////////////////
class CCSGamePlay : public CHeadPacket
{ public:
	int					m_nRoomSeq;
};

class CSCGamePlay : public CHeadPacket
{ public:
	CSCGamePlay() : CHeadPacket(CM_GAME_PLAY) { m_nBodySize = 1; }
	char				m_nResponse;
};
//////////////////////////////////////////////////////////////////////////
class CCSGameResult : public CHeadPacket
{ public:
	int					m_nMvpSeq;
	char				m_nMvpLevel;
	char				m_nMvpPosition;
	char				m_sMvpName[PLAYER_NAME_SIZE];
	float				m_fCurrentTime;
	CResult				m_cHomeResult;
	CResult				m_cAwayResult;
	CEachResult			m_cEachResult[TEAM_SIZE*2];
};

class CSCGameResult : public CHeadPacket
{ public:
	CSCGameResult() : CHeadPacket(CM_GAME_RESULT) { m_nBodySize = 1106; }
	char				m_nResponse;
	int					m_nMvpSeq;
	char				m_nMvpLevel;
	char				m_nMvpPosition;
	char				m_sMvpName[PLAYER_NAME_SIZE];
	float				m_fCurrentTime;
	CResult				m_cHomeResult;
	CResult				m_cAwayResult;
	CEachResult			m_cEachResult[TEAM_SIZE*2];
};
//////////////////////////////////////////////////////////////////////////
class CCSGameEnd : public CHeadPacket
{ public:
	int					m_nRoomSeq;
};

class CSCGameEnd : public CHeadPacket
{ public:
	CSCGameEnd() : CHeadPacket(CM_GAME_END) { m_nBodySize = 1; }
	char				m_nResponse;
};
//////////////////////////////////////////////////////////////////////////
class CCSLevelUp : public CHeadPacket
{ public:
};

class CSCLevelUp: public CHeadPacket
{ public:
	CSCLevelUp() : CHeadPacket(CM_LEVEL_UP) { m_nBodySize = 58; }
	char				m_nResponse;
	CMoney				m_cMoney;
	CLevel				m_cLevel;
	CFaculty			m_cBaseFaculty;
};
//////////////////////////////////////////////////////////////////////////
class CCSChangeTeam : public CHeadPacket
{ public:
	char				m_nChangeTeam;
};

class CSCChangeTeam : public CHeadPacket
{ public:
	CSCChangeTeam() : CHeadPacket(CM_CHANGE_TEAM) { m_nBodySize = 404; }
	char				m_nResponse;
	int					m_nPlayerSeq;
	char				m_nFromTeam;
	char				m_nToTeam;
	char				m_nSeat;
	CReserveSeat		m_cHomeSeat;
	CReserveSeat		m_cAwaySeat;
	CReserveSeat		m_cViewSeat;
};
//////////////////////////////////////////////////////////////////////////
class CCSChangePosition : public CHeadPacket
{ public:
	char				m_nHomePosition[TEAM_SIZE];
	char				m_nAwayPosition[TEAM_SIZE];
};

class CSCChangePosition : public CHeadPacket
{ public:
	CSCChangePosition() : CHeadPacket(CM_CHANGE_POSITION) { m_nBodySize = 265; }
	char				m_nResponse;
	CReserveSeat		m_cHomeSeat;
	CReserveSeat		m_cAwaySeat;
};
//////////////////////////////////////////////////////////////////////////
class CCSChangeMent : public CHeadPacket
{ public:
	char				m_sMent[PLAYER_MENT_SIZE];
};

class CSCChangeMent : public CHeadPacket
{ public:
	CSCChangeMent() : CHeadPacket(CM_CHANGE_MENT) { m_nBodySize = 46; }
	char				m_nResponse;
	char				m_sMent[PLAYER_MENT_SIZE];
};
//////////////////////////////////////////////////////////////////////////
class CCSGrowupCharacter : public CHeadPacket
{ public:
	char				m_nPosition;
};

class CSCGrowupCharacter : public CHeadPacket
{ public:
	CSCGrowupCharacter() : CHeadPacket(CM_GROWUP_CHARACTER) { m_nBodySize = 750; }
	char				m_nResponse;
	char				m_nPosition;	// rebuild: binary CSCGrowupCharacter (offset 0x0d)
	CPlayerInfo			m_cPlayerInfo;
};
//////////////////////////////////////////////////////////////////////////
// ����Ʈ�� ���� �������Ѵ� (CYG)
class CCSQuestReward : public CHeadPacket
{ public:
	char m_nQuest; // ����Ʈ �ڵ�
};

class CSCQuestReward : public CHeadPacket
{ public:
	CSCQuestReward() : CHeadPacket(CM_QUEST_REWARD) { m_nBodySize = 1; }
	char m_nResponse; // �����
};
//////////////////////////////////////////////////////////////////////////

// 3000����
//////////////////////////////////////////////////////////////////////////
// [rebuild] removed stale game item struct CShopItemData (binary-accurate version in game/GameProtocolItem.h)
// [rebuild] removed stale game item struct CCSShopItemList (binary-accurate version in game/GameProtocolItem.h)
// [rebuild] removed stale game item struct CSCShopItemList (binary-accurate version in game/GameProtocolItem.h)
//////////////////////////////////////////////////////////////////////////
class CCSUpdateItem : public CHeadPacket
{ public:
	int                 m_nPlayerSeq;
};

// [rebuild] removed stale game item struct CSCUpdateItem (binary-accurate version in game/GameProtocolItem.h)
//////////////////////////////////////////////////////////////////////////
// [rebuild] removed stale game item struct CCSEquipItem (binary-accurate version in game/GameProtocolItem.h)
// [rebuild] removed stale game item struct CSCEquipItem (binary-accurate version in game/GameProtocolItem.h)
//////////////////////////////////////////////////////////////////////////
// [rebuild] removed stale game item struct CCSDivestItem (binary-accurate version in game/GameProtocolItem.h)
// [rebuild] removed stale game item struct CSCDivestItem (binary-accurate version in game/GameProtocolItem.h)
//////////////////////////////////////////////////////////////////////////
// [rebuild] removed stale game item struct CCSBuyItem (binary-accurate version in game/GameProtocolItem.h)
// [rebuild] removed stale game item struct CSCBuyItem (binary-accurate version in game/GameProtocolItem.h)
//////////////////////////////////////////////////////////////////////////
// [rebuild] removed stale game item struct CCSGiftItem (binary-accurate version in game/GameProtocolItem.h)
// [rebuild] removed stale game item struct CSCGiftItem (binary-accurate version in game/GameProtocolItem.h)
//////////////////////////////////////////////////////////////////////////
// [rebuild] removed stale game item struct CCSExchangeItem (binary-accurate version in game/GameProtocolItem.h)
// [rebuild] removed stale game item struct CSCExchangeItem (binary-accurate version in game/GameProtocolItem.h)
//////////////////////////////////////////////////////////////////////////
class CShopSkillData
{ public:                 
	int                 m_nCode;
	char                m_nDiscount;
};

class CCSShopSkillList : public CHeadPacket
{ public:
	char				m_nType;
	char				m_nPosition;
	short				m_nPage;
};

class CSCShopSkillList : public CHeadPacket
{ public:
	CSCShopSkillList() : CHeadPacket(CM_SHOPSKILL_LIST) { m_nBodySize = 37; }
	char                m_nResponse;
	short				m_nCurrentPage;
	short				m_nTotalPage;
	char				m_nType;
	char				m_nPosition;
	CShopSkillData      m_cShopSkillData[LIST6_SIZE];
};
//////////////////////////////////////////////////////////////////////////
class CCSUpdateSkill : public CHeadPacket
{ public:
	int                 m_nPlayerSeq;
};

class CSCUpdateSkill : public CHeadPacket
{ public:
	CSCUpdateSkill() : CHeadPacket(CM_UPDATE_SKILL) { m_nBodySize = 12; }
	char                m_nResponse;
	char				m_nUpdateKind;
	int					m_nSkillSeq;
	int					m_nCode;
	char				m_nEquipKind;
	char				m_nLevel;
};
//////////////////////////////////////////////////////////////////////////
class CCSEquipSkill : public CHeadPacket
{ public:
	int                 m_nSkillSeq;
};

class CSCEquipSkill : public CHeadPacket
{ public:
	CSCEquipSkill() : CHeadPacket(CM_EQUIP_SKILL) { m_nBodySize = 6; }
	char                m_nResponse;
	int					m_nSkillSeq;
	char				m_nEquipKind;
};
//////////////////////////////////////////////////////////////////////////
class CCSDivestSkill : public CHeadPacket
{ public:
	int                 m_nSkillSeq;
};

class CSCDivestSkill : public CHeadPacket
{ public:
	CSCDivestSkill() : CHeadPacket(CM_DIVEST_SKILL) { m_nBodySize = 6; }
	char                m_nResponse;
	int					m_nSkillSeq;
	char				m_nEquipKind;
};
//////////////////////////////////////////////////////////////////////////
class CCSBuySkill : public CHeadPacket
{ public:
	int                 m_nCode;
	char                m_nBuyKind;
	int                 m_nPrice;
};

class CSCBuySkill : public CHeadPacket
{ public:
	CSCBuySkill() : CHeadPacket(CM_BUY_SKILL) { m_nBodySize = 18; }
	char                m_nResponse;
	CMoney              m_cMoney;
	char                m_nEquipKind;
};
//////////////////////////////////////////////////////////////////////////
class CShopTrainingData
{ public:                 
	int                 m_nCode;
	char                m_nDiscount;
};

class CCSShopTrainingList : public CHeadPacket
{ public:
	short				m_nPage;
};

class CSCShopTrainingList : public CHeadPacket
{ public:
	CSCShopTrainingList() : CHeadPacket(CM_SHOPTRAINING_LIST) { m_nBodySize = 35; }
	char                m_nResponse;
	short				m_nCurrentPage;
	short				m_nTotalPage;
	CShopTrainingData	m_cShopTrainingData[LIST6_SIZE];
};
//////////////////////////////////////////////////////////////////////////
class CCSUpdateTraining : public CHeadPacket
{ public:
	int                 m_nPlayerSeq;
};

class CSCUpdateTraining : public CHeadPacket
{ public:
	CSCUpdateTraining() : CHeadPacket(CM_UPDATE_TRAINING) { m_nBodySize = 11; }
	char                m_nResponse;
	char				m_nUpdateKind;
	int					m_nTrainingSeq;
	int					m_nCode;
	char				m_nLevel;
};
//////////////////////////////////////////////////////////////////////////
class CCSBuyTraining : public CHeadPacket
{ public:
	int                 m_nCode;
	char                m_nBuyKind;
	int                 m_nPrice;
};

class CSCBuyTraining : public CHeadPacket
{ public:
	CSCBuyTraining() : CHeadPacket(CM_BUY_TRAINING) { m_nBodySize = 43; }
	char                m_nResponse;
	CMoney              m_cMoney;
	char                m_nEquipKind;
	CFaculty			m_cTrainingFaculty;
};
//////////////////////////////////////////////////////////////////////////
class CShopCeremonyData
{ public:                 
	int                 m_nCode;
	char                m_nDiscount;
};

class CCSShopCeremonyList : public CHeadPacket
{ public:
	short				m_nPage;
};

class CSCShopCeremonyList : public CHeadPacket
{ public:
	CSCShopCeremonyList() : CHeadPacket(CM_SHOPCEREMONY_LIST) { m_nBodySize = 35; }
	char                m_nResponse;
	short				m_nCurrentPage;
	short				m_nTotalPage;
	CShopCeremonyData	m_cShopCeremonyData[LIST6_SIZE];
};
//////////////////////////////////////////////////////////////////////////
class CCSUpdateCeremony : public CHeadPacket
{ public:
	int                 m_nPlayerSeq;
};

class CSCUpdateCeremony : public CHeadPacket
{ public:
	CSCUpdateCeremony() : CHeadPacket(CM_UPDATE_CEREMONY) { m_nBodySize = 11; }
	char                m_nResponse;
	char				m_nUpdateKind;
	int					m_nCeremonySeq;
	int					m_nCode;
	char				m_nEquipKind;
};
//////////////////////////////////////////////////////////////////////////
class CCSEquipCeremony : public CHeadPacket
{ public:
	int                 m_nCeremonySeq;
};

class CSCEquipCeremony : public CHeadPacket
{ public:
	CSCEquipCeremony() : CHeadPacket(CM_EQUIP_CEREMONY) { m_nBodySize = 6; }
	char                m_nResponse;
	int					m_nCeremonySeq;
	char				m_nEquipKind;
};
//////////////////////////////////////////////////////////////////////////
class CCSDivestCeremony : public CHeadPacket
{ public:
	int                 m_nCeremonySeq;
};

class CSCDivestCeremony : public CHeadPacket
{ public:
	CSCDivestCeremony() : CHeadPacket(CM_DIVEST_CEREMONY) { m_nBodySize = 6; }
	char                m_nResponse;
	int					m_nCeremonySeq;
	char				m_nEquipKind;
};
//////////////////////////////////////////////////////////////////////////
class CCSBuyCeremony : public CHeadPacket
{ public:
	int                 m_nCode;
	char                m_nBuyKind;
	int                 m_nPrice;
};

class CSCBuyCeremony : public CHeadPacket
{ public:
	CSCBuyCeremony() : CHeadPacket(CM_BUY_CEREMONY) { m_nBodySize = 18; }
	char                m_nResponse;
	CMoney              m_cMoney;
	char                m_nEquipKind;
};
//////////////////////////////////////////////////////////////////////////
class CQuestData
{ public:                 
	int                 m_nCode;
};

class CCSQuestList : public CHeadPacket
{ public:
	short				m_nPage;
};

class CSCQuestList : public CHeadPacket
{ public:
	CSCQuestList() : CHeadPacket(CM_QUEST_LIST) { m_nBodySize = 29; }
	char                m_nResponse;
	short				m_nCurrentPage;
	short				m_nTotalPage;
	CQuestData			m_cQuestData[LIST6_SIZE];
};
//////////////////////////////////////////////////////////////////////////
class CCSUpdateQuest : public CHeadPacket
{ public:
	int                 m_nPlayerSeq;
};

class CSCUpdateQuest : public CHeadPacket
{ public:
	char                m_nResponse;
	char				m_nUpdateKind;
	int					m_nQuestSeq;
	int					m_nCode;
	short				m_nAmount;
	int					m_nPlayDate;
};
//////////////////////////////////////////////////////////////////////////
class CCSCreateQuest : public CHeadPacket
{ public:
	char				m_nState;
	char				m_nMode;
	char				m_sTitle[TITLE_NAME_SIZE];
	char				m_sPass[5];
	int					m_nQuestCode;
	char				m_nAttackCode;
	char 				m_nScaleCode;
	char				m_nAICode;
	char 				m_nStartLevel;
	char 				m_nEndLevel;
	char 				m_nCheckClub;
	char 				m_nCheckTime;
	char 				m_nCheckWeather;
	char 				m_nCheckView;
	char 				m_nCheckViewChat;
	char				m_nMaxCount;
	char				m_nHomePosition[TEAM_SIZE];
	char				m_nAwayPosition[TEAM_SIZE];
};

class CSCCreateQuest : public CHeadPacket
{ public:
	CSCCreateQuest() : CHeadPacket(CM_CREATE_QUEST) { m_nBodySize = 1; }
	char				m_nResponse;
};
//////////////////////////////////////////////////////////////////////////

// 5000����
//////////////////////////////////////////////////////////////////////////
class CCSTCPPing : public CHeadPacket
{ public:
	char				m_nNone;
};

class CSCTCPPing : public CHeadPacket
{ public:
	char				m_nResponse;
	CSCTCPPing() : CHeadPacket(CM_TCP_PING) { m_nBodySize = 1; }
};
//////////////////////////////////////////////////////////////////////////
class CCSSendMessage : public CHeadPacket
{ public:
	char				m_nChatKind;
	char				m_sToName[PLAYER_NAME_SIZE];
	char				m_sMessage[MESSAGE_SIZE];
};

class CSCSendMessage : public CHeadPacket
{ public:
	CSCSendMessage() : CHeadPacket(CM_SEND_MESSAGE) { m_nBodySize = 142; }
	int					m_nPlayerSeq;
	char				m_nResponse;
	char				m_nChatKind;
	char				m_sFromName[PLAYER_NAME_SIZE];
	char				m_sMessage[121];	// rebuild: binary wire message is 121 bytes (not MESSAGE_SIZE)
};
//////////////////////////////////////////////////////////////////////////
class CCSRaiseFaculty : public CHeadPacket
{ public:
	CFaculty			m_cChangeFaculty;
};

class CSCRaiseFaculty : public CHeadPacket
{ public:
	CSCRaiseFaculty() : CHeadPacket(CM_RAISE_FACULTY) { m_nBodySize = 38; }
	char				m_nResponse;
	CLevel				m_cLevel;
	CFaculty			m_cRaiseFaculty;
};
//////////////////////////////////////////////////////////////////////////
class CCSChangeSetting : public CHeadPacket
{ public:
	char				m_nInitSetting;
	CSetting			m_cChangeSetting;
};

class CSCChangeSetting : public CHeadPacket
{ public:
	char				m_nResponse;
	char				m_nInitSetting;		// rebuild: present in binary (offset 13)
	CSetting			m_cSetting;
	CSCChangeSetting() : CHeadPacket(CM_CHANGE_SETTING) { m_nBodySize = 0x5a; }
};
//////////////////////////////////////////////////////////////////////////
// UDP PROTOCOL 
//////////////////////////////////////////////////////////////////////////
class CCSUDPPunching : public CHeadPacket
{ public:
	int					m_nPlayerSeq;
};
//////////////////////////////////////////////////////////////////////////
class CSCUDPPing : public CHeadPacket
{ public:
	// rebuild: binary CSCUDPPing has NO body (header-only datagram, 12 bytes).
	// XKICK_Game1 CSCUDPPing ctor: command 9001, body 0.
	CSCUDPPing() : CHeadPacket(CM_UDP_PING) { m_nBodySize = 0; }
};
//////////////////////////////////////////////////////////////////////////
// ĳ���� ã�� (CYG)
class CCSCharacterSearch : public CHeadPacket
{ public:
	char m_sName[PLAYER_NAME_SIZE]; // ĳ���͸�
};

class CSCCharacterSearch : public CHeadPacket
{ public:
	char m_nResponse;
	int  m_nPlayerSeq;
	CSCCharacterSearch() : CHeadPacket(CM_CHARACTER_SEARCH) { m_nBodySize = 5; }
};
//////////////////////////////////////////////////////////////////////////
// ������ ���� (�����ý���) (CYG)
// [rebuild] removed stale game item struct CCSPostItem (binary-accurate version in game/GameProtocolItem.h)
// [rebuild] removed stale game item struct CSCPostItem (binary-accurate version in game/GameProtocolItem.h)
//////////////////////////////////////////////////////////////////////////
// �̼ǿ� ���� ������ �Ѵ�. (CYG)
class CCSMissionReward : public CHeadPacket
{ public:
	CMission m_cMission; // ������ �̼ǳ���
};

class CSCMissionReward : public CHeadPacket
{ public:
	CSCMissionReward() : CHeadPacket(CM_MISSION_REWARD) { m_nBodySize = 1; }
	char m_nResponse; // �����
};
//////////////////////////////////////////////////////////////////////////

//--SOURCE_AUTO_INSERT_SCRIPT		

//////////////////////////////////////////////////////////////////////////
// STS ���� �α��� (CYG)
class CCSStsLogin : public CHeadPacket
{ public:
	short m_nServerCode;
};

class CSCStsLogin : public CHeadPacket
{ public:
	char m_nResponse;
	CSCStsLogin() : CHeadPacket(CM_STS_LOGIN) { m_nBodySize = 1; }
};
//////////////////////////////////////////////////////////////////////////
// Rebuild: certify-only packets recovered from the binary that were absent from
// the old Protocol.Backup. Command ids / body sizes pinned to XKICK_Certify.

// Server -> server STS update acks (CM_UPDATE_WEATHER/NOTICE/SEND_BROADCAST).
#define CM_UPDATE_WEATHER   0x67
#define CM_UPDATE_SCHEDULE  0x68
#define CM_UPDATE_NOTICE    0x69
#define CM_SEND_BROADCAST   0x6A
#define CM_STS_DRAWFORCE    0x66
#define CM_EVENT_LIST       0x7D4
#define CM_DRAWFORCE_PLAYER 0x3EB
#define CM_CHOICE_SERVER    0x515
#define CM_SETTING_INFO     0x138C

class CSCUpdateWeather : public CHeadPacket
{ public:
	char m_nResponse;
	CSCUpdateWeather() : CHeadPacket(CM_UPDATE_WEATHER) { m_nBodySize = 1; }
};

class CSCUpdateNotice : public CHeadPacket
{ public:
	char m_nResponse;
	CSCUpdateNotice() : CHeadPacket(CM_UPDATE_NOTICE) { m_nBodySize = 1; }
};

class CSCSendBroadcast : public CHeadPacket
{ public:
	char m_nResponse;
	CSCSendBroadcast() : CHeadPacket(CM_SEND_BROADCAST) { m_nBodySize = 1; }
};

// Certify -> game STS: force-disconnect a player on the owning game server.
class CCSStsDrawforce : public CHeadPacket
{ public:
	int  m_nPlayerSeq;
	int  m_nMemberSeq;
	CCSStsDrawforce() : CHeadPacket(CM_STS_DRAWFORCE) { m_nBodySize = 8; }
};

// Game -> certify STS ack for a drawforce request.
class CSCStsDrawforce : public CHeadPacket
{ public:
	char m_nResponse;
	CSCStsDrawforce() : CHeadPacket(CM_STS_DRAWFORCE) { m_nBodySize = 1; }
};

// Client <-> certify: forced-login response (CM_DRAWFORCE_PLAYER).
class CSCDrawforcePlayer : public CHeadPacket
{ public:
	char m_nResponse;
	CSCDrawforcePlayer() : CHeadPacket(CM_DRAWFORCE_PLAYER) { m_nBodySize = 1; }
};

// Client -> certify: pick a game server; response carries its address.
class CCSChoiceServer : public CHeadPacket
{ public:
	int      m_nServerCode;
};

class CSCChoiceServer : public CHeadPacket
{ public:
	char     m_nResponse;
	CAddress m_cAddress;
	CSCChoiceServer() : CHeadPacket(CM_CHOICE_SERVER) { m_nBodySize = 0x19; }
};

// One active reward event (wire form of CLoad's CEvent; 16 bytes).
class CEventData
{ public:
	char m_nEventType;
	char m_nRewardType;
	char m_nPad[2];		// rebuild: binary CEvent pads m_nRewardValue to offset 0x4 (16 bytes)
	int  m_nRewardValue;
	int  m_nStartTime;
	int  m_nEndTime;
};

// Client -> certify: list of active reward events.
class CSCEventList : public CHeadPacket
{ public:
	char       m_nResponse;
	short      m_nCount;
	CEventData m_cEventList[30];
	CSCEventList() : CHeadPacket(CM_EVENT_LIST) { m_nBodySize = 0x1e3; }
};

// Client -> certify: another character's setting snapshot.
class CSCSettingInfo : public CHeadPacket
{ public:
	char     m_nResponse;
	int      m_nPlayerSeq;
	CSetting m_cSetting;
	CSCSettingInfo() : CHeadPacket(CM_SETTING_INFO) { m_nBodySize = 0x5d; }
};
//////////////////////////////////////////////////////////////////////////
// GAME REQUEST BODIES (client -> server). Their matching CSC replies live in
// game/include/GameProtocol.h; defined here because the request layouts were
// previously read via raw PFIELD offsets. All verified byte-exact vs XKICK_Game1.
//////////////////////////////////////////////////////////////////////////

// buddy list (replies: CSCBuddyInfo / CSCAddBuddy / CSCDelBuddy)
class CCSBuddyInfo : public CHeadPacket
{ public:
	int					m_nPage;		// PacketBuddyInfo -> GetBuddyInfo(page)
};
static_assert(offsetof(CCSBuddyInfo, m_nPage) == 0xc, "CCSBuddyInfo.m_nPage @0xc");

class CCSAddBuddy : public CHeadPacket
{ public:
	int					m_nPlayerSeq;	// PacketAddBuddy -> AddBuddyInfo(seq)
};
static_assert(offsetof(CCSAddBuddy, m_nPlayerSeq) == 0xc, "CCSAddBuddy.m_nPlayerSeq @0xc");

class CCSDelBuddy : public CHeadPacket
{ public:
	int					m_nPlayerSeq;	// PacketDelBuddy -> DeleteBuddyList(seq)
};
static_assert(offsetof(CCSDelBuddy, m_nPlayerSeq) == 0xc, "CCSDelBuddy.m_nPlayerSeq @0xc");

// blacklist (replies: CSCBlacklistInfo / CSCAddBlacklist / CSCDelBlacklist)
class CCSBlacklistInfo : public CHeadPacket
{ public:
	int					m_nPage;		// PacketBlacklistInfo -> GetBlacklistInfo(page)
};
static_assert(offsetof(CCSBlacklistInfo, m_nPage) == 0xc, "CCSBlacklistInfo.m_nPage @0xc");

class CCSAddBlacklist : public CHeadPacket
{ public:
	int					m_nPlayerSeq;	// PacketAddBlacklist -> AddBlackListInfo(seq)
};
static_assert(offsetof(CCSAddBlacklist, m_nPlayerSeq) == 0xc, "CCSAddBlacklist.m_nPlayerSeq @0xc");

class CCSDelBlacklist : public CHeadPacket
{ public:
	int					m_nPlayerSeq;	// PacketDelBlacklist -> DeleteBlackList(seq)
};
static_assert(offsetof(CCSDelBlacklist, m_nPlayerSeq) == 0xc, "CCSDelBlacklist.m_nPlayerSeq @0xc");

// weekly record / ranking (replies: CSCWeeklyRecord / CSCWeeklyRanking).
// The int is the target player_seq (used in the SQL WHERE player_seq = %d).
class CCSWeeklyRecord : public CHeadPacket
{ public:
	int					m_nPlayerSeq;	// PacketWeeklyRecord -> GetRecordField_Weekly(player_seq)
};
static_assert(offsetof(CCSWeeklyRecord, m_nPlayerSeq) == 0xc, "CCSWeeklyRecord.m_nPlayerSeq @0xc");

class CCSWeeklyRanking : public CHeadPacket
{ public:
	int					m_nPlayerSeq;	// PacketWeeklyRanking -> GetRankingField_Weekly(player_seq)
};
static_assert(offsetof(CCSWeeklyRanking, m_nPlayerSeq) == 0xc, "CCSWeeklyRanking.m_nPlayerSeq @0xc");

// GM room snapshot tool (reply: CSCRoomTool). Binary request type is CGSRoomTool.
class CCSRoomTool : public CHeadPacket
{ public:
	int					m_nRoomSeq;		// PacketRoomTool -> GetRoomPointer(roomseq)
};
static_assert(offsetof(CCSRoomTool, m_nRoomSeq) == 0xc, "CCSRoomTool.m_nRoomSeq @0xc");

// autopilot toggle (reply: CSCAutopilotMode). Single enable byte at 0xc.
class CCSAutopilotMode : public CHeadPacket
{ public:
	char				m_bEnable;		// PacketAutopilotMode -> EnableAutoPilot(enable)
};
static_assert(offsetof(CCSAutopilotMode, m_bEnable) == 0xc, "CCSAutopilotMode.m_bEnable @0xc");

// in-game switch/value broadcast (reply: CSCSwitchValue). char type @0xc, int value @0xd.
class CCSSwitchValue : public CHeadPacket
{ public:
	char				m_nType;		// switch id  (PacketSwitchValue)
	int					m_nValue;		// switch value
};
static_assert(offsetof(CCSSwitchValue, m_nType)  == 0xc, "CCSSwitchValue.m_nType @0xc");
static_assert(offsetof(CCSSwitchValue, m_nValue) == 0xd, "CCSSwitchValue.m_nValue @0xd");
//////////////////////////////////////////////////////////////////////////


#pragma pack(pop)   // rebuild: was an errant 'push,4' in the non-WIN32 original

#endif
