// =============================================================================
// load.cpp - CLoad: open the game CSV tables + DB-sourced notice/event data.
// Portable: CTable / CSql APIs only. See GameLoad.h for the faithfulness note
// (handle-based stand-in for the binary's typed-array CLoad; CSV schema is exact).
// =============================================================================
#include "Main.h"
#include "GameLoad.h"
#include "Sql.h"
#include "CTable.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

CLoad::CLoad()
    : m_hItem(0), m_hItemClass(0), m_hItemFaculty(0), m_hItemMix(0),
      m_hSkill(0), m_hCeremony(0), m_hTraining(0), m_hFaculty(0), m_hOption(0),
      m_hQuest(0), m_hName(0), m_hFilter(0), m_hGround(0), m_hBall(0), m_hPosition(0),
      m_hSituation(0), m_hMsg(0), m_hGameTip(0),
      m_hAIName(0), m_hAIAbility(0), m_hAICard(0), m_hAICostume(0),
      m_nNoticeVersion(0) {}

CLoad::~CLoad() {}

// Open one table under ../Table/<Company>/Table_<name>.csv (company dir from config).
static int OpenTable(const char* name)
{
    char sPath[160];
    snprintf(sPath, sizeof(sPath), "%s/%s", g_Config.m_sPath, name);
    return Table_FindTable(sPath);
}

bool CLoad::InitLoad()
{
    Table_SetRoot("../Table");   // company dir is prefixed per file

    m_hItem        = OpenTable("Table_Item.csv");
    m_hItemClass   = OpenTable("Table_ItemClass.csv");
    m_hItemFaculty = OpenTable("Table_ItemFaculty.csv");
    m_hItemMix     = OpenTable("Table_ItemMix.csv");
    m_hSkill       = OpenTable("Table_Skill.csv");
    m_hCeremony    = OpenTable("Table_Ceremony.csv");
    m_hTraining    = OpenTable("Table_Training.csv");
    m_hFaculty     = OpenTable("Table_Faculty.csv");
    m_hOption      = OpenTable("Table_Option.csv");
    m_hQuest       = OpenTable("Table_Quest.csv");
    m_hName        = OpenTable("Table_Name.csv");
    m_hFilter      = OpenTable("Table_Filter.csv");
    m_hGround      = OpenTable("Table_Ground.csv");
    m_hBall        = OpenTable("Table_Ball.csv");
    m_hPosition    = OpenTable("Table_Position.csv");
    m_hSituation   = OpenTable("Table_Situation.csv");
    m_hMsg         = OpenTable("Table_Msg.csv");
    m_hGameTip     = OpenTable("Table_GameTip.csv");
    m_hAIName      = OpenTable("Table_AI_Name.csv");
    m_hAIAbility   = OpenTable("Table_AI_Ability.csv");
    m_hAICard      = OpenTable("Table_AI_Card.csv");
    m_hAICostume   = OpenTable("Table_AI_Costume.csv");

    LoadItemTable();      // parse Table_Item.csv into the code->CItemTable* map
    LoadEnchantTable();   // parse Table_Enchant.csv into the enchant-cost map
    LoadSituationTable(); // parse Table_Situation.csv into the weighted-ratio map
    LoadOptionTable();    // parse Table_Option.csv into the per-grade/equip maps
    LoadNoticeTable();
    LoadEventList();
    return true;
}

void CLoad::LoadNoticeTable()
{
    ClearNoticeData();
    m_nNoticeVersion = g_Sql.GetNoticeTable(&m_cNoticeArray);
}

void CLoad::LoadEventList()
{
    ClearEventList();
    g_Sql.GetEventList(&m_vEventList);
}

void CLoad::ClearNoticeData()
{
    for (CStringArray::iterator it = m_cNoticeArray.begin(); it != m_cNoticeArray.end(); ++it)
        delete[] it->second;
    m_cNoticeArray.clear();
}

void CLoad::ClearEventList()
{
    for (VectorEventList::iterator it = m_vEventList.begin(); it != m_vEventList.end(); ++it)
        delete *it;
    m_vEventList.clear();
}

int CLoad::GetNoticeVersion() { return m_nNoticeVersion; }

// -----------------------------------------------------------------------------
// Item-table map (CLoad::LoadItemTable @08050d42 / GetItemTable @0805a8d2).
// Parses Table_Item.csv into a code->CItemTable* map. Column->offset mapping is
// byte-exact (see CItemTable in GameLoad.h); fields the runtime never reads are
// skipped. Built lazily on first GetItemTable() call.
// -----------------------------------------------------------------------------
void CLoad::LoadItemTable()
{
    for (std::map<int, CItemTable*>::iterator it = m_mItemTable.begin(); it != m_mItemTable.end(); ++it)
        delete it->second;
    m_mItemTable.clear();
    for (int e = 0; e < 17; ++e)
    {
        m_nManRatio[e] = m_nWomRatio[e] = 0;
        m_cManItemArray[e].clear();
        m_cWomItemArray[e].clear();
    }

    if (m_hItem == 0 || m_hItem == -1) return;

    for (int i = 0; ; ++i)
    {
        tvar_t* code = Table_GetData(m_hItem, i, "Code");
        if (code == 0) break;

        CItemTable* t = new CItemTable();
        memset(t, 0, sizeof(*t));
        t->m_nCode     = FieldToValue(code);
        t->m_nType     = (short)FieldToValue(Table_GetData(m_hItem, i, "Type"));
        t->m_nColor    = (char) FieldToValue(Table_GetData(m_hItem, i, "Color"));
        FieldToString(t->m_sName, Table_GetData(m_hItem, i, "Name"), (int)sizeof(t->m_sName) - 1);
        t->m_nWear     =        FieldToValue(Table_GetData(m_hItem, i, "Wear"));
        t->m_nCompany  = (short)FieldToValue(Table_GetData(m_hItem, i, "Company"));
        t->m_nGender   = (char) FieldToValue(Table_GetData(m_hItem, i, "Gender"));
        t->m_nBrand    = (char) FieldToValue(Table_GetData(m_hItem, i, "Brand"));
        t->m_nDivest   =        FieldToValue(Table_GetData(m_hItem, i, "Divest"));
        t->m_nLimit    = (char) FieldToValue(Table_GetData(m_hItem, i, "Limit"));
        t->m_nExchange = (char) FieldToValue(Table_GetData(m_hItem, i, "Exchange"));
        t->m_nInchant  = (char) FieldToValue(Table_GetData(m_hItem, i, "Inchant"));
        t->m_nSum      = (short)FieldToValue(Table_GetData(m_hItem, i, "Sum"));
        t->m_nGift     = (char) FieldToValue(Table_GetData(m_hItem, i, "Gift"));
        t->m_nParcel   = (char) FieldToValue(Table_GetData(m_hItem, i, "Parcel"));
        t->m_nFree     = (char) FieldToValue(Table_GetData(m_hItem, i, "Free"));
        t->m_nNew      = (char) FieldToValue(Table_GetData(m_hItem, i, "New"));
        t->m_nEvent    = (char) FieldToValue(Table_GetData(m_hItem, i, "Event"));
        t->m_nAmount   = (short)FieldToValue(Table_GetData(m_hItem, i, "Amount"));
        t->m_nDiscount = (char) FieldToValue(Table_GetData(m_hItem, i, "Discount"));
        t->m_nSell     = (char) FieldToValue(Table_GetData(m_hItem, i, "Sell"));
        t->m_nCash     =        FieldToValue(Table_GetData(m_hItem, i, "Cash"));
        t->m_nPoint    =        FieldToValue(Table_GetData(m_hItem, i, "Point"));
        t->m_nOption[0]=        FieldToValue(Table_GetData(m_hItem, i, "Option1"));
        t->m_nOption[1]=        FieldToValue(Table_GetData(m_hItem, i, "Option2"));
        t->m_nOption[2]=        FieldToValue(Table_GetData(m_hItem, i, "Option3"));
        t->m_nOption[3]=        FieldToValue(Table_GetData(m_hItem, i, "Option4"));
        t->m_nOption[4]=        FieldToValue(Table_GetData(m_hItem, i, "Option5"));
        t->m_nRate     =        FieldToValue(Table_GetData(m_hItem, i, "Rate"));

        m_mItemTable[t->m_nCode] = t;

        // Random-shop weighted maps (gender x equip-type, keyed by cumulative Rate):
        //   gender 1 -> man only, 2 -> woman only, 0 -> both. (LoadItemTable @08050d42)
        if (t->m_nRate > 0)
        {
            int e = GetItemEquipFromType(t->m_nType);
            if (e >= 0 && e < 17)
            {
                if (t->m_nGender != 2) { m_nManRatio[e] += t->m_nRate; m_cManItemArray[e][m_nManRatio[e]] = t; }
                if (t->m_nGender != 1) { m_nWomRatio[e] += t->m_nRate; m_cWomItemArray[e][m_nWomRatio[e]] = t; }
            }
        }
    }
}

CItemTable* CLoad::GetItemTable(int nCode)
{
    if (m_mItemTable.empty())
        LoadItemTable();
    std::map<int, CItemTable*>::iterator it = m_mItemTable.find(nCode);
    return (it == m_mItemTable.end()) ? 0 : it->second;
}

// -----------------------------------------------------------------------------
// CLoad::LoadItemEnchantTable @08052e0e - parse Table_Enchant.csv (columns
// Type/Point/Cash/Level) into m_mEnchantTable, keyed by row index.
// -----------------------------------------------------------------------------
void CLoad::LoadEnchantTable()
{
    for (std::map<int, CEnchantTable*>::iterator it = m_mEnchantTable.begin(); it != m_mEnchantTable.end(); ++it)
        delete it->second;
    m_mEnchantTable.clear();

    int h = Table_FindTable("Table_Enchant.csv");   // base Table dir (bare name, like the binary)
    if (h == 0 || h == -1) return;

    for (int i = 0; ; ++i)
    {
        tvar_t* code = Table_GetData(h, i, "Code");
        if (code == 0) break;

        CEnchantTable* t = new CEnchantTable();
        t->m_nEnchantType = (char)FieldToValue(Table_GetData(h, i, "Type"));
        t->m_nPoint       =       FieldToValue(Table_GetData(h, i, "Point"));
        t->m_nCash        =       FieldToValue(Table_GetData(h, i, "Cash"));
        t->m_nLimit       = (char)FieldToValue(Table_GetData(h, i, "Level"));
        m_mEnchantTable[i] = t;
    }
}

// CLoad::GetEnchantTable @080532ea - find the enchant cost row for (type, limit).
// nLimit is normalized up to the next multiple of 5 (the table's level bands).
CEnchantTable* CLoad::GetEnchantTable(int nType, int nLimit)
{
    if (m_mEnchantTable.empty())
        LoadEnchantTable();
    int nBand = nLimit - (nLimit % 5) + 5;
    for (std::map<int, CEnchantTable*>::iterator it = m_mEnchantTable.begin(); it != m_mEnchantTable.end(); ++it)
    {
        CEnchantTable* t = it->second;
        if (t && t->m_nLimit == nBand && t->m_nEnchantType == (char)nType) return t;
    }
    return 0;
}

// -----------------------------------------------------------------------------
// CLoad::LoadSituationTable @080556ba - parse Table_Situation.csv (Code/Ratio)
// into m_cSituationArray, keyed by the RUNNING CUMULATIVE ratio (rows with ratio
// 0 are skipped). GetRandomSituationTable then picks weighted via lower_bound.
// -----------------------------------------------------------------------------
void CLoad::LoadSituationTable()
{
    for (std::map<int, CSituationTable*>::iterator it = m_cSituationArray.begin(); it != m_cSituationArray.end(); ++it)
        delete it->second;
    m_cSituationArray.clear();

    std::string sPath = std::string(g_Config.m_sPath) + "/Table_Situation.csv";
    int h = Table_FindTable(sPath.c_str());
    if (h == 0 || h == -1) return;

    int nCumulative = 0;
    for (int i = 0; ; ++i)
    {
        tvar_t* code = Table_GetData(h, i, "Code");
        if (code == 0) break;

        CSituationTable* t = new CSituationTable();
        t->m_nCode  = FieldToValue(code);
        t->m_nRatio = FieldToValue(Table_GetData(h, i, "Ratio"));
        if (t->m_nRatio != 0)
        {
            nCumulative += t->m_nRatio;
            m_cSituationArray[nCumulative] = t;   // key = cumulative ratio threshold
        }
        else
        {
            delete t;   // ratio 0 -> not selectable (binary allocates-and-drops; we free it)
        }
    }
}

// CLoad::GetSituationTable @0805aa6c - row at the exact cumulative-ratio key.
CSituationTable* CLoad::GetSituationTable(int nKey)
{
    std::map<int, CSituationTable*>::iterator it = m_cSituationArray.find(nKey);
    return (it == m_cSituationArray.end()) ? 0 : it->second;
}

// CLoad::GetRandomSituationTable @08055e14 - order<=1 returns the fixed code-100
// row; otherwise a ratio-weighted pick (rand % maxCumulative -> lower_bound).
CSituationTable* CLoad::GetRandomSituationTable(int nOrder, int nRand)
{
    if (m_cSituationArray.empty()) LoadSituationTable();
    if (nOrder <= 1) return GetSituationTable(100);
    if (m_cSituationArray.empty()) return 0;
    int nMax = m_cSituationArray.rbegin()->first;   // largest cumulative key
    if (nMax <= 0) return 0;
    std::map<int, CSituationTable*>::iterator it = m_cSituationArray.lower_bound(nRand % nMax);
    return (it == m_cSituationArray.end()) ? 0 : it->second;
}

// -----------------------------------------------------------------------------
// CLoad::LoadOptionTable @08053420 - parse Table_Option.csv. Each option row has
// 6 '|'-split value bands (Usual/Special) and, per equip column (Hair=1..Knee=13),
// 5 '|'-split grade ratios. The per-(grade,equip) weighted maps are keyed by the
// RUNNING CUMULATIVE ratio; m_n*Ratio[] hold the running totals (= max key).
// -----------------------------------------------------------------------------
void CLoad::LoadOptionTable()
{
    for (std::map<int, COptionTable*>::iterator it = m_cOptionArray.begin(); it != m_cOptionArray.end(); ++it)
        delete it->second;
    m_cOptionArray.clear();
    for (int e = 0; e < 17; ++e)
    {
        m_nNormalRatio[e] = m_nRareRatio[e] = m_nUniqueRatio[e] = m_nEpicRatio[e] = m_nLegendRatio[e] = 0;
        m_cNormalArray[e].clear(); m_cRareArray[e].clear(); m_cUniqueArray[e].clear();
        m_cEpicArray[e].clear();   m_cLegendArray[e].clear();
    }

    std::string sPath = std::string(g_Config.m_sPath) + "/Table_Option.csv";
    int h = Table_FindTable(sPath.c_str());
    if (h == 0 || h == -1) return;

    static const char* kEquip[13] =
        { "Hair","Tattoo","Shirts","Pants","Glove","Shoes","Socks",
          "Eye","Ear","Neck","Wrist","Arm","Knee" };   // -> equip index 1..13

    for (int i = 0; ; ++i)
    {
        tvar_t* code = Table_GetData(h, i, "Code");
        if (code == 0) break;

        COptionTable* t = new COptionTable();
        memset(t, 0, sizeof(*t));
        t->m_nCode  = FieldToValue(code);
        t->m_nPrice = FieldToValue(Table_GetData(h, i, "Price"));
        tvar_t* usual   = Table_GetData(h, i, "Usual");
        tvar_t* special = Table_GetData(h, i, "Special");
        for (int k = 0; k < 6; ++k)
        {
            t->m_nUsual[k]   = (char)FieldsToValue(k, usual);
            t->m_nSpecial[k] = (char)FieldsToValue(k, special);
        }
        for (int c = 0; c < 13; ++c)
        {
            int e = c + 1;
            tvar_t* col = Table_GetData(h, i, kEquip[c]);
            t->m_nNormalRatio[e] = (char)FieldsToValue(0, col);
            t->m_nRareRatio[e]   = (char)FieldsToValue(1, col);
            t->m_nUniqueRatio[e] = (char)FieldsToValue(2, col);
            t->m_nEpicRatio[e]   = (char)FieldsToValue(3, col);
            t->m_nLegendRatio[e] = (char)FieldsToValue(4, col);
        }

        m_cOptionArray[t->m_nCode] = t;
        for (int e = 1; e <= 13; ++e)
        {
            m_nNormalRatio[e] += t->m_nNormalRatio[e];
            if (t->m_nNormalRatio[e]) m_cNormalArray[e][m_nNormalRatio[e]] = t;
            m_nRareRatio[e]   += t->m_nRareRatio[e];
            if (t->m_nRareRatio[e])   m_cRareArray[e][m_nRareRatio[e]] = t;
            m_nUniqueRatio[e] += t->m_nUniqueRatio[e];
            if (t->m_nUniqueRatio[e]) m_cUniqueArray[e][m_nUniqueRatio[e]] = t;
            m_nEpicRatio[e]   += t->m_nEpicRatio[e];
            if (t->m_nEpicRatio[e])   m_cEpicArray[e][m_nEpicRatio[e]] = t;
            m_nLegendRatio[e] += t->m_nLegendRatio[e];
            if (t->m_nLegendRatio[e]) m_cLegendArray[e][m_nLegendRatio[e]] = t;
        }
    }
}

// CLoad::GetRandomOptionTable @08055c10 - weighted pick from the (grade, equip)
// map: nOrder selects the grade (0 Normal .. 4 Legend), nEquipType the slot.
COptionTable* CLoad::GetRandomOptionTable(int nEquipType, int nOrder, int nRand)
{
    if (m_cOptionArray.empty()) LoadOptionTable();
    if (nEquipType < 0 || nEquipType >= 17) return 0;

    std::map<int, COptionTable*>* pArr = 0;
    int nMax = 0;
    switch (nOrder)
    {
        case 0: pArr = &m_cNormalArray[nEquipType]; nMax = m_nNormalRatio[nEquipType]; break;
        case 1: pArr = &m_cRareArray[nEquipType];   nMax = m_nRareRatio[nEquipType];   break;
        case 2: pArr = &m_cUniqueArray[nEquipType]; nMax = m_nUniqueRatio[nEquipType]; break;
        case 3: pArr = &m_cEpicArray[nEquipType];   nMax = m_nEpicRatio[nEquipType];   break;
        case 4: pArr = &m_cLegendArray[nEquipType]; nMax = m_nLegendRatio[nEquipType]; break;
        default: return 0;
    }
    if (nMax <= 0 || pArr->empty()) return 0;
    std::map<int, COptionTable*>::iterator it = pArr->lower_bound(nRand % nMax);
    return (it == pArr->end()) ? 0 : it->second;
}

// CLoad::GetRandomItemTable @08055ad2 - weighted pick from the (gender, equip)
// item map (gender 1 man / 2 woman); lower_bound(rand % maxCumulativeRate).
CItemTable* CLoad::GetRandomItemTable(int nEquipType, int nGender, int nRand)
{
    if (m_mItemTable.empty()) LoadItemTable();
    if (nEquipType < 0 || nEquipType >= 17) return 0;

    std::map<int, CItemTable*>* pArr;
    if      (nGender == 1) pArr = &m_cManItemArray[nEquipType];
    else if (nGender == 2) pArr = &m_cWomItemArray[nEquipType];
    else                   return 0;

    if (pArr->empty()) return 0;
    int nMax = pArr->rbegin()->first;
    if (nMax == 0) return 0;
    std::map<int, CItemTable*>::iterator it = pArr->lower_bound(nRand % nMax);
    return (it == pArr->end()) ? 0 : it->second;
}

// Banned/reserved word filter over Table_Name.csv (exact=type1 / substring=type2).
bool CLoad::IsPossibleName(const char* name)
{
    if (m_hName == 0) return true;
    std::string stName(name);
    std::transform(stName.begin(), stName.end(), stName.begin(), ::tolower);

    for (int i = 0; ; ++i)
    {
        tvar_t* code = Table_GetData(m_hName, i, "Code");
        if (code == 0) break;
        int nType = FieldToValue(Table_GetData(m_hName, i, "Type"));
        if (nType == 0) continue;

        char sWord[32];
        FieldToString(sWord, Table_GetData(m_hName, i, "Word"), (int)sizeof(sWord) - 1);
        std::string stWord(sWord);
        std::transform(stWord.begin(), stWord.end(), stWord.begin(), ::tolower);

        if (nType == 1 && stName == stWord) return false;
        if (nType == 2 && stName.find(stWord) != std::string::npos) return false;
    }
    return true;
}
