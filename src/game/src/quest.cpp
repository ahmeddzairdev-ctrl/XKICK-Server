// =============================================================================
// quest.cpp - quest catalogue, quest-rooms, quest/mission rewards.
// Reconstructed byte-faithfully from XKICK_Game1 (unstripped) via Ghidra.
//
// TCP handlers (registered in process.cpp):
//   PacketQuestReward   @0807b25e  (0x0A8E)   PacketMissionReward @0807f3b0 (0x0A8F)
//   PacketExecuteQuest  @08081350  (0x0A90)   PacketQuestList     @0807cb64 (0x0DAC)
//   PacketCreateQuest   @0807cbde  (0x0DAE)
// chained sub-info sender (called from PacketPlayerInfo):
//   PacketQuestInfo     @08075ece  (0x0839)
// internal helpers:
//   CreateQuest @0809cf16   PacketRewardMoney @0807b54a (CSCRewardMoney 0x0BC1)
//   CPlayer::GetQuestList @0809080a / GetQuestTotalPage @08090956
//   CSql::UpdateQuest @080b7198 / SetExecuteQuest @080b8204 (rebuild g_Sql.*)
//   CLoad::GetQuestTable @0805abfa (rebuild: Table_Quest.csv handle + columns)
//
// The binary keeps a std::map<int,CQuestTable*> built from Table_Quest.csv; the
// rebuild queries the cached CSV handle (g_Load.GetQuestTable()) by row/column,
// mirroring the skill module. Quest reward triples = (Kind/GiftCode/Amount){1..3}.
//
// Portable: standard C++ only; raw CPlayer/CRoom offsets via PF (matches packet.cpp).
// =============================================================================
#include "Main.h"
#include "Sql.h"
#include "GameLoad.h"
#include "Domain.h"
#include "Player.h"
#include "Room.h"
#include "Global.h"
#include "LogManager.h"
#include <cstring>

#define PF(p,off,ty)  (*reinterpret_cast<ty*>(reinterpret_cast<char*>(p) + (off)))

// cross-module helpers
void PacketChoiceRoom(CPlayer*, CHeadPacket*);   // lobby.cpp
void PacketLevelUp(CPlayer*);                     // lobby.cpp  @0807ab32
int  CPlayer_PutMoneyData(CPlayer*);              // stubs.cpp (money commit)

// level recompute after an exp grant (player/level module). PARTIAL: stubbed until
// that module is reconstructed; UpdateLevelField below still persists the exp.
static void CPlayer_CheckLevelUp(CPlayer* p) { if (p) p->CheckLevelUp(); }

// quest object-reward granters. PARTIAL: these belong to the item/skill/ceremony/
// training modules (CPlayer::Reward{Item,Skill,Ceremony,Training}); reconstructed
// here as documented stubs so the money/exp reward path is fully live. They depend
// on the not-yet-rebuilt item/skill granters (GetItemTable/InsertItemField/...).
static void CPlayer_RewardItem    (CPlayer*, int /*code*/, int /*amount*/, char /*kind*/) {}
static void CPlayer_RewardSkill   (CPlayer*, int /*code*/, char /*kind*/) {}
static void CPlayer_RewardCeremony(CPlayer*, int /*code*/, char /*kind*/) {}
static void CPlayer_RewardTraining(CPlayer*, int /*code*/, char /*kind*/) {}

// =============================================================================
// Quest table (CSV) helpers -- Table_Quest.csv by record index + column.
//   columns: Code, Kind1/GiftCode1/Amount1 .. Kind3/GiftCode3/Amount3, ...
// =============================================================================
static int QF(int idx, const char* col)
{
    return FieldToValue(Table_GetData(g_Load.GetQuestTable(), idx, col));
}
// locate the CSV record index whose "Code" == nCode (-1 if none).
static int QRowOfCode(int nCode)
{
    int h = g_Load.GetQuestTable();
    for (int i = 0; ; ++i)
    {
        tvar_t* code = Table_GetData(h, i, "Code");
        if (code == 0) return -1;
        if (FieldToValue(code) == nCode) return i;
    }
}

// =============================================================================
// CPlayer quest helpers (binary CPlayer:: members) -- the "available quest"
// catalogue is the static quest table, NOT the player's own progress list.
// =============================================================================

// CPlayer::GetQuestTotalPage @08090956 : #pages (6/page) of catalogue quests.
static int CPlayer_GetQuestTotalPage()
{
    int h = g_Load.GetQuestTable();
    int n = 0;
    for (int i = 0; ; ++i)
    {
        tvar_t* code = Table_GetData(h, i, "Code");
        if (code == 0) break;
        ++n;
    }
    return (n % 6 == 0) ? n / 6 : n / 6 + 1;
}

// CPlayer::GetQuestList @0809080a : fill one page (<=6 codes) of the catalogue.
static void CPlayer_GetQuestList(int nPage, CSCQuestList* pPacket)
{
    for (int i = 0; i < 6; ++i)
        pPacket->m_cQuestData[i].m_nCode = 0;

    int nTotal = CPlayer_GetQuestTotalPage();
    if (nPage < 0)               nPage = nTotal - 1;
    else if (nTotal - 1 < nPage) nPage = 0;

    int nSkip = nPage * 6;
    int nSlot = 0, nSeen = 0;
    int h = g_Load.GetQuestTable();
    for (int i = 0; ; ++i)
    {
        tvar_t* code = Table_GetData(h, i, "Code");
        if (code == 0) break;
        if (nSlot > 5) break;
        if (nSkip <= nSeen)
        {
            pPacket->m_cQuestData[nSlot].m_nCode = FieldToValue(code);
            ++nSlot;
        }
        ++nSeen;
    }
    if (pPacket->m_cQuestData[0].m_nCode == 0) pPacket->m_nCurrentPage = 0;
    else                                       pPacket->m_nCurrentPage = (short)nPage;
    pPacket->m_nTotalPage = (short)nTotal;
}

// =============================================================================
// CreateQuest @0809cf16 : claim a free room slot for a quest-room and seed it from
// a CCSCreateQuest packet (mode byte @0x5f = 2 marks the quest room).
// =============================================================================
static CRoom* CreateQuest(int /*nPlayerSeq*/, CHeadPacket* pPacket)
{
    CCSCreateQuest* cs = reinterpret_cast<CCSCreateQuest*>(pPacket);
    CRoom* pRoom = 0;
    for (int i = 1; i < MAX_ROOM_POOL; ++i)
        if (g_RoomPool[i].m_nState == 0) { pRoom = &g_RoomPool[i]; break; }
    if (pRoom == 0) return 0;

    pRoom->InitRoom();
    pRoom->m_nState = cs->m_nState;
    pRoom->m_nMode  = cs->m_nMode;
    StrCopyN(pRoom->m_sTitle, cs->m_sTitle, 0x2f);
    StrCopyN(pRoom->m_sPass,  cs->m_sPass, 5);
    pRoom->m_nGroundCode = 0;
    pRoom->m_nBallCode   = 0;
    pRoom->m_nQuestCode  = cs->m_nQuestCode;
    pRoom->m_nAttackCode = cs->m_nAttackCode; pRoom->m_nScaleCode = cs->m_nScaleCode; pRoom->m_nAICode = cs->m_nAICode;
    pRoom->m_nStartLevel = cs->m_nStartLevel; pRoom->m_nEndLevel  = cs->m_nEndLevel;
    pRoom->m_nCheckClub  = cs->m_nCheckClub; pRoom->m_nCheckTime = cs->m_nCheckTime; pRoom->m_nCheckWeather = cs->m_nCheckWeather;
    pRoom->m_nCheckView  = cs->m_nCheckView; pRoom->m_nCheckViewChat = cs->m_nCheckViewChat; pRoom->m_nMaxCount = cs->m_nMaxCount;
    std::memcpy(pRoom->m_cHomeSeat.m_nReservePosition, cs->m_nHomePosition, 6);
    std::memcpy(pRoom->m_cAwaySeat.m_nReservePosition, cs->m_nAwayPosition, 6);
    pRoom->m_nAttackTeam = 2;
    pRoom->SetAttackTeam();
    return pRoom;
}

// =============================================================================
// PacketQuestInfo @08075ece (0x0839) - chained from PacketPlayerInfo.
// Packs the player's quest-progress rows (14 bytes: code, seq, count, state) in
// pages of 10 (CQuestRow order in the binary is code@0, seq@4, count@8, state@0xa).
// =============================================================================
void PacketQuestInfo(CPlayer* pPlayer)
{
    CSCQuestInfo sc;
    sc.m_nCommand = 0x839;
    char* base = (char*)&sc;
    int   nCount = 0;

    for (size_t i = 0; i < pPlayer->m_vQuestList.size(); ++i)
    {
        CQuestInfo* q = pPlayer->m_vQuestList[i];
        if (q == 0) continue;
        // CQuestRow wire layout (matches original PacketQuestInfo @08075ece and the
        // shared Protocol.h CQuestRow): m_nQuestSeq@0, m_nCode@4, m_nAmount@8, m_nPlayDate@0xa.
        // The rebuild's CQuestInfo domain object reorders these (code first); the WIRE
        // order is fixed and MUST stay seq@0/code@4 or the client reads the quest code
        // out of the seq field (drills never match -> never shown done -> scout-test stalls).
        char* r = base + 0xe + nCount * 14;
        *(int*)(r + 0)   = q->m_nSeq;     // m_nQuestSeq
        *(int*)(r + 4)   = q->m_nCode;    // m_nCode
        *(short*)(r + 8) = q->m_nCount;   // m_nAmount
        *(int*)(r + 0xa) = q->m_nState;   // m_nPlayDate (loader zeroes; original sent playdate)
        if (++nCount == 10)
        {
            sc.m_nBodySize = 0x8e;          // 0xe + 0x80
            sc.m_nCount    = 10;
            sc.m_nResponse = 0;
            SendPlayer(pPlayer, &sc, 0);
            nCount = 0;
        }
    }
    if (nCount != 0)
    {
        sc.m_nBodySize = nCount * 14 + 2;
        sc.m_nCount    = (char)nCount;
        sc.m_nResponse = 0;
        SendPlayer(pPlayer, &sc, 0);
    }
}

// =============================================================================
// PacketQuestList @0807cb64 (0x0DAC)
// =============================================================================
void PacketQuestList(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCQuestList sc;
    sc.m_nCommand  = 0xdac;
    sc.m_nBodySize = 0x1d;
    CPlayer_GetQuestList((int)((CCSQuestList*)pPacket)->m_nPage, &sc);
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// PacketCreateQuest @0807cbde (0x0DAE) - spawn a quest-room, then auto-join it.
// =============================================================================
void PacketCreateQuest(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCCreateQuest sc;
    sc.m_nCommand  = 0xdae;
    sc.m_nBodySize = 1;
    CRoom* pRoom = CreateQuest(pPlayer->m_nPlayerSeq, pPacket);
    if (pRoom == 0)
    {
        sc.m_nResponse = (char)0xff;
        SendPlayer(pPlayer, &sc, -1);
        return;
    }
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);

    CCSChoiceRoom choice;
    choice.m_nRoomSeq = (int)pRoom->m_nRoomSeq;   // CRoom m_nRoomSeq (short@4)
    choice.m_nType    = 0;
    StrCopyN(choice.m_sPass, reinterpret_cast<CCSCreateQuest*>(pPacket)->m_sPass, 5);
    PacketChoiceRoom(pPlayer, (CHeadPacket*)&choice);
}

// =============================================================================
// PacketQuestReward @0807b25e (0x0A8E) - grant a quest's up-to-3 reward triples
// once UpdateQuest accepts the completion. type: 1 item / 2 skill / 3 ceremony /
// 4 training / 5,12 money(point) / 15 exp.
// =============================================================================
void PacketQuestReward(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCQuestReward sc;
    sc.m_nCommand  = 0xa8e;
    sc.m_nBodySize = 1;
    if (pPlayer == 0 || pPacket == 0) return;

    int nCode = (int)((CCSQuestReward*)pPacket)->m_nQuest;
    int row   = QRowOfCode(nCode);
    if (row < 0)
    {
        CLogManager log("quest.cpp", 0x97a, 0, pPlayer, "PacketQuestReward: bad quest(%d)\n", nCode);
        log.LogOut();
        return;
    }

    int nRet = g_Sql.UpdateQuest(pPlayer, (char)nCode);

    static const char* kindCol[3] = { "Kind1", "Kind2", "Kind3" };
    static const char* giftCol[3] = { "GiftCode1", "GiftCode2", "GiftCode3" };
    static const char* amtCol [3] = { "Amount1", "Amount2", "Amount3" };
    for (int i = 0; i < 3; ++i)
    {
        int nType   = QF(row, kindCol[i]);
        int nGift   = QF(row, giftCol[i]);
        int nAmount = QF(row, amtCol[i]);
        switch (nType)
        {
            case 1:  if (nRet == 0) CPlayer_RewardItem(pPlayer, nGift, nAmount, 6); break;
            case 2:  if (nRet == 0) CPlayer_RewardSkill(pPlayer, nGift, 6);   break;
            case 3:  if (nRet == 0) CPlayer_RewardCeremony(pPlayer, nGift, 6); break;
            case 4:  if (nRet == 0) CPlayer_RewardTraining(pPlayer, nGift, 6); break;
            case 5:
            case 12:
                pPlayer->m_cMoney.m_nPoint += nAmount;  // +0xb8 point
                CPlayer_PutMoneyData(pPlayer);
                PacketLevelUp(pPlayer);
                break;
            case 15:
                pPlayer->m_cLevel.m_nExp += nAmount;    // +0xc8 exp
                CPlayer_CheckLevelUp(pPlayer);
                g_Sql.UpdateLevelField(pPlayer);
                PacketLevelUp(pPlayer);
                break;
            default: break;
        }
    }
    sc.m_nResponse = (nRet < 0) ? (char)nRet : 0;
    SendPlayer(pPlayer, &sc, (int)sc.m_nResponse);
}

// =============================================================================
// PacketMissionReward @0807f3b0 (0x0A8F) - pay out the room's active mission
// (room+0x6c holds the rolled mission code; CMission body: seq, count, reward, kind).
// =============================================================================
void PacketMissionReward(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCMissionReward sc;
    sc.m_nCommand  = 0xa8f;
    sc.m_nBodySize = 1;
    CRoom* pRoom = (pPlayer != 0) ? pPlayer->m_pRoom : 0;
    if (pPlayer == 0 || pPacket == 0 || pRoom == 0)
    {
        sc.m_nResponse = (char)0xff;
        CLogManager log("quest.cpp", 0xf9c, 0, pPlayer, "PacketMissionReward: no room(%d)\n", -1);
        log.LogOut();
        if (pPlayer != 0) SendPlayer(pPlayer, &sc, -1);
        return;
    }

    CCSMissionReward* cs = (CCSMissionReward*)pPacket;
    int nReward = cs->m_cMission.m_nReward;
    if (cs->m_cMission.m_nSeq == 0) return;        // no mission code

    if (pRoom->m_nMission == cs->m_cMission.m_nSeq)
    {
        char nKind = cs->m_cMission.m_nKind;
        if (nKind == 12)                           // OBJECT_KIND_POINT
        {
            pPlayer->m_cMoney.m_nPoint += nReward;      // +0xb8
            CPlayer_PutMoneyData(pPlayer);
            PacketLevelUp(pPlayer);
        }
        else if (nKind == 15)                      // OBJECT_KIND_EXP
        {
            if (1000 < nReward)
            {
                CLogManager log("quest.cpp", 0xfb5, 0, pPlayer,
                                "PacketMissionReward: big exp(%d)\n", nReward);
                log.LogOut();
            }
            pPlayer->m_cLevel.m_nExp += nReward;        // +0xc8
            g_Sql.UpdateLevelField(pPlayer);
            CPlayer_CheckLevelUp(pPlayer);
            PacketLevelUp(pPlayer);
        }
        sc.m_nResponse = 0;
        SendPlayer(pPlayer, &sc, 0);
    }
    else
    {
        sc.m_nResponse = (char)-0x0b;
        CLogManager log("quest.cpp", 0xfa9, 0, pPlayer, "PacketMissionReward: mismatch(%d)\n", -0x0b);
        log.LogOut();
        SendPlayer(pPlayer, &sc, (int)sc.m_nResponse);
    }
}

// =============================================================================
// PacketExecuteQuest @08081350 (0x0A90) - mark a quest "in progress" for the
// account and echo back the player's money block.
// =============================================================================
void PacketExecuteQuest(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCExecuteQuest sc;                            // ctor: cmd 0x0A90, body 0x12
    if (pPlayer == 0 || pPacket == 0) return;

    CCSExecuteQuest* cs = (CCSExecuteQuest*)pPacket;
    int nRet = g_Sql.SetExecuteQuest(pPlayer->m_nMemberSeq, (int)cs->m_nQuest);
    if (nRet < 0)
    {
        char nResp;
        if (nRet == -2)
        {
            nResp = (char)-0x0c;
            CLogManager log("quest.cpp", 0x13a1, 0, pPlayer, "PacketExecuteQuest(%d)\n", -0x0c);
            log.LogOut();
        }
        else if (nRet == -1)
        {
            nResp = (char)-0x0b;
            CLogManager log("quest.cpp", 0x139d, 0, pPlayer, "PacketExecuteQuest(%d)\n", -0x0b);
            log.LogOut();
        }
        else nResp = (char)-99;
        sc.m_nResponse = nResp;
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_nQuest          = cs->m_nQuest;
    sc.m_cMoney.m_nCash      = pPlayer->m_cMoney.m_nCash;
    sc.m_cMoney.m_nPoint     = pPlayer->m_cMoney.m_nPoint;
    sc.m_cMoney.m_nCredit    = pPlayer->m_cMoney.m_nCredit;
    sc.m_cMoney.m_nClubPoint = pPlayer->m_cMoney.m_nClubPoint;
    sc.m_nResponse       = 0;
    SendPlayer(pPlayer, &sc, 0);
}
