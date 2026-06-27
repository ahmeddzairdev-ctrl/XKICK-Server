// CLoad - in-memory tables for Certify (load.cpp). Layout from Ghidra (112 bytes).
// NOTE: the surviving common/load.cpp is an OLDER C-style loader (CAPLoad /
// Table_GetData) and does NOT match this CLoad. Reconstruct from the binary.
#ifndef _CERTIFY_LOAD_H_
#define _CERTIFY_LOAD_H_

#include "Main.h"   // CNameArray/CQuestArray/CScheduleArray/CStringArray/VectorEventList

// --- table record types (exact layouts from Ghidra) ---
class CNameTable {        // 20 bytes - banned/reserved word filter (Table_Name.csv)
public:
    int  m_nCode;         // 0
    char m_nType;         // 4
    char m_sWord[15];     // 5
};

class CQuestTable {       // 104 bytes (Table_Quest.csv)
public:
    int  m_nCode;         // 0
    char m_nPosition;     // 4
    char m_nLevel;        // 5
    char m_nDiff;         // 6
    char m_nMainType;     // 7
    char m_nType[3];      // 8
    char m_nLimit;        // 11
    char m_nJoin;         // 12
    char m_nRepeat;       // 13
    int  m_nCondition;    // 16
    CGift m_cGift[3];     // 20 (CGift is 12 bytes; see Struct.h)
    char m_sTitle[47];    // 56
};

class CSchedule {         // 20 bytes - golden-time / tournament schedule
public:
    char m_nTimeType;     // 0
    char m_nDate;         // 1
    int  m_nServerCode;   // 4
    int  m_nTimeSeq;      // 8
    int  m_nStart;        // 12
    int  m_nEnd;          // 16
};

class CEvent {            // 16 bytes - active reward events
public:
    char m_nEventType;    // 0
    char m_nRewardType;   // 1
    int  m_nRewardValue;  // 4
    int  m_nStartTime;    // 8
    int  m_nEndTime;      // 12
};

class CLoad
{
public:
    VectorEventList m_vEventList;     // 0
    CStringArray    m_cNoticeArray;   // 12  (seq -> notice text)
    CScheduleArray  m_cScheduleArray; // 36
    CQuestArray     m_cQuestArray;    // 60
    CNameArray      m_cNameArray;     // 84
    int             m_nNoticeVersion; // 108

    CLoad();
    ~CLoad();

    bool InitLoad();                  // load all tables
    void LoadNoticeTable();           // from DB (CSql::GetNoticeTable)
    void LoadQuestTable();            // Table_Quest.csv
    void LoadNameTable();             // Table_Name.csv (word filter)
    void LoadEventList();             // from DB (CSql::GetEventList)

    void ClearNoticeData();
    void ClearEventList();

    CQuestTable* GetQuestTable(int code);
    CNameTable*  GetNameTable(int code);
    bool         IsPossibleName(const char* name);   // word-filter check
    int          GetNoticeVersion();
};

#endif
