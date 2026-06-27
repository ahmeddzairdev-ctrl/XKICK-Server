// CServer - a connected game server node (server.cpp). Certify tracks the
// game servers that connect over the STS (server-to-server) channel.
// Layout from Ghidra (84 bytes).
#ifndef _CERTIFY_SERVERNODE_H_
#define _CERTIFY_SERVERNODE_H_

#include "Struct.h"   // CAddress

class CServer
{
public:
    short    m_nState;        // 0   SERVER_STATE_EMPTY/CHECKUP/RUN
    int      m_nFd;          // 4   STS socket fd
    int      m_nServerCode;  // 8
    CAddress m_cAddress;     // 12  (24 bytes)
    char     m_sPartner[15]; // 36
    char     m_sServer[31];  // 51  server title

    CServer();
    ~CServer();

    void InitServer();
    int  GetSeq();           // returns m_nServerCode
    void ExitServer();
};

#endif
