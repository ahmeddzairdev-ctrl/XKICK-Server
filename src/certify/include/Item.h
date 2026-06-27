// Item.h (Certify) - the 36-byte inventory item record Certify uses for item
// inserts. This is server-specific (the Game's CItem is a wider 80-byte runtime
// object), so it lives in each server's own include tree, not in shared Struct.h.
// The shared on-the-wire row is CItemInfo (Protocol.h).
#ifndef _CERTIFY_ITEM_H_
#define _CERTIFY_ITEM_H_

class CItem
{
public:
    char  m_nState;          // 0   OBJECT_STATE_*
    int   m_nItemSeq;        // 4
    int   m_nCode;           // 8
    char  m_nEquipKind;      // 12
    short m_nAmount;         // 14
    int   m_nOptionCode[5];  // 16  ITEM_OPTION_SIZE
};

#endif // _CERTIFY_ITEM_H_
