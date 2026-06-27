// sql.cpp - CSql implementation (reconstructed from XKICK_Certify, Ghidra).
//
// CSql derives from CDatabase and owns every account/character DB access the
// Certify server performs. The class is the only surviving record of the MySQL
// schema; see docs/schema.sql for the recovered table layout.
//
// Conventions (matching the binary):
//   * Query(fmt, ...)   -> MYSQL_RES* for SELECTs; caller frees with FREE_RESULT.
//   * Execute(fmt, ...) -> MYSQL* (non-NULL on success) for DML.
//   * FetchRow(res, &row) fills MY_ROW (std::map<string,char*>); row[col] is the
//     raw column text, converted with atoi()/ATOI().
//   * Three logical databases are registered: 0 = main (kicks2),
//     1 = log (kicks2_log), 2 = sample. SetBaseDB() switches which one Query/
//     Execute target; log inserts temporarily flip to DB 1 and restore DB 0.
//
// Return-value convention (recovered): 0 / >=0 on success; negative codes carry
// specific failure meanings (-1 query error, -3 no row, -4 mismatch, -5 blocked,
// -6 not found, etc.) - preserved exactly from the binary.
//
// Portable: only standard C++ + the CDatabase API. No OS headers.
#include "Main.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// External helpers used by the binary (util.cpp / md5.cpp / load.cpp).
extern bool IsCheckSeparateString(const char* sList, const char* sValue, const char* sSep);
extern void IPTokener(char* sOut, const char* sIP);          // first 3 IP octets
extern void md5(std::string& sOut, const std::string& sIn);  // MD5 hex digest

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
CSql::CSql()  : CDatabase() {}
CSql::~CSql() {}

// ---------------------------------------------------------------------------
// InitServer - register the three DBs, read this server's port/slot count from
// tb_game_server, then load the default CSetting template from the column list
// of tb_game_setting (DESC), and finally clear stale logins.
// ---------------------------------------------------------------------------
bool CSql::InitServer()
{
    // Connection parameters come from config.ini (parsed into g_Config).
    SetConnectInfo(g_Config.m_sDBIP, g_Config.m_sDBUser, g_Config.m_sDBPass, g_Config.m_sCharset);
    AddDB(MAINDBFLAG,   g_Config.m_sDBMain);    // 0 -> kicks2
    AddDB(LOGDBFLAG,    g_Config.m_sDBLog);     // 1 -> kicks2_log
    AddDB(SAMPLEDBFLAG, g_Config.m_sDBSample);  // 2 -> sample
    SetBaseDB(MAINDBFLAG);

    // Pull this server's TCP/STS ports and connector (max-slot) count.
    MYSQL_RES* res = Query("SELECT server_port, server_max FROM tb_game_server WHERE server_code = %d",
                           (int)g_Config.m_nServerCode);
    if (res == NULL) return false;

    MY_ROW row;
    if (!FetchRow(res, &row)) { FREE_RESULT(res); return false; }

    g_Config.m_nTCPPort   = (short)atoi(row["server_port"]);
    g_Config.m_nSTSPort   = (short)atoi(row["server_port"]) + 1000;
    g_Config.m_nConnector = (short)atoi(row["server_max"]);
    FREE_RESULT(res);
    row.clear();

    // Load the default game setting from the tb_game_setting schema: DESC returns
    // one row per column (Field / Default); the Default value seeds g_Setting.
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

    // Mark every member that was logged into this server as logged-out.
    InitMemberLogin();
    return true;
}

// ---------------------------------------------------------------------------
// InitMemberLogin - bulk logout of all members tagged to this server.
// ---------------------------------------------------------------------------
int CSql::InitMemberLogin()
{
    if (Execute("UPDATE tb_game_trio SET trio_server=0 WHERE trio_server=%d;",
                (int)g_Config.m_nServerCode) == NULL)
        return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// GetMemberFields - convenience wrapper: GetMemberField then GetTrioField.
// ---------------------------------------------------------------------------
int CSql::GetMemberFields(CMember* pMember, int nCommand)
{
    if (GetMemberField(pMember) < 0) return -1;
    int nCheck = GetTrioField(pMember, nCommand);
    if (nCheck < 0) return nCheck;
    return 0;
}

// ---------------------------------------------------------------------------
// GetMemberField - partner string + net speed from tb_mst_member.
// ---------------------------------------------------------------------------
int CSql::GetMemberField(CMember* pMember)
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT member_partner, member_speed FROM tb_mst_member WHERE member_seq = %d",
                           pMember->m_nMemberSeq);
    if (res == NULL) return -1;

    if (!FetchRow(res, &row)) return -3;

    snprintf(pMember->m_sPartner, 0xf, "%s", row["member_partner"]);
    pMember->m_nNetSpeed = atoi(row["member_speed"]);

    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetTrioField - account-wide (3-character) wallet/flags from tb_game_trio.
// On the login command (nCommand == 1000 / CM_CERTIFY_LOGIN), a non-zero
// trio_server means the account is already online elsewhere (-4 ghost).
// ---------------------------------------------------------------------------
int CSql::GetTrioField(CMember* pMember, int nCommand)
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT * FROM tb_game_trio WHERE member_seq = %d", pMember->m_nMemberSeq);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) return -3;

    if (nCommand == 1000)
    {
        int nCurrentServer = atoi(row["trio_server"]);
        if (nCurrentServer != 0) return -4;
    }

    pMember->m_cMoney.m_nCash   = atoi(row["trio_cash"]);
    pMember->m_cMoney.m_nPoint  = atoi(row["trio_point"]);
    pMember->m_cMoney.m_nCredit = atoi(row["trio_credit"]);
    pMember->m_nLastSeq         = atoi(row["trio_lastseq"]);
    pMember->m_nCount           = (char)atoi(row["trio_count"]);
    pMember->m_nTutorial        = (char)atoi(row["trio_tutorial"]);
    pMember->m_nQuest           = (char)atoi(row["trio_quest"]);

    FREE_RESULT(res);
    row.clear();

    // Second pass: convert login/delete timestamps to unix epochs.
    res = Query("SELECT UNIX_TIMESTAMP(trio_logindate) as logindate, "
                "UNIX_TIMESTAMP(trio_deletedate) as deletedate FROM tb_game_trio WHERE member_seq = %d",
                pMember->m_nMemberSeq);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) return -3;

    pMember->m_nLoginDate  = atoi(row["logindate"]);
    pMember->m_nDeleteDate = atoi(row["deletedate"]);

    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetSettingField - load a character's control/skill setting into pSetting.
// ---------------------------------------------------------------------------
int CSql::GetSettingField(int nPlayerSeq, CSetting* pSetting)
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT * from tb_game_setting WHERE player_seq = %d", nPlayerSeq);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) return -2;

    CSetting cSetting;
    cSetting.m_nCameraType   = (char)ATOI(row["setting_camera"]);
    cSetting.m_nCameraZoom   = (char)ATOI(row["setting_zoom"]);
    cSetting.m_nCameraTarget = (char)ATOI(row["setting_target"]);
    cSetting.m_nCameraTeam   = (char)ATOI(row["setting_camerateam"]);
    cSetting.m_nRadian       = (char)ATOI(row["setting_radian"]);
    cSetting.m_nShadow       = (char)ATOI(row["setting_shadow"]);
    cSetting.m_nLabel        = (char)ATOI(row["setting_label"]);
    cSetting.m_nSound        = (char)ATOI(row["setting_sound"]);
    cSetting.m_nMusic        = (char)ATOI(row["setting_music"]);
    cSetting.m_nInvite       = (char)ATOI(row["setting_invite"]);
    cSetting.m_nWhisper      = (char)ATOI(row["setting_whisper"]);
    cSetting.m_nFriend       = (char)ATOI(row["setting_friend"]);
    cSetting.m_nDefineKey[0]  = (short)ATOI(row["setting_up"]);
    cSetting.m_nDefineKey[1]  = (short)ATOI(row["setting_down"]);
    cSetting.m_nDefineKey[2]  = (short)ATOI(row["setting_left"]);
    cSetting.m_nDefineKey[3]  = (short)ATOI(row["setting_right"]);
    cSetting.m_nDefineKey[4]  = (short)ATOI(row["setting_leftshoot"]);
    cSetting.m_nDefineKey[5]  = (short)ATOI(row["setting_rightshoot"]);
    cSetting.m_nDefineKey[6]  = (short)ATOI(row["setting_longpass"]);
    cSetting.m_nDefineKey[7]  = (short)ATOI(row["setting_shortpass"]);
    cSetting.m_nDefineKey[8]  = (short)ATOI(row["setting_screen"]);
    cSetting.m_nDefineKey[9]  = (short)ATOI(row["setting_tackle"]);
    cSetting.m_nDefineKey[10] = (short)ATOI(row["setting_steal"]);
    cSetting.m_nDefineKey[11] = (short)ATOI(row["setting_quick"]);
    cSetting.m_nDefineKey[12] = (short)ATOI(row["setting_quick2"]);
    cSetting.m_nDefineKey[13] = (short)ATOI(row["setting_skill1"]);
    cSetting.m_nDefineKey[14] = (short)ATOI(row["setting_skill2"]);
    cSetting.m_nDefineKey[15] = (short)ATOI(row["setting_skill3"]);
    cSetting.m_nDefineKey[16] = (short)ATOI(row["setting_skill4"]);
    cSetting.m_nDefineKey[17] = (short)ATOI(row["setting_skill5"]);
    cSetting.m_nAttackSkillCode[0]  = ATOI(row["setting_skill_attack1"]);
    cSetting.m_nAttackSkillCode[1]  = ATOI(row["setting_skill_attack2"]);
    cSetting.m_nAttackSkillCode[2]  = ATOI(row["setting_skill_attack3"]);
    cSetting.m_nAttackSkillCode[3]  = ATOI(row["setting_skill_attack4"]);
    cSetting.m_nAttackSkillCode[4]  = ATOI(row["setting_skill_attack5"]);
    cSetting.m_nDefenceSkillCode[0] = ATOI(row["setting_skill_defence1"]);
    cSetting.m_nDefenceSkillCode[1] = ATOI(row["setting_skill_defence2"]);
    cSetting.m_nDefenceSkillCode[2] = ATOI(row["setting_skill_defence3"]);
    cSetting.m_nDefenceSkillCode[3] = ATOI(row["setting_skill_defence4"]);
    cSetting.m_nDefenceSkillCode[4] = ATOI(row["setting_skill_defence5"]);

    memcpy(pSetting, &cSetting, sizeof(CSetting)); // 0x58 bytes
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetCharacterInfo - load one character (by member + display order) plus its
// equipped items into pInfo. Returns 1 if no character occupies that slot.
// ---------------------------------------------------------------------------
int CSql::GetCharacterInfo(CMember* pMember, int nOrder, CCharacterInfo* pInfo)
{
    MY_ROW row;

    // 1) resolve the player_seq for (member, order, active).
    MYSQL_RES* res = Query("SELECT player_seq FROM tb_game_player WHERE member_seq = %d and "
                           "player_state = '1' and player_order = %d",
                           pMember->m_nMemberSeq, nOrder);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) return 1;     // empty slot

    int nPlayerSeq = atoi(row["player_seq"]);
    FREE_RESULT(res);
    row.clear();

    // 2) full character record.
    res = Query("SELECT * FROM tb_game_player WHERE player_seq = %d and player_state = '1'", nPlayerSeq);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) return -3;

    pInfo->m_nPlayerSeq      = atoi(row["player_seq"]);
    pInfo->m_nOrder          = (short)atoi(row["player_order"]);
    snprintf(pInfo->m_sName, 0xf, "%s", row["player_name"]);
    pInfo->m_nPosition       = (char)atoi(row["player_position"]);
    pInfo->m_nCondition      = (char)atoi(row["player_condition"]);
    pInfo->m_nAlias          = (char)atoi(row["player_alias"]);
    pInfo->m_cLevel.m_nLevel   = (char)atoi(row["player_level"]);
    pInfo->m_cLevel.m_nExp     = atoi(row["player_exp"]);
    pInfo->m_cLevel.m_nFaculty = (short)atoi(row["player_faculty"]);
    pInfo->m_cLevel.m_nSkill   = (short)atoi(row["player_skill"]);
    pInfo->m_cShape.m_nGender  = (char)atoi(row["player_gender"]);
    pInfo->m_cShape.m_nSkin    = (char)atoi(row["player_skin"]);
    pInfo->m_cShape.m_nUniform = (char)atoi(row["player_uniform"]);

    for (int j = 0; j < 0x11; ++j) pInfo->m_nEquipWear[j] = 0;

    // Base appearance/default-wear slots stored directly on the player row.
    pInfo->m_nEquipWear[0] = atoi(row["player_face"]);
    pInfo->m_nEquipWear[1] = atoi(row["player_hair"]);
    pInfo->m_nEquipWear[3] = atoi(row["player_shirts"]);
    pInfo->m_nEquipWear[4] = atoi(row["player_pants"]);
    // Default skin tone item code (per gender) for the body slot.
    if (pInfo->m_cShape.m_nGender == 1) pInfo->m_nEquipWear[5] = 0xc198d0d;
    else                                pInfo->m_nEquipWear[5] = 0xc1990f5;
    pInfo->m_nEquipWear[6] = atoi(row["player_shoes"]);

    FREE_RESULT(res);
    row.clear();

    // 3) overlay equipped items, mapping (item_code / 1000000) to wear slot.
    res = Query("SELECT item_code FROM tb_game_item WHERE player_seq = %d and item_state = '1' "
                "and item_equip = '1'", nPlayerSeq);
    if (res == NULL) return -1;

    while (FetchRow(res, &row))
    {
        int nCode = atoi(row["item_code"]);
        switch (nCode / 1000000)
        {
            case 0x65: pInfo->m_nEquipWear[0]   = nCode; break;  // 101 face
            case 0x66: pInfo->m_nEquipWear[1]   = nCode; break;  // 102 hair
            case 0x67: pInfo->m_nEquipWear[2]   = nCode; break;  // 103
            case 0xc9: pInfo->m_nEquipWear[3]   = nCode; break;  // 201 shirts
            case 0xca: pInfo->m_nEquipWear[4]   = nCode; break;  // 202 pants
            case 0xcb: pInfo->m_nEquipWear[5]   = nCode; break;  // 203 body
            case 0xcc: pInfo->m_nEquipWear[6]   = nCode; break;  // 204 shoes
            case 0xcd: pInfo->m_nEquipWear[7]   = nCode; break;  // 205
            case 0x12d:pInfo->m_nEquipWear[8]   = nCode; break;  // 301
            case 0x12e:pInfo->m_nEquipWear[9]   = nCode; break;  // 302
            case 0x12f:pInfo->m_nEquipWear[10]  = nCode; break;  // 303
            case 0x191:pInfo->m_nEquipWear[11]  = nCode; break;  // 401
            case 0x192:pInfo->m_nEquipWear[12]  = nCode; break;  // 402
            case 0x193:pInfo->m_nEquipWear[13]  = nCode; break;  // 403
            case 0x194:pInfo->m_nEquipWear[14]  = nCode; break;  // 404
            case 0x195:pInfo->m_nEquipWear[15]  = nCode; break;  // 405
            case 0x196:pInfo->m_nEquipWear[16]  = nCode; break;  // 406
            default: break;
        }
    }
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// CheckGhostHost - resolve member_id -> member_seq, then read trio_host (the
// "is this account allowed to host?" flag). Returns the flag (>0 means host).
// ---------------------------------------------------------------------------
int CSql::CheckGhostHost(const char* sID)
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT member_seq FROM tb_mst_member WHERE member_id = '%s'", sID);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) return -3;

    int nMemberSeq = atoi(row["member_seq"]);
    FREE_RESULT(res);
    row.clear();

    res = Query("SELECT trio_host FROM tb_game_trio WHERE member_seq = %d", nMemberSeq);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) return -3;

    int nHost = atoi(row["trio_host"]);
    FREE_RESULT(res);
    row.clear();
    return nHost;
}

// ---------------------------------------------------------------------------
// CheckMemberID - authenticate a login: optional "service not open" gate (start
// table), password verification (md5, with a Dubai/200 plaintext variant and a
// "119x" master bypass), account-state checks, and a version stamp. Returns the
// member_seq on success, or a negative reason code.
// ---------------------------------------------------------------------------
int CSql::CheckMemberID(CMember* pMember, const char* sID, const char* sPass, const char* sVersion)
{
    MY_ROW row;
    int nMemberSeq = 0;

    // Service-open gate. When start_yn != 'Y', only whitelisted admin IDs/IPs
    // (or a LAN address) may connect.
    MYSQL_RES* res = Query("SELECT start_yn, start_adminid, start_adminip FROM tb_mst_start");
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) return -3;

    char cStartYn = *row["start_yn"];
    if (cStartYn != 'Y' && CheckGhostHost(sID) < 1)
    {
        if (!IsCheckSeparateString(row["start_adminid"], sID, "\r\n") &&
            !IsCheckSeparateString(row["start_adminip"], pMember->m_cTCPAddress.m_sIP, "\r\n"))
        {
            char sBuffer[20];
            IPTokener(sBuffer, pMember->m_cTCPAddress.m_sIP);
            if (strcmp(sBuffer, "192.168.1") != 0 && strcmp(sBuffer, "192.168.10") != 0)
            {
                FREE_RESULT(res);
                row.clear();
                return -6;          // service closed
            }
        }
    }
    FREE_RESULT(res);
    row.clear();

    // Look up the account.
    res = Query("SELECT member_seq, member_pass, member_state FROM tb_mst_member WHERE member_id = '%s'", sID);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) return -3;
    FREE_RESULT(res);

    // Password check (skipped entirely when the client sends the "119x" master key).
    if (strncmp(sPass, "119x", 0x41) != 0)
    {
        if (g_Config.m_nCompany == 200)
        {
            // Dubai build stores plaintext passwords; if it differs, re-sync it.
            if (strncmp(sPass, row["member_pass"], 0x41) != 0)
            {
                if (Execute("UPDATE tb_mst_member SET member_pass = '%s' WHERE member_id = '%s'",
                            sPass, sID) == NULL)
                    return -1;
            }
        }
        else
        {
            // Other builds compare an md5 of the supplied password; accept either
            // the md5 form or a legacy plaintext match.
            std::string strSecurePass(sPass);
            std::string strMd5;
            md5(strMd5, strSecurePass);
            if (strncmp(strMd5.c_str(), row["member_pass"], 0x41) != 0 &&
                strncmp(sPass,          row["member_pass"], 0x41) != 0)
                return -4;          // bad password
        }
    }

    // Account-state gate.
    int nState = atoi(row["member_state"]);
    if (nState == 0) return -3;
    if (nState == 4) return -5;     // blocked
    if (nState == 5) return -5;     // blocked

    nMemberSeq = atoi(row["member_seq"]);

    // Stamp the client version onto the account.
    if (sVersion != NULL)
    {
        if (Execute("UPDATE tb_game_trio set trio_version='%s' WHERE member_seq = %d",
                    sVersion, nMemberSeq) == NULL)
            return -1;
    }
    return nMemberSeq;
}

// ---------------------------------------------------------------------------
// CheckPlayerName - validate a proposed name (banned-word table) then ensure it
// is not already taken. 0 = available, -3 = taken, -4 = disallowed.
// ---------------------------------------------------------------------------
int CSql::CheckPlayerName(const char* sName)
{
    MY_ROW row;
    if (!g_Load.IsPossibleName(sName)) return -4;

    MYSQL_RES* res = Query("SELECT player_seq FROM tb_game_player WHERE player_name= \"%s\"", sName);
    if (FetchRow(res, &row)) return -3;

    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// WhereIsPlayer - which game server (and last character) holds this account.
// ---------------------------------------------------------------------------
int CSql::WhereIsPlayer(int nMemberSeq, int* nServerCode, int* nPlayerSeq)
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT trio_server, trio_lastseq FROM tb_game_trio WHERE member_seq = %d",
                           nMemberSeq);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) return -3;

    *nServerCode = atoi(row["trio_server"]);
    *nPlayerSeq  = atoi(row["trio_lastseq"]);

    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// CreateCharacter - allocate a free display slot (0..2) and INSERT a new player
// plus its faculty/setting rows. -5 = no free slot allowed, -6 = slot table full.
// ---------------------------------------------------------------------------
int CSql::CreateCharacter(CMember* pMember, CCSCreateCharacter* pCreate)
{
    MY_ROW row;
    int nOrder[3] = { 0, 0, 0 };

    MYSQL_RES* res = Query("SELECT player_order from tb_game_player where member_seq = %d and "
                           "player_state = '1'", pMember->m_nMemberSeq);
    while (FetchRow(res, &row))
    {
        int n = atoi(row["player_order"]);
        if (n >= 0 && n < 3) nOrder[n] = 1;
    }
    FREE_RESULT(res);
    row.clear();

    int nCount = 0;
    for (int i = 0; i < 3; ++i) if (nOrder[i] == 1) ++nCount;
    if (nCount >= pMember->m_nCount) return -5;     // slot quota reached

    int i = 0;
    for (; i < 3 && nOrder[i] != 0; ++i) {}
    if (i >= 3) return -6;

    MYSQL* hDB = Execute(
        "INSERT INTO tb_game_player  (player_seq, member_seq, player_order, player_name, "
        "player_gender, player_skin,  player_face, player_hair, player_shirts, player_pants, "
        "player_shoes, player_createdate, player_faculty)  "
        "VALUES ('', %d, %d, '%s', '%d', %d, %d, %d, %d, %d, %d, now(), 2)",
        pMember->m_nMemberSeq, i, pCreate->m_sName,
        (int)pCreate->m_cShape.m_nGender, (int)pCreate->m_cShape.m_nSkin,
        pCreate->m_nEquip[0], pCreate->m_nEquip[1], pCreate->m_nEquip[2],
        pCreate->m_nEquip[3], pCreate->m_nEquip[4]);
    if (hDB == NULL) return -1;

    int nPlayerSeq = mysql_insert_id(hDB);

    if (Execute("INSERT INTO tb_game_faculty (player_seq) VALUES (%d)", nPlayerSeq) == NULL) return -1;
    if (Execute("INSERT INTO tb_game_setting (player_seq) VALUES (%d)", nPlayerSeq) == NULL) return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// DeleteCharacter - verify the owner (RNN for Korean/company 100, password
// otherwise), then either hard-delete (level < 20) every related row or soft-
// delete (mark state 0) the player. Always stamps trio_deletedate. The hard
// delete also clears the quarterly record/ranking shard tables.
// ---------------------------------------------------------------------------
int CSql::DeleteCharacter(CMember* pMember, CCSDeleteCharacter* pDelete)
{
    MY_ROW row;
    CTime cTime;
    GetDBTime(&cTime);

    if (g_Config.m_nCompany == 100)
    {
        // Korean build: confirm with the last 7 digits of the resident reg. no.
        MYSQL_RES* res = Query("SELECT RIGHT(member_register, 7) as member_register FROM tb_mst_member "
                               "WHERE member_seq = %d", pMember->m_nMemberSeq);
        if (res == NULL) return -1;
        if (!FetchRow(res, &row)) return -3;
        if (strncmp(pDelete->m_sJumin, row["member_register"], 7) != 0) { FREE_RESULT(res); row.clear(); return -4; }
        FREE_RESULT(res);
        row.clear();
    }
    else
    {
        // Other builds: confirm with the account password.
        MYSQL_RES* res = Query("SELECT member_pass FROM tb_mst_member WHERE member_seq = %d",
                               pMember->m_nMemberSeq);
        if (res == NULL) return -1;
        if (!FetchRow(res, &row)) return -3;
        if (strncmp(pDelete->m_sJumin, row["member_pass"], 0x1e) != 0) { FREE_RESULT(res); row.clear(); return -4; }
        FREE_RESULT(res);
        row.clear();
    }

    // Decide hard vs soft delete by the character's level.
    MYSQL_RES* res = Query("SELECT player_level FROM tb_game_player WHERE player_seq = %d",
                           pDelete->m_nPlayerSeq);
    if (!FetchRow(res, &row)) return -6;
    int nLevel = atoi(row["player_level"]);
    FREE_RESULT(res);
    row.clear();

    if (nLevel < 0x14)      // < 20 : full removal
    {
        if (Execute("DELETE from tb_game_player WHERE player_seq = %d",   pDelete->m_nPlayerSeq) == NULL) return -1;
        if (Execute("DELETE from tb_game_faculty WHERE player_seq = %d",  pDelete->m_nPlayerSeq) == NULL) return -1;
        if (Execute("DELETE from tb_game_setting WHERE player_seq = %d",  pDelete->m_nPlayerSeq) == NULL) return -1;
        if (Execute("DELETE from tb_game_item WHERE player_seq = %d",     pDelete->m_nPlayerSeq) == NULL) return -1;
        if (Execute("DELETE from tb_game_skill WHERE player_seq = %d",    pDelete->m_nPlayerSeq) == NULL) return -1;
        if (Execute("DELETE from tb_game_ceremony WHERE player_seq = %d", pDelete->m_nPlayerSeq) == NULL) return -1;
        if (Execute("DELETE from tb_game_training WHERE player_seq = %d", pDelete->m_nPlayerSeq) == NULL) return -1;
        if (Execute("DELETE from tb_game_quest WHERE player_seq = %d and quest_code >= 200",
                    pDelete->m_nPlayerSeq) == NULL) return -1;
        if (Execute("DELETE from tb_game_record WHERE player_seq = %d",   pDelete->m_nPlayerSeq) == NULL) return -1;
        // Quarterly record shard (return value intentionally ignored, as in binary).
        Execute("DELETE from tb_game_record_%02d_%02d WHERE player_seq = %d",
                cTime.m_nYear - 2000, (int)cTime.m_nQuarter, pDelete->m_nPlayerSeq);
        if (Execute("DELETE from tb_game_ranking WHERE player_seq = %d",  pDelete->m_nPlayerSeq) == NULL) return -1;
    }
    else                    // >= 20 : soft delete (keep history)
    {
        if (Execute("UPDATE tb_game_player SET player_state = '0', player_deletedate = now() "
                    "WHERE player_seq = %d", pDelete->m_nPlayerSeq) == NULL) return -1;
    }

    // Record the account-level delete timestamp and read it back as an epoch.
    if (Execute("UPDATE tb_game_trio SET trio_deletedate = now() WHERE member_seq = %d",
                pMember->m_nMemberSeq) == NULL) return -1;

    res = Query("SELECT UNIX_TIMESTAMP(trio_deletedate) as DeleteDate FROM tb_game_trio WHERE member_seq = %d",
                pMember->m_nMemberSeq);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) return -3;

    pMember->m_nDeleteDate = atoi(row["DeleteDate"]);
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetServerCurrent - current player count on a game server (from tb_game_trio).
// ---------------------------------------------------------------------------
int CSql::GetServerCurrent(int nServerCode)
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT count(*) as current from tb_game_trio WHERE trio_server = %d", nServerCode);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) return -3;

    FREE_RESULT(res);   // freed before reading 'current' in the binary; map already populated
    int nCurrent = atoi(row["current"]);
    row.clear();
    return nCurrent;
}

// ---------------------------------------------------------------------------
// GetServerList - fill the server-select list for a channel (channel 9 = global)
// from tb_game_server, computing live populations via GetServerCurrent.
// ---------------------------------------------------------------------------
int CSql::GetServerList(int nChannel, CSCServerList* pList)
{
    MY_ROW row;
    MYSQL_RES* res = Query(
        "SELECT server_code, server_state, server_name, server_match, server_job, server_free,  "
        "server_slevel, server_elevel, server_max, server_current, server_exip, server_port  "
        "FROM tb_game_server WHERE server_channel = 9 or server_channel = %d order by server_code",
        nChannel);
    if (res == NULL) return -1;

    for (int i = 0; i < 10; ++i) pList->m_cServerData[i].m_nState = 0;

    int i = 0;
    while (FetchRow(res, &row))
    {
        if (atoi(row["server_state"]) == 0) continue;   // skip offline servers

        pList->m_nChannel = (char)nChannel;
        // NOTE(verify): the binary stores server_code into a field the current
        // Protocol.h CServerData does not expose (m_nCode/m_nMatch/m_nJob/...).
        // Field names below follow the binary; reconcile CServerData in Protocol.h.
        pList->m_cServerData[i].m_nCode   = atoi(row["server_code"]);
        pList->m_cServerData[i].m_nState  = (char)atoi(row["server_state"]);
        pList->m_cServerData[i].m_nMatch  = (char)atoi(row["server_match"]);
        pList->m_cServerData[i].m_nJob    = (char)atoi(row["server_job"]);
        pList->m_cServerData[i].m_nFree   = (char)atoi(row["server_free"]);
        pList->m_cServerData[i].m_nSLevel = (char)atoi(row["server_slevel"]);
        pList->m_cServerData[i].m_nELevel = (char)atoi(row["server_elevel"]);
        pList->m_cServerData[i].m_nMax    = (short)atoi(row["server_max"]);
        pList->m_cServerData[i].m_nCurrent= (short)GetServerCurrent(pList->m_cServerData[i].m_nCode);
        snprintf(pList->m_cServerData[i].m_cAddress.m_sIP, 0x14, "%s", row["server_exip"]);
        pList->m_cServerData[i].m_cAddress.m_nPort = atoi(row["server_port"]);
        // Official client (PutGameLogin a2) needs a UDP endpoint; the game UDP port is TCP-1000.
        snprintf(pList->m_cServerData[i].m_cUDPAddress.m_sIP, 0x14, "%s", row["server_exip"]);
        pList->m_cServerData[i].m_cUDPAddress.m_nPort = atoi(row["server_port"]) - 1000;
        snprintf(pList->m_cServerData[i].m_sTitle, 0x1f, "%s", row["server_name"]);
        ++i;
    }
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// CheckChoiceServer - validate that a chosen server is online (state 2) and
// hand back its external IP/port. -4 = server not available.
// ---------------------------------------------------------------------------
int CSql::CheckChoiceServer(int nServerCode, CAddress* pAddress)
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT server_state, server_exip, server_port from tb_game_server "
                           "WHERE server_code = %d", nServerCode);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) return -3;

    if (atoi(row["server_state"]) != 2) { FREE_RESULT(res); row.clear(); return -4; }

    snprintf(pAddress->m_sIP, 0x14, "%s", row["server_exip"]);
    pAddress->m_nPort = atoi(row["server_port"]);

    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetMoneyField - reload the account wallet from tb_game_trio.
// ---------------------------------------------------------------------------
int CSql::GetMoneyField(CMember* pMember)
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT trio_cash, trio_point, trio_credit from tb_game_trio WHERE member_seq = %d",
                           pMember->m_nMemberSeq);
    if (res == NULL) return -1;
    if (!FetchRow(res, &row)) return -3;

    pMember->m_cMoney.m_nCash   = atoi(row["trio_cash"]);
    pMember->m_cMoney.m_nPoint  = atoi(row["trio_point"]);
    pMember->m_cMoney.m_nCredit = atoi(row["trio_credit"]);

    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateMoneyField - persist the account wallet to tb_game_trio.
// ---------------------------------------------------------------------------
int CSql::UpdateMoneyField(CMember* pMember)
{
    if (Execute("UPDATE tb_game_trio set trio_cash=%d, trio_point=%d, trio_credit=%d WHERE member_seq = %d",
                pMember->m_cMoney.m_nCash, pMember->m_cMoney.m_nPoint,
                pMember->m_cMoney.m_nCredit, pMember->m_nMemberSeq) == NULL)
        return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateSettingField - persist a character's full control/skill setting.
// ---------------------------------------------------------------------------
int CSql::UpdateSettingField(int nPlayerSeq, CSetting* pSetting)
{
    Execute(
        "UPDATE tb_game_setting set  setting_camera = %d, setting_zoom = %d, setting_target = %d, "
        "setting_camerateam = %d, setting_radian = %d,  setting_shadow = %d, setting_label = %d, "
        "setting_sound = %d, setting_music = %d, setting_invite = %d, setting_whisper = %d, "
        "setting_friend = %d,  setting_up = %d, setting_down = %d, setting_left = %d, "
        "setting_right = %d,  setting_leftshoot = %d, setting_rightshoot = %d, setting_longpass = %d, "
        "setting_shortpass = %d, setting_screen = %d, setting_tackle = %d, setting_steal = %d, "
        "setting_quick = %d, setting_quick2 = %d, setting_skill1 = %d, setting_skill2 = %d, "
        "setting_skill3 = %d, setting_skill4 = %d, setting_skill5 = %d, setting_skill_attack1 = %d, "
        "setting_skill_attack2 = %d, setting_skill_attack3 = %d, setting_skill_attack4 = %d , "
        "setting_skill_attack5 = %d , setting_skill_defence1 = %d, setting_skill_defence2 = %d, "
        "setting_skill_defence3 = %d, setting_skill_defence4 = %d , setting_skill_defence5 = %d  "
        "WHERE player_seq = %d",
        (int)pSetting->m_nCameraType, (int)pSetting->m_nCameraZoom, (int)pSetting->m_nCameraTarget,
        (int)pSetting->m_nCameraTeam, (int)pSetting->m_nRadian, (int)pSetting->m_nShadow,
        (int)pSetting->m_nLabel, (int)pSetting->m_nSound, (int)pSetting->m_nMusic,
        (int)pSetting->m_nInvite, (int)pSetting->m_nWhisper, (int)pSetting->m_nFriend,
        (int)pSetting->m_nDefineKey[0],  (int)pSetting->m_nDefineKey[1],  (int)pSetting->m_nDefineKey[2],
        (int)pSetting->m_nDefineKey[3],  (int)pSetting->m_nDefineKey[4],  (int)pSetting->m_nDefineKey[5],
        (int)pSetting->m_nDefineKey[6],  (int)pSetting->m_nDefineKey[7],  (int)pSetting->m_nDefineKey[8],
        (int)pSetting->m_nDefineKey[9],  (int)pSetting->m_nDefineKey[10], (int)pSetting->m_nDefineKey[11],
        (int)pSetting->m_nDefineKey[12], (int)pSetting->m_nDefineKey[13], (int)pSetting->m_nDefineKey[14],
        (int)pSetting->m_nDefineKey[15], (int)pSetting->m_nDefineKey[16], (int)pSetting->m_nDefineKey[17],
        pSetting->m_nAttackSkillCode[0], pSetting->m_nAttackSkillCode[1], pSetting->m_nAttackSkillCode[2],
        pSetting->m_nAttackSkillCode[3], pSetting->m_nAttackSkillCode[4],
        pSetting->m_nDefenceSkillCode[0], pSetting->m_nDefenceSkillCode[1], pSetting->m_nDefenceSkillCode[2],
        pSetting->m_nDefenceSkillCode[3], pSetting->m_nDefenceSkillCode[4], nPlayerSeq);
    return 0;
}

// ---------------------------------------------------------------------------
// UpdateLastSeqField - persist the last-selected character seq.
// ---------------------------------------------------------------------------
int CSql::UpdateLastSeqField(CMember* pMember)
{
    if (Execute("UPDATE tb_game_trio SET trio_lastseq = %d WHERE member_seq = %d",
                pMember->m_nLastSeq, pMember->m_nMemberSeq) == NULL)
        return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// ConnectServer - mark a game server online (state 2) and reset its population.
// ---------------------------------------------------------------------------
int CSql::ConnectServer(int nServerCode)
{
    if (Execute("UPDATE tb_game_server SET server_state = '%d', server_current = 0 WHERE server_code = %d",
                2, nServerCode) == NULL)
        return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// DisconnectServer - mark a game server offline (state 1).
// ---------------------------------------------------------------------------
int CSql::DisconnectServer(int nServerCode)
{
    if (Execute("UPDATE tb_game_server SET server_state = '%d' WHERE server_code = %d",
                1, nServerCode) == NULL)
        return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// SetLoginData - stamp member login time + bind the account to this server.
// ---------------------------------------------------------------------------
int CSql::SetLoginData(int nMemberSeq)
{
    if (Execute("UPDATE tb_mst_member SET member_logindate = now() WHERE member_seq = %d", nMemberSeq) == NULL)
        return -1;
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
// SetDeleteDate - stamp the account delete timestamp.
// ---------------------------------------------------------------------------
int CSql::SetDeleteDate(int nMemberSeq)
{
    if (Execute("UPDATE tb_game_trio SET trio_deletedate = now() WHERE member_seq = %d", nMemberSeq) == NULL)
        return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// SetExecuteTutorial - persist tutorial-progress flag.
// ---------------------------------------------------------------------------
int CSql::SetExecuteTutorial(int nMemberSeq, int nTutorial)
{
    if (Execute("UPDATE tb_game_trio SET trio_tutorial = %d WHERE member_seq = %d",
                nTutorial, nMemberSeq) == NULL)
        return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// SetExecuteQuest - persist quest-progress flag.
// ---------------------------------------------------------------------------
int CSql::SetExecuteQuest(int nMemberSeq, int nQuest)
{
    if (Execute("update tb_game_trio set trio_quest = %d where member_seq = %d",
                nQuest, nMemberSeq) == NULL)
        return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// InsertMemberField - create a new account (id/pass) if it does not exist, plus
// its meminfo + trio rows. Returns the new member_seq, 0 if the id already
// exists, or -1 on error.
// ---------------------------------------------------------------------------
int CSql::InsertMemberField(const char* sID, const char* sPass)
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT member_seq FROM tb_mst_member WHERE member_id = '%s'", sID);
    if (res == NULL) return -1;

    if (FetchRow(res, &row)) { FREE_RESULT(res); row.clear(); return 0; }   // already exists
    FREE_RESULT(res);

    MYSQL* hDB = Execute(
        "INSERT INTO tb_mst_member SET member_id = '%s', member_pass = '%s', member_createdate=NOW(), "
        "member_logindate='0000-00-00 00:00:00', member_deletedate='0000-00-00 00:00:00', "
        "member_blockdate='0000-00-00 00:00:00', member_releasedate='0000-00-00 00:00:00'",
        sID, sPass);
    if (hDB == NULL) return -1;

    int nSeq = mysql_insert_id(hDB);
    printf("new member seq (%d)\n", nSeq);

    if (Execute("UPDATE tb_mst_member SET member_register=%d WHERE member_seq = %d", nSeq, nSeq) == NULL)
        return -1;
    if (Execute("INSERT INTO tb_mst_meminfo SET meminfo_seq = %d, member_register=%d, meminfo_memo=''",
                nSeq, nSeq) == NULL)
        return -1;
    if (Execute("INSERT INTO tb_game_trio SET member_seq = %d", nSeq) == NULL)
        return -1;
    return nSeq;
}

// ---------------------------------------------------------------------------
// InsertItemField - add an item to a character's inventory. Returns the new
// item_seq (auto-increment), or -1 on error.
// ---------------------------------------------------------------------------
int CSql::InsertItemField(CMember* pMember, CItem* pItem)
{
    MYSQL* hDB = Execute(
        "INSERT INTO tb_game_item  (item_seq, member_seq, player_seq, item_state, item_code, "
        "item_equip, item_amount,  item_option0, item_option1, item_option2,item_option3,item_option4, "
        "item_buydate)  VALUES ('', %d, %d, '1', %d, '%d', %d, %d, %d, %d, %d, %d, now())",
        pMember->m_nMemberSeq, pMember->m_nLastSeq, pItem->m_nCode,
        (int)pItem->m_nEquipKind, (int)pItem->m_nAmount,
        pItem->m_nOptionCode[0], pItem->m_nOptionCode[1], pItem->m_nOptionCode[2],
        pItem->m_nOptionCode[3], pItem->m_nOptionCode[4]);
    if (hDB == NULL) return -1;
    return mysql_insert_id(hDB);
}

// ---------------------------------------------------------------------------
// InsertSaleLog - append a purchase/sale record to the quarterly log shard in
// the log DB (DB 1), creating the shard table on demand via CheckSampleTable.
// ---------------------------------------------------------------------------
int CSql::InsertSaleLog(CMember* pMember, CSale* pSale)
{
    CTime cTime;
    GetDBTime(&cTime);

    SetBaseDB(LOGDBFLAG);
    CheckSampleTable("tb_log_sale", cTime.m_nYear - 2000, (int)cTime.m_nQuarter);

    MYSQL* hDB = Execute(
        "INSERT INTO tb_log_sale_%02d_%02d  (sale_seq, member_seq, player_seq, object_seq, object_code,  "
        "object_kind, buy_kind, sale_kind, sale_price, sale_amount, store_kind, sale_buydate)  "
        "VALUES ('', %d, %d, %d, %d, '%d', '%d', '%d', %d, %d, '0', now())",
        cTime.m_nYear - 2000, (int)cTime.m_nQuarter,
        pMember->m_nMemberSeq, pMember->m_nLastSeq, pSale->m_nObjectSeq, pSale->m_nObjectCode,
        (int)pSale->m_nObjectKind, (int)pSale->m_nBuyKind, (int)pSale->m_nSaleKind,
        pSale->m_nPrice, pSale->m_nAmount);

    SetBaseDB(MAINDBFLAG);
    return (hDB == NULL) ? -1 : 0;
}

// ---------------------------------------------------------------------------
// InsertConnectLog - append the periodic connection-count sample to the
// monthly log shard (log DB), creating it on demand.
// ---------------------------------------------------------------------------
int CSql::InsertConnectLog(int nConnect, int nRelay, int nRoom)
{
    CTime cTime;
    GetDBTime(&cTime);

    SetBaseDB(LOGDBFLAG);
    CheckSampleTable("tb_log_connect", cTime.m_nYear - 2000, (int)cTime.m_nMonth);

    // NOTE(verify): the shard is created per-month (m_nMonth) but the INSERT
    // formats the table suffix with m_nQuarter - this quirk is copied from the
    // binary verbatim.
    MYSQL* hDB = Execute(
        "INSERT INTO tb_log_connect_%02d_%02d (connect_date, connect_server, connect_count, "
        "relay_count, room_count) VALUES (now(), %d, %d, %d, %d)",
        cTime.m_nYear - 2000, (int)cTime.m_nQuarter,
        (int)g_Config.m_nServerCode, nConnect, nRelay, nRoom);

    SetBaseDB(MAINDBFLAG);
    return (hDB == NULL) ? -1 : 0;
}

// ---------------------------------------------------------------------------
// CheckSampleTable - ensure the quarterly/monthly shard "<strTable>_YY_QQ"
// exists in the current (log) DB. If missing, copy the CREATE TABLE template
// from the sample DB (DB 2), instantiate it for this period, and clear its
// comment. Restores the original base DB before returning.
// ---------------------------------------------------------------------------
int CSql::CheckSampleTable(const char* strTable, int nArg1, int nArg2)
{
    MY_ROW row;
    int nOldDB = m_nBaseDBID;

    MYSQL_RES* res = Query("SHOW TABLES WHERE Tables_in_%s='%s_%02d_%02d';",
                           m_hDBList[m_nBaseDBID].c_str(), strTable, nArg1, nArg2);
    if (!FetchRow(res, &row))
    {
        // Shard absent: pull the CREATE TABLE template from the sample DB.
        SetBaseDB(SAMPLEDBFLAG);
        res = Query("SHOW CREATE TABLE `%s_%%02d_%%02d`;", strTable);
        if (!FetchRow(res, &row)) { SetBaseDB(nOldDB); return -1; }
        SetBaseDB(nOldDB);

        // The "Create Table" text is itself a printf template with %02d_%02d.
        if (Execute(row["Create Table"], nArg1, nArg2) == NULL) { FREE_RESULT(res); row.clear(); return -1; }
        if (Execute("ALTER TABLE %s_%02d_%02d COMMENT='';", strTable, nArg1, nArg2) == NULL)
        { FREE_RESULT(res); row.clear(); return -1; }
    }

    SetBaseDB(nOldDB);
    FREE_RESULT(res);
    row.clear();
    return 0;
}

// ---------------------------------------------------------------------------
// GetNoticeTable - load up to 5 enabled notices into pArray (seq -> text) and
// return the newest notice's UNIX timestamp as a "notice version".
// ---------------------------------------------------------------------------
int CSql::GetNoticeTable(CStringArray* pArray)
{
    MY_ROW row;
    MYSQL_RES* res = Query("SELECT notice_seq, notice_text, UNIX_TIMESTAMP(notice_date) as version "
                           "FROM tb_game_notice WHERE notice_enable=1 ORDER BY notice_date DESC LIMIT 5");
    if (res == NULL) return -1;

    int nVersion = 0;
    while (FetchRow(res, &row))
    {
        char* pTitle = new char[0x79];
        if (nVersion == 0) nVersion = atoi(row["version"]);   // newest row first
        snprintf(pTitle, 0x79, "%s", row["notice_text"]);
        (*pArray)[atoi(row["notice_seq"])] = pTitle;
    }
    FREE_RESULT(res);
    row.clear();
    return nVersion;
}

// ---------------------------------------------------------------------------
// GetEventList - load the active reward events for this server into pList.
// Returns the number of events, or a negative code on error.
// ---------------------------------------------------------------------------
int CSql::GetEventList(VectorEventList* pList)
{
    MY_ROW row;
    MYSQL_RES* res = Query(
        " SELECT event_type, reward_type, reward_value,  "
        "UNIX_TIMESTAMP(event_startdate) as event_startdate, "
        "UNIX_TIMESTAMP(event_enddate) as event_enddate  "
        "from tb_game_event where event_server = %d ",
        (int)g_Config.m_nServerCode);
    if (res == NULL) return -1;

    int nCnt = 0;
    while (FetchRow(res, &row))
    {
        CEvent* pEvent = new CEvent;
        if (pEvent == NULL) { FREE_RESULT(res); return -3; }

        pEvent->m_nEventType   = (char)ATOI(row["event_type"]);
        pEvent->m_nRewardType  = (char)ATOI(row["reward_type"]);
        pEvent->m_nRewardValue = ATOI(row["reward_value"]);
        pEvent->m_nStartTime   = ATOI(row["event_startdate"]);
        pEvent->m_nEndTime     = ATOI(row["event_enddate"]);
        pList->push_back(pEvent);
    }
    FREE_RESULT(res);
    row.clear();
    return nCnt;
}
