// CThread - background threads (thread.cpp). Original held a single pthread_t
// for the STS loop; modernized to std::thread per the cross-platform mandate.
#ifndef _CERTIFY_THREAD_H_
#define _CERTIFY_THREAD_H_

#include <thread>

class CThread
{
public:
    std::thread m_tSTSTh;   // was pthread_t tSTSTh - runs STSThread()

    CThread();
    ~CThread();

    int InitThread();       // spawn STS thread; returns 0 on success
};

#endif
