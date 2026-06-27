// =============================================================================
// XKICK_Game - GameProtocolItem.h
// #pragma pack(1) WIRE structs for the item / card / mix / gift / coupon / shop
// packets owned by the item & card domain modules (item.cpp / card.cpp). These are
// the CCS*/CSC* layouts NOT already shared in common Protocol.h.
//
// Every layout below was derived byte-exact from XKICK_Game1 (Ghidra):
//   * the CSC*::C1() ctor sets the command id + m_nBodySize (recorded in each comment),
//   * the matching Packet* handler's field stores pin every member offset,
//   * the CSql loaders (GetGiftList @080b1a6c / GetSaleField @080b4492) pin the
//     paginated sub-record (CGiftData / CSaleData) strides.
//
// The on-the-wire inventory row is a Game-specific 37-byte record (CItemRow) -- it
// is NOT the common Protocol.h CItemInfo (which carries slot/slotcode). The Game
// serializes seq/code/equip/limit/grade/amount/price + 5 option ints.
//
// Equip-wear + money trailers: many CSC item replies append the player's equip-wear
// block (CPlayer+0x168, 0x158 bytes) and/or the 0x44 money/skin block (CPlayer+0x434).
// Those are reproduced here as opaque fixed-size arrays so the framing stays exact;
// their internal maps belong to the player/room module.
// =============================================================================
#ifndef _GAME_PROTOCOL_ITEM_H_
#define _GAME_PROTOCOL_ITEM_H_

#include <cstddef>       // offsetof
#include "Struct.h"      // CHeadPacket, CSale, CGift, CMoney, PLAYER_NAME_SIZE
#include "Command.h"     // CM_* ids

#ifndef CM_SHOP_ITEM_LIST
#define CM_SHOP_ITEM_LIST  0x0C1C
#endif
#ifndef CM_POST_ITEM
#define CM_POST_ITEM       0x0C23
#endif
#ifndef CM_UPDATE_OPTION
#define CM_UPDATE_OPTION   0x0C24
#endif
#ifndef CM_REWARD_ITEM
#define CM_REWARD_ITEM     0x0C25
#endif

#pragma pack(push,1)

// -----------------------------------------------------------------------------
// Shared on-the-wire item row (Game form). 0x25 = 37 bytes. Built by PacketItemInfo
// @0807551e and PacketUpdateItem @0807b686 from the runtime CItem.
// -----------------------------------------------------------------------------
class CItemRow
{ public:
    int   m_nItemSeq;        // +0x00  CItem+0x00
    int   m_nCode;           // +0x04  CItem+0x04
    char  m_nEquipKind;      // +0x08  CItem+0x08
    char  m_nLimit;          // +0x09  CItem+0x09
    char  m_nGrade;          // +0x0a  CItem+0x0a
    short m_nAmount;         // +0x0b  CItem+0x0c (short)
    int   m_nPrice;          // +0x0d  CItem+0x10
    int   m_nOptionCode[5];  // +0x11  CItem+0x14..0x24   -> total 0x25 (37) bytes
};

// Opaque player trailers (kept fixed-size for byte-exact framing).
class CEquipWear { public: char m_data[0x158]; };  // CPlayer+0x168
class CMoneyBlock{ public: char m_data[0x44];  };  // CPlayer+0x434

// =============================================================================
// ITEM INFO  (PacketItemInfo @0807551e)
//   CSCItemInfo (cmd 0x0835, REUSED from common Protocol.h header decl, but the Game
//   fills it with up to 30 CItemRow's of 0x25 bytes each + count). The handler builds
//   a variable body of 30*0x25 rows; sender sets m_nBodySize at runtime.
// =============================================================================
#ifndef MAX_ITEM_PAGE
#define MAX_ITEM_PAGE 30
#endif

// =============================================================================
// SALE LIST  (PacketSaleList @0807610c ; CSql::GetSaleField @080b4492)
//   CSCSaleList: cmd 0x083E, body 0xae (174). 10 sale rows of 0x11 (17) bytes.
// =============================================================================
class CSaleData       // stride 0x11 (17)
{ public:
    int   m_nCode;           // +0x00  GetSaleField +0x10
    char  m_nObjectKind;     // +0x04  +0x14
    char  m_nBuyKind;        // +0x05  +0x15
    char  m_nSaleKind;       // +0x06  +0x16
    int   m_nPrice;          // +0x07  +0x17
    short m_nAmount;         // +0x0b  +0x1b
    int   m_nSaleDate;       // +0x0d  +0x1d  -> 17 bytes
};
class CSCSaleList : public CHeadPacket
{ public:
    char      m_nResponse;   // +0x0c
    char      m_nPeriod;     // +0x0d
    char      m_nPage;       // +0x0e
    char      m_nMaxPage;    // +0x0f
    CSaleData m_cSaleData[10]; // +0x10  (10 * 17 = 170) -> body 0xae
    CSCSaleList() : CHeadPacket(CM_SALE_LIST) { m_nBodySize = 0xae; }
};

// =============================================================================
// SHOP ITEM LIST  (PacketShopItemList @0807b5bc ; CPlayer::GetShopItemList @0808bc66)
//   CCSShopItemList request: int kind@+0xc, char page@+0x10, short order@+0x11.
//   CSCShopItemList: cmd 0x0C1C, body 0x3b (59). 6 rows of 9 bytes.
// =============================================================================
class CCSShopItemList : public CHeadPacket
{ public:
    int   m_nKind;           // +0x0c  (1 all, 2 cash, else type)
    char  m_nPage;           // +0x10
    short m_nOrder;          // +0x11
};
class CShopItemData   // stride 9
{ public:
    int   m_nCode;           // +0x00
    int   m_nPrice;          // +0x04
    char  m_nReserved;       // +0x08  -> 9 bytes
};
class CSCShopItemList : public CHeadPacket
{ public:
    char          m_nResponse;     // +0x0c
    short         m_nPage;         // +0x0d
    short         m_nMaxPage;      // +0x0f
    CShopItemData m_cItemData[6];  // +0x11  (6 * 9 = 54) -> body 0x3b
    CSCShopItemList() : CHeadPacket(CM_SHOP_ITEM_LIST) { m_nBodySize = 0x3b; }
};

// =============================================================================
// UPDATE ITEM  (PacketUpdateItem @0807b638 / @0807b686)
//   CSCUpdateItem: cmd 0x0C1D, body 0x17f (383). A COption sits at +0x33 (overlaps
//   the start of the equip-wear region). Layout: response, kind(1 add/2 remove),
//   the 0x25 item row, then the 0x158 equip-wear block.
// =============================================================================
class CSCUpdateItem : public CHeadPacket
{ public:
    char       m_nResponse;   // +0x0c
    char       m_nKind;       // +0x0d  1=add, 2=remove
    CItemRow   m_cItemRow;    // +0x0e  (0x25 = 37 bytes)  -> ends at +0x33
    CEquipWear m_cEquipWear;  // +0x33  (0x158)            -> body 0x17f
    CSCUpdateItem() : CHeadPacket(CM_UPDATE_ITEM) { m_nBodySize = 0x17f; }
};

// =============================================================================
// UPDATE OPTION  (PacketUpdateOption @0807fd52)
//   CSCUpdateOption: cmd 0x0C24, body 0x19d (413). response + equip-wear + money;
//   COption at +0x51.
// =============================================================================
class CSCUpdateOption : public CHeadPacket
{ public:
    char        m_nResponse;  // +0x0c
    CMoneyBlock m_cMoney;     // +0x0d  (0x44 money/skin block)
    CEquipWear  m_cEquipWear; // +0x51  (0x158, COption@+0x51) -> body 0x19d
    CSCUpdateOption() : CHeadPacket(CM_UPDATE_OPTION) { m_nBodySize = 0x19d; }
};

// =============================================================================
// EQUIP / DIVEST ITEM  (PacketEquipItem @0807b720 / PacketDivestItem @0807b8f2)
//   CCSEquipItem / CCSDivestItem: int itemSeq@+0xc.
//   CSCEquipItem / CSCDivestItem: response, int seq, char on(1)/off(0),
//   then equip-wear + money trailer (body sized at runtime by the sender).
// =============================================================================
class CCSEquipItem : public CHeadPacket
{ public: int m_nItemSeq; };          // +0x0c
class CCSDivestItem : public CHeadPacket
{ public: int m_nItemSeq; };          // +0x0c

class CSCEquipItem : public CHeadPacket
{ public:
    char        m_nResponse;  // +0x0c
    int         m_nItemSeq;   // +0x0d
    char        m_nEquip;     // +0x11  (1 worn / 0 off)
    CMoneyBlock m_cMoney;     // +0x12  (0x44 money/skin block)
    CEquipWear  m_cEquipWear; // +0x56  (0x158 equip-wear; COption@+0x56) -> body 0x1a2
    CSCEquipItem() : CHeadPacket(CM_EQUIP_ITEM) { m_nBodySize = 0x1a2; }
};
class CSCDivestItem : public CHeadPacket
{ public:
    char        m_nResponse;  // +0x0c
    int         m_nItemSeq;   // +0x0d
    char        m_nEquip;     // +0x11  (always 0)
    CMoneyBlock m_cMoney;     // +0x12
    CEquipWear  m_cEquipWear; // +0x56  -> body 0x1a2
    CSCDivestItem() : CHeadPacket(CM_DIVEST_ITEM) { m_nBodySize = 0x1a2; }
};

// =============================================================================
// BUY / SELL ITEM  (PacketBuyItem @0807bac4 / PacketSellItem @0807bfc6)
//   CCSBuyItem layout is consumed by CPlayer::BuyItem (room/player module).
//   CSCBuyItem: cmd 0x0C20, body 0x59 (89). response, 4 money ints, short, char,
//     then the 0x44 money/skin block, then a trailing result byte.
//   CSCSellItem: response, int seq, int reason, 4 money ints.
// =============================================================================
class CCSBuyItem : public CHeadPacket
{ public:
    int   m_nCode;           // +0x0c  (item code)
    char  m_nBuyKind;        // +0x10  (1=cash, 2=point)
    int   m_nPrice;          // +0x11  (client-asserted price; verified int @ CPlayer::BuyItem 0x808c45e)
    int   m_nOptionCode[5];  // +0x15  (5 option ints) -> body 0x1d (29)
};
static_assert(offsetof(CCSBuyItem, m_nCode)       == 0x0c, "CCSBuyItem.m_nCode");
static_assert(offsetof(CCSBuyItem, m_nBuyKind)    == 0x10, "CCSBuyItem.m_nBuyKind");
static_assert(offsetof(CCSBuyItem, m_nPrice)      == 0x11, "CCSBuyItem.m_nPrice");
static_assert(offsetof(CCSBuyItem, m_nOptionCode) == 0x15, "CCSBuyItem.m_nOptionCode");
static_assert(sizeof(CCSBuyItem) == 0x29, "CCSBuyItem size (body 0x1d)");
class CSCBuyItem : public CHeadPacket
{ public:
    char        m_nResponse;  // +0x0c
    int         m_nMoney[4];  // +0x0d  (cash/point/exp/medal)
    short       m_nAmount;    // +0x1d
    char        m_nReserved;  // +0x1f
    CMoneyBlock m_cMoney;     // +0x20  (0x44)
    char        m_nResult;    // +0x64  -> body 0x59
    CSCBuyItem() : CHeadPacket(CM_BUY_ITEM) { m_nBodySize = 0x59; }
};

class CCSSellItem : public CHeadPacket
{ public: int m_nItemSeq; };          // +0x0c
class CSCSellItem : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    int   m_nItemSeq;        // +0x0d
    int   m_nReason;         // +0x11  (SellItem return value)
    int   m_nMoney[4];       // +0x15
    CSCSellItem() : CHeadPacket(CM_SELL_ITEM) { m_nBodySize = 0x19; }
};

// =============================================================================
// REPAIR ITEM  (PacketRepairItem @0807c0e4)  -- same shape as sell.
// =============================================================================
class CCSRepairItem : public CHeadPacket
{ public: int m_nItemSeq; };          // +0x0c
class CSCRepairItem : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    int   m_nItemSeq;        // +0x0d
    int   m_nReason;         // +0x11
    int   m_nMoney[4];       // +0x15
    CSCRepairItem() : CHeadPacket(CM_REPAIR_ITEM) { m_nBodySize = 0x19; }
};

// =============================================================================
// EXCHANGE ITEM  (PacketExchangeItem @0807bc44)
//   CCSExchangeItem consumed by CPlayer::ExchangeItem.
//   CSCExchangeItem: response, 4 money ints, short, char, then money block.
// =============================================================================
class CCSExchangeItem : public CHeadPacket
{ public:
    int   m_nCode;           // +0x0c  (item code; verified used as code @ CPlayer::ExchangeItem 0x808d10c)
    char  m_nBuyKind;        // +0x10  (1=cash, 2=point)
    int   m_nPrice;          // +0x11  (client-asserted price) -> body 0x09 (9)
};
static_assert(offsetof(CCSExchangeItem, m_nCode)    == 0x0c, "CCSExchangeItem.m_nCode");
static_assert(offsetof(CCSExchangeItem, m_nBuyKind) == 0x10, "CCSExchangeItem.m_nBuyKind");
static_assert(offsetof(CCSExchangeItem, m_nPrice)   == 0x11, "CCSExchangeItem.m_nPrice");
static_assert(sizeof(CCSExchangeItem) == 0x15, "CCSExchangeItem size (body 0x09)");
class CSCExchangeItem : public CHeadPacket
{ public:
    char        m_nResponse;  // +0x0c
    int         m_nMoney[4];  // +0x0d
    short       m_nAmount;    // +0x1d
    char        m_nReserved;  // +0x1f
    CMoneyBlock m_cMoney;     // +0x20  (0x44)
    CSCExchangeItem() : CHeadPacket(CM_EXCHANGE_ITEM) { m_nBodySize = 0x58; }
};

// =============================================================================
// GIFT ITEM  (PacketGiftItem @080810d6)
//   CCSGiftItem consumed by CPlayer::GiftItem; the handler reads sToName[15]@+0xc and
//   an int@+0x99 (target seq) from it.
//   CSCGiftItem: response, int date(+0x99 echo), char[15] name, 4 money ints.
// =============================================================================
class CCSGiftItem : public CHeadPacket
{ public:
    char     m_sToName[PLAYER_NAME_SIZE]; // +0x0c (15) target player name (binary: m_sTargetName)
    char     m_nBuyKind;                  // +0x1b  (1=cash, 2=point)
    char     m_sGiftMsg[0x79];            // +0x1c  (121) gift message text
    CItemRow m_cItemInfo;                 // +0x95  (0x25=37) gifted-item descriptor.
    // NOTE: m_cItemInfo.m_nCode sits at +0x99 (the prior "target seq @+0x99" guess was wrong;
    // CPlayer::GiftItem @0x808cbd6 reads it as the item code, echoed back in CSCGiftItem.m_nDate).
    // body 0xae (174)
};
static_assert(offsetof(CCSGiftItem, m_sToName)  == 0x0c, "CCSGiftItem.m_sToName");
static_assert(offsetof(CCSGiftItem, m_nBuyKind) == 0x1b, "CCSGiftItem.m_nBuyKind");
static_assert(offsetof(CCSGiftItem, m_sGiftMsg) == 0x1c, "CCSGiftItem.m_sGiftMsg");
static_assert(offsetof(CCSGiftItem, m_cItemInfo) == 0x95, "CCSGiftItem.m_cItemInfo");
static_assert(sizeof(CCSGiftItem) == 0xba, "CCSGiftItem size (body 0xae)");
class CSCGiftItem : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    int   m_nDate;           // +0x0d  (echo of CCS+0x99)
    char  m_sToName[PLAYER_NAME_SIZE]; // +0x11 (15)
    int   m_nMoney[4];       // +0x20
    CSCGiftItem() : CHeadPacket(CM_GIFT_ITEM) { m_nBodySize = 0x24; }
};

// =============================================================================
// GIFT LIST  (PacketGiftList @0807be5c ; CSql::GetGiftList @080b1a6c)
//   CCSGiftList request: char type@+0xc, char page@+0xd.
//   CSCGiftList: cmd 0x0C2A, body 0x5ff (1535). 10 gift rows of 0x99 (153) bytes.
// =============================================================================
class CCSGiftList : public CHeadPacket
{ public:
    char  m_nType;           // +0x0c  (verified @ PacketGiftList 0x807be5c -> CSql::GetGiftList)
    char  m_nPage;           // +0x0d  -> body 0x02 (2)
};
static_assert(offsetof(CCSGiftList, m_nType) == 0x0c, "CCSGiftList.m_nType");
static_assert(offsetof(CCSGiftList, m_nPage) == 0x0d, "CCSGiftList.m_nPage");
static_assert(sizeof(CCSGiftList) == 0x0e, "CCSGiftList size (body 0x02)");

class CGiftData       // stride 0x99 (153)
{ public:
    char  m_nKind;           // +0x00  GetGiftList +0x11
    int   m_nItemSeq;        // +0x01  +0x12
    int   m_nCode;           // +0x05  +0x16
    int   m_nAmount;         // +0x09  +0x1a
    char  m_sFromName[15];   // +0x0d  +0x1e  (gifter name)
    char  m_sMessage[0x79];  // +0x1c  +0x2d  (121)
    int   m_nDate;           // +0x95  +0xa6  -> 153 bytes
};
class CSCGiftList : public CHeadPacket
{ public:
    char      m_nResponse;   // +0x0c
    short     m_nPage;       // +0x0d  (0xffff if empty)
    short     m_nMaxPage;    // +0x0f
    CGiftData m_cGiftData[10]; // +0x11  (10 * 153 = 1530) -> body 0x5ff
    CSCGiftList() : CHeadPacket(CM_GIFT_LIST) { m_nBodySize = 0x5ff; }
};

// =============================================================================
// GET GIFT  (PacketGetGift @0807bed8)
//   CCSGetGift: int giftSeq@+0xc.  CSCGetGift: cmd 0x0C2B, response only.
// =============================================================================
class CCSGetGift : public CHeadPacket
{ public: int m_nGiftSeq; };           // +0x0c
class CSCGetGift : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    CSCGetGift() : CHeadPacket(CM_GET_GIFT) { m_nBodySize = 1; }
};

// =============================================================================
// POST ITEM  (PacketPostItem @0807ee98)
//   CCSPostItem: int itemSeq@+0xc, int targetSeq@+0x10.
//   CSCPostItem: cmd 0x0C23, body 0x1a1 (417). response, int seq, then equip-wear +
//   money; COption at +0x55.
// =============================================================================
class CCSPostItem : public CHeadPacket
{ public:
    int   m_nItemSeq;        // +0x0c
    int   m_nTargetSeq;      // +0x10
};
class CSCPostItem : public CHeadPacket
{ public:
    char        m_nResponse;  // +0x0c
    int         m_nItemSeq;   // +0x0d  (uStack_1af)
    CMoneyBlock m_cMoney;     // +0x11  (0x44 money/skin block)
    CEquipWear  m_cEquipWear; // +0x55  (0x158, COption@+0x55) -> body 0x1a1
    CSCPostItem() : CHeadPacket(CM_POST_ITEM) { m_nBodySize = 0x1a1; }
};

// =============================================================================
// ENCHANT ITEM  (PacketEnchantItem @08081506 ; CRandomShop::EnchantItem)
//   CCSEnchantItem consumed by CRandomShop::EnchantItem.
//   CSCEnchantItem: cmd 0x0C28, response + 4 money ints.
// =============================================================================
class CCSEnchantItem : public CHeadPacket
{ public:
    int  m_nItemSeq;   // +0x0c
    int  m_nPrice;     // +0x10
    char m_nType;      // +0x14
    char m_nPay;       // +0x15
};
class CSCEnchantItem : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    int   m_nMoney[4];       // +0x0d
    CSCEnchantItem() : CHeadPacket(CM_ENCHANT_ITEM) { m_nBodySize = 0x11; }
};

// =============================================================================
// MIX ITEM  (PacketMixItem1 @08080b2a / MixItem2 @08080b30 -- both no-op stubs in
//   this build; structs kept for the dispatcher. CSql::InsertMixItem @080b6eaa.)
//   CCSMixItem1: response/kind@+0xc, 12 ingredient item seqs@+0xd.
// =============================================================================
class CCSMixItem1 : public CHeadPacket   // XKICK_Game1 CCSMixItem1: 41 bytes
{ public:
    CCSMixItem1() : CHeadPacket(CM_MIX_ITEM1) { m_nBodySize = 0x1d; }   // 41 - 12
    int   m_nMixSeq;         // +0x0c  mix-recipe code
    int   m_nItemSeq[5];     // +0x10  ingredient item_seq[5]
    char  m_nItemCnt[5];     // +0x24  consume count[5]
};
class CCSMixItem2 : public CHeadPacket
{ public:
    char  m_nKind;           // +0x0c
    int   m_nItemSeq[12];    // +0x0d
};
class CSCMixItem1 : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    CSCMixItem1() : CHeadPacket(CM_MIX_ITEM1) { m_nBodySize = 1; }
};
class CSCMixItem2 : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    CSCMixItem2() : CHeadPacket(CM_MIX_ITEM2) { m_nBodySize = 1; }
};

// =============================================================================
// REWARD ITEM / MONEY  (PacketRewardItem @0807bdba)
//   CSCRewardItem: cmd 0x0C25, body 0x26 (38). response + raw 37 bytes of the
//   runtime CItem object (memcpy of the first 0x25 bytes).
// =============================================================================
class CSCRewardItem : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    char  m_cItem[0x25];     // +0x0d  (raw CItem[0..37)) -> body 0x26
    CSCRewardItem() : CHeadPacket(CM_REWARD_ITEM) { m_nBodySize = 0x26; }
};

// =============================================================================
// COUPONS  (PacketCashCoupon @0807ce82 / PacketPointCoupon @0807cf6c)
//   CCSCashCoupon / CCSPointCoupon: int itemSeq@+0xc.
//   CSCCashCoupon / CSCPointCoupon: cmd 0x1221 / 0x1222, body 0x15 (21).
//     response, int seq, 4 money ints. Grants +5000 cash / +5000 point.
// =============================================================================
class CCSCashCoupon : public CHeadPacket
{ public: int m_nItemSeq; };           // +0x0c
class CCSPointCoupon : public CHeadPacket
{ public: int m_nItemSeq; };           // +0x0c

class CSCCashCoupon : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    int   m_nItemSeq;        // +0x0d
    int   m_nMoney[4];       // +0x11
    CSCCashCoupon() : CHeadPacket(CM_CASH_COUPON) { m_nBodySize = 0x15; }
};
class CSCPointCoupon : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    int   m_nItemSeq;        // +0x0d
    int   m_nMoney[4];       // +0x11
    CSCPointCoupon() : CHeadPacket(CM_POINT_COUPON) { m_nBodySize = 0x15; }
};

// =============================================================================
// RANDOM SHOP  (rndshop owner -- structs needed by item.cpp/card.cpp references)
//   CCSBuyRandomitem / CSCBuyRandomitem (PacketBuyRandomitem @080812a0):
//     request char index@+0xc; reply response, char index, 4 money ints, 0x44 block.
//   CSCRandomshopitemList (PacketRandomshopitemList @0808121e): built by
//     CRandomShop::GetItemList; opaque body owned by rndshop.
//   CSCRefreshShop (PacketRefreshShop @0808162e): response + 4 money ints.
// =============================================================================
class CCSBuyRandomitem : public CHeadPacket
{ public:
    char m_nIndex;     // +0x0c
    char m_pad0d;      // +0x0d
    int  m_nCode;      // +0x0e
    int  m_nPrice;     // +0x12
};
class CSCBuyRandomitem : public CHeadPacket
{ public:
    char        m_nResponse;  // +0x0c
    char        m_nIndex;     // +0x0d
    int         m_nMoney[4];  // +0x0e  (CMoney, 16)
    CMoneyBlock m_cMoney;     // +0x1e  (m_nEquipWear int[17], 0x44=68)
    char        m_nEquipKind; // +0x62  (rebuild: trailing equip-kind byte)
    CSCBuyRandomitem() : CHeadPacket(CM_BUY_RANDOMITEM) { m_nBodySize = 0x57; }
};
// RANDOM SHOP ITEM LIST (PacketRandomshopitemList @0808121e; CRandomShop::GetItemList
//   @0804afee). cmd 0x0C27, body 0x4a7. response, count(short), 4 reserved bytes,
//   then up to 32 CItemRow (0x25) offers. Body sized fixed by the ctor.
#ifndef CM_RANDOMSHOPITEM_LIST
#define CM_RANDOMSHOPITEM_LIST 0x0C27
#endif
class CSCRandomshopitemList : public CHeadPacket
{ public:
    char     m_nResponse;     // +0x0c
    short    m_nCount;        // +0x0d
    char     m_nReserved[4];  // +0x0f
    CItemRow m_cItem[32];     // +0x13  (32 * 0x25 = 0x4a0) -> body 0x4a7
    CSCRandomshopitemList() : CHeadPacket(CM_RANDOMSHOPITEM_LIST) { m_nBodySize = 0x4a7; }
};

class CCSRefreshShop : public CHeadPacket
{ public: char m_nReserved; };         // +0x0c
class CSCRefreshShop : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    int   m_nMoney[4];       // +0x0d
    CSCRefreshShop() : CHeadPacket(CM_REFRESH_SHOP) { m_nBodySize = 0x11; }
};

// =============================================================================
//                                  CARD PACKETS
// =============================================================================
// The on-the-wire card row is the full 23-byte runtime CCard (Domain.h), memcpy'd.

// -----------------------------------------------------------------------------
// CARD INFO  (PacketCardInfo @08080512)
//   CSCCardInfo: cmd 0x083A. response, count, up to 30 cards of 0x17 (23) bytes.
//   Body sized at runtime (count * 0x17).
// -----------------------------------------------------------------------------
class CCardRow { public: char m_data[0x17]; };  // raw CCard (23 bytes)
class CSCCardInfo : public CHeadPacket
{ public:
    char     m_nResponse;    // +0x0c
    char     m_nCount;       // +0x0d
    CCardRow m_cCardData[30];// +0x0e  (30 * 23 = 690)
    CSCCardInfo() : CHeadPacket(CM_CARD_INFO) { m_nBodySize = 0x2b4; }
};

// -----------------------------------------------------------------------------
// UPDATE CARD  (PacketUpdateCard @080806f4) -- response only (filled by caller).
// -----------------------------------------------------------------------------
class CSCUpdateCard : public CHeadPacket
{ public:
    char     m_nResponse;    // +0x0c
    CCardRow m_cCardData;    // +0x0d  (card payload set by caller)
    char     m_nKind;        // +0x24
    CSCUpdateCard() : CHeadPacket(CM_UPDATE_CARD) { m_nBodySize = 0x19; }
};

// -----------------------------------------------------------------------------
// EQUIP / DIVEST CARD  (PacketEquipCard @0808071c / PacketDivestCard @0808086a)
//   CCSEquipCard: int cardSeq@+0xc, char slot@+0x10 (<3), char value@+0x11.
//   CSCEquipCard: response, int seq, char slot, char value.
//   CCSDivestCard: int cardSeq@+0xc.  CSCDivestCard: response, int seq, char value.
// -----------------------------------------------------------------------------
class CCSEquipCard : public CHeadPacket
{ public:
    int   m_nCardSeq;        // +0x0c
    char  m_nSlot;           // +0x10  (0..2)
    char  m_nValue;          // +0x11
};
class CSCEquipCard : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    int   m_nCardSeq;        // +0x0d
    char  m_nSlot;           // +0x11
    char  m_nValue;          // +0x12
    CSCEquipCard() : CHeadPacket(CM_EQUIP_CARD) { m_nBodySize = 7; }
};
class CCSDivestCard : public CHeadPacket
{ public: int m_nCardSeq; };           // +0x0c
class CSCDivestCard : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    int   m_nCardSeq;        // +0x0d
    char  m_nEquipEntry;     // +0x11  (rebuild: binary CSCDivestCard)
    char  m_nEquipKind;      // +0x12  (always 0)
    CSCDivestCard() : CHeadPacket(CM_DIVEST_CARD) { m_nBodySize = 7; }
};

// -----------------------------------------------------------------------------
// CREDIT / REWARD CARD  (PacketCreditCard @0808097a / PacketRewardCard @08080a88)
//   Both take a runtime CCard*, memcpy 0x17 bytes onto the wire. response leads.
//   CSCCreditCard: cmd 0x0E14.  CSCRewardCard: cmd 0x0E19.
// -----------------------------------------------------------------------------
class CSCCreditCard : public CHeadPacket
{ public:
    char     m_nResponse;    // +0x0c
    CCardRow m_cCardData;    // +0x0d  (0x17 raw CCard)
    CSCCreditCard() : CHeadPacket(CM_CREDIT_CARD) { m_nBodySize = 0x18; }
};
class CSCRewardCard : public CHeadPacket
{ public:
    char     m_nResponse;    // +0x0c
    CCardRow m_cCardData;    // +0x0d
    CSCRewardCard() : CHeadPacket(CM_REWARD_CARD) { m_nBodySize = 0x18; }
};

// -----------------------------------------------------------------------------
// CARD ENTRY  (PacketCardEntry @08080a1c)
//   CCSCardEntry: char entry@+0xc (<3 sets CPlayer+0x514).
//   CSCCardEntry: response, char entry.
// -----------------------------------------------------------------------------
class CCSCardEntry : public CHeadPacket
{ public: char m_nEntry; };            // +0x0c
class CSCCardEntry : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    char  m_nEntry;          // +0x0d
    CSCCardEntry() : CHeadPacket(CM_CARD_ENTRY) { m_nBodySize = 2; }
};

// -----------------------------------------------------------------------------
// BUY CARDBOOSTER  (PacketBuyCardbooster @0808173c)
//   CCSBuyCardbooster: int code@+0xc, char payKind@+0x10 (1 cash / else point),
//     int price@+0x11.
//   CSCBuyCardbooster: response (filled at runtime) + the 0x17 new card row.
// -----------------------------------------------------------------------------
class CCSBuyCardbooster : public CHeadPacket
{ public:
    int   m_nCode;           // +0x0c  (AI card table id)
    char  m_nPayKind;        // +0x10  (1=cash, else point)
    int   m_nPrice;          // +0x11  (client-asserted price)
};
class CSCBuyCardbooster : public CHeadPacket
{ public:
    char     m_nResponse;    // +0x0c
    CCardRow m_cCardData;    // +0x0d  (new card, 0x17)
    CMoney   m_cMoney;       // +0x24  (rebuild: binary CSCBuyCardbooster trailing CMoney)
    CSCBuyCardbooster() : CHeadPacket(CM_BUY_CARDBOOSTER) { m_nBodySize = 0x28; }
};

// -----------------------------------------------------------------------------
// MIX CARD  (PacketMixCard1 @08080b36 ; CPlayer::MixCard @08090360)
//   CCSMixCard1: char kind@+0xc, 12 ingredient card seqs@+0xd.
//   CSCMixCard1: cmd 0x0E7F, response + 43-byte CCardInfo result.
//   PacketMixCard2 @08080bfa -- no-op stub (response only).
// -----------------------------------------------------------------------------
class CCSMixCard1 : public CHeadPacket
{ public:
    char  m_nKind;           // +0x0c
    int   m_nCardSeq[12];    // +0x0d
};
class CSCMixCard1 : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    char  m_cCardInfo[23];   // +0x0d  (rebuild: 23-byte CCardInfo result; filled by MixCard)
    CSCMixCard1() : CHeadPacket(CM_MIX_CARD1) { m_nBodySize = 0x18; }
};
class CSCMixCard2 : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    int   m_nItemSeq;        // +0x0d  (rebuild: binary CSCMixCard2)
    short m_nItemCnt;        // +0x11
    CSCMixCard2() : CHeadPacket(CM_MIX_CARD2) { m_nBodySize = 7; }
};

#pragma pack(pop)

#endif // _GAME_PROTOCOL_ITEM_H_
