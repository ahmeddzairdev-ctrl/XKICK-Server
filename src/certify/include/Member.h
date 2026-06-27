// CMember - a connected client/account session (member.cpp).
// Layout from Ghidra (296 bytes). Runtime object (never sent on the wire), so
// m_hClient is the portable anti-cheat handle, not AHNHS_CLIENT_HANDLE.
#ifndef _CERTIFY_MEMBER_H_
#define _CERTIFY_MEMBER_H_

#include "Struct.h"      // CSetting, CMoney, CAddress
#include "AhnHS.h"       // AHNHS_CLIENT_HANDLE
#include <ctime>

class CMember
{
public:
    short    m_nState;            // 0   PLAYER_STATE_EMPTY/USE
    int      m_nFd;              // 4   socket fd (Asio handle id in port)
    bool     m_IsLogin;         // 8
    int      m_nMemberSeq;      // 12
    int      m_nLastSeq;        // 16  last selected character
    char     m_nCount;          // 20  character count
    char     m_nTutorial;       // 21
    char     m_nQuest;          // 22
    char     m_nGhostHost;      // 23
    int      m_nNetSpeed;       // 24
    short    m_nPerformance;    // 28
    CSetting m_cSetting;        // 30  (88 bytes)
    CMoney   m_cMoney;          // 120 (16 bytes)
    CAddress m_cTCPAddress;     // 136 (24 bytes)
    int      m_nEquipItem[17];  // 160 MAX_EQUIP
    int      m_nHomeItem[4];    // 228
    int      m_nAwayItem[4];    // 244
    char     m_sPartner[15];    // 260
    int      m_nLoginDate;      // 276
    int      m_nDeleteDate;     // 280
    time_t   m_tTcpPingTime;    // 284
    char     m_nCheckTCP;       // 288
    AHNHS_CLIENT_HANDLE m_hClient;   // 292 AhnLab HackShield per-client handle

    CMember();
    ~CMember();

    void InitMember();
    void SetMemberSeq(int seq);
    int  GetSeq();
    void ResetPingTime();
    void DeleteMember();        // detach from g_MemberList/g_MemberHash
    void ExitMember();          // close + cleanup session
};

#endif
