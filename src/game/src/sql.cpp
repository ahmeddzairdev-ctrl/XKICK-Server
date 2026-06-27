// sql.cpp - CSql implementation (reconstructed from XKICK_Game1, Ghidra).
//
// CSql derives from CDatabase and owns every character/game DB access the Game
// server performs: the full character load chain, the matching persist
// (Update*/Insert*) methods, gift/sale/shop helpers, the log shards, and the
// server/session bookkeeping. Method bodies, SQL text, and return codes are
// faithful to the unstripped binary (addresses noted in Sql.h).
//
// Conventions (identical to the Certify CSql layer):
//   * Query(fmt, ...)   -> MYSQL_RES* for SELECTs; caller frees with FREE_RESULT.
//   * Execute(fmt, ...) -> MYSQL* (non-NULL on success) for DML.
//   * FetchRow(res, &row) fills MY_ROW (std::map<string,char*>); row[col] is the
//     raw column text, converted with atoi()/ATOI().
//   * Three logical DBs: 0 = main (kicks2), 1 = log (kicks2_log), 2 = sample.
//     SetBaseDB() switches which one Query/Execute target; log inserts flip to
//     DB 1 and restore DB 0.
//
// Return-value convention (recovered): 0 / >=0 on success; negative codes carry
// specific failure meanings (-1 query error, -2/-3 no row/alloc, -4 mismatch,
// -5 disallowed, ...) - preserved exactly from the binary.
//
// Portable: only standard C++ + the CDatabase API + Main.h. No OS headers.
#include "Main.h"
#include "Sql.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Game domain record classes (CSkill/CTraining/CCeremony/CCard/CQuestInfo) now
// live in Domain.h (pulled in via Main.h) and are reused here -- the former
// local placeholder definitions were removed to avoid redefinition.
// ---------------------------------------------------------------------------

// CItem here is the Game-extended inventory record (0x50 bytes). Offsets pinned
// from GetItemField / InsertItemField store targets:
//   +0x00 seq, +0x04 code, +0x08 equip(char), +0x09 limit(char), +0x0a grade(char),
//   +0x0c amount(short), +0x10 price, +0x14..+0x24 option0..4, +0x28 state(char),
//   +0x2c random, +0x30 order(char).
struct CGameItem
{
    int  m_nItemSeq;        // +0x00
    int  m_nCode;           // +0x04
    char m_nEquipKind;      // +0x08
    char m_nLimit;          // +0x09
    char m_nGrade;          // +0x0a
    char m_pad0b;           // +0x0b
    short m_nAmount;        // +0x0c
    char m_pad0e[2];        // +0x0e
    int  m_nPrice;          // +0x10
    int  m_nOptionCode[5];  // +0x14
    char m_nState;          // +0x28
    char m_pad29[3];        // +0x29
    int  m_nRandom;         // +0x2c
    char m_nOrder;          // +0x30
    // ... tail working fields (not persisted)
};

// External helpers (util.cpp / load.cpp).
extern int  GetItemSkin(void* pWearBlock);   // resolve skin item-code from equipped wear

// ---------------------------------------------------------------------------
// Construction (empty bodies; CDatabase ctor/dtor do the work).
// ---------------------------------------------------------------------------
CSql::CSql()  : CDatabase() {}
CSql::~CSql() {}

// ===========================================================================
//  STARTUP
// ===========================================================================

// ---------------------------------------------------------------------------
// InitServer - register the three DBs, read this server's params from
// tb_game_server, fetch the master (code 1) relay server address, seed the
// default CSetting from the tb_game_setting schema (DESC), then clear stale
// logins. Mirrors Certify's InitServer.
// ---------------------------------------------------------------------------
bool CSql::InitServer()
{
    SetConnectInfo(g_Config.m_sDBIP, g_Config.m_sDBUser, g_Config.m_sDBPass, g_Config.m_sCharset);
    AddDB(MAINDBFLAG,   g_Config.m_sDBMain);    // 0 -> kicks2
    AddDB(LOGDBFLAG,    g_Config.m_sDBLog);     // 1 -> kicks2_log
    AddDB(SAMPLEDBFLAG, g_Config.m_sDBSample);  // 2 -> sample
    SetBaseDB(MAINDBFLAG);

    MY_ROW row;

    // This server's matchmaking/level/port params.
    MYSQL_RES* res = Query(
        "SELECT server_channel, server_match, server_job, server_free, server_slevel, "
        "server_elevel, server_max, server_port FROM tb_game_server WHERE server_code = %d",
        (int)g_Config.m_nServerCode);
    if (res == NULL) return false;
    if (!FetchRow(res, &row)) { FREE_RESULT(res); return false; }

    // Faithful to the binary (CSql::InitServer @080a4818):
    //   TCP (off 40) = own server_port; UDP (off 42) = own server_port - 1000.
    //   The STS dial port (off 44) is NOT derived from our own port - it comes from
    //   the MASTER row (server_code=1) below as master_server_port + 1000.
    g_Config.m_nTCPPort   = (short)atoi(row["server_port"]);
    g_Config.m_nUDPPort   = g_Config.m_nTCPPort - 1000;
    g_Config.m_nConnector = (short)atoi(row["server_max"]);
    FREE_RESULT(res);
    row.clear();

    // Master/relay server (code 1): the Game dials this for its STS uplink.
    // m_sMasterIP = server_inip; STS dial port = master server_port + 1000
    // (so a master/Certify on server_port 13301 -> STS 14301). Matches the binary.
    res = Query("SELECT server_inip, server_exip, server_port from tb_game_server WHERE server_code = 1");
    if (res == NULL) return false;
    if (FetchRow(res, &row))
    {
        snprintf(g_Config.m_sMasterIP, sizeof(g_Config.m_sMasterIP), "%s",
                 row["server_inip"] ? row["server_inip"] : "127.0.0.1");
        g_Config.m_nSTSPort = (short)atoi(row["server_port"]) + 1000;
    }
    FREE_RESULT(res);
    row.clear();

    // Seed g_Setting defaults from the tb_game_setting column schema.
    res = Query("DESC tb_game_setting");
    if (res == NULL) return false;

    while (FetchRow(res, &row))
    {
        const char* sField = row["Field"];
        const char* sDef   = row["Default"];

        if      (strncmp("setting_camera",     sField, 0xf)  == 0) g_Setting.m_nCameraType   = (char)ATOI(sDef);
        else if (strncmp("setting_zoom",       sField, 0xd)  == 0) g_Setting.m_nCameraZoom   = (char)ATOI(sDef);
        else if (strncmp("setting_target",     sField, 0xf)  == 0) g_Setting.m_nCameraTarget = (char)ATOI(sDef);
        else if (strncmp("setting_camerateam", sField, 0x13) == 0) g_Setting.m_nCameraTeam   = (char)ATOI(sDef);
        else if (strncmp("setting_radian",     sField, 0xf)  == 0) g_Setting.m_nRadian       = (char)ATOI(sDef);
        else if (strncmp("setting_shadow",     sField, 0xf)  == 0) g_Setting.m_nShadow       = (char)ATOI(sDef);
        else if (strncmp("setting_label",      sField, 0xe)  == 0) g_Setting.m_nLabel        = (char)ATOI(sDef);
        else if (strncmp("setting_sound",      sField, 0xe)  == 0) g_Setting.m_nSound        = (char)ATOI(sDef);
        else if (strncmp("setting_music",      sField, 0xe)  == 0) g_Setting.m_nMusic        = (char)ATOI(sDef);
        else if (strncmp("setting_invite",     sField, 0xf)  == 0) g_Setting.m_nInvite       = (char)ATOI(sDef);
        else if (strncmp("setting_whisper",    sField, 0x10) == 0) g_Setting.m_nWhisper      = (char)ATOI(sDef);
        else if (strncmp("setting_friend",     sField, 0xf)  == 0) g_Setting.m_nFriend       = (char)ATOI(sDef);
        else if (strncmp("setting_up",         sField, 0xb)  == 0) g_Setting.m_nDefineKey[0]  = (short)ATOI(sDef);
        else if (strncmp("setting_down",       sField, 0xd)  == 0) g_Setting.m_nDefineKey[1]  = (short)ATOI(sDef);
        else if (strncmp("setting_left",       sField, 0xd)  == 0) g_Setting.m_nDefineKey[2]  = (short)ATOI(sDef);
        else if (strncmp("setting_right",      sField, 0xe)  == 0) g_Setting.m_nDefineKey[3]  = (short)ATOI(sDef);
        else if (strncmp("setting_leftshoot",  sField, 0x12) == 0) g_Setting.m_nDefineKey[4]  = (short)ATOI(sDef);
        else if (strncmp("setting_rightshoot", sField, 0x13) == 0) g_Setting.m_nDefineKey[5]  = (short)ATOI(sDef);
        else if (strncmp("setting_longpass",   sField, 0x11) == 0) g_Setting.m_nDefineKey[6]  = (short)ATOI(sDef);
        else if (strncmp("setting_shortpass",  sField, 0x12) == 0) g_Setting.m_nDefineKey[7]  = (short)ATOI(sDef);
        else if (strncmp("setting_screen",     sField, 0xf)  == 0) g_Setting.m_nDefineKey[8]  = (short)ATOI(sDef);
        else if (strncmp("setting_tackle",     sField, 0xf)  == 0) g_Setting.m_nDefineKey[9]  = (short)ATOI(sDef);
        else if (strncmp("setting_steal",      sField, 0xe)  == 0) g_Setting.m_nDefineKey[10] = (short)ATOI(sDef);
        else if (strncmp("setting_quick",      sField, 0xe)  == 0) g_Setting.m_nDefineKey[11] = (short)ATOI(sDef);
        else if (strncmp("setting_quick2",     sField, 0xf)  == 0) g_Setting.m_nDefineKey[12] = (short)ATOI(sDef);
        else if (strncmp("setting_skill1",     sField, 0xf)  == 0) g_Setting.m_nDefineKey[13] = (short)ATOI(sDef);
        else if (strncmp("setting_skill2",     sField, 0xf)  == 0) g_Setting.m_nDefineKey[14] = (short)ATOI(sDef);
        else if (strncmp("setting_skill3",     sField, 0xf)  == 0) g_Setting.m_nDefineKey[15] = (short)ATOI(sDef);
        else if (strncmp("setting_skill4",     sField, 0xf)  == 0) g_Setting.m_nDefineKey[16] = (short)ATOI(sDef);
        else if (strncmp("setting_skill5",     sField, 0xf)  == 0) g_Setting.m_nDefineKey[17] = (short)ATOI(sDef);
        else if (strncmp("setting_skill_attack1",  sField, 0x16) == 0) g_Setting.m_nAttackSkillCode[0]  = ATOI(sDef);
        else if (strncmp("setting_skill_attack2",  sField, 0x16) == 0) g_Setting.m_nAttackSkillCode[1]  = ATOI(sDef);
        else if (strncmp("setting_skill_attack3",  sField, 0x16) == 0) g_Setting.m_nAttackSkillCode[2]  = ATOI(sDef);
        else if (strncmp("setting_skill_attack4",  sField, 0x16) == 0) g_Setting.m_nAttackSkillCode[3]  = ATOI(sDef);
        else if (strncmp("setting_skill_attack5",  sField, 0x16) == 0) g_Setting.m_nAttackSkillCode[4]  = ATOI(sDef);
        else if (strncmp("setting_skill_defence1", sField, 0x17) == 0) g_Setting.m_nDefenceSkillCode[0] = ATOI(sDef);
        else if (strncmp("setting_skill_defence2", sField, 0x17) == 0) g_Setting.m_nDefenceSkillCode[1] = ATOI(sDef);
        else if (strncmp("setting_skill_defence3", sField, 0x17) == 0) g_Setting.m_nDefenceSkillCode[2] = ATOI(sDef);
        else if (strncmp("setting_skill_defence4", sField, 0x17) == 0) g_Setting.m_nDefenceSkillCode[3] = ATOI(sDef);
        else if (strncmp("setting_skill_defence5", sField, 0x17) == 0) g_Setting.m_nDefenceSkillCode[4] = ATOI(sDef);
    }
    FREE_RESULT(res);
    row.clear();

    InitMemberLogin();
    return true;
}

// ---------------------------------------------------------------------------
// InitMemberLogin - bulk logout of all accounts tagged to this server.
// ---------------------------------------------------------------------------
int CSql::InitMemberLogin()
{
    if (Execute("UPDATE tb_game_trio SET trio_server=0 WHERE trio_server=%d;",
                (int)g_Config.m_nServerCode) == NULL)
        return -1;
    return 0;
}

// ===========================================================================
//  CHARACTER LOAD CHAIN
// ===========================================================================

// ---------------------------------------------------------------------------
// GetPlayerFields - the load-everything wrapper. Each step's failure carries a
// distinct descending reason code (-1 .. -11). Buddy/blacklist are NOT here.
// ---------------------------------------------------------------------------
int CSql::GetPlayerFields(CPlayer* pPlayer)
{
    if (GetMemberField(pPlayer)   < 0) return -1;
    if (GetPlayerField(pPlayer)   < 0) return -2;
    if (GetFacultyField(pPlayer)  < 0) return -3;
    if (GetRecordField(pPlayer)   < 0) return -4;
    if (GetRankField(pPlayer)     < 0) return -5;
    if (GetItemField(pPlayer)     < 0) return -6;
    if (GetSkillField(pPlayer)    < 0) return -7;
    if (GetTrainingField(pPlayer) < 0) return -8;
    if (GetCeremonyField(pPlayer) < 0) return -9;
    if (GetQuestField(pPlayer)    < 0) return -10;
    if (GetCardField(pPlayer)     < 0) return -11;
    return 0;
}

// ---------------------------------------------------------------------------
// GetMemberField - account-wide wallet/flags from tb_game_trio.
// ---------------------------------------------------------------------------
int CSql::GetMemberField(CPlayer* pPlayer)
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT * from tb_game_trio WHERE member_seq = %d", pPlayer->m_nMemberSeq);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return -2; }

    pPlayer->m_cMoney.m_nCash   = ATOI(row["trio_cash"]);
    pPlayer->m_cMoney.m_nPoint  = ATOI(row["trio_point"]);
    pPlayer->m_cMoney.m_nCredit = ATOI(row["trio_credit"]);
    pPlayer->m_nCount       = (char)ATOI(row["trio_count"]);
    pPlayer->m_nGhostHost        = (char)ATOI(row["trio_host"]);

    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetPlayerField - the base character row from tb_game_player plus the per-
// character CSetting (tb_game_setting). Also: reroll today's shop when the
// stored rand/shopstate no longer matches today, force operation=3 for hosts,
// and stamp the login time.
// ---------------------------------------------------------------------------
int CSql::GetPlayerField(CPlayer* pPlayer)
{
    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT player_name, player_position, player_condition, player_alias, player_level, "
        "player_exp, player_faculty, player_skill, player_gender, player_skin, player_uniform, "
        "player_face, player_hair, player_shirts, player_pants, player_shoes, player_operation, "
        "player_ment, player_rand, player_shopstate, player_cardentry, "
        "DAY(player_logindate) as login_date, DAY(NOW()) as today, "
        "UNIX_TIMESTAMP(player_shutupdate) as shutup_date "
        "FROM tb_game_player WHERE player_seq = %d",
        pPlayer->m_nPlayerSeq);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return -2; }

    snprintf(pPlayer->m_sName, 0xf, "%s", row["player_name"]);
    snprintf(pPlayer->m_sMent, 0x2d, "%s", row["player_ment"]);
    pPlayer->m_nPosition   = (char)ATOI(row["player_position"]);
    pPlayer->m_nCondition  = (char)ATOI(row["player_condition"]);
    pPlayer->m_nAlias      = (char)ATOI(row["player_alias"]);
    pPlayer->m_cLevel.m_nLevel      = (char)ATOI(row["player_level"]);
    pPlayer->m_cLevel.m_nExp        = ATOI(row["player_exp"]);
    pPlayer->m_cLevel.m_nFaculty = (short)ATOI(row["player_faculty"]);
    pPlayer->m_cLevel.m_nSkill   = (short)ATOI(row["player_skill"]);
    pPlayer->m_cShape.m_nGender     = (char)ATOI(row["player_gender"]);
    pPlayer->m_cShape.m_nSkin       = (char)ATOI(row["player_skin"]);
    pPlayer->m_cShape.m_nUniform    = (char)ATOI(row["player_uniform"]);
    pPlayer->m_nOperation  = (char)ATOI(row["player_operation"]);
    pPlayer->m_nCardEntry  = ATOI(row["player_cardentry"]);    // +0x514 (binary 0x80aa98c)
    pPlayer->m_nShutupDate = ATOI(row["shutup_date"]);         // binary 0x80aaa41
    pPlayer->m_nTodaySeed  = ATOI(row["player_rand"]);         // binary 0x80aa76d
    pPlayer->m_nTodayBit   = ATOI(row["player_shopstate"]);    // binary 0x80aa822

    // Appearance ints (m_reserved434 region: face/hair/shirts/pants/shoes).
    pPlayer->m_nFreeWear[0] = ATOI(row["player_face"]);
    pPlayer->m_nFreeWear[1] = ATOI(row["player_hair"]);
    pPlayer->m_nFreeWear[2] = ATOI(row["player_shirts"]);
    pPlayer->m_nFreeWear[3] = ATOI(row["player_pants"]);
    pPlayer->m_nFreeWear[4] = ATOI(row["player_shoes"]);

    int nLoginDay = ATOI(row["login_date"]);   // binary 0x80aac4f
    int nToday    = ATOI(row["today"]);         // binary 0x80aacf2

    FREE_RESULT(res);
    row.clear();

    // Host accounts are forced into operation mode 3.
    if (pPlayer->m_nGhostHost > 0) pPlayer->m_nOperation = 3;

    // Login day differs from today -> reroll the daily random shop.
    if (nLoginDay != nToday) pPlayer->ChangeTodayShop();

    // Per-character control/skill setting.
    res = Query("SELECT * from tb_game_setting WHERE player_seq = %d", pPlayer->m_nPlayerSeq);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return -2; }

    pPlayer->m_cSetting.m_nCameraType   = (char)ATOI(row["setting_camera"]);
    pPlayer->m_cSetting.m_nCameraZoom   = (char)ATOI(row["setting_zoom"]);
    pPlayer->m_cSetting.m_nCameraTarget = (char)ATOI(row["setting_target"]);
    pPlayer->m_cSetting.m_nCameraTeam   = (char)ATOI(row["setting_camerateam"]);
    pPlayer->m_cSetting.m_nRadian       = (char)ATOI(row["setting_radian"]);
    pPlayer->m_cSetting.m_nShadow       = (char)ATOI(row["setting_shadow"]);
    pPlayer->m_cSetting.m_nLabel        = (char)ATOI(row["setting_label"]);
    pPlayer->m_cSetting.m_nSound        = (char)ATOI(row["setting_sound"]);
    pPlayer->m_cSetting.m_nMusic        = (char)ATOI(row["setting_music"]);
    pPlayer->m_cSetting.m_nInvite       = (char)ATOI(row["setting_invite"]);
    pPlayer->m_cSetting.m_nWhisper      = (char)ATOI(row["setting_whisper"]);
    pPlayer->m_cSetting.m_nFriend       = (char)ATOI(row["setting_friend"]);
    pPlayer->m_cSetting.m_nDefineKey[0]  = (short)ATOI(row["setting_up"]);
    pPlayer->m_cSetting.m_nDefineKey[1]  = (short)ATOI(row["setting_down"]);
    pPlayer->m_cSetting.m_nDefineKey[2]  = (short)ATOI(row["setting_left"]);
    pPlayer->m_cSetting.m_nDefineKey[3]  = (short)ATOI(row["setting_right"]);
    pPlayer->m_cSetting.m_nDefineKey[4]  = (short)ATOI(row["setting_leftshoot"]);
    pPlayer->m_cSetting.m_nDefineKey[5]  = (short)ATOI(row["setting_rightshoot"]);
    pPlayer->m_cSetting.m_nDefineKey[6]  = (short)ATOI(row["setting_longpass"]);
    pPlayer->m_cSetting.m_nDefineKey[7]  = (short)ATOI(row["setting_shortpass"]);
    pPlayer->m_cSetting.m_nDefineKey[8]  = (short)ATOI(row["setting_screen"]);
    pPlayer->m_cSetting.m_nDefineKey[9]  = (short)ATOI(row["setting_tackle"]);
    pPlayer->m_cSetting.m_nDefineKey[10] = (short)ATOI(row["setting_steal"]);
    pPlayer->m_cSetting.m_nDefineKey[11] = (short)ATOI(row["setting_quick"]);
    pPlayer->m_cSetting.m_nDefineKey[12] = (short)ATOI(row["setting_quick2"]);
    pPlayer->m_cSetting.m_nDefineKey[13] = (short)ATOI(row["setting_skill1"]);
    pPlayer->m_cSetting.m_nDefineKey[14] = (short)ATOI(row["setting_skill2"]);
    pPlayer->m_cSetting.m_nDefineKey[15] = (short)ATOI(row["setting_skill3"]);
    pPlayer->m_cSetting.m_nDefineKey[16] = (short)ATOI(row["setting_skill4"]);
    pPlayer->m_cSetting.m_nDefineKey[17] = (short)ATOI(row["setting_skill5"]);
    pPlayer->m_cSetting.m_nAttackSkillCode[0]  = ATOI(row["setting_skill_attack1"]);
    pPlayer->m_cSetting.m_nAttackSkillCode[1]  = ATOI(row["setting_skill_attack2"]);
    pPlayer->m_cSetting.m_nAttackSkillCode[2]  = ATOI(row["setting_skill_attack3"]);
    pPlayer->m_cSetting.m_nAttackSkillCode[3]  = ATOI(row["setting_skill_attack4"]);
    pPlayer->m_cSetting.m_nAttackSkillCode[4]  = ATOI(row["setting_skill_attack5"]);
    pPlayer->m_cSetting.m_nDefenceSkillCode[0] = ATOI(row["setting_skill_defence1"]);
    pPlayer->m_cSetting.m_nDefenceSkillCode[1] = ATOI(row["setting_skill_defence2"]);
    pPlayer->m_cSetting.m_nDefenceSkillCode[2] = ATOI(row["setting_skill_defence3"]);
    pPlayer->m_cSetting.m_nDefenceSkillCode[3] = ATOI(row["setting_skill_defence4"]);
    pPlayer->m_cSetting.m_nDefenceSkillCode[4] = ATOI(row["setting_skill_defence5"]);

    FREE_RESULT(res);
    row.clear();

    // Stamp the login time.
    Execute("UPDATE tb_game_player SET player_logindate=now() WHERE player_seq=%d",
            pPlayer->m_nPlayerSeq);
    return 0;
}

// ---------------------------------------------------------------------------
// GetFacultyField - the 25-byte current-faculty block (tb_game_faculty), read
// positionally into m_cRaiseFaculty.
// ---------------------------------------------------------------------------
int CSql::GetFacultyField(CPlayer* pPlayer)
{
    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT faculty_run, faculty_dribble, faculty_quick, faculty_stamina, faculty_elasticity, "
        "faculty_bodycheck, faculty_cross, faculty_shortpass, faculty_longpass, faculty_headshoot, "
        "faculty_shortshoot, faculty_longshoot, faculty_keeping, faculty_steal, faculty_tackle, "
        "faculty_catch, faculty_punch, faculty_guard, faculty_move, faculty_body, faculty_pass, "
        "faculty_shoot, faculty_defense, faculty_goalkeep, faculty_talent "
        "FROM tb_game_faculty WHERE player_seq = %d",
        pPlayer->m_nPlayerSeq);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return -3; }

    static const char* sCols[25] = {
        "faculty_run","faculty_dribble","faculty_quick","faculty_stamina","faculty_elasticity",
        "faculty_bodycheck","faculty_cross","faculty_shortpass","faculty_longpass","faculty_headshoot",
        "faculty_shortshoot","faculty_longshoot","faculty_keeping","faculty_steal","faculty_tackle",
        "faculty_catch","faculty_punch","faculty_guard","faculty_move","faculty_body",
        "faculty_pass","faculty_shoot","faculty_defense","faculty_goalkeep","faculty_talent" };
    for (int i = 0; i < 25; ++i)
        pPlayer->m_cRaiseFaculty.m_nFaculty[i] = (char)ATOI(row[sCols[i]]);

    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetRecordField - lifetime record (tb_game_record) into m_cTotalRecord.m_nRecord, then
// the quarterly shard into m_cQuarterRecord.m_nRecord. A missing row is tolerated (0).
// ---------------------------------------------------------------------------
int CSql::GetRecordField(CPlayer* pPlayer)
{
    static const char* sCols[19] = {
        "record_match","record_win","record_draw","record_mark","record_mvp","record_goal",
        "record_allow","record_assist","record_cut","record_shoot","record_steal","record_tackle",
        "record_pass","record_guard","record_tryshoot","record_trysteal","record_trytackle",
        "record_trypass","record_tryguard" };

    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT record_match, record_win, record_draw, record_mark, record_mvp, record_goal, "
        "record_allow, record_assist, record_cut, record_shoot, record_steal, record_tackle, "
        "record_pass, record_guard, record_tryshoot, record_trysteal, record_trytackle, "
        "record_trypass, record_tryguard FROM tb_game_record WHERE player_seq = %d",
        pPlayer->m_nPlayerSeq);
    if (res == NULL) return -1;
    if (FetchRow(res, &row))
        for (int i = 0; i < 19; ++i) pPlayer->m_cTotalRecord.m_nRecord[i] = ATOI(row[sCols[i]]);
    FREE_RESULT(res);
    row.clear();

    // Quarterly shard -> today/period record.
    CTime cTime;
    GetDBTime(&cTime);
    CheckSampleTable("tb_game_record", cTime.m_nYear - 2000, (int)cTime.m_nQuarter);

    res = Query(
        "SELECT record_match, record_win, record_draw, record_mark, record_mvp, record_goal, "
        "record_allow, record_assist, record_cut, record_shoot, record_steal, record_tackle, "
        "record_pass, record_guard, record_tryshoot, record_trysteal, record_trytackle, "
        "record_trypass, record_tryguard FROM tb_game_record_%02d_%02d WHERE player_seq = %d",
        cTime.m_nYear - 2000, (int)cTime.m_nQuarter, pPlayer->m_nPlayerSeq);
    if (res == NULL) return -1;
    if (FetchRow(res, &row))
        for (int i = 0; i < 19; ++i) pPlayer->m_cQuarterRecord.m_nRecord[i] = ATOI(row[sCols[i]]);
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetRecordField_Weekly - the last 7 days of daily records (tb_game_record_week)
// indexed by day, into a CSCWeeklyRecord packet. Verified vs XKICK_Game1 @080acde6.
// ---------------------------------------------------------------------------
int CSql::GetRecordField_Weekly(int nPlayerSeq, CSCWeeklyRecord* pPacket)
{
    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT (TO_DAYS(NOW())-TO_DAYS(record_date)-1) as DATE_INDEX, record_date, record_match, "
        "record_win, record_draw, record_mark FROM tb_game_record_week WHERE player_seq = %d AND "
        "0 < (TO_DAYS(NOW()) - TO_DAYS(record_date)) AND (TO_DAYS(NOW()) - TO_DAYS(record_date)) <= 7;",
        nPlayerSeq);
    if (res == NULL) return -1;

    pPacket->m_nPlayerSeq = nPlayerSeq;
    for (int i = 0; i < 8; ++i) { pPacket->m_nRecordA[i] = 0; pPacket->m_nRecordB[i] = 0; }
    while (FetchRow(res, &row))
    {
        int idx = ATOI(row["DATE_INDEX"]);
        if (idx < 0 || idx > 7) continue;           // query bounds it to 0..6; guard anyway
        pPacket->m_nRecordA[idx] = ATOI(row["record_match"]);   // weekly match record
        pPacket->m_nRecordB[idx] = ATOI(row["record_mark"]);    // weekly mark record
    }
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetRankingField_Weekly - weekly ranking sample into a CSCWeeklyRanking packet.
// Columns ranking_num / ranking_mark verified against XKICK_Game1 @080ad2d8.
// ---------------------------------------------------------------------------
int CSql::GetRankingField_Weekly(int nPlayerSeq, CSCWeeklyRanking* pPacket)
{
    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT (TO_DAYS(NOW())-TO_DAYS(ranking_date)-1) as DATE_INDEX, ranking_date, ranking_num, ranking_mark "
        "FROM tb_game_ranking_week WHERE player_seq = %d AND "
        "0 < (TO_DAYS(NOW()) - TO_DAYS(ranking_date)) AND (TO_DAYS(NOW()) - TO_DAYS(ranking_date)) <= 7;",
        nPlayerSeq);
    if (res == NULL) return -1;

    pPacket->m_nPlayerSeq = nPlayerSeq;
    for (int i = 0; i < 8; ++i) { pPacket->m_nRankingA[i] = 0; pPacket->m_nRankingB[i] = 0; }
    while (FetchRow(res, &row))
    {
        int idx = ATOI(row["DATE_INDEX"]);
        if (idx < 0 || idx > 7) continue;
        pPacket->m_nRankingA[idx] = ATOI(row["ranking_num"]);
        pPacket->m_nRankingB[idx] = ATOI(row["ranking_mark"]);
    }
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetRankField - rank codes (tb_game_rank) into ranking block A, then the
// quarterly shard into block B. Each row maps a rank_code to a short slot.
// ---------------------------------------------------------------------------
static void MapRankCode(short* pBase, int nCode, short nNum)
{
    // Three contiguous code ranges map to consecutive short slots.
    int nIndex = -1;
    if      (nCode >= 0x65 && nCode <= 0x70) nIndex = nCode - 0x65;        //  0..11
    else if (nCode >= 0xc9 && nCode <= 0xd1) nIndex = 12 + (nCode - 0xc9); // 12..20
    else if (nCode >= 0x12d && nCode <= 0x131) nIndex = 21 + (nCode - 0x12d);// 21..25
    if (nIndex >= 0 && nIndex < 25) pBase[nIndex] = nNum;
}

int CSql::GetRankField(CPlayer* pPlayer)
{
    for (int i = 0; i < 25; ++i) { pPlayer->m_cTotalRanking[i] = 0; pPlayer->m_cQuarterRanking[i] = 0; }

    MY_ROW row;
    MYSQL_RES* res = Query("SELECT rank_num, rank_code from tb_game_rank where player_seq = %d",
                           pPlayer->m_nPlayerSeq);
    if (res == NULL) return -1;
    while (FetchRow(res, &row))
        MapRankCode(pPlayer->m_cTotalRanking, atol(row["rank_code"]), (short)atol(row["rank_num"]));
    FREE_RESULT(res);
    row.clear();

    CTime cTime;
    GetDBTime(&cTime);
    res = Query("SELECT rank_num, rank_code from tb_game_rank_%02d_%02d where player_seq = %d",
                cTime.m_nYear - 2000, (int)cTime.m_nQuarter, pPlayer->m_nPlayerSeq);
    if (res != NULL)
    {
        while (FetchRow(res, &row))
            MapRankCode(pPlayer->m_cQuarterRanking, atol(row["rank_code"]), (short)atol(row["rank_num"]));
        FREE_RESULT(res);
        row.clear();
    }
    return 0;
}

// ---------------------------------------------------------------------------
// GetItemField - inventory (tb_game_item) into m_vItemList.
// ---------------------------------------------------------------------------
int CSql::GetItemField(CPlayer* pPlayer)
{
    if (!pPlayer->m_vItemList.empty()) return 0;

    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT item_seq, item_code, item_equip, item_amount, item_grade, item_limit, item_price, "
        "item_option0, item_option1, item_option2, item_option3, item_option4, "
        "item_random, item_order "
        "FROM tb_game_item WHERE player_seq = %d and item_state = '1' order by item_seq",
        pPlayer->m_nPlayerSeq);
    if (res == NULL) return -1;

    while (FetchRow(res, &row))
    {
        CGameItem* pItem = new CGameItem;
        if (pItem == NULL) { FREE_RESULT(res); row.clear(); return -3; }
        pItem->m_nState        = 1;
        pItem->m_nItemSeq      = ATOI(row["item_seq"]);
        pItem->m_nCode         = ATOI(row["item_code"]);
        pItem->m_nEquipKind    = (char)ATOI(row["item_equip"]);
        pItem->m_nAmount       = (short)ATOI(row["item_amount"]);
        pItem->m_nGrade        = (char)ATOI(row["item_grade"]);
        pItem->m_nLimit        = (char)ATOI(row["item_limit"]);
        pItem->m_nPrice        = ATOI(row["item_price"]);
        pItem->m_nOptionCode[0]= ATOI(row["item_option0"]);
        pItem->m_nOptionCode[1]= ATOI(row["item_option1"]);
        pItem->m_nOptionCode[2]= ATOI(row["item_option2"]);
        pItem->m_nOptionCode[3]= ATOI(row["item_option3"]);
        pItem->m_nOptionCode[4]= ATOI(row["item_option4"]);
        pItem->m_nRandom       = ATOI(row["item_random"]);
        pItem->m_nOrder        = (char)ATOI(row["item_order"]);
        pPlayer->m_vItemList.push_back((CItem*)pItem);
    }
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetGiftItemField - claim one item-gift: load it into the inventory and mark
// it consumed (item_state='1').
// ---------------------------------------------------------------------------
int CSql::GetGiftItemField(CPlayer* pPlayer, int nGiftSeq)
{
    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT item_seq, item_code, item_equip, item_amount, item_grade, item_limit, item_price, "
        "item_option0, item_option1, item_option2, item_option3, item_option4, "
        "item_random, item_order "
        "FROM tb_game_item WHERE player_seq = %d and item_seq = %d and item_state = '3'",
        pPlayer->m_nPlayerSeq, nGiftSeq);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return -2; }

    CGameItem* pItem = new CGameItem;
    if (pItem == NULL) { FREE_RESULT(res); row.clear(); return -3; }
    pItem->m_nState        = 1;
    pItem->m_nItemSeq      = ATOI(row["item_seq"]);
    pItem->m_nCode         = ATOI(row["item_code"]);
    pItem->m_nEquipKind    = (char)ATOI(row["item_equip"]);
    pItem->m_nAmount       = (short)ATOI(row["item_amount"]);
    pItem->m_nGrade        = (char)ATOI(row["item_grade"]);
    pItem->m_nLimit        = (char)ATOI(row["item_limit"]);
    pItem->m_nPrice        = ATOI(row["item_price"]);
    pItem->m_nOptionCode[0]= ATOI(row["item_option0"]);
    pItem->m_nOptionCode[1]= ATOI(row["item_option1"]);
    pItem->m_nOptionCode[2]= ATOI(row["item_option2"]);
    pItem->m_nOptionCode[3]= ATOI(row["item_option3"]);
    pItem->m_nOptionCode[4]= ATOI(row["item_option4"]);
    pItem->m_nRandom       = ATOI(row["item_random"]);
    pItem->m_nOrder        = (char)ATOI(row["item_order"]);
    pPlayer->m_vItemList.push_back((CItem*)pItem);

    FREE_RESULT(res);
    row.clear();

    // Claim the gift item (state 3 -> 1).
    if (Execute("update tb_game_item set item_state = '1' where item_seq = %d", nGiftSeq) == NULL)
        return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// GetCardField - the player cards (tb_game_card) into m_vCardList.
// ---------------------------------------------------------------------------
int CSql::GetCardField(CPlayer* pPlayer)
{
    if (!pPlayer->m_vCardList.empty()) return 0;

    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT card_seq, player_seq, member_seq, card_code, card_equip1, card_equip2, card_equip3, "
        "card_level, card_rank, card_position, card_area, card_skill, card_costume, card_tired "
        "FROM tb_game_card WHERE player_seq = %d and card_state = '1' order by card_seq",
        pPlayer->m_nPlayerSeq);
    if (res == NULL) return -1;

    while (FetchRow(res, &row))
    {
        CCard* pCard = new CCard;
        pCard->m_nCardSeq  = ATOI(row["card_seq"]);
        pCard->m_nState    = 1;
        pCard->m_nCode     = (char)ATOI(row["card_code"]);
        pCard->m_nEquip1   = ATOI(row["card_equip1"]);
        pCard->m_nEquip2   = ATOI(row["card_equip2"]);
        pCard->m_nEquip3   = (char)ATOI(row["card_equip3"]);
        pCard->m_nLevel    = (char)ATOI(row["card_level"]);
        pCard->m_nRank     = (char)ATOI(row["card_rank"]);
        pCard->m_nPosition = (char)ATOI(row["card_position"]);
        pCard->m_nArea     = (char)ATOI(row["card_area"]);
        pCard->m_nSkill    = (char)ATOI(row["card_skill"]);
        pCard->m_nCostume  = (char)ATOI(row["card_costume"]);
        pCard->m_nTired    = (char)ATOI(row["card_tired"]);
        pPlayer->m_vCardList.push_back(pCard);
    }
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetSkillField - learned skills (tb_game_skill) into m_vSkillList.
// ---------------------------------------------------------------------------
int CSql::GetSkillField(CPlayer* pPlayer)
{
    if (!pPlayer->m_vSkillList.empty()) return 0;

    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT skill_seq, skill_code, skill_equip, skill_level FROM tb_game_skill "
        "WHERE player_seq = %d and skill_state = '1' order by skill_seq",
        pPlayer->m_nPlayerSeq);
    if (res == NULL) return -1;

    while (FetchRow(res, &row))
    {
        CSkill* pSkill = new CSkill;
        pSkill->m_nState     = 1;
        pSkill->m_nSkillSeq  = ATOI(row["skill_seq"]);
        pSkill->m_nCode      = ATOI(row["skill_code"]);
        pSkill->m_nEquipKind = (char)ATOI(row["skill_equip"]);
        pSkill->m_nLevel     = (char)ATOI(row["skill_level"]);
        pPlayer->m_vSkillList.push_back(pSkill);
    }
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetTrainingField - training progress (tb_game_training) into m_vTrainingList.
// ---------------------------------------------------------------------------
int CSql::GetTrainingField(CPlayer* pPlayer)
{
    if (!pPlayer->m_vTrainingList.empty()) return 0;

    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT training_seq, training_code, training_type, training_level FROM tb_game_training "
        "WHERE player_seq = %d and training_state = '1' order by training_seq",
        pPlayer->m_nPlayerSeq);
    if (res == NULL) return -1;

    while (FetchRow(res, &row))
    {
        CTraining* pTraining = new CTraining;
        pTraining->m_nState       = 1;
        pTraining->m_nTrainingSeq = ATOI(row["training_seq"]);
        pTraining->m_nCode        = ATOI(row["training_code"]);
        pTraining->m_nLevel       = (char)ATOI(row["training_level"]);
        pTraining->m_nType        = (char)ATOI(row["training_type"]);
        pPlayer->m_vTrainingList.push_back(pTraining);
    }
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetCeremonyField - ceremony unlocks (tb_game_ceremony) into m_vCeremonyList.
// ---------------------------------------------------------------------------
int CSql::GetCeremonyField(CPlayer* pPlayer)
{
    if (!pPlayer->m_vCeremonyList.empty()) return 0;

    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT ceremony_seq, ceremony_code, ceremony_equip FROM tb_game_ceremony "
        "WHERE player_seq = %d and ceremony_state = '1' order by ceremony_seq",
        pPlayer->m_nPlayerSeq);
    if (res == NULL) return -1;

    while (FetchRow(res, &row))
    {
        CCeremony* pCeremony = new CCeremony;
        pCeremony->m_nState       = 1;
        pCeremony->m_nCeremonySeq = ATOI(row["ceremony_seq"]);
        pCeremony->m_nCode        = ATOI(row["ceremony_code"]);
        pCeremony->m_nEquipKind   = (char)ATOI(row["ceremony_equip"]);
        pPlayer->m_vCeremonyList.push_back(pCeremony);
    }
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetQuestField - quest progress (tb_game_quest) into m_vQuestList. Two queries:
// account-wide tutorial quests (code < 200, by member_seq) and per-character
// quests (code >= 200, by player_seq).
// ---------------------------------------------------------------------------
int CSql::GetQuestField(CPlayer* pPlayer)
{
    // Clear any previously loaded quests.
    for (size_t i = 0; i < pPlayer->m_vQuestList.size(); ++i) delete pPlayer->m_vQuestList[i];
    pPlayer->m_vQuestList.clear();

    MY_ROW row;
    for (int nPass = 0; nPass < 2; ++nPass)
    {
        MYSQL_RES* res;
        if (nPass == 0)
            res = Query("SELECT * FROM tb_game_quest WHERE member_seq = %d and quest_code < 200 "
                        "order by quest_seq", pPlayer->m_nMemberSeq);
        else
            res = Query("SELECT * FROM tb_game_quest WHERE player_seq = %d and quest_code >= 200 "
                        "order by quest_seq", pPlayer->m_nPlayerSeq);
        if (res == NULL) return -1;

        while (FetchRow(res, &row))
        {
            CQuestInfo* pQuest = new CQuestInfo;
            if (pQuest == NULL) { FREE_RESULT(res); row.clear(); return -3; }
            pQuest->m_nCode  = ATOI(row["quest_code"]);
            pQuest->m_nCount = ATOI(row["quest_count"]);
            pQuest->m_nSeq   = (short)ATOI(row["quest_seq"]);
            pQuest->m_nState = 0;   // Domain.h CQuestInfo: runtime exec/state (was m_nFlag)
            pPlayer->m_vQuestList.push_back(pQuest);
        }
        FREE_RESULT(res);
        row.clear();
    }
    return 0;
}

// ===========================================================================
//  BUDDY / BLACKLIST
// ===========================================================================

// ---------------------------------------------------------------------------
// CheckBuddyField - buddy list joined with the buddy's player row (tb_game_buddy
// x tb_game_player) into m_vBuddyList.
// ---------------------------------------------------------------------------
int CSql::CheckBuddyField(CPlayer* pPlayer)
{
    if (!pPlayer->m_vBuddyList.empty()) return 0;

    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT a.player_seq, a.member_seq, target_seq, player_position, player_level, player_name "
        "FROM tb_game_buddy as a inner join tb_game_player as b on a.target_seq=b.player_seq "
        "WHERE a.player_seq = %d LIMIT %d",
        pPlayer->m_nPlayerSeq, 30);
    if (res == NULL) return -1;

    while (FetchRow(res, &row))
    {
        CBuddyInfo* pBuddy = new CBuddyInfo;
        if (pBuddy == NULL) { FREE_RESULT(res); row.clear(); return -3; }
        pBuddy->m_nPlayerSeq = ATOI(row["target_seq"]);
        pBuddy->m_nLevel     = (short)ATOI(row["player_level"]);
        pBuddy->m_nPosition  = (char)ATOI(row["player_position"]);
        snprintf(pBuddy->m_sName, 0xf, "%s", row["player_name"]);
        pPlayer->m_vBuddyList.push_back(pBuddy);
    }
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// CheckBlacklistField - blacklist joined with the target's player row
// (tb_game_blacklist x tb_game_player) into m_vBlackList.
// ---------------------------------------------------------------------------
int CSql::CheckBlacklistField(CPlayer* pPlayer)
{
    if (!pPlayer->m_vBlackList.empty()) return 0;

    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT a.player_seq, a.member_seq, target_seq, player_position, player_level, player_name "
        "FROM tb_game_blacklist as a inner join tb_game_player as b on a.target_seq=b.player_seq "
        "WHERE a.player_seq = %d LIMIT %d",
        pPlayer->m_nPlayerSeq, 30);
    if (res == NULL) return -1;

    while (FetchRow(res, &row))
    {
        CBlacklistInfo* pBlack = new CBlacklistInfo;
        if (pBlack == NULL) { FREE_RESULT(res); row.clear(); return -3; }
        pBlack->m_nPlayerSeq = ATOI(row["target_seq"]);
        pBlack->m_nLevel     = (char)ATOI(row["player_level"]);
        pBlack->m_nPosition  = (char)ATOI(row["player_position"]);
        snprintf(pBlack->m_sName, 0xf, "%s", row["player_name"]);
        pPlayer->m_vBlackList.push_back(pBlack);
    }
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// AddBuddyField / AddBlackListField - insert a relation row.
// ---------------------------------------------------------------------------
int CSql::AddBuddyField(CPlayer* pPlayer, CPlayer* pTarget)
{
    if (pPlayer == NULL || pTarget == NULL) return -1;
    if (Execute("INSERT INTO tb_game_buddy SET player_seq=%d, member_seq=%d, target_seq=%d;",
                pPlayer->m_nPlayerSeq, pPlayer->m_nMemberSeq, pTarget->m_nPlayerSeq) == NULL)
        return -1;
    return 0;
}

int CSql::AddBlackListField(CPlayer* pPlayer, CPlayer* pTarget)
{
    if (pPlayer == NULL || pTarget == NULL) return -1;
    if (Execute("INSERT INTO tb_game_blacklist SET player_seq=%d, member_seq=%d, target_seq=%d;",
                pPlayer->m_nPlayerSeq, pPlayer->m_nMemberSeq, pTarget->m_nPlayerSeq) == NULL)
        return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// DeleteBuddyField / DeleteBlackListField - remove a relation row.
// ---------------------------------------------------------------------------
int CSql::DeleteBuddyField(CPlayer* pPlayer, int nPlayerSeq)
{
    if (Execute("DELETE FROM tb_game_buddy WHERE player_seq = %d and target_seq = %d",
                pPlayer->m_nPlayerSeq, nPlayerSeq) == NULL)
        return -1;
    return 0;
}

int CSql::DeleteBlackListField(CPlayer* pPlayer, int nPlayerSeq)
{
    if (Execute("DELETE FROM tb_game_blacklist WHERE player_seq = %d and target_seq = %d",
                pPlayer->m_nPlayerSeq, nPlayerSeq) == NULL)
        return -1;
    return 0;
}

// ===========================================================================
//  PER-FIELD PERSIST (Update*)
// ===========================================================================

// ---------------------------------------------------------------------------
// UpdatePlayerFields - persist-everything wrapper. Distinct reason codes on
// first failure.
// ---------------------------------------------------------------------------
int CSql::UpdatePlayerFields(CPlayer* pPlayer)
{
    if (UpdatePlayerField(pPlayer)        < 0) return -2;
    if (UpdateRecordField(pPlayer)        < 0) return -4;
    if (UpdateRecordField_Weekly(pPlayer) < 0) return -4;
    if (UpdateItemField(pPlayer)          < 0) return -6;
    if (UpdateSkillField(pPlayer)         < 0) return -7;
    if (UpdateTrainingField(pPlayer)      < 0) return -8;
    if (UpdateCeremonyField(pPlayer)      < 0) return -9;
    if (UpdateCardField(pPlayer)          < 0) return -9;
    if (UpdateSkinField(pPlayer)          < 0) return -10;
    if (UpdateBuddyField(pPlayer)         < 0) return -11;
    if (UpdateBlackListField(pPlayer)     < 0) return -11;
    return 0;
}

// ---------------------------------------------------------------------------
// UpdatePlayerField - STUB in the binary (returns 0). Persistence is performed
// by the field-specific methods.
// ---------------------------------------------------------------------------
int CSql::UpdatePlayerField(CPlayer* /*pPlayer*/) { return 0; }

// ---------------------------------------------------------------------------
// UpdateSettingField - persist the full control/skill setting.
// ---------------------------------------------------------------------------
int CSql::UpdateSettingField(CPlayer* pPlayer)
{
    CSetting* p = &pPlayer->m_cSetting;
    Execute(
        "UPDATE tb_game_setting set setting_camera = %d, setting_zoom = %d, setting_target = %d, "
        "setting_camerateam = %d, setting_radian = %d, setting_shadow = %d, setting_label = %d, "
        "setting_sound = %d, setting_music = %d, setting_invite = %d, setting_whisper = %d, "
        "setting_friend = %d, setting_up = %d, setting_down = %d, setting_left = %d, "
        "setting_right = %d, setting_leftshoot = %d, setting_rightshoot = %d, setting_longpass = %d, "
        "setting_shortpass = %d, setting_screen = %d, setting_tackle = %d, setting_steal = %d, "
        "setting_quick = %d, setting_quick2 = %d, setting_skill1 = %d, setting_skill2 = %d, "
        "setting_skill3 = %d, setting_skill4 = %d, setting_skill5 = %d, setting_skill_attack1 = %d, "
        "setting_skill_attack2 = %d, setting_skill_attack3 = %d, setting_skill_attack4 = %d, "
        "setting_skill_attack5 = %d, setting_skill_defence1 = %d, setting_skill_defence2 = %d, "
        "setting_skill_defence3 = %d, setting_skill_defence4 = %d, setting_skill_defence5 = %d "
        "WHERE player_seq = %d",
        (int)p->m_nCameraType, (int)p->m_nCameraZoom, (int)p->m_nCameraTarget,
        (int)p->m_nCameraTeam, (int)p->m_nRadian, (int)p->m_nShadow,
        (int)p->m_nLabel, (int)p->m_nSound, (int)p->m_nMusic,
        (int)p->m_nInvite, (int)p->m_nWhisper, (int)p->m_nFriend,
        (int)p->m_nDefineKey[0],  (int)p->m_nDefineKey[1],  (int)p->m_nDefineKey[2],
        (int)p->m_nDefineKey[3],  (int)p->m_nDefineKey[4],  (int)p->m_nDefineKey[5],
        (int)p->m_nDefineKey[6],  (int)p->m_nDefineKey[7],  (int)p->m_nDefineKey[8],
        (int)p->m_nDefineKey[9],  (int)p->m_nDefineKey[10], (int)p->m_nDefineKey[11],
        (int)p->m_nDefineKey[12], (int)p->m_nDefineKey[13], (int)p->m_nDefineKey[14],
        (int)p->m_nDefineKey[15], (int)p->m_nDefineKey[16], (int)p->m_nDefineKey[17],
        p->m_nAttackSkillCode[0], p->m_nAttackSkillCode[1], p->m_nAttackSkillCode[2],
        p->m_nAttackSkillCode[3], p->m_nAttackSkillCode[4],
        p->m_nDefenceSkillCode[0], p->m_nDefenceSkillCode[1], p->m_nDefenceSkillCode[2],
        p->m_nDefenceSkillCode[3], p->m_nDefenceSkillCode[4], pPlayer->m_nPlayerSeq);
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateRecordField - upsert the lifetime record and the quarterly shard
// (INSERT ... ON DUPLICATE KEY UPDATE col=col+delta). Source = m_cCurrentRecord.m_nRecord
// (the accumulated-since-last-flush delta block @0x358).
// ---------------------------------------------------------------------------
static const char* kRecordCols =
    "record_match, record_win, record_draw, record_mark, record_mvp, record_goal, record_allow, "
    "record_assist, record_cut, record_shoot, record_steal, record_tackle, record_pass, "
    "record_guard, record_tryshoot, record_trysteal, record_trytackle, record_trypass, record_tryguard";

int CSql::UpdateRecordField(CPlayer* pPlayer)
{
    int* r = pPlayer->m_cCurrentRecord.m_nRecord;     // 19-int delta block

    // Lifetime table.
    Execute(
        "INSERT INTO tb_game_record (record_seq, player_seq, %s) VALUES ('', %d, "
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d) "
        "ON DUPLICATE KEY UPDATE record_match=record_match+%d, record_win=record_win+%d, "
        "record_draw=record_draw+%d, record_mark=record_mark+%d, record_mvp=record_mvp+%d, "
        "record_goal=record_goal+%d, record_allow=record_allow+%d, record_assist=record_assist+%d, "
        "record_cut=record_cut+%d, record_shoot=record_shoot+%d, record_steal=record_steal+%d, "
        "record_tackle=record_tackle+%d, record_pass=record_pass+%d, record_guard=record_guard+%d, "
        "record_tryshoot=record_tryshoot+%d, record_trysteal=record_trysteal+%d, "
        "record_trytackle=record_trytackle+%d, record_trypass=record_trypass+%d, "
        "record_tryguard=record_tryguard+%d",
        kRecordCols, pPlayer->m_nPlayerSeq,
        r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7],r[8],r[9],r[10],r[11],r[12],r[13],r[14],r[15],r[16],r[17],r[18],
        r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7],r[8],r[9],r[10],r[11],r[12],r[13],r[14],r[15],r[16],r[17],r[18]);

    // Quarterly shard.
    CTime cTime;
    GetDBTime(&cTime);
    CheckSampleTable("tb_game_record", cTime.m_nYear - 2000, (int)cTime.m_nQuarter);
    Execute(
        "INSERT INTO tb_game_record_%02d_%02d (record_seq, player_seq, %s) VALUES ('', %d, "
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d) "
        "ON DUPLICATE KEY UPDATE record_match=record_match+%d, record_win=record_win+%d, "
        "record_draw=record_draw+%d, record_mark=record_mark+%d, record_mvp=record_mvp+%d, "
        "record_goal=record_goal+%d, record_allow=record_allow+%d, record_assist=record_assist+%d, "
        "record_cut=record_cut+%d, record_shoot=record_shoot+%d, record_steal=record_steal+%d, "
        "record_tackle=record_tackle+%d, record_pass=record_pass+%d, record_guard=record_guard+%d, "
        "record_tryshoot=record_tryshoot+%d, record_trysteal=record_trysteal+%d, "
        "record_trytackle=record_trytackle+%d, record_trypass=record_trypass+%d, "
        "record_tryguard=record_tryguard+%d",
        cTime.m_nYear - 2000, (int)cTime.m_nQuarter, kRecordCols, pPlayer->m_nPlayerSeq,
        r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7],r[8],r[9],r[10],r[11],r[12],r[13],r[14],r[15],r[16],r[17],r[18],
        r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7],r[8],r[9],r[10],r[11],r[12],r[13],r[14],r[15],r[16],r[17],r[18]);
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateRecordField_Weekly - upsert the per-day weekly record (skipped if the
// delta block is empty). Same accumulation pattern.
// ---------------------------------------------------------------------------
int CSql::UpdateRecordField_Weekly(CPlayer* pPlayer)
{
    int* r = pPlayer->m_cCurrentRecord.m_nRecord;
    bool bEmpty = true;
    for (int i = 0; i < 19; ++i) if (r[i] != 0) { bEmpty = false; break; }
    if (bEmpty) return 0;

    if (Execute(
        "INSERT INTO tb_game_record_week (record_seq, player_seq, record_date, %s) "
        "VALUES ('', %d, CURDATE(), %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d) "
        "ON DUPLICATE KEY UPDATE record_match=record_match+%d, record_win=record_win+%d, "
        "record_draw=record_draw+%d, record_mark=record_mark+%d, record_mvp=record_mvp+%d, "
        "record_goal=record_goal+%d, record_allow=record_allow+%d, record_assist=record_assist+%d, "
        "record_cut=record_cut+%d, record_shoot=record_shoot+%d, record_steal=record_steal+%d, "
        "record_tackle=record_tackle+%d, record_pass=record_pass+%d, record_guard=record_guard+%d, "
        "record_tryshoot=record_tryshoot+%d, record_trysteal=record_trysteal+%d, "
        "record_trytackle=record_trytackle+%d, record_trypass=record_trypass+%d, "
        "record_tryguard=record_tryguard+%d",
        kRecordCols, pPlayer->m_nPlayerSeq,
        r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7],r[8],r[9],r[10],r[11],r[12],r[13],r[14],r[15],r[16],r[17],r[18],
        r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7],r[8],r[9],r[10],r[11],r[12],r[13],r[14],r[15],r[16],r[17],r[18]) == NULL)
        return -2;
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateItemField - flush the inventory by per-item dirty state, then free the
// list. state 0 = remove/zero, 2 = modify, 4 = delete; 1 = unchanged (skip).
// ---------------------------------------------------------------------------
int CSql::UpdateItemField(CPlayer* pPlayer)
{
    for (size_t i = 0; i < pPlayer->m_vItemList.size(); ++i)
    {
        CGameItem* p = (CGameItem*)pPlayer->m_vItemList[i];
        if (p->m_nState == 1) continue;     // unchanged

        if (p->m_nState == 2)
        {
            if (Execute(
                "UPDATE tb_game_item set item_state='1', item_code=%d, item_equip='%d', "
                "item_amount=%d, item_grade=%d, item_limit=%d, item_price=%d, item_option0=%d, "
                "item_option1=%d, item_option2=%d, item_option3=%d, item_option4=%d "
                "WHERE item_seq=%d",
                p->m_nCode, (int)p->m_nEquipKind, (int)p->m_nAmount, p->m_nGrade, p->m_nLimit,
                p->m_nPrice, p->m_nOptionCode[0], p->m_nOptionCode[1], p->m_nOptionCode[2],
                p->m_nOptionCode[3], p->m_nOptionCode[4], p->m_nItemSeq) == NULL)
                return -2;
        }
        else // state 0 or 4 : soft-delete with deletedate
        {
            if (Execute(
                "UPDATE tb_game_item set item_state='%d', item_amount=%d, item_deletedate=now() "
                "WHERE item_seq=%d",
                (int)p->m_nState, (int)p->m_nAmount, p->m_nItemSeq) == NULL)
                return -2;
        }
    }
    for (size_t i = 0; i < pPlayer->m_vItemList.size(); ++i)
        delete (CGameItem*)pPlayer->m_vItemList[i];
    pPlayer->m_vItemList.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateCardField - flush dirty cards; state 0 = delete, 2 = update.
// ---------------------------------------------------------------------------
int CSql::UpdateCardField(CPlayer* pPlayer)
{
    for (size_t i = 0; i < pPlayer->m_vCardList.size(); ++i)
    {
        CCard* p = pPlayer->m_vCardList[i];
        if (p->m_nState == 0)
        {
            if (Execute("UPDATE tb_game_card set card_state='0', card_deletedate=now() "
                        "WHERE card_seq=%d", p->m_nCardSeq) == NULL)
                return -2;
        }
        else if (p->m_nState == 2)
        {
            if (Execute(
                "UPDATE tb_game_card set card_state='1', card_code=%d, card_equip1=%d, "
                "card_equip2=%d, card_equip3=%d, card_level=%d, card_rank=%d, card_position=%d, "
                "card_area=%d, card_skill=%d, card_costume=%d, card_tired=%d WHERE card_seq=%d",
                (int)p->m_nCode, p->m_nEquip1, p->m_nEquip2, (int)p->m_nEquip3, (int)p->m_nLevel,
                (int)p->m_nRank, (int)p->m_nPosition, (int)p->m_nArea, (int)p->m_nSkill,
                (int)p->m_nCostume, (int)p->m_nTired, p->m_nCardSeq) == NULL)
                return -2;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// AddItemAmount - bump a stackable item's amount.
// ---------------------------------------------------------------------------
int CSql::AddItemAmount(int nPlayerSeq, int nItemSeq, int /*nArg*/)
{
    if (Execute("UPDATE tb_game_item SET item_amount=item_amount+1 WHERE player_seq=%d and item_seq=%d",
                nPlayerSeq, nItemSeq) == NULL)
        return -2;
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateSkillField - flush dirty skills (state 2 -> update).
// ---------------------------------------------------------------------------
int CSql::UpdateSkillField(CPlayer* pPlayer)
{
    for (size_t i = 0; i < pPlayer->m_vSkillList.size(); ++i)
    {
        CSkill* p = pPlayer->m_vSkillList[i];
        if (p->m_nState == 2)
        {
            if (Execute("UPDATE tb_game_skill set skill_state='1', skill_equip=%d, skill_level=%d "
                        "WHERE skill_seq=%d",
                        (int)p->m_nEquipKind, (int)p->m_nLevel, p->m_nSkillSeq) == NULL)
                return -2;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateTrainingField - flush dirty trainings (state 2 -> update).
// ---------------------------------------------------------------------------
int CSql::UpdateTrainingField(CPlayer* pPlayer)
{
    for (size_t i = 0; i < pPlayer->m_vTrainingList.size(); ++i)
    {
        CTraining* p = pPlayer->m_vTrainingList[i];
        if (p->m_nState == 2)
        {
            if (Execute("UPDATE tb_game_training set training_state='1', training_code=%d, "
                        "training_level=%d WHERE training_seq=%d",
                        p->m_nCode, (int)p->m_nLevel, p->m_nTrainingSeq) == NULL)
                return -2;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateCeremonyField - flush dirty ceremonies; state 0 = delete, 2 = update.
// ---------------------------------------------------------------------------
int CSql::UpdateCeremonyField(CPlayer* pPlayer)
{
    for (size_t i = 0; i < pPlayer->m_vCeremonyList.size(); ++i)
    {
        CCeremony* p = pPlayer->m_vCeremonyList[i];
        if (p->m_nState == 0)
        {
            if (Execute("UPDATE tb_game_ceremony set ceremony_state='0', ceremony_deletedate=now() "
                        "WHERE ceremony_seq=%d", p->m_nCeremonySeq) == NULL)
                return -2;
        }
        else if (p->m_nState == 2)
        {
            if (Execute("UPDATE tb_game_ceremony set ceremony_state='1', ceremony_equip=%d "
                        "WHERE ceremony_seq=%d",
                        (int)p->m_nEquipKind, p->m_nCeremonySeq) == NULL)
                return -2;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateBuddyField / UpdateBlackListField - free the in-memory lists. (Buddy
// persistence is via Add*/Delete*; these just release the loaded vectors.)
// ---------------------------------------------------------------------------
int CSql::UpdateBuddyField(CPlayer* pPlayer)
{
    for (size_t i = 0; i < pPlayer->m_vBuddyList.size(); ++i) delete pPlayer->m_vBuddyList[i];
    pPlayer->m_vBuddyList.clear();
    return 0;
}

int CSql::UpdateBlackListField(CPlayer* pPlayer)
{
    for (size_t i = 0; i < pPlayer->m_vBlackList.size(); ++i) delete pPlayer->m_vBlackList[i];
    pPlayer->m_vBlackList.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateLevelField - persist level/exp/faculty/skill points.
// ---------------------------------------------------------------------------
int CSql::UpdateLevelField(CPlayer* pPlayer)
{
    if (Execute("UPDATE tb_game_player set player_level=%d, player_exp=%d, player_faculty=%d, "
                "player_skill=%d WHERE player_seq=%d",
                (int)pPlayer->m_cLevel.m_nLevel, pPlayer->m_cLevel.m_nExp, (int)pPlayer->m_cLevel.m_nFaculty,
                (int)pPlayer->m_cLevel.m_nSkill, pPlayer->m_nPlayerSeq) == NULL)
        return -2;
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateMoneyField - persist the account wallet.
// ---------------------------------------------------------------------------
int CSql::UpdateMoneyField(CPlayer* pPlayer)
{
    if (Execute("UPDATE tb_game_trio set trio_cash=%d, trio_point=%d, trio_credit=%d "
                "WHERE member_seq=%d",
                pPlayer->m_cMoney.m_nCash, pPlayer->m_cMoney.m_nPoint,
                pPlayer->m_cMoney.m_nCredit, pPlayer->m_nMemberSeq) == NULL)
        return -2;
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateStateField - flag the account blocked (state 5).
// ---------------------------------------------------------------------------
int CSql::UpdateStateField(CPlayer* pPlayer)
{
    if (Execute("UPDATE tb_mst_member set member_state=5 WHERE member_seq=%d",
                pPlayer->m_nMemberSeq) == NULL)
        return -2;
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateSkinField - persist skin only when the resolved (item-driven) skin
// differs from the current one.
// ---------------------------------------------------------------------------
int CSql::UpdateSkinField(CPlayer* pPlayer)
{
    int nSkin = GetItemSkin(pPlayer->m_nEquipWear);
    if (nSkin != pPlayer->m_cShape.m_nSkin)
    {
        pPlayer->m_cShape.m_nSkin = (char)nSkin;
        if (Execute("UPDATE tb_game_player set player_skin=%d WHERE player_seq=%d",
                    nSkin, pPlayer->m_nPlayerSeq) == NULL)
            return -2;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateNameField / UpdateMentField - persist name / ment and mirror locally.
// ---------------------------------------------------------------------------
int CSql::UpdateNameField(CPlayer* pPlayer, char* sName)
{
    if (Execute("UPDATE tb_game_player set player_name='%s' WHERE player_seq=%d",
                sName, pPlayer->m_nPlayerSeq) == NULL)
        return -2;
    snprintf(pPlayer->m_sName, 0xf, "%s", sName);
    return 0;
}

int CSql::UpdateMentField(CPlayer* pPlayer, char* sMent)
{
    if (Execute("UPDATE tb_game_player set player_ment='%s' WHERE player_seq=%d",
                sMent, pPlayer->m_nPlayerSeq) == NULL)
        return -2;
    snprintf(pPlayer->m_sMent, 0x2d, "%s", sMent);
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateTodayField - persist the daily random-shop seed/state.
// ---------------------------------------------------------------------------
int CSql::UpdateTodayField(CPlayer* pPlayer)
{
    if (Execute("UPDATE tb_game_player SET player_rand=%d, player_shopstate=%d WHERE player_seq=%d",
                pPlayer->m_nTodaySeed, pPlayer->m_nTodayBit, pPlayer->m_nPlayerSeq) == NULL)
        return -2;
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateShutupField - set/clear a mute window (player_shutupdate). nState is the
// mute duration in minutes (0 clears).
// ---------------------------------------------------------------------------
int CSql::UpdateShutupField(CPlayer* pPlayer, int nState)
{
    int nShutup = 0;
    if (nState > 0)
    {
        MY_ROW row;
        MYSQL_RES* res = Query("SELECT UNIX_TIMESTAMP(DATE_ADD(NOW(), INTERVAL +%d MINUTE)) as shutup_date",
                               nState);
        if (res == NULL) return -1;
        if (!FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return -1; }
        nShutup = ATOI(row["shutup_date"]);
        FREE_RESULT(res);
        row.clear();
    }
    pPlayer->m_nShutupDate = nShutup;

    if (Execute("UPDATE tb_game_player SET player_shutupdate=FROM_UNIXTIME(%d) WHERE player_seq=%d",
                nShutup, pPlayer->m_nPlayerSeq) == NULL)
        return -2;
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateFaculty - persist the current-faculty block (18 cols) plus the spare
// faculty point on the player row.
// ---------------------------------------------------------------------------
int CSql::UpdateFaculty(CPlayer* pPlayer)
{
    char* f = pPlayer->m_cRaiseFaculty.m_nFaculty;
    if (Execute(
        "UPDATE tb_game_faculty SET faculty_run=%d, faculty_dribble=%d, faculty_quick=%d, "
        "faculty_stamina=%d, faculty_elasticity=%d, faculty_bodycheck=%d, faculty_cross=%d, "
        "faculty_shortpass=%d, faculty_longpass=%d, faculty_headshoot=%d, faculty_shortshoot=%d, "
        "faculty_longshoot=%d, faculty_keeping=%d, faculty_steal=%d, faculty_tackle=%d, "
        "faculty_catch=%d, faculty_punch=%d, faculty_guard=%d WHERE player_seq=%d;",
        (int)f[0],(int)f[1],(int)f[2],(int)f[3],(int)f[4],(int)f[5],(int)f[6],(int)f[7],(int)f[8],
        (int)f[9],(int)f[10],(int)f[11],(int)f[12],(int)f[13],(int)f[14],(int)f[15],(int)f[16],(int)f[17],
        pPlayer->m_nPlayerSeq) == NULL)
        return -1;

    if (Execute("UPDATE tb_game_player SET player_faculty=%d WHERE player_seq=%d",
                (int)pPlayer->m_cLevel.m_nFaculty, pPlayer->m_nPlayerSeq) == NULL)
        return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// UpdatePositionField - change position: zero the faculty block, then write the
// new position and faculty point onto the player row.
// ---------------------------------------------------------------------------
int CSql::UpdatePositionField(CPlayer* pPlayer)
{
    if (Execute(
        "UPDATE tb_game_faculty SET faculty_run=0, faculty_dribble=0, faculty_quick=0, "
        "faculty_stamina=0, faculty_elasticity=0, faculty_bodycheck=0, faculty_cross=0, "
        "faculty_shortpass=0, faculty_longpass=0, faculty_headshoot=0, faculty_shortshoot=0, "
        "faculty_longshoot=0, faculty_keeping=0, faculty_steal=0, faculty_tackle=0, "
        "faculty_catch=0, faculty_punch=0, faculty_guard=0 WHERE player_seq=%d",
        pPlayer->m_nPlayerSeq) == NULL)
        return -1;

    if (Execute("UPDATE tb_game_player SET player_position=%d, player_faculty=%d WHERE player_seq=%d;",
                (int)pPlayer->m_nPosition, (int)pPlayer->m_cLevel.m_nFaculty, pPlayer->m_nPlayerSeq) == NULL)
        return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateSkinHairField - persist face/hair appearance ints.
// ---------------------------------------------------------------------------
int CSql::UpdateSkinHairField(CPlayer* pPlayer)
{
    int nFace = pPlayer->m_nFreeWear[0];
    int nHair = pPlayer->m_nFreeWear[1];
    if (Execute("UPDATE kicks2.tb_game_player SET player_face=%d, player_hair=%d WHERE player_seq=%d;",
                nFace, nHair, pPlayer->m_nPlayerSeq) == NULL)
        return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateQuest - bump (or create) a quest progress counter for a quest code.
// ---------------------------------------------------------------------------
int CSql::UpdateQuest(CPlayer* pPlayer, char nQuestCode)
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT quest_seq, quest_count FROM tb_game_quest "
                           "WHERE player_seq=%d and quest_code=%d",
                           pPlayer->m_nPlayerSeq, (int)nQuestCode);
    if (res == NULL) return -1;

    if (FetchRow(res, &row))
    {
        int nQuestSeq = ATOI(row["quest_seq"]);
        int nCount    = ATOI(row["quest_count"]);
        FREE_RESULT(res);
        row.clear();
        if (Execute("UPDATE tb_game_quest SET quest_count=quest_count+1, quest_playdate=now() "
                    "WHERE quest_seq=%d", nQuestSeq) == NULL)
            return -2;
        return nCount;
    }

    FREE_RESULT(res);
    row.clear();
    if (Execute("INSERT INTO tb_game_quest SET member_seq=%d, player_seq=%d, quest_code=%d, "
                "quest_count=1, quest_playdate=now()",
                pPlayer->m_nMemberSeq, pPlayer->m_nPlayerSeq, (int)nQuestCode) == NULL)
        return -2;
    return 0;
}

// ===========================================================================
//  INSERTS
// ===========================================================================

// ---------------------------------------------------------------------------
// InsertCardField - add a card; writes the new card_seq back into the record.
// ---------------------------------------------------------------------------
int CSql::InsertCardField(CPlayer* pPlayer, CCard* pCard)
{
    MYSQL* hDB = Execute(
        "INSERT INTO tb_game_card SET player_seq=%d, member_seq=%d, card_code=%d, card_equip1=%d, "
        "card_equip2=%d, card_equip3=%d, card_level=%d, card_rank=%d, card_position=%d, "
        "card_area=%d, card_skill=%d, card_costume=%d, card_tired=%d, card_state='%d', "
        "card_deletedate=now()",
        pPlayer->m_nPlayerSeq, pPlayer->m_nMemberSeq, (int)pCard->m_nCode,
        0, 0, 0, (int)pCard->m_nLevel, (int)pCard->m_nRank, (int)pCard->m_nPosition,
        (int)pCard->m_nArea, (int)pCard->m_nSkill, (int)pCard->m_nCostume, (int)pCard->m_nTired,
        (int)pCard->m_nState);
    if (hDB == NULL) return -2;
    pCard->m_nCardSeq = mysql_insert_id(hDB);
    return pCard->m_nCardSeq;
}

// ---------------------------------------------------------------------------
// InsertItemField (CPlayer*, CItem*) - add an item to the owner's inventory;
// writes the new item_seq back into the record.
// ---------------------------------------------------------------------------
int CSql::InsertItemField(CPlayer* pPlayer, CItem* pItem)
{
    return InsertItemField(pPlayer->m_nMemberSeq, pPlayer->m_nPlayerSeq, pItem);
}

// ---------------------------------------------------------------------------
// InsertItemField (int, int, CItem*) - add an item for an explicit member/player
// (used for gift/mail to offline players).
// ---------------------------------------------------------------------------
int CSql::InsertItemField(int nMemberSeq, int nPlayerSeq, CItem* pItem)
{
    CGameItem* p = (CGameItem*)pItem;
    MYSQL* hDB = Execute(
        "INSERT INTO tb_game_item (item_seq, member_seq, player_seq, item_state, item_code, "
        "item_equip, item_amount, item_grade, item_limit, item_price, item_option0, item_option1, "
        "item_option2, item_option3, item_option4, item_buydate, item_random, item_order) "
        "VALUES ('', %d, %d, '%d', %d, '%d', %d, %d, %d, %d, %d, %d, %d, %d, %d, now(), %d, '%d')",
        nMemberSeq, nPlayerSeq, (int)p->m_nState, p->m_nCode, (int)p->m_nEquipKind, (int)p->m_nAmount,
        (int)p->m_nGrade, (int)p->m_nLimit, p->m_nPrice, p->m_nOptionCode[0], p->m_nOptionCode[1],
        p->m_nOptionCode[2], p->m_nOptionCode[3], p->m_nOptionCode[4], p->m_nRandom, (int)p->m_nOrder);
    if (hDB == NULL) return -2;
    p->m_nItemSeq = mysql_insert_id(hDB);
    return p->m_nItemSeq;
}

// ---------------------------------------------------------------------------
// InsertMixItem - persist a crafted (mixed) item. Builds the same tb_game_item
// INSERT, sourcing the result code/options from the mix table.
// ---------------------------------------------------------------------------
int CSql::InsertMixItem(CPlayer* pPlayer, CCSMixItem1* pMix, CItemMixTable* pTable)
{
    // 1) Insert the resulting (mixed) item. Result code/amount/options come from
    //    the mix table; member/player from the player.
    int nCode   = pTable->m_nRewardItemCode;
    int nAmount = pTable->m_nRewardItemCnt;
    const int* nOpt = pTable->m_nOptionCode;   // [5]

    MYSQL* hDB = Execute(
        "INSERT INTO tb_game_item (item_seq, member_seq, player_seq, item_state, item_code, "
        "item_equip, item_amount, item_option0, item_option1, item_option2, item_option3, "
        "item_option4, item_buydate) VALUES ('', %d, %d, '1', %d, '%d', %d, %d, %d, %d, %d, %d, now())",
        pPlayer->m_nMemberSeq, pPlayer->m_nPlayerSeq, nCode, 0, nAmount,
        nOpt[0], nOpt[1], nOpt[2], nOpt[3], nOpt[4]);
    if (hDB == NULL) return -2;
    int nNewItemSeq = mysql_insert_id(hDB);

    // 2) Consume the 5 ingredient slots: item_seq[5] @+0x10, consume-count[5] @+0x24.
    for (int i = 0; i < 5; ++i)
    {
        int nItemSeq = pMix->m_nItemSeq[i];
        int nRemain  = pMix->m_nItemCnt[i];
        if (nItemSeq == 0) continue;
        if (nRemain > 0)
            Execute("UPDATE tb_game_item SET item_amount='%d' WHERE item_seq=%d and player_seq=%d",
                    nRemain, nItemSeq, pPlayer->m_nPlayerSeq);
        else
            Execute("UPDATE tb_game_item SET item_state='0', item_amount='0', item_equip='0' "
                    "WHERE item_seq=%d and player_seq=%d", nItemSeq, pPlayer->m_nPlayerSeq);
    }

    // 3) The crafted item is added to the in-memory inventory by the caller.
    return nNewItemSeq;
}

// ---------------------------------------------------------------------------
// InsertSkillField - learn a skill; writes new skill_seq back.
// ---------------------------------------------------------------------------
int CSql::InsertSkillField(CPlayer* pPlayer, CSkill* pSkill)
{
    MYSQL* hDB = Execute(
        "INSERT INTO tb_game_skill (skill_seq, member_seq, player_seq, skill_state, skill_code, "
        "skill_equip, skill_level, skill_buydate) VALUES ('', %d, %d, '1', %d, %d, %d, now())",
        pPlayer->m_nMemberSeq, pPlayer->m_nPlayerSeq, pSkill->m_nCode,
        (int)pSkill->m_nEquipKind, (int)pSkill->m_nLevel);
    if (hDB == NULL) return -2;
    pSkill->m_nSkillSeq = mysql_insert_id(hDB);
    return pSkill->m_nSkillSeq;
}

// ---------------------------------------------------------------------------
// InsertGiftField - queue a gift to a recipient's mailbox (links to an already-
// inserted gift item via item_seq).
// ---------------------------------------------------------------------------
int CSql::InsertGiftField(int nPlayerSeq, int nGifterSeq, int nItemSeq, char* sMsg)
{
    MYSQL* hDB = Execute(
        "INSERT INTO tb_game_gift (gift_seq, player_seq, gifter_seq, item_seq, gift_msg, "
        "gift_createdate) VALUES ('', %d, %d, %d, '%s', now())",
        nPlayerSeq, nGifterSeq, nItemSeq, sMsg);
    if (hDB == NULL) return -2;
    return mysql_insert_id(hDB);
}

// ---------------------------------------------------------------------------
// InsertTrainingField - record training progress; writes new training_seq back.
// ---------------------------------------------------------------------------
int CSql::InsertTrainingField(CPlayer* pPlayer, CTraining* pTraining)
{
    MYSQL* hDB = Execute(
        "INSERT INTO tb_game_training (training_seq, member_seq, player_seq, training_state, "
        "training_code, training_type, training_level, training_buydate) "
        "VALUES ('', %d, %d, '1', %d, %d, %d, now())",
        pPlayer->m_nMemberSeq, pPlayer->m_nPlayerSeq, pTraining->m_nCode,
        (int)pTraining->m_nType, (int)pTraining->m_nLevel);
    if (hDB == NULL) return -1;
    pTraining->m_nTrainingSeq = mysql_insert_id(hDB);
    return pTraining->m_nTrainingSeq;
}

// ---------------------------------------------------------------------------
// InsertCeremonyField - unlock a ceremony; writes new ceremony_seq back.
// ---------------------------------------------------------------------------
int CSql::InsertCeremonyField(CPlayer* pPlayer, CCeremony* pCeremony)
{
    MYSQL* hDB = Execute(
        "INSERT INTO tb_game_ceremony (ceremony_seq, member_seq, player_seq, ceremony_state, "
        "ceremony_code, ceremony_equip, ceremony_buydate) VALUES ('', %d, %d, '1', %d, %d, now())",
        pPlayer->m_nMemberSeq, pPlayer->m_nPlayerSeq, pCeremony->m_nCode,
        (int)pCeremony->m_nEquipKind);
    if (hDB == NULL) return -2;
    pCeremony->m_nCeremonySeq = mysql_insert_id(hDB);
    return pCeremony->m_nCeremonySeq;
}

// ---------------------------------------------------------------------------
// InsertSaleLog - append a purchase/sale record to the quarterly log shard.
// ---------------------------------------------------------------------------
int CSql::InsertSaleLog(CPlayer* pPlayer, CSale* pSale)
{
    CTime cTime;
    GetDBTime(&cTime);

    SetBaseDB(LOGDBFLAG);
    CheckSampleTable("tb_log_sale", cTime.m_nYear - 2000, (int)cTime.m_nQuarter);

    MYSQL* hDB = Execute(
        "INSERT INTO tb_log_sale_%02d_%02d (sale_seq, member_seq, player_seq, object_seq, "
        "object_code, object_kind, buy_kind, sale_kind, sale_price, sale_amount, store_kind, "
        "sale_buydate) VALUES ('', %d, %d, %d, %d, '%d', '%d', '%d', %d, %d, '0', now())",
        cTime.m_nYear - 2000, (int)cTime.m_nQuarter,
        pPlayer->m_nMemberSeq, pPlayer->m_nPlayerSeq, pSale->m_nObjectSeq, pSale->m_nObjectCode,
        (int)pSale->m_nObjectKind, (int)pSale->m_nBuyKind, (int)pSale->m_nSaleKind,
        pSale->m_nPrice, pSale->m_nAmount);

    SetBaseDB(MAINDBFLAG);
    return (hDB == NULL) ? -1 : 0;
}

// ---------------------------------------------------------------------------
// InsertConnectLog - append the periodic connection-count sample. NOTE: the
// shard is created per-month but the INSERT formats the suffix with quarter
// (quirk preserved from the binary, as in Certify).
// ---------------------------------------------------------------------------
int CSql::InsertConnectLog(int nConnect, int nRelay, int nRoom)
{
    CTime cTime;
    GetDBTime(&cTime);

    SetBaseDB(LOGDBFLAG);
    CheckSampleTable("tb_log_connect", cTime.m_nYear - 2000, (int)cTime.m_nMonth);

    MYSQL* hDB = Execute(
        "INSERT INTO tb_log_connect_%02d_%02d (connect_date, connect_server, connect_count, "
        "relay_count, room_count) VALUES (now(), %d, %d, %d, %d)",
        cTime.m_nYear - 2000, (int)cTime.m_nQuarter,
        (int)g_Config.m_nServerCode, nConnect, nRelay, nRoom);

    SetBaseDB(MAINDBFLAG);
    return (hDB == NULL) ? -1 : 0;
}

// ---------------------------------------------------------------------------
// InsertPlayLog - append the per-session play summary to the monthly playlog
// shard. Records before/after (in/out) snapshots of exp/match/cash/point so the
// session delta can be derived. play-in values come from m_cPlayLog (snapshot
// taken at login); play-out values are the current totals.
// ---------------------------------------------------------------------------
int CSql::InsertPlayLog(CPlayer* pPlayer)
{
    CTime cTime;
    GetDBTime(&cTime);

    SetBaseDB(LOGDBFLAG);
    CheckSampleTable("tb_mst_playlog", cTime.m_nYear - 2000, (int)cTime.m_nMonth);

    char sIP[20];
    snprintf(sIP, sizeof(sIP), "%u.%u.%u.%u",
             (pPlayer->m_nNetSpeed >> 24) & 0xff, (pPlayer->m_nNetSpeed >> 16) & 0xff,
             (pPlayer->m_nNetSpeed >> 8) & 0xff, pPlayer->m_nNetSpeed & 0xff);

    MYSQL* hDB = Execute(
        "INSERT INTO tb_mst_playlog_%02d_%02d SET member_seq=%d, player_seq=%d, server_code=%d, "
        "playlog_exp_in=%d, playlog_exp_out=%d, playlog_match_in=%d, playlog_match_out=%d, "
        "playlog_cash_in=%d, playlog_cash_out=%d, playlog_point_in=%d, playlog_point_out=%d, "
        "playlog_remoteip='%s', playlog_date_in=FROM_UNIXTIME(%d), playlog_date_out=now();",
        cTime.m_nYear - 2000, (int)cTime.m_nMonth,
        pPlayer->m_nMemberSeq, pPlayer->m_nPlayerSeq, (int)g_Config.m_nServerCode,
        pPlayer->m_cPlayLog.m_nExp, pPlayer->m_cLevel.m_nExp,
        pPlayer->m_cPlayLog.m_nMatch, pPlayer->m_cTotalRecord.m_nRecord[0] + pPlayer->m_cCurrentRecord.m_nRecord[0],
        pPlayer->m_cPlayLog.m_nCash,  pPlayer->m_cMoney.m_nCash,
        pPlayer->m_cPlayLog.m_nPoint, pPlayer->m_cMoney.m_nPoint,
        sIP, pPlayer->m_cPlayLog.m_nDate);

    SetBaseDB(MAINDBFLAG);
    return (hDB == NULL) ? -1 : 0;
}

// ---------------------------------------------------------------------------
// InsertHackUser - record a detected cheat/hack event (tb_game_hack).
// ---------------------------------------------------------------------------
int CSql::InsertHackUser(CPlayer* pPlayer, int nCommand, int nType)
{
    if (Execute(
        "INSERT INTO tb_game_hack SET member_seq=%d, player_seq=%d, hack_command=%d, hack_type=%d, "
        "hack_date=now()",
        pPlayer->m_nMemberSeq, pPlayer->m_nPlayerSeq, nCommand, nType) == NULL)
        return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// PostItem - move an item to another player (mail/gift transfer). Clones the
// source item row to the destination player (INSERT ... SELECT, copying all the
// item_* fields), then soft-deletes the source. nSrcItemSeq is the item to move,
// nDstPlayerSeq the recipient. Returns the new item_seq.
// NOTE: the clone SELECT also carries item_level / item_class columns (seen only
// here), so those exist on tb_game_item in addition to the standard set.
// ---------------------------------------------------------------------------
int CSql::PostItem(CPlayer* pPlayer, int nSrcItemSeq, int nDstPlayerSeq)
{
    int nDstMemberSeq = GetMemberSeq(nDstPlayerSeq);
    if (nDstMemberSeq < 0) return -1;

    // Clone the source row onto the destination player.
    MYSQL* hDB = Execute(
        "INSERT INTO tb_game_item (member_seq, player_seq, item_state, item_code, item_equip, "
        "item_amount, item_grade, item_level, item_class, item_price, item_option0, item_option1, "
        "item_option2, item_option3, item_option4, item_buydate, item_deletedate) "
        "SELECT %d, %d, '1', item_code, '0', item_amount, item_grade, item_level, item_class, "
        "item_price, item_option0, item_option1, item_option2, item_option3, item_option4, "
        "now(), item_deletedate FROM tb_game_item WHERE item_seq=%d",
        nDstMemberSeq, nDstPlayerSeq, nSrcItemSeq);
    if (hDB == NULL) return -1;
    int nNewSeq = mysql_insert_id(hDB);

    // Remove the source item.
    Execute("UPDATE tb_game_item SET item_state='0', item_deletedate=now() WHERE item_seq=%d",
            nSrcItemSeq);
    (void)pPlayer;
    return nNewSeq;
}

// ===========================================================================
//  GIFT / SALE LIST (wire-packet fillers)
// ===========================================================================

// ---------------------------------------------------------------------------
// GetGiftList - fill a CSCGiftList packet with the account's pending gifts.
// Verified against XKICK_Game1 CSql::GetGiftList @080b1a6c (count query for the
// page total, then a paged data query; result buffered so the inline name lookup
// is safe).
// ---------------------------------------------------------------------------
int CSql::GetGiftList(CPlayer* pPlayer, int nType, int nPage, CSCGiftList* pList)
{
    MY_ROW row;
    MYSQL_RES* res;
    for (int i = 0; i < 10; ++i) pList->m_cGiftData[i].m_nItemSeq = 0;

    int nTotal = 0;
    if (nType != 0)   // gifts SENT by this player
    {
        res = Query("SELECT count(*) as count FROM tb_game_gift where gifter_seq = %d", pPlayer->m_nPlayerSeq);
        if (res == NULL) return -1;
        if (FetchRow(res, &row)) nTotal = ATOI(row["count"]);
        FREE_RESULT(res); row.clear();
        res = Query(
            "SELECT a.player_seq as player_seq, a.gift_msg as gift_msg, "
            "UNIX_TIMESTAMP(a.gift_createdate) as gift_date, b.item_state as item_state, "
            "a.item_seq as item_seq, b.item_code as item_code, b.item_amount as item_amount "
            "FROM tb_game_gift as a, tb_game_item as b "
            "WHERE a.gifter_seq = %d and a.item_seq = b.item_seq order by gift_date DESC limit %d, 10",
            pPlayer->m_nPlayerSeq, nPage * 10);
    }
    else              // gifts RECEIVED by this player
    {
        res = Query("SELECT count(*) as count FROM tb_game_gift where player_seq = %d", pPlayer->m_nPlayerSeq);
        if (res == NULL) return -1;
        if (FetchRow(res, &row)) nTotal = ATOI(row["count"]);
        FREE_RESULT(res); row.clear();
        res = Query(
            "SELECT a.gifter_seq as player_seq, a.gift_msg as gift_msg, "
            "UNIX_TIMESTAMP(a.gift_createdate) as gift_date, b.item_state as item_state, "
            "a.item_seq as item_seq, b.item_code as item_code, b.item_amount as item_amount "
            "FROM tb_game_gift as a, tb_game_item as b "
            "WHERE a.player_seq = %d and a.item_seq = b.item_seq order by gift_date DESC limit %d, 10",
            pPlayer->m_nPlayerSeq, nPage * 10);
    }
    if (res == NULL) return -1;

    int i = 0;
    while (FetchRow(res, &row) && i <= 9)
    {
        CGiftData& e = pList->m_cGiftData[i];
        e.m_nKind    = (char)ATOI(row["item_state"]);
        e.m_nItemSeq = ATOI(row["item_seq"]);
        e.m_nCode    = ATOI(row["item_code"]);
        e.m_nAmount  = ATOI(row["item_amount"]);
        snprintf(e.m_sFromName, 0xf,  "%s", GetPlayerName(ATOI(row["player_seq"])));
        snprintf(e.m_sMessage,  0x79, "%s", row["gift_msg"]);
        e.m_nDate    = ATOI(row["gift_date"]);
        ++i;
    }
    pList->m_nPage    = (i > 0) ? (short)nPage : (short)-1;
    pList->m_nMaxPage = (nTotal > 0) ? (short)((nTotal - 1) / 10 + 1) : (short)1;
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetSaleField - fill a CSCSaleList packet from a quarter's sale-log shard, paged.
// Verified against XKICK_Game1 CSql::GetSaleField @080b4492 (count query -> page
// total; reject out-of-range page with -3; data query OFFSET/LIMIT).
// ---------------------------------------------------------------------------
int CSql::GetSaleField(CPlayer* pPlayer, int nType, int nPage, CSCSaleList* pList)
{
    CTime cTime;
    GetDBTime(&cTime);

    SetBaseDB(LOGDBFLAG);
    CheckSampleTable("tb_log_sale", cTime.m_nYear - 2000, cTime.m_nQuarter);
    for (int i = 0; i < 10; ++i) pList->m_cSaleData[i].m_nObjectKind = 0;

    // nType != 0 -> current quarter; else the previous one (wrapping 1 -> 4).
    int nQuarter = (int)cTime.m_nQuarter;
    if (nType == 0) { nQuarter -= 1; if (nQuarter <= 0) nQuarter = 4; }

    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT count(*) as cnt from tb_log_sale_%02d_%02d WHERE player_seq = %d",
        cTime.m_nYear - 2000, nQuarter, pPlayer->m_nPlayerSeq);
    if (res == NULL) { pList->m_nMaxPage = 1; SetBaseDB(MAINDBFLAG); return 0; }

    int nTotal = 0;
    if (FetchRow(res, &row)) nTotal = ATOI(row["cnt"]);
    FREE_RESULT(res); row.clear();

    if (nTotal == 0) { pList->m_nMaxPage = 1; SetBaseDB(MAINDBFLAG); return 0; }
    pList->m_nMaxPage = (char)((nTotal % 10) ? (nTotal / 10 + 1) : (nTotal / 10));
    if (nPage + 1 > pList->m_nMaxPage) { SetBaseDB(MAINDBFLAG); return -3; }

    res = Query(
        "SELECT object_code, object_kind, buy_kind, sale_kind, sale_price, sale_amount, "
        "UNIX_TIMESTAMP(sale_buydate) as buydate FROM tb_log_sale_%02d_%02d "
        "WHERE player_seq = %d ORDER BY sale_buydate DESC LIMIT 10 OFFSET %d",
        cTime.m_nYear - 2000, nQuarter, pPlayer->m_nPlayerSeq, nPage * 10);
    if (res == NULL) { SetBaseDB(MAINDBFLAG); return -2; }

    int i = 0;
    while (FetchRow(res, &row) && i <= 9)
    {
        CSaleData& e = pList->m_cSaleData[i];
        e.m_nCode       = ATOI(row["object_code"]);
        e.m_nObjectKind = (char)ATOI(row["object_kind"]);
        e.m_nBuyKind    = (char)ATOI(row["buy_kind"]);
        e.m_nSaleKind   = (char)ATOI(row["sale_kind"]);
        e.m_nPrice      = ATOI(row["sale_price"]);
        e.m_nAmount     = (short)ATOI(row["sale_amount"]);
        e.m_nSaleDate   = ATOI(row["buydate"]);
        ++i;
    }
    FREE_RESULT(res);
    row.clear();
    SetBaseDB(MAINDBFLAG);
    return 0;
}

// ===========================================================================
//  MISC LOOKUPS / HELPERS
// ===========================================================================

// ---------------------------------------------------------------------------
// CheckPlayerName - banned-word gate then uniqueness. -5 disallowed, -4 taken.
// ---------------------------------------------------------------------------
int CSql::CheckPlayerName(char* sName)
{
    if (!g_Load.IsPossibleName(sName)) return -5;

    MY_ROW row;
    MYSQL_RES* res = Query("SELECT player_seq FROM tb_game_player WHERE player_name= \"%s\"", sName);
    if (res != NULL && FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return -4; }

    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetMemberSeq - resolve player_seq -> member_seq.
// ---------------------------------------------------------------------------
int CSql::GetMemberSeq(int nPlayerSeq)
{
    MY_ROW row;
    MYSQL_RES* res = Query("select member_seq from tb_game_player where player_seq = %d", nPlayerSeq);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return -1; }
    int n = ATOI(row["member_seq"]);
    FREE_RESULT(res);
    row.clear();
    return n;
}

// ---------------------------------------------------------------------------
// GetPlayerSeq - resolve an active character's name -> player_seq. 0 if absent.
// ---------------------------------------------------------------------------
int CSql::GetPlayerSeq(char* sName)
{
    MY_ROW row;
    MYSQL_RES* res = Query("select player_seq from tb_game_player where player_state = '1' "
                           "and player_name='%s'", sName);
    if (res == NULL) return 0;
    if (!FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return 0; }
    int n = ATOI(row["player_seq"]);
    FREE_RESULT(res);
    row.clear();
    return n;
}

// ---------------------------------------------------------------------------
// GetPlayerName - resolve player_seq -> name. Copies into a static buffer and
// returns it (the binary returned the raw column pointer, which the caller copied
// immediately; this is the same effect without the dangling-pointer hazard).
// ---------------------------------------------------------------------------
const char* CSql::GetPlayerName(int nPlayerSeq)
{
    static char s_sName[PLAYER_NAME_SIZE];
    s_sName[0] = '\0';
    MY_ROW row;
    MYSQL_RES* res = Query("select player_name from tb_game_player where player_seq = %d", nPlayerSeq);
    if (res == NULL) return s_sName;
    if (FetchRow(res, &row))
        snprintf(s_sName, sizeof(s_sName), "%s", row["player_name"]);
    FREE_RESULT(res);
    row.clear();
    return s_sName;
}

// ---------------------------------------------------------------------------
// GetServerMaxField - reload this server's max slot count into g_Config.
// (Ignores its CPlayer arg; uses g_Config.m_nServerCode.)
// ---------------------------------------------------------------------------
int CSql::GetServerMaxField(CPlayer* /*pPlayer*/)
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT server_max FROM tb_game_server WHERE server_code = %d",
                           (int)g_Config.m_nServerCode);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return -2; }
    g_Config.m_nConnector = (short)ATOI(row["server_max"]);
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// FindItemInPlayer - find an item_seq by code + all five option codes.
// ---------------------------------------------------------------------------
int CSql::FindItemInPlayer(int nPlayerSeq, int nCode, int n0, int n1, int n2, int n3, int n4)
{
    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT item_seq FROM tb_game_item WHERE player_seq=%d and item_code=%d and "
        "item_option0=%d and item_option1=%d and item_option2=%d and item_option3=%d and "
        "item_option4=%d",
        nPlayerSeq, nCode, n0, n1, n2, n3, n4);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return -3; }
    int n = ATOI(row["item_seq"]);
    FREE_RESULT(res);
    row.clear();
    return n;
}

// ---------------------------------------------------------------------------
// GetExpireTime - epoch of NOW() + nDays.
// ---------------------------------------------------------------------------
int CSql::GetExpireTime(int nDays)
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT UNIX_TIMESTAMP(DATE_ADD(NOW(), INTERVAL + %d DAY)) as ExpireTime",
                           nDays);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return -3; }
    int n = ATOI(row["ExpireTime"]);
    FREE_RESULT(res);
    row.clear();
    return n;
}

// ---------------------------------------------------------------------------
// AddExpireTime - epoch of FROM_UNIXTIME(nFrom) + nDays.
// ---------------------------------------------------------------------------
int CSql::AddExpireTime(int nFrom, int nDays)
{
    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT UNIX_TIMESTAMP(DATE_ADD(FROM_UNIXTIME(%d), INTERVAL + %d DAY)) as SumTime",
        nFrom, nDays);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return -3; }
    int n = ATOI(row["SumTime"]);
    FREE_RESULT(res);
    row.clear();
    return n;
}

// ---------------------------------------------------------------------------
// GetCurrentTime / GetCurrentTimeSecond / GetCurrentDay - DB clock helpers.
// ---------------------------------------------------------------------------
int CSql::GetCurrentTime()
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT UNIX_TIMESTAMP(now()) as current;");
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return -1; }
    int n = ATOI(row["current"]);
    FREE_RESULT(res);
    row.clear();
    return n;
}

int CSql::GetCurrentTimeSecond()
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT TIME_TO_SEC(now()) as current;");
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return -1; }
    int n = ATOI(row["current"]);
    FREE_RESULT(res);
    row.clear();
    return n;
}

int CSql::GetCurrentDay()
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT DAYOFWEEK(now()) as current;");
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return -1; }
    int n = ATOI(row["current"]);
    FREE_RESULT(res);
    row.clear();
    if (n == 1) n = 8;     // remap Sunday(1) to 8 (Mon..Sun = 2..8)
    return n;
}

// ---------------------------------------------------------------------------
// GetNoticeTable - load up to 5 enabled notices (seq -> text); return the newest
// notice's UNIX timestamp as a "version".
// ---------------------------------------------------------------------------
int CSql::GetNoticeTable(std::map<int, char*>* pArray)
{
    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT notice_seq, notice_text, UNIX_TIMESTAMP(notice_date) as notice_version "
        "FROM tb_game_notice WHERE notice_enable=1 ORDER BY notice_date DESC LIMIT 5");
    if (res == NULL) return -1;

    int nVersion = 0;
    while (FetchRow(res, &row))
    {
        char* pTitle = new char[0x79];
        if (nVersion == 0) nVersion = ATOI(row["notice_version"]);   // newest first
        snprintf(pTitle, 0x79, "%s", row["notice_text"]);
        (*pArray)[ATOI(row["notice_seq"])] = pTitle;
    }
    FREE_RESULT(res);
    row.clear();
    return nVersion;
}

// ---------------------------------------------------------------------------
// LoadWeatherTable - load the weather schedule (tb_game_weather) into pArray.
// Returns the row count.
// ---------------------------------------------------------------------------
int CSql::LoadWeatherTable(std::map<int, CWeather*>* pArray)
{
    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT weather_seq, round(weather_type+0) as weather_type, "
        "TIME_TO_SEC(weather_start) as weather_start, TIME_TO_SEC(weather_end) as weather_end, "
        "round(weather_date+0) as weather_date FROM tb_game_weather ORDER BY weather_seq");
    if (res == NULL) return -1;

    int nCnt = 0;
    while (FetchRow(res, &row))
    {
        // Populate the authoritative CWeather layout (Global.h). Note: this fixes
        // a former reader/writer offset mismatch - PacketCurrentWeather reads
        // type@0/date@1/start@8/end@0xc, which is this layout.
        CWeather* pW = new CWeather;
        pW->m_nWeatherSeq  = ATOI(row["weather_seq"]);
        pW->m_nWeatherType = (char)ATOI(row["weather_type"]);
        pW->m_nStart       = ATOI(row["weather_start"]);
        pW->m_nEnd         = ATOI(row["weather_end"]);
        pW->m_nDate        = (char)ATOI(row["weather_date"]);
        (*pArray)[ATOI(row["weather_seq"])] = pW;
        ++nCnt;
    }
    FREE_RESULT(res);
    row.clear();
    return nCnt;
}

// ---------------------------------------------------------------------------
// GetEventList - load the active reward events for this server. (Return value
// is left 0 on success, matching the binary/Certify quirk.)
// ---------------------------------------------------------------------------
int CSql::GetEventList(std::vector<CEvent*>* pList)
{
    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT event_type, reward_type, reward_value, "
        "UNIX_TIMESTAMP(event_startdate) as event_startdate, "
        "UNIX_TIMESTAMP(event_enddate) as event_enddate "
        "from tb_game_event where event_server = %d",
        (int)g_Config.m_nServerCode);
    if (res == NULL) return -1;

    int nCnt = 0;
    while (FetchRow(res, &row))
    {
        if (ATOI(row["event_type"]) == 0) continue;
        // CEvent (0x10): { char m_nEventType; char m_nRewardType; int m_nRewardValue;
        //                  int m_nStartTime; int m_nEndTime; }
        struct CEventRec { char m_nEventType; char m_nRewardType; char pad[2];
                           int m_nRewardValue; int m_nStartTime; int m_nEndTime; };
        CEventRec* pEvent = new CEventRec;
        if (pEvent == NULL) { FREE_RESULT(res); return -3; }
        pEvent->m_nEventType   = (char)ATOI(row["event_type"]);
        pEvent->m_nRewardType  = (char)ATOI(row["reward_type"]);
        pEvent->m_nRewardValue = ATOI(row["reward_value"]);
        pEvent->m_nStartTime   = ATOI(row["event_startdate"]);
        pEvent->m_nEndTime     = ATOI(row["event_enddate"]);
        pList->push_back((CEvent*)pEvent);
    }
    FREE_RESULT(res);
    row.clear();
    return nCnt;
}

// ===========================================================================
//  SESSION / SERVER BOOKKEEPING
// ===========================================================================

// ---------------------------------------------------------------------------
// SetLoginData - bind the account to this server and stamp the login time.
// (Unlike Certify, does NOT also touch tb_mst_member.)
// ---------------------------------------------------------------------------
int CSql::SetLoginData(int nMemberSeq)
{
    if (Execute("UPDATE tb_game_trio SET trio_server=%d, trio_logindate = now() WHERE member_seq = %d",
                (int)g_Config.m_nServerCode, nMemberSeq) == NULL)
        return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// SetLogOutData - unbind the account from any server.
// ---------------------------------------------------------------------------
int CSql::SetLogOutData(int nMemberSeq)
{
    if (Execute("UPDATE tb_game_trio SET trio_server=0 WHERE member_seq = %d", nMemberSeq) == NULL)
        return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// SetExecuteQuest - persist the account quest-progress flag.
// ---------------------------------------------------------------------------
int CSql::SetExecuteQuest(int nMemberSeq, int nQuest)
{
    if (Execute("update tb_game_trio set trio_quest = %d where member_seq = %d",
                nQuest, nMemberSeq) == NULL)
        return -1;
    return 0;
}

// ===========================================================================
//  SHARD HELPER
// ===========================================================================

// ---------------------------------------------------------------------------
// CheckSampleTable - ensure the quarterly/monthly shard "<strTable>_YY_QQ"
// exists in the current (log) DB. If absent, copy the CREATE TABLE template from
// the sample DB, instantiate it for this period, and clear its comment.
// (Identical to Certify.)
// ---------------------------------------------------------------------------
int CSql::CheckSampleTable(const char* strTable, int nArg1, int nArg2)
{
    MY_ROW row;
    int nOldDB = m_nBaseDBID;

    MYSQL_RES* res = Query("SHOW TABLES WHERE Tables_in_%s='%s_%02d_%02d';",
                           m_hDBList[m_nBaseDBID].c_str(), strTable, nArg1, nArg2);
    if (res != NULL && !FetchRow(res, &row))
    {
        SetBaseDB(SAMPLEDBFLAG);
        FREE_RESULT(res);
        res = Query("SHOW CREATE TABLE `%s_%%02d_%%02d`;", strTable);
        if (res == NULL || !FetchRow(res, &row)) { SetBaseDB(nOldDB); FREE_RESULT(res); return -1; }
        SetBaseDB(nOldDB);

        if (Execute(row["Create Table"], nArg1, nArg2) == NULL) { FREE_RESULT(res); row.clear(); return -1; }
        if (Execute("ALTER TABLE %s_%02d_%02d COMMENT='';", strTable, nArg1, nArg2) == NULL)
        { FREE_RESULT(res); row.clear(); return -1; }
    }

    SetBaseDB(nOldDB);
    FREE_RESULT(res);
    row.clear();
    return 0;
}
