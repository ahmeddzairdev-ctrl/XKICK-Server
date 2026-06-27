// =============================================================================
// process.cpp - CProcess: the single packet-dispatch + periodic-task hub.
// Reconstructed from XKICK_Game1 (unstripped):
//   CProcess::C1            @080bb298
//   CProcess::InitFunction  @080a08b4  (the authoritative id->handler table)
//   CProcess::PacketProcess @080a1926  (drain g_TCPSocket.m_vPacketList + dispatch)
//   CProcess::PacketProcessUDP @080a1e1c
//   CProcess::TimeProcess   @080a1e54  + CheckPingTime/CheckLogTime/CheckActionTime
//
// Dispatch rule (PacketProcess): each queued CBufferPool carries an owner pointer
// (m_pObject) and a CHeadPacket. Commands < 0xC9 go to the STS map (owner is a
// CServer*); commands >= 0xC9 go to the TCP map (owner is a CPlayer*). A packet
// whose m_nSequence is negative is a replay/hijack and routed to HijackingFunction.
// The buffer is released back to the pool after dispatch; the list is then cleared.
//
// Portable: std C++ only.  All three maps are built EXACTLY as the binary does.
// =============================================================================
#include "Main.h"
#include "packet.h"
#include "Net.h"
#include "Sql.h"
#include "GameLoad.h"
#include "LogManager.h"
#include <cstdio>
#include <cstring>
#include <ctime>

// CBufferPool layout used by the queue: +0 owner (void*), +4 CHeadPacket body.
// (Matches connect.cpp's SavePacketInList framing.)

// ---------------------------------------------------------------------------
// CProcess::CProcess  @080bb298  - zero timers, m_nMinute=-1, build maps.
// ---------------------------------------------------------------------------
CProcess::CProcess()
{
    m_tPingTime   = 0;
    m_tLogTime    = 0;
    m_tActionTime = 0;
    m_nMinute     = -1;
    InitFunction();
}

CProcess::~CProcess() {}

// ---------------------------------------------------------------------------
// CProcess::InitFunction  @080a08b4
// Faithful reproduction of every id->handler registration across all three maps.
// Order preserved from the binary.
// ---------------------------------------------------------------------------
void CProcess::InitFunction()
{
    // ---- TCP client handlers (+0x00 map) ----
    m_mTCPFunctionHash[2000]   = PacketGameLogin;        // 0x07D0
    m_mTCPFunctionHash[0x7d1]  = PacketGameExit;
    m_mTCPFunctionHash[0x7d2]  = PacketUDPConfirm;
    m_mTCPFunctionHash[0x7d3]  = PacketNoticeList;
    m_mTCPFunctionHash[0x138d] = PacketPingSpeed;
    m_mTCPFunctionHash[0x138e] = PacketSaveSpeed;
    m_mTCPFunctionHash[0x834]  = PacketPlayerInfo;
    m_mTCPFunctionHash[0x83e]  = PacketSaleList;
    m_mTCPFunctionHash[0x898]  = PacketRoomInfo;
    m_mTCPFunctionHash[0x899]  = PacketRoomList;
    m_mTCPFunctionHash[0x89a]  = PacketLobbyList;
    m_mTCPFunctionHash[0x89b]  = PacketCreateRoom;
    m_mTCPFunctionHash[0x89c]  = PacketSetRoom;
    m_mTCPFunctionHash[0x89d]  = PacketChoiceRoom;
    m_mTCPFunctionHash[0x89e]  = PacketQuickRoom;
    m_mTCPFunctionHash[0x8fc]  = PacketLeaveRoom;
    m_mTCPFunctionHash[0x8fe]  = PacketChangeJang;
    m_mTCPFunctionHash[0x8ff]  = PacketAthleteInfo;
    m_mTCPFunctionHash[0x901]  = PacketRobotInfo;
    m_mTCPFunctionHash[0x903]  = PacketChangeGround;
    m_mTCPFunctionHash[0x904]  = PacketChangeBall;
    m_mTCPFunctionHash[0x905]  = PacketChangeWeather;
    m_mTCPFunctionHash[0x911]  = PacketForceOut;
    m_mTCPFunctionHash[0x906]  = PacketInvitePlayer;
    m_mTCPFunctionHash[0x1068] = PacketTeamInfo;
    m_mTCPFunctionHash[0x106d] = PacketChoiceTeam;
    m_mTCPFunctionHash[0x106e] = PacketQuickTeam;
    m_mTCPFunctionHash[0x106f] = PacketTeamPosition;
    m_mTCPFunctionHash[0x10cc] = PacketLeaveTeam;
    m_mTCPFunctionHash[0x1130] = PacketTeamReady;
    m_mTCPFunctionHash[0x1131] = PacketApplyMatch;
    m_mTCPFunctionHash[0x960]  = PacketGameReady;
    m_mTCPFunctionHash[0x961]  = PacketGameStart;
    m_mTCPFunctionHash[0x962]  = PacketGameCount;
    m_mTCPFunctionHash[0x963]  = PacketGameLoad;
    m_mTCPFunctionHash[0x964]  = PacketGamePlay;
    m_mTCPFunctionHash[0x965]  = PacketGameResult;
    m_mTCPFunctionHash[0x966]  = PacketGameEnd;
    m_mTCPFunctionHash[0x9c5]  = PacketChangeTeam;
    m_mTCPFunctionHash[0x9c6]  = PacketChangePosition;
    m_mTCPFunctionHash[0xa29]  = PacketChangeMent;
    m_mTCPFunctionHash[0xa8d]  = PacketGrowupCharacter;
    m_mTCPFunctionHash[0xa8e]  = PacketQuestReward;
    m_mTCPFunctionHash[0xc1c]  = PacketShopItemList;
    m_mTCPFunctionHash[0xc1e]  = PacketEquipItem;
    m_mTCPFunctionHash[0xc1f]  = PacketDivestItem;
    m_mTCPFunctionHash[0xc20]  = PacketBuyItem;
    m_mTCPFunctionHash[0xc21]  = PacketGiftItem;
    m_mTCPFunctionHash[0xc22]  = PacketExchangeItem;
    m_mTCPFunctionHash[0xc2a]  = PacketGiftList;
    m_mTCPFunctionHash[0xc2b]  = PacketGetGift;
    m_mTCPFunctionHash[0xc2c]  = PacketSellItem;
    m_mTCPFunctionHash[0xc2d]  = PacketRepairItem;
    m_mTCPFunctionHash[0xc80]  = PacketShopSkillList;
    m_mTCPFunctionHash[0xc82]  = PacketEquipSkill;
    m_mTCPFunctionHash[0xc83]  = PacketDivestSkill;
    m_mTCPFunctionHash[0xc84]  = PacketBuySkill;
    m_mTCPFunctionHash[0xd48]  = PacketShopCeremonyList;
    m_mTCPFunctionHash[0xd4a]  = PacketEquipCeremony;
    m_mTCPFunctionHash[0xd4b]  = PacketDivestCeremony;
    m_mTCPFunctionHash[0xd4c]  = PacketBuyCeremony;
    m_mTCPFunctionHash[0xce4]  = PacketShopTrainingList;
    m_mTCPFunctionHash[0xce8]  = PacketBuyTraining;
    m_mTCPFunctionHash[0xdac]  = PacketQuestList;
    m_mTCPFunctionHash[0xdae]  = PacketCreateQuest;
    m_mTCPFunctionHash[0x120d] = PacketFacultyReset;
    m_mTCPFunctionHash[0x1217] = PacketSkillSlot;
    m_mTCPFunctionHash[0x1221] = PacketCashCoupon;
    m_mTCPFunctionHash[0x1222] = PacketPointCoupon;
    m_mTCPFunctionHash[0x122b] = PacketChangeName;
    m_mTCPFunctionHash[5000]   = PacketTCPPing;          // 0x1388
    m_mTCPFunctionHash[0x1389] = PacketSendMessage;
    m_mTCPFunctionHash[0x138a] = PacketRaiseFaculty;
    m_mTCPFunctionHash[0x138b] = PacketChangeSetting;
    m_mTCPFunctionHash[8000]   = PacketOperationTool;    // 0x1F40
    m_mTCPFunctionHash[0x1f42] = PacketRoomTool;
    m_mTCPFunctionHash[0x4b5]  = PacketCharacterSearch;
    m_mTCPFunctionHash[0xc23]  = PacketPostItem;
    m_mTCPFunctionHash[0xa8f]  = PacketMissionReward;
    m_mTCPFunctionHash[0x7d4]  = PacketEventList;
    m_mTCPFunctionHash[0x83d]  = PacketPlayerinfoEnd;
    m_mTCPFunctionHash[0x968]  = PacketAutopilotMode;
    m_mTCPFunctionHash[0x7d5]  = PacketCurrentWeather;
    m_mTCPFunctionHash[0xc24]  = PacketUpdateOption;
    m_mTCPFunctionHash[0x7d6]  = PacketCurrentTime;
    m_mTCPFunctionHash[0x138c] = PacketSettingInfo;
    m_mTCPFunctionHash[0x96a]  = PacketSynchPlayer;
    m_mTCPFunctionHash[0x907]  = PacketCardbotInfo;
    m_mTCPFunctionHash[0x83a]  = PacketCardInfo;
    m_mTCPFunctionHash[0xe12]  = PacketEquipCard;
    m_mTCPFunctionHash[0xe13]  = PacketDivestCard;
    m_mTCPFunctionHash[0xe75]  = PacketMixItem1;
    m_mTCPFunctionHash[0xe76]  = PacketMixItem2;
    m_mTCPFunctionHash[0xe7f]  = PacketMixCard1;
    m_mTCPFunctionHash[0xe80]  = PacketMixCard2;
    m_mTCPFunctionHash[0x90c]  = PacketBuddyInfo;
    m_mTCPFunctionHash[0x96b]  = PacketGoalinTcp;
    m_mTCPFunctionHash[0x96c]  = PacketSwitchValue;
    m_mTCPFunctionHash[0x90d]  = PacketAddBuddy;
    m_mTCPFunctionHash[0x909]  = PacketBlacklistInfo;
    m_mTCPFunctionHash[0x90a]  = PacketAddBlacklist;
    m_mTCPFunctionHash[0x90f]  = PacketWeeklyRecord;
    m_mTCPFunctionHash[0x90e]  = PacketDelBuddy;
    m_mTCPFunctionHash[0x90b]  = PacketDelBlacklist;
    m_mTCPFunctionHash[0x910]  = PacketWeeklyRanking;
    m_mTCPFunctionHash[0xc27]  = PacketRandomshopitemList;
    m_mTCPFunctionHash[0xc26]  = PacketBuyRandomitem;
    m_mTCPFunctionHash[0xa90]  = PacketExecuteQuest;
    m_mTCPFunctionHash[0xc28]  = PacketEnchantItem;
    m_mTCPFunctionHash[0xc29]  = PacketRefreshShop;
    m_mTCPFunctionHash[0xe15]  = PacketBuyCardbooster;
    m_mTCPFunctionHash[0xe17]  = PacketCardEntry;

    // ---- STS (server-to-server) handlers (+0x30 map) ----
    m_mSTSFunctionHash[0x65]   = GetStsLogin;            // CM_STS_LOGIN
    m_mSTSFunctionHash[0x66]   = GetStsDrawforce;
    m_mSTSFunctionHash[0x67]   = PacketUpdateWeather;
    m_mSTSFunctionHash[0x69]   = PacketUpdateNotice;
    m_mSTSFunctionHash[0x6a]   = PacketSendBroadcast;

    // ---- UDP (datagram) handlers (+0x18 map) ----
    // NOTE: the binary registers only this one entry; PacketProcessUDP actually
    // dispatches with a hardcoded 9000?Punching:Relay test (see below) and does
    // not consult the map. The registration is reproduced for fidelity.
    m_mUDPFunctionHash[9000]   = PacketUDPPunching;      // 0x2328
}

// ---------------------------------------------------------------------------
// CProcess::PacketProcess  @080a1926
// Drain g_TCPSocket.m_vPacketList (carries both client and STS packets), dispatch
// each by command range, release the buffer, then clear the list. Held under the
// packet-list mutex for the whole pass (the binary locks at entry, unlocks at end).
// ---------------------------------------------------------------------------
void CProcess::PacketProcess()
{
    g_TCPSocket.m_cQue.Lock();   // func_0x08049e78(0x81dc594): packet-list mutex

    for (VectorPacketList::iterator it = g_TCPSocket.m_vPacketList.begin();
         it != g_TCPSocket.m_vPacketList.end(); ++it)
    {
        CBufferPool* pPool = *it;
        if (pPool == 0)
        {
            LOGOUT_ERROR("PacketProcess: null buffer pool\n");
            continue;
        }

        CHeadPacket* pPacket = (CHeadPacket*)pPool->m_sBuffer;
        if (pPacket == 0)
        {
            LOGOUT_ERROR("PacketProcess: null packet\n");
            continue;
        }

        // pPool->m_pObject == 0 means the connection was already torn down.
        if (pPool->m_pObject == 0)
        {
            LOGOUT_ERROR("PacketProcess: null object, command(%d)\n", pPacket->m_nCommand);
        }
        else
        {
            // Resolve the handler. Commands < 0xC9 are STS (owner CServer*);
            // commands >= 0xC9 are client TCP (owner CPlayer*).
            void (*pSTSFunc)(CServer*, CHeadPacket*) = 0;
            void (*pTCPFunc)(CPlayer*, CHeadPacket*) = 0;
            if (pPacket->m_nCommand < 0xc9)
                pSTSFunc = m_mSTSFunctionHash[pPacket->m_nCommand];
            else
                pTCPFunc = m_mTCPFunctionHash[pPacket->m_nCommand];

            char sCommand[30];
            memset(sCommand, 0, sizeof(sCommand));

            if (pPacket->m_nSequence < 0)
            {
                // Negative sequence == replay/hijack attempt.
                HijackingFunction((CPlayer*)pPool->m_pObject, pPacket);
            }
            else
            {
                // CM_GAME_LOAD (0x963) is suppressed from the packet log.
                if (pPacket->m_nCommand != 0x963)
                {
                    GetCommandString(pPacket->m_nCommand, sCommand);
                    if (IsCommandString(pPacket->m_nCommand) &&
                        pPool->m_pObject != 0 && ((CPlayer*)pPool->m_pObject)->m_nState != 0)
                    {
                        if (pPacket->m_nCommand < 0xc9)
                        {
                            LOGOUT_SCREEN("\x1b[33m[STS]\x1b[0m ");
                            LOGOUT_PACKET(" STS %s(%d), nSize(%d)\n",
                                          sCommand, pPacket->m_nCommand, pPacket->m_nBodySize);
                        }
                        else
                        {
                            CPlayer* pPlayer = (CPlayer*)pPool->m_pObject;
                            LOGOUT_SCREEN("\x1b[33m[INP]\x1b[0m ");
                            LOGOUT_PACKET(" INP(%d), Seq(%d): %s(%d), nSize(%d)\n",
                                          pPlayer->m_nFd, pPlayer->m_nPlayerSeq, sCommand,
                                          pPacket->m_nCommand, pPacket->m_nBodySize);
                        }
                    }
                }

                if (pTCPFunc == 0 && pSTSFunc == 0)
                    EmptyFunction((CPlayer*)pPool->m_pObject, pPacket);
                else if (pPacket->m_nCommand < 0xc9)
                    pSTSFunc((CServer*)pPool->m_pObject, pPacket);
                else
                    pTCPFunc((CPlayer*)pPool->m_pObject, pPacket);
            }

            // Release the buffer back to the pool (func_0x0804a168: memset 0x800).
            memset(pPool->m_sBuffer, 0, 0x800);
        }
    }

    g_TCPSocket.m_vPacketList.clear();
    g_TCPSocket.m_cQue.Unlock();  // func_0x0804a1a8(0x81dc594)
}

// ---------------------------------------------------------------------------
// CProcess::PacketProcessUDP  @080a1e1c
// Hardcoded UDP dispatch: 9000 -> PacketUDPPunching, every other -> PacketUDPRelay.
// (The UDP source endpoint, originally sockaddr_in*, is CUDPAddress* here.)
// ---------------------------------------------------------------------------
void CProcess::PacketProcessUDP(CUDPAddress* pFrom, CHeadPacket* pPacket)
{
    if (pPacket->m_nCommand == 9000)
        PacketUDPPunching(pFrom, pPacket);
    else
        PacketUDPRelay(pFrom, pPacket);
}

// ---------------------------------------------------------------------------
// CProcess::TimeProcess  @080a1e54  - run the three maintenance timers.
// ---------------------------------------------------------------------------
void CProcess::TimeProcess()
{
    time_t tCurrentTime;
    time(&tCurrentTime);
    CheckPingTime(tCurrentTime);
    CheckLogTime(tCurrentTime);
    CheckActionTime(tCurrentTime);
}

// ---------------------------------------------------------------------------
// CProcess::CheckPingTime  @080a1e9e
// Every >2s, sweep the player pool. A player idle >39s (0x27) since its last TCP
// ping (m_reserved520 @0x520) gets pinged; if it has already missed >1 ping
// (m_nCheckTCP @0x50e) and is still alive (m_bPingCheck @0x52c) it is dropped.
// A player marked UDP-confirmed (m_nCheckUDP @0x50f == 'd'==100) idle >3s since
// its last UDP ping (m_tUdpPingTime @0x524) gets a UDP ping.
// ---------------------------------------------------------------------------
void CProcess::CheckPingTime(time_t tCurrentTime)
{
    if (tCurrentTime - m_tPingTime <= 2)
        return;
    time(&m_tPingTime);

    for (int i = 0; i < MAX_PLAYER_POOL; ++i)
    {
        CPlayer* pPlayer = &g_PlayerPool[i];
        if (pPlayer->m_nState == 0)
            continue;

        if (tCurrentTime - pPlayer->m_tTcpPingTime > 0x27)
        {
            if ((char)pPlayer->m_nCheckTCP > 1)
            {
                if (pPlayer->m_bPingCheck)
                {
                    LOGOUT_PACKET("CheckPingTime: drop idle player\n");
                    pPlayer->ExitPlayer();
                }
            }
            else
            {
                pPlayer->m_tTcpPingTime = (int)tCurrentTime;
                pPlayer->m_nCheckTCP += 1;
                PacketTCPPing(pPlayer);
            }
        }

        if (tCurrentTime - pPlayer->m_tUdpPingTime > 3 && pPlayer->m_nCheckUDP == 'd')
        {
            pPlayer->m_tUdpPingTime = (int)tCurrentTime;
            PacketUDPPing(pPlayer);
        }
    }
}

// ---------------------------------------------------------------------------
// CProcess::CheckLogTime  @080a203a
// Every >9s, and on each distinct 5-minute boundary, write a connection-count row.
// ---------------------------------------------------------------------------
void CProcess::CheckLogTime(time_t tCurrentTime)
{
    if (tCurrentTime - m_tLogTime <= 9)
        return;
    time(&m_tLogTime);

    time_t t;
    time(&t);
    std::tm tp = LocalTime(t);
    int nMinute = tp.tm_min;
    if ((nMinute % 5 == 0) && (m_nMinute != nMinute))
    {
        m_nMinute = nMinute;
        int nRoom   = GetRoomCount();
        int nRelay  = GetRelayCount();
        int nPlayer = GetPlayerCount();
        g_Sql.InsertConnectLog(nPlayer, nRelay, nRoom);
    }
}

// ---------------------------------------------------------------------------
// CProcess::CheckActionTime  @080a20ea
// Every >59s (0x3b), reload the DB-backed reward-event list.
// ---------------------------------------------------------------------------
void CProcess::CheckActionTime(time_t tCurrentTime)
{
    if (tCurrentTime - m_tActionTime <= 0x3b)
        return;
    time(&m_tActionTime);
    g_Load.LoadEventList();
}
