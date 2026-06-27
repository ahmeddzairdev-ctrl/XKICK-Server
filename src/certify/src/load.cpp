// =============================================================================
// load.cpp - CLoad: in-memory tables for Certify (reconstructed from
// XKICK_Certify). Notice text comes from the DB (CSql::GetNoticeTable); the
// Quest/Name tables are CSV (Table_* API). Events come from the DB.
//
// InitLoad in the binary only loads the notice + name tables; quest/event are
// loaded on demand. Table files live under ../Table/<CompanyDir>; we set the
// root to "../Table" and prefix the company dir per file (as the binary does).
//
// Portable: std C++ + the CTable / CSql APIs only. No OS headers.
// =============================================================================
#include "Main.h"
#include "CertifyLoad.h"
#include "CTable.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
CLoad::CLoad()  { m_nNoticeVersion = 0; }
CLoad::~CLoad() {}

// ---------------------------------------------------------------------------
// InitLoad - load notice + name tables (as the binary does).
// ---------------------------------------------------------------------------
bool CLoad::InitLoad()
{
    Table_SetRoot("../Table");   // company dir is prefixed per file below
    LoadNoticeTable();
    LoadNameTable();
    return true;
}

// ---------------------------------------------------------------------------
// LoadNoticeTable - rebuild the notice map from the DB; version = newest stamp.
// ---------------------------------------------------------------------------
void CLoad::LoadNoticeTable()
{
    ClearNoticeData();
    m_nNoticeVersion = g_Sql.GetNoticeTable(&m_cNoticeArray);
}

// ---------------------------------------------------------------------------
// LoadQuestTable - parse Table_Quest.csv into m_cQuestArray.
// ---------------------------------------------------------------------------
void CLoad::LoadQuestTable()
{
    std::string sPath = std::string(g_Config.m_sPath) + "/Table_Quest.csv";
    int h = Table_FindTable(sPath.c_str());

    for (int i = 0; ; ++i)
    {
        tvar_t* field = Table_GetData(h, i, "Code");
        if (field == 0) return;

        CQuestTable* pRec = new CQuestTable();
        if (pRec == 0) { printf("Quest Table Memory Creation error\n"); exit(1); }

        pRec->m_nCode     = ATOI(field->string);
        pRec->m_nPosition = (char)FieldToValue(Table_GetData(h, i, "Position"));
        pRec->m_nDiff     = (char)FieldToValue(Table_GetData(h, i, "Diff"));
        pRec->m_nMainType = (char)FieldToValue(Table_GetData(h, i, "Type"));
        pRec->m_nType[0]  = (char)FieldToValue(Table_GetData(h, i, "Type1"));
        pRec->m_nType[1]  = (char)FieldToValue(Table_GetData(h, i, "Type2"));
        pRec->m_nType[2]  = (char)FieldToValue(Table_GetData(h, i, "Type3"));
        pRec->m_nLimit    = (char)FieldToValue(Table_GetData(h, i, "Limit"));
        pRec->m_nJoin     = (char)FieldToValue(Table_GetData(h, i, "Join"));
        pRec->m_nRepeat   = (char)FieldToValue(Table_GetData(h, i, "Repeat"));
        pRec->m_nCondition = FieldToValue(Table_GetData(h, i, "Condition"));

        pRec->m_cGift[0].m_nKind   = (char)FieldToValue(Table_GetData(h, i, "Kind1"));
        pRec->m_cGift[0].m_nCode   = FieldToValue(Table_GetData(h, i, "Gift1"));
        pRec->m_cGift[0].m_nAmount = FieldToValue(Table_GetData(h, i, "Amount1"));
        pRec->m_cGift[1].m_nKind   = (char)FieldToValue(Table_GetData(h, i, "Kind2"));
        pRec->m_cGift[1].m_nCode   = FieldToValue(Table_GetData(h, i, "Gift2"));
        pRec->m_cGift[1].m_nAmount = FieldToValue(Table_GetData(h, i, "Amount2"));
        pRec->m_cGift[2].m_nKind   = (char)FieldToValue(Table_GetData(h, i, "Kind3"));
        pRec->m_cGift[2].m_nCode   = FieldToValue(Table_GetData(h, i, "Gift3"));
        pRec->m_cGift[2].m_nAmount = FieldToValue(Table_GetData(h, i, "Amount3"));

        FieldToString(pRec->m_sTitle, Table_GetData(h, i, "Title"), 0x2f);

        m_cQuestArray[pRec->m_nCode] = pRec;
    }
}

// ---------------------------------------------------------------------------
// LoadNameTable - parse Table_Name.csv (banned/reserved word filter).
// ---------------------------------------------------------------------------
void CLoad::LoadNameTable()
{
    std::string sPath = std::string(g_Config.m_sPath) + "/Table_Name.csv";
    int h = Table_FindTable(sPath.c_str());

    for (int i = 0; ; ++i)
    {
        tvar_t* field = Table_GetData(h, i, "Code");
        if (field == 0) return;

        CNameTable* pRec = new CNameTable();
        if (pRec == 0) { printf("Name Table Memory Creation error\n"); exit(1); }

        pRec->m_nCode = ATOI(field->string);
        pRec->m_nType = (char)FieldToValue(Table_GetData(h, i, "Type"));
        FieldToString(pRec->m_sWord, Table_GetData(h, i, "Word"), 0xf);

        m_cNameArray[pRec->m_nCode] = pRec;
    }
}

// ---------------------------------------------------------------------------
// LoadEventList - rebuild the event list from the DB.
// ---------------------------------------------------------------------------
void CLoad::LoadEventList()
{
    ClearEventList();
    g_Sql.GetEventList(&m_vEventList);
}

// ---------------------------------------------------------------------------
// ClearNoticeData / ClearEventList - free + clear the owned records.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// GetQuestTable / GetNameTable - lookup by code (NULL if absent). Faithful to
// the binary, which removes the stray default-inserted entry on a miss.
// ---------------------------------------------------------------------------
CQuestTable* CLoad::GetQuestTable(int nCode)
{
    CQuestTable* pRec = m_cQuestArray[nCode];
    if (pRec == 0)
        m_cQuestArray.erase(m_cQuestArray.find(nCode));
    return pRec;
}

CNameTable* CLoad::GetNameTable(int nCode)
{
    CNameTable* pRec = m_cNameArray[nCode];
    if (pRec == 0)
        m_cNameArray.erase(m_cNameArray.find(nCode));
    return pRec;
}

// ---------------------------------------------------------------------------
// IsPossibleName - word filter. For each table word (lowercased): type 1 means
// an exact match is banned; type 2 means a substring match is banned. Returns
// true if the name passes (no banned word matched).
// ---------------------------------------------------------------------------
bool CLoad::IsPossibleName(const char* name)
{
    std::string stName(name);

    for (CNameArray::iterator it = m_cNameArray.begin(); it != m_cNameArray.end(); ++it)
    {
        CNameTable* pTable = it->second;
        if (pTable == 0)
            return false;
        if (pTable->m_nType == 0)
            continue;

        std::string stWord = pTable->m_sWord;
        std::transform(stName.begin(), stName.end(), stName.begin(), ::tolower);
        std::transform(stWord.begin(), stWord.end(), stWord.begin(), ::tolower);

        if (pTable->m_nType == 1)
        {
            if (stName == stWord) return false;     // exact reserved name
        }
        else if (pTable->m_nType == 2)
        {
            if (stName.find(stWord) != std::string::npos) return false;  // banned substring
        }
    }
    return true;
}

int CLoad::GetNoticeVersion() { return m_nNoticeVersion; }
