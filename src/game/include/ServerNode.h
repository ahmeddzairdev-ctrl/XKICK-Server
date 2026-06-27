// =============================================================================
// XKICK_Game - ServerNode.h
// class CServer : the Game server's STS (server-to-server) peer node — the handle
// for the upstream Certify/master server connection. Mirrors Certify's CServer.
//
// Reverse-engineering finding: in XKICK_Game the STS channel is a SINGLE upstream
// link (one global g_STSSocket; SendSTS @08082d2e sends on g_STSSocket directly),
// so CServer here is a near-empty placeholder node:
//   CServer::C1/C2 (ctor)  @0809661a / @08096614   -> empty (no field init)
//   CServer::InitServer    @0809662c               -> empty
//   CServer::ExitServer    @08096632
// The STS dispatch table is keyed by command and the handlers take `CServer*`
// (see GameProcess.h MapSTSFunction), e.g.:
//   GetStsLogin       @08081f66   GetStsDrawforce  @0808222a
//   PacketUpdateWeather @08081fcc PacketUpdateNotice @08082056
//   PacketSendBroadcast @08082106
// but those handlers only log / refresh global tables — none read CServer member
// fields, so the field layout below is INFERRED from the Certify CServer twin and
// is marked uncertain. Adjust once a populating call site is found.
// =============================================================================
#ifndef _GAME_SERVERNODE_H_
#define _GAME_SERVERNODE_H_

#include "Struct.h"     // CAddress

class CServer
{
public:
    // ---- INFERRED layout (mirrors certify/ServerNode.h; not pinned by Game binary) ----
    short    m_nState;        // 0   SERVER_STATE_EMPTY/CHECKUP/RUN   (uncertain)
    int      m_nFd;          // 4   STS socket fd                    (uncertain)
    int      m_nServerCode;  // 8                                    (uncertain)
    CAddress m_cAddress;     // 12  (24 bytes)                       (uncertain)
    char     m_sPartner[15]; // 36                                   (uncertain)
    char     m_sServer[31];  // 51  server title                    (uncertain)

    CServer();                       // @0809661a (empty)
    ~CServer();

    void InitServer();               // @0809662c (empty)
    int  GetSeq();                   // returns m_nServerCode
    void ExitServer();               // @08096632
};

#endif // _GAME_SERVERNODE_H_
