// =============================================================================
// HackShield.h - XKICK-side glue for AhnLab HackShield (call sites).
//
// Wraps the AhnHS.h SDK calls used by the servers:
//   server boot:  CInit::InitServerHack()  -> _AhnHS_CreateServerObject (sets g_hServer)
//   per client:   CInit::InitClientHack()  -> _AhnHS_CreateClientObject (sets m_hClient)
//   challenge:    PacketHackRequest()      -> _AhnHS_MakeRequest    (server -> client)
//   verify:       PacketHackResponse()     -> _AhnHS_VerifyResponse (client -> server)
//
// With AhnHS_stub.cpp linked, all of these succeed and the feature is a no-op,
// so the build is fully cross-platform. Drop in the real AhnLab lib to activate.
// =============================================================================
#ifndef _XKICK_HACKSHIELD_H_
#define _XKICK_HACKSHIELD_H_

#include "AhnHS.h"
#include "Struct.h"    // CHeadPacket

class CMember;

// Command id for the server->client challenge packet (recovered: 0x26AD).
#define CM_HACK_REQUEST   0x26AD   // 9901; CSCHackRequest, body size 0x193 (403)

// Global server handle (NULL when HackCode==0, i.e. HackShield disabled).
// Defined in init.cpp. Was AHNHS_SERVER_HANDLE g_hServer @ 082dd758.
extern AHNHS_SERVER_HANDLE g_hServer;

// ---- HackShield wire packets (pack(1), byte-identical to the binary) ----
#pragma pack(push, 1)

// Server -> Client: integrity challenge (CM_* hack request). 415 bytes.
class CSCHackRequest : public CHeadPacket
{
public:
    char               m_nResponse;                 // 12
    AHNHS_TRANS_BUFFER m_sHackBuf;                  // 13 (402 bytes)
    CSCHackRequest();
};

// Client -> Server: integrity response. Header + transmission buffer.
class CCSHackResponse : public CHeadPacket
{
public:
    AHNHS_TRANS_BUFFER m_sHackBuf;
    CCSHackResponse();
};

#pragma pack(pop)

// ---- call-site wrappers (packet handlers; registered in the dispatch map) ----
void PacketHackRequest(CMember* pMember);                       // make + send challenge
void PacketHackResponse(CMember* pMember, CHeadPacket* pPacket);// verify client response

#endif // _XKICK_HACKSHIELD_H_
