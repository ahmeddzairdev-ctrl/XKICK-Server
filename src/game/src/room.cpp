// =============================================================================
// room.cpp - CRoom methods, the lobby/room-table free functions (Lobby.h), the
// room broadcast helpers (SendRoom/SendTeam), and the cross-module player-lookup
// helpers (GetPlayerPointer/SendAll/IsPlayerConnect).
//
// Reconstructed from XKICK_Game1 (unstripped). The lobby is g_RoomPool[0]; a room
// is a CRoom whose m_nNo > 0. Every CRoom keeps four std::vector<int> id-lists
// (home @0x20c, away @0x218, view @0x224, lobby @0x230) plus two vector-of-vector
// "group views": m_vAthleteList @0x23c = {home,away}, m_vTotalList @0x248 = {home,away,view}.
//
// Field offsets are kept byte-exact to the binary (the runtime CRoom layout in
// Room.h is offset-pinned; here we read/write through the documented members and,
// where the binary indexes per-slot working arrays inside m_slotBlock{A,B,C}, via
// explicit offsets into those 0x84-byte blocks).
//
// Portable: std C++ only.
// =============================================================================
#include "Main.h"
#include "packet.h"
#include "Net.h"
#include "Sql.h"
#include "GameLoad.h"
#include "GameProtocolRoom.h"
#include "LogManager.h"
#include <algorithm>
#include <cstring>
#include <ctime>
#include <cmath>

// ---- per-team reserve-seat blocks (CReserveSeat, 0x84 bytes each) ----------------
// m_cHomeSeat @0x80, m_cAwaySeat @0x104, m_cViewSeat @0x188. Per slot i (0..5):
// m_nReservePosition[i] @+0x00, m_nUsingPosition[i] @+0x06, m_nPlayerSeq[i] @+0x0c,
// m_nLevel[i] @+0x24, m_sName[i][15] @+0x2a. Accessed via named members below.

// =============================================================================
// CRoom::InitRoom  @080985a4  - reset all scalar room fields + per-slot arrays.
// =============================================================================
void CRoom::InitRoom()
{
    m_nState        = 0;   // kind
    m_nMode         = 0;   // cource
    m_nCource       = 0;   // state
    m_nParentSeq    = 0;
    m_nRoomJangTeam = 0;
    m_nHomeJangSeq  = 0;
    m_nAwayJangSeq  = 0;
    for (int i = 0; i < 6; ++i)
    {
        // home block @0x80
        m_cHomeSeat.m_nReservePosition[i] = 0;
        m_cHomeSeat.m_nUsingPosition[i]   = 0;
        m_cHomeSeat.m_nPlayerSeq[i]       = 0;
        m_cHomeSeat.m_nLevel[i]           = 0;
        m_cHomeSeat.m_sName[i][0]         = 0;
        // away block @0x104
        m_cAwaySeat.m_nReservePosition[i] = 0;
        m_cAwaySeat.m_nUsingPosition[i]   = 0;
        m_cAwaySeat.m_nPlayerSeq[i]       = 0;
        m_cAwaySeat.m_nLevel[i]           = 0;
        m_cAwaySeat.m_sName[i][0]         = 0;
        // view block @0x188
        m_cViewSeat.m_nReservePosition[i] = 0;
        m_cViewSeat.m_nUsingPosition[i]   = 0;
        m_cViewSeat.m_nPlayerSeq[i]       = 0;
        m_cViewSeat.m_nLevel[i]           = 0;
        m_cViewSeat.m_sName[i][0]         = 0;
    }
    m_vHomeList.clear();
    m_vAwayList.clear();
    m_vViewList.clear();
    m_vStayList.clear();
}

// =============================================================================
// CRoom::GetRoomInfo  @08098e40  - fill the public CRoomInfo wire struct.
// (scalar field map; the three CReserveSeat blocks come from the slot blocks.)
// =============================================================================
void CRoom::GetRoomInfo(CRoomInfo* pOut)
{
    pOut->m_nState        = m_nState;
    pOut->m_nMode         = m_nMode;
    pOut->m_nCource       = m_nCource;
    pOut->m_nRoomSeq      = (int)m_nRoomSeq;
    pOut->m_nRoomJangTeam = m_nRoomJangTeam;
    pOut->m_nHomeJangSeq  = m_nHomeJangSeq;
    pOut->m_nAwayJangSeq  = m_nAwayJangSeq;
    std::memcpy(pOut->m_sTitle, m_sTitle, 0x2f);
    std::memcpy(pOut->m_sPass,  m_sPass, 5);
    pOut->m_nQuestCode    = m_nQuestCode;
    pOut->m_nGroundCode   = (char)m_nGroundCode;
    pOut->m_nBallCode     = (char)m_nBallCode;
    pOut->m_nTimeCode     = m_nTimeCode;
    pOut->m_nWeatherCode  = m_nWeatherCode;
    pOut->m_nAttackCode   = m_nAttackCode;
    pOut->m_nScaleCode    = m_nScaleCode;
    pOut->m_nAICode       = m_nAICode;
    pOut->m_nPointCode    = m_nPointCode;
    pOut->m_nAreaCode     = m_nAreaCode;
    pOut->m_nStartLevel   = m_nStartLevel;
    pOut->m_nEndLevel     = m_nEndLevel;
    pOut->m_nAttackTeam   = m_nAttackTeam;
    pOut->m_nMaxCount     = m_nMaxCount;
    pOut->m_nCheckClub    = m_nCheckClub;
    pOut->m_nCheckTime    = m_nCheckTime;
    pOut->m_nCheckWeather = m_nCheckWeather;
    pOut->m_nCheckView    = m_nCheckView;
    pOut->m_nCheckViewChat = m_nCheckViewChat;
    std::memcpy(&pOut->m_cHomeSeat, &m_cHomeSeat, 0x84);
    std::memcpy(&pOut->m_cAwaySeat, &m_cAwaySeat, 0x84);
    std::memcpy(&pOut->m_cViewSeat, &m_cViewSeat, 0x84);
}

// =============================================================================
// CRoom::GetTotalAmount  @08098b50 / GetAthleteAmount @08098b08
// =============================================================================
int CRoom::GetTotalAmount()
{
    return (int)m_vHomeList.size() + (int)m_vAwayList.size() + (int)m_vViewList.size();
}
int CRoom::GetAthleteAmount()
{
    return (int)m_vHomeList.size() + (int)m_vAwayList.size();
}

// =============================================================================
// CRoom::GetTeamAmount  @08098a68
// =============================================================================
int CRoom::GetTeamAmount(int nTeam)
{
    if (nTeam == 0 || nTeam == 4) return (int)m_vStayList.size();
    if (nTeam == 1) return (int)m_vHomeList.size();
    if (nTeam == 2) return (int)m_vAwayList.size();
    if (nTeam == 3) return (int)m_vViewList.size();
    return GetTotalAmount();
}

// =============================================================================
// CRoom::GetRoomCource  @0809cbee / SetRoomCource @0809cbe0  (byte @0)
// =============================================================================
int  CRoom::GetRoomCource()        { return (int)m_nCource; }
void CRoom::SetRoomCource(int n)   { m_nCource = (char)n; }

// =============================================================================
// CRoom::GetMinLevel @08098bb4 / GetMaxLevel @08098cfa  (over home+away players)
// =============================================================================
short CRoom::GetMinLevel()
{
    int nMin = 100;
    for (size_t g = 0; g < m_vAthleteList.size(); ++g)
    {
        std::vector<int>* pV = m_vAthleteList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* p = GetPlayerPointer((*pV)[i]);
            if (p != 0 && (char)p->m_cLevel.m_nLevel < nMin)
                nMin = (char)p->m_cLevel.m_nLevel;
        }
    }
    return (short)nMin;
}
short CRoom::GetMaxLevel()
{
    int nMax = 0;
    for (size_t g = 0; g < m_vAthleteList.size(); ++g)
    {
        std::vector<int>* pV = m_vAthleteList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* p = GetPlayerPointer((*pV)[i]);
            if (p != 0 && nMax < (char)p->m_cLevel.m_nLevel)
                nMax = (char)p->m_cLevel.m_nLevel;
        }
    }
    return (short)nMax;
}

// =============================================================================
// CRoom::GetMinPoint  @0809b29e  (lowest player point, player+0xb8)
// =============================================================================
int CRoom::GetMinPoint()
{
    int  nMin   = 0;
    bool bFirst = true;
    for (size_t g = 0; g < m_vAthleteList.size(); ++g)
    {
        std::vector<int>* pV = m_vAthleteList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* p = GetPlayerPointer((*pV)[i]);
            if (p == 0) continue;
            int nPoint = p->m_cMoney.m_nPoint;
            if (bFirst)            { nMin = nPoint; bFirst = false; }
            else if (nPoint < nMin){ nMin = nPoint; }
        }
    }
    return nMin;
}

// =============================================================================
// CRoom::IsRoomJang @0809c7e6 / IsTeamJang @0809c82a
// =============================================================================
char CRoom::IsRoomJang(int nSeq)
{
    if (m_nRoomJangTeam == 1) { if (m_nHomeJangSeq == nSeq) return 1; }
    else                      { if (m_nAwayJangSeq == nSeq) return 1; }
    return 0;
}
char CRoom::IsTeamJang(int nSeq)
{
    if (m_nHomeJangSeq == nSeq) return 1;
    if (m_nAwayJangSeq == nSeq) return 1;
    return 0;
}

// =============================================================================
// CRoom::SetRoomJang  @0809beb4  - derive the room-master team from the two seats.
// =============================================================================
void CRoom::SetRoomJang()
{
    int home = m_nHomeJangSeq;
    int away = m_nAwayJangSeq;
    if (home == 0 && away == 0)        m_nRoomJangTeam = 0;
    else if (home == 0 && away != 0)   m_nRoomJangTeam = 2;
    else if (home != 0 && away == 0)   m_nRoomJangTeam = 1;
    else /* both set */                m_nRoomJangTeam = 1;   // home keeps mastership
}

// =============================================================================
// CRoom::SetTeamJang  @0809bad4  - assign/repair the per-team master seqs.
// If pPlayer is set and is on a team, it becomes that team's master; then both
// masters are validated against the live id-lists (cleared if gone), and the
// room-master team is recomputed and broadcast.
// =============================================================================
void CRoom::SetTeamJang(CPlayer* pPlayer)
{
    if (pPlayer != 0)
    {
        if (pPlayer->m_nTeam == 1)      m_nHomeJangSeq = pPlayer->m_nPlayerSeq;
        else if (pPlayer->m_nTeam == 2) m_nAwayJangSeq = pPlayer->m_nPlayerSeq;
    }

    // validate home master: if not present in home list, fall back to first member.
    {
        bool bFound = false;
        for (size_t i = 0; i < m_vHomeList.size(); ++i)
        {
            CPlayer* p = GetPlayerPointer(m_vHomeList[i]);
            if (p != 0 && p->m_nPlayerSeq == m_nHomeJangSeq) { bFound = true; break; }
        }
        if (!bFound)
        {
            m_nHomeJangSeq = 0;
            for (size_t i = 0; i < m_vHomeList.size(); ++i)
            {
                CPlayer* p = GetPlayerPointer(m_vHomeList[i]);
                if (p != 0) { m_nHomeJangSeq = p->m_nPlayerSeq; break; }
            }
        }
    }
    // validate away master.
    {
        bool bFound = false;
        for (size_t i = 0; i < m_vAwayList.size(); ++i)
        {
            CPlayer* p = GetPlayerPointer(m_vAwayList[i]);
            if (p != 0 && p->m_nPlayerSeq == m_nAwayJangSeq) { bFound = true; break; }
        }
        if (!bFound)
        {
            m_nAwayJangSeq = 0;
            for (size_t i = 0; i < m_vAwayList.size(); ++i)
            {
                CPlayer* p = GetPlayerPointer(m_vAwayList[i]);
                if (p != 0) { m_nAwayJangSeq = p->m_nPlayerSeq; break; }
            }
        }
    }

    SetRoomJang();
    PacketChangeJang(this);
}

// =============================================================================
// CRoom::SetAttackTeam  @0809876c  - pick kickoff team from attack-code @0x56.
// =============================================================================
void CRoom::SetAttackTeam()
{
    char nAttack = m_nAttackCode;
    if (nAttack == 1)          // random
        m_nAttackTeam = (char)(g_Random.Random() % 2) + 1;
    else if (nAttack == 0)     // alternate (1<->2)
        m_nAttackTeam = (m_nAttackTeam == 1) ? 2 : 1;
    else if (nAttack == 3)
        m_nAttackTeam = 1;
    else if (nAttack == 4)
        m_nAttackTeam = 2;
}

// =============================================================================
// CRoom::CreateMission  @080987fc  - roll a random mission (point/exp target).
// =============================================================================
int CRoom::CreateMission()
{
    if (m_nMission == -1)
    {
        CMission* pMission = (CMission*)g_Load.GetRandomMission();
        if (pMission == 0)
        {
            m_cMission.m_nSeq    = 0;
            m_cMission.m_nCount  = 0;
            m_cMission.m_nKind   = 0;
            m_cMission.m_nReward = 0;
        }
        else
        {
            // pMission is a loaded mission TEMPLATE (not a CMission); its int[] layout:
            // [0]=seq, [1]=baseExp, [2]=basePoint, [3..4]=double coef, [5]=minCount,
            // [6]=maxCount, [7]=kindRange, [8]=name. Left as a raw blob read.
            int* m = (int*)pMission;
            m_cMission.m_nSeq = m[0];
            int r = g_Random.Random();
            if (m[5] < m[6])
                m_cMission.m_nCount = r % (m[6] - m[5]) + m[5];
            else
                m_cMission.m_nCount = m[6];

            int r2 = g_Random.Random();
            m_cMission.m_nKind = (char)(0x0c + (r2 % 2));   // OBJECT_KIND_POINT(12) or EXP(13)
            int nCount = m_cMission.m_nCount;
            int nBase  = (m_cMission.m_nKind == 0x0c) ? m[2] : m[1];
            double coef = *(double*)(m + 3);
            int nReward = (int)(std::floor((coef * (double)(nCount * nCount)
                                + (double)(nCount * nBase)) / 10.0)) * 10;
            m_cMission.m_nReward = nReward;
        }
        m_nMission = m_cMission.m_nSeq;
    }
    return 0;
}

// =============================================================================
// CRoom::GetRoomSpeed  @0809ba7c  - parent's saved play-speed (player+0x51c),
// or 1000 if no parent.
// =============================================================================
int CRoom::GetRoomSpeed()
{
    int nParent = GetParent();
    if (nParent < 0) return 1000;
    CPlayer* p = GetPlayerPointer(nParent);
    if (p == 0) return 1000;
    return p->m_nNetSpeed;
}

// =============================================================================
// CRoom::GetParent  @0809b63e  - pick the relay host: the home/away player with
// the highest reported NAT port (player+0x51a) whose speed (player+0x51c) is
// within the current ping window. GM/trio host (GetSpecialParent) wins outright.
// Falls back to the first present player; -1 if none.
// =============================================================================
int CRoom::GetParent()
{
    int nSpecial = GetSpecialParent();
    if (nSpecial >= 1) return nSpecial;

    for (int nWindow = 100; nWindow < 0x44d; nWindow += 100)
    {
        int nBestPort = -1;
        int nBestSeq  = 0;
        for (size_t g = 0; g < m_vAthleteList.size(); ++g)
        {
            std::vector<int>* pV = m_vAthleteList[g];
            for (size_t i = 0; i < pV->size(); ++i)
            {
                CPlayer* p = GetPlayerPointer((*pV)[i]);
                if (p == 0) continue;
                short nPort  = p->m_nPerformance;
                int   nSpeed = p->m_nNetSpeed;
                if (nBestPort < nPort && nSpeed <= nWindow)
                {
                    nBestPort = nPort;
                    nBestSeq  = p->m_nPlayerSeq;
                }
            }
        }
        if (nBestSeq != 0) return nBestSeq;
    }

    // nobody matched a window: just return the first present player.
    for (size_t g = 0; g < m_vAthleteList.size(); ++g)
    {
        std::vector<int>* pV = m_vAthleteList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* p = GetPlayerPointer((*pV)[i]);
            if (p != 0) return p->m_nPlayerSeq;
        }
    }
    return -1;
}

// =============================================================================
// CRoom::SetParent  @0809b5f0  - store GetParent() into room+8.
// =============================================================================
int CRoom::SetParent()
{
    int n = GetParent();
    if (n < 0) return -1;
    m_nParentSeq = n;
    return 0;
}

// =============================================================================
// CRoom::GetSpecialParent  @0809b92a  - first room member flagged trio_host
// (player+0x50d > 0), else -1.
// =============================================================================
int CRoom::GetSpecialParent()
{
    for (size_t g = 0; g < m_vTotalList.size(); ++g)
    {
        std::vector<int>* pV = m_vTotalList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* p = GetPlayerPointer((*pV)[i]);
            if (p != 0 && p->m_nGhostHost > 0)
                return p->m_nPlayerSeq;
        }
    }
    return -1;
}

// =============================================================================
// CRoom::CheckGameStart  @0809c50a  - balanced-team gate for normal/clan rooms.
// =============================================================================
int CRoom::CheckGameStart()
{
    if ((m_nMode == 0 || m_nMode == 2) && (m_nAICode == 0 || m_nAICode == 2))
    {
        if ((int)m_vHomeList.size() != (int)m_vAwayList.size())
            return 0;
    }
    return 1;
}

// =============================================================================
// CRoom::InitSynchFlag @0809c6be / CheckSynchFlag @0809c582
// (player synch flag is player+0x52d)
// =============================================================================
void CRoom::InitSynchFlag()
{
    for (size_t g = 0; g < m_vAthleteList.size(); ++g)
    {
        std::vector<int>* pV = m_vAthleteList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* p = GetPlayerPointer((*pV)[i]);
            if (p != 0) p->m_bSynch = 0;
        }
    }
}
int CRoom::CheckSynchFlag()
{
    for (size_t g = 0; g < m_vAthleteList.size(); ++g)
    {
        std::vector<int>* pV = m_vAthleteList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* p = GetPlayerPointer((*pV)[i]);
            if (p != 0 && p->m_bSynch == 0) return 0;
        }
    }
    return 1;
}

// =============================================================================
// CRoom::ChangeTeam  @0809aa8c  - move a player to a new team/seat (after the
// caller resolved it via IsTeamAndSeatChange), updating the id-lists + slot block.
// =============================================================================
int CRoom::ChangeTeam(CPlayer* pPlayer, int nTeam)
{
    CTeamSeat seat;
    if (!IsTeamAndSeatChange(pPlayer, nTeam, &seat))
        return -1;

    // remove the player's id from whichever group-2 sub-list currently holds it.
    for (size_t g = 0; g < m_vTotalList.size(); ++g)
    {
        std::vector<int>* pV = m_vTotalList[g];
        std::vector<int>::iterator it = std::find(pV->begin(), pV->end(), pPlayer->m_nPlayerSeq);
        if (it != pV->end()) { pV->erase(it); break; }
    }

    // clear the old slot.
    int oldSeat = (int)pPlayer->m_nTeam;   // note: binary reads team@0x13
    {
        int idx = (int)pPlayer->m_nSeat;
        if (pPlayer->m_nTeam == 1)
        {
            m_cHomeSeat.m_nUsingPosition[idx] = 0; m_cHomeSeat.m_nPlayerSeq[idx] = 0; m_cHomeSeat.m_nLevel[idx] = 0; m_cHomeSeat.m_sName[idx][0] = 0;
        }
        else if (pPlayer->m_nTeam == 2)
        {
            m_cAwaySeat.m_nUsingPosition[idx] = 0; m_cAwaySeat.m_nPlayerSeq[idx] = 0; m_cAwaySeat.m_nLevel[idx] = 0; m_cAwaySeat.m_sName[idx][0] = 0;
        }
        else
        {
            m_cViewSeat.m_nUsingPosition[idx] = 0; m_cViewSeat.m_nPlayerSeq[idx] = 0; m_cViewSeat.m_nLevel[idx] = 0; m_cViewSeat.m_sName[idx][0] = 0;
        }
    }
    (void)oldSeat;

    // apply the new team/seat.
    pPlayer->m_nTeam           = seat.m_nTeam;
    pPlayer->m_nSeat         = seat.m_nSeat;
    int idx = (int)seat.m_nSeat;
    if (pPlayer->m_nTeam == 1)
    {
        m_vHomeList.push_back(pPlayer->m_nPlayerSeq);
        m_cHomeSeat.m_nUsingPosition[idx] = pPlayer->m_nPosition;
        m_cHomeSeat.m_nPlayerSeq[idx] = pPlayer->m_nPlayerSeq;
        m_cHomeSeat.m_nLevel[idx] = pPlayer->m_cLevel.m_nLevel;
        std::memcpy(m_cHomeSeat.m_sName[idx], pPlayer->m_sName, 0xf);
    }
    else if (pPlayer->m_nTeam == 2)
    {
        m_vAwayList.push_back(pPlayer->m_nPlayerSeq);
        m_cAwaySeat.m_nUsingPosition[idx] = pPlayer->m_nPosition;
        m_cAwaySeat.m_nPlayerSeq[idx] = pPlayer->m_nPlayerSeq;
        m_cAwaySeat.m_nLevel[idx] = pPlayer->m_cLevel.m_nLevel;
        std::memcpy(m_cAwaySeat.m_sName[idx], pPlayer->m_sName, 0xf);
    }
    else
    {
        m_vViewList.push_back(pPlayer->m_nPlayerSeq);
        m_cViewSeat.m_nUsingPosition[idx] = pPlayer->m_nPosition;
        m_cViewSeat.m_nPlayerSeq[idx] = pPlayer->m_nPlayerSeq;
        m_cViewSeat.m_nLevel[idx] = pPlayer->m_cLevel.m_nLevel;
        std::memcpy(m_cViewSeat.m_sName[idx], pPlayer->m_sName, 0xf);
    }

    SetTeamJang(0);
    return 0;
}

// =============================================================================
// CRoom::ChangePosition  @0809af40  - validate + apply a 6-slot home/away
// formation from a CCSChangePosition. Returns 0 / -1..-4 on a bad slot.
// =============================================================================
int CRoom::ChangePosition(CCSChangePosition* pPacket)
{
    char* h = pPacket->m_nHomePosition;
    char* a = pPacket->m_nAwayPosition;
    for (int i = 0; i <= 5; ++i)
    {
        if (i == 0)        // GK-line forwards: 10..19 or 1
        {
            if ((h[0] < 0x0a || h[0] > 0x13) && h[0] != 1) return -1;
            m_cHomeSeat.m_nReservePosition[0] = h[0];
            if ((a[0] < 0x0a || a[0] > 0x13) && a[0] != 1) return -1;
            m_cAwaySeat.m_nReservePosition[0] = a[0];
        }
        else if (i == 1)   // 20..29 or 1
        {
            if ((h[1] < 0x14 || h[1] > 0x1d) && h[1] != 1) return -2;
            m_cHomeSeat.m_nReservePosition[1] = h[1];
            if ((a[1] < 0x14 || a[1] > 0x1d) && a[1] != 1) return -2;
            m_cAwaySeat.m_nReservePosition[1] = a[1];
        }
        else if (i == 2)   // 30..39 or 1
        {
            if ((h[2] < 0x1e || h[2] > 0x27) && h[2] != 1) return -3;
            m_cHomeSeat.m_nReservePosition[2] = h[2];
            if ((a[2] < 0x1e || a[2] > 0x27) && a[2] != 1) return -3;
            m_cAwaySeat.m_nReservePosition[2] = a[2];
        }
        else if (i == 5)   // keeper must be '(' (0x28)
        {
            if (h[5] != 0x28) return -4;
            m_cHomeSeat.m_nReservePosition[5] = h[5];
            if (a[5] != 0x28) return -4;
            m_cAwaySeat.m_nReservePosition[5] = a[5];
        }
        else               // slots 3,4 copied verbatim
        {
            m_cHomeSeat.m_nReservePosition[i] = h[i];
            m_cAwaySeat.m_nReservePosition[i] = a[i];
        }
    }
    return 0;
}

// =============================================================================
// CRoom::InsertPlayerLobby  @08099434
// =============================================================================
int CRoom::InsertPlayerLobby(CPlayer* pPlayer)
{
    pPlayer->InitPlayerInLobby();
    pPlayer->m_pRoom = this;
    pPlayer->m_pTeam = 0;
    pPlayer->m_nTeam = 4;
    m_vStayList.push_back(pPlayer->m_nPlayerSeq);
    return 0;
}

// =============================================================================
// CRoom::DeletePlayerLobby  @08099c78
// =============================================================================
void CRoom::DeletePlayerLobby(CPlayer* pPlayer)
{
    std::vector<int>::iterator it =
        std::find(m_vStayList.begin(), m_vStayList.end(), pPlayer->m_nPlayerSeq);
    if (it != m_vStayList.end())
        m_vStayList.erase(it);
}

// =============================================================================
// CRoom::DeleteRoom  @0809b23e
// =============================================================================
void CRoom::DeleteRoom()
{
    m_nState = 0;   // kind = 0 -> free
    m_vHomeList.clear();
    m_vAwayList.clear();
    m_vViewList.clear();
    m_vStayList.clear();
}

// =============================================================================
// CRoom::GetPlayer  @0809cbfa  - find a player (by seq) in the LOBBY id-list.
// =============================================================================
CPlayer* CRoom::GetPlayer(int nSeq)
{
    for (size_t i = 0; i < m_vStayList.size(); ++i)
    {
        CPlayer* p = GetPlayerPointer(m_vStayList[i]);
        if (p != 0 && p->m_nPlayerSeq == nSeq) return p;
    }
    return 0;
}

// =============================================================================
// CRoom::SetRoom  @0809b402  - validate + apply a room-options change.
// =============================================================================
int CRoom::SetRoom(CCSSetRoom* pPacket)
{
    CCSSetRoom* req = pPacket;
    if (GetAthleteAmount() > 2 * req->m_nScaleCode)            return -1;  // max*2 < current
    if (req->m_nStartLevel > req->m_nEndLevel)                return -2;
    if (GetMinLevel() < req->m_nStartLevel)                   return -3;
    if (GetMaxLevel() > req->m_nEndLevel)                     return -4;
    if (req->m_nMode == 2 && GetMinPoint() < req->m_nPointCode) return -5;

    m_nState        = req->m_nState;
    m_nMode         = req->m_nMode;
    snprintf(m_sTitle, 0x2f, "%s", req->m_sTitle);
    snprintf(m_sPass,  5,    "%s", req->m_sPass);
    m_nAttackCode   = req->m_nAttackCode;
    m_nScaleCode    = req->m_nScaleCode;
    m_nAICode       = req->m_nAICode;
    m_nPointCode    = req->m_nPointCode;
    m_nAreaCode     = req->m_nAreaCode;
    m_nStartLevel   = req->m_nStartLevel;
    m_nEndLevel     = req->m_nEndLevel;
    m_nCheckClub    = req->m_nCheckClub;
    m_nCheckTime    = req->m_nCheckTime;
    m_nCheckWeather = req->m_nCheckWeather;
    m_nCheckView    = req->m_nCheckView;
    m_nCheckViewChat = req->m_nCheckViewChat;
    m_nMaxCount     = req->m_nMaxCount;
    char b6v6 = (m_nScaleCode == 6);   // a "6 vs 6" scale toggles the per-block reserve-pos[4]
    m_cHomeSeat.m_nReservePosition[4] = b6v6;
    m_cAwaySeat.m_nReservePosition[4] = b6v6;
    return 0;
}

// =============================================================================
// CRoom::IsExistInRoom  @0809a0b6  - is the player already on home/away?
// =============================================================================
char CRoom::IsExistInRoom(CPlayer* pPlayer)
{
    for (size_t g = 0; g < m_vAthleteList.size(); ++g)
    {
        std::vector<int>* pV = m_vAthleteList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* p = GetPlayerPointer((*pV)[i]);
            if (p != 0 && p->m_nPlayerSeq == pPlayer->m_nPlayerSeq) return 1;
        }
    }
    return 0;
}

// =============================================================================
// CRoom::IsTeamAndSeatInsert  @0809a1f8  - find a team+seat for a joining player.
// =============================================================================
static bool SeatMatchPos(char nSeatPos, char nPlayerPos, char nOtherPos)
{
    if (nSeatPos == 1) return true;
    if (nPlayerPos == '2' && nOtherPos != '(') return true;
    if (nSeatPos == nPlayerPos) return true;
    if ((int)nSeatPos == (nPlayerPos / 10) * 10) return true;
    return false;
}

char CRoom::IsTeamAndSeatInsert(CPlayer* pPlayer, CTeamSeat* pOut)
{
    char nPos = pPlayer->m_nPosition;

    if (m_nMode == 5)   // boss/raid cource
    {
        if (nPos == '(')
        {
            if (m_cHomeSeat.m_nPlayerSeq[5] == 0) { pOut->m_nTeam = 1; pOut->m_nSeat = 5; return 1; }
            return 0;
        }
        for (int i = 0; i < 6; ++i)
        {
            if (m_cHomeSeat.m_nPlayerSeq[i] == 0 && m_cHomeSeat.m_nReservePosition[i] != 0 &&
                SeatMatchPos(m_cHomeSeat.m_nReservePosition[i], nPos, m_cAwaySeat.m_nReservePosition[i]))
            { pOut->m_nTeam = 1; pOut->m_nSeat = (char)i; return 1; }
        }
        return 0;
    }
    if (nPos == '(')   // keeper: prefer home GK, else away GK
    {
        if (m_cHomeSeat.m_nPlayerSeq[5] == 0) { pOut->m_nTeam = 1; pOut->m_nSeat = 5; return 1; }
        if (m_cAwaySeat.m_nPlayerSeq[5] == 0) { pOut->m_nTeam = 2; pOut->m_nSeat = 5; return 1; }
        return 0;
    }
    // balance: fill the smaller team first.
    bool awayFirst = (m_vAwayList.size() < m_vHomeList.size());
    for (int pass = 0; pass < 2; ++pass)
    {
        bool home = (pass == 0) ? !awayFirst : awayFirst;
        CReserveSeat& blk = home ? m_cHomeSeat : m_cAwaySeat;
        CReserveSeat& oth = home ? m_cAwaySeat : m_cHomeSeat;
        char  team = home ? 1 : 2;
        for (int i = 0; i < 6; ++i)
        {
            if (blk.m_nPlayerSeq[i] == 0 && blk.m_nReservePosition[i] != 0 &&
                SeatMatchPos(blk.m_nReservePosition[i], nPos, oth.m_nReservePosition[i]))
            { pOut->m_nTeam = team; pOut->m_nSeat = (char)i; return 1; }
        }
    }
    return 0;
}

// =============================================================================
// CRoom::IsTeamAndSeatChange  @0809a79e  - find a seat on a chosen team (1/2/view).
// =============================================================================
char CRoom::IsTeamAndSeatChange(CPlayer* pPlayer, int nTeam, CTeamSeat* pOut)
{
    char nPos = pPlayer->m_nPosition;
    if (nTeam == 1)
    {
        if (nPos == '(')
        {
            if (m_cHomeSeat.m_nPlayerSeq[5] == 0) { pOut->m_nTeam = 1; pOut->m_nSeat = 5; return 1; }
            return 0;
        }
        for (int i = 0; i < 6; ++i)
            if (m_cHomeSeat.m_nPlayerSeq[i] == 0 && m_cHomeSeat.m_nReservePosition[i] != 0 &&
                SeatMatchPos(m_cHomeSeat.m_nReservePosition[i], nPos, m_cAwaySeat.m_nReservePosition[i]))
            { pOut->m_nTeam = 1; pOut->m_nSeat = (char)i; return 1; }
        return 0;
    }
    if (nTeam == 2)
    {
        if (nPos == '(')
        {
            if (m_cAwaySeat.m_nPlayerSeq[5] == 0) { pOut->m_nTeam = 2; pOut->m_nSeat = 5; return 1; }
            return 0;
        }
        for (int i = 0; i < 6; ++i)
            if (m_cAwaySeat.m_nPlayerSeq[i] == 0 && m_cAwaySeat.m_nReservePosition[i] != 0 &&
                SeatMatchPos(m_cAwaySeat.m_nReservePosition[i], nPos, m_cHomeSeat.m_nReservePosition[i]))
            { pOut->m_nTeam = 2; pOut->m_nSeat = (char)i; return 1; }
        return 0;
    }
    // viewer (team 3): seats 0..3 for everyone, 4..5 for GMs (operation > 1).
    for (int i = 0; i < 6; ++i)
        if (m_cViewSeat.m_nPlayerSeq[i] == 0 && (i < 4 || pPlayer->m_nOperation > 1))
        { pOut->m_nTeam = 3; pOut->m_nSeat = (char)i; return 1; }
    return 0;
}

// =============================================================================
// CRoom::InsertPlayerRoom  @08099484  - join a player into a team seat.
// =============================================================================
int CRoom::InsertPlayerRoom(CPlayer* pPlayer, int nChange, char* pPass)
{
    if (m_nState == 0)  return -1;                   // room not in use
    if (m_nCource != 0) return -2;                   // state busy

    // memcmp spans 0x15 bytes from m_sPass (deliberately wider than the 5-byte field).
    if (m_nState == 2 && std::memcmp(m_sPass, pPass, 0x15) != 0 && pPlayer->m_nOperation < 2)
        return -3;                                    // wrong password

    if (GetAthleteAmount() >= m_nMaxCount)           return -5;
    if (m_nMode == 4 && GetAthleteAmount() > 2)      return -5;
    if (IsExistInRoom(pPlayer))                      return -6;
    if ((char)pPlayer->m_cLevel.m_nLevel < m_nStartLevel || m_nEndLevel < (char)pPlayer->m_cLevel.m_nLevel) return -7;
    if (m_nMode == 2)  // clan: min point + level 20
    {
        if (pPlayer->m_cMoney.m_nPoint < m_nPointCode) return -8;
        if ((char)pPlayer->m_cLevel.m_nLevel < 0x14)                       return -8;
    }

    CTeamSeat seat;
    if (nChange == 0)
    {
        if (!IsTeamAndSeatInsert(pPlayer, &seat)) return -4;
    }
    else
    {
        if (!IsTeamAndSeatChange(pPlayer, 3, &seat)) return -5;
    }

    pPlayer->InitPlayerInRoom(this);
    pPlayer->m_nTeam   = seat.m_nTeam;
    pPlayer->m_nSeat = seat.m_nSeat;
    int idx = (int)seat.m_nSeat;
    if (pPlayer->m_nTeam == 1)
    {
        m_vHomeList.push_back(pPlayer->m_nPlayerSeq);
        m_cHomeSeat.m_nUsingPosition[idx] = pPlayer->m_nPosition;
        m_cHomeSeat.m_nPlayerSeq[idx] = pPlayer->m_nPlayerSeq;
        m_cHomeSeat.m_nLevel[idx] = pPlayer->m_cLevel.m_nLevel;
        std::memcpy(m_cHomeSeat.m_sName[idx], pPlayer->m_sName, 0xf);
        InsertCardBot(pPlayer, &m_cHomeSeat);
    }
    else if (pPlayer->m_nTeam == 2)
    {
        m_vAwayList.push_back(pPlayer->m_nPlayerSeq);
        m_cAwaySeat.m_nUsingPosition[idx] = pPlayer->m_nPosition;
        m_cAwaySeat.m_nPlayerSeq[idx] = pPlayer->m_nPlayerSeq;
        m_cAwaySeat.m_nLevel[idx] = pPlayer->m_cLevel.m_nLevel;
        std::memcpy(m_cAwaySeat.m_sName[idx], pPlayer->m_sName, 0xf);
        InsertCardBot(pPlayer, &m_cAwaySeat);
    }
    else
    {
        m_vViewList.push_back(pPlayer->m_nPlayerSeq);
        m_cViewSeat.m_nUsingPosition[idx] = pPlayer->m_nPosition;
        m_cViewSeat.m_nPlayerSeq[idx] = pPlayer->m_nPlayerSeq;
        m_cViewSeat.m_nLevel[idx] = pPlayer->m_cLevel.m_nLevel;
        std::memcpy(m_cViewSeat.m_sName[idx], pPlayer->m_sName, 0xf);
    }
    pPlayer->m_bSynch = 0;
    SetTeamJang(0);
    return 0;
}

// =============================================================================
// CRoom::InsertViewerRoom  @08099b42  - join a player as a spectator.
// =============================================================================
int CRoom::InsertViewerRoom(CPlayer* pPlayer, char* pPass)
{
    if (m_nState == 0) return -1;
    if (m_nCource != 0) return -2;
    if (!(GetTeamAmount(0) < 4 || pPlayer->m_nOperation > 1)) return -3;
    // memcmp spans 0x15 bytes from m_sPass (deliberately wider than the 5-byte field).
    if (m_nState == 2 && std::memcmp(m_sPass, pPass, 0x15) != 0 && pPlayer->m_nOperation < 2)
        return -4;

    CTeamSeat seat;
    if (!IsTeamAndSeatChange(pPlayer, 3, &seat)) return -5;

    pPlayer->InitPlayerInRoom(this);
    pPlayer->m_nTeam   = seat.m_nTeam;
    pPlayer->m_nSeat = seat.m_nSeat;
    m_vViewList.push_back(pPlayer->m_nPlayerSeq);
    m_cViewSeat.m_nUsingPosition[(int)seat.m_nSeat] = pPlayer->m_nPosition;
    m_cViewSeat.m_nPlayerSeq[(int)seat.m_nSeat] = pPlayer->m_nPlayerSeq;
    return 0;
}

// =============================================================================
// CRoom::DeletePlayerRoom  @08099d3c  - remove a player from its team seat.
// Returns 1 if the room emptied (and was deleted), else 0 (and -? never; the
// binary returns a bool: room-now-empty).
// =============================================================================
int CRoom::DeletePlayerRoom(CPlayer* pPlayer)
{
    // remove the seq from whichever group-2 sub-list holds it.
    bool bFound = false;
    for (size_t g = 0; g < m_vTotalList.size() && !bFound; ++g)
    {
        std::vector<int>* pV = m_vTotalList[g];
        std::vector<int>::iterator it = std::find(pV->begin(), pV->end(), pPlayer->m_nPlayerSeq);
        if (it != pV->end()) { pV->erase(it); bFound = true; }
    }
    if (!bFound)
    {
        CLogManager log("room.cpp", 0x255, 0, this, "DeletePlayerRoom: not found seq(%d) room(%d)",
                        pPlayer->m_nPlayerSeq, (int)m_nRoomSeq);
        log.LogOut();
    }

    int idx = (int)pPlayer->m_nSeat;
    if (pPlayer->m_nTeam == 1 && m_cHomeSeat.m_nPlayerSeq[idx] == pPlayer->m_nPlayerSeq)
    {
        m_cHomeSeat.m_nUsingPosition[idx] = 0; m_cHomeSeat.m_nPlayerSeq[idx] = 0; m_cHomeSeat.m_nLevel[idx] = 0; m_cHomeSeat.m_sName[idx][0] = 0;
        DeleteCardBot(pPlayer, &m_cHomeSeat);
    }
    else if (pPlayer->m_nTeam == 2 && m_cAwaySeat.m_nPlayerSeq[idx] == pPlayer->m_nPlayerSeq)
    {
        m_cAwaySeat.m_nUsingPosition[idx] = 0; m_cAwaySeat.m_nPlayerSeq[idx] = 0; m_cAwaySeat.m_nLevel[idx] = 0; m_cAwaySeat.m_sName[idx][0] = 0;
        DeleteCardBot(pPlayer, &m_cAwaySeat);
    }
    else if (pPlayer->m_nTeam == 3 && m_cViewSeat.m_nPlayerSeq[idx] == pPlayer->m_nPlayerSeq)
    {
        m_cViewSeat.m_nUsingPosition[idx] = 0; m_cViewSeat.m_nPlayerSeq[idx] = 0; m_cViewSeat.m_nLevel[idx] = 0; m_cViewSeat.m_sName[idx][0] = 0;
    }

    int n = GetTotalAmount();
    if (n == 0) DeleteRoom();
    return (n == 0) ? 1 : 0;
}

// =============================================================================
// CRoom::InsertCardBot @0809986a / DeleteCardBot @080999e2  (cource 4 only)
// For each of the player's enabled card-entries, mirror the card into the team's
// reserve-seat block at the card's per-entry seat index.
// =============================================================================
void CRoom::InsertCardBot(CPlayer* pPlayer, CReserveSeat* pSeat)
{
    if (m_nMode != 4) return;
    int nEntry = pPlayer->m_nCardEntry;
    for (size_t c = 0; c < pPlayer->m_vCardList.size(); ++c)
    {
        CCard* pCard = pPlayer->m_vCardList[c];
        if (pCard == 0 || pCard->m_nState == 0) continue;
        int nSlot = (&pCard->m_nEquip1)[nEntry];   // m_nEquipKind[nEntry], 1-based seat
        if (nSlot == 0) continue;
        int k = nSlot - 1;
        pSeat->m_nPlayerSeq[k]     = pCard->m_nCardSeq;
        pSeat->m_nUsingPosition[k] = pCard->m_nPosition;
        pSeat->m_nLevel[k]         = pCard->m_nLevel;
        snprintf(pSeat->m_sName[k], PLAYER_NAME_SIZE, "Cardbot");
    }
}

void CRoom::DeleteCardBot(CPlayer* pPlayer, CReserveSeat* pSeat)
{
    if (m_nMode != 4) return;
    int nEntry = pPlayer->m_nCardEntry;
    for (size_t c = 0; c < pPlayer->m_vCardList.size(); ++c)
    {
        CCard* pCard = pPlayer->m_vCardList[c];
        if (pCard == 0 || pCard->m_nState == 0) continue;
        int nSlot = (&pCard->m_nEquip1)[nEntry];   // m_nEquipKind[nEntry], 1-based seat
        if (nSlot == 0) continue;
        int k = nSlot - 1;
        pSeat->m_nPlayerSeq[k]     = 0;
        pSeat->m_nUsingPosition[k] = 0;
        pSeat->m_nLevel[k]         = 0;
        pSeat->m_sName[k][0]       = 0;
    }
}

// =============================================================================
// CRoom::GetOptimizedPosition  @0809928a  - snap a bot's role to a legal position.
// =============================================================================
int CRoom::GetOptimizedPosition(int nLevel, int nPos)
{
    int r = nPos;
    if (nLevel < 0x14)
    {
        switch (nPos)
        {
        case 1:  r = (g_Random.Random() % 3 + 1) * 10; break;
        case 10: case 0xb: case 0xc: case 0xd: r = 10; break;
        case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: r = 0x14; break;
        case 0x1e: case 0x1f: case 0x20: case 0x21: r = 0x1e; break;
        case 0x28: r = 0x28; break;
        }
    }
    else
    {
        if (nPos == 1) r = (g_Random.Random() % 3 + 1) * 10;
        if (r == 0x14)      r = g_Random.Random() % 4 + 0x15;
        else if (r == 10)   r = g_Random.Random() % 4 + 0xb;
        else if (r == 0x1e) r = g_Random.Random() % 3 + 0x1f;
        else if (r == 0x28) r = 0x28;
    }
    return r;
}

// =============================================================================
// CRoom::GetRobotInfo  @08099170  - fill a CRobotInfo for a bot in (team,seat).
// =============================================================================
void CRoom::GetRobotInfo(CRobotInfo* pOut, int nTeam, int nSeat, int nLevel, int nCostume)
{
    pOut->m_nTeam = (char)nTeam;
    pOut->m_nSeat = (char)nSeat;
    pOut->m_nRobotSeq = nTeam * 10 + nSeat + 200000000;
    char nAdd;
    if (nLevel < 0xb) nAdd = (char)(g_Random.Random() % 2);
    else              nAdd = (char)(g_Random.Random() % (nLevel / 5));
    pOut->m_nLevel = (char)nLevel + nAdd;
    if (pOut->m_nLevel > '2') pOut->m_nLevel = 0x31;
    char nSlotPos = (nTeam == 1) ? m_cHomeSeat.m_nReservePosition[nSeat]
                                 : m_cAwaySeat.m_nReservePosition[nSeat];
    pOut->m_nPosition = (char)GetOptimizedPosition(nLevel, (int)nSlotPos);
    pOut->m_nCostume = nCostume;
}

// =============================================================================
// CRoom::GetNormalizeLevel  @0809c864  - balanced bot levels for home/away.
// =============================================================================
void CRoom::GetNormalizeLevel(int* pHomeOut, int* pAwayOut)
{
    double dHome = 0.0, dAway = 0.0;
    for (size_t i = 0; i < m_vHomeList.size(); ++i)
    {
        CPlayer* p = GetPlayerPointer(m_vHomeList[i]);
        if (p != 0) dHome += (double)(short)(char)p->m_cLevel.m_nLevel;
    }
    for (size_t i = 0; i < m_vAwayList.size(); ++i)
    {
        CPlayer* p = GetPlayerPointer(m_vAwayList[i]);
        if (p != 0) dAway += (double)(short)(char)p->m_cLevel.m_nLevel;
    }
    int nHome = (int)m_vHomeList.size();
    int nAway = (int)m_vAwayList.size();
    if (nAway < 1) { nAway = nHome; dAway = dHome; }
    if (nHome < 1) { nHome = nAway; dHome = dAway; }

    int nTotal = nAway + nHome;
    double dAvgTotal = (double)nTotal * ((dHome + dAway) / (double)nTotal);
    // _UNK_080bf100 is the binary's balancing coefficient (= 1.0); kept explicit.
    const double kCoef = 1.0;
    *pHomeOut = (int)(((kCoef * ((dAvgTotal - dHome) / (double)(nTotal - nHome))) >= 0)
                      ? (kCoef * ((dAvgTotal - dHome) / (double)(nTotal - nHome))) + 0.5
                      : (kCoef * ((dAvgTotal - dHome) / (double)(nTotal - nHome))) - 0.5);
    *pAwayOut = (int)(((kCoef * ((dAvgTotal - dAway) / (double)(nTotal - nAway))) >= 0)
                      ? (kCoef * ((dAvgTotal - dAway) / (double)(nTotal - nAway))) + 0.5
                      : (kCoef * ((dAvgTotal - dAway) / (double)(nTotal - nAway))) - 0.5);
    if ((int)(dAway + 0.5) < (int)(dHome + 0.5)) *pHomeOut += 1;
    if ((int)(dHome + 0.5) < (int)(dAway + 0.5)) *pAwayOut += 1;
    if (*pHomeOut < 1) { int d = *pHomeOut; if (d < 0) d = -d; *pAwayOut += d; *pHomeOut = 1; }
    if (*pAwayOut < 1) { int d = *pAwayOut; if (d < 0) d = -d; *pHomeOut += d; *pAwayOut = 1; }
    if (*pHomeOut > 0x32) *pHomeOut = 0x32;
    if (*pAwayOut > 0x32) *pAwayOut = 0x32;
}

// =============================================================================
// CRoom game-flow flags  (per-player bytes: ready @0x528, start @0x529, relay @0x510)
// =============================================================================
char CRoom::IsGameReadyFlag()
{
    for (size_t g = 0; g < m_vAthleteList.size(); ++g)
    {
        std::vector<int>* pV = m_vAthleteList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* p = GetPlayerPointer((*pV)[i]);
            if (p != 0 && p->m_bIsGameReady == 0) return 0;
        }
    }
    return 1;
}
char CRoom::IsGameStartFlag()
{
    for (size_t g = 0; g < m_vAthleteList.size(); ++g)
    {
        std::vector<int>* pV = m_vAthleteList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* p = GetPlayerPointer((*pV)[i]);
            if (p != 0 && p->m_bIsGameStart == 0) return 0;
        }
    }
    return 1;
}
void CRoom::InitGameReadyFlag()
{
    for (size_t g = 0; g < m_vAthleteList.size(); ++g)
    {
        std::vector<int>* pV = m_vAthleteList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* p = GetPlayerPointer((*pV)[i]);
            if (p != 0) p->m_bIsGameReady = 0;
        }
    }
}
void CRoom::InitGameStartFlag()
{
    for (size_t g = 0; g < m_vAthleteList.size(); ++g)
    {
        std::vector<int>* pV = m_vAthleteList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* p = GetPlayerPointer((*pV)[i]);
            if (p != 0) p->m_bIsGameStart = 0;
        }
    }
}
void CRoom::InitGameRelayFlag()
{
    for (size_t g = 0; g < m_vAthleteList.size(); ++g)
    {
        std::vector<int>* pV = m_vAthleteList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* p = GetPlayerPointer((*pV)[i]);
            if (p != 0) p->m_nRelay = 0;
        }
    }
}

// =============================================================================
// CRoom::GetRoomInfo(CGMRoomInfo*)  @08099026  - 89-byte GM room snapshot.
// =============================================================================
void CRoom::GetRoomInfo(CGMRoomInfo* pOut)
{
    pOut->m_nState        = m_nState;
    pOut->m_nMode         = m_nMode;
    pOut->m_nCource       = m_nCource;
    pOut->m_nRoomSeq      = (int)m_nRoomSeq;
    pOut->m_nParentSeq    = GetParent();
    pOut->m_nRoomJangTeam = m_nRoomJangTeam;
    pOut->m_nHomeJangSeq  = m_nHomeJangSeq;
    pOut->m_nAwayJangSeq  = m_nAwayJangSeq;
    snprintf(pOut->m_sTitle, 0x2f, "%s", m_sTitle);
    snprintf(pOut->m_sPass,  5,    "%s", m_sPass);
    pOut->m_nQuestCode    = m_nQuestCode;
    pOut->m_nGroundCode   = (char)m_nGroundCode;
    pOut->m_nBallCode     = (char)m_nBallCode;
    pOut->m_nTimeCode     = m_nTimeCode;
    pOut->m_nWeatherCode  = m_nWeatherCode;
    pOut->m_nAttackCode   = m_nAttackCode;
    pOut->m_nScaleCode    = m_nScaleCode;
    pOut->m_nAICode       = m_nAICode;
    pOut->m_nPointCode    = m_nPointCode;
    pOut->m_nStartLevel   = m_nStartLevel;
    pOut->m_nEndLevel     = m_nEndLevel;
    pOut->m_nAttackTeam   = m_nAttackTeam;
    pOut->m_nMaxCount     = m_nMaxCount;
}

// =============================================================================
// =====================  lobby / room-table free functions  ===================
// =============================================================================

// GetRoomPointer @0809ccb4
CRoom* GetRoomPointer(int nNo)
{
    if (nNo < 1 || nNo >= MAX_ROOM_POOL) return 0;
    return &g_RoomPool[nNo];
}

// GetRoomCount @0809d0f6
int GetRoomCount()
{
    int n = 0;
    for (int i = 1; i < MAX_ROOM_POOL; ++i)
        if (g_RoomPool[i].m_nState != 0) ++n;   // kind != 0 -> in use
    return n;
}

// --- room-list filter shared by GetRoomList / GetTotalRoomList ---
static bool RoomMatchesKind(CRoom* p, int nKind)
{
    switch (nKind)
    {
    case 0: return p->m_nMode == 9;
    case 1: return p->m_nMode != 0;
    case 2: return p->GetRoomCource() == 0;
    case 3: return p->m_nMode == 5;
    case 4: return p->m_nMode == 4;
    }
    return true;
}

// GetTotalRoomList @0809d146  - page count for a filtered list (5 rooms/page).
int GetTotalRoomListInternal(int nKind);   // fwd (used by GetRoomList)
int GetTotalRoomListInternal(int nKind)
{
    int nCount = 0;
    for (int i = 199; i >= 1 && i < MAX_ROOM_POOL; --i)
    {
        CRoom* p = &g_RoomPool[i];
        if (p->m_nState == 0) continue;          // not in use
        if (RoomMatchesKind(p, nKind)) ++nCount;
    }
    if (nCount == 0) return 1;
    return (nCount - 1) / 5 + 1;
}

void GetTotalRoomList(int nLobby, int nKind)
{
    (void)nLobby;
    GetTotalRoomListInternal(nKind);
}

// GetRoomList @0809d2ac  - fill one 5-row CSCRoomList page. (args: kind, page, level)
void GetRoomList(int nKind, int nPage, int nLevel, CSCRoomList* pOut)
{
    (void)nLevel;
    for (int i = 0; i < 5; ++i)
        pOut->m_cRoomData[i].m_nState = 0;

    int nStart = nPage * 5;
    if (nStart < 0 || nStart >= 0xc9) return;

    int nSeen = 0;   // matched rooms passed so far
    int nRow  = 0;   // rows written this page
    for (int i = 199; i >= 1 && i < MAX_ROOM_POOL; --i)
    {
        CRoom* p = &g_RoomPool[i];
        if (p->m_nState == 0) continue;
        if (p->GetTotalAmount() < 1) { p->InitRoom(); continue; }   // GC empty room
        if (!RoomMatchesKind(p, nKind)) continue;

        if (nSeen++ < nStart) continue;
        if (nRow > 4) break;

        CRoomData& r = pOut->m_cRoomData[nRow];
        r.m_nState  = p->m_nState;
        r.m_nMode   = p->m_nMode;
        r.m_nCource = (char)p->GetRoomCource();
        r.m_nRoomSeq = (int)p->m_nRoomSeq;
        std::memcpy(r.m_sTitle, p->m_sTitle, 0x2f);
        r.m_nScaleCode = p->m_nScaleCode;
        r.m_nAICode    = p->m_nAICode;
        r.m_nPointCode = p->m_nPointCode;
        r.m_nAreaCode  = p->m_nAreaCode;
        r.m_nStartLevel = p->m_nStartLevel;
        r.m_nEndLevel   = p->m_nEndLevel;
        r.m_nCheckClub  = p->m_nCheckClub;
        r.m_nCheckView  = p->m_nCheckView;
        r.m_nAthleteCount = (char)((char)p->m_vHomeList.size() + (char)p->m_vAwayList.size());
        r.m_nMaxCount   = p->m_nMaxCount;
        r.m_nViewCount  = (char)p->m_vViewList.size();
        r.m_nRoomSpeed  = (short)p->GetRoomSpeed();
        std::memcpy(&r.m_cHomeSeat, &p->m_cHomeSeat, 0x84);
        std::memcpy(&r.m_cAwaySeat, &p->m_cAwaySeat, 0x84);
        ++nRow;
    }

    if (nRow == 0)
        pOut->m_nPage = (char)((nSeen == 0) ? 0xff : 0xfe); // empty / past-end
    else
        pOut->m_nPage = (char)nPage;
    pOut->m_nTotalPage = (char)GetTotalRoomListInternal(nKind);
}

// GetLobbyList @0809da12  - fill one 10-row CSCLobbyList page from g_RoomPool[0].
void GetLobbyList(int nPage, CSCLobbyList* pOut)
{
    for (int i = 0; i < 10; ++i)
        pOut->m_cLobbyData[i].m_nState = 0;

    int nStart = nPage * 10;
    if (nStart < 0 || nStart >= 0x3e9) return;

    CRoom* pLobby = &g_RoomPool[0];
    int nSeen = 0;
    int nRow  = 0;
    for (size_t k = 0; k < pLobby->m_vStayList.size(); ++k)
    {
        CPlayer* p = GetPlayerPointer(pLobby->m_vStayList[k]);
        if (p == 0) continue;
        if (nSeen++ < nStart) continue;
        if (nRow > 9) break;

        CLobbyData& r = pOut->m_cLobbyData[nRow];
        r.m_nState     = 1;
        r.m_nPlayerSeq = p->m_nPlayerSeq;
        r.m_nOperation = p->m_nOperation;
        r.m_nPosition  = p->m_nPosition;
        r.m_nLevel     = p->m_cLevel.m_nLevel;
        std::memcpy(r.m_sName, p->m_sName, 0xf);
        std::memcpy(r.m_sMent, p->m_sMent, 0x2d);
        ++nRow;
    }

    pOut->m_nPage = (char)((nRow == 0) ? 0xff : nPage);
}

// CreateRoom @0809ccfe  - claim the first free room slot and populate it.
CRoom* CreateRoom(int nPlayerSeq, CCSCreateRoom* pPacket)
{
    (void)nPlayerSeq;
    CRoom* pRoom = 0;
    int nNo = 0;
    for (int i = 1; i < MAX_ROOM_POOL; ++i)
    {
        if (g_RoomPool[i].m_nState == 0) { pRoom = &g_RoomPool[i]; nNo = i; break; }
    }
    if (pRoom == 0) return 0;

    pRoom->InitRoom();
    CCSCreateRoom* req = pPacket;
    pRoom->m_nState   = req->m_nState;
    pRoom->m_nMode    = req->m_nMode;
    snprintf(pRoom->m_sTitle, 0x2f, "%s", req->m_sTitle);
    snprintf(pRoom->m_sPass,  5,    "%s", req->m_sPass);
    pRoom->m_nGroundCode = 2;                // ground default
    pRoom->m_nBallCode   = 0;                // ball default
    pRoom->m_nAttackCode = req->m_nAttackCode;
    pRoom->m_nScaleCode  = req->m_nScaleCode;
    pRoom->m_nAICode     = req->m_nAICode;
    pRoom->m_nStartLevel = req->m_nStartLevel;
    pRoom->m_nEndLevel   = req->m_nEndLevel;
    pRoom->m_nCheckClub  = req->m_nCheckClub;
    pRoom->m_nCheckTime  = req->m_nCheckTime;
    pRoom->m_nPointCode  = req->m_nPointCode;
    pRoom->m_nAreaCode   = req->m_nAreaCode;
    pRoom->m_nCheckWeather = req->m_nCheckWeather;
    pRoom->m_nCheckView  = req->m_nCheckView;
    pRoom->m_nCheckViewChat = req->m_nCheckViewChat;
    pRoom->m_nMaxCount   = req->m_nMaxCount;
    pRoom->m_nWeatherCode = 1;
    pRoom->m_nTimeCode   = 0;
    std::memcpy(pRoom->m_cHomeSeat.m_nReservePosition, req->m_nHomePosition, 6);
    std::memcpy(pRoom->m_cAwaySeat.m_nReservePosition, req->m_nAwayPosition, 6);
    pRoom->m_nAttackTeam = 2;
    pRoom->SetAttackTeam();

    CLogManager log(5);
    log.LogOut("CreateRoom: room(%d) cource(%d)\n", nNo, (int)pRoom->m_nMode);
    return pRoom;
}

// ChoiceQuickRoom @0809dc64  - join the first joinable normal/cource-0 room.
void ChoiceQuickRoom(CPlayer* pPlayer)
{
    if (pPlayer == 0 || pPlayer->m_pRoom == 0) return;
    if (pPlayer->m_pRoom->m_nRoomSeq != 0) return;   // must be in the lobby

    for (int i = 1; i < MAX_ROOM_POOL; ++i)
    {
        CRoom* p = &g_RoomPool[i];
        if (p->m_nState == 0) continue;      // free
        if (p->m_nState == 2) continue;      // password-locked kind
        if (p->m_nMode != 0) continue;       // not a normal cource
        if (p->GetRoomCource() != 0) continue;
        if (p->InsertPlayerRoom(pPlayer, 0, 0) >= 0) return;
    }
}

// =============================================================================
// =========================  room broadcast helpers  ==========================
// =============================================================================
// SendRoom @0808242e  - send a packet to every player currently in this room.
// (iterates m_vTotalList = {home,away,view}; only delivers to a player whose room
// number matches, mirroring the binary's same-room guard.)
void SendRoom(CRoom* pRoom, CHeadPacket* pPacket, int nResult)
{
    for (size_t g = 0; g < pRoom->m_vTotalList.size(); ++g)
    {
        std::vector<int>* pV = pRoom->m_vTotalList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* p = GetPlayerPointer((*pV)[i]);
            if (p != 0 && p->m_pRoom != 0 && p->m_pRoom->m_nRoomSeq == pRoom->m_nRoomSeq)
                WriteTCP(p, pPacket, pPacket->m_nBodySize + 0xc);
        }
    }
    (void)nResult;
}

// SendTeam @08082708  - send to one team's id-list (1=home,2=away,else=view).
void SendTeam(CRoom* pRoom, int nTeam, CHeadPacket* pPacket, int nResult)
{
    std::vector<int>* pV;
    if (nTeam == 1)      pV = &pRoom->m_vHomeList;
    else if (nTeam == 2) pV = &pRoom->m_vAwayList;
    else                 pV = &pRoom->m_vViewList;
    for (size_t i = 0; i < pV->size(); ++i)
    {
        CPlayer* p = GetPlayerPointer((*pV)[i]);
        if (p != 0 && p->m_pRoom != 0 && p->m_pRoom->m_nRoomSeq == pRoom->m_nRoomSeq)
            WriteTCP(p, pPacket, pPacket->m_nBodySize + 0xc);
    }
    (void)nResult;
}

// CRoom::SendRoom / CRoom::SendTeam - thin members so the spine can call
// pRoom->SendRoom(...) / pRoom->SendTeam(...). They forward to the free helpers
// above; SendTeam routes to the calling player's own team id-list.
void CRoom::SendRoom(CHeadPacket* pPacket, int nResult)
{
    ::SendRoom(this, pPacket, nResult);
}

void CRoom::SendTeam(CHeadPacket* pPacket, int nResult, CPlayer* pPlayer)
{
    ::SendTeam(this, pPlayer ? pPlayer->GetPlayerTeam() : 0, pPacket, nResult);
}

// =============================================================================
// ====================  global player-lookup helpers  =========================
// =============================================================================
// GetPlayerPointer @0809126e  - linear scan of g_PlayerList by character seq.
CPlayer* GetPlayerPointer(int nCharacterSeq)
{
    for (VectorPlayerList::iterator it = g_PlayerList.begin(); it != g_PlayerList.end(); ++it)
    {
        CPlayer* p = *it;
        if (p != 0 && p->m_nPlayerSeq == nCharacterSeq) return p;
    }
    return 0;
}

// GetPlayerPointerPc @08091374  - linear scan of g_PlayerList by name.
CPlayer* GetPlayerPointerPc(const char* sName)
{
    for (VectorPlayerList::iterator it = g_PlayerList.begin(); it != g_PlayerList.end(); ++it)
    {
        CPlayer* p = *it;
        if (p != 0 && std::strncmp(p->m_sName, sName, 0xf) == 0) return p;
    }
    return 0;
}

// IsPlayerConnect @080918fc
int IsPlayerConnect(int seq)
{
    for (VectorPlayerList::iterator it = g_PlayerList.begin(); it != g_PlayerList.end(); ++it)
    {
        CPlayer* p = *it;
        if (p != 0 && p->m_nPlayerSeq == seq) return 1;
    }
    return 0;
}

// SendAll @08082b6e  - broadcast a packet to every connected player.
void SendAll(CHeadPacket* pPacket, int nResult)
{
    for (VectorPlayerList::iterator it = g_PlayerList.begin(); it != g_PlayerList.end(); ++it)
    {
        CPlayer* p = *it;
        if (p != 0)
            WriteTCP(p, pPacket, pPacket->m_nBodySize + 0xc);
    }
    (void)nResult;
}
