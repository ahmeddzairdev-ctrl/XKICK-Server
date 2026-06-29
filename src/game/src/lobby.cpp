// =============================================================================
// lobby.cpp - the lobby / room TCP handlers and the in-game lifecycle handlers
// (owner module: room). Each handler builds a CSC* reply (its ctor sets cmd id +
// body size), fills the wire fields, and dispatches via SendPlayer / SendRoom /
// SendTeam / SendAll. Response codes, field offsets, and Send* targets are pinned
// byte-for-byte to XKICK_Game1.
//
// Conventions recovered from the binary:
//   * pPlayer->m_pRoom (@0x18) is the player's current room (the lobby is the room
//     whose m_nNo == 0; g_RoomPool[0]). "in a room" means m_pRoom && m_pRoom->m_nRoomSeq>=1.
//   * SendRoom broadcasts to everyone in the room; SendPlayer to a single client.
//   * Negative SetRoom/Insert results map to negative response codes (see each case).
//
// Portable: std C++ only.
// =============================================================================
#include "Main.h"
#include "packet.h"
#include "Net.h"
#include "Sql.h"
#include "GameLoad.h"
#include "GameProtocolRoom.h"
#include "LogManager.h"
#include <algorithm>
#include <cstring>
#include <ctime>

// PacketLeaveRoom has a two-arg internal form (the public 0x8fc entry calls it
// with nKind=1; ForceOut calls it with 2; team-leave with 3).
void PacketLeaveRoom(CPlayer* pPlayer, int nKind);
void PacketChangeParent(CRoom* pRoom, int nSeq);     // @0807857c
void PacketChangeJang(CRoom* pRoom);                 // @08078830
void PacketRoomInfo(CRoom* pRoom);                   // @080768ba
void PacketAthleteInfo(CPlayer* pPlayer);            // @08078a1a (broadcast-on-join)
void PacketAthleteEnd(CRoom* pRoom);                 // @08078d66
void PacketRobotEnd(CRoom* pRoom);                   // @080792b6
void PacketLevelUp(CPlayer* pPlayer);                // @0807ab32
void PacketLeaveTeam(CPlayer* pPlayer, int nKind);   // team module

// =============================================================================
// ROOM INFO  (PacketRoomInfo 0x898)
// =============================================================================
void PacketRoomInfo(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    (void)pPacket;
    CSCRoomInfo pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0)
    {
        pkt.m_nResponse = -1;
        SendPlayer(pPlayer, &pkt, -1);
        CLogManager log("lobby.cpp", 0x1c2, 0, pPlayer, "PacketRoomInfo: no room (%d)", pkt.m_nResponse);
        log.LogOut();
    }
    else
    {
        pRoom->GetRoomInfo(&pkt.m_cRoomInfo);
        pkt.m_nResponse = 0;
        SendPlayer(pPlayer, &pkt, 0);
        PacketAthleteInfo(pPlayer);   // stream the room's athletes to the joiner
    }
}

// PacketRoomInfo(CRoom*)  @080768ba - rebroadcast the room snapshot to the room.
void PacketRoomInfo(CRoom* pRoom)
{
    CSCRoomInfo pkt;
    if (pRoom != 0)
    {
        pRoom->GetRoomInfo(&pkt.m_cRoomInfo);
        pkt.m_nResponse = 0;
        SendRoom(pRoom, &pkt, 0);
    }
}

// =============================================================================
// ROOM LIST  (PacketRoomList 0x899) - args from body: listKind, page; level @0xc4.
// =============================================================================
void PacketRoomList(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSRoomList* req = (CCSRoomList*)pPacket;
    CSCRoomList pkt;
    GetRoomList((int)req->m_nListKind, (int)req->m_nPage, (int)pPlayer->m_cLevel.m_nLevel, &pkt);
    pkt.m_nResponse = 0;
    SendPlayer(pPlayer, &pkt, 0);
}

// =============================================================================
// LOBBY LIST  (PacketLobbyList 0x89a)
// =============================================================================
void PacketLobbyList(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSLobbyList* req = (CCSLobbyList*)pPacket;
    CSCLobbyList pkt;
    GetLobbyList((int)req->m_nPage, &pkt);
    pkt.m_nResponse = 0;
    SendPlayer(pPlayer, &pkt, 0);
}

// =============================================================================
// CREATE ROOM  (PacketCreateRoom 0x89b)
// =============================================================================
void PacketCreateRoom(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCCreateRoom pkt;
    if (pPlayer->m_pRoom == 0 || pPlayer->m_pRoom->m_nRoomSeq < 1)
    {
        CRoom* pRoom = CreateRoom(pPlayer->m_nPlayerSeq, (CCSCreateRoom*)pPacket);
        if (pRoom == 0)
        {
            pkt.m_nResponse = (char)0xff;
            SendPlayer(pPlayer, &pkt, -1);
        }
        else
        {
            pkt.m_nResponse = 0;
            SendPlayer(pPlayer, &pkt, 0);

            // auto-join the creator into the room they just made.
            CCSChoiceRoom choice;
            choice.m_nRoomSeq = (int)pRoom->m_nRoomSeq;
            choice.m_nType    = 0;
            std::memcpy(choice.m_sPass, ((CCSCreateRoom*)pPacket)->m_sPass, 5);
            PacketChoiceRoom(pPlayer, &choice);
        }
    }
}

// =============================================================================
// SET ROOM  (PacketSetRoom 0x89c)
// =============================================================================
void PacketSetRoom(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCSetRoom pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0)
    {
        pkt.m_nResponse = -1;
        SendPlayer(pPlayer, &pkt, -1);
        CLogManager log("lobby.cpp", 0x225, 0, pPlayer, "PacketSetRoom: no room (%d)", pkt.m_nResponse);
        log.LogOut();
    }
    else
    {
        int ret = pRoom->SetRoom((CCSSetRoom*)pPacket);
        if (ret < 0)
        {
            switch (ret)
            {
            case -5: pkt.m_nResponse = -0xf; break;
            case -4: pkt.m_nResponse = -0xe; break;
            case -3: pkt.m_nResponse = -0xd; break;
            case -2: pkt.m_nResponse = -0xc; break;
            case -1: pkt.m_nResponse = -0xb; break;
            default: pkt.m_nResponse = -99;
            }
            SendPlayer(pPlayer, &pkt, pkt.m_nResponse);
        }
        else
        {
            pkt.m_nResponse = 0;
            SendRoom(pRoom, &pkt, 0);
            PacketRoomInfo(pRoom);
        }
    }
}

// =============================================================================
// CHOICE ROOM  (PacketChoiceRoom 0x89d)
// =============================================================================
void PacketChoiceRoom(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSChoiceRoom* req = (CCSChoiceRoom*)pPacket;
    CSCChoiceRoom pkt;
    if (pPlayer->m_pRoom == 0 || pPlayer->m_pRoom->m_nRoomSeq < 1)
    {
        CRoom* pRoom = GetRoomPointer(req->m_nRoomSeq);
        if (pRoom == 0)
        {
            pkt.m_nResponse = -1;
            SendPlayer(pPlayer, &pkt, -1);
            CLogManager log("lobby.cpp", 0x25e, 0, pPlayer, "PacketChoiceRoom: bad room (%d) seq(%d)",
                            pkt.m_nResponse, req->m_nRoomSeq);
            log.LogOut();
        }
        else
        {
            int ret = pRoom->InsertPlayerRoom(pPlayer, (int)req->m_nType, req->m_sPass);
            if (ret < 0)
            {
                switch (ret)
                {
                case -8: pkt.m_nResponse = -0x12; break;
                case -7: pkt.m_nResponse = -0x11; break;
                case -6: pkt.m_nResponse = -0x10; break;
                case -5: pkt.m_nResponse = -0xf;  break;
                case -4: pkt.m_nResponse = -0xe;  break;
                case -3: pkt.m_nResponse = -0xd;  break;
                case -2: pkt.m_nResponse = -0xc;  break;
                case -1: pkt.m_nResponse = -0xb;  break;
                default: pkt.m_nResponse = -99;
                }
                SendPlayer(pPlayer, &pkt, pkt.m_nResponse);
            }
            else
            {
                g_RoomPool[0].DeletePlayerLobby(pPlayer);
                pkt.m_nRoomSeq = (int)pRoom->m_nRoomSeq;
                pkt.m_nTeam    = pPlayer->m_nTeam;
                pkt.m_nResponse = 0;
                SendPlayer(pPlayer, &pkt, 0);
            }
        }
    }
}

// =============================================================================
// QUICK ROOM  (PacketQuickRoom 0x89e)
// =============================================================================
void PacketQuickRoom(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    (void)pPacket;
    CSCQuickRoom pkt;
    if (pPlayer->m_pRoom == 0 || pPlayer->m_pRoom->m_nRoomSeq < 1)
    {
        // ChoiceQuickRoom returns 0 on success, -1 / other on failure.
        // (signature returns void in Lobby.h; the binary returns an int code.)
        int ret = -99;
        if (pPlayer != 0 && pPlayer->m_pRoom != 0 && pPlayer->m_pRoom->m_nRoomSeq == 0)
        {
            ret = -1;
            for (int i = 1; i < MAX_ROOM_POOL; ++i)
            {
                CRoom* p = &g_RoomPool[i];
                if (p->m_nState == 0 || p->m_nState == 2 || p->m_nMode != 0) continue;
                if (p->GetRoomCource() != 0) continue;
                if (p->InsertPlayerRoom(pPlayer, 0, 0) >= 0) { ret = 0; break; }
            }
        }
        if (ret < 0)
        {
            pkt.m_nResponse = (ret == -1) ? -0xb : -99;
            SendPlayer(pPlayer, &pkt, pkt.m_nResponse);
        }
        else
        {
            g_RoomPool[0].DeletePlayerLobby(pPlayer);
            pkt.m_nResponse = 0;
            SendPlayer(pPlayer, &pkt, 0);
        }
    }
}

// =============================================================================
// LEAVE ROOM  (PacketLeaveRoom 0x8fc -> nKind 1)
// =============================================================================
void PacketLeaveRoom(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    (void)pPacket;
    PacketLeaveRoom(pPlayer, 1);
}

void PacketLeaveRoom(CPlayer* pPlayer, int nKind)
{
    CSCLeaveRoom pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0)
    {
        pkt.m_nResponse = -1;
        SendPlayer(pPlayer, &pkt, -1);
        CLogManager log("lobby.cpp", 0x4bf, 0, pPlayer, "PacketLeaveRoom: no room (%d)", pkt.m_nResponse);
        log.LogOut();
        return;
    }
    if (pRoom->m_nRoomSeq < 1 || pRoom->m_nRoomSeq > 199)
    {
        // Lobby (seq 0): the live client (newer than XKICK_Game1) sends CM_LEAVE_ROOM to
        // exit the lobby for the tutorial / room-create flow and BLOCKS its UI transition
        // on the ack (client handler sub_4BFF00 rebuilds the screen on the response). The
        // old binary replied nothing for room 0, leaving the newer client stuck. Remove
        // the player from the lobby stay-list and ack so the client proceeds.
        g_RoomPool[0].DeletePlayerLobby(pPlayer);
        pkt.m_nLeavePlayerSeq = pPlayer->m_nPlayerSeq;
        pkt.m_nLeaveTeam      = pPlayer->GetPlayerTeam();
        pkt.m_nResponse       = (char)nKind;
        SendPlayer(pPlayer, &pkt, nKind);
        return;
    }

    int ret = pRoom->DeletePlayerRoom(pPlayer);
    if (ret < 0)
    {
        pkt.m_nResponse = -2;
        SendPlayer(pPlayer, &pkt, -2);
        CLogManager log("lobby.cpp", 0x4d0, 0, pPlayer, "PacketLeaveRoom: delete fail (%d)", pkt.m_nResponse);
        log.LogOut();
        return;
    }

    // ret==1 means the room emptied & was deleted; viewers (lobbyState 3) skip the
    // mid-game cleanup. Otherwise, while a match is in progress, hand off the parent
    // and reset the game-flow flags per the room's cource.
    if (ret != 1 && pPlayer->m_nTeam != 3)
    {
        int nCource = pRoom->GetRoomCource();
        if (nCource != 0)
        {
            if (nCource == 1 || nCource == 2 || nCource == 3)
            {
                PacketChangeParent(pRoom, pPlayer->m_nPlayerSeq);
                pRoom->SetRoomCource(0);
                pRoom->InitGameReadyFlag();
                pRoom->InitGameStartFlag();
                pRoom->InitGameRelayFlag();
            }
            else if (nCource == 4)
            {
                pPlayer->JusticeAndPunishment();
                PacketChangeParent(pRoom, pPlayer->m_nPlayerSeq);
            }
            else if (nCource == 5)
            {
                PacketChangeParent(pRoom, pPlayer->m_nPlayerSeq);
            }
            else
            {
                pkt.m_nResponse = -3;
                SendPlayer(pPlayer, &pkt, -3);
                CLogManager log("lobby.cpp", 0x502, 0, pPlayer, "PacketLeaveRoom: bad cource (%d)", pkt.m_nResponse);
                log.LogOut();
                return;
            }
        }
        pRoom->SetTeamJang(pPlayer);
    }

    if (nKind != 3)
        g_RoomPool[0].InsertPlayerLobby(pPlayer);

    pkt.m_nLeavePlayerSeq = pPlayer->m_nPlayerSeq;
    pkt.m_nLeaveTeam      = pPlayer->GetPlayerTeam();
    pkt.m_nResponse       = (char)nKind;
    SendRoom(pRoom, &pkt, nKind);
    if (nKind != 3)
        SendPlayer(pPlayer, &pkt, nKind);

    // a clan match-room (cource 1) that is twinned with a team-room: also leave team.
    if (pRoom->m_nMode == 1 && pPlayer->m_pTeam != 0 && pPlayer->m_pTeam->m_nCource == 2)
        PacketLeaveTeam(pPlayer, nKind);
}

// =============================================================================
// CHANGE PARENT  (PacketChangeParent(CRoom*, int) @0807857c)
// When the leaving player WAS the relay parent, re-elect and notify the room.
// =============================================================================
void PacketChangeParent(CRoom* pRoom, int nSeq)
{
    CSCChangeParent pkt;
    CLogManager dbg(5);
    dbg.LogOut("PacketChangeParent: leave(%d) parent(%d)\n", nSeq, pRoom->m_nParentSeq);
    if (pRoom->m_nParentSeq != nSeq)
        return;

    int ret = pRoom->SetParent();
    if (ret < 0)
    {
        pkt.m_nResponse = -1;
        SendPlayer(0, &pkt, -1);
        CLogManager log("lobby.cpp", 0x53a, 0, (void*)0, "PacketChangeParent: no parent (%d)", pkt.m_nResponse);
        log.LogOut();
        return;
    }
    CPlayer* pNew = GetPlayerPointer(pRoom->m_nParentSeq);
    if (pNew == 0)
    {
        pkt.m_nResponse = -2;
        SendPlayer(0, &pkt, -2);
        CLogManager log("lobby.cpp", 0x543, 0, (void*)0, "PacketChangeParent: parent gone (%d)", pkt.m_nResponse);
        log.LogOut();
        return;
    }
    pkt.m_nParentSeq = pRoom->m_nParentSeq;
    std::memcpy(&pkt.m_cParentAddress, &pNew->m_cUDPAddress, 0x18);
    pkt.m_nResponse = 0;
    SendRoom(pRoom, &pkt, 0);
}

// =============================================================================
// CHANGE JANG  (PacketChangeJang 0x8fe + CRoom overload)
// =============================================================================
void PacketChangeJang(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    (void)pPacket;
    CSCChangeJang pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0)
    {
        pkt.m_nResponse = -1;
        SendPlayer(pPlayer, &pkt, -1);
        CLogManager log("lobby.cpp", 0x55b, 0, pPlayer, "PacketChangeJang: no room (%d)", pkt.m_nResponse);
        log.LogOut();
    }
    else
    {
        pkt.m_nRoomJangTeam = pRoom->m_nRoomJangTeam;
        pkt.m_nHomeJangSeq  = pRoom->m_nHomeJangSeq;
        pkt.m_nAwayJangSeq  = pRoom->m_nAwayJangSeq;
        pkt.m_nResponse = 0;
        SendRoom(pRoom, &pkt, 0);
    }
}

void PacketChangeJang(CRoom* pRoom)
{
    CSCChangeJang pkt;
    pkt.m_nRoomJangTeam = pRoom->m_nRoomJangTeam;
    pkt.m_nHomeJangSeq  = pRoom->m_nHomeJangSeq;
    pkt.m_nAwayJangSeq  = pRoom->m_nAwayJangSeq;
    pkt.m_nResponse = 0;
    SendRoom(pRoom, &pkt, 0);
}

// =============================================================================
// ATHLETE INFO / END  (0x8ff)
// CSCAthleteInfo body size is dynamic (binary PacketAthleteInfo @08078a1a):
//   body = 1172 - 4 * (85 - m_cItemOption.m_nOptionCnt)
// i.e. the trailing item-option block carries only m_nOptionCnt 4-byte entries, not
// the full 85. Sending the fixed 1172 leaves 4*(85-cnt) extra bytes on the wire; the
// client reads cnt options then desyncs into the next packet and crashes.
// m_nOptionCnt is the first int of the 344-byte COption block.
// =============================================================================
static int AthleteBodySize(CAthleteInfo* p)
{
    int nOptCnt = *(int*)p->m_cItemOption;          // COption.m_nOptionCnt
    return 1172 - 4 * (85 - nOptCnt);
}

void PacketAthleteInfo(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    (void)pPacket;
    CSCAthleteInfo pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0)
    {
        pkt.m_nResponse = -1;
        pkt.m_nBodySize = AthleteBodySize(&pkt.m_cAthleteInfo);
        SendPlayer(pPlayer, &pkt, -1);
        CLogManager log("lobby.cpp", 0x5a6, 0, pPlayer, "PacketAthleteInfo: no room (%d)", pkt.m_nResponse);
        log.LogOut();
    }
    else
    {
        pPlayer->GetAthleteInfo(&pkt.m_cAthleteInfo);
        pkt.m_nResponse = 0;
        pkt.m_nBodySize = AthleteBodySize(&pkt.m_cAthleteInfo);
        SendRoom(pRoom, &pkt, 0);
        PacketAthleteEnd(pRoom);
    }
}

// PacketAthleteInfo(CPlayer*)  @08078a1a - called from PacketRoomInfo on join:
// stream every OTHER athlete to the joiner, then broadcast the joiner to the room.
void PacketAthleteInfo(CPlayer* pPlayer)
{
    CSCAthleteInfo pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0)
    {
        pkt.m_nResponse = -1;
        pkt.m_nBodySize = AthleteBodySize(&pkt.m_cAthleteInfo);
        SendPlayer(pPlayer, &pkt, -1);
        CLogManager log("lobby.cpp", 0x5e2, 0, pPlayer, "PacketAthleteInfo: no room (%d)", pkt.m_nResponse);
        log.LogOut();
        return;
    }
    for (size_t g = 0; g < pRoom->m_vTotalList.size(); ++g)
    {
        std::vector<int>* pV = pRoom->m_vTotalList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* pOther = GetPlayerPointer((*pV)[i]);
            if (pOther != 0 && pOther->m_nPlayerSeq != pPlayer->m_nPlayerSeq)
            {
                pOther->GetAthleteInfo(&pkt.m_cAthleteInfo);
                pkt.m_nResponse = 0;
                pkt.m_nBodySize = AthleteBodySize(&pkt.m_cAthleteInfo);
                SendPlayer(pPlayer, &pkt, 0);
            }
        }
    }
    pPlayer->GetAthleteInfo(&pkt.m_cAthleteInfo);
    pkt.m_nResponse = 0;
    pkt.m_nBodySize = AthleteBodySize(&pkt.m_cAthleteInfo);
    SendRoom(pRoom, &pkt, 0);
    PacketAthleteEnd(pRoom);
}

void PacketAthleteEnd(CRoom* pRoom)
{
    CSCAthleteEnd pkt;
    pkt.m_nResponse = 0;
    SendRoom(pRoom, &pkt, 0);
}

// =============================================================================
// ROBOT INFO / END  (0x901)
// For an AI room, fill empty slots with bots (mode @room+0x58: 1=all empty,
// 2=only keeper '(' slots). Bot levels are normalized via GetNormalizeLevel; bot
// costume codes come from a shuffled 0..0x3c index list.
// =============================================================================
void PacketRobotInfo(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    (void)pPacket;
    CSCRobotInfo pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    int nHomeLvl = 0, nAwayLvl = 0;
    if (pRoom == 0)
    {
        pkt.m_nResponse = -1;
        SendPlayer(pPlayer, &pkt, -1);
        CLogManager log("lobby.cpp", 0x6f7, 0, pPlayer, "PacketRobotInfo: no room (%d)", pkt.m_nResponse);
        log.LogOut();
        return;
    }
    pRoom->GetNormalizeLevel(&nHomeLvl, &nAwayLvl);

    std::vector<int> idx;
    for (int i = 0; i < 0x3d; ++i) idx.push_back(i);
    // Original used std::random_shuffle (rand()-based); reproduced with a
    // rand()-driven Fisher-Yates (std::random_shuffle was removed in C++17).
    for (size_t i = idx.size(); i > 1; --i)
        std::swap(idx[i - 1], idx[rand() % i]);

    int nMode = pRoom->m_nAICode;
    if (nMode == 1 || nMode == 2)
    {
        for (int i = 0; i < 6; ++i)
        {
            bool bWant = (pRoom->m_cHomeSeat.m_nReservePosition[i] != 0) && (pRoom->m_cHomeSeat.m_nPlayerSeq[i] == 0);
            if (nMode == 2) bWant = bWant && (pRoom->m_cHomeSeat.m_nReservePosition[i] == 0x28);
            if (bWant)
            {
                pRoom->GetRobotInfo(&pkt.m_cRobotInfo, 1, i, nHomeLvl, idx[i]);
                pkt.m_nResponse = 0;
                SendRoom(pRoom, &pkt, 0);
            }
        }
        for (int i = 0; i < 6; ++i)
        {
            bool bWant = (pRoom->m_cAwaySeat.m_nReservePosition[i] != 0) && (pRoom->m_cAwaySeat.m_nPlayerSeq[i] == 0);
            if (nMode == 2) bWant = bWant && (pRoom->m_cAwaySeat.m_nReservePosition[i] == 0x28);
            if (bWant)
            {
                pRoom->GetRobotInfo(&pkt.m_cRobotInfo, 2, i, nAwayLvl, idx[i + 6]);
                pkt.m_nResponse = 0;
                SendRoom(pRoom, &pkt, 0);
            }
        }
    }
    PacketRobotEnd(pRoom);
}

void PacketRobotEnd(CRoom* pRoom)
{
    CSCRobotEnd pkt;
    pkt.m_nResponse = 0;
    SendRoom(pRoom, &pkt, 0);
}

// =============================================================================
// CARDBOT INFO  (0x907)
// Stream every player's enabled card-entry "cardbots": existing players' to the
// joiner, then the joiner's to the room, terminated by CSCCardbotEnd.
// =============================================================================
void PacketCardbotInfo(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCCardbotInfo pkt;
    if (pPlayer == 0 || pPacket == 0) return;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0)
    {
        pkt.m_nResponse = -1;
        SendPlayer(pPlayer, &pkt, -1);
        CLogManager log("lobby.cpp", 0x10f1, 0, pPlayer, "PacketCardbotInfo: no room (%d)", pkt.m_nResponse);
        log.LogOut();
        return;
    }
    int nEntry = pPlayer->m_nCardEntry;

    // existing players' cardbots -> the joiner.
    for (size_t g = 0; g < pRoom->m_vAthleteList.size(); ++g)
    {
        std::vector<int>* pV = pRoom->m_vAthleteList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* pOther = GetPlayerPointer((*pV)[i]);
            if (pOther == 0 || pOther->m_nPlayerSeq == pPlayer->m_nPlayerSeq) continue;
            for (size_t c = 0; c < pOther->m_vCardList.size(); ++c)
            {
                char* card = (char*)pOther->m_vCardList[c];
                if (card == 0 || card[8] == 0 || card[nEntry + 9] == 0) continue;
                std::memcpy(pkt.m_cCard, card, 0x17);
                pkt.m_nState      = card[nEntry + 9];
                pkt.m_nLobbyState = pOther->m_nTeam;
                pkt.m_nResponse   = 0;
                SendPlayer(pPlayer, &pkt, 0);
            }
        }
    }

    // joiner's cardbots -> the room.
    for (size_t c = 0; c < pPlayer->m_vCardList.size(); ++c)
    {
        char* card = (char*)pPlayer->m_vCardList[c];
        if (card == 0 || card[8] == 0 || card[nEntry + 9] == 0) continue;
        std::memcpy(pkt.m_cCard, card, 0x17);
        pkt.m_nState      = card[nEntry + 9];
        pkt.m_nLobbyState = pPlayer->m_nTeam;
        pkt.m_nResponse   = 0;
        SendRoom(pRoom, &pkt, 0);
    }

    CSCCardbotEnd end;
    end.m_nResponse = 0;
    SendRoom(pRoom, &end, 0);
}

// =============================================================================
// CHANGE GROUND / BALL / WEATHER  (0x903 / 0x904 / 0x905)
// =============================================================================
void PacketChangeGround(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSChangeGround* req = (CCSChangeGround*)pPacket;
    CSCChangeGround pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0)
    {
        pkt.m_nResponse = -1;
        SendPlayer(pPlayer, &pkt, -1);
        CLogManager log("lobby.cpp", 0x67f, 0, pPlayer, "PacketChangeGround: no room (%d)", pkt.m_nResponse);
        log.LogOut();
    }
    else
    {
        pRoom->m_nGroundCode = (short)req->m_nGroundCode;
        pRoom->m_nTimeCode   = (char)req->m_nTimeCode;
        pkt.m_nGroundCode = (int)pRoom->m_nGroundCode;
        pkt.m_nTimeCode   = (int)pRoom->m_nTimeCode;
        pkt.m_nResponse   = 0;
        SendRoom(pRoom, &pkt, 0);
    }
}

void PacketChangeBall(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSChangeBall* req = (CCSChangeBall*)pPacket;
    CSCChangeBall pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0)
    {
        pkt.m_nResponse = -1;
        SendPlayer(pPlayer, &pkt, -1);
        CLogManager log("lobby.cpp", 0x697, 0, pPlayer, "PacketChangeBall: no room (%d)", pkt.m_nResponse);
        log.LogOut();
    }
    else
    {
        pRoom->m_nBallCode = (short)req->m_nBallCode;
        pkt.m_nBallCode = (int)pRoom->m_nBallCode;
        pkt.m_nResponse = 0;
        SendRoom(pRoom, &pkt, 0);
    }
}

void PacketChangeWeather(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSChangeWeather* req = (CCSChangeWeather*)pPacket;
    CSCChangeWeather pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0)
    {
        pkt.m_nResponse = -1;
        SendPlayer(pPlayer, &pkt, -1);
        CLogManager log("lobby.cpp", 0x6ad, 0, pPlayer, "PacketChangeWeather: no room (%d)", pkt.m_nResponse);
        log.LogOut();
    }
    else
    {
        pRoom->m_nWeatherCode = (char)req->m_nWeatherCode;
        pkt.m_nWeatherCode = (int)pRoom->m_nWeatherCode;
        pkt.m_nResponse    = 0;
        SendRoom(pRoom, &pkt, 0);
    }
}

// =============================================================================
// INVITE PLAYER  (0x906)
// =============================================================================
void PacketInvitePlayer(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSInvitePlayer* req = (CCSInvitePlayer*)pPacket;
    CSCInvitePlayer pkt;
    if (pPlayer == 0 || pPlayer->m_pRoom == 0) return;

    CPlayer* pTarget = g_RoomPool[0].GetPlayer(req->m_nPlayerSeq);   // find target in lobby
    if (pTarget == 0)
    {
        pkt.m_nResponse = (char)0xff;
        SendPlayer(pPlayer, &pkt, -1);
    }
    else if (pTarget->m_cSetting.m_nInvite == 1 && pPlayer->m_nOperation < 2)
    {
        // target rejects invites (and inviter is not a GM)
        pkt.m_nResponse = -2;
        std::memcpy(pkt.m_sFromName, pTarget->m_sName, 0xf);
        SendPlayer(pPlayer, &pkt, pkt.m_nResponse);
    }
    else
    {
        pkt.m_nResponse = 0;
        pkt.m_nRoomSeq  = (int)pPlayer->m_pRoom->m_nRoomSeq;
        std::memcpy(pkt.m_sFromName, pPlayer->m_sName, 0xf);
        std::memcpy(pkt.m_sMessage,  req->m_sMessage, 0x79);
        std::memcpy(pkt.m_sPass,     pPlayer->m_pRoom->m_sPass, 5);
        SendPlayer(pTarget, &pkt, pkt.m_nResponse);   // deliver to target
    }
}

// =============================================================================
// FORCE OUT  (0x911) - room master / GM kicks a player.
// =============================================================================
void PacketForceOut(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSForceOut* req = (CCSForceOut*)pPacket;
    CSCForceOut pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0)
    {
        pkt.m_nResponse = -2;
        SendPlayer(pPlayer, &pkt, -2);
        CLogManager log("lobby.cpp", 0x6c4, 0, pPlayer, "PacketForceOut: no room (%d)", pkt.m_nResponse);
        log.LogOut();
        return;
    }
    {
        CLogManager log("lobby.cpp", 0x6c7, 0);
        log.LogOut("PacketForceOut: room(%d) by(%d) target(%d)\n",
                   (int)pRoom->m_nRoomSeq, pPlayer->m_nPlayerSeq, req->m_nPlayerSeq);
    }
    CPlayer* pTarget = GetPlayerPointer(req->m_nPlayerSeq);
    if (pTarget == 0)
    {
        pkt.m_nResponse = -5;
        SendPlayer(pPlayer, &pkt, -5);
        CLogManager log("lobby.cpp", 0x6ce, 0, pPlayer, "PacketForceOut: target gone (%d)", pkt.m_nResponse);
        log.LogOut();
    }
    else if (!pRoom->IsRoomJang(pPlayer->m_nPlayerSeq) && pPlayer->m_nOperation < 2)
    {
        pkt.m_nResponse = (char)0xfd;   // not authorized
        SendPlayer(pPlayer, &pkt, -3);
    }
    else if (pTarget->m_nOperation < 2)
    {
        PacketLeaveRoom(pTarget, 2);    // kick
    }
    else
    {
        pkt.m_nResponse = (char)0xfc;   // target is GM-protected
        SendPlayer(pPlayer, &pkt, -4);
    }
}

// =============================================================================
// CHANGE TEAM  (0x9c5)
// =============================================================================
void PacketChangeTeam(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSChangeTeam* req = (CCSChangeTeam*)pPacket;
    CSCChangeTeam pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0)
    {
        pkt.m_nResponse = (char)0xff;
        SendPlayer(pPlayer, &pkt, -1);
    }
    else if (pPlayer->m_nTeam == req->m_nChangeTeam)
    {
        pkt.m_nResponse = (char)0xfe;
        SendPlayer(pPlayer, &pkt, -2);
    }
    else
    {
        pkt.m_nFromTeam = pPlayer->m_nTeam;
        pRoom->ChangeTeam(pPlayer, (int)req->m_nChangeTeam);
        // (ChangeTeam already validated the move; the binary inlines the result.)
        pkt.m_nPlayerSeq = pPlayer->m_nPlayerSeq;
        pkt.m_nToTeam    = pPlayer->m_nTeam;
        pkt.m_nSeat      = pPlayer->m_nSeat;
        std::memcpy(&pkt.m_cHomeSeat, &pRoom->m_cHomeSeat, 0x84);
        std::memcpy(&pkt.m_cAwaySeat, &pRoom->m_cAwaySeat, 0x84);
        std::memcpy(&pkt.m_cViewSeat, &pRoom->m_cViewSeat, 0x84);
        pkt.m_nResponse = 0;
        SendRoom(pRoom, &pkt, 0);
    }
}

// =============================================================================
// CHANGE POSITION  (0x9c6) - team master sets the formation.
// =============================================================================
void PacketChangePosition(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCChangePosition pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0)
    {
        pkt.m_nResponse = -1;
        SendPlayer(pPlayer, &pkt, -1);
        CLogManager log("lobby.cpp", 0x91e, 0, pPlayer, "PacketChangePosition: no room (%d)", pkt.m_nResponse);
        log.LogOut();
    }
    else if (!pRoom->IsTeamJang(pPlayer->m_nPlayerSeq))
    {
        pkt.m_nResponse = (char)0xfe;
        SendPlayer(pPlayer, &pkt, -2);
    }
    else
    {
        int ret = pRoom->ChangePosition((CCSChangePosition*)pPacket);
        if (ret < 0)
        {
            switch (ret)
            {
            case -1: pkt.m_nResponse = -0xb; break;
            case -2: pkt.m_nResponse = -0xc; break;
            case -3: pkt.m_nResponse = -0xd; break;
            case -4: pkt.m_nResponse = -0xe; break;
            }
            SendRoom(pRoom, &pkt, pkt.m_nResponse);
        }
        else
        {
            std::memcpy(&pkt.m_cHomeSeat, &pRoom->m_cHomeSeat,  0x84);
            std::memcpy(&pkt.m_cAwaySeat, &pRoom->m_cAwaySeat, 0x84);
            pkt.m_nResponse = 0;
            SendRoom(pRoom, &pkt, 0);
        }
    }
}

// =============================================================================
// CHANGE MENT  (0xa29)
// =============================================================================
void PacketChangeMent(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCChangeMent pkt;
    int ret = g_Sql.UpdateMentField(pPlayer, ((CCSChangeMent*)pPacket)->m_sMent);
    if (ret < 0)
    {
        if (ret == -2)
        {
            pkt.m_nResponse = -0xc;
            CLogManager log("lobby.cpp", 0x951, 0, &g_Sql, "ChangeMent fail (%d)", -12);
            log.LogOut();
        }
        else if (ret == -1)
        {
            pkt.m_nResponse = -0xb;
            CLogManager log("lobby.cpp", 0x94d, 0, &g_Sql, "ChangeMent fail (%d)", -11);
            log.LogOut();
        }
        else
        {
            pkt.m_nResponse = -99;
        }
        SendPlayer(pPlayer, &pkt, pkt.m_nResponse);
    }
    else
    {
        std::memcpy(pkt.m_sMent, pPlayer->m_sMent, 0x2d);
        pkt.m_nResponse = 0;
        SendPlayer(pPlayer, &pkt, 0);
    }
}

// =============================================================================
// GROW UP CHARACTER  (0xa8d) - change formation position; resend player info.
// =============================================================================
void PacketGrowupCharacter(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSGrowupCharacter* req = (CCSGrowupCharacter*)pPacket;
    CSCGrowupCharacter pkt;
    pPlayer->ChangePosition((int)req->m_nPosition);
    pPlayer->GetPlayerInfo(&pkt.m_cPlayerInfo);
    pkt.m_nPosition = req->m_nPosition;   // rebuild: binary CSCGrowupCharacter echoes the new position
    pkt.m_nResponse = 0;
    pkt.m_nBodySize = (int)(sizeof(CSCGrowupCharacter) - sizeof(CHeadPacket));
    SendPlayer(pPlayer, &pkt, 0);
}

// =============================================================================
// ===========================  GAME LIFECYCLE  ===============================
// =============================================================================

// GAME READY  (0x960)
void PacketGameReady(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSGameReady* req = (CCSGameReady*)pPacket;
    CSCGameReady pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0)
    {
        pkt.m_nResponse = -1;
        SendPlayer(pPlayer, &pkt, -1);
        return;
    }
    char nReady = req->m_nReady;
    if (nReady == 1)        // clan-team start request
    {
        if (!pRoom->IsRoomJang(pPlayer->m_nPlayerSeq))
        {
            pkt.m_nResponse = (char)0xfe;
            SendPlayer(pPlayer, &pkt, -2);
        }
        else
        {
            pRoom->InitGameReadyFlag();
            pRoom->InitGameStartFlag();
            pkt.m_nReady = 1;
            pkt.m_nResponse = 0;
            SendRoom(pRoom, &pkt, 0);
            pRoom->SetRoomCource(1);
        }
    }
    else if (nReady == 0)
    {
        if (pRoom->GetRoomCource() < 1)
        {
            if (!pRoom->IsRoomJang(pPlayer->m_nPlayerSeq))
            {
                pkt.m_nResponse = (char)0xfe;
                SendPlayer(pPlayer, &pkt, -2);
            }
            else if (!pRoom->CheckGameStart() && pPlayer->m_nOperation < 2)
            {
                pkt.m_nResponse = (char)0xfc;
                SendPlayer(pPlayer, &pkt, -4);
            }
            else
            {
                pRoom->InitGameReadyFlag();
                pRoom->InitGameStartFlag();
                pkt.m_nReady = 0;
                pkt.m_nResponse = 0;
                SendRoom(pRoom, &pkt, 0);
                pRoom->SetRoomCource(1);
            }
        }
        else   // cancel from an in-progress clan flow
        {
            if (!pRoom->IsTeamJang(pPlayer->m_nPlayerSeq))
            {
                pkt.m_nResponse = (char)0xfd;
                SendPlayer(pPlayer, &pkt, -3);
            }
            else
            {
                pRoom->InitGameReadyFlag();
                pRoom->InitGameStartFlag();
                pkt.m_nReady      = 5;
                pkt.m_nCancelTeam = pPlayer->m_nTeam;
                pkt.m_nResponse   = 0;
                SendRoom(pRoom, &pkt, 0);
                pRoom->SetRoomCource(0);
            }
        }
    }
    else if (nReady == 2)
    {
        pPlayer->m_bIsGameReady = 0;
    }
    else if (nReady == 3 && pPlayer->m_bIsGameReady == 0)
    {
        pPlayer->m_bIsGameReady = 1;
        if (pRoom->IsGameReadyFlag())
        {
            pkt.m_nReady = 4;
            pkt.m_nResponse = 0;
            SendRoom(pRoom, &pkt, 0);
        }
    }
}

// GAME START  (0x961)
void PacketGameStart(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    (void)pPacket;
    CSCGameStart pkt;
    CRoom* pRoom = pPlayer->m_pRoom;

    int n = pRoom->SetParent();
    if (n < 0)
    {
        pkt.m_nResponse = -2;
        SendPlayer(pPlayer, &pkt, -2);
        CLogManager log("lobby.cpp", 0x620, 0, pPlayer, "PacketGameStart: no parent (%d)", pkt.m_nResponse);
        log.LogOut();
        return;
    }
    CPlayer* pParent = GetPlayerPointer(pRoom->m_nParentSeq);
    if (pParent == 0)
    {
        pkt.m_nResponse = -3;
        SendPlayer(pPlayer, &pkt, -3);
        CLogManager log("lobby.cpp", 0x62a, 0, pPlayer, "PacketGameStart: parent gone (%d)", pkt.m_nResponse);
        log.LogOut();
        return;
    }

    if (pRoom->m_nMode == 2)   // clan match: validate point/headcount balance
    {
        char nResp = 0;
        if (pRoom->GetMinPoint() < pRoom->m_nPointCode)            nResp = -0xe;
        else if (pRoom->GetTeamAmount(1) != pRoom->GetTeamAmount(2)) nResp = -0xf;
        else if (pRoom->GetTeamAmount(4) > 0)                       nResp = -0x10;
        if (nResp < 0)
        {
            pRoom->SetRoomCource(0);
            pkt.m_nResponse = nResp;
            SendPlayer(pPlayer, &pkt, nResp);
            CSCGameReady cancel;
            cancel.m_nReady = 5;
            cancel.m_nCancelTeam = 0;
            cancel.m_nResponse = 0;
            SendRoom(pRoom, &cancel, 0);
            return;
        }
    }

    pRoom->CreateMission();
    pkt.m_nParentSeq = pRoom->m_nParentSeq;
    std::memcpy(&pkt.m_cParentAddress, &pParent->m_cUDPAddress, 0x18);
    std::memcpy(&pkt.m_cMission, &pRoom->m_cMission, sizeof(CMission));
    // rebuild: binary CSCGameStart also carries weather/time/random/attack-team.
    pkt.m_nWeatherCode = (int)pRoom->m_nWeatherCode;
    pkt.m_nTimeCode    = (int)pRoom->m_nTimeCode;
    pkt.m_nRandom      = (short)((int)time(0) % 10000);
    pkt.m_nAttackTeam  = pRoom->m_nAttackTeam;
    pkt.m_nResponse = 0;
    SendRoom(pRoom, &pkt, 0);
    pRoom->m_tGameStartTime = 0;    // clear in-game elapsed timestamp
}

// GAME COUNT  (0x962)
void PacketGameCount(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSGameCount* req = (CCSGameCount*)pPacket;
    CSCGameCount pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0) { pkt.m_nResponse = -1; SendPlayer(pPlayer, &pkt, -1); return; }
    pkt.m_nCount = req->m_nCount;
    pkt.m_nResponse = 0;
    SendRoom(pRoom, &pkt, 0);
    pRoom->SetRoomCource(2);
}

// GAME LOAD  (0x963)
void PacketGameLoad(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSGameLoad* req = (CCSGameLoad*)pPacket;
    CSCGameLoad pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0) { pkt.m_nResponse = -1; SendPlayer(pPlayer, &pkt, -1); return; }
    pkt.m_nPlayerSeq = pPlayer->m_nPlayerSeq;
    pkt.m_nStep      = req->m_nStep;
    pkt.m_nResponse  = 0;
    SendRoom(pRoom, &pkt, 0);
    if (pRoom->GetRoomCource() != 0)
        pRoom->SetRoomCource(3);
}

// GAME PLAY  (0x964)
void PacketGamePlay(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    (void)pPacket;
    CSCGamePlay pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0) { pkt.m_nResponse = -1; SendPlayer(pPlayer, &pkt, -1); return; }
    pPlayer->m_bIsGameStart = 1;
    if (!pRoom->IsGameStartFlag())
    {
        CLogManager log(5);
        log.LogOut("PacketGamePlay: not all started\n");
        return;
    }
    pkt.m_nResponse = 0;
    SendRoom(pRoom, &pkt, 0);
    if (pRoom->m_tGameStartTime == 0)
        time((time_t*)&pRoom->m_tGameStartTime);   // mark kickoff time
    pRoom->SetRoomCource(4);
    pRoom->InitGameStartFlag();
}

// GAME RESULT  (0x965)
void PacketGameResult(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CCSGameResult* req = (CCSGameResult*)pPacket;
    CSCGameResult pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0) { pkt.m_nResponse = -1; SendPlayer(pPlayer, &pkt, -1); return; }
    char nCource = pRoom->m_nMode;

    // apply each home/away player's record/level/consume, except for
    // friendly/no-record cources (3, 6, 8).
    for (size_t g = 0; g < pRoom->m_vAthleteList.size(); ++g)
    {
        std::vector<int>* pV = pRoom->m_vAthleteList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* pl = GetPlayerPointer((*pV)[i]);
            if (pl == 0) continue;
            if (nCource == 3 || nCource == 6 || nCource == 8) continue;
            pl->SetRecord((CCSGameResult*)pPacket);
            pl->SetLevel((CCSGameResult*)pPacket);
            pl->SetConsume((CCSGameResult*)pPacket);
        }
    }

    // echo the result block back to the room (request body is laid out identically).
    pkt.m_nMvpSeq      = req->m_nMvpSeq;
    pkt.m_nMvpLevel    = req->m_nMvpLevel;
    pkt.m_nMvpPosition = req->m_nMvpPosition;
    std::memcpy(pkt.m_sMvpName, req->m_sMvpName, 0xf);
    pkt.m_fCurrentTime = req->m_fCurrentTime;
    std::memcpy(&pkt.m_cHomeResult, &req->m_cHomeResult, sizeof(CResult));
    std::memcpy(&pkt.m_cAwayResult, &req->m_cAwayResult, sizeof(CResult));
    std::memcpy(pkt.m_cEachResult, req->m_cEachResult, sizeof(pkt.m_cEachResult));
    pkt.m_nResponse = 0;
    SendRoom(pRoom, &pkt, 0);

    // notify level-ups.
    for (size_t g = 0; g < pRoom->m_vAthleteList.size(); ++g)
    {
        std::vector<int>* pV = pRoom->m_vAthleteList[g];
        for (size_t i = 0; i < pV->size(); ++i)
        {
            CPlayer* pl = GetPlayerPointer((*pV)[i]);
            if (pl != 0) PacketLevelUp(pl);
        }
    }

    if (pRoom->m_nAttackCode == 2)   // clan room: bank the win side from the result body
    {
        short nWin = req->m_cHomeResult.m_nResult[0];
        if (nWin == 1)      pRoom->m_nAttackTeam = 2;
        else if (nWin == 3) pRoom->m_nAttackTeam = 1;
    }
    pRoom->SetAttackTeam();
    pRoom->m_nMission = -1;
    pRoom->SetRoomCource(5);
}

// GAME END  (0x966)
void PacketGameEnd(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    (void)pPacket;
    CSCGameEnd pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0) { pkt.m_nResponse = -1; SendPlayer(pPlayer, &pkt, -1); return; }
    pkt.m_nResponse = 0;
    SendRoom(pRoom, &pkt, 0);
    pRoom->SetRoomCource(0);
    pRoom->InitGameRelayFlag();
    if (pRoom->m_nMode == 1)   // clan match-room: tear down the twinned rooms
    {
        // reset the paired home/away team-rooms' cource byte before deleting.
        if (pRoom->m_pHomeTeam != 0) pRoom->m_pHomeTeam->m_nCource = 0;
        if (pRoom->m_pAwayTeam != 0) pRoom->m_pAwayTeam->m_nCource = 0;
        pRoom->DeleteRoom();
    }
}

// LEVEL UP  (0x967, void(CPlayer*))
void PacketLevelUp(CPlayer* pPlayer)
{
    CSCLevelUpGame pkt;
    CRoom* pRoom = pPlayer->m_pRoom;
    if (pRoom == 0) return;
    char nCource = pRoom->m_nMode;
    if (nCource == 3 || nCource == 8) return;

    pkt.m_nPlayerSeq = pPlayer->m_nPlayerSeq;
    std::memcpy(&pkt.m_cMoney, &pPlayer->m_cMoney, 16);
    // m_nLevelBlock is a verbatim 12-byte copy of CPlayer::m_cLevel; only the middle
    // int (m_nExp @+4) is a clean named field, the outer ints straddle the
    // short m_nLevel / short m_nFaculty+m_nSkill members.
    pkt.m_nLevelBlock[0] = *(int*)((char*)&pPlayer->m_cLevel);
    pkt.m_nLevelBlock[1] = pPlayer->m_cLevel.m_nExp;
    pkt.m_nLevelBlock[2] = *(int*)((char*)&pPlayer->m_cLevel + 8);
    std::memcpy(&pkt.m_cBaseFaculty, &pPlayer->m_cBaseFaculty, 25);
    pkt.m_nResponse = 0;
    if ((char)pPlayer->m_cLevel.m_nLevel < 6)
        SendPlayer(pPlayer, &pkt, 0);
    else
        SendRoom(pRoom, &pkt, 0);
}

// AUTOPILOT MODE  (0x968)
void PacketAutopilotMode(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCAutopilotMode pkt;
    if (pPlayer == 0 || pPacket == 0) return;
    CCSAutopilotMode* req = (CCSAutopilotMode*)pPacket;
    pPlayer->EnableAutoPilot(req->m_bEnable != 0);
    pkt.m_nMode      = req->m_bEnable;
    pkt.m_nPlayerSeq = pPlayer->GetPlayerSeq();
    pkt.m_nResponse  = 0;
    SendRoom(pPlayer->m_pRoom, &pkt, 0);
}

// SYNCH PLAYER  (0x96a, void(CPlayer*))
void PacketSynchPlayer(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    (void)pPacket;
    CSCSynchPlayer pkt;
    if (pPlayer == 0) return;
    CRoom* pRoom = pPlayer->m_pRoom;
    pPlayer->m_bSynch = 1;
    if (!pRoom->CheckSynchFlag())
    {
        CLogManager log(5);
        log.LogOut("PacketSynchPlayer: waiting for synch\n");
    }
    else
    {
        CLogManager log(5);
        log.LogOut("PacketSynchPlayer: all synched\n");
        pkt.m_nResponse = 0;
        SendRoom(pRoom, &pkt, 0);
        pRoom->InitSynchFlag();
    }
}

// GOAL-IN (TCP)  (0x96b)
void PacketGoalinTcp(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCGoalinTcp pkt;
    if (pPlayer == 0 || pPacket == 0) return;
    std::memcpy(pkt.m_cGoalin, (char*)pPacket + 0xc, 0x23);
    pkt.m_nResponse = 0;
    SendRoom(pPlayer->m_pRoom, &pkt, 0);
}

// SWITCH VALUE  (0x96c)
void PacketSwitchValue(CPlayer* pPlayer, CHeadPacket* pPacket)
{
    CSCSwitchValue pkt;
    if (pPlayer == 0 || pPacket == 0) return;
    CCSSwitchValue* req = (CCSSwitchValue*)pPacket;
    pkt.m_nSwitch    = req->m_nType;
    pkt.m_nValue     = req->m_nValue;
    pkt.m_nPlayerSeq = pPlayer->GetPlayerSeq();
    pkt.m_nResponse  = 0;
    if (pPlayer->m_pRoom != 0)
        SendRoom(pPlayer->m_pRoom, &pkt, 0);
}
