// =============================================================================
// XKICK_Game - Player.h
// class CPlayer : a connected, in-game character session. The Game-server analog
// of Certify's CMember, but far richer (the full loaded character + inventories).
//
// Layout recovered from Ghidra (XKICK_Game1):
//   CPlayer::C2 (ctor)        @080865dc   (embedded subobjects + total size 0x54c)
//   CPlayer::InitPlayer       @08087426
//   CPlayer::InitPlayerInLobby@080874ee   CPlayer::InitPlayerInTeam @08087544
//   CPlayer::SetPlayer        @08087878   CPlayer::SetPlayerInfo    @08087aba
//   CPlayer::GetTCPSecure     @08087264   (returns this+0x530)
//   CPlayer::ExitPlayer       @080893d0
//   PacketGameLogin           @08074d20   (fills fd/seq/address low offsets)
//   WriteTCP                  @08082e86   (uses this+0x4 as the socket fd)
//   CSql::GetMemberField      @080a97ac   CSql::GetPlayerField  @080a9c10
//   CSql::GetPlayerFields     @080a963e   CSql::GetMoneyField   @080b4fc2
//   CSql::GetLevelField       @080b52c0   CSql::GetFacultyField @080acaa2
//   CSql::GetRecordField      @080acb66
//
// Field names mirror the DB columns read in the CSql::Get*Field methods
// (tb_game_trio member fields; tb_game player_* fields; settings_* for CSetting).
//
// The wire data records (CSetting/CMoney/CItem/CSale/CBuddyInfo/CPlayLog ...) live
// in the SHARED common "Struct.h" and are reused here, NOT redefined. CSecure is
// the shared anti-replay sequencer from common "Secure.h". COption / CRandomShop /
// CRandom are Game-specific helper subobjects; declared (TODO-stubbed) below until
// their own .cpp is reconstructed.
// =============================================================================
#ifndef _GAME_PLAYER_H_
#define _GAME_PLAYER_H_

#include <cstdint>
#include <cstddef>      // offsetof
#include <vector>

#include "Struct.h"     // CSetting, CMoney, CItem, CSale, CBuddyInfo, CPlayLog, CHeadPacket ...
#include "Secure.h"     // CSecure (this+0x530)
#include "Define.h"     // size macros (PLAYER_NAME_SIZE, IP_SIZE, ARRAY_* ...)

// ---- forward declarations (pointers only) ----
class CRoom;
class CTeam;
class CServer;
class CSkill;
class CTraining;
class CCeremony;
class CCard;
class CQuestInfo;
class CBlacklistInfo;
class CItem;
class CFaculty;
class CSCBuddyInfo;
class CSCBlacklistInfo;
class CGMAthleteInfo;

// ---- Game-specific embedded subobjects -------------------------------------
// COption @0x168 : the player's item-option block (CPlayer 0x168..0x2c0 = 344
// bytes). A leading active-option count, then up to 85 collected option codes
// (filled by CPlayer::InitOption / SetItemOption; copied wholesale onto the wire).
class COption
{
public:
    COption();                       // _ZN7COptionC1Ev @0804bbf4 (zeroes m_nCount)
    char m_nCount;                   // 0x00  active option count
    char m_pad[3];                   // 0x01
    int  m_nOption[85];              // 0x04  option codes (0x55) -> 0x158 (344)
};

// ---- forward decls for the random-shop method signatures ----
class CItem;
class CItemInfo;
class COptionTable;
class CSCRandomshopitemList;
class CCSBuyRandomitem;
class CCSEnchantItem;
class CSale;

// CRandom : RNG helper embedded inside CRandomShop. _ZN7CRandomC1Ev.
//   Layout (verified): two ints (seed + running state). InitSeed seeds both;
//   GetFixRandom advances a deterministic srand/rand chain; GetMessRandom/Random
//   reseed from the wall clock each call.
class CRandom
{
public:
    CRandom();
    void InitSeed();                 // _ZN7CRandom8InitSeedEv  (seed from clock)
    void InitSeed(int v);            // _ZN7CRandom8InitSeedEi
    int  Random();                   // _ZN7CRandom6RandomEv    (== GetMessRandom)
    int  GetFixRandom();             // _ZN7CRandom12GetFixRandomEv
    int  GetMessRandom();            // _ZN7CRandom13GetMessRandomEv
    int  m_nSeed;                    // 0x00
    int  m_nState;                   // 0x04
};

// The single global RNG used for mission/attack/robot rolls (data segment).
extern CRandom g_Random;

// CRandomShop @0x408 : occupies CPlayer 0x408..0x420 (= 0x18 bytes).
//   +0x00 CPlayer* m_pPlayer  (back-pointer; ctor 0, InitShop sets it)
//   +0x04 CRandom  m_cRandom
//   +0x0c vector<CItem*> m_vTodayList   (today's generated offers)
class CRandomShop
{
public:
    CRandomShop();                   // _ZN11CRandomShopC2Ev @0804a350
    CPlayer*             m_pPlayer;      // 0x00
    CRandom              m_cRandom;      // 0x04
    std::vector<CItem*>  m_vTodayList;   // 0x0c

    void InitShop(CPlayer* pPlayer);     // _ZN11CRandomShop8InitShopEP7CPlayer @0804a538
    void InitSeed();                     // @0804a564
    int  GetFixRandom();                 // @0804a58a
    int  GetMessRandom();                // @0804a5a4
    void ClearTodayList();               // @0804a5be
    void CreateShop();                   // @0804a68a
    int  CreateItem(CItem*, int, int, int, int); // @0804a7e2
    int  GetItemGrade(int);              // @0804b5da
    char GetItemLimit();                 // @0804b6b8
    int  GetItemPrice(int, int, int);    // @0804b828
    int  GetItemValue(COptionTable*, int, int);  // @0804b752
    int  GetOptionCount(int);            // @0804b862
    CItem* GetShopItem(int);             // @0804b8b4
    void   GetShopItem(CItem*, int, int);// @0804b99e
    int  GetShopIndex(CItemInfo*);       // @0804b9e6
    bool GetTodayBit(int);               // @0804baea
    void SetTodayBit(int, bool);         // @0804bb0c
    bool IsPossibleBuyItemType(int);     // @0804bb48
    int  GetItemList(CSCRandomshopitemList*);     // @0804afee
    int  BuyItem(CCSBuyRandomitem*);     // @0804b1d0
    int  EnchantItem(CCSEnchantItem*);   // @0804aa46
};

// free helpers used by the random-shop item generator (binary _Z* funcs).
int GetItemTypeFromEquip(int);    // _Z20GetItemTypeFromEquipi @08096d10
int GetItemEquipFromType(int);    // _Z20GetItemEquipFromTypei

#pragma pack(push, 1)
class CPlayer
{
public:
    // Byte-exact with the original binary CPlayer (size 0x54c, 67 members). Every
    // field is a named member at its exact offset; alignment holes the binary leaves
    // are explicit _padNNN[] members (no compiler padding under pack(1)).

    // ---- network / identity (low offsets) ----
    char     m_nState;            // 0x000  PLAYER_STATE_EMPTY/USE
    char     m_nMode;             // 0x001
    char     _pad002[2];          // 0x002
    int      m_nFd;               // 0x004  socket fd
    int      m_nMemberSeq;        // 0x008  account seq   (tb_game_trio key)
    int      m_nPlayerSeq;        // 0x00c  character seq (tb_game player key)
    char     m_nPosition;         // 0x010  player_position
    char     m_nCondition;        // 0x011  player_condition
    char     m_nAlias;            // 0x012  player_alias
    char     m_nTeam;             // 0x013  team byte
    char     m_nSeat;             // 0x014  seat byte
    char     _pad015[3];          // 0x015
    CRoom*   m_pRoom;             // 0x018  current room
    CTeam*   m_pTeam;             // 0x01c  current team
    char     m_sName[15];         // 0x020  player_name
    char     m_sMent[45];         // 0x02f  player_ment

    CSetting m_cSetting;          // 0x05c  settings_* (88)
    CMoney   m_cMoney;            // 0x0b4  cash/point/credit/clubpoint (16)
    CLevel   m_cLevel;            // 0x0c4  level/exp/faculty/skill points (12)
    CShape   m_cShape;            // 0x0d0  gender/skin/uniform (3)
    char     _pad0d3[1];          // 0x0d3
    CAddress m_cTCPAddress;       // 0x0d4  TCP ip/port (24)
    CAddress m_cUDPAddress;       // 0x0ec  UDP ip/port (24)

    CFaculty m_cBaseFaculty;      // 0x104  base faculty (25)
    CFaculty m_cRaiseFaculty;     // 0x11d  raised faculty (25)
    CFaculty m_cItemFaculty;      // 0x136  item faculty (25)
    CFaculty m_cTrainingFaculty;  // 0x14f  training faculty (25)
    COption  m_cItemOption;       // 0x168  option block (344)

    CRecord  m_cTotalRecord;      // 0x2c0  lifetime record (76)
    CRecord  m_cQuarterRecord;    // 0x30c  quarter record (76)
    CRecord  m_cCurrentRecord;    // 0x358  current record (76)
    // NOTE: the binary's in-CPlayer CRanking is 25 shorts (50 bytes), NOT the shared
    // 60-byte CRanking in Struct.h (ARRAY_RANKING_SIZE=30). Pinned as short[25].
    short    m_cTotalRanking[25]; // 0x3a4  total ranking (50)
    short    m_cQuarterRanking[25];// 0x3d6 quarter ranking (50)

    CRandomShop m_cRandomShop;    // 0x408  random shop (24)
    CPlayLog    m_cPlayLog;       // 0x420  play-log (20)
    int      m_nEquipWear[17];    // 0x434  equipped-wear item seqs (68)
    int      m_nFreeWear[5];      // 0x478  free-wear (20)
    int      m_nHomeWear[4];      // 0x48c  home-uniform wear (16)
    int      m_nAwayWear[4];      // 0x49c  away-uniform wear (16)

    // ---- per-type loaded lists (heap vectors) ----
    std::vector<CItem*>          m_vItemList;       // 0x4ac
    std::vector<CSkill*>         m_vSkillList;      // 0x4b8
    std::vector<CTraining*>      m_vTrainingList;   // 0x4c4
    std::vector<CCeremony*>      m_vCeremonyList;   // 0x4d0
    std::vector<CCard*>          m_vCardList;       // 0x4dc
    std::vector<CQuestInfo*>     m_vQuestList;      // 0x4e8
    std::vector<CBuddyInfo*>     m_vBuddyList;      // 0x4f4
    std::vector<CBlacklistInfo*> m_vBlackList;      // 0x500

    char     m_nCount;            // 0x50c  trio_count
    char     m_nGhostHost;        // 0x50d  trio_host
    char     m_nCheckTCP;         // 0x50e
    char     m_nCheckUDP;         // 0x50f
    char     m_nRelay;            // 0x510
    char     m_nOperation;        // 0x511  player_operation
    char     _pad512[2];          // 0x512
    int      m_nCardEntry;        // 0x514  player_cardentry
    short    m_nPlayCount;        // 0x518
    short    m_nPerformance;      // 0x51a
    int      m_nNetSpeed;         // 0x51c
    int      m_tTcpPingTime;      // 0x520  (time_t, 4 bytes)
    int      m_tUdpPingTime;      // 0x524  (time_t, 4 bytes)
    bool     m_bIsGameReady;      // 0x528
    bool     m_bIsGameStart;      // 0x529
    bool     m_bIsTeamReady;      // 0x52a
    bool     m_bIsPitIn;          // 0x52b
    bool     m_bPingCheck;        // 0x52c
    bool     m_bSynch;            // 0x52d
    char     _pad52e[2];          // 0x52e

    CSecure  m_hTCPSecure;        // 0x530  anti-replay sequencer (12)

    int      m_nTodaySeed;        // 0x53c
    unsigned int m_nTodayBit;     // 0x540
    int      m_nShutupDate;       // 0x544
    char     m_nAutoPilot;        // 0x548
    char     _pad549[3];          // 0x549  pad to 0x54c
    // total size = 0x54c bytes

    CPlayer();
    ~CPlayer();

    // ---- lifecycle ----
    void InitPlayer();                 // @08087426 reset per-connection working state
    void InitPlayerInLobby();          // @080874ee
    void InitPlayerInTeam(CTeam* pTeam);// @08087544
    void InitPlayerInRoom(CRoom* pRoom);// @0808750e  room-entry reset
    int  SetPlayer();                  // @08087878 load + register in g_PlayerHash
    int  SetPlayerInfo();              // @08087aba load faculty/option/record/ranking
    void ExitPlayer();                 // @080893d0 flush + detach + close socket
    void DeletePlayer();
    void ResetPingTime();

    // ---- accessors ----
    CSecure* GetTCPSecure();           // @08087264 (&m_cSecure)
    int      GetSeq() const;           // m_nCharacterSeq

    // ---- helpers used during load ----
    void InitFaculty(CFaculty* pOut);
    void InitOption(COption* pOut);
    void InitRecord(CRecord* pOut);
    void InitRanking(CRanking* pOut);
    int  InitBaseFaculty();
    int  SetEquipWear();
    int  SetItemOption();
    int  SetTrainingFaculty();
    int  CheckFaculty();
    int  CalcLevelFaculty();           // @08087a60  level -> spare faculty points
    int  CheckLevelUp();               // @0808b190  promote one level if exp >= table
    void SetPlayLog();
    void ChangeTodayShop();
    void DivestSkill(int nSeq);
    int  SpendMoney(class CSale* pSale);       // @08090d54 (charge cash/point per CSale)

    // ---- room / in-game lifecycle accessors (defined in the player module;
    //      declared here so the room module can call them) ----
    int  GetPlayerSeq() const;                 // m_nCharacterSeq
    char GetPlayerTeam();                       // current team byte (lobbyState-derived)
    void GetAthleteInfo(class CAthleteInfo* pOut);   // fill CSCAthleteInfo body
    void GetAthleteInfo(class CGMAthleteInfo* pOut);  // @08088db0 fill GM 56-byte snapshot
    int  GetPlayerInfo(class CPlayerInfo* pOut);     // fill CSCPlayerInfo body (returns trailing-ranking count)

    // ---- player social / faculty / item helpers (reconstructed in stubs.cpp,
    //      verified against XKICK_Game1) ----
    int    CheckPlayerUDP();                         // @player.cpp  (UDP confirm check)
    int    RaiseFaculty(CFaculty* pDelta);           // @08090a3c
    CItem* GetItemPointer(int nItemSeq);             // @08087xxx  (bag lookup)
    int    GetFacultyCount();                        // @0808b310
    bool   IsBlocked(int nSeq);                      // blacklist membership check
    void   GetBuddyInfo(int nPage, CSCBuddyInfo* pOut);          // @080880fe
    void   GetBlacklistInfo(int nPage, CSCBlacklistInfo* pOut);  // @08088488
    int    AddBuddyInfo(int nSeq);                   // @08087c16
    int    DeleteBuddyList(int nSeq);                // @08087f16
    int    AddBlackListInfo(int nSeq);               // @08087d96
    int    DeleteBlackList(int nSeq);                // @0808800a
    void ChangePosition(int nPosition);              // grow/character position change
    void EnableAutoPilot(bool bEnable);
    void JusticeAndPunishment();                     // cardbot-mode leave penalty
    void SetRecord(class CCSGameResult* pResult);    // apply match record
    void SetLevel(class CCSGameResult* pResult);     // apply exp/level
    void SetConsume(class CCSGameResult* pResult);   // apply consumables
};
#pragma pack(pop)

// ---- byte-exact layout proof (vs original binary CPlayer, size 0x54c) ----
static_assert(sizeof(CSetting)    == 88,  "CSetting size");
static_assert(sizeof(CMoney)      == 16,  "CMoney size");
static_assert(sizeof(CLevel)      == 12,  "CLevel size");
static_assert(sizeof(CShape)      == 3,   "CShape size");
static_assert(sizeof(CAddress)    == 24,  "CAddress size");
static_assert(sizeof(CFaculty)    == 25,  "CFaculty size");
static_assert(sizeof(COption)     == 344, "COption size");
static_assert(sizeof(CRecord)     == 76,  "CRecord size");
static_assert(sizeof(CRandomShop) == 24,  "CRandomShop size");
static_assert(sizeof(CPlayLog)    == 20,  "CPlayLog size");
static_assert(sizeof(CSecure)     == 12,  "CSecure size");
static_assert(sizeof(CPlayer)     == 0x54c, "CPlayer size");

static_assert(offsetof(CPlayer, m_nState)          == 0x000, "CPlayer.m_nState");
static_assert(offsetof(CPlayer, m_nMode)           == 0x001, "CPlayer.m_nMode");
static_assert(offsetof(CPlayer, m_nFd)             == 0x004, "CPlayer.m_nFd");
static_assert(offsetof(CPlayer, m_nMemberSeq)      == 0x008, "CPlayer.m_nMemberSeq");
static_assert(offsetof(CPlayer, m_nPlayerSeq)      == 0x00c, "CPlayer.m_nPlayerSeq");
static_assert(offsetof(CPlayer, m_nPosition)       == 0x010, "CPlayer.m_nPosition");
static_assert(offsetof(CPlayer, m_nCondition)      == 0x011, "CPlayer.m_nCondition");
static_assert(offsetof(CPlayer, m_nAlias)          == 0x012, "CPlayer.m_nAlias");
static_assert(offsetof(CPlayer, m_nTeam)           == 0x013, "CPlayer.m_nTeam");
static_assert(offsetof(CPlayer, m_nSeat)           == 0x014, "CPlayer.m_nSeat");
static_assert(offsetof(CPlayer, m_pRoom)           == 0x018, "CPlayer.m_pRoom");
static_assert(offsetof(CPlayer, m_pTeam)           == 0x01c, "CPlayer.m_pTeam");
static_assert(offsetof(CPlayer, m_sName)           == 0x020, "CPlayer.m_sName");
static_assert(offsetof(CPlayer, m_sMent)           == 0x02f, "CPlayer.m_sMent");
static_assert(offsetof(CPlayer, m_cSetting)        == 0x05c, "CPlayer.m_cSetting");
static_assert(offsetof(CPlayer, m_cMoney)          == 0x0b4, "CPlayer.m_cMoney");
static_assert(offsetof(CPlayer, m_cLevel)          == 0x0c4, "CPlayer.m_cLevel");
static_assert(offsetof(CPlayer, m_cShape)          == 0x0d0, "CPlayer.m_cShape");
static_assert(offsetof(CPlayer, m_cTCPAddress)     == 0x0d4, "CPlayer.m_cTCPAddress");
static_assert(offsetof(CPlayer, m_cUDPAddress)     == 0x0ec, "CPlayer.m_cUDPAddress");
static_assert(offsetof(CPlayer, m_cBaseFaculty)    == 0x104, "CPlayer.m_cBaseFaculty");
static_assert(offsetof(CPlayer, m_cRaiseFaculty)   == 0x11d, "CPlayer.m_cRaiseFaculty");
static_assert(offsetof(CPlayer, m_cItemFaculty)    == 0x136, "CPlayer.m_cItemFaculty");
static_assert(offsetof(CPlayer, m_cTrainingFaculty)== 0x14f, "CPlayer.m_cTrainingFaculty");
static_assert(offsetof(CPlayer, m_cItemOption)     == 0x168, "CPlayer.m_cItemOption");
static_assert(offsetof(CPlayer, m_cTotalRecord)    == 0x2c0, "CPlayer.m_cTotalRecord");
static_assert(offsetof(CPlayer, m_cQuarterRecord)  == 0x30c, "CPlayer.m_cQuarterRecord");
static_assert(offsetof(CPlayer, m_cCurrentRecord)  == 0x358, "CPlayer.m_cCurrentRecord");
static_assert(offsetof(CPlayer, m_cTotalRanking)   == 0x3a4, "CPlayer.m_cTotalRanking");
static_assert(offsetof(CPlayer, m_cQuarterRanking) == 0x3d6, "CPlayer.m_cQuarterRanking");
static_assert(offsetof(CPlayer, m_cRandomShop)     == 0x408, "CPlayer.m_cRandomShop");
static_assert(offsetof(CPlayer, m_cPlayLog)        == 0x420, "CPlayer.m_cPlayLog");
static_assert(offsetof(CPlayer, m_nEquipWear)      == 0x434, "CPlayer.m_nEquipWear");
static_assert(offsetof(CPlayer, m_nFreeWear)       == 0x478, "CPlayer.m_nFreeWear");
static_assert(offsetof(CPlayer, m_nHomeWear)       == 0x48c, "CPlayer.m_nHomeWear");
static_assert(offsetof(CPlayer, m_nAwayWear)       == 0x49c, "CPlayer.m_nAwayWear");
static_assert(offsetof(CPlayer, m_vItemList)       == 0x4ac, "CPlayer.m_vItemList");
static_assert(offsetof(CPlayer, m_vSkillList)      == 0x4b8, "CPlayer.m_vSkillList");
static_assert(offsetof(CPlayer, m_vTrainingList)   == 0x4c4, "CPlayer.m_vTrainingList");
static_assert(offsetof(CPlayer, m_vCeremonyList)   == 0x4d0, "CPlayer.m_vCeremonyList");
static_assert(offsetof(CPlayer, m_vCardList)       == 0x4dc, "CPlayer.m_vCardList");
static_assert(offsetof(CPlayer, m_vQuestList)      == 0x4e8, "CPlayer.m_vQuestList");
static_assert(offsetof(CPlayer, m_vBuddyList)      == 0x4f4, "CPlayer.m_vBuddyList");
static_assert(offsetof(CPlayer, m_vBlackList)      == 0x500, "CPlayer.m_vBlackList");
static_assert(offsetof(CPlayer, m_nCount)          == 0x50c, "CPlayer.m_nCount");
static_assert(offsetof(CPlayer, m_nGhostHost)      == 0x50d, "CPlayer.m_nGhostHost");
static_assert(offsetof(CPlayer, m_nCheckTCP)       == 0x50e, "CPlayer.m_nCheckTCP");
static_assert(offsetof(CPlayer, m_nCheckUDP)       == 0x50f, "CPlayer.m_nCheckUDP");
static_assert(offsetof(CPlayer, m_nRelay)          == 0x510, "CPlayer.m_nRelay");
static_assert(offsetof(CPlayer, m_nOperation)      == 0x511, "CPlayer.m_nOperation");
static_assert(offsetof(CPlayer, m_nCardEntry)      == 0x514, "CPlayer.m_nCardEntry");
static_assert(offsetof(CPlayer, m_nPlayCount)      == 0x518, "CPlayer.m_nPlayCount");
static_assert(offsetof(CPlayer, m_nPerformance)    == 0x51a, "CPlayer.m_nPerformance");
static_assert(offsetof(CPlayer, m_nNetSpeed)       == 0x51c, "CPlayer.m_nNetSpeed");
static_assert(offsetof(CPlayer, m_tTcpPingTime)    == 0x520, "CPlayer.m_tTcpPingTime");
static_assert(offsetof(CPlayer, m_tUdpPingTime)    == 0x524, "CPlayer.m_tUdpPingTime");
static_assert(offsetof(CPlayer, m_bIsGameReady)    == 0x528, "CPlayer.m_bIsGameReady");
static_assert(offsetof(CPlayer, m_bIsGameStart)    == 0x529, "CPlayer.m_bIsGameStart");
static_assert(offsetof(CPlayer, m_bIsTeamReady)    == 0x52a, "CPlayer.m_bIsTeamReady");
static_assert(offsetof(CPlayer, m_bIsPitIn)        == 0x52b, "CPlayer.m_bIsPitIn");
static_assert(offsetof(CPlayer, m_bPingCheck)      == 0x52c, "CPlayer.m_bPingCheck");
static_assert(offsetof(CPlayer, m_bSynch)          == 0x52d, "CPlayer.m_bSynch");
static_assert(offsetof(CPlayer, m_hTCPSecure)      == 0x530, "CPlayer.m_hTCPSecure");
static_assert(offsetof(CPlayer, m_nTodaySeed)      == 0x53c, "CPlayer.m_nTodaySeed");
static_assert(offsetof(CPlayer, m_nTodayBit)       == 0x540, "CPlayer.m_nTodayBit");
static_assert(offsetof(CPlayer, m_nShutupDate)     == 0x544, "CPlayer.m_nShutupDate");
static_assert(offsetof(CPlayer, m_nAutoPilot)      == 0x548, "CPlayer.m_nAutoPilot");

#endif // _GAME_PLAYER_H_
