// que.cpp - CQue packet-buffer ring (shared infra; reconstructed from the binary).
// Fully portable. A slot is "free" when its buffer starts with a NUL byte;
// ReleaseBuffer zeroes the 2048-byte buffer to mark it free again.
#include "Que.h"
#include <cstring>

CQue::CQue()  { m_nCurrent = 0; }
CQue::~CQue() {}

// Advance the ring cursor and hand out the slot if it is free, else NULL.
CBufferPool* CQue::GetBuffer()
{
    m_nCurrent++;
    if (m_nCurrent > MAX_BUFFER_POOL - 1)   // 499
        m_nCurrent = 0;

    if (m_cBufferPool[m_nCurrent].m_sBuffer[0] == '\0')
        return &m_cBufferPool[m_nCurrent];
    return 0;
}

// Mark a slot free again by zeroing its 2048-byte buffer (original memset 0x800).
void CQue::ReleaseBuffer(CBufferPool* pPool)
{
    memset(pPool->m_sBuffer, 0, sizeof(pPool->m_sBuffer)); // 0x800
}
