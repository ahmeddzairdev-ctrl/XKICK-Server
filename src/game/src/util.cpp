// util.cpp (Game) - GetCommandString / IsCommandString only. The project-agnostic
// helpers (Tokener/IsCheckSeparateString/IPTokener/ExitMessage) live in kicks_common
// (common/src/util.cpp, Util.h). The command table below differs per server, so it
// stays here. IDs are the 138 Game opcodes recovered from GetCommandString @080729b0.
#include "Main.h"
#include "LogManager.h"
#include <cstdio>

// Only CM_TCP_PING (5000) is suppressed from per-packet logging (matches binary).
int IsCommandString(int nCommand)
{
    return (nCommand != 5000) ? 1 : 0;
}

// Map a command id to its name (debug/logging). Faithful to the binary's table.
void GetCommandString(int nCommand, char* strOut)
{
    const char* s = "";
    switch (nCommand)
    {
        case 0x0065: s = "CM_STS_LOGIN"; break;
        case 0x0067: s = "CM_UPDATE_WEATHER"; break;
        case 0x0068: s = "CM_UPDATE_SCHEDULE"; break;
        case 0x0069: s = "CM_UPDATE_NOTICE"; break;
        case 0x006A: s = "CM_SEND_BROADCAST"; break;
        case 0x07D0: s = "CM_GAME_LOGIN"; break;
        case 0x07D1: s = "CM_GAME_EXIT"; break;
        case 0x07D2: s = "CM_UDP_CONFIRM"; break;
        case 0x07D3: s = "CM_NOTICE_LIST"; break;
        case 0x07D4: s = "CM_EVENT_LIST"; break;
        case 0x0834: s = "CM_PLAYER_INFO"; break;
        case 0x0835: s = "CM_ITEM_INFO"; break;
        case 0x0836: s = "CM_SKILL_INFO"; break;
        case 0x0837: s = "CM_TRAINING_INFO"; break;
        case 0x0838: s = "CM_CEREMONY_INFO"; break;
        case 0x0839: s = "CM_QUEST_INFO"; break;
        case 0x083A: s = "CM_CARD_INFO"; break;
        case 0x083D: s = "CM_PLAYERINFO_END"; break;
        case 0x083E: s = "CM_SALE_LIST"; break;
        case 0x0898: s = "CM_ROOM_INFO"; break;
        case 0x0899: s = "CM_ROOM_LIST"; break;
        case 0x089A: s = "CM_LOBBY_LIST"; break;
        case 0x089B: s = "CM_CREATE_ROOM"; break;
        case 0x089C: s = "CM_SET_ROOM"; break;
        case 0x089D: s = "CM_CHOICE_ROOM"; break;
        case 0x089E: s = "CM_QUICK_ROOM"; break;
        case 0x08FC: s = "CM_LEAVE_ROOM"; break;
        case 0x08FD: s = "CM_CHANGE_PARENT"; break;
        case 0x08FE: s = "CM_CHANGE_JANG"; break;
        case 0x08FF: s = "CM_ATHLETE_INFO"; break;
        case 0x0900: s = "CM_ATHLETE_END"; break;
        case 0x0901: s = "CM_ROBOT_INFO"; break;
        case 0x0902: s = "CM_ROBOT_END"; break;
        case 0x0903: s = "CM_CHANGE_GROUND"; break;
        case 0x0904: s = "CM_CHANGE_BALL"; break;
        case 0x0905: s = "CM_CHANGE_WEATHER"; break;
        case 0x0906: s = "CM_INVITE_PLAYER"; break;
        case 0x0907: s = "CM_CARDBOT_INFO"; break;
        case 0x0908: s = "CM_CARDBOT_END"; break;
        case 0x0909: s = "CM_BLACKLIST_INFO"; break;
        case 0x090A: s = "CM_ADD_BLACKLIST"; break;
        case 0x090B: s = "CM_DEL_BLACKLIST"; break;
        case 0x090C: s = "CM_BUDDY_INFO"; break;
        case 0x090D: s = "CM_ADD_BUDDY"; break;
        case 0x090E: s = "CM_DEL_BUDDY"; break;
        case 0x090F: s = "CM_WEEKLY_RECORD"; break;
        case 0x0910: s = "CM_WEEKLY_RANKING"; break;
        case 0x0911: s = "CM_FORCE_OUT"; break;
        case 0x0960: s = "CM_GAME_READY"; break;
        case 0x0961: s = "CM_GAME_START"; break;
        case 0x0962: s = "CM_GAME_COUNT"; break;
        case 0x0963: s = "CM_GAME_LOAD"; break;
        case 0x0964: s = "CM_GAME_PLAY"; break;
        case 0x0965: s = "CM_GAME_RESULT"; break;
        case 0x0966: s = "CM_GAME_END"; break;
        case 0x0967: s = "CM_LEVEL_UP"; break;
        case 0x0968: s = "CM_AUTOPILOT_MODE"; break;
        case 0x096B: s = "CM_GOALIN_TCP"; break;
        case 0x096C: s = "CM_SWITCH_VALUE"; break;
        case 0x096D: s = "CM_PIT_IN"; break;
        case 0x09C5: s = "CM_CHANGE_TEAM"; break;
        case 0x09C6: s = "CM_CHANGE_POSITION"; break;
        case 0x0A29: s = "CM_CHANGE_MENT"; break;
        case 0x0A8D: s = "CM_GROWUP_CHARACTER"; break;
        case 0x0A8E: s = "CM_QUEST_REWARD"; break;
        case 0x0A8F: s = "CM_MISSION_REWARD"; break;
        case 0x0A90: s = "CM_EXECUTE_QUEST"; break;
        case 0x0C1C: s = "CM_SHOPITEM_LIST"; break;
        case 0x0C1D: s = "CM_UPDATE_ITEM"; break;
        case 0x0C1E: s = "CM_EQUIP_ITEM"; break;
        case 0x0C1F: s = "CM_DIVEST_ITEM"; break;
        case 0x0C20: s = "CM_BUY_ITEM"; break;
        case 0x0C21: s = "CM_GIFT_ITEM"; break;
        case 0x0C22: s = "CM_EXCHANGE_ITEM"; break;
        case 0x0C26: s = "CM_BUY_RANDOMITEM"; break;
        case 0x0C27: s = "CM_RANDOMSHOPITEM_LIST"; break;
        case 0x0C28: s = "CM_ENCHANT_ITEM"; break;
        case 0x0C29: s = "CM_REFRESH_SHOP"; break;
        case 0x0C2A: s = "CM_GIFT_LIST"; break;
        case 0x0C2B: s = "CM_GET_GIFT"; break;
        case 0x0C2C: s = "CM_SELL_ITEM"; break;
        case 0x0C2D: s = "CM_REPAIR_ITEM"; break;
        case 0x0C80: s = "CM_SHOPSKILL_LIST"; break;
        case 0x0C81: s = "CM_UPDATE_SKILL"; break;
        case 0x0C82: s = "CM_EQUIP_SKILL"; break;
        case 0x0C83: s = "CM_DIVEST_SKILL"; break;
        case 0x0C84: s = "CM_BUY_SKILL"; break;
        case 0x0CE4: s = "CM_SHOPTRAINING_LIST"; break;
        case 0x0CE5: s = "CM_UPDATE_TRAINING"; break;
        case 0x0CE8: s = "CM_BUY_TRAINING"; break;
        case 0x0D48: s = "CM_SHOPCEREMONY_LIST"; break;
        case 0x0D49: s = "CM_UPDATE_CEREMONY"; break;
        case 0x0D4A: s = "CM_EQUIP_CEREMONY"; break;
        case 0x0D4B: s = "CM_DIVEST_CEREMONY"; break;
        case 0x0D4C: s = "CM_BUY_CEREMONY"; break;
        case 0x0DAC: s = "CM_QUEST_LIST"; break;
        case 0x0DAD: s = "CM_UPDATE_QUEST"; break;
        case 0x0DAE: s = "CM_CREATE_QUEST"; break;
        case 0x0E11: s = "CM_UPDATE_CARD"; break;
        case 0x0E12: s = "CM_EQUIP_CARD"; break;
        case 0x0E13: s = "CM_DIVEST_CARD"; break;
        case 0x0E14: s = "CM_CREDIT_CARD"; break;
        case 0x0E15: s = "CM_BUY_CARDBOOSTER"; break;
        case 0x0E17: s = "CM_CARD_ENTRY"; break;
        case 0x0E19: s = "CM_REWARD_CARD"; break;
        case 0x0E75: s = "CM_MIX_ITEM1"; break;
        case 0x0E76: s = "CM_MIX_ITEM2"; break;
        case 0x0E7F: s = "CM_MIX_CARD1"; break;
        case 0x0E80: s = "CM_MIX_CARD2"; break;
        case 0x1068: s = "CM_TEAM_INFO"; break;
        case 0x106B: s = "CM_CREATE_TEAM"; break;
        case 0x106C: s = "CM_SET_TEAM"; break;
        case 0x106D: s = "CM_CHOICE_TEAM"; break;
        case 0x106E: s = "CM_QUICK_TEAM"; break;
        case 0x106F: s = "CM_TEAM_POSITION"; break;
        case 0x10CC: s = "CM_LEAVE_TEAM"; break;
        case 0x10CE: s = "CM_CHANGE_TEAMJANG"; break;
        case 0x10CF: s = "CM_TEAMONE_INFO"; break;
        case 0x10D0: s = "CM_TEAMONE_END"; break;
        case 0x1130: s = "CM_TEAM_READY"; break;
        case 0x1131: s = "CM_APPLY_MATCH"; break;
        case 0x1132: s = "CM_MATCH_ROOM"; break;
        case 0x120D: s = "CM_FACULTY_RESET"; break;
        case 0x1217: s = "CM_SKILL_SLOTT"; break;
        case 0x1221: s = "CM_CASH_COUPON"; break;
        case 0x1222: s = "CM_POINT_COUPON"; break;
        case 0x122B: s = "CM_CHANGE_NAME"; break;
        case 0x1388: s = "CM_TCP_PING"; break;
        case 0x1389: s = "CM_SEND_MESSAGE"; break;
        case 0x138A: s = "CM_RAISE_FACULTY"; break;
        case 0x138B: s = "CM_CHANGE_SETTING"; break;
        case 0x138C: s = "CM_SETTING_INFO"; break;
        case 0x138D: s = "CM_PING_SPEED"; break;
        case 0x138E: s = "CM_SAVE_SPEED"; break;
        case 0x1F40: s = "CM_OPERATION_TOOL"; break;
        case 0x1F41: s = "CM_SERVER_TOOL"; break;
        case 0x1F42: s = "CM_ROOM_TOOL"; break;
        case 0x1F43: s = "CM_USER_TOOL"; break;
        // UDP-only (not part of the TCP GetCommandString table, kept for logging):
        case 0x2328: s = "CM_UDP_PUNCHING"; break;   // 9000
        default: return;
    }
    snprintf(strOut, 0x1e, "%s", s);
}
