// =============================================================================
// training.cpp - Game TRAINING shop handlers (training raises a faculty stat).
// Reconstructed byte-faithfully from XKICK_Game1 (unstripped) via Ghidra.
//
// Handlers (TCP client, registered in process.cpp):
//   PacketTrainingInfo     @08075a96   (CM_TRAINING_INFO 0x0837; from PlayerInfo)
//   PacketShopTrainingList @0807c5dc   (CM_SHOPTRAINING_LIST 0x0CE4)
//   PacketBuyTraining      @0807c656   (CM_BUY_TRAINING 0x0CE8)
// internal sender:
//   PacketUpdateTraining   @0807c62e   (CM_UPDATE_TRAINING 0x0CE5)
//
// CPlayer training logic mirrored as free CPlayer_* helpers over m_vTrainingList
// (CPlayer+0x4c4):
//   CPlayer::BuyTraining        @0808ecf0   CPlayer::GetShopTrainingList @0808ea78
//   CPlayer::GetShopTrainingTotalPage @0808ebfe
//   CPlayer::SetTrainingFaculty @0808b72e   (already defined in player.cpp; reused)
//   GetTrainingCash @080973b0   GetTrainingPoint @08097490
//
// A training is keyed by Type; buying upgrades that Type one Value-step at a time
// (a new Type must start at Value 1; an existing Type must buy exactly Value+1 and
// meet the Limit player-level). SetTrainingFaculty re-accumulates every owned
// training's Value into the faculty block at CPlayer+0x14f (index = Type-10).
//
// The static training table (binary map @0x80defa0 from Table_Training.csv) is
// queried via the CSV handle API (g_Load.GetTrainingTable()). Price = round(
// Discount * Price / 100); "Sell" = shop/restrict flag.
//
// Portable: standard C++ only; all packet/player access is via struct members.
// =============================================================================
#include "Main.h"
#include "Sql.h"
#include "GameLoad.h"
#include "Domain.h"
#include "LogManager.h"
#include <cstring>
#include <cmath>

int CPlayer_GetMoneyData(CPlayer*);
int CPlayer_PutMoneyData(CPlayer*);

static void PacketUpdateTraining(CPlayer* pPlayer, CSCUpdateTraining* pPacket);

// =============================================================================
// Training table (CSV) helpers.
// =============================================================================
static int TrF(int idx, const char* col)
{
    return FieldToValue(Table_GetData(g_Load.GetTrainingTable(), idx, col));
}
static int TrRowOfCode(int nCode)
{
    int h = g_Load.GetTrainingTable();
    for (int i = 0; ; ++i)
    {
        tvar_t* code = Table_GetData(h, i, "Code");
        if (code == 0) return -1;
        if (FieldToValue(code) == nCode) return i;
    }
}

// GetTrainingCash @080973b0 : -1 unknown, -2 cash-forbidden.
int GetTrainingCash(int nCode)
{
    int row = TrRowOfCode(nCode);
    if (row < 0) { LOGOUT_ERROR("GetTrainingCash: unknown training code(%d)\n", nCode); return -1; }
    if (TrF(row, "Sell") == 2) return -2;
    return (int)std::lround((double)(TrF(row, "Discount") * TrF(row, "Cash")) / 100.0);
}
// GetTrainingPoint @08097490 : -1 unknown, -2 point-forbidden.
int GetTrainingPoint(int nCode)
{
    int row = TrRowOfCode(nCode);
    if (row < 0) { LOGOUT_ERROR("GetTrainingPoint: unknown training code(%d)\n", nCode); return -1; }
    if (TrF(row, "Sell") == 1) return -2;
    return (int)std::lround((double)(TrF(row, "Discount") * TrF(row, "Point")) / 100.0);
}

// =============================================================================
// CPlayer training helpers.
// =============================================================================

// CPlayer::GetShopTrainingTotalPage @0808ebfe
static int CPlayer_GetShopTrainingTotalPage()
{
    int h = g_Load.GetTrainingTable();
    int n = 0;
    for (int i = 0; ; ++i)
    {
        tvar_t* code = Table_GetData(h, i, "Code");
        if (code == 0) break;
        if (FieldToValue(Table_GetData(h, i, "Sell")) != 0)
            ++n;
    }
    return (n % 6 == 0) ? n / 6 : n / 6 + 1;
}

// CPlayer::GetShopTrainingList @0808ea78
void CPlayer_GetShopTrainingList(CPlayer* /*pPlayer*/, int nPage, CSCShopTrainingList* pPacket)
{
    for (int i = 0; i < 6; ++i)
        pPacket->m_cShopTrainingData[i].m_nCode = 0;

    int nTotal = CPlayer_GetShopTrainingTotalPage();
    if (nPage < 0)              nPage = nTotal - 1;
    else if (nTotal - 1 < nPage) nPage = 0;

    int nSkip = nPage * 6;
    int nSlot = 0, nSeen = 0;
    int h = g_Load.GetTrainingTable();
    for (int i = 0; ; ++i)
    {
        tvar_t* code = Table_GetData(h, i, "Code");
        if (code == 0) break;
        if (FieldToValue(Table_GetData(h, i, "Sell")) == 0) continue;
        if (nSlot > 5) break;
        bool bTake = (nSkip <= nSeen);
        ++nSeen;
        if (bTake)
        {
            pPacket->m_cShopTrainingData[nSlot].m_nCode     = FieldToValue(code);
            pPacket->m_cShopTrainingData[nSlot].m_nDiscount = 0;
            ++nSlot;
        }
    }
    if (pPacket->m_cShopTrainingData[0].m_nCode == 0) pPacket->m_nCurrentPage = 0;
    else                                              pPacket->m_nCurrentPage = (short)nPage;
    pPacket->m_nTotalPage = (short)nTotal;
}

// CPlayer::BuyTraining @0808ecf0 : buy/upgrade a training (cash/point), persist,
// recompute faculty, log, push.  Returns 0 on success, else a reason code:
//   -4 unknown, -16 wrong step / under-level, -17 already at/over step,
//   -1 bad pay kind / alloc, -2 db fail, cash: -11/-12/-13/-14, point: -21..-24.
int CPlayer_BuyTraining(CPlayer* pPlayer, CCSBuyTraining* pBuy)
{
    int   nCode    = pBuy->m_nCode;
    char  nBuyKind = pBuy->m_nBuyKind;
    int   nPrice   = pBuy->m_nPrice;

    int row = TrRowOfCode(nCode);
    if (row < 0) { LOGOUT_ERROR("BuyTraining: unknown training code(%d)\n", nCode); return -4; }

    int nType  = TrF(row, "Type");
    int nLimit = TrF(row, "Limit");          // required player level
    int nValue = TrF(row, "Value");          // this entry's step/level

    // find an owned training of the same Type.
    CTraining* pOwned = 0;
    for (size_t i = 0; i < pPlayer->m_vTrainingList.size(); ++i)
    {
        CTraining* t = pPlayer->m_vTrainingList[i];
        if (t && t->m_nType == (char)nType) { pOwned = t; break; }
    }

    if (pOwned == 0)
    {
        if (nValue > 1)                       return -16;   // must start at step 1
    }
    else
    {
        if (nValue <= (int)pOwned->m_nLevel)  return -17;
        if ((int)pOwned->m_nLevel + 1 != nValue) return -16;
        if ((char)pPlayer->m_cLevel.m_nLevel < (char)nLimit) return -16;
    }

    int nCost;
    if (nBuyKind == 1)
    {
        nCost = GetTrainingCash(nCode);
        if (nCost < 0)                       return -11;
        if (nPrice != nCost)                 return -12;
        if (CPlayer_GetMoneyData(pPlayer) < 0) return -13;
        if (pPlayer->m_cMoney.m_nCash < nCost) return -14;
        pPlayer->m_cMoney.m_nCash -= nCost;
        if (CPlayer_PutMoneyData(pPlayer) < 0) return -13;
    }
    else if (nBuyKind == 2)
    {
        nCost = GetTrainingPoint(nCode);
        if (nCost < 0)                       return -21;
        if (nPrice != nCost)                 return -22;
        if (CPlayer_GetMoneyData(pPlayer) < 0) return -23;
        if (pPlayer->m_cMoney.m_nPoint < nCost) return -24;
        pPlayer->m_cMoney.m_nPoint -= nCost;
        if (CPlayer_PutMoneyData(pPlayer) < 0) return -23;
    }
    else
        return -1;

    CTraining* pUse;
    if (pOwned == 0)                          // brand-new training
    {
        CTraining* pTr = new CTraining();
        if (pTr == 0)
            return -1;
        pTr->m_nState       = 1;
        pTr->m_nTrainingSeq = 0;
        pTr->m_nCode        = nCode;
        pTr->m_nLevel       = (char)nValue;
        pTr->m_nType        = (char)nType;
        pTr->m_nTrainingSeq = g_Sql.InsertTrainingField(pPlayer, pTr);
        if (pTr->m_nTrainingSeq < 0)
        {
            delete pTr;
            return -2;
        }
        pPlayer->m_vTrainingList.push_back(pTr);
        pUse = pTr;
    }
    else                                      // upgrade existing Type (state -> dirty)
    {
        pOwned->m_nState = 2;
        pOwned->m_nCode  = nCode;
        pOwned->m_nLevel = (char)nValue;
        pUse = pOwned;
    }

    pPlayer->SetTrainingFaculty();            // re-accumulate faculty block

    // sale log : {seq, code, kind 4(training), buyKind, sale 1, price, amount 1}
    CSale sale;
    memset(&sale, 0, sizeof(sale));
    sale.m_nObjectSeq  = pUse->m_nTrainingSeq;
    sale.m_nObjectCode = pUse->m_nCode;
    sale.m_nObjectKind = 4;
    sale.m_nBuyKind    = nBuyKind;
    sale.m_nSaleKind   = 1;
    sale.m_nPrice      = nPrice;
    sale.m_nAmount     = 1;
    g_Sql.InsertSaleLog(pPlayer, &sale);

    CSCUpdateTraining ut;
    ut.m_nCommand     = CM_UPDATE_TRAINING;
    ut.m_nBodySize    = 0xb;
    ut.m_nUpdateKind  = (pOwned == 0) ? 1 : 3;   // 1 = added, 3 = upgraded
    ut.m_nTrainingSeq = pUse->m_nTrainingSeq;
    ut.m_nCode        = pUse->m_nCode;
    ut.m_nLevel       = pUse->m_nLevel;
    PacketUpdateTraining(pPlayer, &ut);

    return 0;
}

// =============================================================================
// PacketTrainingInfo @08075a96 (CM_TRAINING_INFO 0x0837) - chained from PlayerInfo.
// Packs up to 50 (0x32) 9-byte rows {seq, code, level} per page.
// =============================================================================
void PacketTrainingInfo(CPlayer* pPlayer)
{
    CSCTrainingInfo sc;
    sc.m_nCommand = CM_TRAINING_INFO;
    int   nCount = 0;

    for (size_t i = 0; i < pPlayer->m_vTrainingList.size(); ++i)
    {
        CTraining* t = pPlayer->m_vTrainingList[i];
        if (t == 0 || t->m_nState == 0)
            continue;
        CTrainingInfo& r = sc.m_cTrainingInfo[nCount];
        r.m_nTrainingSeq = t->m_nTrainingSeq;
        r.m_nCode        = t->m_nCode;
        r.m_nLevel       = t->m_nLevel;
        if (++nCount == 0x32)
        {
            sc.m_nBodySize = nCount * 9 + 2;
            sc.m_nCount    = 0x32;
            sc.m_nResponse = 0;
            SendPlayer(pPlayer, &sc, 0);
            nCount = 0;
        }
    }
    if (nCount != 0)
    {
        sc.m_nBodySize = nCount * 9 + 2;
        sc.m_nCount    = (char)nCount;
        sc.m_nResponse = 0;
        SendPlayer(pPlayer, &sc, 0);
    }
}

// =============================================================================
// PacketShopTrainingList @0807c5dc (CM_SHOPTRAINING_LIST 0x0CE4)
// =============================================================================
void PacketShopTrainingList(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCShopTrainingList sc;
    sc.m_nCommand  = CM_SHOPTRAINING_LIST;
    sc.m_nBodySize = 0x23;
    CPlayer_GetShopTrainingList(pPlayer, (int)((CCSShopTrainingList*)pPacket)->m_nPage, &sc);
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// PacketUpdateTraining @0807c62e (CM_UPDATE_TRAINING 0x0CE5) - internal sender.
// =============================================================================
static void PacketUpdateTraining(CPlayer* pPlayer, CSCUpdateTraining* pPacket)
{
    pPacket->m_nResponse = 0;
    SendPlayer(pPlayer, pPacket, (int)pPacket->m_nResponse);
}

// =============================================================================
// PacketBuyTraining @0807c656 (CM_BUY_TRAINING 0x0CE8)
// On success echoes money + the refreshed 25-byte training-faculty block.
// =============================================================================
void PacketBuyTraining(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCBuyTraining sc;
    sc.m_nCommand  = CM_BUY_TRAINING;
    sc.m_nBodySize = 0x2b;
    int  nRet  = CPlayer_BuyTraining(pPlayer, (CCSBuyTraining*)pPacket);
    char nResp = (char)nRet;
    if (nRet < 0)
    {
        if ((unsigned)(nRet + 2) < 2)
            LOGOUT_ERROR("PacketBuyTraining: BuyTraining=%d seq(%d)\n", nResp, pPlayer->m_nPlayerSeq);
        sc.m_nResponse = nResp;
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_cMoney.m_nCash      = pPlayer->m_cMoney.m_nCash;
    sc.m_cMoney.m_nPoint     = pPlayer->m_cMoney.m_nPoint;
    sc.m_cMoney.m_nCredit    = pPlayer->m_cMoney.m_nCredit;
    sc.m_cMoney.m_nClubPoint = pPlayer->m_cMoney.m_nClubPoint;
    sc.m_nEquipKind = nResp;                  // 0 on success
    memcpy(&sc.m_cTrainingFaculty, &pPlayer->m_cTrainingFaculty, 25);   // CFaculty[25]
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}
