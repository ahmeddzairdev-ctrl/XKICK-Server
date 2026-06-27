// =============================================================================
// XKICK_Game - Sql.h
// class CSql : public CDatabase - every character/game DB access the Game server
// performs. Reconstructed from the unstripped XKICK_Game1 Linux ELF (Ghidra).
//
// CSql owns the full character load chain (member/player/faculty/level/record/
// ranking/skill/training/ceremony/quest/card/item/buddy/blacklist), the matching
// persist (Update*/Insert*) methods, the gift/sale/shop helpers, the log shards,
// and server/session bookkeeping. Method names + their embedded SQL strings are
// the primary source for the Game-owned schema (docs/schema.sql extension).
//
// Conventions match the working Certify CSql layer:
//   * Query(fmt, ...)   -> MYSQL_RES* for SELECTs; caller frees with FREE_RESULT.
//   * Execute(fmt, ...) -> MYSQL* (non-NULL on success) for DML.
//   * FetchRow(res, &row) fills MY_ROW (std::map<string,char*>); row[col] is raw
//     column text, converted with atoi()/ATOI().
//   * Three logical DBs registered in InitServer: 0 = main (kicks2),
//     1 = log (kicks2_log), 2 = sample (CREATE TABLE templates). SetBaseDB()
//     switches which one Query/Execute target; log inserts flip to DB 1 then back.
//
// Return convention (recovered): 0/>=0 success; negative codes are reason codes
// (-1 query error, -2/-3 no row, -4 mismatch, etc.) preserved from the binary.
//
// Signatures are pinned to the binary's DWARF / mangled prototypes (addresses in
// the comments). CDatabase (common/Database.h) is reused, not redefined.
// =============================================================================
#ifndef _GAME_SQL_H_
#define _GAME_SQL_H_

#include "Database.h"   // CDatabase, MY_ROW, FREE_RESULT, *DBFLAG, CTime
#include "Struct.h"     // CSetting/CMoney/CItem/CSkillInfo/CSale/CBuddyInfo/CPlayLog ...

// ---- runtime classes filled by the loaders (defined in their own TUs) --------
class CPlayer;
class CItem;
class CSkill;
class CTraining;
class CCeremony;
class CCard;
class CSale;

// ---- wire packet / table structs used by the methods below (defined in
//      Protocol.h / GameProtocolItem.h / GameLoad.h). Forward-declared here. ----
class CSCGiftList;
class CSCSaleList;
class CSCWeeklyRecord;
class CSCWeeklyRanking;
class CCSMixItem1;
class CItemMixTable;
class CWeather;
class CEvent;

class CSql : public CDatabase
{
public:
    CSql();                                                 // @080a47c0
    ~CSql();                                                // @080a47ec

    bool InitServer();                                      // @080a4818  DBs + ports + g_Setting seed + InitMemberLogin

    // --- bulk / member / player load chain ---
    int  InitMemberLogin();                                 // @080a9604  mark this server logged-out
    int  GetPlayerFields(CPlayer* pPlayer);                 // @080a963e  load-everything wrapper
    int  GetMemberField(CPlayer* pPlayer);                  // @080a97ac  tb_game_trio wallet/flags
    int  GetPlayerField(CPlayer* pPlayer);                  // @080a9c10  tb_game_player base row
    int  GetFacultyField(CPlayer* pPlayer);                 // @080acaa2  tb_game_faculty
    int  GetRecordField(CPlayer* pPlayer);                  // @080acb66  tb_game_record (19-int block)
    int  GetRecordField_Weekly(int nPlayerSeq, CSCWeeklyRecord* pPacket);   // @080acde6
    int  GetRankingField_Weekly(int nPlayerSeq, CSCWeeklyRanking* pPacket); // @080ad2d8
    int  GetRankField(CPlayer* pPlayer);                    // @080ad7ca  tb_game_ranking
    int  GetItemField(CPlayer* pPlayer);                    // @080ae07c  tb_game_item -> m_vItemList
    int  GetGiftItemField(CPlayer* pPlayer, int nGiftSeq);  // @080aebda  tb_game_gift item grant
    int  GetCardField(CPlayer* pPlayer);                    // @080af728  tb_game_card -> m_vCardList
    int  GetSkillField(CPlayer* pPlayer);                   // @080b0122  tb_game_skill -> m_vSkillList
    int  GetTrainingField(CPlayer* pPlayer);                // @080b058c  tb_game_training
    int  GetCeremonyField(CPlayer* pPlayer);                // @080b09f6  tb_game_ceremony
    int  GetQuestField(CPlayer* pPlayer);                   // @080b0db4  tb_game_quest -> m_vQuestList

    // --- gift / sale / lookups ---
    int  GetGiftList(CPlayer* pPlayer, int nArg1, int nArg2, CSCGiftList* pList); // @080b1a6c
    int  GetSaleField(CPlayer* pPlayer, int nArg1, int nArg2, CSCSaleList* pList); // @080b4492
    int  GetMemberSeq(int nPlayerSeq);                      // @080b145e
    int  GetPlayerSeq(char* sName);                         // @080b15dc
    const char* GetPlayerName(int nPlayerSeq);             // @080b175a
    int  GetServerMaxField(CPlayer* pPlayer);               // @080b18cc
    int  CheckPlayerName(char* sName);                      // @080b37ec
    int  FindItemInPlayer(int nPlayerSeq, int nCode, int n3, int n4, int n5, int n6, int n7); // @080b8016

    // --- time / table loaders ---
    int  GetCurrentTime();                                  // @080b2d2c  NOW() epoch
    int  GetCurrentTimeSecond();                            // @080b2e78
    int  GetCurrentDay();                                   // @080b2fc4
    int  GetNoticeTable(std::map<int, char*>* pArray);      // @080b2980  tb_game_notice
    int  LoadWeatherTable(std::map<int, CWeather*>* pArray);// @080b3130  tb_game_weather
    int  GetEventList(std::vector<CEvent*>* pList);         // @080b23e8  tb_game_event
    int  GetExpireTime(int nItemSeq);                       // @080b77f2  item expiry epoch
    int  AddExpireTime(int nItemSeq, int nSeconds);         // @080b79fa

    // --- buddy / blacklist ---
    int  CheckBuddyField(CPlayer* pPlayer);                 // @080b38ec  tb_game_buddy -> m_vBuddyList
    int  CheckBlacklistField(CPlayer* pPlayer);             // @080b3d3c  tb_game_blacklist
    int  AddBuddyField(CPlayer* pPlayer, CPlayer* pTarget); // @080b418c
    int  AddBlackListField(CPlayer* pPlayer, CPlayer* pTarget); // @080b41ea
    int  DeleteBuddyField(CPlayer* pPlayer, int nPlayerSeq);    // @080b4248
    int  DeleteBlackListField(CPlayer* pPlayer, int nPlayerSeq);// @080b4294

    // --- per-field updates ---
    int  UpdatePositionField(CPlayer* pPlayer);             // @080b42e0
    int  UpdateFaculty(CPlayer* pPlayer);                   // @080b435c
    int  UpdatePlayerFields(CPlayer* pPlayer);              // @080b5670  persist-everything wrapper
    int  UpdateSettingField(CPlayer* pPlayer);              // @080b57de
    int  UpdateRecordField_Weekly(CPlayer* pPlayer);        // @080b5966
    int  UpdateRecordField(CPlayer* pPlayer);               // @080b5b9c
    int  UpdateItemField(CPlayer* pPlayer);                 // @080b5f1e
    int  UpdateCardField(CPlayer* pPlayer);                 // @080b615e
    int  AddItemAmount(int nItemSeq, int nAmount, int nArg);// @080b610e
    int  UpdateSkillField(CPlayer* pPlayer);                // @080b647e
    int  UpdateTrainingField(CPlayer* pPlayer);             // @080b65b0
    int  UpdateCeremonyField(CPlayer* pPlayer);             // @080b66e0
    int  UpdateBuddyField(CPlayer* pPlayer);                // @080b6312
    int  UpdateBlackListField(CPlayer* pPlayer);            // @080b63c8
    int  UpdateLevelField(CPlayer* pPlayer);                // @080b6836
    int  UpdateMoneyField(CPlayer* pPlayer);               // @080b689e
    int  UpdateStateField(CPlayer* pPlayer);                // @080b68f6
    int  UpdateSkinField(CPlayer* pPlayer);                 // @080b6934
    int  UpdateNameField(CPlayer* pPlayer, char* sName);    // @080b69b6
    int  UpdateMentField(CPlayer* pPlayer, char* sMent);    // @080b6a0a
    int  UpdateTodayField(CPlayer* pPlayer);                // @080b6a5e
    int  UpdateShutupField(CPlayer* pPlayer, int nState);   // @080b6aae
    int  UpdatePlayerField(CPlayer* pPlayer);               // @080b7b92
    int  UpdateSkinHairField(CPlayer* pPlayer);             // @080b7b9c
    int  UpdateQuest(CPlayer* pPlayer, char nQuestState);   // @080b7198

    // --- inserts ---
    int  InsertCardField(CPlayer* pPlayer, CCard* pCard);              // @080b6e10
    int  InsertItemField(CPlayer* pPlayer, CItem* pItem);             // @080b6cae
    int  InsertItemField(int nMemberSeq, int nPlayerSeq, CItem* pItem);// @080b6d62
    int  InsertMixItem(CPlayer* pPlayer, CCSMixItem1* pMix, CItemMixTable* pTable); // @080b6eaa
    int  InsertSkillField(CPlayer* pPlayer, CSkill* pSkill);          // @080b70da
    int  InsertGiftField(int nPlayerSeq, int nGifterSeq, int nItemSeq, char* sMsg); // @080b7144
    int  InsertTrainingField(CPlayer* pPlayer, CTraining* pTraining); // @080b748e
    int  InsertCeremonyField(CPlayer* pPlayer, CCeremony* pCeremony); // @080b74f2
    int  InsertSaleLog(CPlayer* pPlayer, CSale* pSale);              // @080b754e
    int  InsertConnectLog(int nConnect, int nRelay, int nRoom);     // @080b76b6
    int  InsertPlayLog(CPlayer* pPlayer);                            // @080b7eb2
    int  InsertHackUser(CPlayer* pPlayer, int nCode, int nArg);     // @080b81c0
    int  PostItem(CPlayer* pPlayer, int nArg1, int nArg2);          // @080b377a

    // --- session / server bookkeeping ---
    int  SetLoginData(int nMemberSeq);                     // @080b798a
    int  SetLogOutData(int nMemberSeq);                    // @080b79c4
    int  SetExecuteQuest(int nMemberSeq, int nQuest);      // @080b8204

    // --- shard helper (log/sample DB) ---
    int  CheckSampleTable(const char* strTable, int nArg1, int nArg2);    // @080b7be6
};

#endif // _GAME_SQL_H_
