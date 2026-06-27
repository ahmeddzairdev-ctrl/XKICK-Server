// init.cpp - CInit startup routines + the process signal handler
// (reconstructed from XKICK_Certify). Signal handling is the one place that is
// genuinely platform-specific, so it is quarantined behind _WIN32 here; the rest
// is portable. SVN revision for the banner was 0x9FF (2559).
#include "Main.h"
#include "HackShield.h"
#include "LogManager.h"
#include <cstdio>
#include <cstring>
#include <csignal>
#include <cstdlib>

// Defined in util.cpp: flushes/persists state on a clean SIGINT shutdown.
extern void ExitMessage();

#define CERTIFY_SVN_REVISION 0x9FF   // 2559

// ---- process-wide fatal/term signal handler (shared logic) ----
static void SignalKill(int nSigNum)
{
    static bool bExe = false;
    if (bExe) return;                 // re-entrancy guard
    bExe = true;

    g_Sql.InitMemberLogin();          // mark all members logged-out in DB

    char sName[16];
    memset(sName, 0, sizeof(sName));
    switch (nSigNum)
    {
        case SIGINT:  snprintf(sName, 10, "%s", "SIGINT"); ExitMessage(); return; // graceful
        case SIGILL:  snprintf(sName, 10, "%s", "SIGILL");  break;
        case SIGABRT: snprintf(sName, 10, "%s", "SIGABRT"); break;
        case SIGFPE:  snprintf(sName, 10, "%s", "SIGFPE");  break;
        case SIGSEGV: snprintf(sName, 10, "%s", "SIGSEGV"); break;
        case SIGTERM: snprintf(sName, 10, "%s", "SIGTERM"); break;
        default:      snprintf(sName, 10, "%d", nSigNum);   break;
    }

    _AhnHS_CloseServerHandle(g_hServer);
    LOGOUT_ERROR("DEATH: %s(pid:%d)\n", sName, (int)
#ifdef _WIN32
        _getpid()
#else
        getpid()
#endif
    );
    LOGOUT_OPERATE("[EXIT SERVER] => Abnormal Terminate Process : %s\n", sName);

    if (nSigNum == SIGINT) exit(1);
    abort();
}

bool CInit::InitLogo()
{
    printf("\n");
    printf("********************************************\n");
    printf("**\x1b[33m XKicks Certify Server SVN Revision \x1b[31m%03d\x1b[36m \x1b[0m**\n",
           CERTIFY_SVN_REVISION);
    printf("********************************************\n");
    return true;
}

bool CInit::InitSignal()
{
#ifndef _WIN32
    struct sigaction tAct, tAct2;
    memset(&tAct, 0, sizeof(tAct));
    memset(&tAct2, 0, sizeof(tAct2));
    tAct.sa_handler  = SignalKill;
    tAct2.sa_handler = SIG_IGN;
    sigemptyset(&tAct.sa_mask);
    sigfillset(&tAct.sa_mask);

    // Fatal/terminating signals -> SignalKill (faithful to the original set).
    const int killSigs[] = { SIGSEGV, SIGABRT, SIGILL, SIGTRAP, SIGSYS,
                             SIGXCPU, SIGXFSZ, SIGFPE, SIGINT, SIGTERM };
    for (int s : killSigs) sigaction(s, &tAct, NULL);

    // Everything else -> ignored (later assignments win, matching the binary:
    // SIGQUIT/SIGTRAP/SIGABRT end up ignored).
    const int ignSigs[] = { SIGUSR1, SIGUSR2, SIGCHLD, SIGQUIT, SIGPIPE, SIGHUP,
                            SIGTRAP, SIGABRT, SIGBUS, SIGKILL, SIGALRM, SIGCONT,
                            SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU, SIGURG, SIGVTALRM,
                            SIGPROF, SIGWINCH, SIGIO, SIGPWR };
    for (int s : ignSigs) sigaction(s, &tAct2, NULL);
#else
    // Windows CRT supports a small subset; map the meaningful ones.
    signal(SIGINT,  SignalKill);
    signal(SIGTERM, SignalKill);
    signal(SIGSEGV, SignalKill);
    signal(SIGABRT, SignalKill);
    signal(SIGFPE,  SignalKill);
    signal(SIGILL,  SignalKill);
#endif
    return true;
}

bool CInit::InitMemory()
{
    m_nCount = 0;
    for (int i = 0; i < MAX_MEMBER_POOL; ++i) g_MemberPool[i].m_nState = 0;
    for (int i = 0; i < MAX_SERVER_POOL; ++i) g_ServerPool[i].m_nState = 0;
    return true;
}

void CInit::InitLog(int nServerCode)
{
    snprintf(m_strServerName, 0x1f, "Certify%d_", nServerCode);
    CLogManager::LOG_SYSTEM_NAME = m_strServerName;   // surviving LogManager field name
}

void CInit::WriteBuildDate()
{
    LOGOUT_OPERATE("[BUILD] %s %s\n", __DATE__, __TIME__);
}
