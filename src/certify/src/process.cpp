// =============================================================================
// process.cpp - CProcess (client TCP) and CProcessSTS (server-to-server) packet
// dispatch + periodic maintenance (reconstructed from XKICK_Certify).
//
// InitFunction builds the command-id -> handler map (verified against the
// binary's dispatch tables). PacketProcess drains the socket's m_vPacketList
// each tick, logs the packet, dispatches via the map (EmptyFunction for unmapped
// ids), zeroes the buffer, then clears the list. TimeProcess runs the ping/log/
// action timers.
//
// Portable: std C++ only.
// =============================================================================
#include "Main.h"
#include "packet.h"
#include "HackShield.h"   // PacketHackRequest / PacketHackResponse
#include "LogManager.h"
#include <cstdio>
#include <cstring>
#include <ctime>

// =============================================================================
// CProcess (client TCP)
// =============================================================================
CProcess::CProcess()
{
    m_tPingTime   = 0;
    m_tLogTime    = 0;
    m_tActionTime = 0;
    m_nMinute     = -1;
    InitFunction();
}

CProcess::~CProcess() {}

// ---------------------------------------------------------------------------
// InitFunction - register every client TCP handler. Command ids are exactly the
// ones the binary's InitFunction installs.
// ---------------------------------------------------------------------------
void CProcess::InitFunction()
{
    m_mTCPFunctionHash[1000]   = PacketCertifyLogin;     // 0x3E8
    m_mTCPFunctionHash[0x3e9]  = PacketInstantLogin;
    m_mTCPFunctionHash[0x3ea]  = PacketCertifyExit;
    m_mTCPFunctionHash[0x44c]  = PacketMemberInfo;
    m_mTCPFunctionHash[0x4b0]  = PacketCharacterInfo;
    m_mTCPFunctionHash[0x4b2]  = PacketCreateCharacter;
    m_mTCPFunctionHash[0x4b3]  = PacketDeleteCharacter;
    m_mTCPFunctionHash[0x4b4]  = PacketChoiceCharacter;
    m_mTCPFunctionHash[0x514]  = PacketServerList;
    m_mTCPFunctionHash[0x515]  = PacketChoiceServer;
    m_mTCPFunctionHash[0x578]  = PacketExecuteTutorial;
    m_mTCPFunctionHash[0xa90]  = PacketExecuteQuest;
    m_mTCPFunctionHash[0x138b] = PacketChangeSetting;
    m_mTCPFunctionHash[0x7d3]  = PacketNoticeList;
    m_mTCPFunctionHash[0x4b5]  = PacketCharacterSearch;
    m_mTCPFunctionHash[0x3eb]  = PacketDrawforcePlayer;
    m_mTCPFunctionHash[5000]   = PacketTCPPing;
    m_mTCPFunctionHash[0x138c] = PacketSettingInfo;
    m_mTCPFunctionHash[0x26ae] = PacketHackResponse;     // CM_HACK_RESPONSE
}

// ---------------------------------------------------------------------------
// PacketProcess - dispatch every queued client packet then clear the list.
// ---------------------------------------------------------------------------
void CProcess::PacketProcess()
{
    for (VectorPacketList::iterator it = g_TCPSocket.m_vPacketList.begin();
         it != g_TCPSocket.m_vPacketList.end(); ++it)
    {
        CBufferPool* pPool = *it;
        if (pPool == 0)
            continue;

        CHeadPacket* pPacket = (CHeadPacket*)pPool->m_sBuffer;

        char sCommand[30];
        memset(sCommand, 0, 30);
        GetCommandString(pPacket->m_nCommand, sCommand);
        if (IsCommandString(pPacket->m_nCommand) && pPool->m_pObject != 0)
        {
            if (pPacket->m_nCommand < 0xc9)
            {
                LOGOUT_SCREEN("\x1b[33m[STS]\x1b[0m ");
                LOGOUT_PACKET("STS %s(%d), nSize(%d)\n",
                              sCommand, pPacket->m_nCommand, pPacket->m_nBodySize);
            }
            else
            {
                CMember* pMember = (CMember*)pPool->m_pObject;
                LOGOUT_SCREEN("\x1b[33m[INP]\x1b[0m ");
                LOGOUT_PACKET("INP(%d), Seq(%d): %s(%d), nSize(%d)\n",
                              pMember->m_nFd, pMember->m_nMemberSeq, sCommand,
                              pPacket->m_nCommand, pPacket->m_nBodySize);
            }
        }

        MapTCPFunction::iterator h = m_mTCPFunctionHash.find(pPacket->m_nCommand);
        if (h == m_mTCPFunctionHash.end() || h->second == 0)
            EmptyFunction<CMember>((CMember*)pPool->m_pObject, pPacket);
        else
            h->second((CMember*)pPool->m_pObject, pPacket);

        memset(pPool->m_sBuffer, 0, 0x800);
    }
    g_TCPSocket.m_vPacketList.clear();
}

// ---------------------------------------------------------------------------
// TimeProcess - per-tick periodic work (ping + config-reload timers).
// ---------------------------------------------------------------------------
void CProcess::TimeProcess()
{
    time_t tCurrentTime;
    time(&tCurrentTime);
    CheckPingTime(tCurrentTime);
    CheckActionTime(tCurrentTime);
}

// ---------------------------------------------------------------------------
// CheckPingTime - every ~3s, ping idle members; drop those that miss 2 pings.
// ---------------------------------------------------------------------------
void CProcess::CheckPingTime(time_t tCurrentTime)
{
    if (tCurrentTime - m_tPingTime <= 2)
        return;
    time(&m_tPingTime);

    for (int i = 0; i < MAX_MEMBER_POOL; ++i)
    {
        if (g_MemberPool[i].m_nState == 0)
            continue;
        CMember* pMember = &g_MemberPool[i];
        if (tCurrentTime - g_MemberPool[i].m_tTcpPingTime <= 9)
            continue;

        if (g_MemberPool[i].m_nCheckTCP < 2)
        {
            g_MemberPool[i].m_tTcpPingTime = tCurrentTime;
            g_MemberPool[i].m_nCheckTCP += 1;
            PacketTCPPing(pMember);
            PacketHackRequest(pMember);
        }
        else
        {
            pMember->ExitMember();
        }
    }
}

// ---------------------------------------------------------------------------
// CheckLogTime - on the 5-minute boundary, write a connection-count log row.
// ---------------------------------------------------------------------------
void CProcess::CheckLogTime(time_t tCurrentTime)
{
    if (tCurrentTime - m_tLogTime <= 9)
        return;
    time(&m_tLogTime);

    time_t t;
    time(&t);
    std::tm tp = LocalTime(t);
    int nCurrentMinute = tp.tm_min;
    if ((nCurrentMinute % 5 == 0) && (m_nMinute != nCurrentMinute))
    {
        m_nMinute = nCurrentMinute;
        int nConnect = GetMemberCount();
        g_Sql.InsertConnectLog(nConnect, 0, 0);
    }
}

// ---------------------------------------------------------------------------
// CheckActionTime - every 5 minutes, reload config.ini (hot config).
// ---------------------------------------------------------------------------
void CProcess::CheckActionTime(time_t tCurrentTime)
{
    if (tCurrentTime - m_tActionTime <= 299)
        return;
    time(&m_tActionTime);
    g_Config.InitConfig();
}

// =============================================================================
// CProcessSTS (server-to-server)
// =============================================================================
CProcessSTS::CProcessSTS()
{
    InitFunction();
}

CProcessSTS::~CProcessSTS() {}

// ---------------------------------------------------------------------------
// InitFunction - register the STS handlers (ids from the binary).
// ---------------------------------------------------------------------------
void CProcessSTS::InitFunction()
{
    m_mSTSFunctionHash[0x65] = PacketStsLogin;        // CM_STS_LOGIN (101)
    m_mSTSFunctionHash[0x67] = PacketUpdateWeather;
    m_mSTSFunctionHash[0x69] = PacketUpdateNotice;
    m_mSTSFunctionHash[0x6a] = PacketSendBroadcast;
}

// ---------------------------------------------------------------------------
// PacketProcess - dispatch every queued STS packet then clear the list.
// ---------------------------------------------------------------------------
void CProcessSTS::PacketProcess()
{
    for (VectorPacketList::iterator it = g_STSSocket.m_vPacketList.begin();
         it != g_STSSocket.m_vPacketList.end(); ++it)
    {
        CBufferPool* pPool = *it;
        if (pPool == 0)
            continue;

        CHeadPacket* pPacket = (CHeadPacket*)pPool->m_sBuffer;

        char sCommand[30];
        memset(sCommand, 0, 30);
        GetCommandString(pPacket->m_nCommand, sCommand);
        if (IsCommandString(pPacket->m_nCommand) && pPool->m_pObject != 0)
        {
            if (pPacket->m_nCommand < 0xc9)
            {
                LOGOUT_SCREEN("\x1b[33m[STS]\x1b[0m ");
                LOGOUT_PACKET("STS %s(%d), nSize(%d)\n",
                              sCommand, pPacket->m_nCommand, pPacket->m_nBodySize);
            }
            else
            {
                CServer* pServer = (CServer*)pPool->m_pObject;
                LOGOUT_SCREEN("\x1b[33m[INP]\x1b[0m ");
                LOGOUT_PACKET("INP(%d), Seq(%d): %s(%d), nSize(%d)\n",
                              pServer->m_nFd, pServer->m_nServerCode, sCommand,
                              pPacket->m_nCommand, pPacket->m_nBodySize);
            }
        }

        MapSTSFunction::iterator h = m_mSTSFunctionHash.find(pPacket->m_nCommand);
        if (h == m_mSTSFunctionHash.end() || h->second == 0)
            EmptyFunction<CServer>((CServer*)pPool->m_pObject, pPacket);
        else
            h->second((CServer*)pPool->m_pObject, pPacket);

        memset(pPool->m_sBuffer, 0, 0x800);
    }
    g_STSSocket.m_vPacketList.clear();
}
