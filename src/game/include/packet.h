// =============================================================================
// XKICK_Game - packet.h
// Declarations for the Game packet layer. This is the CONTRACT the domain modules
// (Wave 3) implement against: every CM_* command in CProcess::InitFunction
// (@080a08b4) maps to a declared handler name here, byte-for-byte faithful to the
// binary's mangled symbols.
//
// Three dispatch families (see GameProcess.h):
//   * TCP client : void PacketXxx(CPlayer*,     CHeadPacket*)
//   * STS server : void XxxSts   (CServer*,     CHeadPacket*)
//   * UDP datagram: void PacketXxx(CUDPAddress*, CHeadPacket*)   (was sockaddr_in*)
//
// Send layer (packet.cpp):
//   * SendPlayer @080822c2 : WriteTCP (CSecure-encrypts the body via the player's
//     CSecure @0x530 when InitCheck() && cmd != 2000 && cmd != 0x7d1) + per-packet
//     log. The third int arg is the request/secure flag, used only for logging.
//   * SendServer/SendSTS : raw framed write on the single g_STSSocket uplink.
//
// SPINE handlers (login/exit/notice/event/ping/message/setting/buddy/blacklist/
// record/ranking/name/faculty/tools/udp) are DEFINED in packet.cpp. All other
// handlers are declared here, registered in process.cpp, and DEFINED by the
// domain module that owns them (item/skill/ceremony/training/quest/card/rndshop/
// room/team) -- see the "owner" comment after each declaration.
// =============================================================================
#ifndef _GAME_PACKET_H_
#define _GAME_PACKET_H_

#include "Main.h"          // CPlayer, CServer, CHeadPacket, command ids, SendPlayer
#include "Net.h"           // CUDPAddress
#include "LogManager.h"

class CPlayer;
class CServer;
class CRoom;
struct CUDPAddress;

// ---------------------------------------------------------------------------
// low-level send (packet.cpp)
// ---------------------------------------------------------------------------
// SendPlayer encrypts the body via pPlayer's CSecure (WriteTCP @08082e86) then
// frames+writes it; nResult is the request/secure flag (logging only).
void SendPlayer(CPlayer* pPlayer, CHeadPacket* pPacket, int nResult);   // @080822c2
void SendServer(CServer* pServer, CHeadPacket* pPacket, int nResult);   // STS write
void SendSTS   (CHeadPacket* pPacket, int nResult);                     // @08082d2e (g_STSSocket)
int  WriteTCP  (CPlayer* pPlayer, CHeadPacket* pPacket, unsigned int nLen); // @08082e86

// ---------------------------------------------------------------------------
// dispatcher fallbacks (referenced directly by CProcess::PacketProcess)
// ---------------------------------------------------------------------------
void EmptyFunction(CPlayer* pPlayer, CHeadPacket* pPacket);     // _Z13EmptyFunction... (no-op + log)
void HijackingFunction(CPlayer* pPlayer, CHeadPacket* pPacket); // _Z17HijackingFunction... (m_nSequence < 0)

// ---------------------------------------------------------------------------
// lookup helpers (packet.cpp / player.cpp)
// ---------------------------------------------------------------------------
int      GetPlayerCount();                  // _Z14GetPlayerCountv
int      GetRoomCount();                    // _Z12GetRoomCountv
int      GetRelayCount();                   // _Z13GetRelayCountv
CPlayer* GetPlayerPointer(int nSeq);        // g_PlayerHash lookup

// =============================================================================
// TCP (client) handlers - one per CM_* id installed in InitFunction.
// [SPINE] = defined in packet.cpp.  Otherwise: owner module noted.
// =============================================================================

// ---- login / lobby / session (SPINE) ----
void PacketGameLogin       (CPlayer*, CHeadPacket*);   // 0x07D0  @08074d20  [SPINE]
void PacketGameExit        (CPlayer*, CHeadPacket*);   // 0x07D1            [SPINE]
void PacketUDPConfirm      (CPlayer*, CHeadPacket*);   // 0x07D2            [SPINE]
void PacketNoticeList      (CPlayer*, CHeadPacket*);   // 0x07D3            [SPINE]
void PacketEventList       (CPlayer*, CHeadPacket*);   // 0x07D4            [SPINE]
void PacketCurrentWeather  (CPlayer*, CHeadPacket*);   // 0x07D5            [SPINE]
void PacketCurrentTime     (CPlayer*, CHeadPacket*);   // 0x07D6            [SPINE]

// ---- ping / message / setting / faculty (SPINE) ----
void PacketTCPPing         (CPlayer*, CHeadPacket* pPacket = 0); // 5000   [SPINE] (also CheckPingTime)
void PacketSendMessage     (CPlayer*, CHeadPacket*);   // 0x1389            [SPINE]
void PacketRaiseFaculty    (CPlayer*, CHeadPacket*);   // 0x138A            [SPINE]
void PacketChangeSetting   (CPlayer*, CHeadPacket*);   // 0x138B            [SPINE]
void PacketSettingInfo     (CPlayer*, CHeadPacket*);   // 0x138C            [SPINE]
void PacketPingSpeed       (CPlayer*, CHeadPacket*);   // 0x138D            [SPINE]
void PacketSaveSpeed       (CPlayer*, CHeadPacket*);   // 0x138E            [SPINE]

// ---- player info (SPINE: the two listed; rest owned by domain modules) ----
void PacketPlayerInfo      (CPlayer*, CHeadPacket*);   // 0x0834            [SPINE]
void PacketPlayerinfoEnd   (CPlayer*, CHeadPacket*);   // 0x083D            [SPINE]
void PacketSaleList        (CPlayer*, CHeadPacket*);   // 0x083E   owner: item
void PacketCardInfo        (CPlayer*, CHeadPacket*);   // 0x083A   owner: card

// ---- chained player-sub-info senders (one-arg; called from PacketPlayerInfo) ----
void PacketItemInfo        (CPlayer*);   // 0x0835   owner: item
void PacketSkillInfo       (CPlayer*);   // 0x0836   owner: skill
void PacketTrainingInfo    (CPlayer*);   // 0x0837   owner: training
void PacketCeremonyInfo    (CPlayer*);   // 0x0838   owner: ceremony
void PacketQuestInfo       (CPlayer*);   // 0x0839   owner: quest
void PacketSynchPlayer     (CPlayer*, CHeadPacket*);   // 0x096A   owner: room
void PacketCardbotInfo     (CPlayer*, CHeadPacket*);   // 0x0907   owner: card

// ---- buddy / blacklist / record / ranking / name (SPINE) ----
void PacketBuddyInfo       (CPlayer*, CHeadPacket*);   // 0x090C            [SPINE]
void PacketAddBuddy        (CPlayer*, CHeadPacket*);   // 0x090D            [SPINE]
void PacketDelBuddy        (CPlayer*, CHeadPacket*);   // 0x090E            [SPINE]
void PacketBlacklistInfo   (CPlayer*, CHeadPacket*);   // 0x0909            [SPINE]
void PacketAddBlacklist    (CPlayer*, CHeadPacket*);   // 0x090A            [SPINE]
void PacketDelBlacklist    (CPlayer*, CHeadPacket*);   // 0x090B            [SPINE]
void PacketWeeklyRecord    (CPlayer*, CHeadPacket*);   // 0x090F            [SPINE]
void PacketWeeklyRanking   (CPlayer*, CHeadPacket*);   // 0x0910            [SPINE]
void PacketChangeName      (CPlayer*, CHeadPacket*);   // 0x122B            [SPINE]
void PacketFacultyReset    (CPlayer*, CHeadPacket*);   // 0x120D            [SPINE]
void PacketCharacterSearch (CPlayer*, CHeadPacket*);   // 0x04B5            [SPINE]

// ---- admin tools (SPINE) ----
void PacketOperationTool   (CPlayer*, CHeadPacket*);   // 0x1F40 (8000)     [SPINE]
void PacketRoomTool        (CPlayer*, CHeadPacket*);   // 0x1F42            [SPINE]

// ---- room (owner: room) ----
void PacketRoomInfo        (CPlayer*, CHeadPacket*);   // 0x0898   owner: room
void PacketRoomList        (CPlayer*, CHeadPacket*);   // 0x0899   owner: room
void PacketLobbyList       (CPlayer*, CHeadPacket*);   // 0x089A   owner: room
void PacketCreateRoom      (CPlayer*, CHeadPacket*);   // 0x089B   owner: room
void PacketSetRoom         (CPlayer*, CHeadPacket*);   // 0x089C   owner: room
void PacketChoiceRoom      (CPlayer*, CHeadPacket*);   // 0x089D   owner: room
void PacketQuickRoom       (CPlayer*, CHeadPacket*);   // 0x089E   owner: room
void PacketLeaveRoom       (CPlayer*, CHeadPacket*);   // 0x08FC   owner: room
void PacketChangeJang      (CPlayer*, CHeadPacket*);   // 0x08FE   owner: room
void PacketChangeJang      (CRoom* pRoom);             // @08078830 overload (re-announce master)
void PacketAthleteInfo     (CPlayer*, CHeadPacket*);   // 0x08FF   owner: room
void PacketRobotInfo       (CPlayer*, CHeadPacket*);   // 0x0901   owner: room
void PacketChangeGround    (CPlayer*, CHeadPacket*);   // 0x0903   owner: room
void PacketChangeBall      (CPlayer*, CHeadPacket*);   // 0x0904   owner: room
void PacketChangeWeather   (CPlayer*, CHeadPacket*);   // 0x0905   owner: room
void PacketInvitePlayer    (CPlayer*, CHeadPacket*);   // 0x0906   owner: room
void PacketForceOut        (CPlayer*, CHeadPacket*);   // 0x0911   owner: room
void PacketGoalinTcp       (CPlayer*, CHeadPacket*);   // 0x096B   owner: room

// ---- game lifecycle (owner: room) ----
void PacketGameReady       (CPlayer*, CHeadPacket*);   // 0x0960   owner: room
void PacketGameStart       (CPlayer*, CHeadPacket*);   // 0x0961   owner: room
void PacketGameCount       (CPlayer*, CHeadPacket*);   // 0x0962   owner: room
void PacketGameLoad        (CPlayer*, CHeadPacket*);   // 0x0963   owner: room
void PacketGamePlay        (CPlayer*, CHeadPacket*);   // 0x0964   owner: room
void PacketGameResult      (CPlayer*, CHeadPacket*);   // 0x0965   owner: room
void PacketGameEnd         (CPlayer*, CHeadPacket*);   // 0x0966   owner: room
void PacketAutopilotMode   (CPlayer*, CHeadPacket*);   // 0x0968   owner: room
void PacketSwitchValue     (CPlayer*, CHeadPacket*);   // 0x096C   owner: room
void PacketChangeTeam      (CPlayer*, CHeadPacket*);   // 0x09C5   owner: room
void PacketChangePosition  (CPlayer*, CHeadPacket*);   // 0x09C6   owner: room
void PacketChangeMent      (CPlayer*, CHeadPacket*);   // 0x0A29   owner: room

// ---- grow / quest (owner: quest) ----
void PacketGrowupCharacter (CPlayer*, CHeadPacket*);   // 0x0A8D   owner: quest
void PacketQuestReward     (CPlayer*, CHeadPacket*);   // 0x0A8E   owner: quest
void PacketMissionReward   (CPlayer*, CHeadPacket*);   // 0x0A8F   owner: quest
void PacketExecuteQuest    (CPlayer*, CHeadPacket*);   // 0x0A90   owner: quest
void PacketQuestList       (CPlayer*, CHeadPacket*);   // 0x0DAC   owner: quest
void PacketCreateQuest     (CPlayer*, CHeadPacket*);   // 0x0DAE   owner: quest

// ---- item shop (owner: item) ----
void PacketShopItemList    (CPlayer*, CHeadPacket*);   // 0x0C1C   owner: item
void PacketEquipItem       (CPlayer*, CHeadPacket*);   // 0x0C1E   owner: item
void PacketDivestItem      (CPlayer*, CHeadPacket*);   // 0x0C1F   owner: item
void PacketBuyItem         (CPlayer*, CHeadPacket*);   // 0x0C20   owner: item
void PacketGiftItem        (CPlayer*, CHeadPacket*);   // 0x0C21   owner: item
void PacketExchangeItem    (CPlayer*, CHeadPacket*);   // 0x0C22   owner: item
void PacketPostItem        (CPlayer*, CHeadPacket*);   // 0x0C23   owner: item
void PacketBuyRandomitem   (CPlayer*, CHeadPacket*);   // 0x0C26   owner: rndshop
void PacketRandomshopitemList(CPlayer*, CHeadPacket*); // 0x0C27   owner: rndshop
void PacketEnchantItem     (CPlayer*, CHeadPacket*);   // 0x0C28   owner: item
void PacketRefreshShop     (CPlayer*, CHeadPacket*);   // 0x0C29   owner: rndshop
void PacketGiftList        (CPlayer*, CHeadPacket*);   // 0x0C2A   owner: item
void PacketGetGift         (CPlayer*, CHeadPacket*);   // 0x0C2B   owner: item
void PacketSellItem        (CPlayer*, CHeadPacket*);   // 0x0C2C   owner: item
void PacketRepairItem      (CPlayer*, CHeadPacket*);   // 0x0C2D   owner: item

// ---- skill shop (owner: skill) ----
void PacketShopSkillList   (CPlayer*, CHeadPacket*);   // 0x0C80   owner: skill
void PacketEquipSkill      (CPlayer*, CHeadPacket*);   // 0x0C82   owner: skill
void PacketDivestSkill     (CPlayer*, CHeadPacket*);   // 0x0C83   owner: skill
void PacketBuySkill        (CPlayer*, CHeadPacket*);   // 0x0C84   owner: skill
void PacketSkillSlot       (CPlayer*, CHeadPacket*);   // 0x1217   owner: skill

// ---- ceremony shop (owner: ceremony) ----
void PacketShopCeremonyList(CPlayer*, CHeadPacket*);   // 0x0D48   owner: ceremony
void PacketEquipCeremony   (CPlayer*, CHeadPacket*);   // 0x0D4A   owner: ceremony
void PacketDivestCeremony  (CPlayer*, CHeadPacket*);   // 0x0D4B   owner: ceremony
void PacketBuyCeremony     (CPlayer*, CHeadPacket*);   // 0x0D4C   owner: ceremony

// ---- training shop (owner: training) ----
void PacketShopTrainingList(CPlayer*, CHeadPacket*);   // 0x0CE4   owner: training
void PacketBuyTraining     (CPlayer*, CHeadPacket*);   // 0x0CE8   owner: training

// ---- card (owner: card) ----
void PacketEquipCard       (CPlayer*, CHeadPacket*);   // 0x0E12   owner: card
void PacketDivestCard      (CPlayer*, CHeadPacket*);   // 0x0E13   owner: card
void PacketBuyCardbooster  (CPlayer*, CHeadPacket*);   // 0x0E15   owner: card
void PacketCardEntry       (CPlayer*, CHeadPacket*);   // 0x0E17   owner: card

// ---- mix / fusion (owner: item / card) ----
void PacketMixItem1        (CPlayer*, CHeadPacket*);   // 0x0E75   owner: item
void PacketMixItem2        (CPlayer*, CHeadPacket*);   // 0x0E76   owner: item
void PacketMixCard1        (CPlayer*, CHeadPacket*);   // 0x0E7F   owner: card
void PacketMixCard2        (CPlayer*, CHeadPacket*);   // 0x0E80   owner: card

// ---- coupon (owner: item) ----
void PacketCashCoupon      (CPlayer*, CHeadPacket*);   // 0x1221   owner: item
void PacketPointCoupon     (CPlayer*, CHeadPacket*);   // 0x1222   owner: item

// ---- team (owner: team) ----
void PacketTeamInfo        (CPlayer*, CHeadPacket*);   // 0x1068   owner: team
void PacketChoiceTeam      (CPlayer*, CHeadPacket*);   // 0x106D   owner: team
void PacketQuickTeam       (CPlayer*, CHeadPacket*);   // 0x106E   owner: team
void PacketTeamPosition    (CPlayer*, CHeadPacket*);   // 0x106F   owner: team
void PacketLeaveTeam       (CPlayer*, CHeadPacket*);   // 0x10CC   owner: team
void PacketTeamReady       (CPlayer*, CHeadPacket*);   // 0x1130   owner: team
void PacketApplyMatch      (CPlayer*, CHeadPacket*);   // 0x1131   owner: team
void PacketUpdateOption    (CPlayer*, CHeadPacket*);   // 0x0C24   owner: item (option block)

// =============================================================================
// UDP (datagram) handlers - void(CUDPAddress*, CHeadPacket*).  (was sockaddr_in*)
// PacketProcessUDP @080a1e1c dispatches 9000 -> Punching, else -> Relay.
// =============================================================================
void PacketUDPPunching     (CUDPAddress*, CHeadPacket*);   // 0x2328 (9000)  [SPINE]
void PacketUDPRelay        (CUDPAddress*, CHeadPacket*);   // (else branch)   [SPINE]
void PacketUDPPing         (CPlayer*);                     // _Z13PacketUDPPingP7CPlayer (CheckPingTime)

// =============================================================================
// STS (server-to-server) handlers - void(CServer*, CHeadPacket*).
// Installed in the +0x30 map.  [SPINE] = defined in server.cpp.
// =============================================================================
void GetStsLogin           (CServer*, CHeadPacket*);   // 0x65  @08081f66    [SPINE]
void GetStsDrawforce       (CServer*, CHeadPacket*);   // 0x66  @0808222a    [SPINE]
void PacketUpdateWeather   (CServer*, CHeadPacket*);   // 0x67  @08081fcc    [SPINE]
void UpdateSchedule        (CServer*, CHeadPacket*);   // 0x68  (CM_UPDATE_SCHEDULE) [SPINE]
void PacketUpdateNotice    (CServer*, CHeadPacket*);   // 0x69  @08082056    [SPINE]
void PacketSendBroadcast   (CServer*, CHeadPacket*);   // 0x6A  @08082106    [SPINE]

#endif // _GAME_PACKET_H_
