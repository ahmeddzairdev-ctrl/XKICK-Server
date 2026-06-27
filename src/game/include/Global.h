// =============================================================================
// XKICK_Game - Global.h
// Global singletons, object pools, and container typedefs. Addresses/sizes are
// the originals (XKICK_Game1) for cross-reference. Pool counts confirmed from
// CInit::InitMemory @080742ac (players 1000, rooms 200, teams 400).
//
// Infra classes that pair with their own .cpp (CSql/CLoad and the socket layer)
// are forward-declared here; their full headers (Sql.h / GameLoad.h / Net.h) are
// included by the TUs that use them. extern object declarations of an incomplete
// type are valid declarations (definitions live in globals.cpp).
// =============================================================================
#ifndef _GAME_GLOBAL_H_
#define _GAME_GLOBAL_H_

#include <vector>
#include <map>

#include "Struct.h"        // CSetting wire record (g_Setting template)
#include "Config.h"
#include "Init.h"
#include "Thread.h"
#include "Player.h"
#include "Room.h"
#include "ServerNode.h"
#include "GameProcess.h"   // CProcess + Map{TCP,UDP,STS}Function

// ---- infra classes defined alongside their .cpp (Phase 5d) ----
class CSql;          // Sql.h        (g_Sql,        5892 bytes)
class CLoad;         // GameLoad.h   (g_Load,       3780 bytes)
class CTCPSocket;    // Net.h        (g_TCPSocket,  ~1028108 bytes: embeds CQue)
class CSTSSocket;    // Net.h        (g_STSSocket,  ~1028108 bytes)
class CUDPSocket;    // Net.h        (g_UDPSocket,  4 bytes: lightweight fd/handle)
class CHTTPSocket;   // Net.h        (g_HTTPSocket, 10004 bytes)
class CRandom;       // Player.h
// tb_game_weather row (LoadWeatherTable). Authoritative layout (== CWeatherData
// in Struct.h): the schedule reader in PacketCurrentWeather uses these offsets.
class CWeather {
public:
    char m_nWeatherType;  // 0x00
    char m_nDate;         // 0x01
    int  m_nWeatherSeq;   // 0x04
    int  m_nStart;        // 0x08
    int  m_nEnd;          // 0x0c
};

// ---- capacity constants (from InitMemory loop bounds) ----
#define MAX_PLAYER_POOL  1000
#define MAX_ROOM_POOL    200
#define MAX_TEAM_POOL    400

// ---- container typedefs ----
typedef std::vector<CPlayer*>   VectorPlayerList;   // g_PlayerList (active players)
typedef std::map<int, CPlayer*> MapPlayerHash;      // g_PlayerHash (characterSeq -> CPlayer*)

// ---- global singletons ----
extern CInit       g_Init;        // 080dedc0
extern CConfig     g_Config;      // 080dee00 (320 bytes)
extern CSql        g_Sql;         // 080dfe20
extern CLoad       g_Load;        // 080def40
extern CThread     g_Thread;      // 080e1524
extern CSetting    g_Setting;     // 082d9d00 (88-byte default setting template)
extern CProcess    g_Process;     // 080e1540 (single dispatch hub, 3 maps)
extern CTCPSocket  g_TCPSocket;   // 080e15a0
extern CSTSSocket  g_STSSocket;   // 081dc5c0
extern CUDPSocket  g_UDPSocket;   // 081dc5ac
extern CHTTPSocket g_HTTPSocket;  // 082d75e0

// ---- object pools ----
extern CPlayer g_PlayerPool[MAX_PLAYER_POOL];  // 0830e600
extern CRoom   g_RoomPool[MAX_ROOM_POOL];      // 082d9d60 (room 0.. ; lobbies are rooms)
extern CTeam   g_TeamPool[MAX_TEAM_POOL];      // 082f7540

// ---- active-object views ----
extern VectorPlayerList g_PlayerList;   // 084596e0
extern MapPlayerHash    g_PlayerHash;   // 084596ec

// ---- weather schedule (CSql::LoadWeatherTable fills this; read by PacketCurrentWeather) ----
extern std::map<int, CWeather*> g_Weather;

#endif // _GAME_GLOBAL_H_
