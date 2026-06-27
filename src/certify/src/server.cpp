// =============================================================================
// server.cpp - CServer node object + the free ExitServer helper (reconstructed
// from XKICK_Certify). A CServer tracks one connected game server on the STS
// (server-to-server) channel. CreateServer lives in connect.cpp.
//
// PORTABILITY: the binary used CSTSSocket::CloseEpoll/CloseSocket; the port
// routes through connect.cpp's Net_Close. m_nFd is the logical connection id.
// =============================================================================
#include "Main.h"
#include "Net.h"        // Net_Close
#include <algorithm>

// In the binary CServer's ctor/InitServer/ExitServer are trivial (no member
// initialisation); CreateServer in connect.cpp seeds the fields after InitServer.
CServer::CServer()  {}
CServer::~CServer() {}

void CServer::InitServer() {}

int  CServer::GetSeq() { return m_nServerCode; }

// Member form (faithful to the binary): detach from g_ServerList, close the STS
// socket, mark the slot free, and clear the DB connect flag.
void CServer::ExitServer()
{
    VectorServerList::iterator it = std::find(g_ServerList.begin(), g_ServerList.end(), this);
    if (it != g_ServerList.end())
        g_ServerList.erase(it);

    Net_Close(m_nFd);     // was CSTSSocket::CloseEpoll + CloseSocket
    m_nState = 0;
    g_Sql.DisconnectServer(m_nServerCode);
}

// Free-function form used by the connect.cpp reactor on disconnect.
void ExitServer(CServer* pServer)
{
    if (pServer == 0)
        return;
    pServer->ExitServer();
}
