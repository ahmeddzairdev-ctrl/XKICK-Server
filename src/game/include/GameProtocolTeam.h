// =============================================================================
// XKICK_Game - GameProtocolTeam.h
// Clan-match TEAM wire structs, reconstructed byte-faithfully from XKICK_Game1.
// These are the team module's outgoing CSC packets; GameProtocol.h notes that the
// binary's spine has NO public CTeamInfo struct -- team snapshots go out as the
// CSC*Team packets defined here (cmd id + body size pinned to each binary ctor).
//
//   class             ctor @        cmd      body
//   ----------------  ------------  -------  ----
//   CSCTeamInfo       0808395e      0x1068   0xd5   (response + CTeamInfo 0xd4)
//   CSCChoiceTeam     08083984      0x106d   0x05   (response + int team-no)
//   CSCQuickTeam      080839aa      0x106e   0x01
//   CSCTeamPosition   080839d0      0x106f   0x85   (response + 0x84 slot block)
//   CSCChangeTeamJang 080839f6      0x10ce   0x05   (response + int jang seq)
//   CSCTeamoneInfo    08083a1c      0x10cf   0x495  (response + CTeamoneInfo)
//   CSCTeamoneEnd     08083a70      0x10d0   0x01
//   CSCLeaveTeam      08083a96      0x10cc   0x05   (response + int player seq)
//   CSCTeamReady      08083abc      0x1130   0x06   (response + int seq + kind)
//   CSCApplyMatch     08083ae2      0x1131   0x02   (response + apply kind)
//   CSCMatchRoom      08083b2e      0x1132   0x02
//
// CTeamInfo (0xd4) and CTeamoneInfo (0x494) are filled by raw offset writes inside
// CTeam::GetTeamInfo / CPlayer_GetTeamoneInfo exactly as the binary memcpy's them,
// so they are reconstructed as opaque byte blocks (matches Domain.h's approach).
// =============================================================================
#ifndef _GAME_PROTOCOL_TEAM_H_
#define _GAME_PROTOCOL_TEAM_H_

#include "Struct.h"   // CHeadPacket
#include "Command.h"  // CM_TEAM_* / CM_*_MATCH command IDs (pulls Protocol.h too)
#include <cstddef>    // offsetof

#pragma pack(push,1)

// -- team snapshot row (CTeam::GetTeamInfo) ----------------------------------
struct CTeamInfo
{
    char m_data[0xd4];
};

// -- per-teammate full profile (CPlayer_GetTeamoneInfo) ----------------------
struct CTeamoneInfo
{
    char m_data[0x494];
};

// -- outgoing CSC packets ----------------------------------------------------
class CSCTeamInfo : public CHeadPacket
{ public:
    char       m_nResponse;     // 0x0c
    CTeamInfo  m_cTeamInfo;     // 0x0d
    CSCTeamInfo() : CHeadPacket(CM_TEAM_INFO) { m_nBodySize = 0xd5; }
};

class CSCChoiceTeam : public CHeadPacket
{ public:
    char m_nResponse;           // 0x0c
    int  m_nTeamNo;             // 0x0d
    CSCChoiceTeam() : CHeadPacket(CM_CHOICE_TEAM) { m_nBodySize = 5; }
};

class CSCQuickTeam : public CHeadPacket
{ public:
    char m_nResponse;           // 0x0c
    CSCQuickTeam() : CHeadPacket(CM_QUICK_TEAM) { m_nBodySize = 1; }
};

class CSCTeamPosition : public CHeadPacket
{ public:
    char m_nResponse;           // 0x0c
    char m_nSlotBlock[0x84];    // 0x0d  (copy of CTeam+0x5c)
    CSCTeamPosition() : CHeadPacket(CM_TEAM_POSITION) { m_nBodySize = 0x85; }
};

class CSCChangeTeamJang : public CHeadPacket
{ public:
    char m_nResponse;           // 0x0c
    int  m_nJangSeq;            // 0x0d
    CSCChangeTeamJang() : CHeadPacket(CM_CHANGE_TEAMJANG) { m_nBodySize = 5; }
};

class CSCTeamoneInfo : public CHeadPacket
{ public:
    char         m_nResponse;   // 0x0c
    CTeamoneInfo m_cInfo;       // 0x0d
    CSCTeamoneInfo() : CHeadPacket(CM_TEAMONE_INFO) { m_nBodySize = 0x495; }
};

class CSCTeamoneEnd : public CHeadPacket
{ public:
    char m_nResponse;           // 0x0c
    CSCTeamoneEnd() : CHeadPacket(CM_TEAMONE_END) { m_nBodySize = 1; }
};

class CSCLeaveTeam : public CHeadPacket
{ public:
    char m_nResponse;           // 0x0c
    int  m_nPlayerSeq;          // 0x0d
    CSCLeaveTeam() : CHeadPacket(CM_LEAVE_TEAM) { m_nBodySize = 5; }
};

class CSCTeamReady : public CHeadPacket
{ public:
    char m_nResponse;           // 0x0c
    int  m_nPlayerSeq;          // 0x0d
    char m_nReadyKind;          // 0x11  (2 = un-ready, 3 = ready)
    CSCTeamReady() : CHeadPacket(CM_TEAM_READY) { m_nBodySize = 6; }
};

class CSCApplyMatch : public CHeadPacket
{ public:
    char m_nResponse;           // 0x0c
    char m_nApplyKind;          // 0x0d  (1 = apply, 2 = cancel)
    CSCApplyMatch() : CHeadPacket(CM_APPLY_MATCH) { m_nBodySize = 2; }
};

class CSCMatchRoom : public CHeadPacket
{ public:
    char m_nResponse;           // 0x0c
    char m_nPad;                // 0x0d
    CSCMatchRoom() : CHeadPacket(CM_MATCH_ROOM) { m_nBodySize = 2; }
};

// -- incoming CSC* (CLIENT->SERVER) REQUEST packets --------------------------
// Bodies start at 0x0c (right after the 0xc-byte CHeadPacket).  Each layout is
// pinned field-by-field to its binary consuming function and verified against
// XKICK_Game1 (offsets/widths == the raw k[off] reads they replace).

// CTeam::SetTeam @0809fc08  (body 0x48, struct 0x54)
struct CCSSetTeam : public CHeadPacket
{
    int   m_nTeamSeq;        // 0x0c
    char  m_nState;          // 0x10
    char  m_nMode;           // 0x11
    char  m_sTitle[0x2f];    // 0x12
    char  m_sPass[5];        // 0x41
    char  m_nAttackCode;     // 0x46
    char  m_nScaleCode;      // 0x47
    char  m_nAICode;         // 0x48
    short m_nPointCode;      // 0x49
    char  m_nAreaCode;       // 0x4b
    char  m_nStartLevel;     // 0x4c
    char  m_nEndLevel;       // 0x4d
    char  m_nCheckClub;      // 0x4e
    char  m_nCheckTime;      // 0x4f
    char  m_nCheckWeather;   // 0x50
    char  m_nCheckView;      // 0x51
    char  m_nCheckViewChat;  // 0x52
    char  m_nMaxCount;       // 0x53
};
static_assert(offsetof(CCSSetTeam, m_nTeamSeq)       == 0x0c, "CCSSetTeam.m_nTeamSeq");
static_assert(offsetof(CCSSetTeam, m_nState)         == 0x10, "CCSSetTeam.m_nState");
static_assert(offsetof(CCSSetTeam, m_nMode)          == 0x11, "CCSSetTeam.m_nMode");
static_assert(offsetof(CCSSetTeam, m_sTitle)         == 0x12, "CCSSetTeam.m_sTitle");
static_assert(offsetof(CCSSetTeam, m_sPass)          == 0x41, "CCSSetTeam.m_sPass");
static_assert(offsetof(CCSSetTeam, m_nAttackCode)    == 0x46, "CCSSetTeam.m_nAttackCode");
static_assert(offsetof(CCSSetTeam, m_nScaleCode)     == 0x47, "CCSSetTeam.m_nScaleCode");
static_assert(offsetof(CCSSetTeam, m_nAICode)        == 0x48, "CCSSetTeam.m_nAICode");
static_assert(offsetof(CCSSetTeam, m_nPointCode)     == 0x49, "CCSSetTeam.m_nPointCode");
static_assert(offsetof(CCSSetTeam, m_nAreaCode)      == 0x4b, "CCSSetTeam.m_nAreaCode");
static_assert(offsetof(CCSSetTeam, m_nStartLevel)    == 0x4c, "CCSSetTeam.m_nStartLevel");
static_assert(offsetof(CCSSetTeam, m_nEndLevel)      == 0x4d, "CCSSetTeam.m_nEndLevel");
static_assert(offsetof(CCSSetTeam, m_nCheckClub)     == 0x4e, "CCSSetTeam.m_nCheckClub");
static_assert(offsetof(CCSSetTeam, m_nCheckTime)     == 0x4f, "CCSSetTeam.m_nCheckTime");
static_assert(offsetof(CCSSetTeam, m_nCheckWeather)  == 0x50, "CCSSetTeam.m_nCheckWeather");
static_assert(offsetof(CCSSetTeam, m_nCheckView)     == 0x51, "CCSSetTeam.m_nCheckView");
static_assert(offsetof(CCSSetTeam, m_nCheckViewChat) == 0x52, "CCSSetTeam.m_nCheckViewChat");
static_assert(offsetof(CCSSetTeam, m_nMaxCount)      == 0x53, "CCSSetTeam.m_nMaxCount");
static_assert(sizeof(CCSSetTeam)                     == 0x54, "CCSSetTeam size");

// CTeam::ChangePosition @0809fa46  (body 0x06, struct 0x12)
struct CCSTeamPosition : public CHeadPacket
{
    char m_nTeamPosition[6];  // 0x0c  (per-slot requested position code)
};
static_assert(offsetof(CCSTeamPosition, m_nTeamPosition) == 0x0c, "CCSTeamPosition.m_nTeamPosition");
static_assert(sizeof(CCSTeamPosition)                    == 0x12, "CCSTeamPosition size");

// CreateTeam @080a01f8  (body 0x45, struct 0x51)
struct CCSCreateTeam : public CHeadPacket
{
    char  m_nState;            // 0x0c
    char  m_nMode;             // 0x0d
    char  m_sTitle[0x2f];      // 0x0e
    char  m_sPass[5];          // 0x3d
    char  m_nAttackCode;       // 0x42
    char  m_nScaleCode;        // 0x43
    char  m_nAICode;           // 0x44
    short m_nPointCode;        // 0x45
    char  m_nAreaCode;         // 0x47
    char  m_nStartLevel;       // 0x48
    char  m_nEndLevel;         // 0x49
    char  m_nMaxCount;         // 0x4a
    char  m_nTeamPosition[6];  // 0x4b
};
static_assert(offsetof(CCSCreateTeam, m_nState)       == 0x0c, "CCSCreateTeam.m_nState");
static_assert(offsetof(CCSCreateTeam, m_nMode)        == 0x0d, "CCSCreateTeam.m_nMode");
static_assert(offsetof(CCSCreateTeam, m_sTitle)       == 0x0e, "CCSCreateTeam.m_sTitle");
static_assert(offsetof(CCSCreateTeam, m_sPass)        == 0x3d, "CCSCreateTeam.m_sPass");
static_assert(offsetof(CCSCreateTeam, m_nAttackCode)  == 0x42, "CCSCreateTeam.m_nAttackCode");
static_assert(offsetof(CCSCreateTeam, m_nScaleCode)   == 0x43, "CCSCreateTeam.m_nScaleCode");
static_assert(offsetof(CCSCreateTeam, m_nAICode)      == 0x44, "CCSCreateTeam.m_nAICode");
static_assert(offsetof(CCSCreateTeam, m_nPointCode)   == 0x45, "CCSCreateTeam.m_nPointCode");
static_assert(offsetof(CCSCreateTeam, m_nAreaCode)    == 0x47, "CCSCreateTeam.m_nAreaCode");
static_assert(offsetof(CCSCreateTeam, m_nStartLevel)  == 0x48, "CCSCreateTeam.m_nStartLevel");
static_assert(offsetof(CCSCreateTeam, m_nEndLevel)    == 0x49, "CCSCreateTeam.m_nEndLevel");
static_assert(offsetof(CCSCreateTeam, m_nMaxCount)    == 0x4a, "CCSCreateTeam.m_nMaxCount");
static_assert(offsetof(CCSCreateTeam, m_nTeamPosition)== 0x4b, "CCSCreateTeam.m_nTeamPosition");
static_assert(sizeof(CCSCreateTeam)                   == 0x51, "CCSCreateTeam size");

// PacketChoiceTeam @08076cb4 (0x106d)  (body 0x0a, struct 0x16)
struct CCSChoiceTeam : public CHeadPacket
{
    int  m_nTeamSeq;   // 0x0c  team number to join
    char m_nType;      // 0x10  requested seat/type
    char m_sPass[5];   // 0x11  password
};
static_assert(offsetof(CCSChoiceTeam, m_nTeamSeq) == 0x0c, "CCSChoiceTeam.m_nTeamSeq");
static_assert(offsetof(CCSChoiceTeam, m_nType)    == 0x10, "CCSChoiceTeam.m_nType");
static_assert(offsetof(CCSChoiceTeam, m_sPass)    == 0x11, "CCSChoiceTeam.m_sPass");
static_assert(sizeof(CCSChoiceTeam)               == 0x16, "CCSChoiceTeam size");

#pragma pack(pop)

#endif // _GAME_PROTOCOL_TEAM_H_
