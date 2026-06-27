// =============================================================================
// connect.cpp - Asio reactor for XKICK_Game, reconstructed from the binary's
// epoll-based connect.cpp + TCPThread @080a... / STSThread @080a441e /
// UDPThread @080a438c. All Asio lives here so no OS/Asio headers leak elsewhere
// (PLAN.md mandate).
//
// Faithful behaviour (XKICK_Game1):
//   * Client TCP acceptor on g_Config.m_nTCPPort. Per accept:
//       CreatePlayer(id) (first free g_PlayerPool slot) -> read loop that frames
//       {12-byte CHeadPacket, m_nBodySize (<=2048) body} and queues it with the
//       owning CPlayer* (CTCPSocket::SavePacketInList @080711b2).
//   * Before a queued client packet is pushed, CTCPSocket::SecureLayer @0807151a
//       runs the CSecure RECV decode (see SecureLayerRecv below).
//   * STS uplink: an OUTBOUND connection to the master server on m_nSTSPort
//       (the Game is the client; SendSTS @08082d2e writes on the single fd).
//       Inbound STS packets are framed into the SAME g_TCPSocket queue but with a
//       CServer* owner, then drained by g_Process.PacketProcess().
//   * UDP: bind on m_nUDPPort, recvfrom loop; per datagram parse a CHeadPacket and
//       call g_Process.PacketProcessUDP(&from, pkt). No CQue ring (g_UDPSocket is
//       just a 4-byte fd in the binary).
//   * Queued packets are drained by CProcess::PacketProcess each tick.
//
// CSecure crypto (ACTIVE in Game; Certify used plain XOR 0xE5):
//   * SEND (packet.cpp WriteTCP @08082e86 -> CSecure::MakeSendPacket): for every
//     packet EXCEPT command 2000 (login result) and 0x7d1 (secure-init) and while
//     the player's CSecure is InitCheck()'d, the body bytes [12 .. 12+bodySize)
//     are encrypted, keyed by a freshly advanced send sequence written into
//     CHeadPacket.m_nSequence (offset 8). The header is NOT encrypted. SendPlayer
//     does this in packet.cpp BEFORE the framed write; connect.cpp's Net_Send just
//     ships the already-formed bytes.
//   * RECV (here, in SecureLayerRecv, reproducing SecureLayer @0807151a): command
//     2000 / 0x7d1 are passed through (0x7d1 also clears the player secure-enabled
//     byte @+0x538). Otherwise, if the player CSecure is active, MakeRecvPacket
//     decrypts the body in place; a sequence mismatch (-2) marks the packet
//     m_nSequence = -1 so the dispatcher drops it.
//
// Design notes (port):
//   * A single io_context on the main thread services the client acceptor, the STS
//     uplink, the UDP socket, and the periodic timer, so pool access stays
//     lock-free (the original ran three epoll loops). STSThread()/UDPThread() are
//     therefore thin stubs (the work is folded into the io_context).
//   * m_nFd is a LOGICAL connection id (monotonic int) -> Asio socket, keeping the
//     code int-fd-shaped and Windows-SOCKET-safe.
// =============================================================================
#include "Main.h"
#include "Net.h"

#include <asio.hpp>
#include <map>
#include <memory>
#include <cstring>
#include <cstdio>
#include <chrono>

using asio::ip::tcp;
using asio::ip::udp;

// ---- externs from player.cpp / server.cpp / packet.cpp ----
extern CPlayer* CreatePlayer(int nFd);          // first free g_PlayerPool slot @0809142e
extern void     ExitPlayer(CPlayer* pPlayer);   // flush + detach + close @080893d0
extern CServer* CreateStsServer(int nFd);       // single STS uplink CServer node
void            PutStsLogin();                  // SendSTS(CCSStsLogin) @08081ee2

namespace {

// STS id is a fixed sentinel id distinct from the monotonic client ids.
const int STS_ID = -1000;

struct Connection : std::enable_shared_from_this<Connection>
{
    tcp::socket       socket;
    int               id;
    bool              isSTS;
    void*             owner;       // CPlayer* (client) or CServer* (STS)
    char              header[12];
    std::vector<char> body;

    explicit Connection(asio::io_context& io)
        : socket(io), id(0), isSTS(false), owner(0) {}
};

typedef std::shared_ptr<Connection> ConnPtr;

asio::io_context        g_io;
std::map<int, ConnPtr>  g_conns;
int                     g_nextId = 1;

// UDP receive state (single global datagram socket, like g_UDPSocket's 4-byte fd)
std::unique_ptr<udp::socket> g_udp;
udp::endpoint                g_udpFrom;
char                         g_udpBuf[2048];

// forward declarations (mutually recursive read loop)
void enqueue(const ConnPtr& c, int bodySize);
void closeConn(const ConnPtr& c);
void readHeader(const ConnPtr& c);
void readBody(const ConnPtr& c, int bodySize);

// SecureLayer @0807151a - recv-side CSecure decode applied as the packet is queued.
// Reproduces: pass-through for 2000 and 0x7d1 (0x7d1 also clears the player's
// secure-enabled byte @+0x538); otherwise if the player CSecure is active, decrypt
// the body in place, and on a sequence mismatch mark m_nSequence = -1.
void SecureLayerRecv(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    if (pPacket->m_nCommand == 2000) return;           // CM_*_LOGIN result: plaintext
    if (pPacket->m_nCommand == 0x7d1)                  // secure-init handshake
    {
        // clear the player's "secure active" byte (CPlayer +0x538)
        pPlayer->m_hTCPSecure.m_bInit = 0;
        return;
    }
    CSecure* pSecure = pPlayer->GetTCPSecure();         // &CPlayer::m_hTCPSecure (+0x530)
    if (pSecure->InitCheck())
    {
        if (pSecure->MakeRecvPacket(pPacket) == -2)
            pPacket->m_nSequence = -1;                  // mismatch -> dispatcher drops
    }
}

// SavePacketInList @080711b2 - frame one fully-read packet into g_TCPSocket's CQue
// + packet list (both client and STS packets share this single queue in the binary).
void enqueue(const ConnPtr& c, int bodySize)
{
    CBufferPool* pPool = g_TCPSocket.m_cQue.GetBuffer();
    if (pPool == 0) { closeConn(c); return; }   // queue full (-2 / -4) -> drop conn

    pPool->m_pObject = c->owner;
    memcpy(pPool->m_sBuffer, c->header, 12);
    if (bodySize > 0) memcpy(pPool->m_sBuffer + 12, c->body.data(), bodySize);

    // Recv-side CSecure decode runs only for client (CPlayer) packets; STS packets
    // (CServer* owner) are not player-encrypted.
    if (!c->isSTS && c->owner)
        SecureLayerRecv((CPlayer*)c->owner, (CHeadPacket*)pPool->m_sBuffer);

    g_TCPSocket.m_vPacketList.push_back(pPool);
}

void closeConn(const ConnPtr& c)
{
    asio::error_code ec;
    c->socket.close(ec);
    if (c->owner && !c->isSTS)
    {
        ExitPlayer((CPlayer*)c->owner);   // @080893d0 (also closes the socket fd)
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
        // STS uplink to certify keeps the internal 12-byte header.
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

    // Official client wire header is 16 bytes: short cmd@0, short size@2, int seq@4,
    // 8 reserved@8. Translate into the internal 12-byte CHeadPacket so the rest of the
    // pipeline (CSecure decode, dispatch, handlers) is unchanged; body lands at offset 12.
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

void startAccept(tcp::acceptor& acc)
{
    ConnPtr c = std::make_shared<Connection>(g_io);
    acc.async_accept(c->socket,
        [&acc, c](const asio::error_code& ec)
        {
            if (!ec)
            {
                c->id    = g_nextId++;
                c->isSTS = false;
                g_conns[c->id] = c;

                c->owner = CreatePlayer(c->id);   // assign g_PlayerPool slot
                if (c->owner == 0)
                {
                    // CreatePlayer failed (pool full): log + drop (SetNewClient).
                    LOGOUT_ERROR("Can't Create Player Instance.\n");
                    closeConn(c);
                }
                else
                {
                    readHeader(c);
                }
            }
            startAccept(acc);   // keep accepting
        });
}

// ---- STS uplink (outbound) ----
void startStsRead(const ConnPtr& c);

void connectSTS()
{
    ConnPtr c = std::make_shared<Connection>(g_io);
    c->id    = STS_ID;
    c->isSTS = true;
    g_conns[c->id] = c;

    asio::error_code ec;
    // Master/STS server IP: the binary's STSThread connects to g_Config+0x14
    // (CConfig::m_sMasterIP), which CSql::InitServer fills from tb_game_server at
    // runtime (it is NOT the DB_IP at +0x00). Port is m_nSTSPort (also from DB).
    tcp::endpoint ep(asio::ip::make_address(g_Config.m_sMasterIP, ec),
                     (unsigned short)g_Config.m_nSTSPort);
    if (ec)
    {
        LOGOUT_ERROR("STS: bad master IP\n");
        return;
    }
    c->socket.async_connect(ep,
        [c](const asio::error_code& cec)
        {
            if (cec) { LOGOUT_ERROR("STS: connect to master failed\n"); g_conns.erase(STS_ID); return; }
            // owner is a single CServer node; server.cpp owns the global one.
            c->owner = CreateStsServer(c->id);
            PutStsLogin();          // SendSTS(CCSStsLogin) - PutStsLogin @08081ee2
            startStsRead(c);
        });
}

void startStsRead(const ConnPtr& c) { readHeader(c); }   // same 12+body framing

// ---- UDP receive loop (UDPThread @080a438c) ----
void startUdpReceive()
{
    g_udp->async_receive_from(asio::buffer(g_udpBuf, sizeof(g_udpBuf)), g_udpFrom,
        [](const asio::error_code& ec, std::size_t n)
        {
            // recvfrom < 1 is skipped in the binary; an error just re-arms.
            if (!ec && (int)n >= (int)sizeof(CHeadPacket))
            {
                CUDPAddress from;
                snprintf(from.m_sIP, sizeof(from.m_sIP), "%s",
                         g_udpFrom.address().to_string().c_str());
                from.m_nPort = g_udpFrom.port();
                g_Process.PacketProcessUDP(&from, (CHeadPacket*)g_udpBuf);
            }
            startUdpReceive();
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
        asio::write(it->second->socket, asio::buffer(pData, nLen), ec);
        return;
    }

    // Client: translate internal 12-byte-header packet to the official client's 16-byte
    // wire header (short cmd@0, short size@2, int seq@4, 8 reserved@8, body@16).
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
    if (bodyLen > 0) memcpy(wire.data() + 16, in + 12, bodyLen);
    asio::write(it->second->socket, asio::buffer(wire.data(), wire.size()), ec);
}

void Net_Close(int nFd)
{
    std::map<int, ConnPtr>::iterator it = g_conns.find(nFd);
    if (it != g_conns.end()) { asio::error_code ec; it->second->socket.close(ec); g_conns.erase(it); }
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

// PacketUDPRelay / UDPPing path: sendto on the single g_UDPSocket (@08081e02).
void Net_SendUDP(const CUDPAddress* pTo, const void* pData, int nLen)
{
    if (!g_udp || !pTo) return;
    asio::error_code ec;
    udp::endpoint ep(asio::ip::make_address(pTo->m_sIP, ec),
                     (unsigned short)pTo->m_nPort);
    if (ec) return;
    g_udp->send_to(asio::buffer(pData, nLen), ep, 0, ec);
}

// SendSTS @08082d2e: write a framed packet on the single STS uplink fd.
void Net_SendSTS(const void* pData, int nLen)
{
    std::map<int, ConnPtr>::iterator it = g_conns.find(STS_ID);
    if (it == g_conns.end()) return;
    asio::error_code ec;
    asio::write(it->second->socket, asio::buffer(pData, nLen), ec);
}

// int-returning variant: -1 if the STS uplink is absent, else 0.
int Net_SendSTS_Ret(const void* pData, int nLen)
{
    if (g_conns.find(STS_ID) == g_conns.end()) return -1;
    Net_SendSTS(pData, nLen);
    return 0;
}

// ---- CTCPSocket / CSTSSocket / CUDPSocket / CHTTPSocket ----
CTCPSocket::CTCPSocket()  : m_pImpl(0) {}
CTCPSocket::~CTCPSocket() {}
CSTSSocket::CSTSSocket()  : m_pImpl(0) {}
CSTSSocket::~CSTSSocket() {}
CUDPSocket::CUDPSocket()  : m_pImpl(0) {}
CUDPSocket::~CUDPSocket() {}

bool CTCPSocket::InitListen(short) { return true; } // acceptor created in TCPThread
void CTCPSocket::Close()           {}
bool CSTSSocket::InitConnect(const char*, short) { return true; } // dialed in TCPThread
void CSTSSocket::Close()           {}
bool CUDPSocket::InitBind(short)   { return true; } // bound in TCPThread
void CUDPSocket::Close()           {}

CHTTPSocket::CHTTPSocket()  {}
CHTTPSocket::~CHTTPSocket() {}

// ---- main loops ----
// TCPThread: bring up the client acceptor + UDP socket + STS uplink + the periodic
// process timer, then run the single io_context (replaces three epoll loops).
void TCPThread()
{
    static tcp::acceptor tcpAcc(g_io, tcp::endpoint(tcp::v4(), (unsigned short)g_Config.m_nTCPPort));
    tcpAcc.set_option(asio::socket_base::reuse_address(true));
    startAccept(tcpAcc);

    // UDP datagram socket (g_UDPSocket): bind m_nUDPPort, start recv loop.
    g_udp.reset(new udp::socket(g_io, udp::endpoint(udp::v4(), (unsigned short)g_Config.m_nUDPPort)));
    startUdpReceive();

    // STS uplink: outbound connect to the master server.
    connectSTS();

    // Periodic tick: drain queued packets and run time-based maintenance (~10ms).
    static asio::steady_timer timer(g_io);
    struct Tick {
        static void run(const asio::error_code&)
        {
            g_Process.PacketProcess();   // drains g_TCPSocket.m_vPacketList (TCP + STS)
            g_Process.TimeProcess();
            timer.expires_after(std::chrono::milliseconds(10));
            timer.async_wait(&Tick::run);
        }
    };
    timer.expires_after(std::chrono::milliseconds(10));
    timer.async_wait(&Tick::run);

    g_io.run();
}

// STS + UDP are serviced by the single io_context above, so these are idle stubs
// (kept for structural fidelity with the binary's STSThread/UDPThread).
void* STSThread(void*) { return 0; }
void* UDPThread(void*) { return 0; }
