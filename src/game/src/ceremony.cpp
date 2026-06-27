// =============================================================================
// ceremony.cpp - Game CEREMONY (celebration) shop handlers.
// Reconstructed byte-faithfully from XKICK_Game1 (unstripped) via Ghidra.
//
// Handlers (TCP client, registered in process.cpp):
//   PacketCeremonyInfo     @08075cb2   (CM_CEREMONY_INFO 0x0838; from PlayerInfo)
//   PacketShopCeremonyList @0807c7a2   (CM_SHOPCEREMONY_LIST 0x0D48)
//   PacketEquipCeremony    @0807c81c   (CM_EQUIP_CEREMONY 0x0D4A)
//   PacketDivestCeremony   @0807c942   (CM_DIVEST_CEREMONY 0x0D4B)
//   PacketBuyCeremony      @0807ca52   (CM_BUY_CEREMONY 0x0D4C)
// internal sender:
//   PacketUpdateCeremony   @0807c7f4   (CM_UPDATE_CEREMONY 0x0D49)
//
// CPlayer ceremony logic mirrored as free CPlayer_* helpers over m_vCeremonyList
// (CPlayer+0x4d0):
//   CPlayer::EquipCeremony  @0808f5ea   CPlayer::DivestCeremony @0808f7dc
//   CPlayer::BuyCeremony    @0808f8d6   CPlayer::GetShopCeremonyList @0808f372
//   CPlayer::GetShopCeremonyTotalPage @0808f4f8
//   GetCeremonyCash @08097130  GetCeremonyPoint @08097210
//
// Ceremonies have up to 4 equip slots (1..4); equipping assigns the first free
// slot. The static ceremony table (binary map @0x80def88 from Table_Ceremony.csv)
// is queried via the rebuild's CSV handle API (g_Load.GetCeremonyTable()).
// Price = round(Discount * Price / 100); "Sell" = shop/restrict flag.
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

static void PacketUpdateCeremony(CPlayer* pPlayer, CSCUpdateCeremony* pPacket);

// =============================================================================
// Ceremony table (CSV) helpers.
// =============================================================================
static int CeF(int idx, const char* col)
{
    return FieldToValue(Table_GetData(g_Load.GetCeremonyTable(), idx, col));
}
static int CeRowOfCode(int nCode)
{
    int h = g_Load.GetCeremonyTable();
    for (int i = 0; ; ++i)
    {
        tvar_t* code = Table_GetData(h, i, "Code");
        if (code == 0) return -1;
        if (FieldToValue(code) == nCode) return i;
    }
}

// GetCeremonyCash @08097130 : -1 unknown, -2 cash-forbidden.
int GetCeremonyCash(int nCode)
{
    int row = CeRowOfCode(nCode);
    if (row < 0) { LOGOUT_ERROR("GetCeremonyCash: unknown ceremony code(%d)\n", nCode); return -1; }
    if (CeF(row, "Sell") == 2) return -2;
    return (int)std::lround((double)(CeF(row, "Discount") * CeF(row, "Cash")) / 100.0);
}
// GetCeremonyPoint @08097210 : -1 unknown, -2 point-forbidden.
int GetCeremonyPoint(int nCode)
{
    int row = CeRowOfCode(nCode);
    if (row < 0) { LOGOUT_ERROR("GetCeremonyPoint: unknown ceremony code(%d)\n", nCode); return -1; }
    if (CeF(row, "Sell") == 1) return -2;
    return (int)std::lround((double)(CeF(row, "Discount") * CeF(row, "Point")) / 100.0);
}

// =============================================================================
// CPlayer ceremony helpers.
// =============================================================================

// CPlayer::EquipCeremony @0808f5ea : equip a ceremony into the first free slot
// (1..4). -1 null entry, -2 ceremony not owned, -3 no free slot. else the slot.
int CPlayer_EquipCeremony(CPlayer* pPlayer, int nCeremonySeq)
{
    int nSlot;
    for (nSlot = 1; nSlot < 5; ++nSlot)
    {
        bool bUsed = false;
        for (size_t i = 0; i < pPlayer->m_vCeremonyList.size(); ++i)
        {
            CCeremony* c = pPlayer->m_vCeremonyList[i];
            if (c == 0) return -1;
            if (c->m_nState != 0 && c->m_nEquipKind == nSlot) { bUsed = true; break; }
        }
        if (!bUsed) break;                // nSlot is free
    }
    if (nSlot >= 6)
        return -3;

    CCeremony* pTarget = 0;
    for (size_t i = 0; i < pPlayer->m_vCeremonyList.size(); ++i)
    {
        CCeremony* c = pPlayer->m_vCeremonyList[i];
        if (c == 0) return -1;
        if (c->m_nCeremonySeq == nCeremonySeq) { pTarget = c; break; }
    }
    if (pTarget == 0)
        return -2;
    pTarget->ChangeCeremonyState();
    pTarget->m_nEquipKind = (char)nSlot;
    return pTarget->m_nEquipKind;
}

// CPlayer::DivestCeremony @0808f7dc : take off a ceremony by seq.
//   -1 null entry, -2 not found, else 0.
int CPlayer_DivestCeremony(CPlayer* pPlayer, int nCeremonySeq)
{
    CCeremony* pTarget = 0;
    for (size_t i = 0; i < pPlayer->m_vCeremonyList.size(); ++i)
    {
        CCeremony* c = pPlayer->m_vCeremonyList[i];
        if (c == 0) return -1;
        if (c->m_nCeremonySeq == nCeremonySeq) { pTarget = c; break; }
    }
    if (pTarget == 0)
        return -2;
    pTarget->m_nEquipKind = 0;
    pTarget->ChangeCeremonyState();
    return pTarget->m_nEquipKind;         // 0
}

// CPlayer::GetShopCeremonyTotalPage @0808f4f8
static int CPlayer_GetShopCeremonyTotalPage()
{
    int h = g_Load.GetCeremonyTable();
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

// CPlayer::GetShopCeremonyList @0808f372
void CPlayer_GetShopCeremonyList(CPlayer* /*pPlayer*/, int nPage, CSCShopCeremonyList* pPacket)
{
    for (int i = 0; i < 6; ++i)
        pPacket->m_cShopCeremonyData[i].m_nCode = 0;

    int nTotal = CPlayer_GetShopCeremonyTotalPage();
    if (nPage < 0)              nPage = nTotal - 1;
    else if (nTotal - 1 < nPage) nPage = 0;

    int nSkip = nPage * 6;
    int nSlot = 0, nSeen = 0;
    int h = g_Load.GetCeremonyTable();
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
            pPacket->m_cShopCeremonyData[nSlot].m_nCode     = FieldToValue(code);
            pPacket->m_cShopCeremonyData[nSlot].m_nDiscount = 0;
            ++nSlot;
        }
    }
    if (pPacket->m_cShopCeremonyData[0].m_nCode == 0) pPacket->m_nCurrentPage = 0;
    else                                              pPacket->m_nCurrentPage = (short)nPage;
    pPacket->m_nTotalPage = (short)nTotal;
}

// CPlayer::BuyCeremony @0808f8d6 : buy a ceremony (cash/point), persist, log, push.
//   -4 unknown, -6 already owned, -1 bad pay kind / alloc, -2 db fail,
//   cash: -11/-12/-13/-14, point: -21/-22/-23/-24. else 0.
int CPlayer_BuyCeremony(CPlayer* pPlayer, CCSBuyCeremony* pBuy)
{
    int   nCode    = pBuy->m_nCode;
    char  nBuyKind = pBuy->m_nBuyKind;
    int   nPrice   = pBuy->m_nPrice;

    if (CeRowOfCode(nCode) < 0)
    { LOGOUT_ERROR("BuyCeremony: unknown ceremony code(%d)\n", nCode); return -4; }

    for (size_t i = 0; i < pPlayer->m_vCeremonyList.size(); ++i)
    {
        CCeremony* c = pPlayer->m_vCeremonyList[i];
        if (c && c->m_nCode == nCode)
            return -6;
    }

    int nCost;
    if (nBuyKind == 1)
    {
        nCost = GetCeremonyCash(nCode);
        if (nCost < 0)                       return -11;
        if (nPrice != nCost)                 return -12;
        if (CPlayer_GetMoneyData(pPlayer) < 0) return -13;
        if (pPlayer->m_cMoney.m_nCash < nCost) return -14;
        pPlayer->m_cMoney.m_nCash -= nCost;
        if (CPlayer_PutMoneyData(pPlayer) < 0) return -13;
    }
    else if (nBuyKind == 2)
    {
        nCost = GetCeremonyPoint(nCode);
        if (nCost < 0)                       return -21;
        if (nPrice != nCost)                 return -22;
        if (CPlayer_GetMoneyData(pPlayer) < 0) return -23;
        if (pPlayer->m_cMoney.m_nPoint < nCost) return -24;
        pPlayer->m_cMoney.m_nPoint -= nCost;
        if (CPlayer_PutMoneyData(pPlayer) < 0) return -23;
    }
    else
        return -1;

    CCeremony* pCeremony = new CCeremony();
    if (pCeremony == 0)
        return -1;
    pCeremony->m_nState       = 1;
    pCeremony->m_nCeremonySeq = 0;
    pCeremony->m_nCode        = nCode;
    pCeremony->m_nEquipKind   = 0;
    pCeremony->m_nCeremonySeq = g_Sql.InsertCeremonyField(pPlayer, pCeremony);
    if (pCeremony->m_nCeremonySeq < 0)
    {
        delete pCeremony;
        return -2;
    }
    pPlayer->m_vCeremonyList.push_back(pCeremony);

    // sale log : {seq, code, kind 3(ceremony), buyKind, sale 1, price, amount 1}
    CSale sale;
    memset(&sale, 0, sizeof(sale));
    sale.m_nObjectSeq  = pCeremony->m_nCeremonySeq;
    sale.m_nObjectCode = pCeremony->m_nCode;
    sale.m_nObjectKind = 3;
    sale.m_nBuyKind    = nBuyKind;
    sale.m_nSaleKind   = 1;
    sale.m_nPrice      = nPrice;
    sale.m_nAmount     = 1;
    g_Sql.InsertSaleLog(pPlayer, &sale);

    CSCUpdateCeremony uc;
    uc.m_nCommand     = CM_UPDATE_CEREMONY;
    uc.m_nBodySize    = 0xb;
    uc.m_nUpdateKind  = 1;
    uc.m_nCeremonySeq = pCeremony->m_nCeremonySeq;
    uc.m_nCode        = pCeremony->m_nCode;
    uc.m_nEquipKind   = pCeremony->m_nEquipKind;
    PacketUpdateCeremony(pPlayer, &uc);

    return pCeremony->m_nEquipKind;       // 0
}

// =============================================================================
// PacketCeremonyInfo @08075cb2 (CM_CEREMONY_INFO 0x0838) - chained from PlayerInfo.
// Packs up to 50 (0x32) 9-byte rows {seq, code, equip} per page.
// =============================================================================
void PacketCeremonyInfo(CPlayer* pPlayer)
{
    CSCCeremonyInfo sc;
    sc.m_nCommand = CM_CEREMONY_INFO;
    int   nCount = 0;

    for (size_t i = 0; i < pPlayer->m_vCeremonyList.size(); ++i)
    {
        CCeremony* c = pPlayer->m_vCeremonyList[i];
        if (c == 0 || c->m_nState == 0)
            continue;
        CCeremonyInfo& r = sc.m_cCeremonyInfo[nCount];
        r.m_nCeremonySeq = c->m_nCeremonySeq;
        r.m_nCode        = c->m_nCode;
        r.m_nEquipKind   = c->m_nEquipKind;
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
// PacketShopCeremonyList @0807c7a2 (CM_SHOPCEREMONY_LIST 0x0D48)
// =============================================================================
void PacketShopCeremonyList(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCShopCeremonyList sc;
    sc.m_nCommand  = CM_SHOPCEREMONY_LIST;
    sc.m_nBodySize = 0x23;
    CPlayer_GetShopCeremonyList(pPlayer, (int)((CCSShopCeremonyList*)pPacket)->m_nPage, &sc);
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// PacketUpdateCeremony @0807c7f4 (CM_UPDATE_CEREMONY 0x0D49) - internal sender.
// =============================================================================
static void PacketUpdateCeremony(CPlayer* pPlayer, CSCUpdateCeremony* pPacket)
{
    pPacket->m_nResponse = 0;
    SendPlayer(pPlayer, pPacket, (int)pPacket->m_nResponse);
}

// =============================================================================
// PacketEquipCeremony @0807c81c (CM_EQUIP_CEREMONY 0x0D4A)
// =============================================================================
void PacketEquipCeremony(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCEquipCeremony sc;
    sc.m_nCommand  = CM_EQUIP_CEREMONY;
    sc.m_nBodySize = 6;
    int nSeq = ((CCSEquipCeremony*)pPacket)->m_nCeremonySeq;
    int nRet = CPlayer_EquipCeremony(pPlayer, nSeq);
    if (nRet < 0)
    {
        char nResp = (char)nRet;
        if (nRet == -2)      nResp = (char)0xf5;          // -11
        else if (nRet == -3) nResp = (char)0xf4;          // -12
        else if (nRet == -1)
            LOGOUT_ERROR("PacketEquipCeremony: EquipCeremony=-1 seq(%d)\n", pPlayer->m_nPlayerSeq);
        sc.m_nResponse = nResp;
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_nCeremonySeq = nSeq;
    sc.m_nResponse    = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// PacketDivestCeremony @0807c942 (CM_DIVEST_CEREMONY 0x0D4B)
// =============================================================================
void PacketDivestCeremony(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCDivestCeremony sc;
    sc.m_nCommand  = CM_DIVEST_CEREMONY;
    sc.m_nBodySize = 6;
    int nSeq = ((CCSDivestCeremony*)pPacket)->m_nCeremonySeq;
    int nRet = CPlayer_DivestCeremony(pPlayer, nSeq);
    if (nRet < 0)
    {
        char nResp = (char)nRet;
        if (nRet == -2)      nResp = (char)0xf4;          // -12
        else if (nRet == -1) { nResp = (char)0xf5;        // -11
            LOGOUT_ERROR("PacketDivestCeremony: DivestCeremony=-1 seq(%d)\n", pPlayer->m_nPlayerSeq); }
        sc.m_nResponse = nResp;
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_nCeremonySeq = nSeq;
    sc.m_nEquipKind   = 0;
    sc.m_nResponse    = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// PacketBuyCeremony @0807ca52 (CM_BUY_CEREMONY 0x0D4C)
// =============================================================================
void PacketBuyCeremony(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCBuyCeremony sc;
    sc.m_nCommand  = CM_BUY_CEREMONY;
    sc.m_nBodySize = 0x12;
    int  nRet  = CPlayer_BuyCeremony(pPlayer, (CCSBuyCeremony*)pPacket);
    char nResp = (char)nRet;
    if (nRet < 0)
    {
        if ((unsigned)(nRet + 2) < 2)
            LOGOUT_ERROR("PacketBuyCeremony: BuyCeremony=%d seq(%d)\n", nResp, pPlayer->m_nPlayerSeq);
        sc.m_nResponse = nResp;
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_cMoney.m_nCash      = pPlayer->m_cMoney.m_nCash;
    sc.m_cMoney.m_nPoint     = pPlayer->m_cMoney.m_nPoint;
    sc.m_cMoney.m_nCredit    = pPlayer->m_cMoney.m_nCredit;
    sc.m_cMoney.m_nClubPoint = pPlayer->m_cMoney.m_nClubPoint;
    sc.m_nEquipKind = nResp;
    sc.m_nResponse  = 0;
    SendPlayer(pPlayer, &sc, 0);
}
