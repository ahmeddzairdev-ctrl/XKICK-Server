// =============================================================================
// globals.cpp - definitions of the Certify global singletons + fixed pools that
// Main.h declares extern. The binary placed these at fixed .bss addresses; here
// they are ordinary file-scope globals (construction order within this TU is
// top-to-bottom, which is fine: nothing here touches another global at ctor).
// =============================================================================
#include "Main.h"

CInit        g_Init;
CConfig      g_Config;
CSql         g_Sql;
CLoad        g_Load;
CThread      g_Thread;
CSetting     g_Setting;        // default setting template (seeded by CSql::InitServer)
CProcess     g_Process;
CProcessSTS  g_ProcessSTS;
CTCPSocket   g_TCPSocket;
CSTSSocket   g_STSSocket;
CHTTPSocket  g_HTTPSocket;

CMember      g_MemberPool[MAX_MEMBER_POOL];
CServer      g_ServerPool[MAX_SERVER_POOL];
VectorMemberList g_MemberList;
VectorServerList g_ServerList;
MapMemberHash    g_MemberHash;
