// =============================================================================
// XKICK_Game - GameProcess.h
// class CProcess : the single packet-dispatch + periodic-task hub (process.cpp).
//
// Reverse-engineering finding: unlike Certify (which splits CProcess / CProcessSTS),
// XKICK_Game uses ONE CProcess that owns THREE dispatch maps — client-TCP,
// client-UDP, and server-to-server (STS) — plus the periodic timers.
//
// Layout recovered from Ghidra (XKICK_Game1), total size 0x58 = 88 bytes:
//   CProcess::C1 (ctor)        @080bb298
//   CProcess::InitFunction     @080a08b4   (builds all three maps)
//   CProcess::PacketProcess    @080a1926   (drain client TCP list + dispatch)
//   CProcess::PacketProcessUDP @080a1e1c   void(sockaddr_in*, CHeadPacket*)
//   CProcess::TimeProcess      @080a1e54
//   CProcess::CheckPingTime    @080a1e9e   CheckLogTime @080a203a  CheckActionTime @080a20ea
//
//   +0x00 std::map<int, void(*)(CPlayer*,  CHeadPacket*)>   TCP handlers
//   +0x18 std::map<int, void(*)(sockaddr_in*, CHeadPacket*)> UDP handlers (raw type)
//   +0x30 std::map<int, void(*)(CServer*,  CHeadPacket*)>   STS handlers
//   +0x48 time_t m_tPingTime    (ctor = 0)
//   +0x4c time_t m_tLogTime     (ctor = 0)
//   +0x50 time_t m_tActionTime  (ctor = 0)
//   +0x54 int    m_nMinute      (ctor = -1)
//
// PORTABILITY: the original UDP handler arg is `sockaddr_in*` (an OS type). Per the
// no-OS-headers mandate we express it portably as `CUDPAddress*` (a small POD that
// the Net layer maps to the platform endpoint). The raw original type is noted in a
// comment so the wire/dispatch semantics stay traceable.
// =============================================================================
#ifndef _GAME_PROCESS_H_
#define _GAME_PROCESS_H_

#include "Struct.h"     // CHeadPacket
#include <map>
#include <ctime>

class CPlayer;
class CServer;

// Portable stand-in for the original sockaddr_in* UDP source endpoint.
// (Raw original handler signature: void(*)(sockaddr_in*, CHeadPacket*).)
struct CUDPAddress;     // defined in the portable Net layer

typedef std::map<int, void (*)(CPlayer*,      CHeadPacket*)> MapTCPFunction; // +0x00
typedef std::map<int, void (*)(CUDPAddress*,  CHeadPacket*)> MapUDPFunction; // +0x18 (was sockaddr_in*)
typedef std::map<int, void (*)(CServer*,      CHeadPacket*)> MapSTSFunction; // +0x30

class CProcess
{
public:
    MapTCPFunction m_mTCPFunctionHash;  // 0x00  CM_* (client) -> handler(CPlayer*,  CHeadPacket*)
    MapUDPFunction m_mUDPFunctionHash;  // 0x18  CM_* (UDP)    -> handler(CUDPAddress*, CHeadPacket*)
    MapSTSFunction m_mSTSFunctionHash;  // 0x30  CM_STS_* -> handler(CServer*,  CHeadPacket*)
    time_t         m_tPingTime;         // 0x48
    time_t         m_tLogTime;          // 0x4c
    time_t         m_tActionTime;       // 0x50
    int            m_nMinute;           // 0x54

    CProcess();                         // @080bb298 (zeroes timers, m_nMinute=-1, InitFunction)
    ~CProcess();

    void InitFunction();                // @080a08b4 register TCP + UDP + STS handlers
    void PacketProcess();               // @080a1926 drain client packet list + dispatch
    void PacketProcessUDP(CUDPAddress* pFrom, CHeadPacket* pPacket); // @080a1e1c
    void TimeProcess();                 // @080a1e54 per-tick periodic work
    void CheckPingTime(time_t tCurrentTime);    // @080a1e9e
    void CheckLogTime(time_t tCurrentTime);     // @080a203a
    void CheckActionTime(time_t tCurrentTime);  // @080a20ea
};

#endif // _GAME_PROCESS_H_
