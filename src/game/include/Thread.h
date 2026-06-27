// CThread (Game) - background threads (thread.cpp). The original held pthread_t
// handles for the STS loop and the UDP receive loop; modernized to std::thread
// per the cross-platform mandate. g_Thread @080e1524.
//
// In the Asio port the STS + UDP reactors are serviced by the single io_context
// on the main thread (connect.cpp), so these std::thread members may run thin
// stub loops; kept for structural fidelity with the binary.
#ifndef _GAME_THREAD_H_
#define _GAME_THREAD_H_

#include <thread>

class CThread
{
public:
    std::thread m_tSTSTh;   // server-to-server loop (STSThread)
    std::thread m_tUDPTh;   // UDP P2P receive loop (UDPThread)

    CThread();
    ~CThread();

    int InitThread();       // spawn STS + UDP threads; returns 0 on success
};

#endif // _GAME_THREAD_H_
