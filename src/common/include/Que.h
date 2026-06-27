// =============================================================================
// Que.h - shared packet-buffer ring (CQue / CBufferPool).
//
// Project-agnostic infrastructure used by BOTH Certify and Game: the socket
// layer fills CBufferPool slots from a CQue and the per-server PacketProcess
// drains the resulting VectorPacketList. Implementation in common/src/que.cpp.
//
// A slot is "free" when its 2048-byte buffer starts with a NUL byte;
// ReleaseBuffer zeroes the buffer to mark it free again (matches the binary).
// Fully portable: only <vector>, no OS headers.
// =============================================================================
#ifndef _XKICK_QUE_H_
#define _XKICK_QUE_H_

#include <vector>
#include <mutex>

#define MAX_BUFFER_POOL 500

// One reusable receive buffer + owner pointer (2052 bytes in the original).
class CBufferPool
{
public:
    void* m_pObject;        // owning CMember*/CPlayer* (TCP) or CServer* (STS), as void*
    char  m_sBuffer[2048];  // raw packet: 12-byte header + body
};

// Ring of CBufferPool slots; a slot is free when its buffer starts with NUL.
class CQue
{
public:
    int         m_nCurrent;
    CBufferPool m_cBufferPool[MAX_BUFFER_POOL];
    std::mutex  m_cLock;        // packet-list mutex (binary: func_0x08049e78 @0x81dc594)

    CQue();
    ~CQue();
    CBufferPool* GetBuffer();
    void         ReleaseBuffer(CBufferPool* p);
    void         Lock()   { m_cLock.lock();   }
    void         Unlock() { m_cLock.unlock(); }
};

typedef std::vector<CBufferPool*> VectorPacketList;

#endif // _XKICK_QUE_H_
