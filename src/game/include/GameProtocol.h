// =============================================================================
// XKICK_Game - GameProtocol.h
// The Game-specific #pragma pack(1) WIRE structs for the SPINE commands defined in
// packet.cpp, PLUS the widely-shared records other domain modules need. Each layout
// was derived from the handler that builds/reads it in XKICK_Game1 (memcpy sizes +
// field offsets are byte-exact).
//
// REUSE: most CCS*/CSC* spine packets already exist in common "Protocol.h" and are
// NOT redefined here. This header only adds:
//   (a) Game forms that DIFFER from the Certify-era Protocol.h layout (login carries
//       port+ip; raise-faculty/faculty-reset use a flat int level/exp block, not
//       CLevel), and
//   (b) packets Protocol.h never had (current-time/weather, buddy/blacklist list
//       data rows, weekly record/ranking, change-name, playerinfo-end, GM tools).
//
// Header note: CHeadPacket (Struct.h) is the 12-byte {int cmd; int bodySize; int
// seq;} frame; every body offset below is relative to that (body starts at +0xc).
//
// PACKETS OWNED BY DOMAIN MODULES (Wave 3) -- intentionally NOT defined here:
//   item    : CSCSaleList/CCSSaleList, CSCShopItemList, CSCBuyItem, CSCGiftItem,
//             CSCExchangeItem, CSCGiftList, CSCGetGift, CSCSellItem, CSCRepairItem,
//             CSCEnchantItem, CSCPostItem, CSCUpdateItem, CSCEquipItem,
//             CSCDivestItem, CSCMixItem1/2, CSCCashCoupon/PointCoupon, UpdateOption
//   skill   : CSCShopSkillList, CSCEquipSkill, CSCDivestSkill, CSCBuySkill, SkillSlot
//   ceremony: CSCShopCeremonyList, CSCEquipCeremony, CSCDivestCeremony, CSCBuyCeremony
//   training: CSCShopTrainingList, CSCBuyTraining
//   quest   : CSCQuestList, CSCCreateQuest, CSCGrowupCharacter, CSCQuestReward,
//             CSCMissionReward, CSCExecuteQuest
//   card    : CSCCardInfo, CSCCardbotInfo, CSCEquipCard, CSCDivestCard,
//             CSCBuyCardbooster, CSCCardEntry, CSCMixCard1/2
//   rndshop : CSCRandomshopitemList, CSCBuyRandomitem, CSCRefreshShop
//   room    : CSCRoomInfo/List, CSCLobbyList, CSCCreateRoom/SetRoom/ChoiceRoom/
//             QuickRoom/LeaveRoom, CSCChangeJang, CSCAthleteInfo, CSCRobotInfo,
//             CSCChangeGround/Ball/Weather, CSCInvitePlayer, CSCForceOut,
//             CSCGame{Ready,Start,Count,Load,Play,Result,End}, CSCAutopilotMode,
//             CSCSwitchValue, CSCGoalinTcp, CSCChangeTeam/Position, CSCChangeMent,
//             CSCSynchPlayer
//   team    : CSCTeamInfo, CSCChoiceTeam, CSCQuickTeam, CSCTeamPosition,
//             CSCLeaveTeam, CSCTeamReady, CSCApplyMatch
// (These will be defined in the owning module's header. Their item/sub-record
//  structs CItemInfo/CSkillInfo/CTrainingInfo/CCeremonyInfo/CQuestInfo already live
//  in Struct.h/Protocol.h and are reused.)
// =============================================================================
#ifndef _GAME_PROTOCOL_H_
#define _GAME_PROTOCOL_H_

#include "Struct.h"     // CHeadPacket, CSetting, CFaculty, CMoney, CLevel ...
#include "Command.h"    // CM_* ids
#include <cstddef>      // offsetof (CGMAthleteInfo layout asserts)

// Command ids for spine packets not present in Command.h's main block.
#ifndef CM_CURRENT_WEATHER
#define CM_CURRENT_WEATHER  0x07D5
#endif
#ifndef CM_CURRENT_TIME
#define CM_CURRENT_TIME     0x07D6
#endif

#pragma pack(push,1)

// =============================================================================
// LOGIN / SESSION  (Game forms wider than Protocol.h's Certify-era versions)
// =============================================================================
// PacketGameLogin @08074d20 reads 16 body bytes (Protocol.h's CCSGameLogin only
// declared memberSeq+playerSeq); the Game client additionally sends its UDP
// endpoint (port + ip).
class CCSGameLoginGame : public CHeadPacket
{ public:
    int     m_nMemberSeq;     // +0x0c
    int     m_nPlayerSeq;     // +0x10
    int     m_nPort;          // +0x14  (truncated to short into CPlayer+0x51a)
    int     m_nIP;            // +0x18  (CPlayer+0x51c)
};

// CSCGameLogin @08074d20: 1-byte body (just m_nResponse). The CSecure seed is
// echoed in the HEADER m_nSequence field (offset 8), NOT the body - the client
// primes its rolling cipher from m_nSequence. cmd 2000 is sent RAW (WriteTCP
// skips encryption), so the seed in the header survives untouched.
// (verified vs XKICK_Game1: CSCGameLogin ctor sets m_nBodySize=1; the handler
//  does cPacket.m_nSequence = GetTCPSecure()->m_nSendSeq.)
class CSCGameLoginGame : public CHeadPacket
{ public:
    char    m_nResponse;      // +0x0c  (body = 1 byte)
    CSCGameLoginGame() : CHeadPacket(CM_GAME_LOGIN) { m_nBodySize = 1; }
};
// (CCSGameExit/CSCGameExit, CCSUDPConfirm/CSCUDPConfirm, CSCNoticeList/CCSNoticeList,
//  CSCEventList/CEventData, CSCTCPPing -- REUSED from Protocol.h, byte-exact.)

// =============================================================================
// CURRENT TIME / WEATHER  (new; not in Protocol.h)
// =============================================================================
// PacketCurrentTime @0807fece : 6-byte body. m_nTimeType is a 0..3 time-of-day
// bucket; m_nTime is GetCurrentTimeSecond().
class CSCCurrentTime : public CHeadPacket
{ public:
    char    m_nResponse;      // +0x0c
    char    m_nTimeType;      // +0x0d
    int     m_nTime;          // +0x0e
    CSCCurrentTime() : CHeadPacket(CM_CURRENT_TIME) { m_nBodySize = 6; }
};

// PacketCurrentWeather @0807f954 : 6-byte body. m_nWeatherType from the matched
// CWeather row (default 1); m_nEndTime is that row's end (default now+300).
class CSCCurrentWeather : public CHeadPacket
{ public:
    char    m_nResponse;      // +0x0c
    char    m_nWeatherType;   // +0x0d
    int     m_nEndTime;       // +0x0e
    CSCCurrentWeather() : CHeadPacket(CM_CURRENT_WEATHER) { m_nBodySize = 6; }
};

// =============================================================================
// SETTING  (REUSE Protocol.h CCSChangeSetting/CSCChangeSetting body 0x5a and
//           CSCSettingInfo body 0x5d -- both confirmed byte-exact vs the binary.)
// =============================================================================

// =============================================================================
// PLAYER INFO
// =============================================================================
// PacketPlayerInfo @0807543c builds CSCPlayerInfo (REUSE Protocol.h CSCPlayerInfo
// + CPlayerInfo). Its body size is computed dynamically (base 0x2ed minus 4 per
// missing trailing ranking entry), so the sender sets m_nBodySize at runtime.
// PacketPlayerinfoEnd @0807f88e : tiny ack.
class CSCPlayerinfoEnd : public CHeadPacket
{ public:
    char    m_nResponse;      // +0x0c
    CSCPlayerinfoEnd() : CHeadPacket(CM_PLAYERINFO_END) { m_nBodySize = 1; }
};

// =============================================================================
// SEND MESSAGE  (Protocol.h CCSSendMessage/CSCSendMessage field ORDER matches the
//   binary; the only requirement is MESSAGE_SIZE == 0x79 (121). Reused as-is.)
//   CCS: char nChatKind@+0xc; char sToName[15]@+0xd; char sMessage[121]@+0x1c.
//   CSC: int nPlayerSeq@+0xc; char nResponse@+0x10; char nChatKind@+0x11;
//        char sFromName[15]@+0x12; char sMessage[121]@+0x21.
// =============================================================================

// =============================================================================
// RAISE FACULTY / FACULTY RESET  (Game forms differ from Protocol.h: they use a
//   FLAT int level/exp/facultyPoint block, NOT the 10-byte CLevel.)
// =============================================================================
// PacketRaiseFaculty @0807e220 : request is a 25-byte faculty delta block.
class CCSRaiseFacultyGame : public CHeadPacket
{ public:
    CFaculty m_cChangeFaculty;   // +0x0c  (25-byte delta)
};

// CSCRaiseFaculty @0807e220 : body 0x26 (38) -- flat level/exp/point + faculty.
class CSCRaiseFacultyGame : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    int      m_nLevel;           // +0x0d  (CPlayer m_nLevel)
    int      m_nExp;             // +0x11  (CPlayer m_nExp)
    int      m_nFacultyPoint;    // +0x15  (CPlayer m_nFacultyPoint)
    CFaculty m_cRaiseFaculty;    // +0x19  (current faculty block CPlayer+0x11d, 25B)
    CSCRaiseFacultyGame() : CHeadPacket(CM_RAISE_FACULTY) { m_nBodySize = 0x26; }
};

// PacketFacultyReset @0807cca4 : consume a reset-ticket item; resend faculty.
class CCSFacultyReset : public CHeadPacket
{ public:
    int      m_nItemSeq;         // +0x0c
};

class CSCFacultyReset : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    int      m_nItemSeq;         // +0x0d
    int      m_nLevel;           // +0x11
    int      m_nExp;             // +0x15
    int      m_nFacultyPoint;    // +0x19
    CFaculty m_cFaculty;         // +0x1d  (current faculty block, 25B)
    CSCFacultyReset() : CHeadPacket(CM_FACULTY_RESET) { m_nBodySize = 0x2a; }
};

// =============================================================================
// CHANGE NAME  (new; not in Protocol.h)
// =============================================================================
// PacketChangeName @0807d056 : consume a rename-ticket item, rename character.
class CCSChangeName : public CHeadPacket
{ public:
    int      m_nItemSeq;         // +0x0c  (rename-ticket item seq)
    char     m_sName[PLAYER_NAME_SIZE]; // +0x10  (15)
};

class CSCChangeName : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    int      m_nPlayerSeq;       // +0x0d  (binary reuses this field for item seq echo)
    char     m_sName[PLAYER_NAME_SIZE]; // +0x11  (15)
    CSCChangeName() : CHeadPacket(CM_CHANGE_NAME) { m_nBodySize = 0x14; }
};

// =============================================================================
// BUDDY  (PacketBuddyInfo @08080c40 / AddBuddy @08080d98 / DelBuddy @08080fa2)
// =============================================================================
// One paginated buddy row (26 bytes). m_nWhere: 0 offline,1 lobby,2 room,3 playing.
class CBuddyData
{ public:
    int      m_nPlayerSeq;       // +0x00
    char     m_nWhere;           // +0x04
    int      m_nState;           // +0x05  (server/room code when online)
    char     m_nLevel;           // +0x09
    char     m_nPosition;        // +0x0a
    char     m_sName[PLAYER_NAME_SIZE]; // +0x0b  (15)  -> total 26 bytes
};

class CSCBuddyInfo : public CHeadPacket
{ public:
    char       m_nResponse;      // +0x0c
    char       m_nPage;          // +0x0d  (0xFF if empty)
    char       m_nCount;         // +0x0e
    int        m_nTotalPage;     // +0x0f  (rebuild: present in binary CSCBuddyInfo)
    CBuddyData m_cBuddyData[10]; // +0x13  (10 * 26 = 260) -> body 0x10b
    CSCBuddyInfo() : CHeadPacket(CM_BUDDY_INFO) { m_nBodySize = 0x10b; }
};

class CSCAddBuddy : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    CSCAddBuddy() : CHeadPacket(CM_ADD_BUDDY) { m_nBodySize = 1; }
};

class CSCDelBuddy : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    int      m_nPlayerSeq;       // +0x0d  (echoed deleted seq)
    CSCDelBuddy() : CHeadPacket(CM_DEL_BUDDY) { m_nBodySize = 5; }
};

// =============================================================================
// BLACKLIST  (PacketBlacklistInfo @08080e02 / AddBlacklist @08080e7c /
//             DelBlacklist @0808103c)
// =============================================================================
// One paginated blacklist row (22 bytes).
class CBlacklistData
{ public:
    int      m_nPlayerSeq;       // +0x00
    char     m_nWhere;           // +0x04
    char     m_nLevel;           // +0x05
    char     m_nPosition;        // +0x06
    char     m_sName[PLAYER_NAME_SIZE]; // +0x07  (15)  -> total 22 bytes
};

class CSCBlacklistInfo : public CHeadPacket
{ public:
    char           m_nResponse;          // +0x0c
    char           m_nPage;              // +0x0d  (0xFF if empty)
    char           m_nCount;             // +0x0e
    int            m_nTotalPage;         // +0x0f  (rebuild: present in binary CSCBlacklistInfo)
    CBlacklistData m_cBlacklistData[10]; // +0x13  (10 * 22 = 220) -> body 0xe3
    CSCBlacklistInfo() : CHeadPacket(CM_BLACKLIST_INFO) { m_nBodySize = 0xe3; }
};

class CSCAddBlacklist : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    CSCAddBlacklist() : CHeadPacket(CM_ADD_BLACKLIST) { m_nBodySize = 1; }
};

class CSCDelBlacklist : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    int      m_nPlayerSeq;       // +0x0d  (echoed deleted seq)
    CSCDelBlacklist() : CHeadPacket(CM_DEL_BLACKLIST) { m_nBodySize = 5; }
};

// =============================================================================
// WEEKLY RECORD / RANKING  (PacketWeeklyRecord @08080ed6 /
//   PacketWeeklyRanking @08080f3c). Both are byte-identical 69-byte bodies; filled
//   by CSql::GetRecordField_Weekly @080acde6 / GetRankingField_Weekly @080ad2d8.
// =============================================================================
class CSCWeeklyRecord : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    int      m_nPlayerSeq;       // +0x0d
    int      m_nRecordA[8];      // +0x11  (32)
    int      m_nRecordB[8];      // +0x31  (32) -> body 0x45
    CSCWeeklyRecord() : CHeadPacket(CM_WEEKLY_RECORD) { m_nBodySize = 0x45; }
};

class CSCWeeklyRanking : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    int      m_nPlayerSeq;       // +0x0d
    int      m_nRankingA[8];     // +0x11  (32)
    int      m_nRankingB[8];     // +0x31  (32) -> body 0x45
    CSCWeeklyRanking() : CHeadPacket(CM_WEEKLY_RANKING) { m_nBodySize = 0x45; }
};

// =============================================================================
// ADMIN / GM TOOLS  (PacketOperationTool @0807e3be / PacketRoomTool @0807eab2)
// =============================================================================
// OperationTool: the body is a sub-op dispatch. m_nSubOp selects the action
// (1 broadcast, 2 notice-all, 4 kick-room, 5 kick-user, 6 force-master,
//  7 force-out, 8 disconnect+save, 9 shutup, 10 disconnect). m_nArg is a room seq
// or shutup state; the 121-byte string region holds a name or broadcast text.
class CCSOperationTool : public CHeadPacket
{ public:
    int      m_nSubOp;           // +0x0c
    int      m_nArg;             // +0x10  (room seq / shutup state)
    int      m_nReserved14;      // +0x14
    char     m_sText[0x79];      // +0x18  (121: target name or broadcast text)
};

// Reply is a single GM ack byte (sub-op failures send 0xFF via SendPlayer(...,-1)).
class CSCOperationTool : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    CSCOperationTool() : CHeadPacket(CM_OPERATION_TOOL) { m_nBodySize = 1; }
};

// RoomTool reply: a GM-only room snapshot + up to 18 GM athlete rows.
// CGMRoomInfo is 89 bytes (room snapshot); CGMAthleteInfo is 56 bytes per player.
// These GM structs are distinct from the public CRoomInfo/CAthleteInfo; their
// internal field maps are owned by the room module (sizes pinned here for framing).
// CGMRoomInfo: 89-byte SERVER->GM room snapshot. Layout verified field-by-field
// against CRoom::GetRoomInfo @08099026 (XKICK_Game1); unaligned ints rely on the
// surrounding #pragma pack(1).
class CGMRoomInfo
{ public:
    char  m_nState;          // 0x00
    char  m_nMode;           // 0x01
    char  m_nCource;         // 0x02
    int   m_nRoomSeq;        // 0x03
    int   m_nParentSeq;      // 0x07
    char  m_nRoomJangTeam;   // 0x0b
    int   m_nHomeJangSeq;    // 0x0c
    int   m_nAwayJangSeq;    // 0x10
    char  m_sTitle[47];      // 0x14
    char  m_sPass[5];        // 0x43
    int   m_nQuestCode;      // 0x48
    char  m_nGroundCode;     // 0x4c
    char  m_nBallCode;       // 0x4d
    char  m_nTimeCode;       // 0x4e
    char  m_nWeatherCode;    // 0x4f
    char  m_nAttackCode;     // 0x50
    char  m_nScaleCode;      // 0x51
    char  m_nAICode;         // 0x52
    short m_nPointCode;      // 0x53
    char  m_nStartLevel;     // 0x55
    char  m_nEndLevel;       // 0x56
    char  m_nAttackTeam;     // 0x57
    char  m_nMaxCount;       // 0x58
};
// CGMAthleteInfo: 56-byte SERVER->GM athlete-list row. Layout verified field-by-field
// against CPlayer::GetAthleteInfo @08088db0 (XKICK_Game1). All 56 bytes are written;
// there are no gaps/reserved bytes. m_nNetSpeed/m_nPerformance are unaligned ints
// (rely on the surrounding #pragma pack(push,1)). m_nPerformance is a sign-extended
// short stored as a 4-byte int. m_cAddress is the 24-byte m_cUDPAddress blob.
class CGMAthleteInfo
{ public:
    int  m_nPlayerSeq;     // +0x00 (4)  this->m_nPlayerSeq
    char m_sName[15];      // +0x04 (15) snprintf(name,15,this->m_sName)
    char m_nPosition;      // +0x13 (1)  this+0x10
    char m_nTeam;          // +0x14 (1)  this+0x13
    char m_nLevel;         // +0x15 (1)  this->m_cLevel.m_nLevel (this+0xC4)
    int  m_nNetSpeed;      // +0x16 (4)  this+0x51C
    int  m_nPerformance;   // +0x1a (4)  movsx int <- short this+0x51A
    char m_nRelay;         // +0x1e (1)  this+0x510
    char m_nOperation;     // +0x1f (1)  this+0x511
    char m_cAddress[24];   // +0x20 (24) this->m_cUDPAddress (this+0xEC)
};                                                       // CPlayer::GetAthleteInfo target
static_assert(sizeof(CGMAthleteInfo) == 56,                     "CGMAthleteInfo must be 56 bytes");
static_assert(offsetof(CGMAthleteInfo, m_nPlayerSeq)   == 0x00, "CGMAthleteInfo.m_nPlayerSeq @0x00");
static_assert(offsetof(CGMAthleteInfo, m_sName)        == 0x04, "CGMAthleteInfo.m_sName @0x04");
static_assert(offsetof(CGMAthleteInfo, m_nPosition)    == 0x13, "CGMAthleteInfo.m_nPosition @0x13");
static_assert(offsetof(CGMAthleteInfo, m_nTeam)        == 0x14, "CGMAthleteInfo.m_nTeam @0x14");
static_assert(offsetof(CGMAthleteInfo, m_nLevel)       == 0x15, "CGMAthleteInfo.m_nLevel @0x15");
static_assert(offsetof(CGMAthleteInfo, m_nNetSpeed)    == 0x16, "CGMAthleteInfo.m_nNetSpeed @0x16");
static_assert(offsetof(CGMAthleteInfo, m_nPerformance) == 0x1a, "CGMAthleteInfo.m_nPerformance @0x1a");
static_assert(offsetof(CGMAthleteInfo, m_nRelay)       == 0x1e, "CGMAthleteInfo.m_nRelay @0x1e");
static_assert(offsetof(CGMAthleteInfo, m_nOperation)   == 0x1f, "CGMAthleteInfo.m_nOperation @0x1f");
static_assert(offsetof(CGMAthleteInfo, m_cAddress)     == 0x20, "CGMAthleteInfo.m_cAddress @0x20");

class CSCRoomTool : public CHeadPacket
{ public:
    char           m_nResponse;          // +0x0c
    CGMRoomInfo    m_cRoomInfo;          // +0x0d  (89)
    char           m_nCount;            // +0x66
    CGMAthleteInfo m_cAthlete[18];      // +0x67  (18 * 56 = 1008)
    CSCRoomTool() : CHeadPacket(CM_ROOM_TOOL) { m_nBodySize = 1 + 89 + 1 + 18 * 56; }
};

// =============================================================================
// UDP  (REUSE Protocol.h CCSUDPPunching{int m_nPlayerSeq} and CSCUDPPing.)
// =============================================================================

// =============================================================================
// STS  (REUSE Protocol.h CSCStsLogin, CCSStsDrawforce, CSCUpdateWeather,
//        CSCUpdateNotice, CSCSendBroadcast. SendBroadcast request body is
//        {char nKind@+0xc; char sMessage[121]@+0xd} -- the broadcast text.)
// =============================================================================
class CCSSendBroadcast : public CHeadPacket
{ public:
    char     m_nKind;            // +0x0c
    char     m_sMessage[0x79];   // +0x0d  (121)
};

#pragma pack(pop)

// =============================================================================
// SHARED RUNTIME-FILLED WIRE TYPES other modules read (already in Protocol.h):
//   CSCPlayerInfo + CPlayerInfo  (player info; PacketPlayerInfo)
//   CRoomInfo / CSCRoomInfo      (room module)
//   CAthleteInfo / CSCAthleteInfo
// CTeamInfo: no public CTeamInfo wire struct exists in the binary's spine; team
// snapshots go out as CSCTeamInfo defined by the team module (Wave 3). Listed in
// the TODO block at the top.
// =============================================================================

#endif // _GAME_PROTOCOL_H_
