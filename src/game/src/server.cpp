// =============================================================================
// server.cpp - CServer (STS peer node) + the STS (server-to-server) handlers.
// Reconstructed from XKICK_Game1 (unstripped).
//
// In the Game the STS channel is a SINGLE upstream link to the master/certify
// server (one global g_STSSocket; SendSTS @08082d2e writes on it directly), so
// CServer is a near-empty placeholder:
//   CServer::C1/C2 @0809661a/@08096614   InitServer @0809662c   ExitServer @08096632
//
// The STS handlers (installed in CProcess's +0x30 map) take a CServer* but none of
// them read CServer member fields -- they only refresh global tables or relay to a
// player. Handlers:
//   GetStsLogin         @08081f66   (log-only ack: uplink is up)
//   GetStsDrawforce     @0808222a   (force-disconnect a duplicate login)
//   PacketUpdateWeather @08081fcc   (reload weather table)
//   PacketUpdateNotice  @08082056   (reload notice table + broadcast NoticeList)
//   PacketSendBroadcast @08082106   (system broadcast message to all players)
//   UpdateSchedule      (cmd 0x68)  : NOT present in this build -- see note below.
//
// Portable: std C++ only.
// =============================================================================
#include "Main.h"
#include "packet.h"
#include "GameProtocol.h"
#include "Net.h"
#include "Sql.h"
#include "GameLoad.h"
#include "LogManager.h"
#include <cstring>

// cross-module lookup (player.cpp): characterSeq -> CPlayer*.
CPlayer* GetPlayerPointer(int nSeq);
// broadcast to every connected player (packet.cpp / player.cpp helper).
void     SendAll(CHeadPacket* pPacket, int nResult);

// =============================================================================
// CServer - placeholder STS node (no field initialization in the binary).
// =============================================================================
CServer::CServer() {}
CServer::~CServer() {}

void CServer::InitServer() {}

int CServer::GetSeq()
{
    return m_nServerCode;
}

void CServer::ExitServer() {}

// =============================================================================
// STS HANDLERS  void(CServer*, CHeadPacket*)
// =============================================================================

// GetStsLogin @08081f66 - the uplink reported it is logged in; log-only ack.
void GetStsLogin(CServer* /*pServer*/, CHeadPacket* /*pPacket*/)
{
    LOGOUT_PACKET("GetStsLogin: STS uplink up\n");
}

// GetStsDrawforce @0808222a - certify asks us to force-disconnect a duplicate
// login. If the player is here, kick it (reason 2); otherwise just flip its DB
// logout flag. Always ack on the STS channel.
void GetStsDrawforce(CServer* pServer, CHeadPacket* pPacket)
{
    CSCStsDrawforce cOut;
    if (pServer == 0 || pPacket == 0)
        return;

    CCSStsDrawforce* req = (CCSStsDrawforce*)pPacket;
    int nPlayerSeq = req->m_nPlayerSeq;
    int nMemberSeq = req->m_nMemberSeq;

    CPlayer* pPlayer = GetPlayerPointer(nPlayerSeq);
    if (pPlayer == 0)
    {
        g_Sql.SetLogOutData(nMemberSeq);
    }
    else
    {
        CCSGameExit cExit;
        cExit.m_nReason = 2;
        PacketGameExit(pPlayer, &cExit);
    }

    cOut.m_nResponse = 0;
    SendSTS(&cOut, 0);
}

// PacketUpdateWeather @08081fcc - reload the weather table from the DB.
void PacketUpdateWeather(CServer* pServer, CHeadPacket* pPacket)
{
    if (pServer == 0 || pPacket == 0)
        return;
    g_Sql.LoadWeatherTable(&g_Weather);   // owner is CSql, not CLoad
    LOGOUT_PACKET("STS: weather table reloaded\n");
}

// PacketUpdateNotice @08082056 - reload notices, then re-broadcast NoticeList to
// all connected players (PacketNoticeList with a null player == broadcast path).
void PacketUpdateNotice(CServer* pServer, CHeadPacket* pPacket)
{
    if (pServer == 0 || pPacket == 0)
        return;
    g_Load.LoadNoticeTable();
    CCSNoticeList cReq;
    PacketNoticeList(0, &cReq);
    LOGOUT_PACKET("STS: notice table reloaded + broadcast\n");
}

// PacketSendBroadcast @08082106 - push a system broadcast chat to every player.
//   request body: { char m_nKind@+0xc; char m_sMessage[121]@+0xd }.
void PacketSendBroadcast(CServer* pServer, CHeadPacket* pPacket)
{
    if (pServer == 0 || pPacket == 0)
        return;

    CCSSendBroadcast* pIn = (CCSSendBroadcast*)pPacket;

    CSCSendMessage cMsg;
    cMsg.m_nChatKind = pIn->m_nKind;
    memset(cMsg.m_sFromName, 0, PLAYER_NAME_SIZE);
    memcpy(cMsg.m_sFromName, "operator", 8);          // fixed sender label (@080be3e8)
    memset(cMsg.m_sMessage, 0, MESSAGE_SIZE);
    StrCopyN(cMsg.m_sMessage, pIn->m_sMessage, MESSAGE_SIZE - 1);
    cMsg.m_nResponse = 0;

    LOGOUT_PACKET("STS broadcast kind(%d): %s\n", pIn->m_nKind, pIn->m_sMessage);
    SendAll(&cMsg, 0);
}

// UpdateSchedule (CM_UPDATE_SCHEDULE 0x68): the Game's InitFunction does NOT
// register a 0x68 handler (only 0x65/0x66/0x67/0x69/0x6A exist) and no such
// function is present in this build. Declared in packet.h for completeness; left
// undefined here on purpose -- it is a no-op the dispatcher never reaches.
