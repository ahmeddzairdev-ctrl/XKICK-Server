// =============================================================================
// packet.cpp - Certify packet handlers (reconstructed from XKICK_Certify).
//
// Each handler builds a CSC* response (command id + body size set by its ctor),
// fills the wire fields, and calls SendMember/SendServer. Response command ids
// and field assignments are pinned to the binary (wire-critical). DB access goes
// through g_Sql; in-memory tables through g_Load. The third SendMember arg is the
// request/secure flag used only for logging.
//
// Certify writes server->client packets RAW (XOR 0xE5 applies to inbound bodies
// only, handled in connect.cpp); SendMember/SendServer just write + log.
//
// Portable: std C++ only. Net_Send/Net_GetIP (connect.cpp) replace raw write().
// =============================================================================
#include "Main.h"
#include "packet.h"
#include "Net.h"          // Net_Send
#include "HackShield.h"   // PacketHackRequest (called from CheckPingTime path)
#include "md5.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

// CHTTPSocket::SendURL (external billing check) is a thin client; declared here
// so the System==2001 login path links. Implemented as an inert stub for now.
extern int  HttpSendURL(const char* sHost, int nPort, const char* sURL, char* sOut, int nOutSize);

// External billing buffer (the binary read g_HTTPSocket.m_sBuffer after SendURL).
static char g_sHttpBuffer[4096];

// ---------------------------------------------------------------------------
// SendMember - raw write of (header + body) to the client, then per-packet log.
// ---------------------------------------------------------------------------
void SendMember(CMember* pMember, CHeadPacket* pPacket, int nRequest)
{
    Net_Send(pMember->m_nFd, pPacket, pPacket->m_nBodySize + 12);

    char str[30];
    memset(str, 0, sizeof(str));
    if (IsCommandString(pPacket->m_nCommand))
    {
        GetCommandString(pPacket->m_nCommand, str);
        int nColor = (nRequest != 0) ? 0x1f : 0x24;
        LOGOUT_SCREEN("\x1b[%dm[OUT]\x1b[0m", nColor);
        LOGOUT_PACKET(" OUT(%d), Seq(%d): %s(%d), nSize(%d), Req(%d)\n",
                      pMember->m_nFd, pMember->m_nMemberSeq, str,
                      pPacket->m_nCommand, pPacket->m_nBodySize, nRequest);
    }
}

// ---------------------------------------------------------------------------
// SendServer - raw write to a game server (STS channel) + log.
// ---------------------------------------------------------------------------
void SendServer(CServer* pServer, CHeadPacket* pPacket, int nRequest)
{
    Net_Send(pServer->m_nFd, pPacket, pPacket->m_nBodySize + 12);

    char str[30];
    memset(str, 0, sizeof(str));
    if (IsCommandString(pPacket->m_nCommand))
    {
        GetCommandString(pPacket->m_nCommand, str);
        LOGOUT_PACKET("Fd(%d), ServerCode(%d): %s(%d), nSize(%d), Req(%d)\n",
                      pServer->m_nFd, pServer->m_nServerCode, str,
                      pPacket->m_nCommand, pPacket->m_nBodySize, nRequest);
    }
}

// ---------------------------------------------------------------------------
// SendAllServer - broadcast a packet to every connected game server, skipping
// the certify/relay codes 1 and 99.
// ---------------------------------------------------------------------------
void SendAllServer(CHeadPacket* pPacket, int nRequest)
{
    for (VectorServerList::iterator it = g_ServerList.begin(); it != g_ServerList.end(); ++it)
    {
        CServer* pServer = *it;
        if (pServer == 0 || pServer->m_nServerCode == 1 || pServer->m_nServerCode == 99)
            continue;

        Net_Send(pServer->m_nFd, pPacket, pPacket->m_nBodySize + 12);

        char str[30];
        memset(str, 0, sizeof(str));
        if (!IsCommandString(pPacket->m_nCommand))
            return;
        GetCommandString(pPacket->m_nCommand, str);
        LOGOUT_PACKET("Fd(%d), ServerCode(%d): %s(%d), nSize(%d), Req(%d)\n",
                      pServer->m_nFd, pServer->m_nServerCode, str,
                      pPacket->m_nCommand, pPacket->m_nBodySize, nRequest);
    }
}

// ---------------------------------------------------------------------------
// lookup helpers
// ---------------------------------------------------------------------------
int GetMemberCount()
{
    int nCount = 0;
    for (int i = 0; i < MAX_MEMBER_POOL; ++i)
        if (g_MemberPool[i].m_nState != 0) ++nCount;
    return nCount;
}

CMember* GetMemberPointer(int nSeq)
{
    CMember* pAny = g_MemberHash[nSeq];
    if (pAny == 0)
        g_MemberHash.erase(g_MemberHash.find(nSeq));
    return pAny;
}

CServer* FindServer(int nServerCode)
{
    for (VectorServerList::iterator it = g_ServerList.begin(); it != g_ServerList.end(); ++it)
    {
        CServer* p = *it;
        if (p != 0 && p->m_nServerCode == nServerCode)
            return p;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// PacketCertifyLogin (CM_CERTIFY_LOGIN) - ID/password authentication. System
// 2001 first runs an external billing check; then CheckMemberID validates the
// credentials, GetMemberFields loads the account, and the session goes live.
// ---------------------------------------------------------------------------
void PacketCertifyLogin(CMember* pMember, CHeadPacket* pPacket)
{
    CSCCertifyLogin  cPacket;

    // Read via the official-client CCSCertifyLogin layout (CLIENT_SHARE): id @
    // body+0, password @ body+31. The official 2008 client's body is 96 bytes and
    // carries NO version field, so sVersion is NULL (CheckMemberID then skips the
    // trio_version UPDATE). The Aug-2010 binary read a version@body+96, which is
    // past the end of this client's packet -- that beta-era read is reverted here.
    CCSCertifyLogin* pIn = (CCSCertifyLogin*)pPacket;
    char* sID      = pIn->m_sID;             // body + 0
    char* sPass    = pIn->m_sPass;           // body + 31
    char* sVersion = NULL;                   // official client sends no version

    if (g_Config.m_nSystem == 0x7d1)   // 2001 -> external billing system
    {
        std::string strPass;
        md5(strPass, std::string(sPass));

        char sBuffer[1000];
        snprintf(sBuffer, sizeof(sBuffer),
                 "http://192.168.200.101:9191/stBilling/SystemFrontController.jsp?"
                 "un=REG_usr&pw=gp7ADmin13&_tdl=%%25-336225261&"
                 "_vas=[userName=%s,password=%%23%%23%%23%s]&_cnc=pk&_an=login",
                 sID, strPass.c_str());

        int nRet = HttpSendURL("192.168.200.101", 0x23e7, sBuffer,
                               g_sHttpBuffer, sizeof(g_sHttpBuffer));
        if (nRet < 0)
        {
            cPacket.m_nResponse = -0x10;
            LOGOUT_ERROR("CertifyLogin(): URL Login(%d)\n", -0x10);
            SendMember(pMember, &cPacket, cPacket.m_nResponse);
            return;
        }
        if (strstr(g_sHttpBuffer, "true") == 0)
        {
            cPacket.m_nResponse = -0xd;
            SendMember(pMember, &cPacket, -0xd);
            return;
        }
        if (g_Sql.InsertMemberField(sID, sPass) < 0)
        {
            cPacket.m_nResponse = -0xb;
            LOGOUT_ERROR("CertifyLogin(): Error(%d)\n", -0xb);
            SendMember(pMember, &cPacket, cPacket.m_nResponse);
            return;
        }
    }

    int nCheck = g_Sql.CheckMemberID(pMember, sID, sPass, sVersion);
    if (nCheck < 0)
    {
        switch (nCheck)
        {
        case -7: cPacket.m_nResponse = -0x11; break;
        case -6: cPacket.m_nResponse = -0x10; break;
        case -5: cPacket.m_nResponse = -0xf;  break;
        case -4: cPacket.m_nResponse = -0xe;  break;
        case -3: cPacket.m_nResponse = -0xd;  break;
        case -2: cPacket.m_nResponse = -0xc;  LOGOUT_ERROR("CertifyLogin(): Error(%d)\n", -0xc); break;
        case -1: cPacket.m_nResponse = -0xb;  LOGOUT_ERROR("CertifyLogin(): Error(%d)\n", -0xb); break;
        default: cPacket.m_nResponse = -99;
        }
        SendMember(pMember, &cPacket, cPacket.m_nResponse);
        return;
    }

    pMember->SetMemberSeq(nCheck);
    nCheck = g_Sql.GetMemberFields(pMember, 1000);
    if (nCheck >= 0)
    {
        g_Sql.SetLoginData(pMember->m_nMemberSeq);
        g_MemberHash[pMember->m_nMemberSeq] = pMember;
        pMember->m_IsLogin = true;

        cPacket.m_nMemberSeq = pMember->m_nMemberSeq;
        cPacket.m_nResponse  = 0;
        SendMember(pMember, &cPacket, 0);
        g_Init.InitClientHack(pMember);
        return;
    }

    switch (nCheck)
    {
    case -3: cPacket.m_nResponse = -0x17; LOGOUT_ERROR("CertifyLogin(): Error(%d)\n", -0x17); break;
    case -2: cPacket.m_nResponse = -0x16; LOGOUT_ERROR("CertifyLogin(): Error(%d)\n", -0x16); break;
    case -1: cPacket.m_nResponse = -0x15; LOGOUT_ERROR("CertifyLogin(): Error(%d)\n", -0x15); break;
    case -4: cPacket.m_nResponse = -0x18; break;
    default: cPacket.m_nResponse = -99;
    }
    SendMember(pMember, &cPacket, cPacket.m_nResponse);
    pMember->m_IsLogin = false;
}

// ---------------------------------------------------------------------------
// PacketInstantLogin (CM_INSTANT_LOGIN) - re-login by member seq (no password).
// ---------------------------------------------------------------------------
void PacketInstantLogin(CMember* pMember, CHeadPacket* pPacket)
{
    CCSInstantLogin* pIn = (CCSInstantLogin*)pPacket;
    CSCInstantLogin  cPacket;

    pMember->SetMemberSeq(pIn->m_nMemberSeq);
    int nCheck = g_Sql.GetMemberFields(pMember, 0x3e9);
    if (nCheck >= 0)
    {
        g_Sql.SetLoginData(pMember->m_nMemberSeq);
        g_MemberHash[pMember->m_nMemberSeq] = pMember;
        pMember->m_IsLogin = true;
        cPacket.m_nResponse = 0;
        SendMember(pMember, &cPacket, 0);
        return;
    }

    switch (nCheck)
    {
    case -3: cPacket.m_nResponse = -0xd; LOGOUT_ERROR("InstantLogin(): Error(%d)\n", -0xd); break;
    case -2: cPacket.m_nResponse = -0xc; LOGOUT_ERROR("InstantLogin(): Error(%d)\n", -0xc); break;
    case -1: cPacket.m_nResponse = -0xb; LOGOUT_ERROR("InstantLogin(): Error(%d)\n", -0xb); break;
    case -4: cPacket.m_nResponse = -0xe; break;
    default: cPacket.m_nResponse = -99;
    }
    SendMember(pMember, &cPacket, cPacket.m_nResponse);
    pMember->m_IsLogin = false;
}

// ---------------------------------------------------------------------------
// PacketCertifyExit (CM_CERTIFY_EXIT) - acknowledge an exit and tear down.
// ---------------------------------------------------------------------------
void PacketCertifyExit(CMember* pMember, CHeadPacket* pPacket)
{
    CCSCertifyExit* pIn = (CCSCertifyExit*)pPacket;
    CSCCertifyExit  cPacket;

    cPacket.m_nResponse = 0;
    cPacket.m_nReason   = pIn->m_nReason;
    SendMember(pMember, &cPacket, 0);

    if (pIn->m_nReason == 0)      LOGOUT_SCREEN("Normal Exit\n");
    else if (pIn->m_nReason == 2) LOGOUT_SCREEN("Drawforce Exit\n");
    else                          LOGOUT_SCREEN("Transport Exit\n");

    pMember->ExitMember();
}

// ---------------------------------------------------------------------------
// PacketMemberInfo (CM_MEMBER_INFO) - return the cached account summary.
// ---------------------------------------------------------------------------
void PacketMemberInfo(CMember* pMember, CHeadPacket* /*pPacket*/)
{
    CSCMemberInfo cPacket;
    cPacket.m_nLastSeq   = pMember->m_nLastSeq;
    cPacket.m_nCount     = pMember->m_nCount;
    cPacket.m_nTutorial  = pMember->m_nTutorial;
    cPacket.m_nQuest     = pMember->m_nQuest;
    memcpy(&cPacket.m_cSetting, &pMember->m_cSetting, 0x58);
    cPacket.m_cMoney     = pMember->m_cMoney;
    cPacket.m_nLoginDate  = pMember->m_nLoginDate;
    cPacket.m_nDeleteDate = pMember->m_nDeleteDate;
    cPacket.m_nResponse  = 0;
    SendMember(pMember, &cPacket, 0);
}

// ---------------------------------------------------------------------------
// PacketNoticeList (CM_NOTICE_LIST) - send the notice table if the client's
// version is older than the server's.
// ---------------------------------------------------------------------------
void PacketNoticeList(CMember* pMember, CHeadPacket* pPacket)
{
    CSCNoticeList cPacket;
    if (pMember == 0 || pPacket == 0)
        return;

    CCSNoticeList* pIn = (CCSNoticeList*)pPacket;
    cPacket.m_nVersion = g_Load.GetNoticeVersion();

    for (int i = 0; i < 5; ++i)
    {
        cPacket.m_cNoticeList[i].m_nNoticeSeq = 0;
        memset(cPacket.m_cNoticeList[i].m_strText, 0, 0x79);
    }

    if (pIn->m_nVersion < cPacket.m_nVersion)
    {
        int i = 0;
        for (CStringArray::iterator it = g_Load.m_cNoticeArray.begin();
             it != g_Load.m_cNoticeArray.end(); ++it)
        {
            cPacket.m_cNoticeList[i].m_nNoticeSeq = it->first;
            snprintf(cPacket.m_cNoticeList[i].m_strText, 0x79, "%s", it->second);
            ++i;
        }
        cPacket.m_nCount    = i;
        cPacket.m_nResponse = 0;
        SendMember(pMember, &cPacket, 0);
    }
}

// ---------------------------------------------------------------------------
// PacketCharacterInfo (CM_CHARACTER_INFO) - stream each character then end.
// ---------------------------------------------------------------------------
void PacketCharacterInfo(CMember* pMember, CHeadPacket* /*pPacket*/)
{
    CSCCharacterInfo cPacket;

    for (int i = 0; i <= 2; ++i)
    {
        memset(&cPacket.m_cCharacterInfo, 0, sizeof(cPacket.m_cCharacterInfo));
        int nRet = g_Sql.GetCharacterInfo(pMember, i, &cPacket.m_cCharacterInfo);
        if (nRet < 0)
        {
            cPacket.m_nResponse = (char)(nRet - 10);
            SendMember(pMember, &cPacket, cPacket.m_nResponse);
            LOGOUT_ERROR("CharacterInfo(): Error(%d)\n", (int)cPacket.m_nResponse);
            return;
        }
        if (nRet != 1)
        {
            cPacket.m_nResponse = 0;
            SendMember(pMember, &cPacket, 0);
        }
    }
    PacketCharacterEnd(pMember);
}

// ---------------------------------------------------------------------------
// PacketCharacterEnd - terminates the CharacterInfo stream.
// ---------------------------------------------------------------------------
void PacketCharacterEnd(CMember* pMember)
{
    CSCCharacterEnd cPacket;
    cPacket.m_nResponse = 0;
    SendMember(pMember, &cPacket, 0);
}

// ---------------------------------------------------------------------------
// PacketCreateCharacter (CM_CREATE_CHARACTER)
// ---------------------------------------------------------------------------
void PacketCreateCharacter(CMember* pMember, CHeadPacket* pPacket)
{
    CCSCreateCharacter* pIn = (CCSCreateCharacter*)pPacket;
    CSCCreateCharacter  cPacket;

    int nCheck = g_Sql.CreateCharacter(pMember, pIn);
    if (nCheck < 0)
    {
        switch (nCheck)
        {
        case -5: cPacket.m_nResponse = -0xf; break;
        case -4: cPacket.m_nResponse = -0xe; break;
        case -3: cPacket.m_nResponse = -0xd; break;
        case -2: cPacket.m_nResponse = -0xc; LOGOUT_ERROR("CreateCharacter(): Error(%d)\n", -0xc); break;
        case -1: cPacket.m_nResponse = -0xb; LOGOUT_ERROR("CreateCharacter(): Error(%d)\n", -0xb); break;
        default: cPacket.m_nResponse = -99;
        }
        SendMember(pMember, &cPacket, cPacket.m_nResponse);
    }
    else
    {
        cPacket.m_nResponse = 0;
        SendMember(pMember, &cPacket, 0);
    }
}

// ---------------------------------------------------------------------------
// PacketDeleteCharacter (CM_DELETE_CHARACTER)
// ---------------------------------------------------------------------------
void PacketDeleteCharacter(CMember* pMember, CHeadPacket* pPacket)
{
    CCSDeleteCharacter* pIn = (CCSDeleteCharacter*)pPacket;
    CSCDeleteCharacter  cPacket;

    int nCheck = g_Sql.DeleteCharacter(pMember, pIn);
    if (nCheck < 0)
    {
        switch (nCheck)
        {
        case -6: cPacket.m_nResponse = -0x10; break;
        case -5: cPacket.m_nResponse = -0xf;  break;
        case -4: cPacket.m_nResponse = -0xe;  break;
        case -3: cPacket.m_nResponse = -0xd;  LOGOUT_ERROR("DeleteCharacter(): Error(%d)\n", -0xd); break;
        case -2: cPacket.m_nResponse = -0xc;  LOGOUT_ERROR("DeleteCharacter(): Error(%d)\n", -0xc); break;
        case -1: cPacket.m_nResponse = -0xb;  LOGOUT_ERROR("DeleteCharacter(): Error(%d)\n", -0xb); break;
        default: cPacket.m_nResponse = -99;
        }
        SendMember(pMember, &cPacket, cPacket.m_nResponse);
    }
    else
    {
        cPacket.m_nResponse   = 0;
        cPacket.m_nDeleteDate = (int)time(0);
        SendMember(pMember, &cPacket, cPacket.m_nResponse);
    }
}

// ---------------------------------------------------------------------------
// PacketChoiceCharacter (CM_CHOICE_CHARACTER) - remember the last character.
// ---------------------------------------------------------------------------
void PacketChoiceCharacter(CMember* pMember, CHeadPacket* pPacket)
{
    CCSChoiceCharacter* pIn = (CCSChoiceCharacter*)pPacket;
    CSCChoiceCharacter  cPacket;

    pMember->m_nLastSeq = pIn->m_nPlayerSeq;
    g_Sql.UpdateLastSeqField(pMember);
    cPacket.m_nPlayerSeq = pMember->m_nLastSeq;
    cPacket.m_nResponse  = 0;
    SendMember(pMember, &cPacket, 0);
}

// ---------------------------------------------------------------------------
// PacketServerList (CM_SERVER_LIST)
// ---------------------------------------------------------------------------
void PacketServerList(CMember* pMember, CHeadPacket* pPacket)
{
    CCSServerList* pIn = (CCSServerList*)pPacket;
    CSCServerList  cPacket;
    memset(cPacket.m_cServerData, 0, sizeof(cPacket.m_cServerData));  // zero unused entries (state 0 -> client skips)

    int nCheck = g_Sql.GetServerList((int)pIn->m_nChannel, &cPacket);
    if (nCheck < 0)
    {
        if (nCheck == -2)      { cPacket.m_nResponse = -0xc; LOGOUT_ERROR("ServerList(): Error(%d)\n", -0xc); }
        else if (nCheck == -1) { cPacket.m_nResponse = -0xb; LOGOUT_ERROR("ServerList(): Error(%d)\n", -0xb); }
        else                     cPacket.m_nResponse = -99;
        SendMember(pMember, &cPacket, cPacket.m_nResponse);
    }
    else
    {
        cPacket.m_nResponse = 0;
        SendMember(pMember, &cPacket, 0);
    }
}

// ---------------------------------------------------------------------------
// PacketChoiceServer (CM_CHOICE_SERVER) - resolve a server's connect address.
// ---------------------------------------------------------------------------
void PacketChoiceServer(CMember* pMember, CHeadPacket* pPacket)
{
    CCSChoiceServer* pIn = (CCSChoiceServer*)pPacket;
    CSCChoiceServer  cPacket;

    int nCheck = g_Sql.CheckChoiceServer(pIn->m_nServerCode, &cPacket.m_cAddress);
    if (nCheck >= 0)
    {
        cPacket.m_nResponse = 0;
        SendMember(pMember, &cPacket, 0);
        return;
    }

    switch (nCheck)
    {
    case -3: cPacket.m_nResponse = -0xd; break;
    case -2: cPacket.m_nResponse = -0xc; LOGOUT_ERROR("ChoiceServer(): Error(%d)\n", -0xc); break;
    case -1: cPacket.m_nResponse = -0xb; LOGOUT_ERROR("ChoiceServer(): Error(%d)\n", -0xb); break;
    case -4: cPacket.m_nResponse = -0xe; break;
    default: cPacket.m_nResponse = -99;
    }
    SendMember(pMember, &cPacket, cPacket.m_nResponse);
}

// ---------------------------------------------------------------------------
// PacketExecuteTutorial (CM_EXECUTE_TUTORIAL) - mark the tutorial done and grant
// the fixed reward item (logged as a quest-reward sale).
// ---------------------------------------------------------------------------
void PacketExecuteTutorial(CMember* pMember, CHeadPacket* pPacket)
{
    CCSExecuteTutorial* pIn = (CCSExecuteTutorial*)pPacket;
    CSCExecuteTutorial  cPacket;

    int nCheck = g_Sql.SetExecuteTutorial(pMember->m_nMemberSeq, (int)pIn->m_nTutorial);
    if (nCheck < 0)
    {
        if (nCheck == -2)      { cPacket.m_nResponse = -0xc; LOGOUT_ERROR("ExecuteTutorial(): Error(%d)\n", -0xc); }
        else if (nCheck == -1) { cPacket.m_nResponse = -0xb; LOGOUT_ERROR("ExecuteTutorial(): Error(%d)\n", -0xb); }
        else                     cPacket.m_nResponse = -99;
        SendMember(pMember, &cPacket, cPacket.m_nResponse);
        return;
    }

    CItem cItem;
    cItem.m_nState     = 1;
    cItem.m_nEquipKind = 0;
    cItem.m_nOptionCode[0] = cItem.m_nOptionCode[1] = cItem.m_nOptionCode[2] =
        cItem.m_nOptionCode[3] = cItem.m_nOptionCode[4] = 0;
    cItem.m_nCode = 0x1dde322d;   // fixed reward item code (all tutorial orders)

    CSale cSale;
    cSale.m_nObjectSeq  = g_Sql.InsertItemField(pMember, &cItem);
    cSale.m_nObjectCode = cItem.m_nCode;
    cSale.m_nObjectKind = 1;   // OBJECT_KIND_ITEM
    cSale.m_nBuyKind    = 5;   // BUY_KIND_QUEST
    cSale.m_nSaleKind   = 5;   // SALE_KIND_QUEST_REWARD
    cSale.m_nAmount     = 1;
    cSale.m_nPrice      = 0;
    g_Sql.InsertSaleLog(pMember, &cSale);

    pMember->m_nTutorial = pIn->m_nTutorial;
    cPacket.m_nTutorial  = pMember->m_nTutorial;
    cPacket.m_nResponse  = 0;
    SendMember(pMember, &cPacket, 0);
}

// ---------------------------------------------------------------------------
// PacketExecuteQuest (0xA90) - mark a quest done; echo updated wallet.
// ---------------------------------------------------------------------------
void PacketExecuteQuest(CMember* pMember, CHeadPacket* pPacket)
{
    CCSExecuteQuest* pIn = (CCSExecuteQuest*)pPacket;
    CSCExecuteQuest  cPacket;

    int nCheck = g_Sql.SetExecuteQuest(pMember->m_nMemberSeq, (int)pIn->m_nQuest);
    if (nCheck < 0)
    {
        if (nCheck == -2)      { cPacket.m_nResponse = -0xc; LOGOUT_ERROR("ExecuteQuest(): Error(%d)\n", -0xc); }
        else if (nCheck == -1) { cPacket.m_nResponse = -0xb; LOGOUT_ERROR("ExecuteQuest(): Error(%d)\n", -0xb); }
        else                     cPacket.m_nResponse = -99;
        SendMember(pMember, &cPacket, cPacket.m_nResponse);
    }
    else
    {
        pMember->m_nQuest   = pIn->m_nQuest;
        cPacket.m_nQuest    = pMember->m_nQuest;
        cPacket.m_cMoney    = pMember->m_cMoney;
        cPacket.m_nResponse = 0;
        SendMember(pMember, &cPacket, 0);
    }
}

// ---------------------------------------------------------------------------
// PacketChangeSetting (CM_CHANGE_SETTING) - persist or reset the game setting.
// ---------------------------------------------------------------------------
void PacketChangeSetting(CMember* pMember, CHeadPacket* pPacket)
{
    CSCChangeSetting cPacket;

    // Incoming body layout (wider than CCSChangeSetting): initFlag@body+0,
    // playerSeq@body+1, setting@body+5. Read by raw offset to match the binary.
    char* sBody       = (char*)(pPacket + 1);
    char  nInit       = sBody[0];
    int   nPlayerSeq  = *(int*)(sBody + 1);

    if (nInit == 0)
    {
        memcpy(&cPacket.m_cSetting, sBody + 5, 0x58);
        g_Sql.UpdateSettingField(nPlayerSeq, &cPacket.m_cSetting);
    }
    else
    {
        memcpy(&cPacket.m_cSetting, &g_Setting, 0x58);
    }

    cPacket.m_nInitSetting = nInit;
    cPacket.m_nResponse    = 0;
    SendMember(pMember, &cPacket, 0);
}

// ---------------------------------------------------------------------------
// PacketCharacterSearch (CM_CHARACTER_SEARCH) - existence check by name.
// ---------------------------------------------------------------------------
void PacketCharacterSearch(CMember* pMember, CHeadPacket* pPacket)
{
    CSCCharacterSearch cPacket;
    if (pMember == 0 || pPacket == 0)
        return;

    int nCheck = g_Sql.CheckPlayerName((const char*)(pPacket + 1));
    if (nCheck >= 0)
    {
        cPacket.m_nResponse = 0;
        SendMember(pMember, &cPacket, 0);
        return;
    }

    switch (nCheck)
    {
    case -3: cPacket.m_nResponse = -0xd; break;
    case -2: cPacket.m_nResponse = -0xc; LOGOUT_ERROR("PacketCharacterSearch(): Error(%d)\n", -0xc); break;
    case -1: cPacket.m_nResponse = -0xb; LOGOUT_ERROR("PacketCharacterSearch(): Error(%d)\n", -0xb); break;
    case -4: cPacket.m_nResponse = -0xe; break;
    default: cPacket.m_nResponse = -99;
    }
    SendMember(pMember, &cPacket, cPacket.m_nResponse);
}

// ---------------------------------------------------------------------------
// PacketDrawforcePlayer (CM_DRAWFORCE_PLAYER) - force-disconnect an already
// logged-in account: locally if on this certify, else via STS to its game server.
// ---------------------------------------------------------------------------
void PacketDrawforcePlayer(CMember* pMember, CHeadPacket* pPacket)
{
    CSCDrawforcePlayer cPacket;
    if (pMember == 0 || pPacket == 0)
        return;

    char* sBody = (char*)(pPacket + 1);
    int nMemberSeq = g_Sql.CheckMemberID(pMember, sBody, sBody + 31, 0);
    if (nMemberSeq >= 0)
    {
        int nServerCode = 0, nLastPlayerSeq = 0;
        g_Sql.WhereIsPlayer(nMemberSeq, &nServerCode, &nLastPlayerSeq);

        if (nServerCode == 1)   // logged in on this certify server
        {
            CMember* pTarget = GetMemberPointer(nMemberSeq);
            if (pTarget == 0)
            {
                g_Sql.SetLogOutData(nMemberSeq);
                return;
            }
            CCSGameExit cExit;
            cExit.m_nReason = 2;   // drawforce
            PacketCertifyExit(pTarget, &cExit);
            return;
        }

        CServer* pServer = FindServer(nServerCode);
        if (pServer == 0)
            return;

        CCSStsDrawforce cSTS;
        cSTS.m_nPlayerSeq = nLastPlayerSeq;
        cSTS.m_nMemberSeq = nMemberSeq;
        SendServer(pServer, &cSTS, 0);
        return;
    }

    switch (nMemberSeq)
    {
    case -3: cPacket.m_nResponse = -0xd; break;
    case -2: cPacket.m_nResponse = -0xc; LOGOUT_ERROR("CertifyLogin(): Error(%d)\n", -0xc); break;
    case -1: cPacket.m_nResponse = -0xb; LOGOUT_ERROR("CertifyLogin(): Error(%d)\n", -0xb); break;
    case -4: cPacket.m_nResponse = -0xe; break;
    default: cPacket.m_nResponse = -99;
    }
    SendMember(pMember, &cPacket, cPacket.m_nResponse);
}

// ---------------------------------------------------------------------------
// PacketEventList (CM_EVENT_LIST) - stream active reward events in pages of 30.
// ---------------------------------------------------------------------------
void PacketEventList(CMember* pMember, CHeadPacket* /*pPacket*/)
{
    CSCEventList cPacket;
    const int nBase = 0xf;   // header overhead used in the body-size formula

    int n = 0;
    for (VectorEventList::iterator it = g_Load.m_vEventList.begin();
         it != g_Load.m_vEventList.end(); ++it)
    {
        CEvent* pEvent = *it;
        if (pEvent == 0)
            continue;

        cPacket.m_cEventList[n].m_nEventType   = pEvent->m_nEventType;
        cPacket.m_cEventList[n].m_nRewardType  = pEvent->m_nRewardType;
        cPacket.m_cEventList[n].m_nRewardValue = pEvent->m_nRewardValue;
        cPacket.m_cEventList[n].m_nStartTime   = pEvent->m_nStartTime;
        cPacket.m_cEventList[n].m_nEndTime     = pEvent->m_nEndTime;
        ++n;

        if (n == 30)
        {
            cPacket.m_nBodySize = nBase + 0x1d4;
            cPacket.m_nCount    = 30;
            cPacket.m_nResponse = 0;
            SendMember(pMember, &cPacket, 0);
            n = 0;
        }
    }

    if (n != 0)
    {
        cPacket.m_nBodySize = n * 0x10 + nBase - 0xc;
        cPacket.m_nCount    = (short)n;
        cPacket.m_nResponse = 0;
        SendMember(pMember, &cPacket, 0);
    }
}

// ---------------------------------------------------------------------------
// PacketSettingInfo (CM_SETTING_INFO) - fetch another player's setting snapshot.
// ---------------------------------------------------------------------------
void PacketSettingInfo(CMember* pMember, CHeadPacket* pPacket)
{
    CSCSettingInfo cPacket;
    if (pMember == 0 || pPacket == 0)
        return;

    CSetting cSetting;
    int nPlayerSeq = *(int*)(pPacket + 1);   // body offset 0 = player seq
    if (g_Sql.GetSettingField(nPlayerSeq, &cSetting) >= 0)
        memcpy(&cPacket.m_cSetting, &cSetting, 0x58);

    cPacket.m_nResponse = 0;
    SendMember(pMember, &cPacket, 0);
}

// ---------------------------------------------------------------------------
// PacketTCPPing (CM_TCP_PING) - keepalive ack. Called both from the dispatch
// table and from CheckPingTime (pPacket defaulted to 0).
// ---------------------------------------------------------------------------
void PacketTCPPing(CMember* pMember, CHeadPacket* /*pPacket*/)
{
    CSCTCPPing cPacket;
    cPacket.m_nResponse = 0;
    SendMember(pMember, &cPacket, 0);
}

// =============================================================================
// STS (server-to-server) handlers
// =============================================================================

// ---------------------------------------------------------------------------
// PacketStsLogin (CM_STS_LOGIN) - register a game server's code and mark it up.
// ---------------------------------------------------------------------------
void PacketStsLogin(CServer* pServer, CHeadPacket* pPacket)
{
    CCSStsLogin* pIn = (CCSStsLogin*)pPacket;
    CSCStsLogin  cPacket;

    cPacket.m_nResponse    = 0;
    pServer->m_nServerCode = (int)pIn->m_nServerCode;
    SendServer(pServer, &cPacket, 0);
    g_Sql.ConnectServer(pServer->m_nServerCode);

    if (pServer->m_nServerCode == 99)
        LOGOUT_DATABASE("STS Login : Relay Server Connected\n");
    else
        LOGOUT_SCREEN("STS Login : ServerCode(%d)\n", (int)pIn->m_nServerCode);
}

// ---------------------------------------------------------------------------
// PacketUpdateWeather (CM_UPDATE_WEATHER) - rebroadcast a weather update.
// ---------------------------------------------------------------------------
void PacketUpdateWeather(CServer* pServer, CHeadPacket* pPacket)
{
    CSCUpdateWeather cPacket;
    if (pServer == 0 || pPacket == 0)
        return;

    SendAllServer(pPacket, 0);
    cPacket.m_nResponse = 0;
    SendServer(pServer, &cPacket, 0);
    LOGOUT_DATABASE("UPDATE WEATHER\n");
}

// ---------------------------------------------------------------------------
// PacketUpdateNotice (CM_UPDATE_NOTICE) - rebroadcast a notice update.
// ---------------------------------------------------------------------------
void PacketUpdateNotice(CServer* pServer, CHeadPacket* pPacket)
{
    CSCUpdateNotice cPacket;
    if (pServer == 0 || pPacket == 0)
        return;

    SendAllServer(pPacket, 0);
    cPacket.m_nResponse = 0;
    SendServer(pServer, &cPacket, 0);
    LOGOUT_DATABASE("UPDATE NOTICE\n");
}

// ---------------------------------------------------------------------------
// PacketSendBroadcast (CM_SEND_BROADCAST) - rebroadcast an operator message.
// ---------------------------------------------------------------------------
void PacketSendBroadcast(CServer* pServer, CHeadPacket* pPacket)
{
    CSCSendBroadcast cPacket;
    if (pServer == 0 || pPacket == 0)
        return;

    cPacket.m_nResponse = 0;
    SendAllServer(pPacket, 0);
    LOGOUT_DATABASE("SEND BROADCAST : %s\n", (const char*)(pPacket + 1));
}
