// =============================================================================
// XKICK_Game - GameLoad.h
// class CLoad : the static game-data tables loaded from ../Table/<Company>/*.csv
// (22 CSV files) plus notice/event data pulled from the DB (like Certify).
//
// NOTE (faithfulness): the binary's CLoad (g_Load, 3780 bytes) parses each CSV
// into typed in-memory arrays. Pending a byte-exact Ghidra pass of CLoad::InitLoad,
// this is reconstructed with the portable CTable handle API: InitLoad opens every
// table once (Table_FindTable caches the parse) and the Get*Table() accessors hand
// back the cached handle so handlers query rows via Table_GetData. Functionally
// equivalent for lookups; the typed-array internals can be refined later. The CSV
// schema (columns) is the ground truth either way.  TODO(5d-verify): pin CLoad's
// internal layout + per-table record structs to the binary.
// =============================================================================
#ifndef _GAME_LOAD_H_
#define _GAME_LOAD_H_

#include "CTable.h"     // Table_FindTable / Table_GetData / FieldToValue
#include <map>
#include <vector>

// Raw-row compatibility accessor used by player.cpp's binary-format table reads
// (InitBaseFaculty/SetEquipWear: Table_GetData(handle, index, -1) -> row blob).
// TODO(reconstruct): the binary parsed tables into packed records; until CLoad's
// typed layout is pinned this returns a zeroed scratch row (see stubs.cpp).
void* Table_GetData(int handle, int recordIndex, int rawRowSentinel);

// reward event row (tb_game_event), 16 bytes. Accessed by offset via PFIELD and
// allocated in CSql::GetEventList; complete here so ClearEventList can delete it.
class CEvent {
public:
    char m_nEventType;    // 0x0
    char m_nRewardType;   // 0x1
    int  m_nRewardValue;  // 0x4
    int  m_nStartTime;    // 0x8
    int  m_nEndTime;      // 0xc
};
// Item-mix recipe table row (Table_Item_Mix), 116 bytes. Layout verified against
// XKICK_Game1 CItemMixTable; natural alignment (not packed) reproduces the binary
// offsets - the int[5] arrays land 4-aligned after the char fields.
class CItemMixTable {
public:
    int  m_nCode;             // 0x00
    char m_sName[41];         // 0x04
    char m_nGender;           // 0x2d
    int  m_nItemCode[5];      // 0x30
    int  m_nItemCnt[5];       // 0x44
    int  m_nRewardItemCode;   // 0x58
    int  m_nRewardItemCnt;    // 0x5c
    int  m_nOptionCode[5];    // 0x60  -> 0x74 (116)
};

// Item-enchant cost row (Table_Enchant.csv), 16 bytes. Verified vs XKICK_Game1
// CEnchantTable; loaded by CLoad::LoadEnchantTable, selected by GetEnchantTable.
class CEnchantTable {
public:
    char m_nEnchantType;  // 0x00
    int  m_nPoint;        // 0x04
    int  m_nCash;         // 0x08
    char m_nLimit;        // 0x0c  (level band, normalized to a multiple of 5)
};

// Random-situation row (Table_Situation.csv), 8 bytes. Verified vs XKICK_Game1
// CSituationTable; the map is keyed by CUMULATIVE ratio for weighted selection.
class CSituationTable {
public:
    int m_nCode;          // 0x00
    int m_nRatio;         // 0x04
};

class COptionTable;   // full 108-byte definition in GameItem.h

typedef std::map<int, char*> CStringArray;   // seq -> notice text (DB)
typedef std::vector<CEvent*> VectorEventList;

// -----------------------------------------------------------------------------
// CItemTable -- one parsed row of Table_Item.csv (CLoad::LoadItemTable @08050d42).
// Field offsets are byte-exact (verified against the loader stores + the consumer
// reads in CPlayer::BuyItem/SellItem/RepairItem/ExchangeItem/GetShopItemList and
// GetItemSkin). Size 0x70. The runtime keeps a std::map<int,CItemTable*> keyed by
// item code (CLoad::GetItemTable @0805a8d2 == map::operator[]).
// -----------------------------------------------------------------------------
#pragma pack(push,1)
struct CItemTable
{
    int   m_nCode;        // 0x00  "Code"
    short m_nType;        // 0x04  "Type"
    char  m_pad06[2];     // 0x06
    char  m_pad08;        // 0x08
    char  m_nColor;       // 0x09  "Color"   (skin code; GetItemSkin)
    char  m_sName[0x29];  // 0x0a  "Name"    (0x0a..0x32)
    char  m_pad33;        // 0x33
    int   m_nWear;        // 0x34  "Wear"
    short m_nCompany;     // 0x38  "Company"
    char  m_nGender;      // 0x3a  "Gender"  (0 any / else must == player gender)
    char  m_nBrand;       // 0x3b  "Brand"
    int   m_nDivest;      // 0x3c  "Divest"
    char  m_nLimit;       // 0x40  "Limit"
    char  m_nExchange;    // 0x41  "Exchange" (exchange price multiplier)
    char  m_nInchant;     // 0x42  "Inchant"
    char  m_pad43;        // 0x43
    short m_nSum;         // 0x44  "Sum"     (stackable flag; ==1 -> stack)
    char  m_nGift;        // 0x46  "Gift"
    char  m_nParcel;      // 0x47  "Parcel"
    char  m_nFree;        // 0x48  "Free"    (hidden/not-sale flag; ==1 -> skip in shop)
    char  m_nNew;         // 0x49  "New"     (cash-item flag; ==1 -> kind==2 match)
    char  m_nEvent;       // 0x4a  "Event"
    char  m_pad4b;        // 0x4b
    short m_nAmount;      // 0x4c  "Amount"  (buy amount / repair max-durability)
    char  m_nDiscount;    // 0x4e  "Discount"(price multiplier, /100)
    char  m_nSell;        // 0x4f  "Sell"    (money-kind: 0 not-sold / 1 cash-only / 2 point-only)
    int   m_nCash;        // 0x50  "Cash"    (unit cash price)
    int   m_nPoint;       // 0x54  "Point"   (unit point price)
    int   m_nOption[5];   // 0x58  "Option1".."Option5"
    int   m_nRate;        // 0x6c  "Rate"    (random-shop spawn weight)
};
#pragma pack(pop)

class CLoad
{
public:
    // ---- CSV table handles (../Table/<Company>/Table_*.csv) ----
    int m_hItem, m_hItemClass, m_hItemFaculty, m_hItemMix;
    int m_hSkill, m_hCeremony, m_hTraining, m_hFaculty, m_hOption;
    int m_hQuest, m_hName, m_hFilter, m_hGround, m_hBall, m_hPosition;
    int m_hSituation, m_hMsg, m_hGameTip;
    int m_hAIName, m_hAIAbility, m_hAICard, m_hAICostume;

    // ---- DB-sourced data (like Certify) ----
    CStringArray    m_cNoticeArray;   // seq -> notice text
    VectorEventList m_vEventList;
    int             m_nNoticeVersion;

    // ---- parsed item table (code -> row); lazy-built from Table_Item.csv ----
    std::map<int, CItemTable*> m_mItemTable;

    // ---- enchant cost table (row index -> CEnchantTable); from Table_Enchant.csv ----
    std::map<int, CEnchantTable*> m_mEnchantTable;

    // ---- random-situation table (cumulative-ratio key -> row); Table_Situation.csv ----
    std::map<int, CSituationTable*> m_cSituationArray;

    // ---- random-option table (Table_Option.csv). m_cOptionArray is by code; the
    //      per-grade x per-equip-type weighted maps (indices 1..13) are keyed by
    //      cumulative ratio, with the running totals in m_n*Ratio[]. ----
    std::map<int, COptionTable*> m_cOptionArray;
    std::map<int, COptionTable*> m_cNormalArray[17];
    std::map<int, COptionTable*> m_cRareArray[17];
    std::map<int, COptionTable*> m_cUniqueArray[17];
    std::map<int, COptionTable*> m_cEpicArray[17];
    std::map<int, COptionTable*> m_cLegendArray[17];
    int m_nNormalRatio[17], m_nRareRatio[17], m_nUniqueRatio[17], m_nEpicRatio[17], m_nLegendRatio[17];

    // ---- random-item tables (built alongside m_mItemTable from Table_Item.csv):
    //      per-gender x per-equip-type weighted maps keyed by cumulative Rate. ----
    std::map<int, CItemTable*> m_cManItemArray[17];
    std::map<int, CItemTable*> m_cWomItemArray[17];
    int m_nManRatio[17], m_nWomRatio[17];

    CLoad();
    ~CLoad();

    bool InitLoad();                 // open all CSV tables + load notice/event from DB

    // ---- DB-backed reloadable lists ----
    void LoadNoticeTable();          // CSql::GetNoticeTable
    void LoadEventList();            // CSql::GetEventList
    void ClearNoticeData();
    void ClearEventList();
    int  GetNoticeVersion();

    // ---- parsed item-table accessor (code -> row); builds the map on first use ----
    CItemTable* GetItemTable(int nCode);        // CLoad::GetItemTable @0805a8d2
    void        LoadItemTable();                // build m_mItemTable from the CSV

    // ---- random-shop typed tables (CLoad random generators). The portable CLoad
    //      does not yet pin these typed maps, so they currently return 0 (the daily
    //      random shop generates an empty offer list -- links + runs safely). ----
    CItemTable*    GetRandomItemTable(int nEquipType, int nGender, int nRand); // @08055ad2

    // ---- random-option table (fully reconstructed: Table_Option.csv) ----
    void           LoadOptionTable();                              // @08053420
    COptionTable*  GetRandomOptionTable(int nEquipType, int nOrder, int nRand); // @08055c10

    // ---- enchant cost table (fully reconstructed: Table_Enchant.csv has data) ----
    void               LoadEnchantTable();                          // CLoad::LoadItemEnchantTable @08052e0e
    CEnchantTable*     GetEnchantTable(int nType, int nLimit);      // @080532ea

    // ---- random-situation table (fully reconstructed: Table_Situation.csv) ----
    void               LoadSituationTable();                        // @080556ba
    CSituationTable*   GetSituationTable(int nKey);                 // @0805aa6c  (by map key)
    CSituationTable*   GetRandomSituationTable(int nOrder, int nRand); // @08055e14

    // ---- CSV table accessors (handle -> Table_GetData by row index/field) ----
    int  GetItemTableHandle() const { return m_hItem; }
    int  GetItemClassTable()  const { return m_hItemClass; }
    int  GetItemFacultyTable()const { return m_hItemFaculty; }
    int  GetItemMixTable()    const { return m_hItemMix; }
    int  GetSkillTable()      const { return m_hSkill; }
    int  GetCeremonyTable()   const { return m_hCeremony; }
    int  GetTrainingTable()   const { return m_hTraining; }
    int  GetFacultyTable()    const { return m_hFaculty; }
    int  GetOptionTable()     const { return m_hOption; }
    int  GetQuestTable()      const { return m_hQuest; }
    int  GetGroundTable()     const { return m_hGround; }
    int  GetBallTable()       const { return m_hBall; }
    int  GetPositionTable()   const { return m_hPosition; }

    // ---- name/word filter (Table_Name.csv + Table_Filter.csv) ----
    bool IsPossibleName(const char* name);   // banned/reserved word check

    // ---- mission roll (used by CRoom::CreateMission) ----
    // Returns a pointer to a random mission row (int[]: seq, baseExp, basePoint,
    // double coef, minCount, maxCount, kindRange, name...), or 0 if none loaded.
    void* GetRandomMission();        // @0809... (CLoad::GetRandomMission)
};

#endif // _GAME_LOAD_H_
