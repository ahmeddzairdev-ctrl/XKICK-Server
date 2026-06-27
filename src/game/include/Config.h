// =============================================================================
// CConfig (Game) - parses ../config.ini. Layout recovered from Ghidra
// (CConfig::InitConfig @0804c6f0 + main() banner field offsets).
//
// Differs from the Certify CConfig: the Game has THREE listen ports (TCP/UDP/STS)
// at offsets 40/42/44 and a wider tail. Ports + Connector are filled from the DB
// (CSql::InitServer / tb_game_server), the rest from config.ini keys:
//   DB_IP DB_User DB_Pass DB_Main DB_Log DB_Sample Charset Company Nation
//   System HackCode ServerCode  (Company 100=Korean/200=Dubai_Arb/900=Default_Eng)
// =============================================================================
#ifndef _GAME_CONFIG_H_
#define _GAME_CONFIG_H_

class CConfig
{
public:
    char  m_sDBIP[20];       // 0x00 (0)    DB_IP
    char  m_sMasterIP[20];   // 0x14 (20)   master/Certify STS uplink IP (set at runtime
                             //             by CSql::InitServer; Game dials this via g_STSSocket)
    short m_nTCPPort;        // 0x28 (40)   client TCP listen port      (from DB)
    short m_nUDPPort;        // 0x2a (42)   UDP (P2P relay/confirm) port (from DB)
    short m_nSTSPort;        // 0x2c (44)   server-to-server port        (from DB)
    char  m_sDBUser[20];     // 0x2e (46)   DB_User
    char  m_sDBPass[50];     // 0x42 (66)   DB_Pass
    char  m_sDBMain[20];     // 0x74 (116)  DB_Main   (kicks2)
    char  m_sDBLog[20];      // 0x88 (136)  DB_Log    (kicks2_log)
    char  m_sDBSample[20];   // 0x9c (156)  DB_Sample
    char  m_sCharset[20];    // 0xb0 (176)  Charset   (euckr)
    char  m_sPath[100];      // 0xc4 (196)  table path (set by Company)
    short m_nCompany;        // 0x128 (296) Company
    short m_nNation;         // 0x12a (298) Nation
    short m_nSystem;         // 0x12c (300) System
    int   m_nHackCode;       // 0x130 (304) HackCode
    short m_nServerCode;     // 0x134 (308) ServerCode
    short m_nVersion;        // 0x136 (310) (build/version; tail)
    short m_nConnector;      // 0x13c (316) max connections (from DB)
    short m_nPad;            // 0x13e

    CConfig();
    ~CConfig();
    bool InitConfig();       // reads ../config.ini, fills the above
};

#endif // _GAME_CONFIG_H_
