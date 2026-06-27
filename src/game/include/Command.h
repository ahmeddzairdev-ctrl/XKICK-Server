// =============================================================================
// Command.h (Game) - GAME-ONLY protocol command IDs, recovered byte-for-byte from
// XKICK_Game1's GetCommandString (@0x080729b0) string table. These IDs are part
// of the FROZEN wire format (the live client sends them); do not renumber.
//
// Command IDs shared with the Certify server live in the common Protocol.h (the
// single source of truth); only the IDs unique to the Game server remain here, to
// avoid double-definition. Protocol.h is always included alongside this header.
//
// Grouping by leading value: 0x006x server-to-server (STS); 0x07Dx login/lobby;
// 0x08xx player-info / room; 0x09xx room-detail + game lifecycle; 0x0Axx
// grow/quest; 0x0Cxx-0x0Exx shops/cards/mix; 0x10xx-0x11xx team/match; 0x12xx
// faculty/coupon/name; 0x13xx misc (ping/message/setting); 0x1Fxx admin tools.
// =============================================================================
#ifndef _GAME_COMMAND_H_
#define _GAME_COMMAND_H_

#include "Protocol.h"   // shared command IDs (CM_*) + wire structs

// ---- server-to-server (STS) ----

// ---- login / lobby ----

// ---- player info ----
#define CM_CARD_INFO           0x083A   // 2106
#define CM_PLAYERINFO_END      0x083D   // 2109

// ---- room ----

// ---- room detail ----
#define CM_CHANGE_WEATHER      0x0905   // 2309
#define CM_CARDBOT_INFO        0x0907   // 2311
#define CM_CARDBOT_END         0x0908   // 2312
#define CM_BLACKLIST_INFO      0x0909   // 2313
#define CM_ADD_BLACKLIST       0x090A   // 2314
#define CM_DEL_BLACKLIST       0x090B   // 2315
#define CM_BUDDY_INFO          0x090C   // 2316
#define CM_ADD_BUDDY           0x090D   // 2317
#define CM_DEL_BUDDY           0x090E   // 2318
#define CM_WEEKLY_RECORD       0x090F   // 2319
#define CM_WEEKLY_RANKING      0x0910   // 2320

// ---- game lifecycle ----
#define CM_AUTOPILOT_MODE      0x0968   // 2408
#define CM_GOALIN_TCP          0x096B   // 2411
#define CM_SWITCH_VALUE        0x096C   // 2412
#define CM_PIT_IN              0x096D   // 2413

// ---- team-position / mention ----

// ---- grow / quest reward ----

// ---- item shop ----
#define CM_BUY_RANDOMITEM      0x0C26   // 3110
#define CM_RANDOMSHOPITEM_LIST 0x0C27   // 3111
#define CM_ENCHANT_ITEM        0x0C28   // 3112
#define CM_REFRESH_SHOP        0x0C29   // 3113
#define CM_GIFT_LIST           0x0C2A   // 3114
#define CM_GET_GIFT            0x0C2B   // 3115
#define CM_SELL_ITEM           0x0C2C   // 3116
#define CM_REPAIR_ITEM         0x0C2D   // 3117

// ---- skill shop ----

// ---- training shop ----

// ---- ceremony shop ----

// ---- quest ----

// ---- card ----
#define CM_UPDATE_CARD         0x0E11   // 3601
#define CM_EQUIP_CARD          0x0E12   // 3602
#define CM_DIVEST_CARD         0x0E13   // 3603
#define CM_CREDIT_CARD         0x0E14   // 3604
#define CM_BUY_CARDBOOSTER     0x0E15   // 3605
#define CM_CARD_ENTRY          0x0E17   // 3607
#define CM_REWARD_CARD         0x0E19   // 3609

// ---- mix (item/card fusion) ----
#define CM_MIX_ITEM1           0x0E75   // 3701
#define CM_MIX_ITEM2           0x0E76   // 3702
#define CM_MIX_CARD1           0x0E7F   // 3711
#define CM_MIX_CARD2           0x0E80   // 3712

// ---- team ----
#define CM_TEAM_INFO           0x1068   // 4200
#define CM_CREATE_TEAM         0x106B   // 4203
#define CM_SET_TEAM            0x106C   // 4204
#define CM_CHOICE_TEAM         0x106D   // 4205
#define CM_QUICK_TEAM          0x106E   // 4206
#define CM_TEAM_POSITION       0x106F   // 4207
#define CM_LEAVE_TEAM          0x10CC   // 4300
#define CM_CHANGE_TEAMJANG     0x10CE   // 4302
#define CM_TEAMONE_INFO        0x10CF   // 4303
#define CM_TEAMONE_END         0x10D0   // 4304

// ---- team match ----
#define CM_TEAM_READY          0x1130   // 4400
#define CM_APPLY_MATCH         0x1131   // 4401
#define CM_MATCH_ROOM          0x1132   // 4402

// ---- faculty / coupon / name ----
#define CM_FACULTY_RESET       0x120D   // 4621
#define CM_SKILL_SLOTT         0x1217   // 4631
#define CM_CASH_COUPON         0x1221   // 4641
#define CM_POINT_COUPON        0x1222   // 4642
#define CM_CHANGE_NAME         0x122B   // 4651

// ---- misc ----
#define CM_PING_SPEED          0x138D   // 5005
#define CM_SAVE_SPEED          0x138E   // 5006

// ---- UDP-only (P2P, processed on the UDP socket, not the TCP dispatch table) ----
// (CM_UDP_CONFIRM 0x07D2 above is a TCP handler, despite the name.)

// ---- admin tools ----
#define CM_OPERATION_TOOL      0x1F40   // 8000
#define CM_SERVER_TOOL         0x1F41   // 8001
#define CM_ROOM_TOOL           0x1F42   // 8002
#define CM_USER_TOOL           0x1F43   // 8003

#endif // _GAME_COMMAND_H_
