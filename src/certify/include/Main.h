// =============================================================================
// XKICK_Certify - Main.h  (umbrella header)
// Reconstructed from certify/XKICK_Certify (Aug 2010, unstripped) via Ghidra.
//
// Architecture (from main()/symbols):
//   global singletons + STS thread; epoll reactor in original -> Asio in port.
//   main(): InitLogo -> InitSignal -> InitMemory -> InitConfig -> InitLog ->
//           CSql::InitServer -> CLoad::InitLoad -> CThread::InitThread -> TCPThread()
//
// PORTABILITY (per PLAN.md mandate): only #pragma pack(1) WIRE structs
// (CHeadPacket + Protocol.h + CSetting/CMoney/CAddress) must stay byte-identical.
// Runtime classes below are modernized: std::thread/mutex, Asio sockets, no
// epoll/pthread in general code. AhnLab HackShield call sites are preserved
// (AhnHS.h / HackShield.h) and backed by dummy stubs (AhnHS_stub.cpp) until the
// real AhnLab files are supplied, so the tree builds on Windows+Linux.
// =============================================================================
#ifndef _CERTIFY_MAIN_H_
#define _CERTIFY_MAIN_H_

// ---- shared/common (wire format + infra contracts) ----
// Include.h (pulled in by Database.h) provides the std headers + `using std`
// that Struct.h/Protocol.h rely on, so it must come before them. Define.h must
// also precede Struct.h (it defines the size macros the wire structs use).
#include <cstdint>
#include <map>
#include <vector>
#include <string>
#include <cstring>

#include "Define.h"
#include "Database.h"    // CDatabase (MySQL Connector/C, std::mutex pool) -> Include.h
#include "Struct.h"      // CSetting(88) CMoney(16) CAddress(24) ... wire structs
#include "Item.h"        // Certify's 36-byte CItem (server-specific, not in Struct.h)
#include "Protocol.h"    // CHeadPacket + CCS*/CSC* packet layouts
#include "LogManager.h"
#include "Secure.h"
#include "CTable.h"      // portable CSV table API (Table_FindTable/Table_GetData)

// ---- forward declarations ----
class CConfig;  class CInit;   class CSql;    class CLoad;   class CThread;
class CMember;  class CServer; class CProcess; class CProcessSTS;
class CTCPSocket; class CSTSSocket; class CHTTPSocket;
class CQue; class CBufferPool;
class CNameTable; class CQuestTable; class CSchedule; class CEvent;

// ---- dispatch + container typedefs (recovered from RTTI/template symbols) ----
typedef void (*PFN_TCP)(CMember*, CHeadPacket*);   // client TCP handler
typedef void (*PFN_STS)(CServer*, CHeadPacket*);   // server-to-server handler
typedef std::map<int, PFN_TCP>            MapTCPFunction;
typedef std::map<int, PFN_STS>            MapSTSFunction;
// NOTE: VectorPacketList (the socket packet list) is std::vector<CBufferPool*>,
// defined in Net.h next to CBufferPool. It is intentionally NOT typedef'd here.
typedef std::vector<CMember*>             VectorMemberList;
typedef std::vector<CServer*>             VectorServerList;
typedef std::vector<CEvent*>              VectorEventList;
typedef std::map<int, CMember*>           MapMemberHash;
typedef std::map<int, CNameTable*>        CNameArray;
typedef std::map<int, CQuestTable*>       CQuestArray;
typedef std::map<int, CSchedule*>         CScheduleArray;
typedef std::map<int, char*>              CStringArray;   // notice text by seq

// ---- capacity constants (from g_MemberPool[1000], g_ServerPool[50]) ----
#define MAX_MEMBER_POOL  1000
#define MAX_SERVER_POOL  50

// ---- class headers ----
#include "AhnHS.h"       // AhnLab HackShield SDK surface (dummy-backed for now)
#include "HackShield.h"  // g_hServer + Packet{Hack}Request/Response call sites
#include "Config.h"
#include "Init.h"
#include "Sql.h"
#include "CertifyLoad.h"
#include "Member.h"
#include "ServerNode.h"
#include "Net.h"         // CQue, CBufferPool, CTCPSocket, CSTSSocket
#include "CertifyProcess.h"   // renamed from Process.h (collided with CRT <process.h>)
#include "Thread.h"

// ---- global singletons (addresses are the originals, for cross-reference) ----
extern CInit        g_Init;        // 0809a180
extern CConfig      g_Config;      // 0809a1c0
extern CSql         g_Sql;         // 0809a300
extern CLoad        g_Load;        // 08294200
extern CThread      g_Thread;      // 08294270
extern CSetting     g_Setting;     // 082941a0  (default setting template)
extern CProcess     g_Process;     // 0809ba20
extern CProcessSTS  g_ProcessSTS;  // 0809ba48
extern CTCPSocket   g_TCPSocket;   // 0809ba80
extern CSTSSocket   g_STSSocket;   // 08196a80
extern CHTTPSocket  g_HTTPSocket;  // 08291a80

extern CMember      g_MemberPool[MAX_MEMBER_POOL];  // 08294280
extern CServer      g_ServerPool[MAX_SERVER_POOL];  // 082dc6c0
extern VectorMemberList g_MemberList;  // 082dd728  (active members)
extern VectorServerList g_ServerList;  // 082dd734  (connected game servers)
extern MapMemberHash    g_MemberHash;  // 082dd740  (memberSeq -> CMember*)

// ---- command-name helpers (util.cpp) used by the packet logger ----
void GetCommandString(int nCommand, char* strOut);   // id -> name (debug/log)
int  IsCommandString(int nCommand);                   // 1 unless CM_TCP_PING

// ---- entry-point thread loops (connect.cpp / main.cpp) ----
void  TCPThread();      // main accept/event loop (Asio io_context.run)
void* STSThread(void*); // server-to-server loop (run on g_Thread)

#endif // _CERTIFY_MAIN_H_
