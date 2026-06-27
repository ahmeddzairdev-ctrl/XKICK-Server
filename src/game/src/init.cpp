// init.cpp - CInit startup routines + the process signal handler (reconstructed
// from XKICK_Game1: InitMemory @080742ac, main @080ba0c4). Signal handling is the
// one genuinely platform-specific spot, quarantined behind _WIN32; the rest is
// portable.
#include "Main.h"
#include "HackShield.h"
#include "LogManager.h"
#include <cstdio>
#include <cstring>
#include <csignal>
#include <cstdlib>

// Defined in kicks_common util.cpp: clean SIGINT shutdown banner + exit(0).
extern void ExitMessage();

// AhnHS anti-hack server handle (HackShield.h declares it extern "defined in init.cpp").
AHNHS_SERVER_HANDLE g_hServer = 0;

// SVN revision shown in the InitLogo banner (XKICK_Game1 CInit::InitLogo @08074212).
#define GAME_SVN_REVISION 2559

// ---- process-wide fatal/term signal handler ----
static void SignalKill(int nSigNum)
{
    static bool bExe = false;
    if (bExe) return;                 // re-entrancy guard
    bExe = true;

    // Bulk-mark this server's players offline in the DB (trio_server=0), matching
    // XKICK_Game1 SignalKill @08074712 which calls CSql::InitMemberLogin here.
    g_Sql.InitMemberLogin();

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
    printf("**\x1b[33m XKicks Game Server SVN Revision \x1b[31m%03d\x1b[36m \x1b[0m**\n",
           GAME_SVN_REVISION);
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

    const int killSigs[] = { SIGSEGV, SIGABRT, SIGILL, SIGTRAP, SIGSYS,
                             SIGXCPU, SIGXFSZ, SIGFPE, SIGINT, SIGTERM };
    for (int s : killSigs) sigaction(s, &tAct, NULL);

    const int ignSigs[] = { SIGUSR1, SIGUSR2, SIGCHLD, SIGQUIT, SIGPIPE, SIGHUP,
                            SIGTRAP, SIGABRT, SIGBUS, SIGALRM, SIGCONT,
                            SIGTSTP, SIGTTIN, SIGTTOU, SIGURG, SIGVTALRM,
                            SIGPROF, SIGWINCH, SIGIO, SIGPWR };
    for (int s : ignSigs) sigaction(s, &tAct2, NULL);
#else
    signal(SIGINT,  SignalKill);
    signal(SIGTERM, SignalKill);
    signal(SIGSEGV, SignalKill);
    signal(SIGABRT, SignalKill);
    signal(SIGFPE,  SignalKill);
    signal(SIGILL,  SignalKill);
#endif
    return true;
}

// InitMemory @080742ac: reset all players, then number the room + team pools so
// each slot carries its index as its room/team number (used as the table key).
bool CInit::InitMemory()
{
    m_nCount = 0;
    for (int i = 0; i < MAX_PLAYER_POOL; ++i)
        g_PlayerPool[i].InitPlayer();
    for (int i = 0; i < MAX_ROOM_POOL; ++i)
    {
        g_RoomPool[i].m_nState   = 0;         // binary resets room+1
        g_RoomPool[i].m_nRoomSeq = (short)i;  // binary sets short@room+4
    }
    for (int i = 0; i < MAX_TEAM_POOL; ++i)
    {
        g_TeamPool[i].m_nState = 0;           // binary resets team+0
        g_TeamPool[i].m_nNo    = (short)i;    // binary sets short@team+6
    }
    return true;
}

void CInit::InitLog(int nServerCode)
{
    snprintf(m_strServerName, 0x1f, "Game%d_", nServerCode);
    CLogManager::LOG_SYSTEM_NAME = m_strServerName;
}

void CInit::WriteBuildDate()
{
    LOGOUT_OPERATE("[BUILD] %s %s\n", __DATE__, __TIME__);
}

// ---- AhnLab HackShield call sites (inert when HackCode == 0; dummy-backed) ----
bool CInit::InitServerHack()
{
    if (g_Config.m_nHackCode == 0) return true;   // disabled in config -> no-op
    char sPath[64];
    snprintf(sPath, sizeof(sPath), "../Hack/%d.hsb", g_Config.m_nHackCode);
    g_hServer = _AhnHS_CreateServerObject(sPath);
    return (g_hServer != 0);
}

bool CInit::InitClientHack(CPlayer* /*pPlayer*/)
{
    // Intentionally a no-op: XKICK_Game1 has NO AhnLab HackShield at all (verified -
    // no InitClientHack/CreateClientObject/AhnHS_* anywhere in the binary). The
    // request/response hack flow + per-client handles are Certify-only.
    return true;
}
