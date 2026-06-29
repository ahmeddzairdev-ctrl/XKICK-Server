// =============================================================================
// connect.cpp - Asio reactor for XKICK_Certify (reconstructed from the binary's
// epoll-based connect.cpp + TCPThread). All Asio lives here so no OS/Asio headers
// leak elsewhere (PLAN.md mandate).
//
// Faithful behaviour:
//   * Two TCP acceptors: clients (m_nTCPPort) and game servers / STS (m_nSTSPort).
//   * Per accept: CreateMember/CreateServer from the fixed pools, then a read
//     loop that frames {12-byte CHeadPacket, <=2048-byte body} and queues it.
//   * Queued packets are drained by CProcess::PacketProcess each tick.
//   * NOTE: the Certify client TCP body is PLAINTEXT on the wire. CTCPSocket::DecodeBody
//     (XOR 0xE5) exists but is VESTIGIAL/UNCALLED in the binary (verified: zero xrefs;
//     SavePacketInList/PacketProcess pass the body raw), so it is intentionally never
//     invoked here. (The Game server, by contrast, encrypts its body via CSecure.)
//
// Design notes (port):
//   * Single io_context on the main thread services clients + STS + the periodic
//     timer, so pool access stays lock-free (the original ran one epoll loop per
//     thread). STSThread() is therefore a stub.
//   * m_nFd is a LOGICAL connection id (monotonic int) -> Asio socket, so the
//     code stays int-fd-shaped and Windows-SOCKET-safe.
// =============================================================================
#include "Main.h"

#include <asio.hpp>
#include <map>
#include <memory>
#include <cstring>
#include <cstdio>

using asio::ip::tcp;

// ---- externs from member.cpp / server.cpp (Phase 3) ----
extern CMember* CreateMember(int nFd);              // assign a g_MemberPool slot
extern void     ExitMember(CMember* pMember);       // free slot + cleanup
extern CServer* CreateServer(int nFd);              // assign a g_ServerPool slot
extern void     ExitServer(CServer* pServer);

namespace {

struct Connection : std::enable_shared_from_this<Connection>
{
    tcp::socket      socket;
    int              id;
    bool             isSTS;
    void*            owner;        // CMember* (client) or CServer* (STS)
    char             header[12];
    std::vector<char> body;

    explicit Connection(asio::io_context& io)
        : socket(io), id(0), isSTS(false), owner(0) {}
};

typedef std::shared_ptr<Connection> ConnPtr;

asio::io_context           g_io;
std::map<int, ConnPtr>     g_conns;
int                        g_nextId = 1;

// forward declarations (mutually recursive read loop)
void enqueue(const ConnPtr& c, int bodySize);
void closeConn(const ConnPtr& c);
void readHeader(const ConnPtr& c);
void readBody(const ConnPtr& c, int bodySize);

// Frame one fully-read packet into the owning socket's CQue + packet list,
// reproducing CTCPSocket::SavePacketInList.
void enqueue(const ConnPtr& c, int bodySize)
{
    CQue&             que  = c->isSTS ? g_STSSocket.m_cQue       : g_TCPSocket.m_cQue;
    VectorPacketList& list = c->isSTS ? g_STSSocket.m_vPacketList: g_TCPSocket.m_vPacketList;

    CBufferPool* pPool = que.GetBuffer();
    if (pPool == 0) { closeConn(c); return; }   // queue full -> drop client (was -4)

    pPool->m_pObject = c->owner;
    memcpy(pPool->m_sBuffer, c->header, 12);
    if (bodySize > 0) memcpy(pPool->m_sBuffer + 12, c->body.data(), bodySize);
    list.push_back(pPool);
}

void closeConn(const ConnPtr& c)
{
    asio::error_code ec;
    c->socket.close(ec);
    if (c->owner)
    {
        if (c->isSTS) ExitServer((CServer*)c->owner);
        else          ExitMember((CMember*)c->owner);
        c->owner = 0;
    }
    g_conns.erase(c->id);
}

void readBody(const ConnPtr& c, int bodySize)
{
    if (bodySize == 0) { enqueue(c, 0); readHeader(c); return; }
    c->body.assign(bodySize, 0);
    asio::async_read(c->socket, asio::buffer(c->body.data(), bodySize),
        [c, bodySize](const asio::error_code& ec, std::size_t n)
        {
            if (ec || (int)n != bodySize) { closeConn(c); return; }
            enqueue(c, bodySize);
            readHeader(c);
        });
}

void readHeader(const ConnPtr& c)
{
    if (c->isSTS)
    {
        // Server-to-server (STS) keeps the internal 12-byte header.
        asio::async_read(c->socket, asio::buffer(c->header, 12),
            [c](const asio::error_code& ec, std::size_t n)
            {
                if (ec || n != 12) { closeConn(c); return; }
                CHeadPacket head;
                memcpy(&head, c->header, 12);
                if (head.m_nBodySize < 0 || head.m_nBodySize > 2048)
                {
                    LOGOUT_ERROR("PACKET SIZE OVERFLOW!!\n");
                    closeConn(c);
                    return;
                }
                readBody(c, head.m_nBodySize);
            });
        return;
    }

    // Official client wire header is 16 bytes: short m_nCommand@0, short m_nBodySize@2,
    // int m_nSequence@4, 8 reserved bytes@8. Translate it into the server's internal
    // 12-byte CHeadPacket {int cmd; int size; int seq} so the rest of the pipeline is
    // unchanged (body then lands at internal offset 12 as every wire struct expects).
    auto wire = std::make_shared<std::vector<char>>(16);
    asio::async_read(c->socket, asio::buffer(wire->data(), 16),
        [c, wire](const asio::error_code& ec, std::size_t n)
        {
            if (ec || n != 16) { closeConn(c); return; }
            unsigned short wcmd = 0, wsize = 0; int wseq = 0;
            memcpy(&wcmd,  wire->data() + 0, 2);
            memcpy(&wsize, wire->data() + 2, 2);
            memcpy(&wseq,  wire->data() + 4, 4);
            int cmd = wcmd, size = wsize;
            memcpy(c->header + 0, &cmd,  4);
            memcpy(c->header + 4, &size, 4);
            memcpy(c->header + 8, &wseq, 4);
            if (size < 0 || size > 2048)
            {
                LOGOUT_ERROR("PACKET SIZE OVERFLOW!!\n");
                closeConn(c);
                return;
            }
            readBody(c, size);
        });
}

void startAccept(tcp::acceptor& acc, bool isSTS)
{
    ConnPtr c = std::make_shared<Connection>(g_io);
    acc.async_accept(c->socket,
        [&acc, isSTS, c](const asio::error_code& ec)
        {
            if (!ec)
            {
                c->id    = g_nextId++;
                c->isSTS = isSTS;
                g_conns[c->id] = c;

                if (isSTS) c->owner = CreateServer(c->id);
                else       c->owner = CreateMember(c->id);

                if (c->owner == 0)
                {
                    LOGOUT_ERROR("WARNING: Can't Make %s Instance.\n", isSTS ? "Server" : "Member");
                    closeConn(c);
                }
                else
                {
                    readHeader(c);
                }
            }
            startAccept(acc, isSTS);   // keep accepting
        });
}

} // namespace

// ---- helpers exposed to packet.cpp ----
void Net_Send(int nFd, const void* pData, int nLen)
{
    std::map<int, ConnPtr>::iterator it = g_conns.find(nFd);
    if (it == g_conns.end()) return;
    asio::error_code ec;

    if (it->second->isSTS)
    {
        // Server-to-server keeps the internal 12-byte header.
        asio::write(it->second->socket, asio::buffer(pData, nLen), ec);
        return;
    }

    // Client: translate the internal 12-byte-header packet into the official client's
    // 16-byte wire header (short cmd@0, short size@2, int seq@4, 8 reserved@8, body@16).
    const char* in = (const char*)pData;
    int cmd = 0, size = 0, seq = 0;
    memcpy(&cmd,  in + 0, 4);
    memcpy(&size, in + 4, 4);
    memcpy(&seq,  in + 8, 4);
    int bodyLen = nLen - 12; if (bodyLen < 0) bodyLen = 0;
    std::vector<char> wire(16 + bodyLen, 0);
    unsigned short wcmd = (unsigned short)cmd, wsize = (unsigned short)size;
    memcpy(wire.data() + 0, &wcmd,  2);
    memcpy(wire.data() + 2, &wsize, 2);
    memcpy(wire.data() + 4, &seq,   4);
    // bytes 8..15 left zero (client sends uninitialized there; presumed reserved)
    if (bodyLen > 0) memcpy(wire.data() + 16, in + 12, bodyLen);
    asio::write(it->second->socket, asio::buffer(wire.data(), wire.size()), ec);
}

void Net_Close(int nFd)
{
    std::map<int, ConnPtr>::iterator it = g_conns.find(nFd);
    if (it != g_conns.end())
    {
        asio::error_code ec;
        // Graceful close: half-shutdown sends a FIN AFTER the OS flushes any queued TX,
        // so a reply written immediately before the close (e.g. the CM_CERTIFY_EXIT ack
        // that gates the client's hop to the game server) is delivered instead of being
        // discarded by an abortive RST. The Linux original relied on this close semantics.
        it->second->socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        it->second->socket.close(ec);
        g_conns.erase(it);
    }
}

int Net_GetIP(int nFd, char* sIP)
{
    std::map<int, ConnPtr>::iterator it = g_conns.find(nFd);
    if (it == g_conns.end()) { if (sIP) sIP[0] = '\0'; return -1; }
    asio::error_code ec;
    tcp::endpoint ep = it->second->socket.remote_endpoint(ec);
    if (ec) { sIP[0] = '\0'; return -1; }
    snprintf(sIP, 0x14, "%s", ep.address().to_string().c_str());
    return 0;
}

// ---- CTCPSocket / CSTSSocket ----
CTCPSocket::CTCPSocket()  : m_pImpl(0) {}
CTCPSocket::~CTCPSocket() {}
CSTSSocket::CSTSSocket()  : m_pImpl(0) {}
CSTSSocket::~CSTSSocket() {}

void CTCPSocket::DecodeBody(char* sBody, int nSize)
{
    for (int i = 0; i < nSize; ++i) sBody[i] = (char)(sBody[i] ^ 0xe5);
}
void CSTSSocket::DecodeBody(char* sBody, int nSize)
{
    for (int i = 0; i < nSize; ++i) sBody[i] = (char)(sBody[i] ^ 0xe5);
}

bool CTCPSocket::InitListen(short) { return true; } // acceptor created in TCPThread
void CTCPSocket::Close()           {}
bool CSTSSocket::InitListen(short) { return true; }
void CSTSSocket::Close()           {}

CHTTPSocket::CHTTPSocket()  {}
CHTTPSocket::~CHTTPSocket() {}

// HttpSendURL - external billing GET (the binary's CHTTPSocket::SendURL hit a
// hardcoded internal billing server with raw sockets). Reached only when
// g_Config.m_nSystem == 2001. Implemented as a portable Asio HTTP/1.0 GET; the
// raw response text is copied into sOut (PacketCertifyLogin scans it for "true").
int HttpSendURL(const char* sHost, int nPort, const char* sURL, char* sOut, int nOutSize)
{
    if (sOut && nOutSize > 0) sOut[0] = '\0';
    try
    {
        asio::io_context io;
        tcp::socket sock(io);
        tcp::endpoint ep(asio::ip::make_address(sHost), (unsigned short)nPort);
        asio::error_code ec;
        sock.connect(ep, ec);
        if (ec) return -3;

        char req[1100];
        snprintf(req, sizeof(req), "GET %s\r\n", sURL);
        asio::write(sock, asio::buffer(req, strlen(req)), ec);
        if (ec) { sock.close(ec); return -3; }

        int total = 0;
        for (;;)
        {
            char buf[1024];
            std::size_t n = sock.read_some(asio::buffer(buf, sizeof(buf)), ec);
            if (n == 0 || ec) break;
            if (sOut && total + (int)n < nOutSize)
            {
                memcpy(sOut + total, buf, n);
                total += (int)n;
            }
        }
        if (sOut && total < nOutSize) sOut[total] = '\0';
        sock.close(ec);
        return 0;
    }
    catch (...)
    {
        LOGOUT_ERROR("DEATH: HTTP socket() error\n");
        return -1;
    }
}

// CreateServer: first free slot of g_ServerPool[50] (mirror of CreateMember).
CServer* CreateServer(int nFd)
{
    for (int i = 0; i < MAX_SERVER_POOL; ++i)
    {
        if (g_ServerPool[i].m_nState == 0)
        {
            CServer* p = &g_ServerPool[i];
            p->InitServer();
            p->m_nState = 1;
            p->m_nFd    = nFd;
            Net_GetIP(nFd, p->m_cAddress.m_sIP);
            return p;
        }
    }
    return 0;
}

// ---- main loops ----
// TCPThread: set up both acceptors + the periodic process timer, then run the
// single io_context (replaces the original epoll loop). Never returns normally.
void TCPThread()
{
    static tcp::acceptor tcpAcc(g_io, tcp::endpoint(tcp::v4(), (unsigned short)g_Config.m_nTCPPort));
    static tcp::acceptor stsAcc(g_io, tcp::endpoint(tcp::v4(), (unsigned short)g_Config.m_nSTSPort));
    tcpAcc.set_option(asio::socket_base::reuse_address(true));
    stsAcc.set_option(asio::socket_base::reuse_address(true));

    startAccept(tcpAcc, false);
    startAccept(stsAcc, true);

    // Periodic tick: drain queued packets and run time-based maintenance.
    static asio::steady_timer timer(g_io);
    struct Tick {
        static void run(const asio::error_code&)
        {
            g_Process.PacketProcess();      // drains g_TCPSocket.m_vPacketList
            g_ProcessSTS.PacketProcess();   // drains g_STSSocket.m_vPacketList
            g_Process.TimeProcess();
            timer.expires_after(std::chrono::milliseconds(10));
            timer.async_wait(&Tick::run);
        }
    };
    timer.expires_after(std::chrono::milliseconds(10));
    timer.async_wait(&Tick::run);

    g_io.run();
}

// STS is serviced by the single io_context above, so this thread is idle.
void* STSThread(void*) { return 0; }
