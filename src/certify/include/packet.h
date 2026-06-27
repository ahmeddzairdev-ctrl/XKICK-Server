// =============================================================================
// packet.h - declarations for the Certify packet layer (packet.cpp):
//   * SendMember/SendServer (raw send + logging)
//   * every TCP client handler  void Packet*(CMember*, CHeadPacket*)
//   * every STS server handler  void Packet*(CServer*, CHeadPacket*)
//   * EmptyFunction<T> (no-op for unmapped commands; logged by PacketProcess)
//   * small lookup helpers (GetMemberCount/Pointer, FindServer, SendAllServer)
// The dispatch tables (process.cpp) reference these by address.
// =============================================================================
#ifndef _CERTIFY_PACKET_H_
#define _CERTIFY_PACKET_H_

#include "Main.h"          // CMember, CServer, CHeadPacket, command ids
#include "LogManager.h"

// ---- low-level send (raw write; Certify does not encrypt server->client) ----
void SendMember(CMember* pMember, CHeadPacket* pPacket, int nRequest);
void SendServer(CServer* pServer, CHeadPacket* pPacket, int nRequest);
void SendAllServer(CHeadPacket* pPacket, int nRequest);   // broadcast to game servers

// ---- lookup helpers ----
int      GetMemberCount();
CMember* GetMemberPointer(int nSeq);
CServer* FindServer(int nServerCode);

// ---- TCP (client) handlers ----
void PacketCertifyLogin(CMember*, CHeadPacket*);
void PacketInstantLogin(CMember*, CHeadPacket*);
void PacketCertifyExit(CMember*, CHeadPacket*);
void PacketMemberInfo(CMember*, CHeadPacket*);
void PacketNoticeList(CMember*, CHeadPacket*);
void PacketCharacterInfo(CMember*, CHeadPacket*);
void PacketCharacterEnd(CMember*);                 // (no packet arg; called internally)
void PacketCreateCharacter(CMember*, CHeadPacket*);
void PacketDeleteCharacter(CMember*, CHeadPacket*);
void PacketChoiceCharacter(CMember*, CHeadPacket*);
void PacketServerList(CMember*, CHeadPacket*);
void PacketChoiceServer(CMember*, CHeadPacket*);
void PacketExecuteTutorial(CMember*, CHeadPacket*);
void PacketExecuteQuest(CMember*, CHeadPacket*);
void PacketChangeSetting(CMember*, CHeadPacket*);
void PacketCharacterSearch(CMember*, CHeadPacket*);
void PacketDrawforcePlayer(CMember*, CHeadPacket*);
void PacketEventList(CMember*, CHeadPacket*);
void PacketSettingInfo(CMember*, CHeadPacket*);
// One form serves both the dispatch table and CheckPingTime's PacketTCPPing(pMember).
void PacketTCPPing(CMember*, CHeadPacket* pPacket = 0);

// ---- STS (server-to-server) handlers ----
void PacketStsLogin(CServer*, CHeadPacket*);
void PacketUpdateWeather(CServer*, CHeadPacket*);
void PacketUpdateNotice(CServer*, CHeadPacket*);
void PacketSendBroadcast(CServer*, CHeadPacket*);

// ---- generic no-op for unmapped command ids (logged by PacketProcess) ----
template <class T>
void EmptyFunction(T* /*pObject*/, CHeadPacket* pPacket)
{
    char sCommand[30];
    memset(sCommand, 0, sizeof(sCommand));
    GetCommandString(pPacket->m_nCommand, sCommand);
    LOGOUT_ERROR("unknown command : %s(%d)\n", sCommand, pPacket->m_nCommand);
}

// EmptyFunction is invoked through a void* in PacketProcess; expose the exact
// instantiations the binary used so the symbols exist for the dispatcher.
inline void EmptyFunctionMember(void* p, CHeadPacket* pk) { EmptyFunction<CMember>((CMember*)p, pk); }
inline void EmptyFunctionServer(void* p, CHeadPacket* pk) { EmptyFunction<CServer>((CServer*)p, pk); }

#endif // _CERTIFY_PACKET_H_
