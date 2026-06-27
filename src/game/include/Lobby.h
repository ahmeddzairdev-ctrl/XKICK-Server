// =============================================================================
// XKICK_Game - Lobby.h
//
// IMPORTANT (reverse-engineering finding): XKICK_Game has NO distinct `CLobby`
// class. The lobby is implemented AS a CRoom: the global g_RoomPool acts as the
// room table, and the lobby is the room whose number/no == 0 (room->m_nNo == 0,
// tested as `*(short*)(room+4) == 0` in CPlayer::ExitPlayer @080893d0). A player
// sitting "in the lobby" has CPlayer::m_pRoom -> that lobby CRoom and is tracked
// in CRoom::m_vLobbyList (the std::vector<int> at room+0x230); see
// CRoom::InsertPlayerLobby @08099434 / DeletePlayerLobby @08099c78.
//
// So this header does not declare a class; it declares the lobby/room-table free
// functions (room.cpp / lobby.cpp) and the global containers, mirroring how the
// binary actually organizes lobby state. (Kept as a separate header per the
// deliverable list; CRoom itself lives in Room.h.)
//
// Recovered symbols:
//   GetLobbyList       @0809da12   void GetLobbyList(int, CSCLobbyList*)
//   GetRoomList        @0809d2ac   void GetRoomList(int,int,int, CSCRoomList*)
//   GetTotalRoomList   @0809d146   GetRoomCount @0809d0f6  GetTotalRoomList @0809d146
//   CreateRoom         @0809ccfe   int  CreateRoom(int, CCSCreateRoom*)
//   GetRoomPointer     @0809ccb4   CRoom* GetRoomPointer(int nNo)
//   ChoiceQuickRoom    @0809dc64   ChoiceQuickTeam @080a055e
// =============================================================================
#ifndef _GAME_LOBBY_H_
#define _GAME_LOBBY_H_

#include <vector>
#include "Struct.h"
#include "Room.h"        // CRoom (the lobby is a CRoom)

class CPlayer;
class CTeam;
class CSCLobbyList;
class CSCRoomList;
class CCSCreateRoom;

// ---- lobby / room-table free functions (room.cpp) ----
CRoom* GetRoomPointer(int nNo);                               // @0809ccb4
int    GetRoomCount();                                       // @0809d0f6
void   GetRoomList(int nLobby, int nKind, int nPage, CSCRoomList* pOut); // @0809d2ac
void   GetTotalRoomList(int nLobby, int nKind);              // @0809d146
void   GetLobbyList(int nLobby, CSCLobbyList* pOut);         // @0809da12
CRoom* CreateRoom(int nPlayerSeq, CCSCreateRoom* pPacket);   // @0809ccfe (returns the new room)
void   ChoiceQuickRoom(CPlayer* pPlayer);                    // @0809dc64
int    ChoiceQuickTeam(CPlayer* pPlayer);                    // @080a055e

// ---- room/all broadcast + player-lookup helpers (room.cpp) ----
void     SendRoom(CRoom* pRoom, CHeadPacket* pPacket, int nResult);            // @0808242e
void     SendTeam(CRoom* pRoom, int nTeam, CHeadPacket* pPacket, int nResult); // @08082708
void     SendAll (CHeadPacket* pPacket, int nResult);                          // @08082b6e
CPlayer* GetPlayerPointer(int nCharacterSeq);                                  // @0809126e
CPlayer* GetPlayerPointerPc(const char* sName);                               // @08091374
int      IsPlayerConnect(int nSeq);                                           // @080918fc

// ---- global room/team pools (the "lobby table"); see Main.h for addresses ----
//   g_RoomPool : CRoom[]  — index 0..N; lobbies are the rooms with m_nNo == 0.
//   g_TeamPool : CTeam[]  — clan-match team rooms.
// (Declared as extern in Main.h, not here, to avoid duplicate definitions.)

#endif // _GAME_LOBBY_H_
