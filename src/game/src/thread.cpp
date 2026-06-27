// thread.cpp - CThread (reconstructed from XKICK_Game1).
// Original used pthread_create for the STS (server-to-server) and UDP receive
// loops; modernized to std::thread per the cross-platform mandate. In the Asio
// port the STS + UDP reactors are folded into the single io_context (connect.cpp),
// so STSThread()/UDPThread() are thin stubs and these threads are detached.
#include "Main.h"
#include "LogManager.h"
#include <thread>
#include <chrono>
#include <cstdlib>

CThread::CThread()  {}
CThread::~CThread() {}

int CThread::InitThread()
{
    try
    {
        m_tSTSTh = std::thread([]{ STSThread(0); });
        m_tSTSTh.detach();
        m_tUDPTh = std::thread([]{ UDPThread(0); });
        m_tUDPTh.detach();
    }
    catch (...)
    {
        LOGOUT_ERROR("thread error\n");
        exit(1);
    }
    std::this_thread::sleep_for(std::chrono::microseconds(50000));
    return 0;
}
