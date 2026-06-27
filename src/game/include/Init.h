// CInit (Game) - startup routines (init.cpp). Layout mirrors Certify (36 bytes,
// g_Init @080dedc0). main() order: InitLogo->InitSignal->InitMemory->InitConfig->
// InitLog->CSql::InitServer->CLoad::InitLoad->CThread::InitThread->TCPThread.
#ifndef _GAME_INIT_H_
#define _GAME_INIT_H_

class CPlayer;

class CInit
{
public:
    int  m_nCount;              // 0x00
    char m_strServerName[31];   // 0x04   ("Game<code>")
    char m_pad[1];

    bool InitLogo();                  // SVN/build banner
    bool InitSignal();                // signal handlers (POSIX sigaction; _WIN32-guarded layer)
    bool InitMemory();                // InitPlayer x1000 + number rooms[200]/teams[400]
    void InitLog(int serverCode);     // open log files

    // --- AhnLab HackShield call sites (shared HackShield.h / AhnHS.h, dummy-backed) ---
    bool InitServerHack();            // builds ../Hack/<code>.hsb, g_hServer = CreateServerObject
    bool InitClientHack(CPlayer* p);  // p->m_cSecure-adjacent client handle; no-op when HackCode==0

    void WriteBuildDate();            // stamp build date to log
};

#endif // _GAME_INIT_H_
