// =============================================================================
// XKICK_Game - GameProtocolRoom.h
// The room / lobby / team-display / in-game-lifecycle #pragma pack(1) WIRE structs
// that are NOT already provided by common Protocol.h or GameProtocol.h. Each layout
// is derived byte-exact from the handler that builds it in XKICK_Game1 (the CSC*
// ctor pins cmd id + body size; the handler pins field offsets/sizes).
//
// REUSE NOTE (do not redefine): the vast majority of the room/lobby/lifecycle wire
// structs ALREADY live in common Protocol.h, byte-exact to this binary, and are used
// as-is by room.cpp / lobby.cpp:
//   CRoomInfo/CSCRoomInfo/CCSRoomInfo, CRoomData/CSCRoomList/CCSRoomList,
//   CLobbyData/CSCLobbyList/CCSLobbyList, CCSCreateRoom/CSCCreateRoom,
//   CCSSetRoom/CSCSetRoom, CCSChoiceRoom/CSCChoiceRoom, CCSQuickRoom/CSCQuickRoom,
//   CCSLeaveRoom/CSCLeaveRoom, CCSChangeParent/CSCChangeParent,
//   CCSChangeJang/CSCChangeJang, CAthleteInfo/CSCAthleteInfo/CSCAthleteEnd,
//   CRobotInfo/CSCRobotInfo/CSCRobotEnd, CCSChangeGround/CSCChangeGround,
//   CCSChangeBall/CSCChangeBall, CCSForceOut/CSCForceOut,
//   CCSInvitePlayer/CSCInvitePlayer, CCSGameReady/CSCGameReady,
//   CCSGameStart/CSCGameStart, CCSGameCount/CSCGameCount, CCSGameLoad/CSCGameLoad,
//   CCSGamePlay/CSCGamePlay, CCSGameResult/CSCGameResult, CCSGameEnd/CSCGameEnd,
//   CCSChangeTeam/CSCChangeTeam, CCSChangePosition/CSCChangePosition,
//   CCSChangeMent/CSCChangeMent, CCSGrowupCharacter/CSCGrowupCharacter,
//   CCSLevelUp (the request) and the shared CReserveSeat/CTeamSeat (Struct.h).
//
// This header ADDS only the Game forms the binary defines that Protocol.h never had:
//   CSCChangeWeather  (0x905, body 5)   - weather option echo
//   CSCCardbotInfo    (0x907, body 0x1a)- per-cardbot row sent on room join
//   CSCCardbotEnd     (0x908, body 1)
//   CSCAutopilotMode  (0x968, body 6)
//   CSCGoalinTcp      (0x96b, body 0x24)- wraps a 35-byte CPPGoalIn relay blob
//   CSCSwitchValue    (0x96c, body 0xa)
//   CSCSynchPlayer    (0x96a, body 1)
//   CSCLevelUpGame    (0x967, body 0x3a)- WIDER than Protocol.h CSCLevelUp: it carries
//                                         a leading int playerSeq and a 12-byte level
//                                         block (player+0xc4/0xc8/0xcc).
// =============================================================================
#ifndef _GAME_PROTOCOL_ROOM_H_
#define _GAME_PROTOCOL_ROOM_H_

#include "Struct.h"     // CHeadPacket, CMoney, CFaculty, CReserveSeat
#include "Command.h"    // CM_* ids (CM_CHANGE_WEATHER, CM_CARDBOT_INFO/END, ...)

// Command ids the Game binary uses that Command.h may not name explicitly.
#ifndef CM_SYNCH_PLAYER
#define CM_SYNCH_PLAYER     0x096A
#endif
#ifndef CM_LEVEL_UP
#define CM_LEVEL_UP         0x0967
#endif

#pragma pack(push,1)

// =============================================================================
// CHANGE WEATHER  (PacketChangeWeather @080794d6)
// Parallels CSCChangeGround/CSCChangeBall: request carries the new weather code,
// reply echoes it. Room stores it as a single byte @room+0x55, widened to int here.
// =============================================================================
class CCSChangeWeather : public CHeadPacket
{ public:
    int      m_nWeatherCode;     // +0x0c
};

class CSCChangeWeather : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    int      m_nWeatherCode;     // +0x0d
    CSCChangeWeather() : CHeadPacket(CM_CHANGE_WEATHER) { m_nBodySize = 5; }
};

// =============================================================================
// CARDBOT INFO / END  (PacketCardbotInfo @080800c0)
// On entering a room each player's enabled "cardbot" entries are streamed: the
// existing players' cardbots to the joiner (SendPlayer) then the joiner's cardbots
// to the room (SendRoom), terminated by CSCCardbotEnd. The 23-byte m_cCard blob is
// copied straight from the runtime CCard object (card+0x00..0x17).
// =============================================================================
class CSCCardbotInfo : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    char     m_nLobbyState;      // +0x0d  (owner player lobbyState @0x13)
    char     m_nState;           // +0x0e  (card per-entry enabled byte: card[entry+9])
    char     m_cCard[23];        // +0x0f  (raw CCard front block, 0x17 bytes)
    CSCCardbotInfo() : CHeadPacket(CM_CARDBOT_INFO) { m_nBodySize = 0x1a; }
};

class CSCCardbotEnd : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    CSCCardbotEnd() : CHeadPacket(CM_CARDBOT_END) { m_nBodySize = 1; }
};

// =============================================================================
// AUTOPILOT MODE  (PacketAutopilotMode @0807f8c8)
// Toggles the player's auto-pilot; broadcast to the room so peers can mirror it.
// =============================================================================
class CSCAutopilotMode : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    char     m_nMode;            // +0x0d  (request byte echoed)
    int      m_nPlayerSeq;       // +0x0e
    CSCAutopilotMode() : CHeadPacket(CM_AUTOPILOT_MODE) { m_nBodySize = 6; }
};

// =============================================================================
// GOAL-IN (TCP)  (PacketGoalinTcp @08080cba)
// A goal event relayed over TCP. The body is a 35-byte CPPGoalIn blob copied
// verbatim from the request (the client's authoritative goal snapshot).
// =============================================================================
class CPPGoalIn : public CHeadPacket
{ public:
    char     m_data[23];         // 35 - 12(header) = 23 payload bytes
};

class CSCGoalinTcp : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    char     m_cGoalin[35];      // +0x0d  (raw CPPGoalIn, 0x23 bytes)
    CSCGoalinTcp() : CHeadPacket(CM_GOALIN_TCP) { m_nBodySize = 0x24; }
};

// =============================================================================
// SWITCH VALUE  (PacketSwitchValue @08080d1e)
// Generic in-match "switch" toggle (e.g. a stadium switch); broadcast to the room.
// =============================================================================
class CSCSwitchValue : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    char     m_nSwitch;          // +0x0d  (request byte)
    int      m_nValue;           // +0x0e  (request int)
    int      m_nPlayerSeq;       // +0x12
    CSCSwitchValue() : CHeadPacket(CM_SWITCH_VALUE) { m_nBodySize = 0xa; }
};

// =============================================================================
// SYNCH PLAYER  (PacketSynchPlayer @0807ff94)
// Per-player "I'm synchronized" ack. When the whole room is synched the server
// broadcasts this 1-byte packet and resets the synch flags.
// =============================================================================
class CSCSynchPlayer : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    CSCSynchPlayer() : CHeadPacket(CM_SYNCH_PLAYER) { m_nBodySize = 1; }
};

// =============================================================================
// LEVEL UP  (PacketLevelUp @0807ab32)
// Game form of the level-up broadcast. WIDER than Protocol.h CSCLevelUp: the binary
// (body 0x3a) prefixes the character seq and ships a 12-byte level block taken
// directly from player+0xc4/0xc8/0xcc (level/exp/faculty+skill), then the 25-byte
// base-faculty block from player+0x104.
//   resp(1) + seq(4) + CMoney(16) + level(12) + CFaculty(25) = 58 = 0x3a.
// =============================================================================
class CSCLevelUpGame : public CHeadPacket
{ public:
    char     m_nResponse;        // +0x0c
    int      m_nPlayerSeq;       // +0x0d  (player+0xc)
    CMoney   m_cMoney;           // +0x11  (player+0xb4, 16 bytes)
    int      m_nLevelBlock[3];   // +0x21  (player+0xc4/0xc8/0xcc, 12 bytes)
    CFaculty m_cBaseFaculty;     // +0x2d  (player+0x104, 25 bytes)
    CSCLevelUpGame() : CHeadPacket(CM_LEVEL_UP) { m_nBodySize = 0x3a; }
};

#pragma pack(pop)

#endif // _GAME_PROTOCOL_ROOM_H_
