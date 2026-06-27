// =============================================================================
// Net.h - socket/reactor layer (connect.cpp), Asio-backed (PLAN.md mandate).
// The original used raw sockets + epoll; the port uses standalone Asio, kept
// ENTIRELY inside connect.cpp so no Asio/OS headers leak into general code.
//
// Behaviour preserved from the binary:
//   * Accept -> CreateMember (first free slot of g_MemberPool[1000]) -> read loop.
//   * Framing: read 12-byte CHeadPacket, then m_nBodySize (<=2048) bytes of body,
//     store {owner, header+body} in a CQue slot, push to m_vPacketList.
//   * CProcess::PacketProcess drains m_vPacketList each tick and dispatches.
//   * Certify client TCP body is PLAINTEXT (DecodeBody/XOR 0xE5 below is vestigial -
//     uncalled in the binary; verified). The Game server encrypts its body via CSecure.
//
// PORTABILITY: m_nFd is a LOGICAL connection id (monotonic int), not a raw OS fd
// (Windows SOCKET doesn't fit int). connect.cpp maps id -> Asio socket.
// =============================================================================
#ifndef _CERTIFY_NET_H_
#define _CERTIFY_NET_H_

#include "Struct.h"   // CHeadPacket
#include "Que.h"      // CQue / CBufferPool / VectorPacketList (shared infra)
#include <vector>

// ---- TCP listener for clients ----
// Asio acceptor/io_context live behind m_pImpl (defined in connect.cpp). The
// CQue + packet list stay here because CProcess::PacketProcess drains them.
class CTCPSocket
{
public:
    CQue             m_cQue;
    VectorPacketList m_vPacketList;
    void*            m_pImpl;     // connect.cpp: acceptor + io_context handle

    CTCPSocket();
    ~CTCPSocket();

    bool InitListen(short nPort);          // create acceptor, start async_accept loop
    void Close();
    static void DecodeBody(char* sBody, int nSize);   // XOR 0xE5 (Certify)
};

// ---- STS listener (game servers connecting to certify) ----
class CSTSSocket
{
public:
    CQue             m_cQue;
    VectorPacketList m_vPacketList;
    void*            m_pImpl;

    CSTSSocket();
    ~CSTSSocket();

    bool InitListen(short nPort);
    void Close();
    static void DecodeBody(char* sBody, int nSize);
};

// HTTP helper (HackShield .hsb / patch fetch); thin Asio client in the port.
class CHTTPSocket
{
public:
    CHTTPSocket();
    ~CHTTPSocket();
};

// ---- connection helpers (implemented in connect.cpp) ----
// Send a fully-formed packet (header+body) to the connection with this logical id.
void  Net_Send(int nFd, const void* pData, int nLen);
// Close/cleanup a connection by logical id.
void  Net_Close(int nFd);
// Copy the peer IP (dotted) for a connection id into sIP (>=20 bytes).
int   Net_GetIP(int nFd, char* sIP);

#endif
