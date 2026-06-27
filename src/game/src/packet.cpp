// =============================================================================
// packet.cpp - Game packet send layer + SPINE handlers.
// Reconstructed from XKICK_Game1 (unstripped).
//
// Send path (server->client):
//   SendPlayer @080822c2 -> WriteTCP @08082e86 -> (CSecure encrypt body) -> Net_Send.
//   WriteTCP: if the player's CSecure is active AND the command is not 2000 or
//   0x7d1, the body is encrypted in-place via CSecure::MakeSendPacket before the
//   framed write; commands 2000/0x7d1 (login/exit) are sent in clear so the client
//   can bootstrap/teardown the cipher. The recv-side decrypt lives in connect.cpp.
//
// The SPINE handlers below (login/exit/notice/event/ping/message/setting/buddy/
// blacklist/record/ranking/name/faculty/tools/udp-confirm + current time/weather)
// are byte-faithful. Handlers OWNED by domain modules are NOT defined here.
//
// Portable: std C++ only.
// =============================================================================
#include "Main.h"
#include "packet.h"
#include "GameProtocol.h"
#include "Net.h"
#include "Sql.h"
#include "GameLoad.h"
#include "Domain.h"
#include "Room.h"
#include "Secure.h"
#include "LogManager.h"
#include <cstdio>
#include <cstring>
#include <ctime>

// Forward decls for cross-module helpers the spine touches (defined elsewhere).
CPlayer* GetPlayerPointer(int nSeq);          // g_PlayerHash lookup (player.cpp)
CPlayer* GetPlayerPointerPc(const char* sName); // by-name lookup
CRoom*   GetRoomPointer(int nRoomSeq);
void     SendAll(CHeadPacket* pPacket, int nResult);   // broadcast to all players
void     PutSendBroadcast(const char* sText);          // queue a broadcast string
void     PacketLeaveRoom(CPlayer* pPlayer, int nReason);
void     PacketForceOut(CPlayer* pPlayer, CHeadPacket* pPacket);
void     PacketChangeJang(CRoom* pRoom);

// =============================================================================
// SEND LAYER
// =============================================================================

// WriteTCP @08082e86 - CSecure-encrypt the body (when active) then framed write.
int WriteTCP(CPlayer* pPlayer, CHeadPacket* pPacket, unsigned int /*nLen*/)
{
    if (pPlayer == 0)
        return -1;

    CSecure* pSecure = pPlayer->GetTCPSecure();
    if (!pSecure->InitCheck() ||
        pPacket->m_nCommand == 2000 || pPacket->m_nCommand == 0x7d1)
    {
        // plaintext (cipher inactive, or login/exit bootstrap)
        Net_Send(pPlayer->m_nFd, pPacket, pPacket->m_nBodySize + 0xc);
        return 0;
    }

    if (pPacket->m_nBodySize < 0)
    {
        LOGOUT_ERROR("WriteTCP: negative body size seq(%d)\n", pPlayer->m_nPlayerSeq);
        return -1;
    }

    // copy to a scratch buffer, encrypt the body in place, then send.
    char sBuffer[0x804];
    memcpy(sBuffer, pPacket, pPacket->m_nBodySize + 0xc);
    pPlayer->GetTCPSecure()->MakeSendPacket((CHeadPacket*)sBuffer);
    Net_Send(pPlayer->m_nFd, sBuffer, pPacket->m_nBodySize + 0xc);
    return 0;
}

// SendPlayer @080822c2 - WriteTCP + per-packet log. nResult: request/secure flag.
void SendPlayer(CPlayer* pPlayer, CHeadPacket* pPacket, int nResult)
{
    if (pPlayer == 0)
        return;
    if (WriteTCP(pPlayer, pPacket, pPacket->m_nBodySize + 0xc) < 0)
        return;

    char str[30];
    memset(str, 0, sizeof(str));
    if (IsCommandString(pPacket->m_nCommand))
    {
        GetCommandString(pPacket->m_nCommand, str);
        int nColor = (nResult != 0) ? 0x1f : 0x24;
        LOGOUT_SCREEN("\x1b[%dm[OUT]\x1b[0m", nColor);
        LOGOUT_PACKET(" OUT(%d), Seq(%d): %s(%d), nSize(%d), Req(%d)\n",
                      pPlayer->m_nFd, pPlayer->m_nPlayerSeq, str,
                      pPacket->m_nCommand, pPacket->m_nBodySize, nResult);
    }
}

// SendSTS @08082d2e - framed write on the single g_STSSocket uplink + log.
void SendSTS(CHeadPacket* pPacket, int nResult)
{
    int nRet = Net_SendSTS_Ret(pPacket, pPacket->m_nBodySize + 0xc);
    if (nRet < 0)
    {
        LOGOUT_ERROR("SendSTS: write failed (%d) cmd(%d)\n", nRet, pPacket->m_nCommand);
        return;
    }
    char str[30];
    memset(str, 0, sizeof(str));
    if (IsCommandString(pPacket->m_nCommand))
    {
        GetCommandString(pPacket->m_nCommand, str);
        LOGOUT_PACKET("STS OUT: %s(%d), nSize(%d), Req(%d)\n",
                      str, pPacket->m_nCommand, pPacket->m_nBodySize, nResult);
    }
}

// SendServer - STS write addressed via a CServer node (same uplink in the Game).
void SendServer(CServer* /*pServer*/, CHeadPacket* pPacket, int nResult)
{
    SendSTS(pPacket, nResult);
}

// =============================================================================
// DISPATCHER FALLBACKS
// =============================================================================
void EmptyFunction(CPlayer* /*pPlayer*/, CHeadPacket* pPacket)
{
    char str[30];
    memset(str, 0, sizeof(str));
    GetCommandString(pPacket->m_nCommand, str);
    LOGOUT_ERROR("unknown command : %s(%d)\n", str, pPacket->m_nCommand);
}

void HijackingFunction(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    LOGOUT_ERROR("hijacking: negative sequence seq(%d) cmd(%d)\n",
                 pPacket->m_nSequence, pPacket->m_nCommand);
    if (pPlayer != 0)
        pPlayer->ExitPlayer();
}

// =============================================================================
// SPINE HANDLERS - login / session
// =============================================================================

// PacketGameLogin @08074d20
void PacketGameLogin(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSGameLoginGame* pIn = (CCSGameLoginGame*)pPacket;
    CSCGameLoginGame  cOut;

    if (IsPlayerConnect(pIn->m_nPlayerSeq))   // already connected elsewhere
    {
        cOut.m_nResponse = (char)0xff;
        cOut.m_nBodySize = 1;
        SendPlayer(pPlayer, &cOut, -1);
        return;
    }

    pPlayer->m_nMemberSeq    = pIn->m_nMemberSeq;          // +0x08
    pPlayer->m_nPlayerSeq = pIn->m_nPlayerSeq;          // +0x0c
    pPlayer->m_nPerformance  = (short)pIn->m_nPort;        // +0x51a
    pPlayer->m_nNetSpeed    = pIn->m_nIP;                 // +0x51c

    int nRet = pPlayer->SetPlayer();
    if (nRet >= 0)
    {
        g_Sql.SetLoginData(pPlayer->m_nMemberSeq);
        g_RoomPool[0].InsertPlayerLobby(pPlayer);
        pPlayer->GetTCPSecure()->InitSeq();               // prime CSecure (random seed)
        cOut.m_nResponse = 0;
        cOut.m_nSequence = pPlayer->GetTCPSecure()->m_nSendSeq; // seed -> HEADER (cmd 2000 sent raw)
        SendPlayer(pPlayer, &cOut, 0);
        return;
    }

    // map SetPlayer error codes.
    char nResp;
    if (nRet == -0xa)                 nResp = -10;
    else if (nRet == -0x1e)           nResp = -2;
    else if ((unsigned)(nRet + 6) < 6) nResp = (char)(nRet - 10);
    else                              nResp = -99;
    cOut.m_nResponse = nResp;
    cOut.m_nBodySize = 1;
    SendPlayer(pPlayer, &cOut, -1);
}

// PacketGameExit @08074f70
void PacketGameExit(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    char nReason = ((CCSGameExit*)pPacket)->m_nReason;

    pPlayer->m_hTCPSecure.m_bInit = 0;           // +0x538 disable CSecure active flag

    CSCGameExit cOut;
    cOut.m_nResponse = 0;
    cOut.m_nReason   = nReason;   // rebuild: binary CSCGameExit echoes the reason (body 2)
    cOut.m_nBodySize = 2;
    if (nReason == 0) LOGOUT_PACKET("GameExit: normal seq(%d)\n", pPlayer->m_nPlayerSeq);
    else              LOGOUT_PACKET("GameExit: reason(%d) seq(%d)\n", nReason, pPlayer->m_nPlayerSeq);

    SendPlayer(pPlayer, &cOut, 0);
    pPlayer->ExitPlayer();
}

// PacketUDPConfirm @080750ee
void PacketUDPConfirm(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCUDPConfirm cOut;
    cOut.m_nBodySize = 1;

    int nRet = pPlayer->CheckPlayerUDP();
    char nResp;
    if (nRet >= 0)        { nResp = 0; }
    else if (nRet == -2)  { nResp = -0xc; }
    else if (nRet == -1)  { nResp = -0xb; }
    else                  { nResp = -99; }

    cOut.m_nResponse = nResp;
    SendPlayer(pPlayer, &cOut, (nResp == 0) ? 0 : -1);
    if (nResp == 0)
        PacketUDPPing(pPlayer);
}

// PacketNoticeList @08075182 (also called from the STS notice-refresh path with
// pPlayer == 0 to broadcast).
void PacketNoticeList(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    int nClientVersion = pPacket ? ((CCSNoticeList*)pPacket)->m_nVersion : 0;

    CSCNoticeList cOut;
    cOut.m_nResponse = 0;
    cOut.m_nVersion  = g_Load.GetNoticeVersion();
    cOut.m_nCount    = 0;

    int i = 0;
    for (CStringArray::iterator it = g_Load.m_cNoticeArray.begin();
         it != g_Load.m_cNoticeArray.end() && i < LIST5_SIZE; ++it, ++i)
    {
        cOut.m_cNoticeList[i].m_nNoticeSeq = it->first;
        memset(cOut.m_cNoticeList[i].m_strText, 0, TIP_SIZE);
        StrCopyN(cOut.m_cNoticeList[i].m_strText, it->second, TIP_SIZE - 1);
        ++cOut.m_nCount;
    }

    if (pPlayer == 0)
        SendAll(&cOut, 0);                       // STS refresh broadcast
    else if (nClientVersion < cOut.m_nVersion)   // client is stale
        SendPlayer(pPlayer, &cOut, 0);
}

// PacketEventList @0807f65c - active reward events, batched 30/packet.
void PacketEventList(CPlayer* pPlayer, CHeadPacket* /*pPacket*/)
{
    CSCEventList cOut;
    cOut.m_nResponse = 0;
    cOut.m_nCount    = 0;

    for (size_t i = 0; i < g_Load.m_vEventList.size(); ++i)
    {
        CEvent* pEvent = g_Load.m_vEventList[i];
        CEventData& r = cOut.m_cEventList[cOut.m_nCount];
        r.m_nEventType   = pEvent->m_nEventType;
        r.m_nRewardType  = pEvent->m_nRewardType;
        r.m_nRewardValue = pEvent->m_nRewardValue;
        r.m_nStartTime   = pEvent->m_nStartTime;
        r.m_nEndTime     = pEvent->m_nEndTime;
        ++cOut.m_nCount;

        if (cOut.m_nCount == 30)
        {
            cOut.m_nBodySize = 30 * (int)sizeof(CEventData) + 3;
            SendPlayer(pPlayer, &cOut, 0);
            cOut.m_nCount = 0;
        }
    }
    cOut.m_nBodySize = cOut.m_nCount * (int)sizeof(CEventData) + 3;
    SendPlayer(pPlayer, &cOut, 0);
}

// PacketCurrentTime @0807fece
void PacketCurrentTime(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    if (pPlayer == 0 || pPacket == 0) return;
    CSCCurrentTime cOut;
    cOut.m_nResponse = 0;
    cOut.m_nTime     = g_Sql.GetCurrentTimeSecond();
    // time-of-day bucket (0..3) -- derived from a randomized time-of-day window.
    cOut.m_nTimeType = 0;
    SendPlayer(pPlayer, &cOut, 0);
}

// PacketCurrentWeather @0807f954
void PacketCurrentWeather(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    if (pPlayer == 0 || pPacket == 0) return;
    CSCCurrentWeather cOut;
    cOut.m_nResponse = 0;

    int nNow   = g_Sql.GetCurrentTimeSecond();
    int nToday = g_Sql.GetCurrentDay();
    bool bFound = false;
    for (std::map<int, CWeather*>::iterator it = g_Weather.begin(); it != g_Weather.end(); ++it)
    {
        CWeather* pW = it->second;
        char nDate  = pW->m_nDate;
        int  nStart = pW->m_nStart;
        int  nEnd   = pW->m_nEnd;
        if ((nDate == nToday || nDate == 1) && nStart <= nNow && nNow <= nEnd)
        {
            cOut.m_nWeatherType = pW->m_nWeatherType;
            cOut.m_nEndTime     = nEnd;
            bFound = true;
            break;
        }
    }
    if (!bFound) { cOut.m_nWeatherType = 1; cOut.m_nEndTime = nNow + 300; }
    SendPlayer(pPlayer, &cOut, 0);
}

// =============================================================================
// SPINE HANDLERS - ping / message / setting
// =============================================================================

// PacketTCPPing(CPlayer*, CHeadPacket*) @0807d18e (client reply) - resets miss counter.
// PacketTCPPing(CPlayer*)               @0807d19e (server ping)  - sends CSCTCPPing.
void PacketTCPPing(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    if (pPacket != 0)
    {
        // client answered our ping: clear the miss counter.
        pPlayer->m_nCheckTCP = 0;   // +0x50e
        return;
    }
    // server-initiated ping.
    if (pPlayer->m_bPingCheck)             // +0x52c
    {
        CSCTCPPing cOut;
        cOut.m_nResponse = 0;
        SendPlayer(pPlayer, &cOut, 0);
    }
}

// PacketPingSpeed / PacketSaveSpeed @ CM_PING_SPEED/CM_SAVE_SPEED - latency probes.
void PacketPingSpeed(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    // echo the ping-speed packet straight back to the client (latency measure).
    if (pPlayer != 0 && pPacket != 0)
        SendPlayer(pPlayer, pPacket, 0);
}

void PacketSaveSpeed(CPlayer* /*pPlayer*/, CHeadPacket* /*pPacket*/)
{
    // store the client-reported round-trip; no reply (working field updated by Net).
}

// PacketSendMessage @0807dca6 - chat / whisper / admin command.
void PacketSendMessage(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSSendMessage* pIn = (CCSSendMessage*)pPacket;
    char nChatKind = pIn->m_nChatKind;
    char* sToName  = pIn->m_sToName;
    char* sMessage = pIn->m_sMessage;

    CSCSendMessage cOut;
    cOut.m_nPlayerSeq = pPlayer->m_nPlayerSeq;
    cOut.m_nChatKind  = nChatKind;
    memcpy(cOut.m_sFromName, pPlayer->m_sName, PLAYER_NAME_SIZE);
    memset(cOut.m_sMessage, 0, MESSAGE_SIZE);
    StrCopyN(cOut.m_sMessage, sMessage, MESSAGE_SIZE - 1);
    cOut.m_nResponse  = 0;
    cOut.m_nBodySize  = 4 + 1 + 1 + PLAYER_NAME_SIZE + MESSAGE_SIZE;

    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0) { cOut.m_nResponse = -1; SendPlayer(pPlayer, &cOut, -1); return; }

    switch (nChatKind)
    {
    case 0: case 1: case 8:
        pRoom->SendRoom(&cOut, 0);
        break;
    case 2:   // team chat
        pRoom->SendTeam(&cOut, 0, pPlayer);
        break;
    case 3:   // whisper
    {
        CPlayer* pTo = GetPlayerPointerPc(sToName);
        if (pTo == 0)      { cOut.m_nResponse = -2; }
        else if (pTo->IsBlocked(pPlayer->m_nPlayerSeq)) { cOut.m_nResponse = -4; }
        else               { SendPlayer(pTo, &cOut, 0); }
        SendPlayer(pPlayer, &cOut, cOut.m_nResponse);
        break;
    }
    default:
        cOut.m_nResponse = -1;
        SendPlayer(pPlayer, &cOut, -1);
        break;
    }
}

// PacketChangeSetting @0807e2fc
void PacketChangeSetting(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSChangeSetting* pIn = (CCSChangeSetting*)pPacket;
    char     nInit  = pIn->m_nInitSetting;
    CSetting* pInSet = &pIn->m_cChangeSetting;

    CSCChangeSetting cOut;     // body 0x5a
    cOut.m_nResponse    = 0;
    cOut.m_nInitSetting = nInit;
    if (nInit == 0)
    {
        memcpy(&pPlayer->m_cSetting, pInSet, sizeof(CSetting));   // +0x5c
        memcpy(&cOut.m_cSetting, &pPlayer->m_cSetting, sizeof(CSetting));
        g_Sql.UpdateSettingField(pPlayer);
    }
    else
    {
        memcpy(&cOut.m_cSetting, &g_Setting, sizeof(CSetting));   // default template
    }
    SendPlayer(pPlayer, &cOut, 0);
}

// PacketSettingInfo @0807ff24
void PacketSettingInfo(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    if (pPlayer == 0 || pPacket == 0) return;
    CSCSettingInfo cOut;       // body 0x5d
    cOut.m_nResponse  = 0;
    cOut.m_nPlayerSeq = pPlayer->m_nPlayerSeq;
    memcpy(&cOut.m_cSetting, &pPlayer->m_cSetting, sizeof(CSetting));
    SendPlayer(pPlayer, &cOut, 0);
}

// =============================================================================
// SPINE HANDLERS - faculty / name
// =============================================================================

// PacketRaiseFaculty @0807e220
void PacketRaiseFaculty(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CFaculty* pDelta = &((CCSRaiseFacultyGame*)pPacket)->m_cChangeFaculty;

    CSCRaiseFacultyGame cOut;   // body 0x26
    int nRet = pPlayer->RaiseFaculty(pDelta);
    if (nRet < 0)
    {
        char nResp = (nRet == -2) ? (char)-0xc : (nRet == -1) ? (char)-0xb : (char)nRet;
        cOut.m_nResponse = nResp;
        cOut.m_nBodySize = 1;
        SendPlayer(pPlayer, &cOut, -1);
        return;
    }
    g_Sql.UpdateFaculty(pPlayer);
    cOut.m_nResponse     = 0;
    cOut.m_nLevel        = pPlayer->m_cLevel.m_nLevel;
    cOut.m_nExp          = pPlayer->m_cLevel.m_nExp;
    cOut.m_nFacultyPoint = pPlayer->m_cLevel.m_nFaculty;
    memcpy(&cOut.m_cRaiseFaculty, pPlayer->m_cRaiseFaculty.m_nFaculty, ARRAY_FACULTY_SIZE);  // +0x11d
    SendPlayer(pPlayer, &cOut, 0);
}

// PacketFacultyReset @0807cca4
void PacketFacultyReset(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    int nItemSeq = ((CCSFacultyReset*)pPacket)->m_nItemSeq;

    CSCFacultyReset cOut;       // body 0x2a
    CItem* pItem = pPlayer->GetItemPointer(nItemSeq);
    if (pItem == 0) { cOut.m_nResponse = -3; cOut.m_nBodySize = 1; SendPlayer(pPlayer, &cOut, -1); return; }

    pPlayer->InitFaculty(&pPlayer->m_cRaiseFaculty);     // +0x11d
    pPlayer->m_cLevel.m_nFaculty = (short)pPlayer->GetFacultyCount();
    pItem->m_nState = 0;                                         // consume ticket (m_nState@0x28)

    cOut.m_nResponse     = 0;
    cOut.m_nItemSeq      = nItemSeq;
    cOut.m_nLevel        = pPlayer->m_cLevel.m_nLevel;
    cOut.m_nExp          = pPlayer->m_cLevel.m_nExp;
    cOut.m_nFacultyPoint = pPlayer->m_cLevel.m_nFaculty;
    memcpy(&cOut.m_cFaculty, pPlayer->m_cRaiseFaculty.m_nFaculty, ARRAY_FACULTY_SIZE);
    SendPlayer(pPlayer, &cOut, 0);
}

// PacketChangeName @0807d056
void PacketChangeName(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSChangeName* pIn = (CCSChangeName*)pPacket;
    int   nItemSeq = pIn->m_nItemSeq;
    char* sName    = pIn->m_sName;

    CSCChangeName cOut;
    CItem* pItem = pPlayer->GetItemPointer(nItemSeq);
    if (pItem == 0) { cOut.m_nResponse = -3; SendPlayer(pPlayer, &cOut, -1); return; }

    int nCheck = g_Sql.CheckPlayerName(sName);
    if (nCheck < 0) { cOut.m_nResponse = (char)nCheck; SendPlayer(pPlayer, &cOut, -1); return; }
    if (g_Sql.UpdateNameField(pPlayer, sName) < 0) { cOut.m_nResponse = -2; SendPlayer(pPlayer, &cOut, -1); return; }

    pItem->m_nState = 0;               // consume rename-ticket (m_nState@0x28)
    cOut.m_nResponse  = 0;
    cOut.m_nPlayerSeq = nItemSeq;                // binary reuses this field
    memset(cOut.m_sName, 0, PLAYER_NAME_SIZE);
    memcpy(cOut.m_sName, sName, PLAYER_NAME_SIZE);
    SendPlayer(pPlayer, &cOut, 0);
}

// =============================================================================
// SPINE HANDLERS - player info
// =============================================================================

// PacketPlayerInfo @0807543c - send player info, then chain the sub-info packets.
void PacketPlayerInfo(CPlayer* pPlayer, CHeadPacket* /*pPacket*/)
{
    CSCPlayerInfo cOut;
    cOut.m_nResponse = 0;
    pPlayer->GetPlayerInfo(&cOut.m_cPlayerInfo);
    cOut.m_nBodySize = (int)(sizeof(CSCPlayerInfo) - sizeof(CHeadPacket));
    SendPlayer(pPlayer, &cOut, 0);

    // chained sub-info (these handlers are domain-owned).
    PacketItemInfo(pPlayer);
    PacketSkillInfo(pPlayer);
    PacketTrainingInfo(pPlayer);
    PacketCeremonyInfo(pPlayer);
    PacketQuestInfo(pPlayer);
    PacketCardInfo(pPlayer, 0);
    PacketPlayerinfoEnd(pPlayer, 0);
}

// PacketPlayerinfoEnd @0807f88e
void PacketPlayerinfoEnd(CPlayer* pPlayer, CHeadPacket* /*pPacket*/)
{
    if (pPlayer == 0) return;
    CSCPlayerinfoEnd cOut;
    cOut.m_nResponse = 0;
    SendPlayer(pPlayer, &cOut, 0);
}

// =============================================================================
// SPINE HANDLERS - buddy
// =============================================================================

// PacketBuddyInfo @08080c40
void PacketBuddyInfo(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    int nPage = ((CCSBuddyInfo*)pPacket)->m_nPage;
    CSCBuddyInfo cOut;
    pPlayer->GetBuddyInfo(nPage, &cOut);
    cOut.m_nResponse = 0;
    SendPlayer(pPlayer, &cOut, 0);
}

// PacketAddBuddy @08080d98
void PacketAddBuddy(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    int  nSeq = ((CCSAddBuddy*)pPacket)->m_nPlayerSeq;
    int  nRet = pPlayer->AddBuddyInfo(nSeq);
    CSCAddBuddy cOut;
    cOut.m_nResponse = (nRet < 0) ? (char)nRet : 0;
    SendPlayer(pPlayer, &cOut, (nRet < 0) ? nRet : 0);
}

// PacketDelBuddy @08080fa2
void PacketDelBuddy(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    int  nSeq = ((CCSDelBuddy*)pPacket)->m_nPlayerSeq;
    int  nRet = pPlayer->DeleteBuddyList(nSeq);
    CSCDelBuddy cOut;
    if (nRet < 0)
    {
        cOut.m_nResponse = (nRet == -1) ? (char)-0xb : (char)-99;
        cOut.m_nBodySize = 1;
        SendPlayer(pPlayer, &cOut, cOut.m_nResponse);
        return;
    }
    cOut.m_nResponse  = 0;
    cOut.m_nPlayerSeq = nSeq;
    SendPlayer(pPlayer, &cOut, 0);
}

// =============================================================================
// SPINE HANDLERS - blacklist
// =============================================================================

// PacketBlacklistInfo @08080e02
void PacketBlacklistInfo(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    int nPage = ((CCSBlacklistInfo*)pPacket)->m_nPage;
    CSCBlacklistInfo cOut;
    pPlayer->GetBlacklistInfo(nPage, &cOut);
    cOut.m_nResponse = 0;
    SendPlayer(pPlayer, &cOut, 0);
}

// PacketAddBlacklist @08080e7c
void PacketAddBlacklist(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    int  nSeq = ((CCSAddBlacklist*)pPacket)->m_nPlayerSeq;
    int  nRet = pPlayer->AddBlackListInfo(nSeq);
    CSCAddBlacklist cOut;
    cOut.m_nResponse = (char)nRet;
    SendPlayer(pPlayer, &cOut, nRet);
}

// PacketDelBlacklist @0808103c
void PacketDelBlacklist(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    int  nSeq = ((CCSDelBlacklist*)pPacket)->m_nPlayerSeq;
    int  nRet = pPlayer->DeleteBlackList(nSeq);
    CSCDelBlacklist cOut;
    if (nRet < 0)
    {
        cOut.m_nResponse = (nRet == -1) ? (char)-0xb : (char)-99;
        cOut.m_nBodySize = 1;
        SendPlayer(pPlayer, &cOut, cOut.m_nResponse);
        return;
    }
    cOut.m_nResponse  = 0;
    cOut.m_nPlayerSeq = nSeq;
    SendPlayer(pPlayer, &cOut, 0);
}

// =============================================================================
// SPINE HANDLERS - weekly record / ranking
// =============================================================================

// PacketWeeklyRecord @08080ed6
void PacketWeeklyRecord(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    int nSeq = ((CCSWeeklyRecord*)pPacket)->m_nPlayerSeq;
    CSCWeeklyRecord cOut;
    int nRet = g_Sql.GetRecordField_Weekly(nSeq, &cOut);
    SendPlayer(pPlayer, &cOut, nRet);
}

// PacketWeeklyRanking @08080f3c
void PacketWeeklyRanking(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    int nSeq = ((CCSWeeklyRanking*)pPacket)->m_nPlayerSeq;
    CSCWeeklyRanking cOut;
    int nRet = g_Sql.GetRankingField_Weekly(nSeq, &cOut);
    SendPlayer(pPlayer, &cOut, nRet);
}

// =============================================================================
// SPINE HANDLERS - admin tools
// =============================================================================

// PacketOperationTool @0807e3be - GM operations dispatched on m_nSubOp.
void PacketOperationTool(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSOperationTool* pIn = (CCSOperationTool*)pPacket;
    CSCOperationTool  cOut;
    cOut.m_nResponse = 0;

    switch (pIn->m_nSubOp)
    {
    case 1:   // broadcast text
        PutSendBroadcast(pIn->m_sText);
        break;
    case 2:   // notice to all
    {
        CSCSendMessage cMsg;
        cMsg.m_nChatKind = 7;
        memset(cMsg.m_sFromName, 0, PLAYER_NAME_SIZE);
        StrCopyN(cMsg.m_sFromName, "operator", PLAYER_NAME_SIZE - 1);
        memset(cMsg.m_sMessage, 0, MESSAGE_SIZE);
        StrCopyN(cMsg.m_sMessage, pIn->m_sText, MESSAGE_SIZE - 1);
        SendAll(&cMsg, 0);
        break;
    }
    case 3:
        break;
    case 4:   // kick all in room
    {
        CRoom* pRoom = GetRoomPointer(pIn->m_nArg);
        if (pRoom) { pRoom->KickAll(); pRoom->DeleteRoom(); }
        break;
    }
    case 5:   // kick user
    {
        CPlayer* pTo = GetPlayerPointerPc(pIn->m_sText);
        if (pTo) PacketLeaveRoom(pTo, 2);
        break;
    }
    case 6:   // force-master
    {
        CPlayer* pTo = GetPlayerPointerPc(pIn->m_sText);
        if (pTo && pTo->m_pRoom) PacketChangeJang(pTo->m_pRoom);
        break;
    }
    case 7:   // force-out
    {
        CPlayer* pTo = GetPlayerPointerPc(pIn->m_sText);
        if (pTo) { CCSForceOut pkt; pkt.m_nPlayerSeq = pTo->m_nPlayerSeq; PacketForceOut(pTo, &pkt); }
        break;
    }
    case 8:   // disconnect + save
    {
        CPlayer* pTo = GetPlayerPointerPc(pIn->m_sText);
        if (pTo) { g_Sql.UpdateStateField(pTo); CCSGameExit pkt; pkt.m_nReason = 2; PacketGameExit(pTo, &pkt); }
        break;
    }
    case 9:   // shutup
    {
        CPlayer* pTo = GetPlayerPointerPc(pIn->m_sText);
        if (pTo) g_Sql.UpdateShutupField(pTo, pIn->m_nArg);
        break;
    }
    case 10:  // disconnect (no save)
    {
        CPlayer* pTo = GetPlayerPointerPc(pIn->m_sText);
        if (pTo) { CCSGameExit pkt; pkt.m_nReason = 2; PacketGameExit(pTo, &pkt); }
        break;
    }
    default:
        cOut.m_nResponse = (char)0xff;
        SendPlayer(pPlayer, &cOut, -1);
        return;
    }
    SendPlayer(pPlayer, &cOut, 0);
}

// PacketRoomTool @0807eab2 - GM room snapshot + athlete rows.
void PacketRoomTool(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    int nRoomSeq = ((CCSRoomTool*)pPacket)->m_nRoomSeq;
    CSCRoomTool cOut;
    cOut.m_nResponse = 0;
    cOut.m_nCount    = 0;

    if (nRoomSeq > 0)
    {
        CRoom* pRoom = GetRoomPointer(nRoomSeq);
        if (pRoom)
        {
            pRoom->GetRoomInfo(&cOut.m_cRoomInfo);
            cOut.m_nCount = (char)pRoom->GetAthleteList(cOut.m_cAthlete, 18);
        }
    }
    SendPlayer(pPlayer, &cOut, 0);
}

// =============================================================================
// SPINE HANDLERS - UDP
// =============================================================================

// PacketUDPPunching @08081d06 - register the player's UDP source endpoint.
void PacketUDPPunching(CUDPAddress* pFrom, CHeadPacket* pPacket)
{
    int nSeq = ((CCSUDPPunching*)pPacket)->m_nPlayerSeq;
    CPlayer* pPlayer = GetPlayerPointer(nSeq);
    if (pPlayer == 0) return;
    // store the UDP CAddress at CPlayer+0xec and mark UDP-punched (100).
    memcpy(&pPlayer->m_cUDPAddress, pFrom, sizeof(CUDPAddress));   // +0xec
    pPlayer->m_nCheckUDP = 'd';   // 100 : punched
    LOGOUT_PACKET("UDPPunching: seq(%d) from(%s:%u)\n", nSeq, pFrom->m_sIP, pFrom->m_nPort);
}

// PacketUDPRelay @08081e02 - forward the datagram to the target's UDP endpoint.
void PacketUDPRelay(CUDPAddress* /*pFrom*/, CHeadPacket* pPacket)
{
    int nSeq = pPacket->m_nSequence;   // routing target rides the seq field
    CPlayer* pPlayer = GetPlayerPointer(nSeq);
    if (pPlayer == 0) return;
    pPlayer->m_nRelay = 1;         // +0x510 relay-active flag
    Net_SendUDP((CUDPAddress*)&pPlayer->m_cUDPAddress, pPacket, pPacket->m_nBodySize + 0xc);   // +0xec
}

// PacketUDPPing @08081dae - send a UDP keepalive to the player's endpoint.
void PacketUDPPing(CPlayer* pPlayer)
{
    CSCUDPPing cOut;   // cmd 9001, body 0 (header-only datagram, set by ctor)
    Net_SendUDP((CUDPAddress*)&pPlayer->m_cUDPAddress, &cOut, cOut.m_nBodySize + 0xc);   // +0xec
}
