// =============================================================================
// card.cpp - Game CARD handlers (info / update / equip / divest / credit /
// cardbooster / entry / reward / mix).
// Reconstructed byte-faithfully from XKICK_Game1 (unstripped) via Ghidra.
//
// The runtime CCard (Domain.h) is a 23-byte (0x17) object; the wire row is the whole
// object memcpy'd. Heavyweight card logic (CPlayer::EquipCard/DivestCard/MixCard/
// GetBagCard/GetCardCount, CSql::InsertCardField/InsertSaleLog, the GetCard*
// generators) lives in the player module / shared helpers and is forward-declared.
//
// Portable: standard C++ only; request packets are accessed through their wire
// structs (CCS*), and the AI-card booster table row through CAICardRow.
// =============================================================================
#include "Main.h"
#include "GameProtocolItem.h"
#include "Sql.h"
#include "GameLoad.h"
#include "Domain.h"
#include "LogManager.h"
#include <cstring>

// AI-card booster table row (returned as void* by GetAICardTable). Only the
// fields the booster path reads are named; the rest is reserved padding so the
// named members land at their established offsets.
struct CAICardRow
{
    char m_reserved0[5];
    char m_nFixedPos;     // +0x05
    char m_nFixedArea;    // +0x06
    char m_reserved7;
    int  m_nPointPrice;   // +0x08
    int  m_nCashPrice;    // +0x0c
};

// g_Sql / g_Load are global OBJECTS (Global.h, pulled in via Main.h).

// ---- CPlayer card logic owned by the player module (forward-declared) ----
int    CPlayer_EquipCard   (CPlayer*, int nCardSeq, int nSlot, int nValue); // _ZN7CPlayer9EquipCardEicc
int    CPlayer_DivestCard  (CPlayer*, int nCardSeq);                        // _ZN7CPlayer10DivestCardEi
CCard* CPlayer_GetBagCard  (CPlayer*, int nCardSeq);                        // _ZN7CPlayer10GetBagCardEi
int    CPlayer_GetCardCount(CPlayer*);                                      // _ZN7CPlayer12GetCardCountEv
int    CPlayer_GetMoneyData(CPlayer*);                                      // _ZN7CPlayer12GetMoneyDataEv
int    CPlayer_PutMoneyData(CPlayer*);                                      // _ZN7CPlayer12PutMoneyDataEv
int    CPlayer_MixCard     (CPlayer*, CCSMixCard1*, char* pCardInfo);       // _ZN7CPlayer7MixCardEP11CCSMixCard1P9CCardInfo

// ---- card generators (free functions; mirror the binary's _Z* helpers) ----
char   GetCardLevel();              // _Z12GetCardLevelv
char   GetCardRank(int);            // _Z11GetCardRanki
char   GetCardPosition(int);        // _Z15GetCardPositioni
char   GetCardArea();               // _Z11GetCardAreav
char   GetCardSkill();              // _Z12GetCardSkillv
int    GetCardCostume();            // _Z14GetCardCostumev
int    MakeCardCode(int, int, int, int); // _Z12MakeCardCodeiiii
int    Random_Random();             // _ZN7CRandom6RandomEv (&g_Random)

// AI-card table row lookup (mirrors CLoad::GetAICardTable @080..; the reconstructed
// CLoad uses CSV handles, so the row pointer is fetched via this module helper).
// Row fields used: +5 fixedPosition(char), +6 fixedArea(char), +8 pointPrice(int),
// +0xc cashPrice(int).  Returns 0 if the code is unknown.
void*  GetAICardTable(int nCode);   // _ZN5CLoad14GetAICardTableEi

// =============================================================================
// CARD INFO  (PacketCardInfo @08080512, CM_CARD_INFO 0x083A)
// Walks m_vCardList, packing up to 30 (0x1e) raw 0x17-byte cards per CSCCardInfo.
// =============================================================================
void PacketCardInfo(CPlayer* pPlayer, CHeadPacket* /*pPacket*/)
{
    CSCCardInfo sc;
    const int BASE   = 0xe;          // resp + count
    const int STRIDE = 0x17;         // raw CCard size
    int  nCount = 0;
    char* pRows = (char*)sc.m_cCardData;

    for (size_t i = 0; i < pPlayer->m_vCardList.size(); ++i)
    {
        CCard* pCard = pPlayer->m_vCardList[i];
        if (pCard == 0 || pCard->m_nState == 0)
            continue;

        memcpy(pRows + nCount * STRIDE, pCard, STRIDE);
        if (++nCount == 0x1e)
        {
            sc.m_nBodySize = BASE + 0x2a6;     // 30*0x17 + header (matches binary)
            sc.m_nCount    = 0x1e;
            sc.m_nResponse = 0;
            SendPlayer(pPlayer, &sc, 0);
            nCount = 0;
        }
    }
    sc.m_nBodySize = nCount * STRIDE + BASE - 0xc;
    sc.m_nCount    = (char)nCount;
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// UPDATE CARD  (PacketUpdateCard @080806f4, CM_UPDATE_CARD 0x0E11)
// =============================================================================
void PacketUpdateCard(CPlayer* pPlayer, CSCUpdateCard* pPacket)
{
    pPacket->m_nResponse = 0;
    SendPlayer(pPlayer, pPacket, (int)pPacket->m_nResponse);
}

// =============================================================================
// EQUIP CARD  (PacketEquipCard @0808071c, CM_EQUIP_CARD 0x0E12)
// =============================================================================
void PacketEquipCard(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCEquipCard sc;
    if (pPlayer == 0 || pPacket == 0)
        return;
    CCSEquipCard* req = (CCSEquipCard*)pPacket;
    char nSlot = req->m_nSlot;
    if (nSlot >= 3)
        return;

    int nCardSeq = req->m_nCardSeq;
    char nValue  = req->m_nValue;
    int  nRet = CPlayer_EquipCard(pPlayer, nCardSeq, nSlot, nValue);
    if (nRet < 0)
    {
        char nResp = (char)nRet;
        if (nRet == -2)      nResp = (char)0xf4;
        else if (nRet == -1) { nResp = (char)0xf5;
            LOGOUT_ERROR("PacketEquipCard: EquipCard=-1 seq(%d)\n", pPlayer->m_nPlayerSeq); }
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_nCardSeq = nCardSeq;
    sc.m_nValue   = req->m_nValue;
    sc.m_nSlot    = req->m_nSlot;
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// DIVEST CARD  (PacketDivestCard @0808086a, CM_DIVEST_CARD 0x0E13)
// =============================================================================
void PacketDivestCard(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCDivestCard sc;
    int nCardSeq = ((CCSDivestCard*)pPacket)->m_nCardSeq;
    int nRet = CPlayer_DivestCard(pPlayer, nCardSeq);
    if (nRet < 0)
    {
        char nResp = (char)nRet;
        if (nRet == -2)      nResp = (char)0xf4;
        else if (nRet == -1) { nResp = (char)0xf5;
            LOGOUT_ERROR("PacketDivestCard: DivestCard=-1 seq(%d)\n", pPlayer->m_nPlayerSeq); }
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_nCardSeq = nCardSeq;
    sc.m_nEquipKind = 0;
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// CREDIT CARD  (PacketCreditCard @0808097a, CM_CREDIT_CARD 0x0E14)
// Takes a freshly-built runtime CCard; memcpy 0x17 bytes onto the wire. The seq
// field (CCard+0) doubles as a reason code: negative -> failure (frees the card).
// =============================================================================
void PacketCreditCard(CPlayer* pPlayer, CCard* pCard)
{
    CSCCreditCard sc;
    if (pPlayer == 0)
        return;
    memcpy(&sc.m_cCardData, pCard, 0x17);
    if (pCard->m_nCardSeq < 0)
    {
        sc.m_nResponse = (char)pCard->m_nCardSeq;
        SendPlayer(pPlayer, &sc, (int)sc.m_nResponse);
        delete pCard;
        return;
    }
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// REWARD CARD  (PacketRewardCard @08080a88, CM_REWARD_CARD 0x0E19) -- same shape.
// =============================================================================
void PacketRewardCard(CPlayer* pPlayer, CCard* pCard)
{
    CSCRewardCard sc;
    if (pPlayer == 0)
        return;
    memcpy(&sc.m_cCardData, pCard, 0x17);
    if (pCard->m_nCardSeq < 0)
    {
        sc.m_nResponse = (char)pCard->m_nCardSeq;
        SendPlayer(pPlayer, &sc, (int)sc.m_nResponse);
        delete pCard;
        return;
    }
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// CARD ENTRY  (PacketCardEntry @08080a1c, CM_CARD_ENTRY 0x0E17)
// Selects the active card-entry slot (0..2) stored at CPlayer+0x514.
// =============================================================================
void PacketCardEntry(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCCardEntry sc;
    if (pPlayer == 0 || pPacket == 0)
        return;
    char nEntry = ((CCSCardEntry*)pPacket)->m_nEntry;
    if (nEntry < 3)
        pPlayer->m_nCardEntry = (int)nEntry;
    sc.m_nEntry    = (char)pPlayer->m_nCardEntry;
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// BUY CARDBOOSTER  (PacketBuyCardbooster @0808173c, CM_BUY_CARDBOOSTER 0x0E15)
// Buys an AI-card booster: validate price (cash or point), spend, then generate a
// new CCard (level/rank/position/area/skill/costume), persist, push, log the sale.
// =============================================================================
void PacketBuyCardbooster(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCBuyCardbooster sc;
    if (pPlayer == 0 || pPacket == 0)
        return;

    int nResult = 0;
    if (CPlayer_GetCardCount(pPlayer) >= 0x65)   // inventory full (>=101)
    {
        nResult = -3;
        goto reply;
    }
    {
        CCSBuyCardbooster* req = (CCSBuyCardbooster*)pPacket;
        CAICardRow* pTable = (CAICardRow*)GetAICardTable(req->m_nCode);
        if (pTable == 0) { nResult = -1; goto reply; }

        int  nPrice;
        char nPay = req->m_nPayKind;
        if (nPay == 1)                          // pay with cash
        {
            nPrice = pTable->m_nCashPrice;
            if (req->m_nPrice != nPrice) { nResult = -12; goto reply; }
            if (CPlayer_GetMoneyData(pPlayer) < 0)    { nResult = -13; goto reply; }
            if (pPlayer->m_cMoney.m_nCash < nPrice)   { nResult = -14; goto reply; }
            pPlayer->m_cMoney.m_nCash -= nPrice;
            if (CPlayer_PutMoneyData(pPlayer) < 0)    { nResult = -13; goto reply; }
        }
        else                                    // pay with point
        {
            nPrice = pTable->m_nPointPrice;
            if (req->m_nPrice != nPrice) { nResult = -22; goto reply; }
            if (CPlayer_GetMoneyData(pPlayer) < 0)    { nResult = -23; goto reply; }
            if (pPlayer->m_cMoney.m_nPoint < nPrice)  { nResult = -24; goto reply; }
            pPlayer->m_cMoney.m_nPoint -= nPrice;
            if (CPlayer_PutMoneyData(pPlayer) < 0)    { nResult = -23; goto reply; }
        }

        CCard* pCard = new CCard();
        if (pCard == 0) { nResult = -1; goto reply; }
        pCard->m_nState   = 1;
        pCard->m_nCardSeq = 0;
        pCard->m_nEquip1  = 0;
        pCard->m_nEquip2  = 0;
        pCard->m_nEquip3  = 0;
        pCard->m_nLevel   = GetCardLevel();
        pCard->m_nRank    = GetCardRank(-1);

        char nFixedPos = pTable->m_nFixedPos;
        if (nFixedPos == 0)
            pCard->m_nPosition = GetCardPosition((int)(char)pCard->m_nLevel);
        else
        {
            int nRand = Random_Random() % 5;
            pCard->m_nPosition = (char)nFixedPos + (char)nRand;
            if (pCard->m_nPosition == 0x22)         // '"' -> 0x28
                pCard->m_nPosition = 0x28;
            if ((char)pCard->m_nLevel < 0x14)       // round down to 10s below lv20
                pCard->m_nPosition = (pCard->m_nPosition / 10) * 10;
        }

        char nFixedArea = pTable->m_nFixedArea;
        pCard->m_nArea = (nFixedArea == 0) ? GetCardArea() : nFixedArea;

        pCard->m_nSkill   = GetCardSkill();
        pCard->m_nTired   = 0;
        pCard->m_nCostume = GetCardCostume();
        // (CCard+0x16 trailing pad already 0 from ctor)

        pCard->m_nCode = MakeCardCode(0x2bd,
                                      (int)(char)pCard->m_nPosition,
                                      (int)(char)pCard->m_nRank,
                                      (int)(char)pCard->m_nLevel);

        pCard->m_nCardSeq = g_Sql.InsertCardField(pPlayer, pCard);
        if (pCard->m_nCardSeq < 0)
        {
            delete pCard;
            nResult = -2;
            goto reply;
        }
        pPlayer->m_vCardList.push_back(pCard);

        // sale log: {seq, code, kind 5, buyKind from pkt+0x10, sale 1, price, amount 1}
        CSale sale;
        memset(&sale, 0, sizeof(sale));
        sale.m_nObjectSeq  = pCard->m_nCardSeq;
        sale.m_nObjectCode = pCard->m_nCode;
        sale.m_nObjectKind = 5;
        sale.m_nBuyKind    = req->m_nPayKind;
        sale.m_nSaleKind   = 1;
        sale.m_nPrice      = nPrice;
        sale.m_nAmount     = 1;
        g_Sql.InsertSaleLog(pPlayer, &sale);

        memcpy(&sc.m_cCardData, pCard, 0x17);
        sc.m_cMoney.m_nCash      = pPlayer->m_cMoney.m_nCash;   // rebuild: binary copies CPlayer m_cMoney
        sc.m_cMoney.m_nPoint     = pPlayer->m_cMoney.m_nPoint;
        sc.m_cMoney.m_nCredit    = pPlayer->m_cMoney.m_nCredit;
        sc.m_cMoney.m_nClubPoint = pPlayer->m_cMoney.m_nClubPoint;
        nResult = 0;
    }
reply:
    sc.m_nResponse = (char)nResult;
    SendPlayer(pPlayer, &sc, (int)sc.m_nResponse);
}

// =============================================================================
// MIX CARD  (PacketMixCard1 @08080b36, CM_MIX_CARD1 0x0E7F)
// Fuse up to 12 ingredient cards into a new card; on success the ingredients are
// invalidated (state = 0). PacketMixCard2 @08080bfa is a no-op stub.
// =============================================================================
void PacketMixCard1(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCMixCard1 sc;
    if (pPlayer == 0 || pPacket == 0)
        return;
    int nRet = CPlayer_MixCard(pPlayer, (CCSMixCard1*)pPacket, sc.m_cCardInfo);
    if (nRet == 0)
    {
        for (int i = 0; i < 12; ++i)
        {
            int nSeq = ((CCSMixCard1*)pPacket)->m_nCardSeq[i];
            if (nSeq != 0)
            {
                CCard* pCard = CPlayer_GetBagCard(pPlayer, nSeq);
                if (pCard)
                    pCard->m_nState = 0;     // consume ingredient
            }
        }
    }
    sc.m_nResponse = (char)nRet;
    SendPlayer(pPlayer, &sc, (int)sc.m_nResponse);
}

void PacketMixCard2(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCMixCard2 sc;
    if (pPlayer == 0 || pPacket == 0)
        return;
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// CARD GENERATORS  (binary _Z* helpers; all draw g_Random via Random_Random()).
// =============================================================================
char GetCardLevel()                       // _Z12GetCardLevelv @08097624
{
    int r = Random_Random() % 100;
    if (r == 0)     return 0x32;                                // 50
    if (r < 5)      return (char)(Random_Random() % 10 + 0x28); // 40..49
    if (r < 0xf)    return (char)(Random_Random() % 10 + 0x1e); // 30..39
    if (r < 0x23)   return (char)(Random_Random() % 10 + 0x14); // 20..29
    if (r < 0x41)   return (char)(Random_Random() % 10 + 10);   // 10..19
    return (char)(Random_Random() % 9 + 1);                     // 1..9
}

char GetCardRank(int nLevel)              // _Z11GetCardRanki @08097768
{
    int r = Random_Random() % 100;
    switch (nLevel) {
    case 0:  return (char)((r < 0x5a) ? 0 : 1);
    case 1:  if (r < 10) return 0; if (r < 0x5a) return 1; return 2;
    case 2:  if (r < 10) return 1; if (r < 0x5a) return 2; return 3;
    case 3:  if (r < 10) return 2; if (r < 0x5a) return 3; return 4;
    case 4:  return (char)((r < 0x1e) ? 3 : 4);
    case -1: if (r < 0x3c) return 0; if (r < 0x5f) return 1; return 2;
    default: return 0;
    }
}

char GetCardPosition(int nLevel)          // _Z15GetCardPositioni @08097898
{
    Random_Random(); Random_Random();          // two warm-up draws (faithful)
    if (nLevel < 0x14) {
        int a = Random_Random();
        int b = Random_Random();
        int pos = b % 5 + (a % 3) * 10 + 10;
        if (pos == 0x22) pos = 0x28;
        pos = (pos / 10) * 10;                  // round down to tens
        return (char)pos;
    } else {
        int a = Random_Random();
        int b = Random_Random();
        int pos = (b % 4) + (a % 3) * 10 + 0xb;
        if (pos == 0x22) pos = 0x28;
        return (char)pos;
    }
}

char GetCardArea()                        // _Z11GetCardAreav @08097b0c
{ return (char)(Random_Random() % 0xc + 1); }   // 1..12

char GetCardSkill()                       // _Z12GetCardSkillv @08097b3c
{
    int r = Random_Random() % 100;
    if (r < 10)    return 3;
    if (r < 0x1e)  return 2;
    if (r < 0x3c)  return 1;
    return 0;
}

int GetCardCostume()                      // _Z14GetCardCostumev @08097b9e
{ return Random_Random() % 0x3d; }              // 0..60

int MakeCardCode(int a, int b, int c, int d)    // _Z12MakeCardCodeiiii @0809759c
{ return a * 1000000 + b * 1000 + c * 100 + d; }

// =============================================================================
// CARD INVENTORY HELPERS  (CPlayer card members; walk m_vCardList @0x4dc).
// =============================================================================
int CPlayer_GetCardCount(CPlayer* p)      // CPlayer::GetCardCount @08090fba
{
    int n = 0;
    for (size_t i = 0; i < p->m_vCardList.size(); ++i) {
        CCard* pCard = p->m_vCardList[i];
        if (pCard != 0 && pCard->m_nState != 0) ++n;
    }
    return n;
}

CCard* CPlayer_GetBagCard(CPlayer* p, int nCardSeq)   // CPlayer::GetBagCard @0809071a
{
    for (size_t i = 0; i < p->m_vCardList.size(); ++i) {
        CCard* pCard = p->m_vCardList[i];
        if (pCard == 0) return 0;                       // first NULL slot ends the walk
        if (pCard->m_nState != 0 && pCard->m_nCardSeq == nCardSeq) return pCard;
    }
    return 0;
}

int CPlayer_EquipCard(CPlayer* p, int nCardSeq, int nSlot, int nValue)  // @0808fe82
{
    CCard* pFound = 0;
    for (size_t i = 0; i < p->m_vCardList.size(); ++i) {
        CCard* pCard = p->m_vCardList[i];
        if (pCard == 0) return -1;
        if (pCard->m_nCardSeq == nCardSeq) { pFound = pCard; break; }
    }
    if (pFound == 0) return -2;
    // equip byte chosen by the active card-entry index (CPlayer m_nCardEntry), not nSlot.
    int nEntry = p->m_nCardEntry;
    (&pFound->m_nEquip1)[nEntry] = (char)nValue;        // m_nEquip1/2/3 by entry (0..2)
    if (pFound->m_nState != 0) pFound->m_nState = 2;
    (void)nSlot;
    return 0;
}

int CPlayer_DivestCard(CPlayer* p, int nCardSeq)        // CPlayer::DivestCard @0808ff98
{
    bool bFound = false;
    for (size_t i = 0; i < p->m_vCardList.size(); ++i) {
        CCard* pCard = p->m_vCardList[i];
        if (pCard == 0) return (nCardSeq != 0 && !bFound) ? -2 : 0;
        if (pCard->m_nCardSeq == nCardSeq) {
            pCard->m_nEquip1 = 0; pCard->m_nEquip2 = 0; pCard->m_nEquip3 = 0;
            if (pCard->m_nState != 0) pCard->m_nState = 2;
            bFound = true;
            break;
        }
        if (nCardSeq == 0) {                            // nCardSeq==0 => divest ALL
            pCard->m_nEquip1 = 0; pCard->m_nEquip2 = 0; pCard->m_nEquip3 = 0;
            if (pCard->m_nState != 0) pCard->m_nState = 2;
        }
    }
    if (nCardSeq != 0 && !bFound) return -2;
    return 0;
}

// =============================================================================
// MIX CARD  (CPlayer::MixCard @08090360)  -- fuse 3 equal-rank cards into a new one.
// =============================================================================
int CPlayer_MixCard(CPlayer* p, CCSMixCard1* pIn, char* pCardInfo)
{
    CCard* p0 = CPlayer_GetBagCard(p, pIn->m_nCardSeq[0]);
    if (p0 == 0) { LOGOUT_ERROR("MixCard: ingredient0 not found seq(%d)\n", pIn->m_nCardSeq[0]); return -1; }
    CCard* p1 = CPlayer_GetBagCard(p, pIn->m_nCardSeq[1]);
    if (p1 == 0) { LOGOUT_ERROR("MixCard: ingredient1 not found seq(%d)\n", pIn->m_nCardSeq[1]); return -1; }
    CCard* p2 = CPlayer_GetBagCard(p, pIn->m_nCardSeq[2]);
    if (p2 == 0) { LOGOUT_ERROR("MixCard: ingredient2 not found seq(%d)\n", pIn->m_nCardSeq[2]); return -1; }

    if (p0->m_nRank != p1->m_nRank) return -2;
    if (p0->m_nRank != p2->m_nRank) return -2;

    CCard* pCard = new CCard();
    if (pCard == 0) return -1;
    pCard->m_nState    = 1;
    pCard->m_nCardSeq  = 0;
    pCard->m_nEquip1   = 0;
    pCard->m_nEquip2   = 0;
    pCard->m_nEquip3   = 0;
    pCard->m_nLevel    = GetCardLevel();
    pCard->m_nRank     = GetCardRank((int)(char)p0->m_nRank);
    pCard->m_nPosition = GetCardPosition((int)(char)pCard->m_nLevel);
    pCard->m_nArea     = GetCardArea();
    pCard->m_nSkill    = GetCardSkill();
    pCard->m_nTired    = 0;
    pCard->m_nCostume  = GetCardCostume();
    pCard->m_pad16     = 0;
    pCard->m_nCode     = MakeCardCode(0x2bd,
                                      (int)(char)pCard->m_nPosition,
                                      (int)(char)pCard->m_nRank,
                                      (int)(char)pCard->m_nLevel);

    pCard->m_nCardSeq = g_Sql.InsertCardField(p, pCard);
    if (pCard->m_nCardSeq < 0) { delete pCard; return -2; }
    p->m_vCardList.push_back(pCard);

    CSale sale;
    memset(&sale, 0, sizeof(sale));
    sale.m_nObjectSeq  = pCard->m_nCardSeq;
    sale.m_nObjectCode = pCard->m_nCode;
    sale.m_nObjectKind = 5;
    sale.m_nBuyKind    = 0xc;
    sale.m_nSaleKind   = 0xb;
    sale.m_nPrice      = 0;
    sale.m_nAmount     = 1;
    g_Sql.InsertSaleLog(p, &sale);

    memcpy(pCardInfo, pCard, 0x17);
    return 0;
}

// =============================================================================
// AI CARD TABLE  (CLoad::GetAICardTable @08053282; rows from Table_AI_Card.csv)
// Row: +0x00 Code, +0x04 Type, +0x05 Position(fixed;0=roll), +0x06 Area(fixed;0=roll),
//      +0x08 Point price, +0x0c Cash price (the offsets card.cpp reads: +5,+6,+8,+0xc).
// =============================================================================
#pragma pack(push,1)
struct CAICardTable {
    int  m_nCode;       // 0x00
    char m_nType;       // 0x04
    char m_nPosition;   // 0x05
    char m_nArea;       // 0x06
    char m_pad07;       // 0x07
    int  m_nPoint;      // 0x08
    int  m_nCash;       // 0x0c
};
#pragma pack(pop)

void* GetAICardTable(int nCode)           // CLoad::GetAICardTable @08053282
{
    static std::map<int, CAICardTable> s_table;
    static bool s_loaded = false;
    if (!s_loaded) {
        s_loaded = true;
        int h = g_Load.m_hAICard;
        for (int i = 0; ; ++i) {
            tvar_t* code = Table_GetData(h, i, "Code");
            if (code == 0) break;
            CAICardTable row;
            row.m_nCode     = FieldToValue(code);
            row.m_nType     = (char)FieldToValue(Table_GetData(h, i, "Type"));
            row.m_nPosition = (char)FieldToValue(Table_GetData(h, i, "Position"));
            row.m_nArea     = (char)FieldToValue(Table_GetData(h, i, "Area"));
            row.m_pad07     = 0;
            row.m_nPoint    = FieldToValue(Table_GetData(h, i, "Point"));
            row.m_nCash     = FieldToValue(Table_GetData(h, i, "Cash"));
            s_table[row.m_nCode] = row;
        }
    }
    std::map<int, CAICardTable>::iterator it = s_table.find(nCode);
    return (it == s_table.end()) ? 0 : (void*)&it->second;
}
