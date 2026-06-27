// CInit - startup routines (init.cpp). Layout from Ghidra (36 bytes).
#ifndef _CERTIFY_INIT_H_
#define _CERTIFY_INIT_H_

class CMember;

class CInit
{
public:
    int  m_nCount;              // 0
    char m_strServerName[31];   // 4

    bool InitLogo();                  // banner
    bool InitSignal();                // signal handlers (SIGPIPE etc.) - platform layer
    bool InitMemory();                // pre-allocate pools (g_MemberPool/g_ServerPool)
    void InitLog(int serverCode);     // open log files

    // --- AhnLab HackShield call sites (see HackShield.h / AhnHS.h) ---
    // InitServerHack: if g_Config.m_nHackCode != 0, builds "../Hack/<code>.hsb"
    //   and g_hServer = _AhnHS_CreateServerObject(path); returns false on NULL.
    bool InitServerHack();
    // InitClientHack: pMember->m_hClient = _AhnHS_CreateClientObject(g_hServer);
    //   returns false on NULL. No-op (true) when HackCode == 0.
    bool InitClientHack(CMember* pMember);

    void WriteBuildDate();            // stamp build date to log
};

#endif
