// =============================================================================
// rndshop.cpp - daily RANDOM SHOP module + the RNG it rides on.
// Reconstructed byte-faithfully from XKICK_Game1 (Ghidra).
//
// Owns: CRandom (RNG), CRandomShop (the embedded daily shop @CPlayer+0x408),
// CLoad::GetRandomMission, CPlayer::SpendMoney/ChangeTodayShop, and the three
// client handlers (CM_BUY_RANDOMITEM 0xC26 / CM_RANDOMSHOPITEM_LIST 0xC27 /
// CM_REFRESH_SHOP 0xC29).
//
// CRandomShop offset-0 is a CPlayer* back-pointer (set by InitShop), so the shop
// methods reach the owning player via m_pPlayer. Today's purchased-item bitmask
// lives in the player's m_nTodayBit (+0x540); the RNG seed source is m_nTodaySeed
// (+0x53c).
//
// NOTE (faithful degradation): the binary generates offers from typed random
// tables inside CLoad (Item/Option/Situation/Enchant maps) whose internal layout
// is not yet pinned in the portable rebuild. CLoad::GetRandom*Table/GetEnchantTable
// currently return 0, so CreateItem/EnchantItem early-out and the daily shop is an
// empty offer list -- everything links and runs safely. The control flow is exact.
// =============================================================================
#include "Main.h"
#include "GameProtocolItem.h"
#include "Sql.h"
#include "GameLoad.h"
#include "GameItem.h"
#include "Domain.h"
#include "Config.h"
#include <cstring>
#include <cstdlib>
#include <ctime>


// ---- cross-module helpers (defined elsewhere) ----
int  CPlayer_GetMoneyData(CPlayer*);          // stubs.cpp
int  CPlayer_PutMoneyData(CPlayer*);          // stubs.cpp
CItem* CPlayer_GetBagItem(CPlayer*, int);     // stubs.cpp
int  CPlayer_EquipItem(CPlayer*, int);        // item.cpp
void PacketUpdateItem(CPlayer*, CSCUpdateItem*); // item.cpp (inventory delta push)

// =============================================================================
//                                   RNG
// =============================================================================
static unsigned RngClockSeed()
{
    return (unsigned)std::time(0) ^ (unsigned)std::clock();
}

CRandom::CRandom() { InitSeed(); }
void CRandom::InitSeed(int v) { m_nSeed = v; m_nState = v; }
void CRandom::InitSeed()      { m_nSeed = m_nState = (int)RngClockSeed(); }
int  CRandom::GetFixRandom()  { std::srand((unsigned)m_nState); m_nState = std::rand(); return m_nState; }
int  CRandom::GetMessRandom() { std::srand(RngClockSeed()); return std::rand(); }
int  CRandom::Random()        { return GetMessRandom(); }

// card.cpp / others draw the global RNG through this thin helper.
int Random_Random() { return g_Random.Random(); }

// =============================================================================
//  free item-type maps used by the shop generator (binary _Z* funcs)
// =============================================================================
int GetItemTypeFromEquip(int t)    // _Z20GetItemTypeFromEquipi @08096d10
{
    static const int k[17] = {0x65,0x66,0x67,0xc9,0xca,0xcb,0xcc,0xcd,
                              0x12d,0x12e,0x12f,0x191,0x192,0x193,0x194,0x195,0x196};
    return (t >= 0 && t <= 0x10) ? k[t] : 0;
}
int GetItemEquipFromType(int t)    // _Z20GetItemEquipFromTypei (inverse map)
{
    static const int k[17] = {0x65,0x66,0x67,0xc9,0xca,0xcb,0xcc,0xcd,
                              0x12d,0x12e,0x12f,0x191,0x192,0x193,0x194,0x195,0x196};
    for (int i = 0; i < 17; ++i) if (k[i] == t) return i;
    return 0;
}

// =============================================================================
//                              CRandomShop
// =============================================================================
CRandomShop::CRandomShop() { m_pPlayer = 0; }

void CRandomShop::InitShop(CPlayer* pPlayer) { m_pPlayer = pPlayer; InitSeed(); ClearTodayList(); }
void CRandomShop::InitSeed()      { if (m_pPlayer) m_cRandom.InitSeed(m_pPlayer->m_nTodaySeed); }
int  CRandomShop::GetFixRandom()  { return m_cRandom.GetFixRandom(); }
int  CRandomShop::GetMessRandom() { return m_cRandom.GetMessRandom(); }

void CRandomShop::ClearTodayList()
{
    for (size_t i = 0; i < m_vTodayList.size(); ++i) delete m_vTodayList[i];
    m_vTodayList.clear();
}

bool CRandomShop::GetTodayBit(int n)
{
    if (m_pPlayer == 0) return false;
    return ((m_pPlayer->m_nTodayBit >> n) & 1) != 0;
}
void CRandomShop::SetTodayBit(int n, bool v)
{
    if (m_pPlayer == 0) return;
    if (v) m_pPlayer->m_nTodayBit |=  (1 << n);
    else   m_pPlayer->m_nTodayBit &= ~(1 << n);
}

bool CRandomShop::IsPossibleBuyItemType(int t) { return (t >= 1 && t <= 0xd); }  // 1..13

void CRandomShop::CreateShop()
{
    InitSeed();
    ClearTodayList();
    for (int type = 0; type < 0x11; ++type) {           // 17 item types
        if (!IsPossibleBuyItemType(type)) continue;
        for (int j = 0; j < 2; ++j) {                   // 2 offers per type
            int index = type * 2 + j;
            CItem* pItem = new CItem();
            if (pItem == 0) continue;
            int r = GetFixRandom();
            if (CreateItem(pItem, 0, type, index, r) < 0) { delete pItem; continue; }
            if (GetTodayBit(index) == 0) m_vTodayList.push_back(pItem);
            else delete pItem;
        }
    }
}

int CRandomShop::CreateItem(CItem* pItem, int kind, int type, int index, int rnd)
{
    int  priceAcc = 0;
    char limit    = GetItemLimit();
    int  equipT   = GetItemTypeFromEquip(type);
    int  grade    = GetItemGrade(equipT);
    int  optCnt   = GetOptionCount(grade);

    CItemTable* pItemRow = g_Load.GetRandomItemTable(type,
                       m_pPlayer ? (int)m_pPlayer->m_cShape.m_nGender : 0, GetFixRandom());
    if (pItemRow == 0) return -3;

    for (int k = 0; k < 5; ++k) pItem->m_nOptionCode[k] = 0;
    for (int k = 0; k < optCnt; ++k) {
        CSituationTable* sit = g_Load.GetRandomSituationTable(k, GetFixRandom());
        if (sit == 0) return -1;
        int sitId = sit->m_nCode;
        COptionTable* opt = g_Load.GetRandomOptionTable(type, k, GetFixRandom());
        if (opt == 0) return -1;
        int optId = opt->m_nCode;
        int val = GetItemValue(opt, kind, GetFixRandom());
        pItem->m_nOptionCode[k] = sitId * 100000 + optId * 100 + val;
        priceAcc += opt->m_nPrice;
    }
    pItem->m_nItemSeq   = index;
    pItem->m_nCode      = pItemRow->m_nCode;
    pItem->m_nEquipKind = 0;
    pItem->m_nLimit     = limit;
    pItem->m_nGrade     = (char)grade;
    pItem->m_nPrice     = GetItemPrice(pItemRow->m_nPoint, priceAcc, grade);
    pItem->m_nAmount    = 1;
    pItem->m_nRandom    = rnd;
    pItem->m_nOrder     = (char)index;
    return 0;
}

int CRandomShop::GetItemGrade(int equipT)
{
    int r = GetFixRandom() % 1000 + 1;
    if ((unsigned)(equipT - 0xc9) < 5) {                // types 0xc9..0xcd
        if (r < 0x227) return 1; if (r < 0x353) return 2; if (r < 0x3b7) return 3;
        if (r < 0x3df) return 4; return 5;
    }
    if (r < 0x23b) return 1; if (r < 0x367) return 2; if (r < 0x3cb) return 3;
    if (r < 0x3e4) return 4; return 5;
}

char CRandomShop::GetItemLimit()
{
    int lvl = m_pPlayer ? (int)m_pPlayer->m_cLevel.m_nLevel : 1;
    int v = GetFixRandom() % (lvl + 10) + 1;
    if (v < lvl / 3) v = lvl / 3;
    v -= v % 5;
    if (v < 1) v = 1; else if (v > 0x32) v = 0x32;
    return (char)v;
}

int CRandomShop::GetItemPrice(int base, int optSum, int grade) { return optSum + base + grade * 500; }

int CRandomShop::GetOptionCount(int grade)
{
    int v = GetFixRandom() % (grade + 1);
    if (v == 0) v = GetFixRandom() % (grade + 1);
    return v;
}

int CRandomShop::GetItemValue(COptionTable* pOpt, int kind, int rnd)
{
    int lvl = m_pPlayer ? (int)m_pPlayer->m_cLevel.m_nLevel : 1;
    int tier = rnd % lvl + 1; tier -= tier % 10; if (tier < 1) tier = 1;
    int idx = 0;
    switch (tier) { case 1: idx=0;break; case 10: idx=1;break; case 0x14: idx=2;break;
                    case 0x1e: idx=3;break; case 0x28: idx=4;break; case 0x32: idx=5;break; }
    if (kind == 0) return (int)pOpt->m_nUsual[idx];
    if (kind == 1) return (int)pOpt->m_nSpecial[idx];
    return 0;
}

CItem* CRandomShop::GetShopItem(int index)
{
    if (m_vTodayList.empty() || GetTodayBit(index) != 0) return 0;
    for (size_t i = 0; i < m_vTodayList.size(); ++i)
        if (m_vTodayList[i] && m_vTodayList[i]->m_nItemSeq == index) return m_vTodayList[i];
    return 0;
}
void CRandomShop::GetShopItem(CItem* pItem, int index, int seed)
{
    m_cRandom.InitSeed(seed);
    CreateItem(pItem, 0, index / 2, index, seed);
}

int CRandomShop::GetShopIndex(CItemInfo* pInfo)
{
    int* q = (int*)pInfo;
    for (size_t i = 0; i < m_vTodayList.size(); ++i) {
        CItem* it = m_vTodayList[i];
        if (it == 0) continue;
        if (it->m_nCode == q[1] &&
            it->m_nOptionCode[0]==q[5] && it->m_nOptionCode[1]==q[6] &&
            it->m_nOptionCode[2]==q[7] && it->m_nOptionCode[3]==q[8] &&
            it->m_nOptionCode[4]==q[9])
            return it->m_nItemSeq;
    }
    return -1;
}

int CRandomShop::GetItemList(CSCRandomshopitemList* pOut)
{
    int n = 0;
    CreateShop();
    for (size_t i = 0; i < m_vTodayList.size() && n < 32; ++i) {
        CItem* it = m_vTodayList[i];
        if (it == 0) continue;
        if (GetTodayBit(it->m_nItemSeq) != 0) continue;
        CItemRow* r = &pOut->m_cItem[n];
        r->m_nItemSeq   = it->m_nItemSeq;
        r->m_nCode      = it->m_nCode;
        r->m_nEquipKind = it->m_nEquipKind;
        r->m_nLimit     = it->m_nLimit;
        r->m_nGrade     = it->m_nGrade;
        r->m_nAmount    = it->m_nAmount;
        r->m_nPrice     = it->m_nPrice;
        memcpy(r->m_nOptionCode, it->m_nOptionCode, 0x14);
        ++n;
    }
    pOut->m_nCount = (short)n;
    return 0;
}

// =============================================================================
// BUY RANDOM ITEM  (CRandomShop::BuyItem @0804b1d0)  -- pays in POINT.
// CCSBuyRandomitem: char index@+0xc, int code@+0xe, int price@+0x12.
// =============================================================================
int CRandomShop::BuyItem(CCSBuyRandomitem* pIn)
{
    CPlayer* p = m_pPlayer;
    if (p == 0 || pIn == 0) return -1;

    int nIndex = (int)pIn->m_nIndex;

    int nCount = 0;
    for (size_t i = 0; i < p->m_vItemList.size(); ++i) {
        CItem* it = p->m_vItemList[i];
        if (it && !it->IsEmptyState()) ++nCount;
    }
    if (nCount > 0x50) return -3;                        // inventory full (>80)

    CreateShop();
    CItem* pShop = GetShopItem(nIndex);
    if (pShop == 0)                              return -4;
    if (pShop->m_nCode != pIn->m_nCode)          return -4;

    int nPrice = pShop->m_nPrice;
    if (nPrice < 0)                              return -21;
    if (pIn->m_nPrice != nPrice)                 return -22;
    if (CPlayer_GetMoneyData(p) < 0)             return -23;
    if (p->m_cMoney.m_nPoint < nPrice)                      return -24;   // pays with POINT

    CItem* pItem = new CItem();
    if (pItem == 0) return -1;
    pItem->m_nItemSeq   = 0;
    pItem->m_nState     = 1;
    pItem->m_nCode      = pShop->m_nCode;
    pItem->m_nEquipKind = 0;
    pItem->m_nLimit     = pShop->m_nLimit;
    pItem->m_nGrade     = pShop->m_nGrade;
    pItem->m_nAmount    = 1;
    pItem->m_nPrice     = pShop->m_nPrice;
    pItem->m_nRandom    = pShop->m_nRandom;
    pItem->m_nOrder     = pShop->m_nOrder;
    for (int i = 0; i < 5; ++i) pItem->m_nOptionCode[i] = pShop->m_nOptionCode[i];

    pItem->m_nItemSeq = g_Sql.InsertItemField(p, pItem);
    if (pItem->m_nItemSeq < 0) { delete pItem; return -2; }

    SetTodayBit(nIndex, true);
    if (g_Sql.UpdateTodayField(p) < 0) return -2;

    p->m_cMoney.m_nPoint -= nPrice;
    if (CPlayer_PutMoneyData(p) < 0) return -23;

    p->m_vItemList.push_back(pItem);
    CPlayer_EquipItem(p, pItem->m_nItemSeq);

    CSale sale;
    sale.m_nObjectSeq  = pItem->m_nItemSeq;
    sale.m_nObjectCode = pItem->m_nCode;
    sale.m_nObjectKind = 1;
    sale.m_nBuyKind    = 2;
    sale.m_nSaleKind   = 1;
    sale.m_nPrice      = pItem->m_nPrice;
    sale.m_nAmount     = pItem->m_nAmount;
    g_Sql.InsertSaleLog(p, &sale);

    CSCUpdateItem ui;
    ui.m_nKind = 1;
    ui.m_cItemRow.m_nItemSeq   = pItem->m_nItemSeq;
    ui.m_cItemRow.m_nCode      = pItem->m_nCode;
    ui.m_cItemRow.m_nEquipKind = pItem->m_nEquipKind;
    ui.m_cItemRow.m_nLimit     = pItem->m_nLimit;
    ui.m_cItemRow.m_nGrade     = pItem->m_nGrade;
    ui.m_cItemRow.m_nAmount    = pItem->m_nAmount;
    ui.m_cItemRow.m_nPrice     = pItem->m_nPrice;
    for (int i = 0; i < 5; ++i) ui.m_cItemRow.m_nOptionCode[i] = pItem->m_nOptionCode[i];
    memcpy(ui.m_cEquipWear.m_data, &p->m_cItemOption, 0x158);
    PacketUpdateItem(p, &ui);
    return 0;
}

// =============================================================================
// ENCHANT ITEM  (CRandomShop::EnchantItem @0804aa46)  -- add a random option.
// CCSEnchantItem: int itemSeq@+0xc, int price@+0x10, char type@+0x14, char pay@+0x15.
// =============================================================================
int CRandomShop::EnchantItem(CCSEnchantItem* pIn)
{
    CPlayer* p = m_pPlayer;
    if (p == 0 || pIn == 0) return -1;

    CItem* pItem = CPlayer_GetBagItem(p, pIn->m_nItemSeq);
    if (pItem == 0) return -2;

    CEnchantTable* pEnch = g_Load.GetEnchantTable(pIn->m_nType, pItem->m_nLimit);
    if (pEnch == 0) return -2;

    int nUsed = 0;
    for (int i = 0; i < 5; ++i)
        if (pItem->m_nOptionCode[i] != 0) ++nUsed;
    if (!(nUsed < pItem->m_nGrade && nUsed < 5)) return -4;

    int nPrice;
    char nPay = pIn->m_nPay;
    if (nPay == 1) {                                     // cash
        nPrice = pEnch->m_nCash;
        if (nPrice < 0)                  return -11;
        if (pIn->m_nPrice != nPrice)     return -12;
        if (CPlayer_GetMoneyData(p) < 0) return -13;
        if (p->m_cMoney.m_nCash < nPrice)               return -14;
    } else if (nPay == 2) {                              // point
        nPrice = pEnch->m_nPoint;
        if (nPrice < 0)                  return -21;
        if (pIn->m_nPrice != nPrice)     return -22;
        if (CPlayer_GetMoneyData(p) < 0) return -23;
        if (p->m_cMoney.m_nPoint < nPrice)               return -24;
    } else {
        return -1;
    }

    CSituationTable* pSit = g_Load.GetRandomSituationTable(nUsed, GetMessRandom());
    if (pSit == 0) return -3;
    int nSituation = pSit->m_nCode;

    int nEquipType = GetItemEquipFromType(pItem->m_nCode / 1000000);
    COptionTable* pOpt = g_Load.GetRandomOptionTable(nEquipType, nUsed, GetMessRandom());
    if (pOpt == 0) return -3;
    int nOptCode = pOpt->m_nCode;

    int nValue = GetItemValue(pOpt, pIn->m_nType, GetMessRandom());
    pItem->m_nOptionCode[nUsed] = nSituation * 100000 + nOptCode * 100 + nValue;
    pItem->m_nState  = 2;
    pItem->m_nPrice += pOpt->m_nPrice;

    if (nPay == 1) { p->m_cMoney.m_nCash -= nPrice; if (CPlayer_PutMoneyData(p) < 0) return -13; }
    else           { p->m_cMoney.m_nPoint -= nPrice; if (CPlayer_PutMoneyData(p) < 0) return -23; }

    CSale sale;
    sale.m_nObjectSeq  = pItem->m_nItemSeq;
    sale.m_nObjectCode = pItem->m_nCode;
    sale.m_nObjectKind = 1;
    sale.m_nBuyKind    = nPay;
    sale.m_nSaleKind   = 4;
    sale.m_nPrice      = nPrice;
    sale.m_nAmount     = 1;
    g_Sql.InsertSaleLog(p, &sale);

    CSCUpdateItem ui;
    ui.m_nKind = 3;
    ui.m_cItemRow.m_nItemSeq   = pItem->m_nItemSeq;
    ui.m_cItemRow.m_nCode      = pItem->m_nCode;
    ui.m_cItemRow.m_nEquipKind = pItem->m_nEquipKind;
    ui.m_cItemRow.m_nLimit     = pItem->m_nLimit;
    ui.m_cItemRow.m_nGrade     = pItem->m_nGrade;
    ui.m_cItemRow.m_nAmount    = pItem->m_nAmount;
    ui.m_cItemRow.m_nPrice     = pItem->m_nPrice;
    for (int i = 0; i < 5; ++i) ui.m_cItemRow.m_nOptionCode[i] = pItem->m_nOptionCode[i];
    memcpy(ui.m_cEquipWear.m_data, &p->m_cItemOption, 0x158);
    PacketUpdateItem(p, &ui);
    return 0;
}

// =============================================================================
//                          CPlayer money / today-shop
// =============================================================================
// CPlayer::SpendMoney @08090d54 : CSale +9 buyKind (1 cash@0xb4 / 2 point@0xb8),
//   +0xc price. Returns 0 ok / negative reason.
int CPlayer::SpendMoney(CSale* pSale)
{
    int nPrice = pSale->m_nPrice;
    if (nPrice < 0)                       return -21;
    if (CPlayer_GetMoneyData(this) < 0)   return -23;
    if (pSale->m_nBuyKind == 2) {                        // point
        if (this->m_cMoney.m_nPoint < nPrice)        return -24;
        this->m_cMoney.m_nPoint -= nPrice;
        if (CPlayer_PutMoneyData(this) < 0) return -23;
    } else if (pSale->m_nBuyKind == 1) {                 // cash
        if (this->m_cMoney.m_nCash < nPrice)        return -14;
        this->m_cMoney.m_nCash -= nPrice;
        if (CPlayer_PutMoneyData(this) < 0) return -13;
    } else {
        return -1;
    }
    return 0;
}

// CPlayer::ChangeTodayShop @08087a06 : reseed the daily shop (fresh seed into the
// RNG seed source m_nTodaySeed, clear the today-bits), drop today's list, persist.
void CPlayer::ChangeTodayShop()
{
    m_nTodaySeed = g_Random.GetMessRandom();
    m_nTodayBit     = 0;
    m_cRandomShop.ClearTodayList();
    g_Sql.UpdateTodayField(this);
}

// =============================================================================
//  CLoad::GetRandomMission @0805ad2c -- random mission row (typed table not yet
//  pinned; returns 0 = no mission, used by CRoom::CreateMission).
// =============================================================================
void* CLoad::GetRandomMission() { return 0; }

// =============================================================================
//                           RNDSHOP CLIENT HANDLERS
// =============================================================================
static inline CRandomShop* SHOP(CPlayer* p) { return &p->m_cRandomShop; }

// BUY RANDOM ITEM  (PacketBuyRandomitem @080812a0, CM_BUY_RANDOMITEM 0x0C26)
void PacketBuyRandomitem(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCBuyRandomitem sc;
    if (pPlayer == 0 || pPacket == 0)
        return;
    char nRet = (char)SHOP(pPlayer)->BuyItem((CCSBuyRandomitem*)pPacket);
    memcpy(sc.m_cMoney.m_data, pPlayer->m_nEquipWear, 0x44);
    sc.m_nMoney[0] = pPlayer->m_cMoney.m_nCash;
    sc.m_nMoney[1] = pPlayer->m_cMoney.m_nPoint;
    sc.m_nMoney[2] = pPlayer->m_cMoney.m_nCredit;
    sc.m_nMoney[3] = pPlayer->m_cMoney.m_nClubPoint;
    sc.m_nIndex    = ((CCSBuyRandomitem*)pPacket)->m_nIndex;
    sc.m_nResponse = nRet;
    SendPlayer(pPlayer, &sc, (int)nRet);
}

// RANDOM SHOP ITEM LIST  (PacketRandomshopitemList @0808121e, 0x0C27)
void PacketRandomshopitemList(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCRandomshopitemList sc;
    if (pPlayer == 0 || pPacket == 0)
        return;
    char nRet = (char)SHOP(pPlayer)->GetItemList(&sc);
    SendPlayer(pPlayer, &sc, (int)nRet);
}

// REFRESH SHOP  (PacketRefreshShop @0808162e, CM_REFRESH_SHOP 0x0C29)
void PacketRefreshShop(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCRefreshShop sc;
    if (pPlayer == 0 || pPacket == 0)
        return;

    CSale sale;
    sale.m_nObjectSeq  = 0;
    sale.m_nObjectCode = 0xC29;
    sale.m_nObjectKind = 7;
    sale.m_nBuyKind    = ((CCSRefreshShop*)pPacket)->m_nReserved;
    sale.m_nSaleKind   = 1;
    if      (g_Config.m_nCompany == 100) sale.m_nPrice = 100;
    else if (g_Config.m_nCompany == 200) sale.m_nPrice = 1000;
    else                                 sale.m_nPrice = 500;
    sale.m_nAmount     = 1;

    int nResult = 0;
    if (pPlayer->SpendMoney(&sale) < 0)
        nResult = -3;
    else
    {
        sc.m_nMoney[0] = pPlayer->m_cMoney.m_nCash;
        sc.m_nMoney[1] = pPlayer->m_cMoney.m_nPoint;
        sc.m_nMoney[2] = pPlayer->m_cMoney.m_nCredit;
        sc.m_nMoney[3] = pPlayer->m_cMoney.m_nClubPoint;
        pPlayer->ChangeTodayShop();
        g_Sql.InsertSaleLog(pPlayer, &sale);
    }
    sc.m_nResponse = (char)nResult;
    SendPlayer(pPlayer, &sc, nResult);
}

// item.cpp forwards CM_ENCHANT_ITEM here.
int CRandomShop_EnchantItem(CPlayer* pPlayer, CCSEnchantItem* pIn)
{
    return SHOP(pPlayer)->EnchantItem(pIn);
}
