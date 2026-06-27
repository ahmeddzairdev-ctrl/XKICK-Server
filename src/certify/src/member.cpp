// =============================================================================
// member.cpp - CMember session object + the CreateMember/ExitMember helpers
// (reconstructed from XKICK_Certify). A CMember is a runtime object (never sent
// on the wire); it tracks one connected account session.
//
// PORTABILITY: the binary used getpeername()/CTCPSocket::CloseEpoll/CloseSocket;
// the port routes those through connect.cpp's Net_GetIP/Net_Close so no socket
// headers leak here (PLAN.md mandate). m_nFd is the logical connection id.
// =============================================================================
#include "Main.h"
#include "Net.h"          // Net_GetIP / Net_Close
#include "AhnHS.h"        // _AhnHS_CloseClientHandle
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
CMember::CMember()  { m_IsLogin = false; }   // m_cSetting ctor zeroes the setting
CMember::~CMember() {}

// ---------------------------------------------------------------------------
// InitMember - reset per-session ping bookkeeping.
// ---------------------------------------------------------------------------
void CMember::InitMember()
{
    m_nCheckTCP = 0;
    ResetPingTime();
}

void CMember::SetMemberSeq(int nMemberSeq) { m_nMemberSeq = nMemberSeq; }

int  CMember::GetSeq() { return m_nMemberSeq; }

void CMember::ResetPingTime() { time(&m_tTcpPingTime); }

// ---------------------------------------------------------------------------
// DeleteMember - detach from the active list / hash and release the anti-cheat
// client handle. Does NOT touch the socket (ExitMember does that).
// ---------------------------------------------------------------------------
void CMember::DeleteMember()
{
    VectorMemberList::iterator it = std::find(g_MemberList.begin(), g_MemberList.end(), this);
    if (it != g_MemberList.end())
        g_MemberList.erase(it);

    MapMemberHash::iterator h = g_MemberHash.find(m_nMemberSeq);
    if (h != g_MemberHash.end())
        g_MemberHash.erase(h);

    m_nState = 0;
    _AhnHS_CloseClientHandle(m_hClient);
}

// ---------------------------------------------------------------------------
// ExitMember - full session teardown: persist logout, detach, close socket.
// ---------------------------------------------------------------------------
void CMember::ExitMember()
{
    if (m_IsLogin == true)
        g_Sql.SetLogOutData(m_nMemberSeq);

    DeleteMember();
    Net_Close(m_nFd);     // was CTCPSocket::CloseEpoll + CloseSocket
}

// ---------------------------------------------------------------------------
// CreateMember - claim the first free slot of g_MemberPool[1000], init it, and
// record the peer IP. m_nMemberSeq starts at -1 (not yet logged in).
// ---------------------------------------------------------------------------
// Free-function form used by the connect.cpp reactor on disconnect.
void ExitMember(CMember* pMember)
{
    if (pMember == 0)
        return;
    pMember->ExitMember();
}

CMember* CreateMember(int nFd)
{
    CMember* pMember = 0;
    for (int i = 0; i < MAX_MEMBER_POOL; ++i)
    {
        if (g_MemberPool[i].m_nState == 0)
        {
            pMember = &g_MemberPool[i];
            break;
        }
    }
    if (pMember == 0)
        return 0;

    pMember->InitMember();
    pMember->m_nState     = 1;
    pMember->m_nFd        = nFd;
    pMember->m_nMemberSeq = -1;
    Net_GetIP(nFd, pMember->m_cTCPAddress.m_sIP);   // was getpeername + inet_ntoa
    return pMember;
}
