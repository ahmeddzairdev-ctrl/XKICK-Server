// =============================================================================
// globals.cpp - definitions of the XKICK_Game global singletons + fixed pools
// that Global.h declares extern. The binary placed these at fixed .bss addresses
// (see Global.h); here they are ordinary file-scope globals. Construction within
// this TU is top-to-bottom; nothing here touches another global at ctor time.
// =============================================================================
#include "Main.h"
#include "Sql.h"        // CSql      (full type needed to define g_Sql)
#include "GameLoad.h"   // CLoad     (full type needed to define g_Load)
#include "Net.h"        // CTCPSocket/CSTSSocket/CUDPSocket/CHTTPSocket

CInit        g_Init;
CConfig      g_Config;
CSql         g_Sql;
CLoad        g_Load;
CThread      g_Thread;
CSetting     g_Setting;        // default setting template (seeded by CSql::InitServer)
CProcess     g_Process;        // single dispatch hub (TCP + UDP + STS maps)
CTCPSocket   g_TCPSocket;
CSTSSocket   g_STSSocket;
CUDPSocket   g_UDPSocket;
CHTTPSocket  g_HTTPSocket;

CPlayer g_PlayerPool[MAX_PLAYER_POOL];   // 1000
CRoom   g_RoomPool[MAX_ROOM_POOL];       // 200 (room 0..; lobbies are rooms)
CTeam   g_TeamPool[MAX_TEAM_POOL];       // 400

VectorPlayerList g_PlayerList;
MapPlayerHash    g_PlayerHash;

std::map<int, CWeather*> g_Weather;   // tb_game_weather schedule (LoadWeatherTable)

CRandom g_Random;                     // global RNG (mission/attack/robot rolls)
