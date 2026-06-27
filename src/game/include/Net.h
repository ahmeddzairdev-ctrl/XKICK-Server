// =============================================================================
// Net.h - socket/reactor layer (connect.cpp) for XKICK_Game, Asio-backed.
// Reconstructed from the binary's epoll-based connect.cpp + TCPThread/STSThread/
// UDPThread (XKICK_Game1, unstripped). Mirrors certify/Net.h but reproduces the
// THREE Game differences confirmed by RE:
//
//   1. CPlayer (not CMember). Accept -> CreatePlayer (first free slot of
//      g_PlayerPool[1000], m_nState==0) -> InitPlayer + set m_nFd + dotted IP.
//      (CTCPSocket::SetNewClient @08070efe ; CreatePlayer @0809142e ;
//       SavePacketInList @080711b2 ; ExitPlayer @080893d0)
//
//   2. THREE network paths:
//        * client TCP : g_TCPSocket, listen on g_Config.m_nTCPPort (acceptor)
//        * STS        : g_STSSocket, OUTBOUND connect to the master/certify server
//                       on m_nSTSPort (the Game is the client; SendSTS @08082d2e
//                       writes on the single g_STSSocket fd). STS packets are
//                       framed into g_TCPSocket's queue with a CServer* owner and
//                       drained by the same g_Process.PacketProcess().
//        * UDP        : g_UDPSocket, a lightweight datagram socket (4-byte fd in
//                       the binary). UDPThread @080a438c does recvfrom in a loop;
//                       per datagram it parses a CHeadPacket and calls
//                       g_Process.PacketProcessUDP(&from, pkt). (No CQue ring.)
//
//   3. CSecure rolling cipher is ACTIVE (Certify used plain XOR 0xE5). Per-CPlayer
//      CSecure @0x530 (CPlayer::GetTCPSecure @08087264). See connect.cpp for the
//      exact send/recv keying. The cipher itself lives in common/Secure.h; Net only
//      supplies the framed write and invokes the recv-side decode (SecureLayer).
//
// PORTABILITY: m_nFd is a LOGICAL connection id (monotonic int), not a raw OS fd
// (Windows SOCKET doesn't fit int). connect.cpp maps id -> Asio socket. The UDP
// source endpoint, originally sockaddr_in*, is expressed as CUDPAddress (a POD)
// per the no-OS-headers mandate. ALL Asio/OS code stays in connect.cpp.
// =============================================================================
#ifndef _GAME_NET_H_
#define _GAME_NET_H_

#include "Struct.h"   // CHeadPacket
#include "Que.h"      // CQue / CBufferPool / VectorPacketList (shared infra)
#include <vector>

// -----------------------------------------------------------------------------
// CUDPAddress - portable stand-in for the original sockaddr_in* UDP endpoint.
// Layout mirrors what ConvertSockaddrToCAddress @08072870 extracted: the dotted
// IP and the host-order port. connect.cpp maps this to/from an Asio udp::endpoint.
// Kept a trivially-copyable POD so it can be stored on the stack and handed to
// g_Process.PacketProcessUDP() and Net_SendUDP() exactly like the binary did.
// -----------------------------------------------------------------------------
struct CUDPAddress
{
    char         m_sIP[20];   // dotted-decimal peer IP
    unsigned int m_nPort;     // host-order peer port
};

// ---- TCP listener for clients (g_TCPSocket) ----
// Asio acceptor/io_context live behind m_pImpl (defined in connect.cpp). The CQue
// + packet list stay here because CProcess::PacketProcess drains them (the binary
// stored both client packets and STS packets into this one queue).
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
};

// ---- STS link (g_STSSocket) ----
// In the Game this is an OUTBOUND connection to the master server, not a listener.
// It carries the same CQue ring shape as CTCPSocket (the binary used a CTCPSocket
// here too), but in the port the single io_context services it; SendServer() ->
// Net_SendSTS writes the framed packet on it.
class CSTSSocket
{
public:
    CQue             m_cQue;
    VectorPacketList m_vPacketList;
    void*            m_pImpl;

    CSTSSocket();
    ~CSTSSocket();

    bool InitConnect(const char* sIP, short nPort);  // dial the master server
    void Close();
};

// ---- UDP datagram socket (g_UDPSocket) ----
// The binary's g_UDPSocket is just a 4-byte fd. Here it is a thin handle whose
// Asio udp::socket lives behind m_pImpl. The async receive loop frames one
// CHeadPacket per datagram and dispatches via g_Process.PacketProcessUDP().
class CUDPSocket
{
public:
    void* m_pImpl;     // connect.cpp: udp::socket handle (binary: 4-byte fd)

    CUDPSocket();
    ~CUDPSocket();

    bool InitBind(short nPort);   // bind + start async receive loop
    void Close();
};

// HTTP helper (patch / external billing fetch); thin Asio client in the port.
class CHTTPSocket
{
public:
    CHTTPSocket();
    ~CHTTPSocket();
};

// ---- connection helpers (implemented in connect.cpp) ----
// Send a fully-formed packet (header+body) to the TCP connection with this id.
// (Used by WriteTCP/SendPlayer AFTER CSecure::MakeSendPacket has encrypted body.)
void  Net_Send(int nFd, const void* pData, int nLen);
// Close/cleanup a TCP connection by logical id.
void  Net_Close(int nFd);
// Copy the peer IP (dotted) for a connection id into sIP (>=20 bytes).
int   Net_GetIP(int nFd, char* sIP);
// Send one datagram to a UDP peer (PacketUDPRelay/UDPPing path; sendto on g_UDPSocket).
void  Net_SendUDP(const CUDPAddress* pTo, const void* pData, int nLen);
// Send a framed packet on the single STS uplink (SendSTS @08082d2e).
void  Net_SendSTS(const void* pData, int nLen);
// int-returning variant used by packet.cpp's SendSTS (0 ok, <0 on no uplink).
int   Net_SendSTS_Ret(const void* pData, int nLen);

#endif // _GAME_NET_H_
