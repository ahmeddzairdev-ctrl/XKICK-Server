// CConfig - parses KicksServer/config.ini. Layout from Ghidra (292 bytes).
#ifndef _CERTIFY_CONFIG_H_
#define _CERTIFY_CONFIG_H_

class CConfig
{
public:
    char  m_sDBIP[20];       // 0
    short m_nSTSPort;        // 20  server-to-server listen port
    short m_nTCPPort;        // 22  client listen port
    char  m_sDBUser[20];     // 24
    char  m_sDBPass[50];     // 44
    char  m_sDBMain[20];     // 94   (kicks2)
    char  m_sDBLog[20];      // 114  (kicks2_log)
    char  m_sDBSample[20];   // 134
    char  m_sCharset[20];    // 154  (euckr)
    char  m_sPath[100];      // 174  table path
    short m_nCompany;        // 274
    short m_nNation;         // 276
    short m_nSystem;         // 278
    int   m_nVersion;        // 280
    int   m_nHackCode;       // 284
    short m_nServerCode;     // 288
    short m_nConnector;      // 290

    CConfig();
    ~CConfig();
    bool InitConfig();       // reads config.ini + version.h, fills the above
};

#endif
