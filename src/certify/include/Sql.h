// CSql : public CDatabase - all account/character DB access for Certify.
// 38 methods recovered from Ghidra. Signatures are best-effort and get pinned
// when bodies are reconstructed (Phase 2/3) by decompiling each function.
// These method names + the SQL strings they contain are the primary source for
// reverse-engineering the DB schema (Phase 4).
#ifndef _CERTIFY_SQL_H_
#define _CERTIFY_SQL_H_

#include "Database.h"
#include "Struct.h"      // CSetting, CAddress, CSale (CItem is now certify Item.h)
#include "Protocol.h"    // CCharacterInfo, CSCServerList, CCSCreateCharacter, CCSDeleteCharacter

class CMember;
class CServer;
class CEvent;
class CItem;             // 36-byte record (certify/include/Item.h); pointer param below

// Signatures pinned against the binary (DWARF prototypes from Ghidra). All DB
// methods return int (0/>=0 success, negative reason codes); see sql.cpp.
class CSql : public CDatabase
{
public:
    CSql();
    ~CSql();

    bool InitServer();                         // register DBs, read port/slots, load default setting

    // --- login / member ---
    int  InitMemberLogin();                    // bulk: mark all members on this server logged-out
    int  GetMemberFields(CMember* pMember, int nCommand);    // GetMemberField + GetTrioField
    int  GetMemberField(CMember* pMember);
    int  GetTrioField(CMember* pMember, int nCommand);       // account-wide wallet/flags
    int  GetSettingField(int nPlayerSeq, CSetting* pSetting);
    int  GetCharacterInfo(CMember* pMember, int nOrder, CCharacterInfo* pInfo);
    int  CheckGhostHost(const char* sID);      // returns trio_host flag (>0 = host allowed)
    int  CheckMemberID(CMember* pMember, const char* sID, const char* sPass, const char* sVersion);
    int  CheckPlayerName(const char* sName);
    int  WhereIsPlayer(int nMemberSeq, int* nServerCode, int* nPlayerSeq);

    // --- character CRUD ---
    int  CreateCharacter(CMember* pMember, CCSCreateCharacter* pCreate);
    int  DeleteCharacter(CMember* pMember, CCSDeleteCharacter* pDelete);

    // --- server list / routing ---
    int  GetServerCurrent(int nServerCode);
    int  GetServerList(int nChannel, CSCServerList* pList);
    int  CheckChoiceServer(int nServerCode, CAddress* pAddress);

    // --- money / setting / sequence ---
    int  GetMoneyField(CMember* pMember);
    int  UpdateMoneyField(CMember* pMember);
    int  UpdateSettingField(int nPlayerSeq, CSetting* pSetting);
    int  UpdateLastSeqField(CMember* pMember);

    // --- session bookkeeping ---
    int  ConnectServer(int nServerCode);
    int  DisconnectServer(int nServerCode);
    int  SetLoginData(int nMemberSeq);
    int  SetLogOutData(int nMemberSeq);
    int  SetDeleteDate(int nMemberSeq);
    int  SetExecuteTutorial(int nMemberSeq, int nTutorial);
    int  SetExecuteQuest(int nMemberSeq, int nQuest);

    // --- inserts / logs ---
    int  InsertMemberField(const char* sID, const char* sPass);
    int  InsertItemField(CMember* pMember, CItem* pItem);
    int  InsertSaleLog(CMember* pMember, CSale* pSale);
    int  InsertConnectLog(int nConnect, int nRelay, int nRoom);

    // --- misc tables ---
    int  CheckSampleTable(const char* strTable, int nArg1, int nArg2);  // ensure quarterly/monthly shard
    int  GetNoticeTable(CStringArray* pArray);   // returns notice "version" (newest timestamp)
    int  GetEventList(VectorEventList* pList);
};

#endif
