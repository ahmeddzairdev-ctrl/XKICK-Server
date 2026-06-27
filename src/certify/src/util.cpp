// util.cpp - small helpers (reconstructed from XKICK_Certify).
// GetCommandString/IsCommandString are used by the packet logger (SendMember).
// Command ids are the Certify protocol opcodes recovered from the binary.
#include "Main.h"
#include "LogManager.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// Only CM_TCP_PING (5000) is suppressed from per-packet logging.
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
        case 0x067: s = "CM_UPDATE_WEATHER";   break; // STS
        case 0x068: s = "CM_UPDATE_SCHEDULE";  break; // STS
        case 0x069: s = "CM_UPDATE_NOTICE";    break; // STS
        case 0x06A: s = "CM_SEND_BROADCAST";   break; // STS
        case 1000:  s = "CM_CERTIFY_LOGIN";    break; // 0x3E8
        case 0x3E9: s = "CM_INSTANT_LOGIN";    break;
        case 0x3EA: s = "CM_CERTIFY_EXIT";     break;
        case 0x3EB: s = "CM_DRAWFORCE_PLAYER"; break;
        case 0x44C: s = "CM_MEMBER_INFO";      break;
        case 0x4B0: s = "CM_CHARACTER_INFO";   break;
        case 0x4B1: s = "CM_CHARACTER_END";    break;
        case 0x4B2: s = "CM_CREATE_CHARACTER"; break;
        case 0x4B3: s = "CM_DELETE_CHARACTER"; break;
        case 0x4B4: s = "CM_CHOICE_CHARACTER"; break;
        case 0x4B5: s = "CM_CHARACTER_SEARCH"; break;
        case 0x514: s = "CM_SERVER_LIST";      break;
        case 0x515: s = "CM_CHOICE_SERVER";    break;
        case 0x578: s = "CM_EXECUTE_TUTORIAL"; break;
        case 0x7D3: s = "CM_NOTICE_LIST";      break;
        case 0x7D4: s = "CM_EVENT_LIST";       break;
        case 0xA90: s = "CM_EXECUTE_QUEST";    break;
        case 5000:  s = "CM_TCP_PING";         break; // 0x1388
        case 0x138B: s = "CM_CHANGE_SETTING";  break;
        case 0x138C: s = "CM_SETTING_INFO";    break;
        default: return;
    }
    snprintf(strOut, 0x1e, "%s", s);
}

// NOTE: the project-agnostic helpers (Tokener / IsCheckSeparateString /
// IPTokener / ExitMessage) moved to kicks_common (common/src/util.cpp,
// declared in Util.h) so Certify and Game share one copy.
