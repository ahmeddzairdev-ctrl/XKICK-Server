// =============================================================================
// player.cpp - CPlayer: a connected, in-game character session.
// Reconstructed from XKICK_Game1 (unstripped). The full character load chain runs
// through g_Sql.* loaders; the loaded character is registered in g_PlayerHash by
// SetPlayer. Offsets in comments are the binary's CPlayer offsets (see Player.h).
//
//   InitPlayer        @08087426   InitPlayerInLobby @080874ee
//   InitPlayerInTeam  @08087544   SetPlayer         @08087878
//   SetPlayerInfo     @08087aba   GetTCPSecure      @08087264
//   ExitPlayer        @080893d0   DeletePlayer      @080892be
//   ResetPingTime     @080874d2   CheckPlayerUDP    @080897ca
//   InitFaculty/InitOption/InitRecord/InitRanking/InitBaseFaculty/SetEquipWear/
//   SetItemOption/SetTrainingFaculty/CheckFaculty/SetPlayLog -- load helpers.
//
// Portable: std C++ only; OS/socket calls go through g_TCPSocket (connect.cpp).
// =============================================================================
#include "Main.h"
#include "packet.h"
#include "Net.h"
#include "Sql.h"
#include "GameLoad.h"
#include "Domain.h"
#include "Room.h"
#include "LogManager.h"
#include <cstring>
#include <ctime>
#include <algorithm>

// The binary stores the UDP source endpoint (CAddress) at CPlayer+0xec, the TCP
// ping timestamp at +0x520, and the UDP ping timestamp at +0x524. Player.h pins
// the public fields, accessed directly as struct members.

// ---------------------------------------------------------------------------
// GetTCPSecure @08087264 - &m_hTCPSecure (this + 0x530).
// ---------------------------------------------------------------------------
CSecure* CPlayer::GetTCPSecure()
{
    return &m_hTCPSecure;
}

int CPlayer::GetSeq() const
{
    return m_nPlayerSeq;
}

// ---------------------------------------------------------------------------
// ResetPingTime @080874d2 - stamp the TCP ping time (CPlayer+0x520) with now.
// ---------------------------------------------------------------------------
void CPlayer::ResetPingTime()
{
    time((time_t*)&m_tTcpPingTime);   // +0x520
}

// ---------------------------------------------------------------------------
// InitPlayer @08087426 - reset per-connection working state.
// ---------------------------------------------------------------------------
void CPlayer::InitPlayer()
{
    m_nTeam   = 0;        // +0x13
    m_pRoom         = 0;        // +0x18
    m_pTeam         = 0;        // +0x1c
    m_nCheckTCP  = 0;        // +0x50e ping-miss counter
    m_nCheckUDP  = 0;        // +0x50f UDP confirm state
    m_nRelay    = 0;        // +0x510
    m_nPlayCount  = 0;        // +0x518

    m_cRandomShop.InitShop(this);  // +0x408

    m_tUdpPingTime  = 0;        // +0x524 UDP ping time
    m_nAutoPilot  = 0;        // +0x548
    m_bSynch  = 0;        // +0x52d

    memset(&m_cPlayLog, 0, 0x14);  // +0x420 (20 bytes)

    ResetPingTime();
}

// ---------------------------------------------------------------------------
// InitPlayerInLobby @080874ee
// ---------------------------------------------------------------------------
void CPlayer::InitPlayerInLobby()
{
    m_nTeam = 0;   // +0x13
    m_pRoom       = 0;   // +0x18
    m_nRelay  = 0;   // +0x510
}

// ---------------------------------------------------------------------------
// InitPlayerInTeam @08087544
// ---------------------------------------------------------------------------
void CPlayer::InitPlayerInTeam(CTeam* pTeam)
{
    m_pTeam      = pTeam;  // +0x1c
    m_bIsTeamReady = 0;      // +0x52a
}

// ---------------------------------------------------------------------------
// SetPlayer @08087878 - capacity check, load, register in g_PlayerHash.
//   returns 0 on success; -30 if server full; the SetPlayerInfo error otherwise.
// ---------------------------------------------------------------------------
int CPlayer::SetPlayer()
{
    g_Sql.GetServerMaxField(this);

    int nCount = GetPlayerCount();
    if (nCount >= (short)g_Config.m_nConnector)   // g_Config + 0x13c (max connections)
        return -0x1e;                             // -30 : server full

    int nRet = SetPlayerInfo();
    if (nRet >= 0)
    {
        g_PlayerHash[m_nPlayerSeq] = this;     // +0xc -> this
        LOGOUT_PACKET("SetPlayer: seq(%d) players(%d)\n", m_nPlayerSeq, nCount + 1);
        nRet = 0;
    }
    return nRet;
}

// ---------------------------------------------------------------------------
// SetPlayerInfo @08087aba - zero the embedded blocks, load everything, derive.
//   returns 0 ok; the loader error if GetPlayerFields fails; -10 if CheckFaculty.
// ---------------------------------------------------------------------------
int CPlayer::SetPlayerInfo()
{
    InitFaculty(&m_cBaseFaculty);     // +0x104
    InitFaculty(&m_cRaiseFaculty);      // +0x11d
    InitFaculty(&m_cTrainingFaculty);     // +0x14f
    InitOption(&m_cItemOption);                      // +0x168
    InitRecord((CRecord*)m_cTotalRecord.m_nRecord);        // +0x2c0
    InitRecord((CRecord*)m_cQuarterRecord.m_nRecord);        // +0x30c
    InitRecord((CRecord*)m_cCurrentRecord.m_nRecord);         // +0x358
    InitRanking((CRanking*)m_cTotalRanking);         // +0x3a4
    InitRanking((CRanking*)m_cQuarterRanking);         // +0x3d6

    int n = g_Sql.GetPlayerFields(this);
    if (n < 0)
    {
        m_nMemberSeq    = -1;   // +0x08
        m_nPlayerSeq = -1;   // +0x0c
        return n;
    }

    InitBaseFaculty();
    SetEquipWear();
    SetItemOption();
    SetTrainingFaculty();

    if (CheckFaculty() < 0)
        return -10;

    SetPlayLog();
    return 0;
}

// ---------------------------------------------------------------------------
// ExitPlayer @080893d0 - persist + detach from room/lobby + close the socket.
// ---------------------------------------------------------------------------
void CPlayer::ExitPlayer()
{
    m_hTCPSecure.m_bInit = 0;   // +0x538 disable CSecure active flag

    g_Sql.InsertPlayLog(this);
    g_Sql.SetLogOutData(m_nMemberSeq);

    if (m_pRoom != 0)
    {
        // a "lobby" is a room whose m_nState (room+4 short) == 0.
        if (m_pRoom->m_nRoomSeq == 0)
            g_RoomPool[0].DeletePlayerLobby(this);   // g_RoomPool used as lobby ns
        else
            PacketLeaveRoom(this, (CHeadPacket*)3);  // leave reason 3 (disconnect)
    }

    if (m_nPlayerSeq > 0)
    {
        g_Sql.UpdatePlayerFields(this);
        m_cRandomShop.ClearTodayList();
    }

    DeletePlayer();
    m_nState = 0;   // +0x00 : mark slot empty

    Net_Close(m_nFd);   // g_TCPSocket CloseEpoll + CloseSocket
    LOGOUT_PACKET("ExitPlayer: fd(%d) seq(%d)\n", m_nFd, m_nPlayerSeq);
}

// ---------------------------------------------------------------------------
// DeletePlayer @080892be - remove this from g_PlayerList and g_PlayerHash.
// ---------------------------------------------------------------------------
void CPlayer::DeletePlayer()
{
    VectorPlayerList::iterator it = std::find(g_PlayerList.begin(), g_PlayerList.end(), this);
    if (it != g_PlayerList.end())
        g_PlayerList.erase(it);

    MapPlayerHash::iterator h = g_PlayerHash.find(m_nMemberSeq);  // keyed by +0x08
    if (h != g_PlayerHash.end())
        g_PlayerHash.erase(h);
}

// ---------------------------------------------------------------------------
// load helpers
// ---------------------------------------------------------------------------

// InitFaculty @08087272 - zero 25 faculty bytes.
void CPlayer::InitFaculty(CFaculty* pOut)
{
    for (int i = 0; i < ARRAY_FACULTY_SIZE; ++i)
        pOut->m_nFaculty[i] = 0;
}

// InitOption @08087298 - zero option byte 0 + 85 trailing ints (the 0x158 block).
void CPlayer::InitOption(COption* pOut)
{
    pOut->m_nCount = 0;
    for (int i = 0; i < 0x55; ++i)   // 85 ints
        pOut->m_nOption[i] = 0;
}

// InitRecord @0808733e - zero 19 record ints.
void CPlayer::InitRecord(CRecord* pOut)
{
    for (int i = 0; i < ARRAY_RECORD_SIZE; ++i)
        pOut->m_nRecord[i] = 0;
}

// InitRanking @08087368 - zero 25 ranking shorts.
void CPlayer::InitRanking(CRanking* pOut)
{
    for (int i = 0; i < 25; ++i)
        pOut->m_nRanking[i] = 0;
}

// CheckFaculty @0808730a - validity stub (binary always returns 1/ok).
int CPlayer::CheckFaculty()
{
    return 1;
}

// SetPlayLog @08087390 - snapshot session start values into m_cPlayLog once.
void CPlayer::SetPlayLog()
{
    if (m_cPlayLog.m_nExp == 0)   // +0x420
    {
        m_cPlayLog.m_nDate  = g_Sql.GetCurrentTime();   // +0x430
        m_cPlayLog.m_nExp   = m_cLevel.m_nExp;                    // +0x420 <- +0xc8
        m_cPlayLog.m_nCash  = m_cMoney.m_nCash;          // +0x428 <- money cash
        m_cPlayLog.m_nPoint = m_cMoney.m_nPoint;         // +0x42c <- money point
        m_cPlayLog.m_nMatch = m_cTotalRecord.m_nRecord[0] + m_cCurrentRecord.m_nRecord[0]; // +0x424
    }
}

// CheckPlayerUDP @080897ca - UDP-confirm state machine over m_nCheckUDP.
//   100('d') == confirmed -> 0; < 50('2') == still trying -> -1; else -> -2.
int CPlayer::CheckPlayerUDP()
{
    if (m_nCheckUDP == 'd')        // 100
        return 0;
    if ((unsigned char)m_nCheckUDP < '2')  // 50
    {
        m_nCheckUDP += 1;
        return -1;
    }
    return -2;
}

// ---------------------------------------------------------------------------
// InitBaseFaculty @0808755c - seed base faculty from the position table + a
// level-based bonus on the 3 "main" faculties; return the clamped overflow.
// ---------------------------------------------------------------------------
int CPlayer::InitBaseFaculty()
{
    // The binary keys the position table by its Code column (CLoad::GetPositionTable is a
    // std::map<int,CPositionTable*> lookup), NOT by CSV row index. A fresh character has
    // m_nPosition=0 (the position is chosen later, in the scout-test, via CM_GROWUP_CHARACTER);
    // no row has Code 0, so GetPositionTable returns NULL and the base faculty stays ZERO.
    // The old code used m_nPosition as a raw row index, so position 0 read the dummy row 0
    // (Code -1, placeholder "???") -> garbage base faculties that hung the scout-test client.
    int hPos = g_Load.GetPositionTable();
    int row = -1;
    for (int i = 0; ; ++i)
    {
        tvar_t* code = Table_GetData(hPos, i, "Code");
        if (code == 0) break;
        if (FieldToValue(code) == (int)m_nPosition) { row = i; break; }
    }

    const char* pRow = (row >= 0) ? (const char*)Table_GetData(hPos, row, -1) : 0;
    if (pRow == 0)
    {
        memset(m_cBaseFaculty.m_nFaculty, 0, ARRAY_FACULTY_SIZE);   // unset/unknown position
        return 0;
    }

    memcpy(m_cBaseFaculty.m_nFaculty, pRow + 9, 0x12);   // 18 base-faculty bytes

    // level bonus (matches binary): increase = level - min(level,5) for level<=19, else level-20.
    int nLevel = m_cLevel.m_nLevel;
    int ia = (nLevel <= 19) ? ((nLevel <= 4) ? nLevel : 5) : 20;
    char cBonus = (char)(nLevel - ia);

    int nOverflow = 0;
    for (int i = 1; i <= 3; ++i)      // 3 main-faculty indices @ row[1..3]
    {
        unsigned char idx = (unsigned char)pRow[i];
        if (idx > 0x11) continue;
        m_cBaseFaculty.m_nFaculty[idx] += cBonus;
        int nSum = (unsigned char)m_cBaseFaculty.m_nFaculty[idx] + (unsigned char)m_cRaiseFaculty.m_nFaculty[idx];
        if (nSum >= 0x65)             // cap base+cur at 100
        {
            m_cBaseFaculty.m_nFaculty[idx] = (char)(100 - (unsigned char)m_cRaiseFaculty.m_nFaculty[idx]);
            nOverflow += (nSum - 100);
        }
    }
    return nOverflow;
}

// ---------------------------------------------------------------------------
// SetEquipWear @0808b382 - build the equip-wear int block from default appearance
// + equipped inventory items, keyed by each item's slot (table column 0x34).
//   returns 0 ok, -2 on a missing item-table row.
// ---------------------------------------------------------------------------
int CPlayer::SetEquipWear()
{
    int* pWear = m_nEquipWear;   // +0x434
    for (int i = 0; i < 0x11; ++i) pWear[i] = 0;   // 17 ints

    int* pAppear = m_nFreeWear;    // +0x478  m_nFreeWear[5] (face/hair/shirts/pants/shoes)

    // Map each appearance item to its wear slot via the parsed item table
    // (binary @0808b382 uses CLoad::GetItemTable(code)->m_nWear, NOT the raw -1 row).
    for (int i = 0; i < 5; ++i)
    {
        if (pAppear[i] == 0) continue;
        CItemTable* t = g_Load.GetItemTable(pAppear[i]);
        if (t == 0) { LOGOUT_ERROR("SetEquipWear: no item table(%d)\n", pAppear[i]); continue; }
        if (t->m_nWear >= 0 && t->m_nWear < 17) pWear[t->m_nWear] = pAppear[i];
    }

    // gender-default body item in slot 5.
    pWear[5] = (m_cShape.m_nGender == 1) ? 203001101 : 203002101;

    for (size_t i = 0; i < m_vItemList.size(); ++i)
    {
        CItem* pItem = m_vItemList[i];
        if (pItem == 0) continue;
        if (pItem->m_nState == 0) continue;     // empty state (m_nState@0x28)
        int  nCode  = pItem->m_nCode;           // m_nCode@0x04
        char nEquip = pItem->m_nEquipKind;      // m_nEquipKind@0x08
        if (nCode == 0 || nEquip == 0) continue;
        CItemTable* t = g_Load.GetItemTable(nCode);
        if (t == 0) { LOGOUT_ERROR("SetEquipWear: no item table(%d)\n", nCode); continue; }
        if (t->m_nWear >= 0 && t->m_nWear < 17) pWear[t->m_nWear] = nCode;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// SetItemOption @0808b60a - collect every non-zero item option code into the
// option block (CPlayer+0x168), set the leading count byte.
// ---------------------------------------------------------------------------
int CPlayer::SetItemOption()
{
    InitOption(&m_cItemOption);   // +0x168
    int nCount = 0;
    int* pOut = m_cItemOption.m_nOption;            // +0x16c (option block)

    for (size_t i = 0; i < m_vItemList.size(); ++i)
    {
        CItem* pItem = m_vItemList[i];
        if (pItem == 0) continue;
        if (pItem->m_nState == 0) continue;               // empty state
        // skip items whose equip/limit/grade region (the 4 bytes at 0x08) is all
        // zero - the binary read this as one int; m_pad0b is the 4th (always 0) byte.
        if (pItem->m_nEquipKind == 0 && pItem->m_nLimit == 0 &&
            pItem->m_nGrade == 0 && pItem->m_pad0b == 0)  continue;
        int* pOpt = pItem->m_nOptionCode;
        for (int j = 0; j < 5; ++j)
            if (pOpt[j] != 0)
                pOut[nCount++] = pOpt[j];
    }

    m_cItemOption.m_nCount = (char)nCount;     // +0x168
    return 0;
}

// ---------------------------------------------------------------------------
// SetTrainingFaculty @0808b72e - accumulate each training's level into the
// training-faculty block (CPlayer+0x14f), indexed by (training type - 10).
// ---------------------------------------------------------------------------
int CPlayer::SetTrainingFaculty()
{
    InitFaculty(&m_cTrainingFaculty);   // +0x14f

    for (size_t i = 0; i < m_vTrainingList.size(); ++i)
    {
        CTraining* pT = m_vTrainingList[i];
        if (pT == 0 || pT->m_nState == 0) continue;
        int nType = pT->m_nType - 10;
        if (nType < 0 || nType > 0x18) continue;
        m_cTrainingFaculty.m_nFaculty[nType] += pT->m_nLevel;   // block @ +0x14f
    }
    return 0;
}
