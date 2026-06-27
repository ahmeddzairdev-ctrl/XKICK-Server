// CProcess / CProcessSTS - packet dispatch + periodic tasks (process.cpp).
// CProcess handles client (TCP) packets; CProcessSTS handles server-to-server.
// Layouts from Ghidra: CProcess 40 bytes, CProcessSTS 28 bytes.
#ifndef _CERTIFY_PROCESS_H_
#define _CERTIFY_PROCESS_H_

// Self-contained (does not rely on Main.h include ordering). The dispatch
// typedefs below are identical to Main.h's; redefining a typedef to the same
// type is legal C++, so the two coexist.
#include "Struct.h"   // CHeadPacket
#include <map>
#include <ctime>

class CMember;
class CServer;

typedef std::map<int, void (*)(CMember*, CHeadPacket*)> MapTCPFunction;
typedef std::map<int, void (*)(CServer*, CHeadPacket*)> MapSTSFunction;

class CProcess
{
public:
    MapTCPFunction m_mTCPFunctionHash;  // 0   command id -> handler(CMember*, CHeadPacket*)
    time_t         m_tPingTime;         // 24
    time_t         m_tLogTime;          // 28
    time_t         m_tActionTime;       // 32
    int            m_nMinute;           // 36

    CProcess();
    ~CProcess();

    void InitFunction();        // register all TCP handlers into m_mTCPFunctionHash
    void PacketProcess();       // drain g_TCPSocket.m_vPacketList; dispatch (body is plaintext)
    void TimeProcess();         // per-tick periodic work (ping/log/action timers)
    void CheckPingTime(time_t tCurrentTime);
    void CheckLogTime(time_t tCurrentTime);
    void CheckActionTime(time_t tCurrentTime);
};

class CProcessSTS
{
public:
    MapSTSFunction m_mSTSFunctionHash;  // 0   command id -> handler(CServer*, CHeadPacket*)
    time_t         m_nLastPingTime;     // 24

    CProcessSTS();
    ~CProcessSTS();

    void InitFunction();
    void PacketProcess();       // drain g_STSSocket.m_vPacketList + dispatch
};

#endif
