// =============================================================================
// main.cpp - XKICK_Certify entry point (reconstructed from XKICK_Certify).
//
// Startup order (faithful to the binary):
//   InitLogo -> InitSignal -> InitMemory -> InitConfig -> CSql::InitServer ->
//   CLoad::InitLoad -> InitServerHack -> InitLog -> print config block ->
//   log build date -> CThread::InitThread -> TCPThread()  (never returns)
//
// Portable: std C++; the Asio reactor lives in connect.cpp (TCPThread).
// =============================================================================
#include "Main.h"
#include "LogManager.h"
#include <cstdio>

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

    printf("# Init Server ....... ");
    g_Sql.InitServer() ? STR_OK_OUT() : STR_FAIL_OUT();

    printf("# Init Load ......... ");
    g_Load.InitLoad() ? STR_OK_OUT() : STR_FAIL_OUT();

    printf("# Init Hack ......... ");
    g_Init.InitServerHack() ? STR_OK_OUT() : STR_FAIL_OUT();

    g_Init.InitLog((int)g_Config.m_nServerCode);

    printf("\n");
    printf("# DB IP ............. \x1b[32m%s\x1b[0m\n", g_Config.m_sDBIP);
    printf("# TCP Port .......... \x1b[32m%d\x1b[0m\n", (int)g_Config.m_nTCPPort);
    printf("# STS Port .......... \x1b[32m%d\x1b[0m\n", (int)g_Config.m_nSTSPort);
    printf("# ServerCode ........ \x1b[32m%d\x1b[0m\n", (int)g_Config.m_nServerCode);
    printf("# Company ........... \x1b[32m%d\x1b[0m\n", (int)g_Config.m_nCompany);
    printf("# Nation ............ \x1b[32m%d\x1b[0m\n", (int)g_Config.m_nNation);
    printf("# Connector ......... \x1b[32m%d\x1b[0m\n", (int)g_Config.m_nConnector);
    printf("# Character set ..... \x1b[32m%s\x1b[0m\n", g_Config.m_sCharset);
    printf("# Table Path ........ \x1b[32m%s\x1b[0m\n", g_Config.m_sPath);
    printf("# Check Version ..... \x1b[32m%d\x1b[0m\n", g_Config.m_nVersion);
    printf("# HackCode  ......... \x1b[32m%d\x1b[0m\n", g_Config.m_nHackCode);
    printf("*******************************************\n");

    LOGOUT_PACKET("\x1b[33mLast Build : %s / %s\x1b[0m\n\x1b[0m", __DATE__, __TIME__);
    printf("*******************************************\n");

    g_Thread.InitThread();
    TCPThread();
    return 0;
}
