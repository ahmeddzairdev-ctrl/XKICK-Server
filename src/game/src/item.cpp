// =============================================================================
// item.cpp - Game ITEM / SHOP / MIX / GIFT / COUPON handlers.
// Reconstructed byte-faithfully from XKICK_Game1 (unstripped) via Ghidra.
//
// These are the TCP client handlers for the item-shop family (CM_* 0x0C1C..0x0C2D
// + sale-list 0x083E, item-info 0x0835, coupons 0x1221/0x1222). Each is a thin
// wrapper: it invokes the owning CPlayer/CRandomShop/CSql routine, then either
// returns the reason code via SendPlayer(...,reason) or builds the success reply
// (echoing the player's money/equip-wear block) and SendPlayer(...,0).
//
// The heavyweight item logic (CPlayer::EquipItem/BuyItem/SellItem/DivestItem/
// GiftItem/GetGift/ExchangeItem/RepairItem/GetBagItem/GetItemPointer/SetItemState,
// CRandomShop::EnchantItem) lives in the player/rndshop module; we forward-declare
// it (mirroring the binary's _ZN7CPlayer... members as free helpers operating on a
// CPlayer*) so this TU compiles standalone without editing Player.h.
//
// Portable: standard C++ only; all packet/player/item access is via struct members.
// =============================================================================
#include "Main.h"
#include "GameProtocolItem.h"
#include "Sql.h"
#include "GameLoad.h"
#include "Domain.h"
#include "GameItem.h"
#include "LogManager.h"
#include <cstring>

// ---- cross-module helpers (room/player module) ----
// g_Sql / g_Load are global OBJECTS (Global.h, pulled in via Main.h).
CPlayer* GetPlayerPointer(int nSeq);                       // player.cpp / room module

// ---- CPlayer item logic owned by the player/rndshop module (forward-declared) ----
int   CPlayer_EquipItem      (CPlayer*, int nItemSeq);     // _ZN7CPlayer9EquipItemEi
int   CPlayer_DivestItem     (CPlayer*, int nItemSeq);     // _ZN7CPlayer10DivestItemEi
int   CPlayer_BuyItem        (CPlayer*, CCSBuyItem*);      // _ZN7CPlayer7BuyItemEP10CCSBuyItem
int   CPlayer_SellItem       (CPlayer*, int nItemSeq);     // _ZN7CPlayer8SellItemEi
int   CPlayer_RepairItem     (CPlayer*, int nItemSeq);     // _ZN7CPlayer10RepairItemEi
int   CPlayer_GiftItem       (CPlayer*, CCSGiftItem*);     // _ZN7CPlayer8GiftItemEP11CCSGiftItem
int   CPlayer_GetGift        (CPlayer*, int nGiftSeq);     // _ZN7CPlayer7GetGiftEi
int   CPlayer_ExchangeItem   (CPlayer*, CCSExchangeItem*); // _ZN7CPlayer12ExchangeItemEP15CCSExchangeItem
CItem* CPlayer_GetItemPointer(CPlayer*, int nItemSeq);     // _ZN7CPlayer14GetItemPointerEi
CItem* CPlayer_GetBagItem    (CPlayer*, int nItemSeq);     // _ZN7CPlayer10GetBagItemEi
void  CPlayer_SetItemState   (CPlayer*, int nItemSeq, int nState); // _ZN7CPlayer12SetItemStateEii
int   CPlayer_GetMoneyData   (CPlayer*);                   // _ZN7CPlayer12GetMoneyDataEv
int   CPlayer_PutMoneyData   (CPlayer*);                   // _ZN7CPlayer12PutMoneyDataEv
void  CPlayer_GetShopItemList(CPlayer*, int kind, int page, int order, CSCShopItemList*);
                                                           // _ZN7CPlayer15GetShopItemListEiiiP...
void  CPlayer_SetEquipWear   (CPlayer*);                   // CPlayer::SetEquipWear
void  CPlayer_SetItemOption  (CPlayer*);                   // CPlayer::SetItemOption
int   CRandomShop_EnchantItem(CPlayer*, CCSEnchantItem*);  // _ZN11CRandomShop11EnchantItemE...

// =============================================================================
// SALE LIST  (PacketSaleList @0807610c, CM_SALE_LIST 0x083E)
// =============================================================================
void PacketSaleList(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCSaleList sc;
    CCSSaleList* cs = (CCSSaleList*)pPacket;
    char  nPeriod = cs->m_nPeriod;
    char  nPage   = cs->m_nPage;
    int   nRet    = g_Sql.GetSaleField(pPlayer, nPeriod, nPage, (CSCSaleList*)&sc);
    if (nRet >= 0)
    {
        sc.m_nPeriod = nPeriod;
        sc.m_nPage   = nPage;
        sc.m_nResponse = 0;
        SendPlayer(pPlayer, &sc, 0);
        return;
    }
    char nResp;
    if (nRet == -2) {
        nResp = (char)0xf4;
        LOGOUT_ERROR("PacketSaleList: GetSaleField=-2 seq(%d)\n", pPlayer->m_nPlayerSeq);
    } else if (nRet == -1) {
        nResp = (char)0xf5;
        LOGOUT_ERROR("PacketSaleList: GetSaleField=-1 seq(%d)\n", pPlayer->m_nPlayerSeq);
    } else if (nRet == -3) {
        nResp = (char)0xf3;
    } else {
        nResp = (char)0x9d;
    }
    sc.m_nResponse = nResp;
    SendPlayer(pPlayer, &sc, (int)nResp);
}

// =============================================================================
// ITEM INFO  (PacketItemInfo @0807551e, CM_ITEM_INFO 0x0835)
// Walks m_vItemList, packing up to 30 (0x1e) CItemRow's per CSCItemInfo page.
// =============================================================================
void PacketItemInfo(CPlayer* pPlayer)
{
    CSCItemInfo sc;                       // common Protocol.h (cmd 0x0835)
    const int BASE   = 0xe;               // header before rows (resp + count)
    const int STRIDE = 0x25;              // CItemRow size
    int  nCount = 0;
    // The Game serializes a 37-byte CItemRow per item into the opaque row block,
    // NOT the wider common CItemInfo.
    CItemRow* pRow = (CItemRow*)sc.m_cItemInfo;

    for (size_t i = 0; i < pPlayer->m_vItemList.size(); ++i)
    {
        CItem* pItem = pPlayer->m_vItemList[i];
        if (pItem == 0 || pItem->IsEmptyState() || pItem->m_nAmount == 0)
            continue;

        CItemRow* r = pRow + nCount;          // CItemRow is exactly STRIDE (0x25) bytes
        r->m_nItemSeq   = pItem->m_nItemSeq;
        r->m_nCode      = pItem->m_nCode;
        r->m_nEquipKind = pItem->m_nEquipKind;
        r->m_nLimit     = pItem->m_nLimit;
        r->m_nGrade     = pItem->m_nGrade;
        r->m_nAmount    = pItem->m_nAmount;
        r->m_nPrice     = pItem->m_nPrice;
        memcpy(r->m_nOptionCode, pItem->m_nOptionCode, 0x14);

        if (++nCount == 0x1e)
        {
            sc.m_nBodySize = BASE + 0x1e * STRIDE - 0xc;  // matches binary framing
            sc.m_nCount    = 0x1e;
            sc.m_nResponse = 0;
            SendPlayer(pPlayer, &sc, 0);
            nCount = 0;
        }
    }
    if (nCount != 0)
    {
        sc.m_nBodySize = nCount * STRIDE + BASE - 0xc;
        sc.m_nCount    = (char)nCount;
        sc.m_nResponse = 0;
        SendPlayer(pPlayer, &sc, 0);
    }
}

// =============================================================================
// SHOP ITEM LIST  (PacketShopItemList @0807b5bc, CM_SHOP_ITEM_LIST 0x0C1C)
// =============================================================================
void PacketShopItemList(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCShopItemList sc;
    CCSShopItemList* cs = (CCSShopItemList*)pPacket;
    int nKind = cs->m_nKind;
    if (nKind > 2)
    {
        if (nKind < 5)                    // kind 3,4: empty reply (no list build)
            goto reply;
        if (nKind == 5)                   // kind 5: ignored entirely
            return;
    }
    // kind <= 2 (1=all, 2=cash) or kind >= 6: build the paged list.
    CPlayer_GetShopItemList(pPlayer, nKind, cs->m_nPage, cs->m_nOrder, &sc);
reply:
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// UPDATE ITEM  (PacketUpdateItem @0807b638 / @0807b686, CM_UPDATE_ITEM 0x0C1D)
// Internal helper: pushes an inventory delta (add/remove) + equip-wear to a client.
// =============================================================================
void PacketUpdateItem(CPlayer* pPlayer, CSCUpdateItem* pPacket)
{
    pPacket->m_nResponse = 0;
    // body = base 0x17f adjusted by the equip-wear count byte (WEAR[0]); see binary.
    pPacket->m_nBodySize = (0x55 - pPacket->m_cEquipWear.m_data[0]) * 4 + pPacket->m_nBodySize - 0x17f;
    SendPlayer(pPlayer, pPacket, (int)pPacket->m_nResponse);
}

void PacketUpdateItem(CPlayer* pPlayer, CSCUpdateItem* pPacket, CItem* pItem)
{
    pPacket->m_cItemRow.m_nItemSeq   = pItem->m_nItemSeq;
    pPacket->m_cItemRow.m_nCode      = pItem->m_nCode;
    pPacket->m_cItemRow.m_nEquipKind = pItem->m_nEquipKind;
    pPacket->m_cItemRow.m_nLimit     = pItem->m_nLimit;
    pPacket->m_cItemRow.m_nGrade     = pItem->m_nGrade;
    pPacket->m_cItemRow.m_nAmount    = pItem->m_nAmount;
    pPacket->m_cItemRow.m_nPrice     = pItem->m_nPrice;
    for (int i = 0; i < 5; ++i)
        pPacket->m_cItemRow.m_nOptionCode[i] = pItem->m_nOptionCode[i];
    PacketUpdateItem(pPlayer, pPacket);
}

// =============================================================================
// EQUIP ITEM  (PacketEquipItem @0807b720, CM_EQUIP_ITEM 0x0C1E)
// =============================================================================
void PacketEquipItem(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCEquipItem sc;
    int nItemSeq = ((CCSEquipItem*)pPacket)->m_nItemSeq;
    int nRet = CPlayer_EquipItem(pPlayer, nItemSeq);
    if (nRet < 0)
    {
        char nResp = (char)nRet;
        if (nRet == -2)      nResp = (char)0xf4;
        else if (nRet == -1) { nResp = (char)0xf5;
            LOGOUT_ERROR("PacketEquipItem: EquipItem=-1 seq(%d)\n", pPlayer->m_nPlayerSeq); }
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_nItemSeq = nItemSeq;
    sc.m_nEquip   = 1;
    memcpy(sc.m_cEquipWear.m_data, &pPlayer->m_cItemOption, 0x158);
    memcpy(sc.m_cMoney.m_data,     (char*)pPlayer->m_nEquipWear, 0x44);
    sc.m_nResponse = 0;
    sc.m_nBodySize = (0x55 - pPlayer->m_cItemOption.m_nCount) * 4 + sc.m_nBodySize - 0x1a2;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// DIVEST ITEM  (PacketDivestItem @0807b8f2, CM_DIVEST_ITEM 0x0C1F)
// =============================================================================
void PacketDivestItem(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCDivestItem sc;
    int nItemSeq = ((CCSDivestItem*)pPacket)->m_nItemSeq;
    int nRet = CPlayer_DivestItem(pPlayer, nItemSeq);
    if (nRet < 0)
    {
        char nResp = (char)nRet;
        if (nRet == -2)      nResp = (char)0xf4;
        else if (nRet == -1) { nResp = (char)0xf5;
            LOGOUT_ERROR("PacketDivestItem: DivestItem=-1 seq(%d)\n", pPlayer->m_nPlayerSeq); }
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_nItemSeq = nItemSeq;
    sc.m_nEquip   = 0;
    memcpy(sc.m_cEquipWear.m_data, &pPlayer->m_cItemOption, 0x158);
    memcpy(sc.m_cMoney.m_data,     (char*)pPlayer->m_nEquipWear, 0x44);
    sc.m_nResponse = 0;
    sc.m_nBodySize = (0x55 - pPlayer->m_cItemOption.m_nCount) * 4 + sc.m_nBodySize - 0x1a2;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// BUY ITEM  (PacketBuyItem @0807bac4, CM_BUY_ITEM 0x0C20)
// =============================================================================
void PacketBuyItem(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCBuyItem sc;
    int nRet = CPlayer_BuyItem(pPlayer, (CCSBuyItem*)pPacket);
    char nResp = (char)nRet;
    if (nRet < 0)
    {
        if ((unsigned)(nRet + 2) < 2)
            LOGOUT_ERROR("PacketBuyItem: BuyItem=%d seq(%d)\n", nResp, pPlayer->m_nPlayerSeq);
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_nMoney[0] = pPlayer->m_cMoney.m_nCash;
    sc.m_nMoney[1] = pPlayer->m_cMoney.m_nPoint;
    sc.m_nMoney[2] = pPlayer->m_cMoney.m_nCredit;
    sc.m_nMoney[3] = pPlayer->m_cMoney.m_nClubPoint;
    sc.m_nAmount   = *(short*)&pPlayer->m_cShape;
    sc.m_nReserved = pPlayer->m_cShape.m_nUniform;
    sc.m_nResult   = nResp;
    memcpy(sc.m_cMoney.m_data, (char*)pPlayer->m_nEquipWear, 0x44);
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// SELL ITEM  (PacketSellItem @0807bfc6, CM_SELL_ITEM 0x0C2C)
// =============================================================================
void PacketSellItem(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCSellItem sc;
    int nItemSeq = ((CCSSellItem*)pPacket)->m_nItemSeq;
    int nRet = CPlayer_SellItem(pPlayer, nItemSeq);
    if (nRet < 0)
    {
        char nResp = (char)nRet;
        if ((unsigned)(nRet + 2) < 2)
            LOGOUT_ERROR("PacketSellItem: SellItem=%d seq(%d)\n", nResp, pPlayer->m_nPlayerSeq);
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_nItemSeq  = nItemSeq;
    sc.m_nReason   = nRet;
    sc.m_nMoney[0] = pPlayer->m_cMoney.m_nCash;
    sc.m_nMoney[1] = pPlayer->m_cMoney.m_nPoint;
    sc.m_nMoney[2] = pPlayer->m_cMoney.m_nCredit;
    sc.m_nMoney[3] = pPlayer->m_cMoney.m_nClubPoint;
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// REPAIR ITEM  (PacketRepairItem @0807c0e4, CM_REPAIR_ITEM 0x0C2D)
// =============================================================================
void PacketRepairItem(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCRepairItem sc;
    int nItemSeq = ((CCSRepairItem*)pPacket)->m_nItemSeq;
    int nRet = CPlayer_RepairItem(pPlayer, nItemSeq);
    if (nRet < 0)
    {
        char nResp = (char)nRet;
        if ((unsigned)(nRet + 2) < 2)
            LOGOUT_ERROR("PacketRepairItem: RepairItem=%d seq(%d)\n", nResp, pPlayer->m_nPlayerSeq);
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_nItemSeq  = nItemSeq;
    sc.m_nReason   = nRet;
    sc.m_nMoney[0] = pPlayer->m_cMoney.m_nCash;
    sc.m_nMoney[1] = pPlayer->m_cMoney.m_nPoint;
    sc.m_nMoney[2] = pPlayer->m_cMoney.m_nCredit;
    sc.m_nMoney[3] = pPlayer->m_cMoney.m_nClubPoint;
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// GIFT ITEM  (PacketGiftItem @080810d6, CM_GIFT_ITEM 0x0C21)
// =============================================================================
void PacketGiftItem(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCGiftItem sc;
    if (pPlayer == 0 || pPacket == 0)
        return;
    int nRet = CPlayer_GiftItem(pPlayer, (CCSGiftItem*)pPacket);
    if (nRet < 0)
    {
        char nResp = (char)nRet;
        if ((unsigned)(nRet + 2) < 2)
            LOGOUT_ERROR("PacketGiftItem: GiftItem=%d seq(%d)\n", nResp, pPlayer->m_nPlayerSeq);
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_nMoney[0] = pPlayer->m_cMoney.m_nCash;
    sc.m_nMoney[1] = pPlayer->m_cMoney.m_nPoint;
    sc.m_nMoney[2] = pPlayer->m_cMoney.m_nCredit;
    sc.m_nMoney[3] = pPlayer->m_cMoney.m_nClubPoint;
    memcpy(sc.m_sToName, ((CCSGiftItem*)pPacket)->m_sToName, 0xf);
    sc.m_nDate = ((CCSGiftItem*)pPacket)->m_cItemInfo.m_nCode;   // echoes the gifted item code at +0x99
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// GIFT LIST  (PacketGiftList @0807be5c, CM_GIFT_LIST 0x0C2A)
// =============================================================================
void PacketGiftList(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCGiftList sc;
    char nRet = (char)g_Sql.GetGiftList(pPlayer,
                       ((CCSGiftList*)pPacket)->m_nType,
                       ((CCSGiftList*)pPacket)->m_nPage, (CSCGiftList*)&sc);
    SendPlayer(pPlayer, &sc, (int)nRet);
}

// =============================================================================
// GET GIFT  (PacketGetGift @0807bed8, CM_GET_GIFT 0x0C2B)
// =============================================================================
void PacketGetGift(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCGetGift sc;
    int nGiftSeq = ((CCSGetGift*)pPacket)->m_nGiftSeq;
    int nRet = CPlayer_GetGift(pPlayer, nGiftSeq);
    if (nRet < 0)
    {
        char nResp = (char)nRet;
        if (nRet == -1)
            LOGOUT_ERROR("PacketGetGift: GetGift=-1 seq(%d)\n", pPlayer->m_nPlayerSeq);
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// EXCHANGE ITEM  (PacketExchangeItem @0807bc44, CM_EXCHANGE_ITEM 0x0C22)
// =============================================================================
void PacketExchangeItem(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCExchangeItem sc;
    int nRet = CPlayer_ExchangeItem(pPlayer, (CCSExchangeItem*)pPacket);
    if (nRet < 0)
    {
        char nResp = (char)nRet;
        if ((unsigned)(nRet + 2) < 2)
            LOGOUT_ERROR("PacketExchangeItem: ExchangeItem=%d seq(%d)\n", nResp, pPlayer->m_nPlayerSeq);
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_nMoney[0] = pPlayer->m_cMoney.m_nCash;
    sc.m_nMoney[1] = pPlayer->m_cMoney.m_nPoint;
    sc.m_nMoney[2] = pPlayer->m_cMoney.m_nCredit;
    sc.m_nMoney[3] = pPlayer->m_cMoney.m_nClubPoint;
    sc.m_nAmount   = *(short*)&pPlayer->m_cShape;
    sc.m_nReserved = pPlayer->m_cShape.m_nUniform;
    memcpy(sc.m_cMoney.m_data, (char*)pPlayer->m_nEquipWear, 0x44);
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// ENCHANT ITEM  (PacketEnchantItem @08081506, CM_ENCHANT_ITEM 0x0C28)
// =============================================================================
void PacketEnchantItem(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCEnchantItem sc;
    if (pPlayer == 0 || pPacket == 0)
        return;
    int nRet = CRandomShop_EnchantItem(pPlayer, (CCSEnchantItem*)pPacket);
    if (nRet < 0)
    {
        char nResp = (char)nRet;
        if ((unsigned)(nRet + 3) < 3)
            LOGOUT_ERROR("PacketEnchantItem: EnchantItem=%d seq(%d)\n", nResp, pPlayer->m_nPlayerSeq);
        SendPlayer(pPlayer, &sc, (int)nResp);
        return;
    }
    sc.m_nMoney[0] = pPlayer->m_cMoney.m_nCash;
    sc.m_nMoney[1] = pPlayer->m_cMoney.m_nPoint;
    sc.m_nMoney[2] = pPlayer->m_cMoney.m_nCredit;
    sc.m_nMoney[3] = pPlayer->m_cMoney.m_nClubPoint;
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// POST ITEM  (PacketPostItem @0807ee98, CM_POST_ITEM 0x0C23)
// Moves an item to another online player. If equipped, divests first; then
// CSql::PostItem persists the transfer and both clients are sent inventory deltas.
// =============================================================================
void PacketPostItem(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCPostItem sc;
    if (pPlayer == 0 || pPacket == 0)
        return;

    CCSPostItem* cs = (CCSPostItem*)pPacket;
    int nItemSeq   = cs->m_nItemSeq;
    int nTargetSeq = cs->m_nTargetSeq;

    // If the item is currently worn, divest it first (server-side echo).
    CItem* pBag = CPlayer_GetBagItem(pPlayer, nItemSeq);
    if (pBag && pBag->m_nEquipKind == 1)
    {
        CCSDivestItem cs;
        cs.m_nItemSeq = nItemSeq;
        PacketDivestItem(pPlayer, &cs);
    }

    int  nNewSeq = g_Sql.PostItem(pPlayer, nTargetSeq, nItemSeq);
    CItem* pItem = 0;
    char nResp;
    if (nNewSeq < 1)
        nResp = (char)0xfe;            // -2
    else
    {
        nResp = 0;
        pItem = CPlayer_GetBagItem(pPlayer, nItemSeq);
        if (pItem)
        {
            CPlayer_DivestItem(pPlayer, nItemSeq);
            CSCUpdateItem ui;
            ui.m_nKind = 2;            // remove
            // fill the item row, refresh equip-wear, then push the inventory delta.
            ui.m_cItemRow.m_nItemSeq   = pItem->m_nItemSeq;
            ui.m_cItemRow.m_nCode      = pItem->m_nCode;
            ui.m_cItemRow.m_nEquipKind = pItem->m_nEquipKind;
            ui.m_cItemRow.m_nLimit     = pItem->m_nLimit;
            ui.m_cItemRow.m_nGrade     = pItem->m_nGrade;
            ui.m_cItemRow.m_nAmount    = pItem->m_nAmount;
            ui.m_cItemRow.m_nPrice     = pItem->m_nPrice;
            for (int i = 0; i < 5; ++i)
                ui.m_cItemRow.m_nOptionCode[i] = pItem->m_nOptionCode[i];
            CPlayer_SetEquipWear(pPlayer);
            CPlayer_SetItemOption(pPlayer);
            memcpy(ui.m_cEquipWear.m_data, &pPlayer->m_cItemOption, 0x158);
            PacketUpdateItem(pPlayer, &ui);
            CPlayer_SetItemState(pPlayer, nItemSeq, 0);
        }
    }

    sc.m_nItemSeq = nItemSeq;
    memcpy(sc.m_cEquipWear.m_data, &pPlayer->m_cItemOption,   0x158);
    memcpy(sc.m_cMoney.m_data,     (char*)pPlayer->m_nEquipWear, 0x44);
    sc.m_nBodySize = (0x55 - pPlayer->m_cItemOption.m_nCount) * 4 + sc.m_nBodySize - 0x1a1;
    sc.m_nResponse = nResp;
    SendPlayer(pPlayer, &sc, (int)nResp);

    // Deliver the granted item to the (online) recipient.
    CPlayer* pTarget = GetPlayerPointer(nTargetSeq);
    if (pTarget && pItem)
    {
        CItem* pNew = new CItem();
        pNew->m_nState     = 1;
        pNew->m_nItemSeq   = nNewSeq;
        pNew->m_nCode      = pItem->m_nCode;
        pNew->m_nEquipKind = 0;
        pNew->m_nLimit     = pItem->m_nLimit;
        pNew->m_nGrade     = pItem->m_nGrade;
        pNew->m_nAmount    = pItem->m_nAmount;
        pNew->m_nPrice     = pItem->m_nPrice;
        for (int i = 0; i < 5; ++i)
            pNew->m_nOptionCode[i] = pItem->m_nOptionCode[i];
        pTarget->m_vItemList.push_back(pNew);

        CSCUpdateItem ui;
        ui.m_nKind = 1;               // add
        ui.m_cItemRow.m_nItemSeq   = pNew->m_nItemSeq;
        ui.m_cItemRow.m_nCode      = pNew->m_nCode;
        ui.m_cItemRow.m_nEquipKind = pNew->m_nEquipKind;
        ui.m_cItemRow.m_nLimit     = pNew->m_nLimit;
        ui.m_cItemRow.m_nGrade     = pNew->m_nGrade;
        ui.m_cItemRow.m_nAmount    = pNew->m_nAmount;
        ui.m_cItemRow.m_nPrice     = pNew->m_nPrice;
        for (int i = 0; i < 5; ++i)
            ui.m_cItemRow.m_nOptionCode[i] = pNew->m_nOptionCode[i];
        CPlayer_SetEquipWear(pTarget);
        CPlayer_SetItemOption(pTarget);
        memcpy(ui.m_cEquipWear.m_data, &pTarget->m_cItemOption, 0x158);
        PacketUpdateItem(pTarget, &ui);
    }
}

// =============================================================================
// MIX ITEM  (PacketMixItem1 @08080b2a / PacketMixItem2 @08080b30)
// Both are disabled no-op stubs in this build (the binary returns immediately).
// CSql::InsertMixItem @080b6eaa exists but is unreferenced by these handlers.
// =============================================================================
void PacketMixItem1(CPlayer* /*pPlayer*/, CHeadPacket* /*pPacket*/) { /* no-op (binary stub) */ }
void PacketMixItem2(CPlayer* /*pPlayer*/, CHeadPacket* /*pPacket*/) { /* no-op (binary stub) */ }

// =============================================================================
// UPDATE OPTION  (PacketUpdateOption @0807fd52, CM_UPDATE_OPTION 0x0C24)
// Re-pushes the player's equip-wear + money/option block after an option change.
// =============================================================================
void PacketUpdateOption(CPlayer* pPlayer, CHeadPacket* /*pPacket*/)
{
    CSCUpdateOption sc;
    if (pPlayer == 0)
        return;
    memcpy(sc.m_cEquipWear.m_data, &pPlayer->m_cItemOption,   0x158);
    memcpy(sc.m_cMoney.m_data,     (char*)pPlayer->m_nEquipWear, 0x44);
    sc.m_nResponse = 0;
    sc.m_nBodySize = (0x55 - pPlayer->m_cItemOption.m_nCount) * 4 + sc.m_nBodySize - 0x19d;
    SendPlayer(pPlayer, &sc, (int)sc.m_nResponse);
}

// =============================================================================
// COUPONS  (PacketCashCoupon @0807ce82 / PacketPointCoupon @0807cf6c)
// Consume a coupon item, grant +5000 cash (cash coupon) / +5000 point (point coupon).
// =============================================================================
void PacketCashCoupon(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCCashCoupon sc;
    if (pPlayer == 0 || pPacket == 0)
        return;
    int nItemSeq = ((CCSCashCoupon*)pPacket)->m_nItemSeq;
    CItem* pItem = CPlayer_GetItemPointer(pPlayer, nItemSeq);
    if (pItem == 0)
    {
        sc.m_nResponse = (char)0xfd;
        SendPlayer(pPlayer, &sc, -3);
        return;
    }
    CPlayer_GetMoneyData(pPlayer);
    pPlayer->m_cMoney.m_nCash += 5000;          // +5000 cash
    CPlayer_PutMoneyData(pPlayer);
    pItem->m_nState = 0;              // consume

    sc.m_nItemSeq  = nItemSeq;
    sc.m_nMoney[0] = pPlayer->m_cMoney.m_nCash;
    sc.m_nMoney[1] = pPlayer->m_cMoney.m_nPoint;
    sc.m_nMoney[2] = pPlayer->m_cMoney.m_nCredit;
    sc.m_nMoney[3] = pPlayer->m_cMoney.m_nClubPoint;
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

void PacketPointCoupon(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCPointCoupon sc;
    if (pPlayer == 0 || pPacket == 0)
        return;
    int nItemSeq = ((CCSPointCoupon*)pPacket)->m_nItemSeq;
    CItem* pItem = CPlayer_GetItemPointer(pPlayer, nItemSeq);
    if (pItem == 0)
    {
        sc.m_nResponse = (char)0xfd;
        SendPlayer(pPlayer, &sc, -3);
        return;
    }
    CPlayer_GetMoneyData(pPlayer);
    pPlayer->m_cMoney.m_nPoint += 5000;          // +5000 point
    CPlayer_PutMoneyData(pPlayer);
    pItem->m_nState = 0;              // consume

    sc.m_nItemSeq  = nItemSeq;
    sc.m_nMoney[0] = pPlayer->m_cMoney.m_nCash;
    sc.m_nMoney[1] = pPlayer->m_cMoney.m_nPoint;
    sc.m_nMoney[2] = pPlayer->m_cMoney.m_nCredit;
    sc.m_nMoney[3] = pPlayer->m_cMoney.m_nClubPoint;
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// =============================================================================
// REWARD ITEM  (PacketRewardItem @0807bdba)  -- quest/event item grant echo.
// Takes a freshly-built runtime CItem; memcpy's the first 0x25 bytes onto the wire.
// On failure (seq < 0) the CItem is freed.
// =============================================================================
void PacketRewardItem(CPlayer* pPlayer, CItem* pItem)
{
    CSCRewardItem sc;
    if (pPlayer == 0)
        return;
    memcpy(sc.m_cItem, pItem, 0x25);
    if (pItem->m_nItemSeq < 0)
    {
        sc.m_nResponse = (char)pItem->m_nItemSeq;
        SendPlayer(pPlayer, &sc, (int)sc.m_nResponse);
        delete pItem;
        return;
    }
    sc.m_nResponse = 0;
    SendPlayer(pPlayer, &sc, 0);
}

// NOTE: PacketBuyRandomitem @080812a0 / PacketRandomshopitemList @0808121e /
// PacketRefreshShop @0808162e are RNDSHOP-owned (see packet.h). They delegate to the
// embedded CRandomShop (CPlayer+0x408) and the CSCRandomshopitemList reply type,
// which the rndshop module defines. They are therefore reconstructed in that module,
// not here, to avoid duplicating its types. The byte-exact logic was recovered:
//   * BuyRandomitem -> CRandomShop::BuyItem, echo money + the 0x44 block + index byte.
//   * Randomshopitemlist -> CRandomShop::GetItemList (opaque rndshop list body).
//   * RefreshShop -> build a CSale {kind 7, buyKind from pkt+0xc, sale 1, amount 1,
//     price 100/1000/500 by g_Config skin 100/200/else}, CPlayer::SpendMoney, then
//     ChangeTodayShop + InsertSaleLog; reply response + 4 money ints (body 0x11).

// =============================================================================
// ITEM-TABLE PRICE/ATTRIBUTE GETTERS  (g_Load.GetItemTable(code) -> CItemTable*)
// Faithful: cash/point prices are round(Discount * unitPrice / 100); exchange is
// round(Exchange * Cash / 100). money-kind (Sell): 1 cash-only, 2 point-only.
// =============================================================================
static int  ItemRound(double v)         { return (int)(v + 0.5); }

int GetItemAmount(int nCode)
{
    CItemTable* t = g_Load.GetItemTable(nCode);
    return (t == 0) ? 1 : (int)t->m_nAmount;
}
int GetItemCash(int nCode)
{
    CItemTable* t = g_Load.GetItemTable(nCode);
    if (t == 0)            return -1;
    if (t->m_nSell == 2)   return -2;        // point-only
    return ItemRound((double)((int)t->m_nDiscount * t->m_nCash) / 100.0);
}
int GetItemPoint(int nCode)
{
    CItemTable* t = g_Load.GetItemTable(nCode);
    if (t == 0)            return -1;
    if (t->m_nSell == 1)   return -2;        // cash-only
    return ItemRound((double)((int)t->m_nDiscount * t->m_nPoint) / 100.0);
}
int GetItemExchange(int nCode)
{
    CItemTable* t = g_Load.GetItemTable(nCode);
    if (t == 0)            return -1;
    return ItemRound((double)((int)t->m_nExchange * t->m_nCash) / 100.0);
}
int GetItemSkin(int nCode)               // _Z11GetItemSkini (NB: int overload)
{
    CItemTable* t = g_Load.GetItemTable(nCode);
    return (t == 0) ? 0xb : (int)t->m_nColor;
}

// ---- inventory helpers (file-local; mirror CPlayer::GetItemCount/GetBagItemByCode) ----
static int Item_GetItemCount(CPlayer* p)
{
    int n = 0;
    for (size_t i = 0; i < p->m_vItemList.size(); ++i)
    {
        CItem* it = p->m_vItemList[i];
        if (it && !it->IsEmptyState()) ++n;
    }
    return n;
}
static CItem* Item_GetBagItemByCode(CPlayer* p, int nCode)
{
    for (size_t i = 0; i < p->m_vItemList.size(); ++i)
    {
        CItem* it = p->m_vItemList[i];
        if (it && !it->IsEmptyState() && it->m_nCode == nCode) return it;
    }
    return 0;
}

// =============================================================================
// SHOP ITEM LIST  (CPlayer::GetShopItemList @0808bc66 / GetShopItemTotalPage @0808bebc)
// Iterates the parsed item table (sorted by code), filters by kind + gender +
// hidden + shop-enable, dedups items sharing the same code/10 group, paginates 6
// 9-byte rows. NOTE: the binary writes price 0 for every shop row (only code is
// filled); `order` (4th arg) is the real page index, `page` (3rd) is unused.
// =============================================================================
static int Shop_GetTotalPage(CPlayer* p, int kind)
{
    int nLastCode = 0, nCount = 0;
    char nGender = p->m_cShape.m_nGender;
    for (std::map<int, CItemTable*>::iterator it = g_Load.m_mItemTable.begin();
         it != g_Load.m_mItemTable.end(); ++it)
    {
        CItemTable* t = it->second;
        if (t == 0 || t->m_nFree == 1 || t->m_nSell == 0
            || nLastCode / 10 == t->m_nCode / 10)
            continue;
        nLastCode = t->m_nCode;
        if (t->m_nGender != 0 && t->m_nGender != nGender) continue;

        bool accept;
        if (kind == 1)      accept = true;
        else if (kind == 2) accept = (t->m_nNew == 1);
        else                accept = ((short)kind == t->m_nType);
        if (accept) ++nCount;
    }
    return (nCount % 6 == 0) ? (nCount / 6) : (nCount / 6 + 1);
}

void CPlayer_GetShopItemList(CPlayer* p, int kind, int /*page (unused)*/, int order,
                             CSCShopItemList* out)
{
    for (int i = 0; i < 6; ++i)
        out->m_cItemData[i].m_nCode = 0;

    int nTotalPage = Shop_GetTotalPage(p, kind);
    int nPage = order;
    if (nPage < 0)                 nPage = nTotalPage - 1;
    else if (nPage > nTotalPage-1) nPage = 0;

    int  nSkip     = nPage * 6;
    int  nLastCode = 0;
    int  nMatch    = 0;
    int  nRows     = 0;
    char nGender   = p->m_cShape.m_nGender;

    for (std::map<int, CItemTable*>::iterator it = g_Load.m_mItemTable.begin();
         it != g_Load.m_mItemTable.end(); ++it)
    {
        CItemTable* t = it->second;
        if (t == 0 || t->m_nFree == 1 || t->m_nSell == 0
            || (t->m_nGender != 0 && t->m_nGender != nGender)
            || nLastCode / 10 == t->m_nCode / 10)
            continue;
        nLastCode = t->m_nCode;
        if (nRows > 5) break;

        bool accept;
        if (kind == 1)      accept = true;
        else if (kind == 2) accept = (t->m_nNew == 1);
        else                accept = ((short)kind == t->m_nType);

        if (accept)
        {
            if (nSkip <= nMatch)
            {
                out->m_cItemData[nRows].m_nCode     = t->m_nCode;
                out->m_cItemData[nRows].m_nPrice    = 0;
                out->m_cItemData[nRows].m_nReserved = 0;
                ++nRows;
            }
            ++nMatch;
        }
    }
    out->m_nPage    = (out->m_cItemData[0].m_nCode == 0) ? 0 : (short)nPage;
    out->m_nMaxPage = (short)nTotalPage;
}

// =============================================================================
// EQUIP / DIVEST  (CPlayer::EquipItem @0808c04c / DivestItem @0808c244)
// =============================================================================
int CPlayer_EquipItem(CPlayer* p, int nItemSeq)
{
    CItem* pFound = 0;
    for (size_t i = 0; i < p->m_vItemList.size(); ++i) {
        CItem* it = p->m_vItemList[i];
        if (it && !it->IsEmptyState() && it->m_nItemSeq == nItemSeq) { pFound = it; break; }
    }
    if (pFound == 0) return -2;
    for (size_t i = 0; i < p->m_vItemList.size(); ++i) {
        CItem* it = p->m_vItemList[i];
        if (it && !it->IsEmptyState() &&
            pFound->m_nCode / 1000000 == it->m_nCode / 1000000) {
            it->m_nEquipKind = 0;
            it->m_nState     = 2;
        }
    }
    pFound->m_nEquipKind = 1;
    pFound->m_nState     = 2;
    p->SetEquipWear();
    p->SetItemOption();
    return (int)(char)pFound->m_nEquipKind;        // == 1
}

int CPlayer_DivestItem(CPlayer* p, int nItemSeq)
{
    CItem* pFound = 0;
    for (size_t i = 0; i < p->m_vItemList.size(); ++i) {
        CItem* it = p->m_vItemList[i];
        if (it && !it->IsEmptyState() && it->m_nItemSeq == nItemSeq) { pFound = it; break; }
    }
    if (pFound == 0) return -2;
    pFound->m_nEquipKind = 0;
    pFound->m_nState     = 2;
    p->SetEquipWear();
    p->SetItemOption();
    return (int)(char)pFound->m_nEquipKind;        // == 0
}

// =============================================================================
// BUY ITEM  (CPlayer::BuyItem @0808c45e)
// =============================================================================
int CPlayer_BuyItem(CPlayer* p, CCSBuyItem* pIn)
{
    CSCUpdateItem sc;
    if (Item_GetItemCount(p) >= 0x51) return -3;            // inventory full (>80)

    int nCode = pIn->m_nCode;
    CItemTable* pTable = g_Load.GetItemTable(nCode);
    if (pTable == 0) {
        LOGOUT_ERROR("BuyItem: GetItemTable(%d) null\n", nCode);
        return -4;
    }

    char nBuyKind = pIn->m_nBuyKind;
    int  nPrice;
    if (nBuyKind == 1) {                                    // cash
        nPrice = GetItemCash(nCode);
        if (nPrice < 0)                       return -0xb;
        if (pIn->m_nPrice != nPrice)          return -0xc;
        if (CPlayer_GetMoneyData(p) < 0)      return -0xd;
        if (p->m_cMoney.m_nCash < nPrice)               return -0xe;
        p->m_cMoney.m_nCash -= nPrice;
        if (CPlayer_PutMoneyData(p) < 0)      return -0xd;
    } else if (nBuyKind == 2) {                             // point
        nPrice = GetItemPoint(nCode);
        if (nPrice < 0)                       return -0x15;
        if (pIn->m_nPrice != nPrice)          return -0x16;
        if (CPlayer_GetMoneyData(p) < 0)      return -0x17;
        if (p->m_cMoney.m_nPoint < nPrice)               return -0x18;
        p->m_cMoney.m_nPoint -= nPrice;
        if (CPlayer_PutMoneyData(p) < 0)      return -0x17;
    } else {
        return -1;
    }

    int nCat  = nCode / 1000000;
    int nCat2 = (nCode / 100000000) * 100;

    CItem* pCur = Item_GetBagItemByCode(p, nCode);
    bool bSame = false;
    if (pCur) {
        bSame = true;
        for (int i = 0; i < 5; ++i)
            if (pCur->m_nOptionCode[i] != pIn->m_nOptionCode[i]) { bSame = false; break; }
    }

    short nAmount;
    if (pTable->m_nSum == 1 && bSame) {                     // stackable + same options
        sc.m_nKind = 3;
        nAmount = (short)GetItemAmount(nCode);
        pCur->m_nState = 2;
        if (5000 < (int)((unsigned short)pCur->m_nAmount + nAmount)) return -3;
        pCur->m_nAmount = (short)(pCur->m_nAmount + nAmount);
    } else {
        sc.m_nKind = 1;
        CItem* pNew = new CItem();
        if (pNew == 0) return -1;
        pCur = pNew;
        nAmount = (short)GetItemAmount(nCode);
        pNew->m_nState     = 1;
        pNew->m_nItemSeq   = 0;
        pNew->m_nCode      = nCode;
        pNew->m_nEquipKind = 0;
        pNew->m_nAmount    = nAmount;
        for (int i = 0; i < 5; ++i)
            pNew->m_nOptionCode[i] = pIn->m_nOptionCode[i];

        if (nCat == 0x65) {                                // skin item: insert+equip+updateskin
            p->m_cShape.m_nSkin = (char)GetItemSkin(pNew->m_nCode);
            pNew->m_nItemSeq = g_Sql.InsertItemField(p, pNew);
            if (pNew->m_nItemSeq < 0) return -2;
            p->m_vItemList.push_back(pNew);
            CPlayer_EquipItem(p, pNew->m_nItemSeq);
            if (g_Sql.UpdateSkinField(p) < 0) return -2;
        } else if (nCat2 == 500 || nCat2 == 600) {         // bag-only
            pNew->m_nItemSeq = g_Sql.InsertItemField(p, pNew);
            if (pNew->m_nItemSeq < 0) return -2;
            p->m_vItemList.push_back(pNew);
        } else if (nCat2 != 800) {                         // insert + auto-equip
            pNew->m_nItemSeq = g_Sql.InsertItemField(p, pNew);
            if (pNew->m_nItemSeq < 0) return -2;
            p->m_vItemList.push_back(pNew);
            CPlayer_EquipItem(p, pNew->m_nItemSeq);
        } else {
            delete pNew;                                   // nCat2==800: never inserted
            pCur = 0;
        }
    }

    if (pCur)
    {
        CSale sale;
        sale.m_nObjectSeq  = pCur->m_nItemSeq;
        sale.m_nObjectCode = pCur->m_nCode;
        sale.m_nObjectKind = 1;
        sale.m_nBuyKind    = nBuyKind;
        sale.m_nSaleKind   = 1;
        sale.m_nPrice      = pIn->m_nPrice;
        sale.m_nAmount     = nAmount;
        g_Sql.InsertSaleLog(p, &sale);

        sc.m_cItemRow.m_nItemSeq   = pCur->m_nItemSeq;
        sc.m_cItemRow.m_nCode      = pCur->m_nCode;
        sc.m_cItemRow.m_nEquipKind = pCur->m_nEquipKind;
        sc.m_cItemRow.m_nLimit     = pCur->m_nLimit;
        sc.m_cItemRow.m_nGrade     = pCur->m_nGrade;
        sc.m_cItemRow.m_nAmount    = pCur->m_nAmount;
        sc.m_cItemRow.m_nPrice     = pCur->m_nPrice;
        for (int i = 0; i < 5; ++i) sc.m_cItemRow.m_nOptionCode[i] = pCur->m_nOptionCode[i];
        memcpy(sc.m_cEquipWear.m_data, &p->m_cItemOption, 0x158);
        PacketUpdateItem(p, &sc);
        return (int)(char)pCur->m_nEquipKind;
    }
    return 0;
}

// =============================================================================
// SELL ITEM  (CPlayer::SellItem @0808d99c)  -- 0.2*price in POINT.
// =============================================================================
int CPlayer_SellItem(CPlayer* p, int nItemSeq)
{
    CItem* pItem = CPlayer_GetItemPointer(p, nItemSeq);
    if (pItem == 0)              return -3;
    if (pItem->m_nEquipKind != 0) return -4;               // currently equipped
    pItem->m_nState = 4;
    int nGain = (int)(0.2f * (float)pItem->m_nPrice + 0.5f);
    if (CPlayer_GetMoneyData(p) < 0) return -2;
    p->m_cMoney.m_nPoint += nGain;
    if (g_Sql.UpdateMoneyField(p) < 0) return -2;
    return nGain;
}

// =============================================================================
// REPAIR ITEM  (CPlayer::RepairItem @0808da68)  -- (maxDur - amount)*20 POINT.
// =============================================================================
int CPlayer_RepairItem(CPlayer* p, int nItemSeq)
{
    CItem* pItem = CPlayer_GetItemPointer(p, nItemSeq);
    if (pItem == 0)            return -3;
    if (pItem->m_nAmount == -1) return -4;
    CItemTable* pTable = g_Load.GetItemTable(pItem->m_nCode);
    if (pTable == 0) {
        LOGOUT_ERROR("RepairItem: GetItemTable(%d) null\n", pItem->m_nCode);
        return -2;
    }
    int nCost = ((int)(unsigned short)pTable->m_nAmount
               - (int)(unsigned short)pItem->m_nAmount) * 0x14;
    if (CPlayer_GetMoneyData(p) < 0) return -2;
    if (p->m_cMoney.m_nPoint < nCost)            return -5;
    p->m_cMoney.m_nPoint -= nCost;
    if (g_Sql.UpdateMoneyField(p) < 0) return -2;
    return nCost;
}

// =============================================================================
// GIFT ITEM  (CPlayer::GiftItem @0808cbd6)
// =============================================================================
int CPlayer_GiftItem(CPlayer* p, CCSGiftItem* pIn)
{
    int nCode = pIn->m_cItemInfo.m_nCode;
    CItemTable* pTable = g_Load.GetItemTable(nCode);
    if (pTable == 0) {
        LOGOUT_ERROR("GiftItem: GetItemTable(%d) null\n", nCode);
        return -4;
    }
    int nTargetSeq = g_Sql.GetPlayerSeq(pIn->m_sToName);
    int nTargetMem = g_Sql.GetMemberSeq(nTargetSeq);
    if (nTargetMem == 0 || nTargetSeq == 0) return -5;

    char nBuyKind = pIn->m_nBuyKind;
    int  nPrice   = pIn->m_cItemInfo.m_nPrice;
    if (nBuyKind == 1) {
        if (nPrice < 0)                  return -0xb;
        if (CPlayer_GetMoneyData(p) < 0) return -0xd;
        if (p->m_cMoney.m_nCash < nPrice)          return -0xe;
        p->m_cMoney.m_nCash -= nPrice;
        if (CPlayer_PutMoneyData(p) < 0) return -0xd;
    } else if (nBuyKind == 2) {
        if (nPrice < 0)                  return -0x15;
        if (CPlayer_GetMoneyData(p) < 0) return -0x17;
        if (p->m_cMoney.m_nPoint < nPrice)          return -0x18;
        p->m_cMoney.m_nPoint -= nPrice;
        if (CPlayer_PutMoneyData(p) < 0) return -0x17;
    } else {
        return -1;
    }

    p->m_cRandomShop.CreateShop();
    int nIdx = p->m_cRandomShop.GetShopIndex((CItemInfo*)&pIn->m_cItemInfo);
    if (nIdx != -1) {
        p->m_cRandomShop.SetTodayBit(nIdx, true);
        if (g_Sql.UpdateTodayField(p) < 0) return -2;
    }

    CItem gift;
    gift.m_nItemSeq   = 0;
    gift.m_nCode      = nCode;
    gift.m_nEquipKind = 0;
    gift.m_nLimit     = pIn->m_cItemInfo.m_nLimit;
    gift.m_nGrade     = pIn->m_cItemInfo.m_nGrade;
    gift.m_nAmount    = pIn->m_cItemInfo.m_nAmount;
    gift.m_nPrice     = pIn->m_cItemInfo.m_nPrice;
    gift.m_nOptionCode[0] = pIn->m_cItemInfo.m_nOptionCode[0];
    gift.m_nOptionCode[1] = pIn->m_cItemInfo.m_nOptionCode[1];
    gift.m_nOptionCode[2] = pIn->m_cItemInfo.m_nOptionCode[2];
    gift.m_nOptionCode[3] = pIn->m_cItemInfo.m_nOptionCode[3];
    gift.m_nOptionCode[4] = pIn->m_cItemInfo.m_nOptionCode[4];
    gift.m_nState  = 3;
    gift.m_nRandom = 0;
    gift.m_nOrder  = 0;

    int nNewSeq = g_Sql.InsertItemField(nTargetMem, nTargetSeq, &gift);
    if (nNewSeq < 0) return -2;
    gift.m_nItemSeq = nNewSeq;

    g_Sql.InsertGiftField(nTargetSeq, p->m_nPlayerSeq, nNewSeq, pIn->m_sGiftMsg);

    CSale sale;
    sale.m_nObjectSeq  = nNewSeq;
    sale.m_nObjectCode = nCode;
    sale.m_nObjectKind = 1;
    sale.m_nBuyKind    = nBuyKind;
    sale.m_nSaleKind   = 2;
    sale.m_nPrice      = pIn->m_cItemInfo.m_nPrice;
    sale.m_nAmount     = (int)(unsigned short)gift.m_nAmount;
    g_Sql.InsertSaleLog(p, &sale);
    return 0;
}

// =============================================================================
// GET GIFT  (CPlayer::GetGift @0808d658)
// =============================================================================
int CPlayer_GetGift(CPlayer* p, int nGiftSeq)
{
    int nRet = g_Sql.GetGiftItemField(p, nGiftSeq);
    if (nRet < 0) return nRet;
    CItem* pItem = CPlayer_GetBagItem(p, nGiftSeq);
    if (pItem == 0) return nRet;
    CSCUpdateItem sc;
    sc.m_nKind = 1;
    sc.m_cItemRow.m_nItemSeq   = pItem->m_nItemSeq;
    sc.m_cItemRow.m_nCode      = pItem->m_nCode;
    sc.m_cItemRow.m_nEquipKind = pItem->m_nEquipKind;
    sc.m_cItemRow.m_nLimit     = pItem->m_nLimit;
    sc.m_cItemRow.m_nGrade     = pItem->m_nGrade;
    sc.m_cItemRow.m_nAmount    = pItem->m_nAmount;
    sc.m_cItemRow.m_nPrice     = pItem->m_nPrice;
    for (int i = 0; i < 5; ++i) sc.m_cItemRow.m_nOptionCode[i] = pItem->m_nOptionCode[i];
    memcpy(sc.m_cEquipWear.m_data, &p->m_cItemOption, 0x158);
    PacketUpdateItem(p, &sc);
    return 0;
}

// =============================================================================
// EXCHANGE ITEM  (CPlayer::ExchangeItem @0808d10c)  -- re-skin an equipped item.
// =============================================================================
int CPlayer_ExchangeItem(CPlayer* p, CCSExchangeItem* pIn)
{
    int nCode = pIn->m_nCode;
    CItem* pItem = 0;
    for (size_t i = 0; i < p->m_vItemList.size(); ++i) {
        CItem* it = p->m_vItemList[i];
        if (it && !it->IsEmptyState() && it->m_nEquipKind != 0 &&
            it->m_nCode / 1000000 == nCode / 1000000) { pItem = it; break; }
    }
    if (pItem == 0) return -1;

    int nCat = nCode / 1000000;
    CItemTable* pTable = g_Load.GetItemTable(nCode);
    if (pTable == 0) {
        LOGOUT_ERROR("ExchangeItem: GetItemTable(%d) null\n", nCode);
        return -4;
    }

    char nBuyKind = pIn->m_nBuyKind;
    int  nPrice;
    if (nBuyKind == 1) {
        nPrice = GetItemExchange(nCode);
        if (nPrice < 0)                       return -0xb;
        if (pIn->m_nPrice != nPrice)          return -0xc;
        if (CPlayer_GetMoneyData(p) < 0)      return -0xd;
        if (p->m_cMoney.m_nCash < nPrice)               return -0xe;
        p->m_cMoney.m_nCash -= nPrice;
        if (CPlayer_PutMoneyData(p) < 0)      return -0xd;
    } else if (nBuyKind == 2) {
        nPrice = GetItemExchange(nCode);
        if (nPrice < 0)                       return -0x15;
        if (pIn->m_nPrice != nPrice)          return -0x16;
        if (CPlayer_GetMoneyData(p) < 0)      return -0x17;
        if (p->m_cMoney.m_nPoint < nPrice)               return -0x18;
        p->m_cMoney.m_nPoint -= nPrice;
        if (CPlayer_PutMoneyData(p) < 0)      return -0x17;
    } else {
        return -1;
    }

    if (nCat == 0x65) {
        p->m_nFreeWear[0] = nCode;
        p->m_cShape.m_nSkin = (char)GetItemSkin(nCode);
    } else if (nCat == 0x66) {
        p->m_nFreeWear[1] = nCode;
    } else {
        pItem->m_nCode  = nCode;
        pItem->m_nState = 2;
    }
    CPlayer_EquipItem(p, pItem->m_nItemSeq);

    CSale sale;
    sale.m_nObjectSeq  = pItem->m_nItemSeq;
    sale.m_nObjectCode = nCode;
    sale.m_nObjectKind = 1;
    sale.m_nBuyKind    = nBuyKind;
    sale.m_nSaleKind   = 3;
    sale.m_nPrice      = pIn->m_nPrice;
    sale.m_nAmount     = (int)(unsigned short)pItem->m_nAmount;
    g_Sql.InsertSaleLog(p, &sale);

    CSCUpdateItem sc;
    sc.m_nKind = 3;
    sc.m_cItemRow.m_nItemSeq   = pItem->m_nItemSeq;
    sc.m_cItemRow.m_nCode      = pItem->m_nCode;
    sc.m_cItemRow.m_nEquipKind = pItem->m_nEquipKind;
    sc.m_cItemRow.m_nLimit     = pItem->m_nLimit;
    sc.m_cItemRow.m_nGrade     = pItem->m_nGrade;
    sc.m_cItemRow.m_nAmount    = pItem->m_nAmount;
    sc.m_cItemRow.m_nPrice     = pItem->m_nPrice;
    for (int i = 0; i < 5; ++i) sc.m_cItemRow.m_nOptionCode[i] = pItem->m_nOptionCode[i];
    memcpy(sc.m_cEquipWear.m_data, &p->m_cItemOption, 0x158);
    PacketUpdateItem(p, &sc);
    return 0;
}
