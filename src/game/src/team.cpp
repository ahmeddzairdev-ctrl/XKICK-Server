// =============================================================================
// team.cpp - clan-match TEAM rooms (g_TeamPool[400]).  Reconstructed
// byte-faithfully from XKICK_Game1 (unstripped) via Ghidra.
//
// TCP handlers (registered in process.cpp):
//   PacketTeamInfo     @08076b7a  (0x1068)   PacketChoiceTeam   @08076cb4 (0x106d)
//   PacketQuickTeam    @08076eb6  (0x106e)   PacketTeamPosition @08076f58 (0x106f)
//   PacketLeaveTeam    @0807756e  (0x10cc)   PacketTeamReady    @080778c6 (0x1130)
//   PacketApplyMatch   @080779ee  (0x1131)
// internal senders / helpers:
//   PacketLeaveTeam(CPlayer*,int) @08077586   PacketChoiceTeam(CPlayer*,CTeam*) @08076e66
//   PacketTeamoneInfo  @08077290   PacketTeamoneEnd   @0807753c
//   PacketChangeTeamJang(CTeam*) @08077254 / (CPlayer*,CHeadPacket*) @0807717c
//   PacketMatchRoom    @08077b72   ChoiceQuickTeam    @080a055e
//   CreateTeam @080a01f8  CreateInstantTeam @080a038e  GetTeamPointer @080a01aa
//   GetTeamCount @080a050a  AutoMatchingSystem @080a067c  SendTeam(CTeam*) @0808294e
//
// CTeam methods (declared Room.h) defined here. CPlayer team logic uses the
// CPlayer_*(CPlayer*,...) free-helper convention (GetTeamoneInfo/InitPlayerInTeam).
//
// Portable: standard C++ only; raw CTeam/CPlayer offsets via TB/PF (matches the
// room/skill modules). #pragma pack(1) only on the GameProtocolTeam.h wire structs.
// =============================================================================
#include "Main.h"
#include "Room.h"
#include "Player.h"
#include "Global.h"
#include "LogManager.h"
#include <cstring>
#include <cstdio>

// cross-module helpers (CreateRoom in Lobby.h; GetPlayerPointer/WriteTCP/
// PacketChoiceRoom/PacketChangeJang(CRoom*) in packet.h -- both via Main.h)
void     PacketRoomInfo(CRoom*);                     // lobby.cpp (CRoom overload)

// forward decls (this file)  -- CCS* request structs come in via GameProtocolTeam.h
void  SendTeam(CTeam* pTeam, CHeadPacket* pPacket, int nResult);   // @0808294e
static void PacketChoiceTeam(CPlayer* pPlayer, CTeam* pTeam);      // @08076e66
static void PacketChangeTeamJang(CTeam* pTeam);                    // @08077254
static void PacketTeamoneEnd(CTeam* pTeam);                        // @0807753c
static void PacketMatchRoom(CTeam* pTeam);                         // @08077b72
static CTeam* CreateInstantTeam(int nPlayerSeq);                  // @080a038e
static CTeam* AutoMatchingSystem(CTeam* pTeam);                   // @080a067c
void  CPlayer_GetTeamoneInfo(CPlayer* pPlayer, CTeamoneInfo* pOut); // @08088e64
void  CPlayer_InitPlayerInTeam(CPlayer* pPlayer, CTeam* pTeam);     // @08087544

// =============================================================================
// team-pool helpers
// =============================================================================

// GetTeamPointer @080a01aa : g_TeamPool slot by team number (1..399), else 0.
CTeam* GetTeamPointer(int nNo)
{
    if (nNo < 1 || nNo >= MAX_TEAM_POOL) return 0;
    return &g_TeamPool[nNo];
}

// GetTeamCount @080a050a : live teams in the pool.
int GetTeamCount()
{
    int n = 0;
    for (int i = 1; i < MAX_TEAM_POOL; ++i)
        if (g_TeamPool[i].m_nState != 0) ++n;
    return n;
}

// SendTeam @0808294e : write a packet to every member of one team (id-list @0xe0
// whose loaded CTeam number matches), via WriteTCP.
void SendTeam(CTeam* pTeam, CHeadPacket* pPacket, int /*nResult*/)
{
    if (pTeam == 0 || pPacket == 0) return;
    std::vector<int>& list = pTeam->m_vPlayerList;
    for (size_t i = 0; i < list.size(); ++i)
    {
        CPlayer* pl = GetPlayerPointer(list[i]);
        if (pl == 0) continue;
        CTeam* t = pl->m_pTeam;
        if (t != 0 && t->m_nNo == pTeam->m_nNo)
            WriteTCP(pl, pPacket, (unsigned)(pPacket->m_nBodySize + 0xc));
    }
}

// =============================================================================
// CPlayer team helpers (binary CPlayer:: members)
// =============================================================================

// CPlayer::InitPlayerInTeam @08087544 : bind player to a team, clear ready flag.
void CPlayer_InitPlayerInTeam(CPlayer* pPlayer, CTeam* pTeam)
{
    pPlayer->m_pTeam = pTeam;
    pPlayer->m_bIsTeamReady  = 0;
}

// CPlayer::GetTeamoneInfo @08088e64 : pack one teammate's full match profile into
// a CTeamoneInfo (0x494). Reconstructed as the binary's raw field/memcpy copy.
// NOTE: the variable-length tail count that the binary folds into the packet body
// size is not pinned; PacketTeamoneInfo uses the constructor's fixed 0x495 body.
void CPlayer_GetTeamoneInfo(CPlayer* pPlayer, CTeamoneInfo* pOut)
{
    char* d = pOut->m_data;
    std::memset(d, 0, sizeof(pOut->m_data));

    *(int*)(d + 0x00)  = pPlayer->m_nPlayerSeq;          // seq
    d[0x04]            = pPlayer->m_nOperation;           // grade
    d[0x05]            = pPlayer->m_nPosition;            // position
    d[0x06]            = pPlayer->m_nTeam;                // team-area
    d[0x07]            = pPlayer->m_nSeat;                // team-seat
    d[0x08]            = pPlayer->m_bIsTeamReady;         // ready flag
    *(int*)(d + 0x39)  = *(int*)&pPlayer->m_cLevel;          // level (4-byte read: m_nLevel+pad)
    *(int*)(d + 0x3d)  = pPlayer->m_cLevel.m_nExp;          // exp
    *(int*)(d + 0x41)  = *(int*)&pPlayer->m_cLevel.m_nFaculty;// faculty+skill (4-byte read)
    *(short*)(d + 0x45)= *(short*)&pPlayer->m_cShape;       // gender+skin (2-byte read)
    d[0x47]            = pPlayer->m_cShape.m_nUniform;

    std::memcpy(d + 0xc3,  &pPlayer->m_cItemOption, 0x158);
    std::memcpy(d + 0xab,  pPlayer->m_nEquipWear, 0x44);
    std::memcpy(d + 0xef,  pPlayer->m_nHomeWear, 0x10);
    std::memcpy(d + 0xff,  pPlayer->m_nAwayWear, 0x10);
    StrCopyN(d + 0x18, pPlayer->m_sMent, 0x2d);      // ment
    StrCopyN(d + 0x09, pPlayer->m_sName, 0x0f);      // name

    // equipped skills (m_vSkillList, equip==1), up to 50 rows of 10 bytes @0x10f
    int n = 0;
    for (size_t i = 0; i < pPlayer->m_vSkillList.size(); ++i)
    {
        CSkill* s = pPlayer->m_vSkillList[i];
        if (s == 0) continue;
        if (n > 49) break;
        if (s->m_nEquipKind == 1)
        {
            char* r = d + 0x10f + n * 10;
            *(int*)(r + 0) = s->m_nSkillSeq;
            *(int*)(r + 4) = s->m_nCode;
            r[8] = s->m_nEquipKind;
            r[9] = s->m_nLevel;
            ++n;
        }
    }
    // equipped ceremonies (m_vCeremonyList, equip!=0), up to 5 rows of 9 bytes @0x303
    n = 0;
    for (size_t i = 0; i < pPlayer->m_vCeremonyList.size(); ++i)
    {
        CCeremony* c = pPlayer->m_vCeremonyList[i];
        if (c == 0) continue;
        if (n > 4) break;
        if (c->m_nEquipKind != 0)
        {
            char* r = d + 0x303 + n * 9;
            *(int*)(r + 0) = c->m_nCeremonySeq;
            *(int*)(r + 4) = c->m_nCode;
            r[8] = c->m_nEquipKind;
            ++n;
        }
    }
}

// =============================================================================
// CTeam methods (Room.h)
// =============================================================================

void CTeam::InitTeam()                              // @0809f15c
{
    m_nState = 0; m_nKind = 0; m_nCource = 0;
    m_nReserved008 = 0;
    m_nReserved054 = 0;
    m_nReserved058 = 0;
    for (int i = 0; i < 6; ++i)
    {
        m_slotName[i]   = 0;
        m_slotPos[i]    = 0;
        m_slotSeq[i]    = 0;
        m_slotFlagA[i]  = 0;
        m_slotFlagB[i * 0xf] = 0;
    }
    m_vPlayerList.clear();
}

void CTeam::GetTeamInfo(CTeamInfo* pOut)            // @0809f3a2
{
    char* d = pOut->m_data;   // CTeamInfo opaque wire body (offset-addressed)
    d[0] = m_nState; d[1] = m_nKind; d[2] = m_nCource;
    *(int*)(d + 3) = (int)m_nNo;
    *(int*)(d + 7) = m_nReserved008;
    StrCopyN(d + 0x0b, m_sName, 0x2f);
    StrCopyN(d + 0x3a, m_sPassword, 5);
    *(int*)(d + 0x3f) = m_nReserved040;
    d[0x43] = (char)m_nReserved044;
    d[0x44] = (char)m_nReserved046;
    d[0x45] = m_nOption[0]; d[0x46] = m_nOption[1]; d[0x47] = m_nOption[2];
    d[0x48] = m_nOption[3]; d[0x49] = m_nOption[4];
    *(short*)(d + 0x4a) = *(short*)&m_nOption[6];
    d[0x4c] = m_nOption[8]; d[0x4d] = m_nOption[9]; d[0x4e] = m_nOption[10]; d[0x4f] = m_nOption[11];
    std::memcpy(d + 0x50, m_slotName, 0x84);
}

int CTeam::GetTeamAmount()                          // @0809f20e
{
    return (int)m_vPlayerList.size();
}

short CTeam::GetMinLevel()                          // @0809f22a
{
    int nMin = 100;
    for (size_t i = 0; i < m_vPlayerList.size(); ++i)
    {
        CPlayer* pl = GetPlayerPointer(m_vPlayerList[i]);
        if (pl != 0 && (char)pl->m_cLevel.m_nLevel < nMin) nMin = (char)pl->m_cLevel.m_nLevel;
    }
    return (short)nMin;
}

short CTeam::GetMaxLevel()                          // @0809f2e6
{
    int nMax = 0;
    for (size_t i = 0; i < m_vPlayerList.size(); ++i)
    {
        CPlayer* pl = GetPlayerPointer(m_vPlayerList[i]);
        if (pl != 0 && nMax < (char)pl->m_cLevel.m_nLevel) nMax = (char)pl->m_cLevel.m_nLevel;
    }
    return (short)nMax;
}

bool CTeam::IsExistInTeam(CPlayer* pPlayer)         // @0809f868
{
    for (size_t i = 0; i < m_vPlayerList.size(); ++i)
    {
        CPlayer* pl = GetPlayerPointer(m_vPlayerList[i]);
        if (pl != 0 && pl->m_nPlayerSeq == pPlayer->m_nPlayerSeq) return true;
    }
    return false;
}

// CTeam::IsTeamAndSeatInsert @0809f920 : choose a free seat matching the player's
// position into pSeat->m_nSeat (GK at slot 5).  Returns 1 on success.
char CTeam::IsTeamAndSeatInsert(CPlayer* pPlayer, CTeamSeat* pSeat)
{
    char  pos = pPlayer->m_nPosition;
    if (pos == 0x28)                              // goalkeeper
    {
        if (m_slotSeq[5] == 0) { pSeat->m_nSeat = 5; return 1; }
        return 0;
    }
    for (int i = 0; i < 6; ++i)
    {
        char slot = m_slotName[i];
        if (m_slotSeq[i] != 0 || slot == 0) continue;
        if (slot == 1 ||
            (pos == 0x32 && slot != 0x28) ||
            slot == pos ||
            (int)slot == (pos / 10) * 10)
        {
            pSeat->m_nSeat = (char)i;
            return 1;
        }
    }
    return 0;
}

int CTeam::InsertPlayerTeam(CPlayer* pPlayer, int /*nSeat*/, char* pName) // @0809f4dc
{
    if (m_nState == 0) return -1;
    if (m_nCource != 0) return -2;
    if (m_nState == 2 && std::memcmp(m_sPassword, pName, 0x15) != 0 &&
        pPlayer->m_nOperation < 2)
        return -3;
    if (GetTeamAmount() >= m_nOption[11])  return -5;
    if (IsExistInTeam(pPlayer))      return -6;
    if ((char)pPlayer->m_cLevel.m_nLevel < m_nOption[9] || m_nOption[10] < (char)pPlayer->m_cLevel.m_nLevel)
        return -7;

    CTeamSeat seat; seat.m_nTeam = 0; seat.m_nSeat = 0;
    if (!IsTeamAndSeatInsert(pPlayer, &seat)) return -4;

    CPlayer_InitPlayerInTeam(pPlayer, this);
    pPlayer->m_nTeam = seat.m_nTeam;
    pPlayer->m_nSeat = seat.m_nSeat;
    int s = seat.m_nSeat;
    m_vPlayerList.push_back(pPlayer->m_nPlayerSeq);
    m_slotPos[s]    = pPlayer->m_nPosition;
    m_slotSeq[s]    = pPlayer->m_nPlayerSeq;
    m_slotFlagA[s]  = (char)pPlayer->m_cLevel.m_nLevel;
    StrCopyN(&m_slotFlagB[s * 0xf], pPlayer->m_sName, 0xf);
    SetTeamJang(0);
    return 0;
}

int CTeam::DeletePlayerTeam(CPlayer* pPlayer)       // @0809f6b2
{
    int   nSeq = pPlayer->m_nPlayerSeq;
    std::vector<int>& list = m_vPlayerList;
    for (std::vector<int>::iterator it = list.begin(); it != list.end(); ++it)
        if (*it == nSeq) { list.erase(it); break; }

    int s = pPlayer->m_nSeat;
    if (m_slotSeq[s] == nSeq)
    {
        m_slotPos[s]         = 0;
        m_slotSeq[s]         = 0;
        m_slotFlagA[s]       = 0;
        m_slotFlagB[s * 0xf] = 0;
    }
    if (GetTeamAmount() == 0) { DeleteTeam(); return 1; }
    return 0;
}

int CTeam::SetTeam(CCSSetTeam* pPacket)            // @0809fc08
{
    if (pPacket->m_nScaleCode < GetTeamAmount())          return -1;  // requested max < current
    if (pPacket->m_nEndLevel  < pPacket->m_nStartLevel)   return -2;  // maxLevel < minLevel
    if (GetMinLevel()         < pPacket->m_nStartLevel)   return -3;
    if (pPacket->m_nEndLevel  < GetMaxLevel())            return -4;

    m_nState = pPacket->m_nState; m_nKind = pPacket->m_nMode;
    StrCopyN(m_sName, pPacket->m_sTitle, 0x2f);
    StrCopyN(m_sPassword, pPacket->m_sPass, 5);
    m_nOption[2] = pPacket->m_nAttackCode; m_nOption[3] = pPacket->m_nScaleCode; m_nOption[4] = pPacket->m_nAICode;
    *(short*)&m_nOption[6] = pPacket->m_nPointCode;
    m_nOption[8] = pPacket->m_nAreaCode; m_nOption[9] = pPacket->m_nStartLevel; m_nOption[10] = pPacket->m_nEndLevel; m_nOption[11] = pPacket->m_nMaxCount;
    m_slotName[4] = (m_nOption[3] == 6) ? 1 : 0;
    return 0;
}

int CTeam::ChangePosition(CCSTeamPosition* pPacket) // @0809fa46
{
    char* p = pPacket->m_nTeamPosition;            // body @0x0c, 6 per-slot codes
    for (int i = 0; i < 6; ++i)
    {
        if (i == 0)
        {
            if ((p[0] < 0x0a || 0x13 < p[0]) && p[0] != 1) return -1;
            m_slotName[0] = p[0];
        }
        else if (i == 1)
        {
            if ((p[1] < 0x14 || 0x1d < p[1]) && p[1] != 1) return -2;
            m_slotName[1] = p[1];
        }
        else if (i == 2)
        {
            if ((p[2] < 0x1e || 0x27 < p[2]) && p[2] != 1) return -3;
            m_slotName[2] = p[2];
        }
        else if (i == 5)
        {
            if (p[5] != 0x28) return -4;
            m_slotName[5] = p[5];
        }
        else
        {
            m_slotName[i] = p[i];
        }
    }
    return 0;
}

void CTeam::SetTeamJang(CPlayer* pPlayer)          // @0809fd78
{
    if (pPlayer != 0) m_nReserved008 = pPlayer->m_nPlayerSeq;

    bool bFound = false;
    for (size_t i = 0; i < m_vPlayerList.size(); ++i)
    {
        CPlayer* pl = GetPlayerPointer(m_vPlayerList[i]);
        if (pl != 0 && pl->m_nPlayerSeq == m_nReserved008) { bFound = true; break; }
    }
    if (!bFound)
    {
        bool bSet = false;
        for (size_t i = 0; i < m_vPlayerList.size(); ++i)
        {
            CPlayer* pl = GetPlayerPointer(m_vPlayerList[i]);
            if (pl != 0) { m_nReserved008 = pl->m_nPlayerSeq; bSet = true; break; }
        }
        if (!bSet) m_nReserved008 = 0;
    }
    PacketChangeTeamJang(this);
}

char CTeam::IsTeamJang(int nSeq)                    // @0809ff36
{
    return (char)(m_nReserved008 == nSeq ? 1 : 0);
}

void CTeam::DeleteTeam()                            // @0809fbe6
{
    m_nState = 0;
    m_vPlayerList.clear();
}

void CTeam::InitTeamReadyFlag()                     // @0809ff9a
{
    for (size_t i = 0; i < m_vPlayerList.size(); ++i)
    {
        CPlayer* pl = GetPlayerPointer(m_vPlayerList[i]);
        if (pl != 0) pl->m_bIsTeamReady = 0;
    }
}

int CTeam::IsTeamReadyFlag()                        // @080a003a
{
    for (size_t i = 0; i < m_vPlayerList.size(); ++i)
    {
        CPlayer* pl = GetPlayerPointer(m_vPlayerList[i]);
        if (pl != 0 && pl->m_bIsTeamReady == 0) return 0;
    }
    return 1;
}

CPlayer* CTeam::GetPlayer(int nSeq)                 // @080a00f0
{
    for (size_t i = 0; i < m_vPlayerList.size(); ++i)
    {
        CPlayer* pl = GetPlayerPointer(m_vPlayerList[i]);
        if (pl != 0 && pl->m_nPlayerSeq == nSeq) return pl;
    }
    return 0;
}

// =============================================================================
// team creation / matchmaking helpers
// =============================================================================

// CreateTeam @080a01f8 : claim a free pool slot, seed from a CCSCreateTeam packet.
CTeam* CreateTeam(int /*nPlayerSeq*/, CCSCreateTeam* pPacket)
{
    CTeam* pTeam = 0;
    for (int i = 1; i < MAX_TEAM_POOL; ++i)
        if (g_TeamPool[i].m_nState == 0) { pTeam = &g_TeamPool[i]; break; }
    if (pTeam == 0) return 0;

    pTeam->InitTeam();
    pTeam->m_nState = pPacket->m_nState; pTeam->m_nKind = pPacket->m_nMode;
    StrCopyN(pTeam->m_sName, pPacket->m_sTitle, 0x2f);
    StrCopyN(pTeam->m_sPassword, pPacket->m_sPass, 5);
    pTeam->m_nReserved044 = 2;
    pTeam->m_nReserved046 = 0;
    pTeam->m_nOption[0] = 0; pTeam->m_nOption[1] = 1;
    pTeam->m_nOption[2] = pPacket->m_nAttackCode; pTeam->m_nOption[3] = pPacket->m_nScaleCode; pTeam->m_nOption[4] = pPacket->m_nAICode;
    pTeam->m_nOption[9] = pPacket->m_nStartLevel; pTeam->m_nOption[10] = pPacket->m_nEndLevel; pTeam->m_nOption[11] = pPacket->m_nMaxCount;
    std::memcpy(pTeam->m_slotName, pPacket->m_nTeamPosition, 6);
    return pTeam;
}

// CreateInstantTeam @080a038e : a default open quick-match team (state=2).
static CTeam* CreateInstantTeam(int /*nPlayerSeq*/)
{
    CTeam* pTeam = 0;
    for (int i = 1; i < MAX_TEAM_POOL; ++i)
        if (g_TeamPool[i].m_nState == 0) { pTeam = &g_TeamPool[i]; break; }
    if (pTeam == 0) return 0;

    pTeam->InitTeam();
    pTeam->m_nState = 1; pTeam->m_nKind = 0; pTeam->m_sName[0] = 0; pTeam->m_sPassword[0] = 0;
    pTeam->m_nReserved044 = 2;
    pTeam->m_nReserved046 = 0;
    pTeam->m_nOption[0] = 0; pTeam->m_nOption[1] = 1; pTeam->m_nOption[2] = 3; pTeam->m_nOption[3] = 5; pTeam->m_nOption[4] = 1;
    pTeam->m_nOption[9] = 1; pTeam->m_nOption[10] = 0x32; pTeam->m_nOption[11] = 2;
    for (int i = 0; i < 6; ++i)
    {
        pTeam->m_slotName[0] = 10; pTeam->m_slotName[1] = 0x14; pTeam->m_slotName[2] = 0x1e; pTeam->m_slotName[3] = 1;
        pTeam->m_slotName[4] = (pTeam->m_nOption[3] == 6) ? 1 : 0;
        pTeam->m_slotName[5] = 0x28;
    }
    return pTeam;
}

// ChoiceQuickTeam @080a055e : join the first open quick-match team, else spawn one.
int ChoiceQuickTeam(CPlayer* pPlayer)
{
    if (pPlayer == 0) return -1;
    for (int i = 1; i < MAX_TEAM_POOL; ++i)
    {
        CTeam* pTeam = &g_TeamPool[i];
        if (pTeam->m_nState != 0 && pTeam->m_nKind == 0 && pTeam->m_nCource == 0 &&
            pTeam->InsertPlayerTeam(pPlayer, 0, 0) >= 0)
        {
            PacketChoiceTeam(pPlayer, pTeam);
            return 0;
        }
    }
    CTeam* pNew = CreateInstantTeam(pPlayer->m_nPlayerSeq);
    if (pNew == 0) return -1;

    CSCChoiceTeam sc;
    sc.m_nTeamNo  = (int)pNew->m_nNo;
    sc.m_nResponse= 0;
    PacketChoiceTeam(pPlayer, (CHeadPacket*)&sc);     // (CPlayer*,CHeadPacket*) overload
    return 0;
}

// AutoMatchingSystem @080a067c : pick an opponent team whose applied (flag==1) and
// rating(+0x54) is within +/-step of ours; widen the step up to 3, else any.
static CTeam* AutoMatchingSystem(CTeam* pTeam)
{
    for (int step = 0; step < 4; ++step)
    {
        for (int i = 1; i < MAX_TEAM_POOL; ++i)
        {
            CTeam* o = &g_TeamPool[i];
            if (o->m_nState != 0 && o->m_nCource == 1 && o != pTeam)
            {
                if (pTeam->m_nReserved054 == step + o->m_nReserved054) return o;
                if (pTeam->m_nReserved054 == o->m_nReserved054 - step) return o;
            }
        }
    }
    for (int i = 1; i < MAX_TEAM_POOL; ++i)
    {
        CTeam* o = &g_TeamPool[i];
        if (o->m_nState != 0 && o->m_nCource == 1 && o != pTeam) return o;
    }
    return 0;
}

// =============================================================================
// internal senders
// =============================================================================

// PacketChangeTeamJang(CTeam*) @08077254 : announce the team master to the team.
static void PacketChangeTeamJang(CTeam* pTeam)
{
    CSCChangeTeamJang sc;
    sc.m_nJangSeq  = pTeam->m_nReserved008;
    sc.m_nResponse = 0;
    SendTeam(pTeam, &sc, 0);
}

// PacketTeamoneEnd(CTeam*) @0807753c : end-of-teamone-info marker.
static void PacketTeamoneEnd(CTeam* pTeam)
{
    CSCTeamoneEnd sc;
    sc.m_nResponse = 0;
    SendTeam(pTeam, &sc, 0);
}

// PacketChoiceTeam(CPlayer*,CTeam*) @08076e66 : leave the lobby, confirm the join.
static void PacketChoiceTeam(CPlayer* pPlayer, CTeam* pTeam)
{
    CSCChoiceTeam sc;
    g_RoomPool[0].DeletePlayerLobby(pPlayer);
    sc.m_nTeamNo   = (int)pTeam->m_nNo;
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// PacketTeamoneInfo @08077290 : send each OTHER teammate's profile to pPlayer, then
// broadcast pPlayer's own profile to the team, then close with PacketTeamoneEnd.
void PacketTeamoneInfo(CPlayer* pPlayer)
{
    CTeam* pTeam = pPlayer->m_pTeam;
    if (pTeam == 0)
    {
        CSCTeamoneInfo sc;
        sc.m_nResponse = (char)0xff;
        SendPlayer(pPlayer, &sc, -1);
        CLogManager log("team.cpp", 0x36d, 0, pPlayer, "PacketTeamoneInfo: no team(%d)\n", -1);
        log.LogOut();
        return;
    }
    for (size_t i = 0; i < pTeam->m_vPlayerList.size(); ++i)
    {
        CPlayer* pl = GetPlayerPointer(pTeam->m_vPlayerList[i]);
        if (pl != 0 && pl->m_nPlayerSeq != pPlayer->m_nPlayerSeq)
        {
            CSCTeamoneInfo sc;
            CPlayer_GetTeamoneInfo(pl, &sc.m_cInfo);
            sc.m_nResponse = 0;
            SendPlayer(pPlayer, &sc, 0);
        }
    }
    CSCTeamoneInfo self;
    CPlayer_GetTeamoneInfo(pPlayer, &self.m_cInfo);
    self.m_nResponse = 0;
    SendTeam(pTeam, &self, 0);
    PacketTeamoneEnd(pTeam);
}

// PacketMatchRoom @08077b72 : auto-match an opponent, spin up a real game room from
// the two teams, migrate everyone in, and announce.  PARTIAL: the binary also wires
// the room's two parent-team pointers (room+0x254/+0x258) which the rebuild's CRoom
// does not expose, so post-game result routing back to the teams is not re-linked.
static void PacketMatchRoom(CTeam* pTeam)
{
    CSCMatchRoom sc;
    CTeam* pOther = AutoMatchingSystem(pTeam);
    if (pOther == 0) return;

    CCSCreateRoom cr;
    std::memset(&cr, 0, sizeof(cr));
    cr.m_nState     = 2;
    cr.m_nMode      = 1;
    cr.m_nAttackCode= pTeam->m_nOption[2];
    cr.m_nScaleCode = pTeam->m_nOption[3];
    cr.m_nAICode    = pTeam->m_nOption[4];
    cr.m_nMaxCount  = (char)(pTeam->m_nOption[3] << 1);
    std::snprintf(cr.m_sTitle, TITLE_NAME_SIZE, "%s vs %s", pTeam->m_sName, pOther->m_sName);
    cr.m_sPass[0]   = 0;

    CRoom* pRoom = CreateRoom(0, &cr);
    if (pRoom == 0) return;

    pRoom->m_vHomeList = pTeam->m_vPlayerList;
    pRoom->m_vAwayList = pOther->m_vPlayerList;
    std::memcpy(&pRoom->m_cHomeSeat, pTeam->m_slotName, 0x84);
    std::memcpy(&pRoom->m_cAwaySeat, pOther->m_slotName, 0x84);

    for (size_t i = 0; i < pRoom->m_vHomeList.size(); ++i)
    {
        CPlayer* pl = GetPlayerPointer(pRoom->m_vHomeList[i]);
        if (pl) { pl->m_pRoom = pRoom; pl->m_nTeam = 1; }
    }
    for (size_t i = 0; i < pRoom->m_vAwayList.size(); ++i)
    {
        CPlayer* pl = GetPlayerPointer(pRoom->m_vAwayList[i]);
        if (pl) { pl->m_pRoom = pRoom; pl->m_nTeam = 2; }
    }

    pRoom->m_nRoomJangTeam = 1;
    pRoom->m_nHomeJangSeq  = pTeam->m_nReserved008;
    pRoom->m_nAwayJangSeq  = pOther->m_nReserved008;

    PacketRoomInfo(pRoom);
    PacketChangeJang(pRoom);

    sc.m_nResponse = 0;
    SendTeam(pTeam,  &sc, 0);
    SendTeam(pOther, &sc, 0);
    pTeam->m_nCource  = 2;
    pOther->m_nCource = 2;
}

// =============================================================================
// registered TCP handlers
// =============================================================================

// PacketTeamInfo @08076b7a (0x1068)
void PacketTeamInfo(CPlayer* pPlayer, CHeadPacket* /*pPacket*/)
{
    CSCTeamInfo sc;
    CTeam* pTeam = pPlayer->m_pTeam;
    if (pTeam == 0)
    {
        sc.m_nResponse = (char)0xff;
        SendPlayer(pPlayer, &sc, -1);
        CLogManager log("team.cpp", 0x2aa, 0, pPlayer, "PacketTeamInfo: no team(%d)\n", -1);
        log.LogOut();
        return;
    }
    pTeam->GetTeamInfo(&sc.m_cTeamInfo);
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
    PacketTeamoneInfo(pPlayer);
}

// PacketChoiceTeam @08076cb4 (0x106d)
void PacketChoiceTeam(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCChoiceTeam sc;
    CCSChoiceTeam* req = reinterpret_cast<CCSChoiceTeam*>(pPacket);  // client request body
    CTeam* pCur = pPlayer->m_pTeam;
    if (pCur != 0 && pCur->m_nNo >= 1)
        return;                                  // already on a team

    CTeam* pTeam = GetTeamPointer(req->m_nTeamSeq);
    if (pTeam == 0)
    {
        sc.m_nResponse = (char)0xff;
        SendPlayer(pPlayer, &sc, -1);
        CLogManager log("team.cpp", 0x2c9, 0, pPlayer, "PacketChoiceTeam: bad team(%d)\n",
                        req->m_nTeamSeq);
        log.LogOut();
        return;
    }
    int nRet = pTeam->InsertPlayerTeam(pPlayer, (int)req->m_nType, req->m_sPass);
    if (nRet < 0)
    {
        char nResp;
        switch (nRet) {
            case -8: nResp = (char)-0x12; break;
            case -7: nResp = (char)-0x11; break;
            case -6: nResp = (char)-0x10; break;
            case -5: nResp = (char)-0x0f; break;
            case -4: nResp = (char)-0x0e; break;
            case -3: nResp = (char)-0x0d; break;
            case -2: nResp = (char)-0x0c; break;
            case -1: nResp = (char)-0x0b; break;
            default: nResp = (char)-99;   break;
        }
        sc.m_nResponse = nResp;
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    g_RoomPool[0].DeletePlayerLobby(pPlayer);
    sc.m_nTeamNo   = (int)pTeam->m_nNo;
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// PacketQuickTeam @08076eb6 (0x106e)
void PacketQuickTeam(CPlayer* pPlayer, CHeadPacket* /*pPacket*/)
{
    CSCQuickTeam sc;
    CTeam* pCur = pPlayer->m_pTeam;
    if (pCur != 0 && pCur->m_nNo >= 1) return;

    int nRet = ChoiceQuickTeam(pPlayer);
    if (nRet < 0)
    {
        sc.m_nResponse = (nRet == -1) ? (char)-0x0b : (char)-99;
        SendPlayer(pPlayer, &sc, (int)sc.m_nResponse);
        return;
    }
    g_RoomPool[0].DeletePlayerLobby(pPlayer);
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// PacketTeamPosition @08076f58 (0x106f)
void PacketTeamPosition(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCTeamPosition sc;
    CTeam* pTeam = pPlayer->m_pTeam;
    if (pTeam == 0)
    {
        sc.m_nResponse = (char)0xff;
        SendPlayer(pPlayer, &sc, -1);
        CLogManager log("team.cpp", 0x321, 0, pPlayer, "PacketTeamPosition: no team(%d)\n", -1);
        log.LogOut();
        return;
    }
    if (!pTeam->IsTeamJang(pPlayer->m_nPlayerSeq))
    {
        sc.m_nResponse = (char)0xfe;
        SendPlayer(pPlayer, &sc, -2);
        return;
    }
    int nRet = pTeam->ChangePosition((CCSTeamPosition*)pPacket);
    if (nRet < 0)
    {
        if      (nRet == -3) sc.m_nResponse = (char)-0x0d;
        else if (nRet == -4) sc.m_nResponse = (char)-0x0e;
        else if (nRet == -2) sc.m_nResponse = (char)-0x0c;
        else if (nRet == -1) sc.m_nResponse = (char)-0x0b;
        SendTeam(pTeam, &sc, (int)sc.m_nResponse);
        return;
    }
    std::memcpy(sc.m_nSlotBlock, pTeam->m_slotName, 0x84);
    sc.m_nResponse = 0;
    SendTeam(pTeam, &sc, 0);
}

// PacketLeaveTeam(CPlayer*,int) @08077586 : leave (kind 1=quit,2=kick,3=match-start).
void PacketLeaveTeam(CPlayer* pPlayer, int nKind)
{
    CSCLeaveTeam sc;
    CTeam* pTeam = pPlayer->m_pTeam;
    if (pTeam == 0)
    {
        sc.m_nResponse = (char)0xff;
        SendPlayer(pPlayer, &sc, -1);
        CLogManager log("team.cpp", 0x3a0, 0, pPlayer, "PacketLeaveTeam: no team(%d)\n", -1);
        log.LogOut();
        return;
    }
    if (pTeam->m_nNo < 1 || 399 < pTeam->m_nNo)
    {
        CLogManager log("team.cpp", 0x3a7, 0, pPlayer, "PacketLeaveTeam: bad team no(%d)\n",
                        (int)pTeam->m_nNo);
        log.LogOut();
        return;
    }
    bool bEmpty = pTeam->DeletePlayerTeam(pPlayer);
    if (!bEmpty)
    {
        if (pTeam->m_nCource != 0)
        {
            if (pTeam->m_nCource == 1) pTeam->m_nCource = 0;
            else if (pTeam->m_nCource == 2) { pTeam->m_nCource = 0; pTeam->InitTeamReadyFlag(); }
            else
            {
                sc.m_nResponse = (char)-3;
                SendPlayer(pPlayer, &sc, -3);
                CLogManager log("team.cpp", 0x3c7, 0, pPlayer, "PacketLeaveTeam: bad state(%d)\n", -3);
                log.LogOut();
                return;
            }
        }
        pTeam->SetTeamJang(pPlayer);
    }
    if (nKind != 3) g_RoomPool[0].InsertPlayerLobby(pPlayer);
    sc.m_nPlayerSeq = pPlayer->m_nPlayerSeq;
    sc.m_nResponse  = (char)nKind;
    SendTeam(pTeam, &sc, nKind);
    if (nKind != 3) SendPlayer(pPlayer, &sc, nKind);
}

// PacketLeaveTeam @0807756e (0x10cc) : registered handler -> quit (kind 1).
void PacketLeaveTeam(CPlayer* pPlayer, CHeadPacket* /*pPacket*/)
{
    PacketLeaveTeam(pPlayer, 1);
}

// PacketTeamReady @080778c6 (0x1130) : toggle per-player ready flag (only pre-apply).
void PacketTeamReady(CPlayer* pPlayer, CHeadPacket* /*pPacket*/)
{
    CSCTeamReady sc;
    CTeam* pTeam = pPlayer->m_pTeam;
    if (pTeam == 0)
    {
        sc.m_nResponse = (char)0xff;
        SendPlayer(pPlayer, &sc, -1);
        CLogManager log("team.cpp", 0x3eb, 0, pPlayer, "PacketTeamReady: no team(%d)\n", -1);
        log.LogOut();
        return;
    }
    if (pTeam->m_nCource < 1)
    {
        if (pPlayer->m_bIsTeamReady == 0) { pPlayer->m_bIsTeamReady = 1; sc.m_nReadyKind = 3; }
        else                               { pPlayer->m_bIsTeamReady = 0; sc.m_nReadyKind = 2; }
        sc.m_nPlayerSeq = pPlayer->m_nPlayerSeq;
        sc.m_nResponse  = 0;
        SendTeam(pTeam, &sc, 0);
    }
    else
    {
        sc.m_nResponse = (char)0xfd;
        SendPlayer(pPlayer, &sc, -3);
    }
}

// PacketApplyMatch @080779ee (0x1131) : team master applies/cancels matchmaking.
void PacketApplyMatch(CPlayer* pPlayer, CHeadPacket* /*pPacket*/)
{
    CSCApplyMatch sc;
    CTeam* pTeam = pPlayer->m_pTeam;
    if (pTeam == 0)
    {
        sc.m_nResponse = (char)0xff;
        SendPlayer(pPlayer, &sc, -1);
        CLogManager log("team.cpp", 0x412, 0, pPlayer, "PacketApplyMatch: no team(%d)\n", -1);
        log.LogOut();
        return;
    }
    if (!pTeam->IsTeamJang(pPlayer->m_nPlayerSeq))
    {
        sc.m_nResponse = (char)0xfd;
        SendPlayer(pPlayer, &sc, -3);
        return;
    }
    if (pTeam->m_nCource < 1)
    {
        if (!pTeam->IsTeamReadyFlag())
        {
            sc.m_nResponse = (char)0xfc;
            SendPlayer(pPlayer, &sc, -4);
            return;
        }
        pTeam->m_nCource = 1;
        sc.m_nApplyKind = 1;
        sc.m_nResponse  = 0;
        SendTeam(pTeam, &sc, 0);
        PacketMatchRoom(pTeam);
    }
    else
    {
        pTeam->m_nCource = 0;
        sc.m_nApplyKind = 2;
        sc.m_nResponse  = 0;
        SendTeam(pTeam, &sc, 0);
    }
}
