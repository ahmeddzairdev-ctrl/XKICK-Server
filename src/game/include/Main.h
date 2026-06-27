// =============================================================================
// XKICK_Game - Main.h  (umbrella header)
// Reconstructed from certify/XKICK_Game1 (Aug 2010, unstripped) via Ghidra.
//
// Architecture (from main() @080ba0c4, identical order to Certify):
//   InitLogo -> InitSignal -> InitMemory -> InitConfig -> InitLog ->
//   CSql::InitServer -> CLoad::InitLoad -> CThread::InitThread -> TCPThread()
//
// Game vs Certify differences (see Global.h / GameProcess.h):
//   * CPlayer (rich character session) instead of CMember.
//   * A THIRD network path: UDP (P2P confirm/relay) alongside client-TCP + STS.
//   * CSecure rolling cipher is ACTIVE here (per-CPlayer @0x530); Certify used XOR.
//   * A single CProcess owns three dispatch maps (TCP/UDP/STS).
//   * Domain subsystems: item/skill/ceremony/training/quest/card + room/team/lobby.
//
// PORTABILITY (per PLAN.md mandate): only #pragma pack(1) WIRE structs (CHeadPacket
// + Protocol.h packets + embedded wire records) must stay byte-identical. Runtime
// classes (CPlayer/CRoom/CTeam/CProcess) are internal and only self-consistent.
// All Asio/OS code is confined to connect.cpp; shared infra comes from kicks_common.
// =============================================================================
#ifndef _GAME_MAIN_H_
#define _GAME_MAIN_H_

#include <cstdint>
#include <cstring>
#include <map>
#include <vector>
#include <string>

// ---- shared/common (wire format + infra contracts) ----
#include "Define.h"
#include "Database.h"    // CDatabase (MySQL Connector/C, std::mutex pool) -> Include.h
#include "Struct.h"      // CSetting/CMoney/CItem/CSale/... wire structs + CHeadPacket
#include "Protocol.h"    // CCS*/CSC* packet layouts (+ game additions, Phase 5e)
#include "LogManager.h"
#include "Secure.h"      // CSecure (rolling cipher, ACTIVE in Game)
#include "CTable.h"      // portable CSV table API (Table_*)
#include "Que.h"         // CQue / CBufferPool (shared infra)
#include "Util.h"        // Tokener / IPTokener / ExitMessage (shared infra)
#include "md5.h"         // password hashing (shared infra)

// ---- game classes ----
#include "Command.h"     // 138 CM_* protocol ids
#include "GameItem.h"    // 80-byte runtime CItem (server-specific; not in Struct.h)
#include "Global.h"      // singletons + pools + typedefs (pulls in Config/Init/Thread/
                         //   Player/Room/ServerNode/GameProcess)
#include "Domain.h"      // CSkill/CTraining/CCeremony/CCard/CQuestInfo/CBlacklistInfo
#include "Sql.h"         // CSql (full type; g_Sql used across handlers + main)
#include "GameLoad.h"    // CLoad (full type; g_Load)
#include "Net.h"         // CTCPSocket/CUDPSocket/CSTSSocket + Net_* helpers
#include "GameProtocol.h"     // spine wire structs (CSCPlayerInfo/CRoomInfo/CTeamInfo + ...)
#include "GameProtocolItem.h" // item/card/mix/shop wire structs
#include "GameProtocolRoom.h" // room/lobby/team-display/game-lifecycle wire structs
#include "GameProtocolEquip.h"// skill/ceremony/training shop wire structs (CSCSkillSlot)
#include "GameProtocolTeam.h" // clan-match team wire structs (CSCTeamInfo/...)
#include "packet.h"      // all 138 handler decls + SendPlayer/SendServer
#include "Lobby.h"       // lobby == CRoom helpers (free functions)

// ---- command-name helpers (util.cpp, Game command table) ----
void GetCommandString(int nCommand, char* strOut);   // id -> name (debug/log)
int  IsCommandString(int nCommand);                  // 1 unless CM_TCP_PING

// ---- packet send helpers (packet.cpp) ----
// SendPlayer encrypts via the player's CSecure then writes the framed packet.
void SendPlayer(CPlayer* pPlayer, CHeadPacket* pPacket, int nResult);   // @080822c2
void SendServer(CServer* pServer, CHeadPacket* pPacket);

// ---- entry-point thread loops (connect.cpp / main.cpp) ----
void  TCPThread();        // main accept/event loop (Asio io_context.run)
void* STSThread(void*);   // server-to-server loop
void* UDPThread(void*);   // UDP P2P receive loop

#endif // _GAME_MAIN_H_
