// =============================================================================
// HackShield.cpp - XKICK-side AhnLab HackShield call sites (faithfully
// reconstructed from XKICK_Certify). These drive the AhnHS SDK (real or stub).
//
// Behaviour is gated on g_Config.m_nHackCode: 0 => HackShield disabled, every
// entry point is a no-op success. With the dummy AhnHS_stub.cpp linked, the
// calls also succeed, so the feature is inert until the real AhnLab lib is used.
//
// Depends on the 3-int server CHeadPacket (Struct.h) and SendMember() (packet.cpp).
// =============================================================================
#include "HackShield.h"
#include "Config.h"
#include "Init.h"
#include "Member.h"
#include <cstdio>
#include <cstring>

extern CConfig g_Config;

// Server handle (NULL until InitServerHack succeeds). Originally @ 082dd758.
AHNHS_SERVER_HANDLE g_hServer = 0;

// Defined in packet.cpp; third arg is the request/secure flag (0 here).
extern void SendMember(CMember* pMember, CHeadPacket* pPacket, int nReq);

// ---- wire packet constructors ----
CSCHackRequest::CSCHackRequest() : CHeadPacket(CM_HACK_REQUEST)
{
    m_nBodySize = 0x193;            // 403 = sizeof(m_nResponse) + 402-byte buffer
    m_nResponse = 0;
}

CCSHackResponse::CCSHackResponse() : CHeadPacket(0)
{
    m_nBodySize = AHNHS_TRANS_BUFFER_SIZE;
}

// ---- CInit::InitServerHack ----
// Loads ../Hack/<HackCode>.hsb and creates the global server object.
bool CInit::InitServerHack()
{
    if (g_Config.m_nHackCode == 0)
        return true;                                   // HackShield disabled

    char sPath[100];
    snprintf(sPath, sizeof(sPath), "../Hack/%d.hsb", g_Config.m_nHackCode);
    g_hServer = _AhnHS_CreateServerObject(sPath);
    if (g_hServer == 0)
    {
        printf("error: ahnHS_createserverobject\n");
        return false;
    }
    return true;
}

// ---- CInit::InitClientHack ----
// Creates a per-connection client object bound to the global server object.
bool CInit::InitClientHack(CMember* pMember)
{
    if (g_Config.m_nHackCode == 0)
        return true;

    printf("InitClientHack start..\n");
    pMember->m_hClient = 0;
    pMember->m_hClient = _AhnHS_CreateClientObject(g_hServer);
    if (pMember->m_hClient == 0)
    {
        printf("error: ahnHS_createclientobject\n");
        return false;
    }
    printf("InitClientHack end..\n");
    return true;
}

// ---- PacketHackRequest: server -> client integrity challenge ----
void PacketHackRequest(CMember* pMember)
{
    CSCHackRequest cPacket;
    if (g_Config.m_nHackCode == 0)
        return;

    printf("Hack Server ---> Client\n");
    unsigned long ulRet = _AhnHS_MakeRequest(pMember->m_hClient, cPacket.m_sHackBuf);
    if (ulRet == AHNHS_SUCCESS)
    {
        cPacket.m_nResponse = 0;
        SendMember(pMember, &cPacket, 0);
    }
    else
    {
        printf("error: MakeRequest(%lu)\n", ulRet);
    }
}

// ---- PacketHackResponse: verify the client's integrity response ----
void PacketHackResponse(CMember* pMember, CHeadPacket* pPacket)
{
    printf("Hack Client ---> Server\n");
    CCSHackResponse* pRes = (CCSHackResponse*)pPacket;
    unsigned long ulRet = _AhnHS_VerifyResponse(
        pMember->m_hClient, pRes->m_sHackBuf,
        (unsigned short)pPacket->m_nBodySize);

    // This specific set of AhnLab codes marks a failed client. NOTE: verified
    // against XKICK_Certify PacketHackResponse @08059a50 - the original does NOT
    // kick or flag the member; it only logs the result and lets the connection
    // continue. (There is no InsertHackUser / kick path on this code in certify.)
    if (ulRet == 0xe9040008 || ulRet == 0xe9040009 || ulRet == 0xe904000a ||
        ulRet == 0xe904000b || ulRet == 0xe904000c || ulRet == 0xe904000d ||
        ulRet == 0xe9040010 || ulRet == 0xe904000e || ulRet == 0xe9040011 ||
        ulRet == 0xe9040012 || ulRet == 0xe9040015 || ulRet == 0xe9040003 ||
        ulRet == 0xe9040018 || ulRet == 0xe9040019)
    {
        printf("error: VerifyResponse(%lu)\n", ulRet);
    }
    else
    {
        printf("sucess VerifyResponse\n");
    }
}
