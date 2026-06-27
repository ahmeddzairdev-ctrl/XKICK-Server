// =============================================================================
// main.cpp - XKICK_Game entry point (reconstructed from XKICK_Game1 @080ba0c4).
//
// Startup order (faithful to the binary; identical shape to Certify):
//   InitLogo -> InitSignal -> InitMemory -> InitConfig -> InitLog ->
//   CSql::InitServer -> CLoad::InitLoad -> CThread::InitThread ->
//   print config banner -> WriteBuildDate -> TCPThread()  (never returns)
//
// Portable: std C++; the Asio reactor (TCP + STS + UDP) lives in connect.cpp.
// =============================================================================
#include "Main.h"
#include "LogManager.h"
#include <cstdio>
#include <cstdlib>

#define STR_OK_OUT()   printf("\x1b[32m[OK]\n\x1b[0m")
#define STR_FAIL_OUT() printf("\x1b[31m[FAIL]\n\x1b[0m")

int main()
{
    g_Init.InitLogo();

    printf("# Init Signal ....... ");
    g_Init.InitSignal() ? STR_OK_OUT() : STR_FAIL_OUT();

    printf("# Init Memory ....... ");
    g_Init.InitMemory() ? STR_OK_OUT() : STR_FAIL_OUT();

    printf("# Init Config ....... ");
    g_Config.InitConfig() ? STR_OK_OUT() : STR_FAIL_OUT();

    g_Init.InitLog((int)g_Config.m_nServerCode);

    printf("# Init Server ....... ");
    g_Sql.InitServer() ? STR_OK_OUT() : STR_FAIL_OUT();

    printf("# Init Load ......... ");
    g_Load.InitLoad() ? STR_OK_OUT() : STR_FAIL_OUT();

    if (g_Thread.InitThread() != 0)
    {
        STR_FAIL_OUT();
        exit(0);
    }
    STR_OK_OUT();

    printf("\n");
    printf("# DB IP ............. \x1b[32m%s\x1b[0m\n", g_Config.m_sDBIP);
    printf("# TCP Port .......... \x1b[32m%d\x1b[0m\n", (int)g_Config.m_nTCPPort);
    printf("# UDP Port .......... \x1b[32m%d\x1b[0m\n", (int)g_Config.m_nUDPPort);
    printf("# STS Port .......... \x1b[32m%d\x1b[0m\n", (int)g_Config.m_nSTSPort);
    printf("# ServerCode ........ \x1b[32m%d\x1b[0m\n", (int)g_Config.m_nServerCode);
    printf("# Company ........... \x1b[32m%d\x1b[0m\n", (int)g_Config.m_nCompany);
    printf("# Nation ............ \x1b[32m%d\x1b[0m\n", (int)g_Config.m_nNation);
    printf("# Connector ......... \x1b[32m%d\x1b[0m\n", (int)g_Config.m_nConnector);
    printf("# Character set ..... \x1b[32m%s\x1b[0m\n", g_Config.m_sCharset);
    printf("# Table Path ........ \x1b[32m%s\x1b[0m\n", g_Config.m_sPath);
    printf("# HackCode  ......... \x1b[32m%d\x1b[0m\n", g_Config.m_nHackCode);
    printf("*******************************************\n");

    g_Init.WriteBuildDate();
    LOGOUT_PACKET("\x1b[33mLast Build : %s / %s\x1b[0m\n\x1b[0m", __DATE__, __TIME__);
    printf("*******************************************\n");

    TCPThread();   // Asio io_context.run() - services TCP + STS + UDP (never returns)
    return 0;
}
