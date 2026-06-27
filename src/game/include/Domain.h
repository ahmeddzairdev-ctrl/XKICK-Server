// =============================================================================
// XKICK_Game - Domain.h
// Runtime, heap-owned domain objects that live in CPlayer's std::vector members
// (Player.h: m_vSkillList / m_vTrainingList / m_vCeremonyList / m_vCardList /
//  m_vQuestList / m_vBlacklistList). These are the IN-MEMORY objects the Game
// server allocates while a character is loaded -- DISTINCT from the packed *Info
// wire ROW structs in common "Struct.h" (CSkillInfo/CTrainingInfo/CCeremonyInfo).
//
// Each layout was recovered from the XKICK_Game1 ELF: the heap size each CSql
// loader allocates (operator new arg), the column->offset writes inside the
// loader, and the field reads inside the matching Packet*Info handler that copies
// the object onto the wire.
//
//   Class           ctor            CLoad/CSql loader            wire handler
//   --------------  --------------  ---------------------------  --------------------------
//   CSkill          C1 @08091a66    CSql::GetSkillField @080b0122    PacketSkillInfo @0807584a
//   CTraining       C1 @08091a92    CSql::GetTrainingField @080b058c PacketTrainingInfo @08075a96
//   CCeremony       C1 @08091abe    CSql::GetCeremonyField @080b09f6 PacketCeremonyInfo @08075cb2
//   CCard           C1 @08084b3a    CSql::GetCardField @080af728     PacketCardInfo @08080512
//   CQuestInfo      (POD, no ctor)  CSql::GetQuestField @080b0db4    PacketQuestInfo @08075ece
//   CBlacklistInfo  (POD, no ctor)  CPlayer::AddBlackListInfo @08087d96 (+ GetBlacklistInfo @08088488)
//                                                                    PacketBlacklistInfo @08080e02
//
// DB column names come from the SELECT field strings used by each loader:
//   skill_*    @080c1312    training_* @080c13ee    ceremony_* @080c14c7
//   card_*     @080c11f1    quest_*    @080c155a
//
// All six runtime ctors are trivial (the loaders memset/zero the body and set the
// +0 state byte = 1 by hand), so these are reconstructed as plain data classes.
//
// NOTE on CItem: the Game server's runtime CItem is 0x50 (80) bytes -- NOT the
// 36-byte shared CItem in common "Struct.h" -- and its field offsets differ. See
// the CItem note block at the bottom of this file. We do NOT redefine CItem here
// and do NOT edit Struct.h.
// =============================================================================
#ifndef _GAME_DOMAIN_H_
#define _GAME_DOMAIN_H_

#include <cstdint>

#include "Struct.h"   // shared wire ROW structs + any embedded subobjects

// ---- forward declarations (pointers only) ----
class CPlayer;

#pragma pack(push,1)

// -----------------------------------------------------------------------------
// CSkill  -- a learned skill.  size 0x14 (20) bytes (operator new(0x14)).
//   DB: tb_game skill_seq / skill_code / skill_equip / skill_level
//   Wire row (PacketSkillInfo): 10 bytes = seq+code+equip+level, from +4..+0xd;
//   validity gated on m_nState (+0). Matches packed CSkillInfo in Struct.h.
// -----------------------------------------------------------------------------
class CSkill
{
public:
    CSkill() : m_nState(0), m_nSkillSeq(0), m_nCode(0),
               m_nEquipKind(0), m_nLevel(0) {}

    char   m_nState;          // 0x00  loaded/valid flag (loader sets 1); DB-backed: no
    char   m_pad01[3];        // 0x01  pad
    int    m_nSkillSeq;       // 0x04  skill_seq   [WIRE]
    int    m_nCode;           // 0x08  skill_code  [WIRE]
    char   m_nEquipKind;      // 0x0c  skill_equip [WIRE]
    char   m_nLevel;          // 0x0d  skill_level [WIRE]
    char   m_pad0e[6];        // 0x0e  tail pad to 0x14

    // CSkill::ChangeSkillState @08096ddc : flag the row dirty for the next
    // CSql::UpdateSkillField flush (-1 if unloaded, else m_nState <- 2).
    int ChangeSkillState() { if (m_nState == 0) return -1; m_nState = 2; return 0; }
};

// -----------------------------------------------------------------------------
// CTraining  -- a trained attribute.  size 0x14 (20) bytes (operator new(0x14)).
//   DB: tb_game training_seq / training_code / training_type / training_level
//   Wire row (PacketTrainingInfo): 9 bytes = seq+code+level, from +4/+8/+0xc;
//   m_nType (+0xd) is DB-backed but NOT sent on the wire.
//   (Note: the shared CTrainingInfo wire row has only seq/code/level.)
// -----------------------------------------------------------------------------
class CTraining
{
public:
    CTraining() : m_nState(0), m_nTrainingSeq(0), m_nCode(0),
                  m_nLevel(0), m_nType(0) {}

    char   m_nState;          // 0x00  loaded/valid flag (loader sets 1)
    char   m_pad01[3];        // 0x01  pad
    int    m_nTrainingSeq;    // 0x04  training_seq   [WIRE]
    int    m_nCode;           // 0x08  training_code  [WIRE]
    char   m_nLevel;          // 0x0c  training_level [WIRE]
    char   m_nType;           // 0x0d  training_type  (DB-backed, not wired)
    char   m_pad0e[6];        // 0x0e  tail pad to 0x14

    // CTraining::ChangeTrainingState @080972f0
    int ChangeTrainingState() { if (m_nState == 0) return -1; m_nState = 2; return 0; }
};

// -----------------------------------------------------------------------------
// CCeremony  -- an equipped ceremony/celebration.  size 0x14 (20) bytes.
//   DB: tb_game ceremony_seq / ceremony_code / ceremony_equip
//   Wire row (PacketCeremonyInfo): 9 bytes = seq+code+equip, from +4/+8/+0xc.
//   Matches packed CCeremonyInfo in Struct.h.
// -----------------------------------------------------------------------------
class CCeremony
{
public:
    CCeremony() : m_nState(0), m_nCeremonySeq(0), m_nCode(0), m_nEquipKind(0) {}

    char   m_nState;          // 0x00  loaded/valid flag (loader sets 1)
    char   m_pad01[3];        // 0x01  pad
    int    m_nCeremonySeq;    // 0x04  ceremony_seq   [WIRE]
    int    m_nCode;           // 0x08  ceremony_code  [WIRE]
    char   m_nEquipKind;      // 0x0c  ceremony_equip [WIRE]
    char   m_pad0d[7];        // 0x0d  tail pad to 0x14

    // CCeremony::ChangeCeremonyState @08097070
    int ChangeCeremonyState() { if (m_nState == 0) return -1; m_nState = 2; return 0; }
};

// -----------------------------------------------------------------------------
// CCard  -- a player card.  size 0x17 (23) bytes (operator new(0x17)).
//   DB: card_seq/card_code/card_costume/card_equip1..3/card_level/card_position/
//       card_area/card_rank/card_skill/card_tired
//   Wire (PacketCardInfo): the FULL 23-byte object is memcpy'd onto the wire;
//   validity gated on m_nState (+0x08). So every field below is wire-serialized.
// -----------------------------------------------------------------------------
class CCard
{
public:
    CCard() : m_nCardSeq(0), m_nCode(0), m_nState(0),
              m_nEquip1(0), m_nEquip2(0), m_nEquip3(0),
              m_nLevel(0), m_nRank(0), m_nPosition(0), m_nArea(0),
              m_nSkill(0), m_nTired(0), m_nCostume(0) {}

    int    m_nCardSeq;        // 0x00  card_seq       [WIRE]
    int    m_nCode;           // 0x04  card_code      [WIRE]
    char   m_nState;          // 0x08  loaded/valid flag (loader sets 1) [WIRE]
    char   m_nEquip1;         // 0x09  card_equip1    [WIRE]
    char   m_nEquip2;         // 0x0a  card_equip2    [WIRE]
    char   m_nEquip3;         // 0x0b  card_equip3    [WIRE]
    char   m_nLevel;          // 0x0c  card_level     [WIRE]
    char   m_nRank;           // 0x0d  card_rank      [WIRE]
    char   m_nPosition;       // 0x0e  card_position  [WIRE]
    char   m_nArea;           // 0x0f  card_area      [WIRE]
    char   m_nSkill;          // 0x10  card_skill     [WIRE]
    char   m_nTired;          // 0x11  card_tired     [WIRE]
    int    m_nCostume;        // 0x12  card_costume   [WIRE]  (unaligned, packed)
    char   m_pad16;           // 0x16  trailing pad (operator new(0x17) = 23 bytes)
                              // 0x17  end (size 0x17)
};

// -----------------------------------------------------------------------------
// CQuestInfo  -- an in-progress / completed quest.  size 0x0e (14) bytes.
//   DB: tb_game/tb_clear quest_code / quest_count / quest_seq.
//   Wire (PacketQuestInfo): full 14-byte row = code+seq+count+state, from
//   +0/+4/+8(short)/+0xa(int). All four fields sent.
//   The runtime int @+0xa is zeroed by the loader (per-session exec state).
// -----------------------------------------------------------------------------
class CQuestInfo
{
public:
    CQuestInfo() : m_nCode(0), m_nSeq(0), m_nCount(0), m_nState(0) {}

    int    m_nCode;           // 0x00  quest_code   [WIRE]
    int    m_nSeq;            // 0x04  quest_seq    [WIRE]
    short  m_nCount;          // 0x08  quest_count  [WIRE]
    int    m_nState;          // 0x0a  runtime exec/state (loader zeroes) [WIRE]
                              // 0x0e  end (size 0x0e)
};

// -----------------------------------------------------------------------------
// CBlacklistInfo  -- one blocked/blacklisted player.  size 0x16 (22) bytes
//   (operator new(0x16)).  Built by CPlayer::AddBlackListInfo from the target
//   player's pointer; CSql::AddBlackListField persists it.
//   Fields: target seq (+0), and a snapshot of the target's level/position/name
//   captured at add-time (level <- target+0xc4, position <- target+0x10,
//   name <- target+0x20, 15 bytes). Byte @+4 is unused/state padding.
//   The wire copy is produced indirectly by GetBlacklistInfo() into a CSC packet
//   (not a raw memcpy of this object), so this struct itself is DB-backed, not a
//   direct wire row.
// -----------------------------------------------------------------------------
class CBlacklistInfo
{
public:
    CBlacklistInfo() : m_nPlayerSeq(0), m_nState(0),
                       m_nLevel(0), m_nPosition(0) { m_sName[0] = 0; }

    int    m_nPlayerSeq;      // 0x00  blocked player seq
    char   m_nState;          // 0x04  state/flag byte (unused at add-time)
    char   m_nLevel;          // 0x05  snapshot of target player level
    char   m_nPosition;       // 0x06  snapshot of target player position
    char   m_sName[15];       // 0x07  snapshot of target player name (PLAYER_NAME_SIZE)
                              // 0x16  end (size 0x16)
};

#pragma pack(pop)

// =============================================================================
// CItem MISMATCH FLAG (do NOT edit Struct.h)
// -----------------------------------------------------------------------------
// common Struct.h defines a 36-byte shared CItem:
//     m_nState@0(char) m_nItemSeq@4 m_nCode@8 m_nEquipKind@12 m_nAmount@14(short)
//     m_nOptionCode[5]@16 ... (= 36 bytes)
//
// The XKICK_Game runtime CItem is DIFFERENT. Verified against
//     CSql::GetItemField @080ae07c  (operator new(0x50) -> 80 bytes) and
//     PacketItemInfo     @0807551e  (uses CItem::IsEmptyState; 0x25=37-byte wire row).
// Game CItem layout (offsets from the loader/handler):
//     +0x00 int   item_seq          +0x04 int   code
//     +0x08 char  equip_kind        +0x09 char   (field A)
//     +0x0a char  (field B)         +0x0c short  amount
//     +0x10..+0x24  int  m_nOptionCode[5]   (5 option ints; wire copies 0x14 bytes)
//     +0x28 char  m_nState (loader sets 1)
//     ... trailing runtime fields out to size 0x50 (80 bytes total)
//   Wire row (PacketItemInfo, 0x25=37 bytes): item_seq, code, equip, fldA, fldB,
//   amount(short), one extra int (+0x10), then 20 bytes (5 option ints) -- i.e.
//   the Game serializes a 37-byte item record, not the shared 36-byte struct.
//
// => The shared 36-byte CItem (used by Certify for item inserts) does NOT match
//    the Game's 80-byte runtime CItem in either size or field offsets. The Game's
//    CItem belongs in its own header (Item.h) when that .cpp is reconstructed;
//    it is intentionally NOT defined here and Struct.h is left untouched.
// =============================================================================

#endif // _GAME_DOMAIN_H_
