// =============================================================================
// XKICK_Game - GameProtocolEquip.h
// #pragma pack(1) WIRE structs for the SKILL / CEREMONY / TRAINING shop modules
// (skill.cpp / ceremony.cpp / training.cpp) that are NOT already shared in common
// Protocol.h.
//
// Almost every skill/ceremony/training packet (CSCSkillInfo, CSCShopSkillList,
// CSCUpdateSkill, CSCEquipSkill, CSCDivestSkill, CSCBuySkill, CSCShopCeremonyList,
// CSCEquipCeremony, CSCDivestCeremony, CSCBuyCeremony, CSCUpdateCeremony,
// CSCShopTrainingList, CSCBuyTraining, CSCUpdateTraining, ...) ALREADY exists in
// common Protocol.h and is REUSED verbatim by those modules -- do not redefine
// them here.
//
// The ONE reply that has no Protocol.h analogue is the skill-slot expansion reply
// CSCSkillSlot (CM_SKILL_SLOTT 0x1217), recovered byte-exact from the binary:
//   CSCSkillSlot::C1 @0808434a  -> cmd 0x1217, body 0x11 (17).
//   PacketSkillSlot @0807cda6   -> response, int itemSeq, then the player's
//                                  level-word (CPlayer+0xc4), exp (CPlayer+0xc8),
//                                  faculty/skill-point word (CPlayer+0xcc).
// =============================================================================
#ifndef _GAME_PROTOCOL_EQUIP_H_
#define _GAME_PROTOCOL_EQUIP_H_

#include "Struct.h"      // CHeadPacket
#include "Command.h"     // CM_* ids

#ifndef CM_SKILL_SLOTT
#define CM_SKILL_SLOTT 0x1217
#endif

#pragma pack(push,1)

// -----------------------------------------------------------------------------
// SKILL SLOT  (PacketSkillSlot @0807cda6, CM_SKILL_SLOTT 0x1217)
//   Request CCSSkillSlot: int itemSeq@+0xc (a skill-slot expansion item).
//   Consuming the item adds +5 skill points (CPlayer+0xce) and replies with the
//   refreshed level/exp/faculty block. body 0x11 (17).
// -----------------------------------------------------------------------------
class CCSSkillSlot : public CHeadPacket
{ public:
    int   m_nItemSeq;        // +0x0c
};

class CSCSkillSlot : public CHeadPacket
{ public:
    char  m_nResponse;       // +0x0c
    int   m_nItemSeq;        // +0x0d  (echo of request itemSeq)
    int   m_nLevel;          // +0x11  word @CPlayer+0xc4 (level + pad)
    int   m_nExp;            // +0x15  CPlayer+0xc8
    int   m_nFaculty;        // +0x19  word @CPlayer+0xcc (faculty + skill point)
    CSCSkillSlot() : CHeadPacket(CM_SKILL_SLOTT) { m_nBodySize = 0x11; }
};

#pragma pack(pop)

#endif // _GAME_PROTOCOL_EQUIP_H_
