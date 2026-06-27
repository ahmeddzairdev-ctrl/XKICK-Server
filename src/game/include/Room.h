// =============================================================================
// XKICK_Game - Room.h
// class CRoom : a game room (also doubles as the lobby container — g_RoomPool[0]
//               is the lobby; see Lobby.h). Holds the 6 player slots per team,
//               room options, mission/synch state, and the lobby player-id lists.
// class CTeam : a "team" (clan-match) room; a lighter sibling of CRoom.
//
// Layouts recovered from Ghidra (XKICK_Game1):
//   CRoom::C1/C2 (ctor)   @08097f9a / @08097bf4   (vectors + a couple scalars)
//   CRoom::InitRoom       @080985a4               (per-slot arrays, x6)
//   CRoom::GetRoomInfo    @08098e40               (scalar field map -> CRoomInfo)
//   CRoom::InsertPlayerLobby @08099434            (lobby id-list push_back @0x230)
//   CTeam::C1/C2 (ctor)   @0809f0bc / @0809f054
//   CTeam::InitTeam       @0809f15c
//   CTeam::GetTeamInfo    @0809f3a2               (scalar field map -> CTeamInfo)
//
// Reuses common "Struct.h" (CReserveSeat, CHeadPacket) and forward-declares CPlayer.
// =============================================================================
#ifndef _GAME_ROOM_H_
#define _GAME_ROOM_H_

#include <vector>
#include <cstddef>      // offsetof
#include "Struct.h"     // CReserveSeat, CHeadPacket, CTeamSeat
#include "Define.h"     // TEAM_SIZE, ROOM_NAME_SIZE ...

class CPlayer;
class CTeam;
class  CRoomInfo;
struct CTeamInfo;
class  CCSCreateRoom;
class  CCSChangePosition;
class  CGMAthleteInfo;

// ============================== CRoom ========================================
// Byte-exact with the original binary CRoom (size 0x25c = 604, 43 members). Every
// field is a named member at its exact offset; alignment holes are explicit
// _padNNN[] members under pack(1).
#pragma pack(push, 1)
class CRoom
{
public:
    char   m_nCource;            // 0x000
    char   m_nState;             // 0x001  ROOM_STATE_*
    char   m_nMode;              // 0x002
    char   _pad003[1];           // 0x003
    short  m_nRoomSeq;           // 0x004  room number
    char   _pad006[2];           // 0x006
    int    m_nParentSeq;         // 0x008
    char   m_nRoomJangTeam;      // 0x00c
    char   _pad00d[3];           // 0x00d
    int    m_nHomeJangSeq;       // 0x010
    int    m_nAwayJangSeq;       // 0x014
    char   m_sTitle[47];         // 0x018  room title
    char   m_sPass[5];           // 0x047  room password
    int    m_nQuestCode;         // 0x04c
    short  m_nGroundCode;        // 0x050
    short  m_nBallCode;          // 0x052
    char   m_nTimeCode;          // 0x054
    char   m_nWeatherCode;       // 0x055
    char   m_nAttackCode;        // 0x056
    char   m_nScaleCode;         // 0x057
    char   m_nAICode;            // 0x058
    char   _pad059[1];           // 0x059
    short  m_nPointCode;         // 0x05a
    char   m_nAreaCode;          // 0x05c
    char   m_nStartLevel;        // 0x05d
    char   m_nEndLevel;          // 0x05e
    char   m_nAttackTeam;        // 0x05f
    char   m_nMaxCount;          // 0x060
    char   m_nCheckClub;         // 0x061
    char   m_nCheckTime;         // 0x062
    char   m_nCheckWeather;      // 0x063
    char   m_nCheckView;         // 0x064
    char   m_nCheckViewChat;     // 0x065
    char   _pad066[2];           // 0x066
    int    m_tGameStartTime;     // 0x068  (time_t, 4 bytes)
    int    m_nMission;           // 0x06c
    CMission m_cMission;         // 0x070  (16)
    // ---- per-team reserve-seat blocks (132 each) ----
    CReserveSeat m_cHomeSeat;    // 0x080
    CReserveSeat m_cAwaySeat;    // 0x104
    CReserveSeat m_cViewSeat;    // 0x188
    // ---- room player-id lists ----
    std::vector<int>               m_vHomeList;  // 0x20c
    std::vector<int>               m_vAwayList;  // 0x218
    std::vector<int>               m_vViewList;  // 0x224
    std::vector<int>               m_vStayList;  // 0x230  (InsertPlayerLobby push_back here)
    std::vector<std::vector<int>*> m_vAthleteList; // 0x23c
    std::vector<std::vector<int>*> m_vTotalList;    // 0x248
    CTeam* m_pHomeTeam;          // 0x254
    CTeam* m_pAwayTeam;          // 0x258
    // total size = 0x25c bytes

    CRoom();
    ~CRoom();

    void InitRoom();                         // @080985a4
    void GetRoomInfo(CRoomInfo* pOut);       // @08098e40
    void GetRoomInfo(class CGMRoomInfo* pOut); // @08099026 (GM snapshot overload)
    int  InsertPlayerLobby(CPlayer* pPlayer);// @08099434
    void DeletePlayerLobby(CPlayer* pPlayer);// @08099c78
    int  InsertPlayerRoom(CPlayer* pPlayer, int nChange, char* pPass); // @08099484
    int  DeletePlayerRoom(CPlayer* pPlayer); // @08099d3c
    int  InsertViewerRoom(CPlayer* pPlayer, char* pPass);             // @08099b42
    void InsertCardBot(CPlayer* pPlayer, CReserveSeat* pSeat);        // @0809986a
    void DeleteCardBot(CPlayer* pPlayer, CReserveSeat* pSeat);        // @080999e2
    int  CreateMission();                    // @080987fc
    int  SetRoom(class CCSSetRoom* pPacket);// @0809b402
    void DeleteRoom();                       // @0809b23e
    char IsRoomJang(int nSeq);               // @0809c7e6
    char IsTeamJang(int nSeq);               // @0809c82a
    void SetRoomJang();                      // @0809beb4
    void SetTeamJang(CPlayer* pPlayer);      // @0809bad4
    short GetMinLevel();                     // @08098bb4
    short GetMaxLevel();                     // @08098cfa
    int  GetMinPoint();                      // @0809b29e
    int  GetTeamAmount(int nTeam);           // @08098a68
    int  GetTotalAmount();                   // @08098b50
    int  GetAthleteAmount();                 // @08098b08
    int  GetRoomCource();                    // @0809cbee
    void SetRoomCource(int nCource);         // @0809cbe0
    void SetAttackTeam();                    // @0809876c
    int  GetParent();                        // @0809b63e
    int  SetParent();                        // @0809b5f0
    int  GetSpecialParent();                 // @0809b92a
    int  GetRoomSpeed();                     // @0809ba7c
    CPlayer* GetPlayer(int nSeq);            // @0809cbfa  (lobby id-list lookup)
    void GetRobotInfo(class CRobotInfo* pOut, int nTeam, int nSeat, int nLevel, int nCostume); // @08099170
    int  GetOptimizedPosition(int nLevel, int nPos);                 // @0809928a
    char IsExistInRoom(CPlayer* pPlayer);                            // @0809a0b6
    char IsTeamAndSeatInsert(CPlayer* pPlayer, CTeamSeat* pOut);     // @0809a1f8
    char IsTeamAndSeatChange(CPlayer* pPlayer, int nTeam, CTeamSeat* pOut); // @0809a79e
    void GetNormalizeLevel(int* pHomeOut, int* pAwayOut);           // @0809c864
    char IsGameReadyFlag();                  // @0809c292
    char IsGameStartFlag();                  // @0809c3ce
    void InitGameReadyFlag();                // @0809bf1a
    void InitGameStartFlag();                // @0809c042
    void InitGameRelayFlag();                // @0809c16a
    int  ChangeTeam(CPlayer* pPlayer, int nTeam); // @0809aa8c
    int  ChangePosition(CCSChangePosition* pPacket); // @0809af40
    int  CheckGameStart();                   // @0809c50a
    void InitSynchFlag();                    // @0809c6be
    int  CheckSynchFlag();                   // @0809c582

    // ---- broadcast convenience members (thin wrappers over the free
    //      SendRoom/SendTeam in room.cpp; let the spine call pRoom->Send*()) ----
    void SendRoom(CHeadPacket* pPacket, int nResult);
    void SendTeam(CHeadPacket* pPacket, int nResult, CPlayer* pPlayer);

    // ---- GM/admin helpers (reconstructed in stubs.cpp; inlined in the GM handlers
    //      in XKICK_Game1 - PacketOperationTool @0807e3be / PacketRoomTool @0807eab2) ----
    void KickAll();                                           // kick-room sub-op
    int  GetAthleteList(CGMAthleteInfo* pOut, int nMax);      // RoomTool athlete list (cap 18)
};
#pragma pack(pop)

// ---- byte-exact layout proof (vs original binary CRoom, size 0x25c) ----
static_assert(sizeof(CMission)     == 16,  "CMission size");
static_assert(sizeof(CReserveSeat) == 132, "CReserveSeat size");
static_assert(sizeof(CRoom)        == 0x25c, "CRoom size");
static_assert(offsetof(CRoom, m_nCource)        == 0x000, "CRoom.m_nCource");
static_assert(offsetof(CRoom, m_nState)         == 0x001, "CRoom.m_nState");
static_assert(offsetof(CRoom, m_nMode)          == 0x002, "CRoom.m_nMode");
static_assert(offsetof(CRoom, m_nRoomSeq)       == 0x004, "CRoom.m_nRoomSeq");
static_assert(offsetof(CRoom, m_nParentSeq)     == 0x008, "CRoom.m_nParentSeq");
static_assert(offsetof(CRoom, m_nRoomJangTeam)  == 0x00c, "CRoom.m_nRoomJangTeam");
static_assert(offsetof(CRoom, m_nHomeJangSeq)   == 0x010, "CRoom.m_nHomeJangSeq");
static_assert(offsetof(CRoom, m_nAwayJangSeq)   == 0x014, "CRoom.m_nAwayJangSeq");
static_assert(offsetof(CRoom, m_sTitle)         == 0x018, "CRoom.m_sTitle");
static_assert(offsetof(CRoom, m_sPass)          == 0x047, "CRoom.m_sPass");
static_assert(offsetof(CRoom, m_nQuestCode)     == 0x04c, "CRoom.m_nQuestCode");
static_assert(offsetof(CRoom, m_nGroundCode)    == 0x050, "CRoom.m_nGroundCode");
static_assert(offsetof(CRoom, m_nBallCode)      == 0x052, "CRoom.m_nBallCode");
static_assert(offsetof(CRoom, m_nTimeCode)      == 0x054, "CRoom.m_nTimeCode");
static_assert(offsetof(CRoom, m_nWeatherCode)   == 0x055, "CRoom.m_nWeatherCode");
static_assert(offsetof(CRoom, m_nAttackCode)    == 0x056, "CRoom.m_nAttackCode");
static_assert(offsetof(CRoom, m_nScaleCode)     == 0x057, "CRoom.m_nScaleCode");
static_assert(offsetof(CRoom, m_nAICode)        == 0x058, "CRoom.m_nAICode");
static_assert(offsetof(CRoom, m_nPointCode)     == 0x05a, "CRoom.m_nPointCode");
static_assert(offsetof(CRoom, m_nAreaCode)      == 0x05c, "CRoom.m_nAreaCode");
static_assert(offsetof(CRoom, m_nStartLevel)    == 0x05d, "CRoom.m_nStartLevel");
static_assert(offsetof(CRoom, m_nEndLevel)      == 0x05e, "CRoom.m_nEndLevel");
static_assert(offsetof(CRoom, m_nAttackTeam)    == 0x05f, "CRoom.m_nAttackTeam");
static_assert(offsetof(CRoom, m_nMaxCount)      == 0x060, "CRoom.m_nMaxCount");
static_assert(offsetof(CRoom, m_nCheckClub)     == 0x061, "CRoom.m_nCheckClub");
static_assert(offsetof(CRoom, m_nCheckTime)     == 0x062, "CRoom.m_nCheckTime");
static_assert(offsetof(CRoom, m_nCheckWeather)  == 0x063, "CRoom.m_nCheckWeather");
static_assert(offsetof(CRoom, m_nCheckView)     == 0x064, "CRoom.m_nCheckView");
static_assert(offsetof(CRoom, m_nCheckViewChat) == 0x065, "CRoom.m_nCheckViewChat");
static_assert(offsetof(CRoom, m_tGameStartTime) == 0x068, "CRoom.m_tGameStartTime");
static_assert(offsetof(CRoom, m_nMission)       == 0x06c, "CRoom.m_nMission");
static_assert(offsetof(CRoom, m_cMission)       == 0x070, "CRoom.m_cMission");
static_assert(offsetof(CRoom, m_cHomeSeat)      == 0x080, "CRoom.m_cHomeSeat");
static_assert(offsetof(CRoom, m_cAwaySeat)      == 0x104, "CRoom.m_cAwaySeat");
static_assert(offsetof(CRoom, m_cViewSeat)      == 0x188, "CRoom.m_cViewSeat");
static_assert(offsetof(CRoom, m_vHomeList)      == 0x20c, "CRoom.m_vHomeList");
static_assert(offsetof(CRoom, m_vAwayList)      == 0x218, "CRoom.m_vAwayList");
static_assert(offsetof(CRoom, m_vViewList)      == 0x224, "CRoom.m_vViewList");
static_assert(offsetof(CRoom, m_vStayList)      == 0x230, "CRoom.m_vStayList");
static_assert(offsetof(CRoom, m_vAthleteList)   == 0x23c, "CRoom.m_vAthleteList");
static_assert(offsetof(CRoom, m_vTotalList)     == 0x248, "CRoom.m_vTotalList");
static_assert(offsetof(CRoom, m_pHomeTeam)      == 0x254, "CRoom.m_pHomeTeam");
static_assert(offsetof(CRoom, m_pAwayTeam)      == 0x258, "CRoom.m_pAwayTeam");

// ============================== CTeam ========================================
// A clan-match team-room. Single team block + one player-id vector.
class CTeam
{
public:
    char   m_nState;             // 0x000  (GetTeamInfo[0])
    char   m_nKind;              // 0x001  (GetTeamInfo[1])
    char   m_nCource;            // 0x002  (GetTeamInfo[2])
    char   m_reserved003[3];     // 0x003
    short  m_nNo;                // 0x006  team number (GetTeamInfo, widened)
    int    m_nReserved008;       // 0x008  (GetTeamInfo +8)
    char   m_sName[47];          // 0x00c  team title    (0x2f bytes)
    char   m_sPassword[5];       // 0x03b  team password (5 bytes)
    int    m_nReserved040;       // 0x040  (GetTeamInfo +0x40)
    short  m_nReserved044;       // 0x044
    short  m_nReserved046;       // 0x046
    char   m_nOption[12];        // 0x048  options 0x48..0x53 (bytes + 1 short @0x4e)
    int    m_nReserved054;       // 0x054  (InitTeam = 0)
    char   m_nReserved058;       // 0x058  (InitTeam = 0)
    char   m_reserved059[3];     // 0x059
    // ---- per-slot working arrays (InitTeam loops 6) ----
    char   m_slotName[6];        // 0x05c  per-slot byte (also reserve-seat base 0x5c, 0x84)
    char   m_slotPos[6];         // 0x062
    int    m_slotSeq[6];         // 0x068  per-slot player seq
    char   m_slotFlagA[6];       // 0x080
    char   m_slotFlagB[0x5a];    // 0x086  per-slot block (stride 0xf x6 -> 0x86..0xe0)
    std::vector<int> m_vPlayerList; // 0x0e0  team player-id list (ctor; InitTeam clears)
    // total size ~= 0xec bytes

    CTeam();
    ~CTeam();

    void InitTeam();                          // @0809f15c
    void GetTeamInfo(CTeamInfo* pOut);        // @0809f3a2
    int  InsertPlayerTeam(CPlayer* pPlayer, int nSeat, char* pName); // @0809f4dc
    int  DeletePlayerTeam(CPlayer* pPlayer);  // @0809f6b2 (returns 1 if team emptied)
    int  SetTeam(struct CCSSetTeam* pPacket);          // @0809fc08
    int  ChangePosition(struct CCSTeamPosition* pPacket); // @0809fa46
    void SetTeamJang(CPlayer* pPlayer);
    char IsTeamJang(int nSeq);
    int  GetTeamAmount();
    short GetMinLevel();
    short GetMaxLevel();
    void DeleteTeam();
    void InitTeamReadyFlag();
    int  IsTeamReadyFlag();
    CPlayer* GetPlayer(int nSeat);
    bool IsExistInTeam(CPlayer* pPlayer);                          // @0809f868
    char IsTeamAndSeatInsert(CPlayer* pPlayer, CTeamSeat* pSeat);  // @0809f920
};

#endif // _GAME_ROOM_H_
