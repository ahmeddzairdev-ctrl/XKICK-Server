// thread.cpp - CThread (reconstructed from XKICK_Certify).
// Original used pthread_create to launch the STS (server-to-server) loop;
// modernized to std::thread per the cross-platform mandate. The actual loops
// TCPThread()/STSThread() are the Asio reactors (connect.cpp / main.cpp).
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
        // STS is serviced by the main io_context (connect.cpp), so STSThread() is
        // a stub; detach so the (idle) thread never blocks shutdown.
        m_tSTSTh = std::thread([]{ STSThread(0); });
        m_tSTSTh.detach();
    }
    catch (...)
    {
        LOGOUT_ERROR("thread error\n");
        exit(1);
    }
    std::this_thread::sleep_for(std::chrono::microseconds(50000));
    return 0;
}
