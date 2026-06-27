// =============================================================================
// XKICK_Game - GameItem.h
// The Game server's runtime inventory item: an 80-byte (0x50) object. CItem is a
// server-specific type (not in shared Struct.h); Certify has its own 36-byte CItem
// in certify/include/Item.h. Stored in CPlayer::m_vItemList.
//
// Layout FINALIZED from Ghidra:
//   * CSql::GetItemField @080ae07c -- `operator new(0x50)` (80 bytes); maps every
//     DB column from the SELECT @080c0df4:
//       item_seq, item_code, item_equip, item_amount, item_grade, item_limit,
//       item_price, item_option0..4, item_random, item_order  (tb_game_item).
//   * PacketItemInfo @0807551e -- serializes the 0x25 (37-byte) wire row.
//   * The item handlers (buy/equip/mix/post/enchant/repair) touch the trailing
//     runtime working fields (0x31..0x50): item class/level cache, random flag,
//     order slot, delete-date, and a temp option-block scratch.
//
// Loader column -> offset map (puVar is undefined4*, so puVar[n] is byte n*4):
//   *puVar5            0x00 int   item_seq
//   puVar5[1]          0x04 int   item_code
//   *(char*)(p+2)      0x08 char  item_equip
//   *(char*)(p+9)      0x09 char  item_limit
//   *(char*)(p+10)     0x0a char  item_grade
//   *(short*)(p+3)     0x0c short item_amount
//   puVar5[4]          0x10 int   item_price
//   puVar5[5]          0x14 int   item_option0
//   puVar5[6]          0x18 int   item_option1
//   puVar5[7]          0x1c int   item_option2
//   puVar5[8]          0x20 int   item_option3
//   puVar5[9]          0x24 int   item_option4
//   *(char*)(p+10byte) 0x28 char  m_nState (loader sets 1 explicitly)
//   puVar5[0xb]        0x2c int   item_random
//   *(char*)(p+0xc)    0x30 char  item_order
// The remaining bytes (0x31..0x50) are zero-initialised runtime scratch used by the
// shop/equip/mix handlers; they are never read out of the DB row.
//
// The on-the-wire row is a Game-specific 37-byte record (NOT the common Protocol.h
// CItemInfo, which carries slot/slotcode): see CItemRow in GameProtocolItem.h.
// =============================================================================
#ifndef _GAME_ITEM_H_
#define _GAME_ITEM_H_

#include <cstdint>
#include <cstddef>      // offsetof

// Random-option table row (CItem::m_pTable back-pointer; returned by
// GetRandomOptionTable). 108 bytes, verified vs XKICK_Game1 COptionTable. The
// per-equip-type ratio arrays (indices 1..13 used) drive the 5-grade weighted
// random selection; m_nUsual/m_nSpecial hold the rolled option value bands.
class COptionTable
{
public:
    int  m_nCode;            // 0x00
    int  m_nPrice;           // 0x04
    char m_nUsual[6];        // 0x08  (kind 0 value band)
    char m_nSpecial[6];      // 0x0e  (kind 1 value band)
    char m_nNormalRatio[17]; // 0x14
    char m_nRareRatio[17];   // 0x25
    char m_nUniqueRatio[17]; // 0x36
    char m_nEpicRatio[17];   // 0x47
    char m_nLegendRatio[17]; // 0x58  -> 0x69 (+3 pad) = 108
};

#pragma pack(push, 1)
// CItem : the Game server's 80-byte (0x50) runtime inventory item.
// Layout is byte-exact with the original binary's CItem; every field is a named
// member at its exact offset (no raw "reserved" regions).
class CItem
{
public:
    int   m_nItemSeq;        // 0x00  item_seq
    int   m_nCode;           // 0x04  item_code
    char  m_nEquipKind;      // 0x08  item_equip
    char  m_nLimit;          // 0x09  item_limit
    char  m_nGrade;          // 0x0a  item_grade
    char  m_pad0b;           // 0x0b
    short m_nAmount;         // 0x0c  item_amount (binary u16; kept signed for behavior parity)
    short m_pad0e;           // 0x0e
    int   m_nPrice;          // 0x10  item_price
    int   m_nOptionCode[5];  // 0x14  item_option0..4  (0x14..0x27)
    char  m_nState;          // 0x28  OBJECT_STATE_* (loader sets 1)
    char  m_pad29[3];        // 0x29
    int   m_nRandom;         // 0x2c  item_random
    char  m_nOrder;          // 0x30  item_order (inventory slot index)
    char  m_pad31[3];        // 0x31
    // ---- runtime working fields (0x34..0x50) ----
    int   m_nGiftSeq;        // 0x34  gift sender seq
    int   m_nGiftDate;       // 0x38  gift date / expiry epoch
    char  m_sGiftName[15];   // 0x3c  gift sender name (0x3c..0x4a)
    char  m_bConsume;        // 0x4b  consumable flag
    COptionTable* m_pTable;  // 0x4c  item-class / option table row back-pointer

    CItem() { for (int i = 0; i < (int)sizeof(*this); ++i) ((char*)this)[i] = 0; }

    bool IsEmptyState() const { return m_nState == 0; }   // CItem::IsEmptyState (binary)
};
#pragma pack(pop)

// ---- byte-exact layout proof (vs original binary CItem, size 0x50) ----
static_assert(sizeof(CItem) == 0x50, "CItem size");
static_assert(offsetof(CItem, m_nItemSeq)   == 0x00, "CItem.m_nItemSeq");
static_assert(offsetof(CItem, m_nCode)       == 0x04, "CItem.m_nCode");
static_assert(offsetof(CItem, m_nEquipKind)  == 0x08, "CItem.m_nEquipKind");
static_assert(offsetof(CItem, m_nLimit)      == 0x09, "CItem.m_nLimit");
static_assert(offsetof(CItem, m_nGrade)      == 0x0a, "CItem.m_nGrade");
static_assert(offsetof(CItem, m_nAmount)     == 0x0c, "CItem.m_nAmount");
static_assert(offsetof(CItem, m_nPrice)      == 0x10, "CItem.m_nPrice");
static_assert(offsetof(CItem, m_nOptionCode) == 0x14, "CItem.m_nOptionCode");
static_assert(offsetof(CItem, m_nState)      == 0x28, "CItem.m_nState");
static_assert(offsetof(CItem, m_nRandom)     == 0x2c, "CItem.m_nRandom");
static_assert(offsetof(CItem, m_nOrder)      == 0x30, "CItem.m_nOrder");
static_assert(offsetof(CItem, m_nGiftSeq)    == 0x34, "CItem.m_nGiftSeq");
static_assert(offsetof(CItem, m_nGiftDate)   == 0x38, "CItem.m_nGiftDate");
static_assert(offsetof(CItem, m_sGiftName)   == 0x3c, "CItem.m_sGiftName");
static_assert(offsetof(CItem, m_bConsume)    == 0x4b, "CItem.m_bConsume");
static_assert(offsetof(CItem, m_pTable)      == 0x4c, "CItem.m_pTable");

#endif // _GAME_ITEM_H_
