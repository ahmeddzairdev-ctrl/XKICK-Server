// =============================================================================
// STUB MODULE - integration-debt placeholders for the XKICK_Game rebuild.
//
// FINAL stub-reconstruction pass (player-lifecycle / social / faculty / spine).
// Everything below is now a real reconstruction of the unstripped XKICK_Game1
// ELF, EXCEPT two documented safe-partials that depend on data-load modules not
// yet reconstructed in the portable CLoad:
//   * CPlayer::CheckLevelUp  -- needs the level/exp CSV table (no CLoad::GetLevelTable).
//   * CPlayer::SetLevel      -- the reward economy (event/mission/consumable/transjob
//                              multipliers + float-constant table) is deferred; the
//                              base-point award + persist path is reconstructed.
//   * Table_GetData          -- raw typed-row accessor (typed CSV layout not pinned).
//
// Earlier passes moved item/card/skill/quest/rndshop helpers into their own .cpp;
// this file keeps the connection/spine helpers + the CPlayer/CRoom members whose
// owning module had no natural home.
// =============================================================================
#include "Main.h"
#include "Sql.h"
#include "GameItem.h"
#include <cstring>
#include <cstdio>
#include <ctime>

// ---- free helpers defined in their own modules (resolved at link) ----
extern int      GetItemSkin(int nCode);                 // item.cpp  _Z11GetItemSkini
extern void     PacketLeaveRoom(CPlayer*, int nKind);   // lobby.cpp  int-form
extern void     PacketUpdateItem(CPlayer*, CSCUpdateItem*, CItem*); // item.cpp

// -----------------------------------------------------------------------------
// Runtime-class ctors/dtors (declared in Player.h/Room.h, not yet reconstructed).
// (CRandom/CRandomShop ctors now live in rndshop.cpp -- their layout changed.)
// -----------------------------------------------------------------------------
CPlayer::CPlayer() { m_nState = 0; m_bPingCheck = true; m_pRoom = 0; m_pTeam = 0; }
CPlayer::~CPlayer() {}
CRoom::CRoom()     { m_nState = 0; m_tGameStartTime = 0; m_nMission = -1;
    // Wire the pointer-lists once, mirroring CRoom ctor @08097bf4:
    //   athlete-list (group1) = {home, away};  total-list (group2) = {home, away, view, stay}.
    // SendRoom/SendTeam iterate these; without them every room/lobby broadcast (incl. chat)
    // reaches no one. g_RoomPool entries are constructed in place (no copies), so &member is stable.
    m_vAthleteList.push_back(&m_vHomeList); m_vAthleteList.push_back(&m_vAwayList);
    m_vTotalList.push_back(&m_vHomeList); m_vTotalList.push_back(&m_vAwayList);
    m_vTotalList.push_back(&m_vViewList); m_vTotalList.push_back(&m_vStayList);
}
CRoom::~CRoom()    {}
CTeam::CTeam()     { m_nState = 0; }
CTeam::~CTeam()    {}
COption::COption() { m_nCount = 0; }

// -----------------------------------------------------------------------------
// MONEY snapshot/commit (CPlayer money block @0xb4: cash/point/credit/clubpoint).
//   The in-memory CMoney @0xb4 (loaded at login via GetMemberField) is authoritative
//   -- the rebuild's CSql has no money SELECT (GetMoneyField/GetLevelField are not
//   reconstructed methods), so GetMoneyData is a no-op success and PutMoneyData
//   persists via CSql::UpdateMoneyField.
// -----------------------------------------------------------------------------
int  CPlayer_GetMoneyData(CPlayer* /*p*/) { return 0; }
int  CPlayer_PutMoneyData(CPlayer* p)     { return g_Sql.UpdateMoneyField(p); }

// -----------------------------------------------------------------------------
// Item-pointer getters (walk m_vItemList @0x4ac; skip null/empty; match seq).
// -----------------------------------------------------------------------------
CItem* CPlayer_GetItemPointer(CPlayer* p, int nItemSeq)
{
    for (size_t i = 0; i < p->m_vItemList.size(); ++i)
    {
        CItem* pItem = p->m_vItemList[i];
        if (pItem && !pItem->IsEmptyState() && pItem->m_nItemSeq == nItemSeq)
            return pItem;
    }
    return 0;
}
CItem* CPlayer_GetBagItem(CPlayer* p, int nItemSeq) { return CPlayer_GetItemPointer(p, nItemSeq); }

void CPlayer_SetItemState(CPlayer* p, int nItemSeq, int nState)
{
    for (size_t i = 0; i < p->m_vItemList.size(); ++i)
    {
        CItem* pItem = p->m_vItemList[i];
        if (pItem && !pItem->IsEmptyState() && pItem->m_nItemSeq == nItemSeq)
        {
            pItem->m_nState = (char)nState;
            return;
        }
    }
}

CItem* CPlayer::GetItemPointer(int nItemSeq) { return CPlayer_GetItemPointer(this, nItemSeq); }

// thin forwards to the real members (defined in player.cpp).
void  CPlayer_SetEquipWear   (CPlayer* p) { if (p) p->SetEquipWear(); }
void  CPlayer_SetItemOption  (CPlayer* p) { if (p) p->SetItemOption(); }

// =============================================================================
// CONNECT / SPINE FACTORIES + COUNTERS  (connect.cpp / packet.cpp / server.cpp)
// =============================================================================

// CreatePlayer @0809142e - first-free g_PlayerPool slot, InitPlayer, set fd +
// sentinel seqs, then register in g_PlayerList (hash registration happens later
// at SetPlayer/login). The peer-IP string the binary stashes at +0xd4 is set from
// the OS socket; in the portable build connect.cpp owns the socket layer, so it is
// filled from the CM_GAME_LOGIN packet instead and skipped here.
CPlayer* CreatePlayer(int nFd)
{
    for (int i = 0; i < MAX_PLAYER_POOL; ++i)
    {
        if (g_PlayerPool[i].m_nState == 0)
        {
            CPlayer* p = &g_PlayerPool[i];
            p->InitPlayer();
            p->m_nState        = 1;
            p->m_nFd           = nFd;
            p->m_nMemberSeq    = -1;
            p->m_nPlayerSeq = -1;
            g_PlayerList.push_back(p);
            return p;
        }
    }
    return 0;
}

// ExitPlayer @080893d0 - flush/detach/close/erase; the member is reconstructed in
// player.cpp, this is the free entry the connection layer calls.
void     ExitPlayer(CPlayer* pPlayer)            { if (pPlayer) pPlayer->ExitPlayer(); }

// The STS channel is a single upstream link; CServer is a near-empty node.
CServer* CreateStsServer(int nFd)
{
    static CServer s_stsNode;
    s_stsNode.m_nFd = nFd;
    return &s_stsNode;
}

// PutStsLogin @08081ee2 - announce this game server's code to the STS peer.
void PutStsLogin()
{
    CCSStsLogin pkt;
    pkt.m_nServerCode = g_Config.m_nServerCode;     // g_Config +0x134
    SendSTS(&pkt, 0);
}

// PutSendBroadcast @08081f14 - relay a GM broadcast string up the STS link.
void PutSendBroadcast(const char* sText)
{
    CCSSendBroadcast pkt;
    pkt.m_nKind = 7;
    memset(pkt.m_sMessage, 0, sizeof(pkt.m_sMessage));
    if (sText) StrCopyN(pkt.m_sMessage, sText, sizeof(pkt.m_sMessage) - 1);
    SendSTS(&pkt, 0);
}

int   GetPlayerCount()                           { return (int)g_PlayerList.size(); }

// GetRelayCount @0809185a - players currently flagged in-relay (byte @0x510 != 0).
int   GetRelayCount()
{
    int n = 0;
    for (size_t i = 0; i < g_PlayerList.size(); ++i)
    {
        CPlayer* p = g_PlayerList[i];
        if (p && p->m_nRelay != 0) ++n;
    }
    return n;
}

// GetItemSkin(void*) - CSql::UpdateSkinField reads the FIRST equip-wear code in the
// wear block (player+0x434) and maps it to its skin/colour via GetItemSkin(int).
int   GetItemSkin(void* pWearBlock)
{
    if (!pWearBlock) return 0xb;
    return GetItemSkin(*(int*)pWearBlock);
}

// PacketCharacterSearch @0807ed08 - resolve a character name to its seq.
void  PacketCharacterSearch(CPlayer* p, CHeadPacket* h)
{
    CSCCharacterSearch out;
    if (p == 0 || h == 0) return;

    int nSeq = g_Sql.GetPlayerSeq(((CCSCharacterSearch*)h)->m_sName);
    if (nSeq < 0)
    {
        char nResp = (nSeq == -2) ? (char)-12 : (nSeq == -1) ? (char)-11 : (char)-99;
        out.m_nResponse = nResp;
        SendPlayer(p, &out, (int)nResp);
    }
    else
    {
        out.m_nResponse  = 0;
        out.m_nPlayerSeq = nSeq;
        SendPlayer(p, &out, 0);
    }
}

// -----------------------------------------------------------------------------
// Raw-row table accessor expected by player.cpp's binary-format reads.
// The typed CSV row layout is not pinned in the portable CTable; documented safe
// stub -> a zeroed scratch row (links + runs; reads return zeroes).
// -----------------------------------------------------------------------------
void* Table_GetData(int /*handle*/, int /*recordIndex*/, int /*rawRowSentinel*/)
{
    static char s_zeroRow[0x40] = {0};
    return s_zeroRow;
}

// =============================================================================
// PLAYER-MGMT MEMBERS
// =============================================================================

int  CPlayer::GetPlayerSeq() const { return m_nPlayerSeq; }

// CalcLevelFaculty @08087a60
int  CPlayer::CalcLevelFaculty() { return (m_cLevel.m_nLevel / 10) + m_cLevel.m_nLevel * 2; }

// GetFacultyCount @0808b310
int  CPlayer::GetFacultyCount()  { return m_cLevel.m_nLevel * 2 + (int)m_cLevel.m_nLevel / 10; }

// GetPlayerTeam @08089530 - locate this player in the room's four id-lists.
//   1 team-A, 2 team-B, 3 spectator/C, 4 lobby, 0 not found.
char CPlayer::GetPlayerTeam()
{
    if (m_pRoom == 0) return 0;
    std::vector<int>* lists[4] = {
        &m_pRoom->m_vHomeList, &m_pRoom->m_vAwayList, &m_pRoom->m_vViewList, &m_pRoom->m_vStayList
    };
    for (int li = 0; li < 4; ++li)
    {
        std::vector<int>* v = lists[li];
        for (size_t i = 0; i < v->size(); ++i)
        {
            CPlayer* p = GetPlayerPointer((*v)[i]);
            if (p && p->m_nPlayerSeq == m_nPlayerSeq) return (char)(li + 1);
        }
    }
    return 0;
}

// ChangePosition @08087988 - set formation position, reset spare faculty to the
// level baseline, persist, then reload derived info.
void CPlayer::ChangePosition(int nPosition)
{
    m_nPosition     = (char)nPosition;
    m_cLevel.m_nFaculty = (short)CalcLevelFaculty();
    if (g_Sql.UpdatePositionField(this) < 0)
    {
        m_nMemberSeq    = -1;
        m_nPlayerSeq = -1;
    }
    else
    {
        SetPlayerInfo();
    }
}

// EnableAutoPilot @08090b5c - flag @0x548.
void CPlayer::EnableAutoPilot(bool bEnable) { m_nAutoPilot = bEnable ? 1 : 0; }

// InitPlayerInRoom @0808750e - attach room + clear per-room working flags.
void CPlayer::InitPlayerInRoom(CRoom* pRoom)
{
    m_pRoom       = pRoom;
    m_nRelay  = 0;                 // +0x510
    m_bIsGameReady = 0;            // +0x528
    m_bIsGameStart = 0;            // +0x529
    m_bIsPitIn     = 0;            // +0x52b
}

// JusticeAndPunishment @08090b76 - cardbot-mode early-leave point penalty.
void CPlayer::JusticeAndPunishment()
{
    if (m_pRoom == 0) return;
    if (m_pRoom->m_nMode != 2) return;                      // +0x02 (cardbot mode)

    int nPenalty = m_pRoom->m_nPointCode;                   // +0x5a room option point
    if ((int)time(NULL) - m_pRoom->m_tGameStartTime > 0x50) // > 80s into the game
        nPenalty <<= 1;

    m_cMoney.m_nPoint -= nPenalty;                          // +0xb8
    if (m_cMoney.m_nPoint < 0) m_cMoney.m_nPoint = 0;
    g_Sql.UpdateMoneyField(this);
}

// DivestSkill @0808e2e4 - unequip a learned skill (nSeq==0 -> first equipped one)
// and flag it dirty for the next CSql::UpdateSkillField flush.
void CPlayer::DivestSkill(int nSeq)
{
    for (size_t i = 0; i < m_vSkillList.size(); ++i)
    {
        CSkill* s = m_vSkillList[i];
        if (s == 0 || s->m_nState == 0) continue;
        if (nSeq == 0 || s->m_nSkillSeq == nSeq)
        {
            s->m_nEquipKind = 0;
            s->ChangeSkillState();
            return;
        }
    }
}

// RaiseFaculty @08090a3c - spend faculty points to raise the current faculties by
// the per-stat deltas in pDelta (cap 100 per stat incl. base; needs enough points).
int CPlayer::RaiseFaculty(CFaculty* pDelta)
{
    int nSum = 0;
    for (int i = 0; i < ARRAY_FACULTY_SIZE; ++i)
    {
        char d = pDelta->m_nFaculty[i];
        if (d < 0) return -1;
        if ((int)m_cRaiseFaculty.m_nFaculty[i] + (int)d + (int)m_cBaseFaculty.m_nFaculty[i] > 100) return -1;
        nSum += d;
    }
    if (m_cLevel.m_nFaculty < nSum) return -2;

    for (int i = 0; i < ARRAY_FACULTY_SIZE; ++i)
        m_cRaiseFaculty.m_nFaculty[i] = (char)(m_cRaiseFaculty.m_nFaculty[i] + pDelta->m_nFaculty[i]);
    m_cLevel.m_nFaculty = (short)(m_cLevel.m_nFaculty - nSum);

    if (CheckFaculty() < 0) return -1;
    return 0;
}

// CheckLevelUp @0808b190 - DOCUMENTED PARTIAL.  The binary promotes one level when
// m_cLevel.m_nExp >= CLoad::GetLevelTable(level+1).exp, grants +2 (+1 every 10th) faculty
// points and recomputes the base faculty.  The portable CLoad does not expose a
// level/exp table (no GetLevelTable), so the exp threshold cannot be evaluated ->
// no promotion is performed.  Reconstruct fully once the level CSV table is loaded.
int CPlayer::CheckLevelUp()
{
    if (m_cLevel.m_nLevel + 1 >= 0x33) return 0;     // already at/above max level (50)
    return 0;                               // level table unavailable -> no level-up
}

// SetRecord @08089a28 - accumulate this match into the weekly record block
// (m_cCurrentRecord.m_nRecord @0x358..0x3a0). pResult is a CCSGameResult (raw offsets).
void CPlayer::SetRecord(CCSGameResult* pResult)
{
    short result = (m_nTeam == 1) ? pResult->m_cHomeResult.m_nResult[0]
                                  : pResult->m_cAwayResult.m_nResult[0];
    if (result == 1)      { m_cCurrentRecord.m_nRecord[0] += 1; m_cCurrentRecord.m_nRecord[1] += 1; } // match,win
    else if (result == 2) { m_cCurrentRecord.m_nRecord[0] += 1; m_cCurrentRecord.m_nRecord[2] += 1; } // match,draw
    else if (result == 3) { m_cCurrentRecord.m_nRecord[0] += 1; }                                     // match (lose)
    else return;

    if (pResult->m_nMvpSeq == m_nPlayerSeq) m_cCurrentRecord.m_nRecord[4] += 1;          // mvp

    // locate this player's per-result slot (CEachResult), then read its CResult block.
    CResult* slot = 0;
    for (int i = 0; i < 12; ++i)
    {
        int s = pResult->m_cEachResult[i].m_nPlayerSeq;
        if (s == 0) break;
        if (s == m_nPlayerSeq) { slot = &pResult->m_cEachResult[i].m_cPlayerResult; break; }
    }
    if (slot == 0) return;

    m_cCurrentRecord.m_nRecord[3]  += slot->m_nResult[17];
    m_cCurrentRecord.m_nRecord[5]  += slot->m_nResult[1];
    m_cCurrentRecord.m_nRecord[6]  += slot->m_nResult[2];
    m_cCurrentRecord.m_nRecord[7]  += slot->m_nResult[3];
    m_cCurrentRecord.m_nRecord[8]  += slot->m_nResult[4];
    m_cCurrentRecord.m_nRecord[9]  += slot->m_nResult[5];
    m_cCurrentRecord.m_nRecord[10] += slot->m_nResult[6];
    m_cCurrentRecord.m_nRecord[11] += slot->m_nResult[7];
    m_cCurrentRecord.m_nRecord[12] += slot->m_nResult[8];
    m_cCurrentRecord.m_nRecord[13] += slot->m_nResult[9];
    m_cCurrentRecord.m_nRecord[14] += slot->m_nResult[12];
    m_cCurrentRecord.m_nRecord[15] += slot->m_nResult[13];
    m_cCurrentRecord.m_nRecord[16] += slot->m_nResult[14];
    m_cCurrentRecord.m_nRecord[17] += slot->m_nResult[15];
    m_cCurrentRecord.m_nRecord[18] += slot->m_nResult[16];
}

// SetLevel @08089d02 - DOCUMENTED PARTIAL.  The binary computes exp/point from a
// faculty-weighted base, then layers event/mission/consumable/transjob multipliers
// (float-constant table + several unreconstructed CPlayer helpers).  Those bonus
// stages are deferred; the slot's base mission-point is awarded as exp + point and
// the level/money persist + level-up check path is reconstructed faithfully.
void CPlayer::SetLevel(CCSGameResult* pResult)
{
    if (m_pRoom == 0) return;

    CEachResult* slot = 0;
    for (int i = 0; i < 12; ++i)
    {
        int s = pResult->m_cEachResult[i].m_nPlayerSeq;
        if (s == m_nPlayerSeq) { slot = &pResult->m_cEachResult[i]; break; }
        if (s == 0) break;
    }
    if (slot == 0) return;

    short result = (m_nTeam == 1) ? pResult->m_cHomeResult.m_nResult[0]
                                  : pResult->m_cAwayResult.m_nResult[0];
    if (result == 0) return;

    // clear the slot's per-result output block (flags m_nIsMvp..m_nIsLevelUp, exp/point)
    slot->m_nIsMvp = slot->m_nIsExpItem = slot->m_nIsPointItem = slot->m_nIsGoldenTime =
        slot->m_nIsNumber = slot->m_nIsEvent = slot->m_nIsLevelUp = 0;
    slot->m_nExp   = 0;
    slot->m_nPoint = 0;

    int nBase = slot->m_cPlayerResult.m_nResult[17];
    if (nBase < 0) nBase = 0;
    int nExp = nBase, nPoint = nBase;

    if (pResult->m_nMvpSeq == m_nPlayerSeq) slot->m_nIsMvp = 1;   // mvp

    m_cLevel.m_nExp           += nExp;
    m_cMoney.m_nPoint += nPoint;
    if (m_cMoney.m_nPoint < 0) m_cMoney.m_nPoint = 0;
    slot->m_nExp   = nExp;
    slot->m_nPoint = nPoint;

    slot->m_nIsLevelUp = CheckLevelUp() ? 1 : 0;
    slot->m_nLevel     = (short)m_cLevel.m_nLevel;
    slot->m_nPlayCount = m_nPlayCount;          // +0x518

    g_Sql.UpdateLevelField(this);
    g_Sql.UpdateMoneyField(this);
}

// SetConsume @0808a636 - decrement limited/consumable items one charge per match;
// when a charge hits 0 the item is emptied, wear/options are refreshed and the
// client is notified.  (The binary's play-ratio gate uses an unknown float
// constant; omitted -> every match consumes a charge.)
void CPlayer::SetConsume(CCSGameResult* /*pResult*/)
{
    for (size_t i = 0; i < m_vItemList.size(); ++i)
    {
        CItem* it = m_vItemList[i];
        if (it == 0 || it->IsEmptyState()) continue;
        if (it->m_nEquipKind == 0 || it->m_nAmount == -1) continue;
        if (it->m_nCode <= 0x1dde322c) continue;

        it->m_nAmount = (short)(it->m_nAmount - 1);
        if (it->m_nAmount == 0)
        {
            it->m_nState = 0;
            SetEquipWear();
            SetItemOption();
            CSCUpdateItem ui;
            PacketUpdateItem(this, &ui, it);
        }
        else
        {
            it->m_nState = 2;        // dirty -> next UpdateItemField flush
        }
    }
}

// GetPlayerInfo @080887b0 - fill the CPlayerInfo wire body (identity, level,
// faculties, total/quarter records, rankings).  Fills the rebuild's named struct
// (self-consistent); the framing size is taken from the struct, not a count byte.
int CPlayer::GetPlayerInfo(CPlayerInfo* pOut)
{
    // Live-client CM_PLAYER_INFO body (CPlayerInfo, 788 bytes; parser sub_4BD200).
    // Zero-fill first so the record/block/item blocks default to safe (non-crashing)
    // zeros; then populate the fields the client actually consumes for the lobby +
    // scout-test gate. The level lands at wire 86 -> CObjPlayerInfo+612 (gate/exp-bar).
    memset(pOut, 0, sizeof(CPlayerInfo));

    pOut->m_nPlayerSeq = m_nPlayerSeq;
    pOut->m_nPosition  = (char)m_nPosition;             // 0x04 -> CObjPlayerInfo+402
    memcpy(pOut->m_sName, m_sName, PLAYER_NAME_SIZE);   // 0x09 -> obj+415
    memcpy(pOut->m_sMent, m_sMent, PLAYER_MENT_SIZE);   // 0x18 -> obj+430

    pOut->m_cLevel.m_nLevel   = (short)m_cLevel.m_nLevel;   // 0x45 first byte -> obj+612 = gate level
    pOut->m_cLevel.m_nExp     = m_cLevel.m_nExp;            // 0x49 -> obj+616
    pOut->m_cLevel.m_nFaculty = m_cLevel.m_nFaculty;        // 0x4D -> obj+620 (low word)
    pOut->m_cLevel.m_nSkill   = m_cLevel.m_nSkill;          // 0x4F -> obj+620 (high word)

    // 0x51 equip-wear (4 ints): consumed by the model object (sub_51BE00[3..6]); slot
    // order not yet verified against the live client, left zeroed (default appearance)
    // so a wrong model code can't crash the lobby avatar load.

    memcpy(pOut->m_cBaseFaculty.m_nFaculty,     m_cBaseFaculty.m_nFaculty,  ARRAY_FACULTY_SIZE);  // 0x61 -> obj+864
    memcpy(pOut->m_cRaiseFaculty.m_nFaculty,    m_cRaiseFaculty.m_nFaculty, ARRAY_FACULTY_SIZE);  // 0x7A -> obj+889
    memcpy(pOut->m_cTrainingFaculty.m_nFaculty, m_cItemFaculty.m_nFaculty,  ARRAY_FACULTY_SIZE);  // 0x93 -> obj+939

    return 0x55;   // caller sizes the body from the struct
}

// GetAthleteInfo @08088966 - fill the public CAthleteInfo wire body (identity,
// shape, level, faculties, equip-wear, equipped skills + ceremonies).
void CPlayer::GetAthleteInfo(CAthleteInfo* pOut)
{
    pOut->m_nPlayerSeq = m_nPlayerSeq;
    pOut->m_nOperation = m_nOperation;             // rebuild: binary copies CPlayer m_nOperation
    pOut->m_nPosition  = m_nPosition;
    pOut->m_nTeam      = GetPlayerTeam();
    pOut->m_nSeat      = 0;
    memcpy(pOut->m_sName, m_sName, PLAYER_NAME_SIZE);
    memcpy(pOut->m_sMent, m_sMent, PLAYER_MENT_SIZE);

    pOut->m_cLevel.m_nLevel   = (short)m_cLevel.m_nLevel;
    pOut->m_cLevel.m_nExp     = m_cLevel.m_nExp;
    pOut->m_cLevel.m_nFaculty = m_cLevel.m_nFaculty;
    pOut->m_cLevel.m_nSkill   = m_cLevel.m_nSkill;

    pOut->m_cShape.m_nGender  = m_cShape.m_nGender;
    pOut->m_cShape.m_nSkin    = m_cShape.m_nSkin;
    pOut->m_cShape.m_nUniform = m_cShape.m_nUniform;

    // UDP address (binary qmemcpy's CPlayer m_cUDPAddress@0xec -> m_cAddress). Without
    // this the 24-byte address field is uninitialised stack garbage and the client
    // crashes parsing the bad IP string when it receives the athlete broadcast.
    memcpy(&pOut->m_cAddress, &m_cUDPAddress, sizeof(pOut->m_cAddress));

    memcpy(pOut->m_cBaseFaculty.m_nFaculty,     m_cBaseFaculty.m_nFaculty, ARRAY_FACULTY_SIZE);
    memcpy(pOut->m_cRaiseFaculty.m_nFaculty,    m_cRaiseFaculty.m_nFaculty,  ARRAY_FACULTY_SIZE);
    memcpy(pOut->m_cTrainingFaculty.m_nFaculty, m_cItemFaculty.m_nFaculty,  ARRAY_FACULTY_SIZE);
    // rebuild: binary CAthleteInfo has no item-faculty; the 344-byte option block trails.
    memset(pOut->m_cItemOption, 0, sizeof(pOut->m_cItemOption));

    // equip-wear working block @0x434 (17 ints), home @0x48c, away @0x49c.
    memcpy(pOut->m_nEquipWear, m_nEquipWear, sizeof(pOut->m_nEquipWear));
    memcpy(pOut->m_nHomeWear,  m_nHomeWear,  sizeof(pOut->m_nHomeWear));
    memcpy(pOut->m_nAwayWear,  m_nAwayWear,  sizeof(pOut->m_nAwayWear));

    // equipped skills (equip-kind == 1) -> CSkillInfo rows.
    memset(pOut->m_cSkillInfo, 0, sizeof(pOut->m_cSkillInfo));
    int ns = 0;
    for (size_t i = 0; i < m_vSkillList.size() && ns < MAX_SKILL; ++i)
    {
        CSkill* s = m_vSkillList[i];
        if (s == 0 || s->m_nEquipKind != 1) continue;
        pOut->m_cSkillInfo[ns].m_nSkillSeq  = s->m_nSkillSeq;
        pOut->m_cSkillInfo[ns].m_nCode      = s->m_nCode;
        pOut->m_cSkillInfo[ns].m_nEquipKind = s->m_nEquipKind;
        pOut->m_cSkillInfo[ns].m_nLevel     = s->m_nLevel;
        ++ns;
    }

    // equipped ceremonies -> CCeremonyInfo rows.
    memset(pOut->m_cCeremonyInfo, 0, sizeof(pOut->m_cCeremonyInfo));
    int nc = 0;
    for (size_t i = 0; i < m_vCeremonyList.size() && nc < MAX_CEREMONY; ++i)
    {
        CCeremony* c = m_vCeremonyList[i];
        if (c == 0 || c->m_nEquipKind == 0) continue;
        pOut->m_cCeremonyInfo[nc].m_nCeremonySeq = c->m_nCeremonySeq;
        pOut->m_cCeremonyInfo[nc].m_nCode        = c->m_nCode;
        pOut->m_cCeremonyInfo[nc].m_nEquipKind   = c->m_nEquipKind;
        ++nc;
    }
}

// GetAthleteInfo @08088db0 - GM 56-byte snapshot (raw byte layout per binary).
void CPlayer::GetAthleteInfo(CGMAthleteInfo* pOut)
{
    memset(pOut, 0, sizeof(*pOut));                       // 56 bytes
    pOut->m_nPlayerSeq   = m_nPlayerSeq;                  // +0x00 (4)
    memcpy(pOut->m_sName, m_sName, PLAYER_NAME_SIZE);     // +0x04 (15)
    pOut->m_nPosition    = m_nPosition;                   // +0x13
    pOut->m_nTeam        = m_nTeam;                       // +0x14
    pOut->m_nLevel       = m_cLevel.m_nLevel;             // +0x15
    pOut->m_nNetSpeed    = m_nNetSpeed;                   // +0x16 (4)
    pOut->m_nPerformance = (int)m_nPerformance;           // +0x1a (4)
    pOut->m_nRelay       = m_nRelay;                      // +0x1e
    pOut->m_nOperation   = m_nOperation;                  // +0x1f
    memcpy(pOut->m_cAddress, &m_cUDPAddress, 24);         // +0x20 (24) CPlayer m_cUDPAddress (+0xec)
}

// =============================================================================
// SOCIAL : BUDDY / BLACKLIST  (runtime objects in m_vBuddyList/m_vBlackList;
//   both use the CBuddyInfo layout the CSql loaders allocate -- the blacklist
//   vector stores CBuddyInfo* reinterpreted as CBlacklistInfo*, matching sql.cpp.)
// =============================================================================

// AddBuddyInfo @08087c16 - add a buddy relation + runtime row.
int CPlayer::AddBuddyInfo(int nSeq)
{
    if (nSeq == m_nPlayerSeq) return -11;
    CPlayer* pT = GetPlayerPointer(nSeq);
    if (pT == 0) return -12;
    for (size_t i = 0; i < m_vBuddyList.size(); ++i)
        if (m_vBuddyList[i] && m_vBuddyList[i]->m_nPlayerSeq == nSeq) return -13;

    if (g_Sql.AddBuddyField(this, pT) < 0) return -2;

    CBuddyInfo* pN = new CBuddyInfo;
    if (pN == 0) return -3;
    memset(pN, 0, sizeof(CBuddyInfo));
    pN->m_nPlayerSeq = nSeq;
    pN->m_nLevel     = (short)pT->m_cLevel.m_nLevel;
    pN->m_nPosition  = pT->m_nPosition;
    memcpy(pN->m_sName, pT->m_sName, PLAYER_NAME_SIZE);
    m_vBuddyList.push_back(pN);
    return 0;
}

// DeleteBuddyList @08087f16 - drop a buddy relation + runtime row.
int CPlayer::DeleteBuddyList(int nSeq)
{
    for (size_t i = 0; i < m_vBuddyList.size(); ++i)
    {
        CBuddyInfo* pB = m_vBuddyList[i];
        if (pB && pB->m_nPlayerSeq == nSeq)
        {
            m_vBuddyList.erase(m_vBuddyList.begin() + i);
            g_Sql.DeleteBuddyField(this, nSeq);
            delete pB;
            return 0;
        }
    }
    return -1;
}

// AddBlackListInfo @08087d96 - add a blacklist relation + runtime row.
int CPlayer::AddBlackListInfo(int nSeq)
{
    if (nSeq == m_nPlayerSeq) return -11;
    CPlayer* pT = GetPlayerPointer(nSeq);
    if (pT == 0) return -12;
    for (size_t i = 0; i < m_vBlackList.size(); ++i)
        if (m_vBlackList[i] && m_vBlackList[i]->m_nPlayerSeq == nSeq) return -13;

    if (g_Sql.AddBlackListField(this, pT) < 0) return -2;

    CBlacklistInfo* pN = new CBlacklistInfo;
    if (pN == 0) return -3;
    pN->m_nPlayerSeq = nSeq;
    pN->m_nLevel     = (char)pT->m_cLevel.m_nLevel;
    pN->m_nPosition  = pT->m_nPosition;
    snprintf(pN->m_sName, PLAYER_NAME_SIZE, "%s", pT->m_sName);
    m_vBlackList.push_back(pN);
    return 0;
}

// DeleteBlackList @0808800a - drop a blacklist relation + runtime row.
int CPlayer::DeleteBlackList(int nSeq)
{
    for (size_t i = 0; i < m_vBlackList.size(); ++i)
    {
        CBlacklistInfo* pB = m_vBlackList[i];
        if (pB && pB->m_nPlayerSeq == nSeq)
        {
            m_vBlackList.erase(m_vBlackList.begin() + i);
            g_Sql.DeleteBlackListField(this, nSeq);
            delete pB;
            return 0;
        }
    }
    return -1;
}

// IsBlocked - is nSeq on this player's blacklist?
bool CPlayer::IsBlocked(int nSeq)
{
    for (size_t i = 0; i < m_vBlackList.size(); ++i)
        if (m_vBlackList[i] && m_vBlackList[i]->m_nPlayerSeq == nSeq) return true;
    return false;
}

// GetBuddyInfo @080880fe - one page (10 rows) of buddies into a CSCBuddyInfo body.
void CPlayer::GetBuddyInfo(int nPage, CSCBuddyInfo* pOut)
{
    g_Sql.CheckBuddyField(this);

    for (int i = 0; i < 10; ++i)
    {
        pOut->m_cBuddyData[i].m_nPlayerSeq = 0;
        pOut->m_cBuddyData[i].m_nWhere     = 0;
        pOut->m_cBuddyData[i].m_nState     = 0;
        pOut->m_cBuddyData[i].m_nPosition  = 0;
    }

    int nBase = nPage * 10;
    int nOut = 0, nSeen = 0;
    if (nBase < 0) return;

    for (size_t k = 0; k < m_vBuddyList.size() && nOut < 10; ++k)
    {
        CBuddyInfo* pB = m_vBuddyList[k];
        if (pB == 0) continue;
        if (nSeen++ < nBase) continue;

        CBuddyData& d = pOut->m_cBuddyData[nOut];
        d.m_nPlayerSeq = pB->m_nPlayerSeq;

        CPlayer* pT = GetPlayerPointer(pB->m_nPlayerSeq);
        if (pT == 0)
        {
            d.m_nLevel    = (char)pB->m_nLevel;
            d.m_nPosition = pB->m_nPosition;
            d.m_nWhere    = 0;
            d.m_nState    = 0;
        }
        else
        {
            CRoom* pR = pT->m_pRoom;
            d.m_nLevel    = pT->m_cLevel.m_nLevel;
            d.m_nPosition = pT->m_nPosition;
            if (pR == 0) { d.m_nWhere = 1; d.m_nState = 0; }
            else
            {
                if (pR->GetRoomCource() == 0) d.m_nWhere = (pR->m_nRoomSeq == 0) ? 1 : 2;
                else                          d.m_nWhere = 3;
                d.m_nState = pR->m_nRoomSeq;
            }
        }
        memcpy(d.m_sName, pB->m_sName, PLAYER_NAME_SIZE);
        ++nOut;
    }

    pOut->m_nPage  = (nOut == 0) ? (char)0xFF : (char)nPage;
    pOut->m_nCount = (char)nOut;
}

// GetBlacklistInfo @08088488 - one page (10 rows) into a CSCBlacklistInfo body.
void CPlayer::GetBlacklistInfo(int nPage, CSCBlacklistInfo* pOut)
{
    g_Sql.CheckBlacklistField(this);

    for (int i = 0; i < 10; ++i)
    {
        pOut->m_cBlacklistData[i].m_nPlayerSeq = 0;
        pOut->m_cBlacklistData[i].m_nWhere     = 0;
        pOut->m_cBlacklistData[i].m_nLevel     = 0;
        pOut->m_cBlacklistData[i].m_nPosition  = 0;
    }

    int nBase = nPage * 10;
    int nOut = 0, nSeen = 0;
    if (nBase < 0) return;

    for (size_t k = 0; k < m_vBlackList.size() && nOut < 10; ++k)
    {
        CBlacklistInfo* pB = m_vBlackList[k];
        if (pB == 0) continue;
        if (nSeen++ < nBase) continue;

        CBlacklistData& d = pOut->m_cBlacklistData[nOut];
        d.m_nPlayerSeq = pB->m_nPlayerSeq;

        CPlayer* pT = GetPlayerPointer(pB->m_nPlayerSeq);
        if (pT == 0)
        {
            d.m_nLevel    = (char)pB->m_nLevel;
            d.m_nPosition = pB->m_nPosition;
            d.m_nWhere    = 0;
        }
        else
        {
            CRoom* pR = pT->m_pRoom;
            d.m_nLevel    = pT->m_cLevel.m_nLevel;
            d.m_nPosition = pT->m_nPosition;
            if (pR == 0) d.m_nWhere = 1;
            else if (pR->GetRoomCource() == 0) d.m_nWhere = (pR->m_nRoomSeq == 0) ? 1 : 2;
            else d.m_nWhere = 3;
        }
        memcpy(d.m_sName, pB->m_sName, PLAYER_NAME_SIZE);
        ++nOut;
    }

    pOut->m_nPage  = (nOut == 0) ? (char)0xFF : (char)nPage;
    pOut->m_nCount = (char)nOut;
}

// =============================================================================
// CRoom GM/admin members  (inlined in the GM handlers in the binary; reconstructed
// from the room id-lists + CSCRoomTool framing).
// =============================================================================

// KickAll - force every player in the room out (the handler deletes the room next).
void CRoom::KickAll()
{
    std::vector<CPlayer*> players;
    for (size_t g = 0; g < m_vTotalList.size(); ++g)
    {
        std::vector<int>* v = m_vTotalList[g];
        for (size_t i = 0; i < v->size(); ++i)
        {
            CPlayer* p = GetPlayerPointer((*v)[i]);
            if (p) players.push_back(p);
        }
    }
    for (size_t i = 0; i < players.size(); ++i)
        PacketLeaveRoom(players[i], 2);   // kicked
}

// GetAthleteList - fill up to nMax GM athlete rows for the room; returns the count.
int CRoom::GetAthleteList(CGMAthleteInfo* pOut, int nMax)
{
    int nCount = 0;
    for (size_t g = 0; g < m_vTotalList.size() && nCount < nMax; ++g)
    {
        std::vector<int>* v = m_vTotalList[g];
        for (size_t i = 0; i < v->size() && nCount < nMax; ++i)
        {
            CPlayer* p = GetPlayerPointer((*v)[i]);
            if (p == 0) continue;
            p->GetAthleteInfo(&pOut[nCount]);
            ++nCount;
        }
    }
    return nCount;
}
