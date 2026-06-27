#ifndef _STRUCT_H_
#define _STRUCT_H_

#include "Define.h"
#include <cstring>   // memset (CSetting ctor)
#include <map>       // CStringArray / CScheduleArray / CWeatherArray typedefs
//ĳ���� ���� ����//////////////////////////////////////////////////////////////////
//ĳ���� ���� ���� Ŭ����
class CShape
{
public:
	char		m_nGender;
	char		m_nSkin;
	char		m_nUniform;
};

//���� ���� Ŭ����
class CLevel
{
public:
	short		m_nLevel;
	int			m_nExp;
	short		m_nFaculty;
	short		m_nSkill;
};

//�� ���� Ŭ����
class CMoney
{
public:
	int			m_nCash;
	int			m_nPoint;
	int			m_nCredit;
	int			m_nClubPoint;
};

//���� ũ�� ���� Ŭ����
class CSize
{
public:
	char		m_nCharacterSize;
	char		m_nInvenSize;
	char		m_nSkillSize;
};

//Ŭ�� ���� Ŭ����
class CClub
{
public:
	char		m_sClubNmae[CLUB_NAME_SIZE];
	int			m_nClubSeq;
	char		m_nClubGradeCode;
};

//�ּ� ���� Ŭ����
class CAddress
{
public:
	char		m_sIP[IP_SIZE];
	int			m_nPort;
};

class CFaculty
{
public:
	char		m_nFaculty[ARRAY_FACULTY_SIZE];
};

class CItemFacultyInfo
{
public:
	CItemFacultyInfo() { m_nFacultyCnt = 0; }
	char				m_nFacultyCnt;
	unsigned int		m_nFaculty[MAX_ITEM_FACULTY_SIZE];
};

// �ɷ�ġ �˻縦 ���� Int���� (CYG)
class CFacultyInt
{
public:
	int			m_nFaculty[ARRAY_FACULTY_SIZE];
};

// ģ������
class CBuddyInfo
{
public:
	int			m_nSeq;
	int			m_nPlayerSeq;
	int			m_nState;
	int			m_nWhere;
	short		m_nLevel;
	char		m_nPosition;
	char		m_sName[PLAYER_NAME_SIZE];

};

class CResult
{
public:
	short		m_nResult[ARRAY_RESULT_SIZE];
};

class CRecord
{
public:
	int			m_nRecord[ARRAY_RECORD_SIZE]; // ���ڵ� ������ ����(CYG)

public:
	CRecord		operator+(const CRecord& cRecord)
	{
		for(int i=0;i<ARRAY_RECORD_SIZE;++i)
		{
			m_nRecord[i] += cRecord.m_nRecord[i];
		}
		return *this;
	}

	bool IsEmpty()
	{
		for(int i=0;i<ARRAY_RECORD_SIZE;++i)
		{
			if( m_nRecord[i] != 0 ) return false;
		}
		return true;
	}

};


class CRanking
{
public:
	short		m_nRanking[ARRAY_RANKING_SIZE];
};

#pragma pack(push,1)

class CSkillInfo
{
public:
	int					m_nSkillSeq;
	int					m_nCode;
	char				m_nEquipKind;
	char				m_nLevel;
};

class CTrainingInfo
{
public:
	int					m_nTrainingSeq;
	int					m_nCode;
	char				m_nLevel;
};

class CCeremonyInfo
{
public:
	int					m_nCeremonySeq;
	int					m_nCode;
	char				m_nEquipKind;
};

class CSetting
{
public:
	char		m_nCameraType;								//ī�޶� ���� enum CAMERA_TYPE
	char		m_nCameraTarget;							//ī�޶� ���ü enum CAMERA_TARGET
	char		m_nCameraTeam;
	char		m_nCameraZoom;								//ī�޶� ���ξƿ�
	char		m_nRadian;									//ī�޶� ����
	char		m_nShadow;									//�׸��� ����
	char		m_nLabel;									//�� ���� ����(0:�󺧾Ⱥ�, 1:�����Ǹ�, 2:������+ĳ����)

	char		m_nSound;									//ȿ���� ����(0�� ��� ���� off)
	char		m_nMusic;									//����� ����(0�� ��� ���� off)

	char		m_nInvite;									//�ʴ� ���ϱ� ����
	char		m_nWhisper;									//�ӼӸ� ���ϱ� ����
	char		m_nFriend;									//ģ���߰� ��� ����

	short		m_nDefineKey[ARRAY_KEY_SIZE];				//Ű ����

	int			m_nAttackSkillCode[ARRAY_SKILL_KEY_SIZE];	//���� ��ų ����
	int			m_nDefenceSkillCode[ARRAY_SKILL_KEY_SIZE];	//��� ��ų ����



	CSetting()
	{
		memset(this, 0, sizeof(CSetting));
	}
};



#pragma pack(pop)    // rebuild: close the pack(1) region (was unbalanced)
#pragma pack(push,4)

class CMission
{
public:
	int					m_nSeq;		// �̼��ڵ�
	int					m_nCount;	// �������� ī��Ʈ
	int					m_nReward;  // ����
	char				m_nKind;	// OBJECT_KIND_POINT �Ǵ� OBJECT_KIND_EXP
};

/*
//�ɷ�ġ ���� Ŭ����
class CFaculty
{
public:
	char		m_nRun;			//�޸���
	char		m_nDribble;		//�帮��
	char		m_nQuick;		//���߷�

	char		m_nStamina;		//�ٷ�
	char		m_nElasticity;	//ź��
	char		m_nBodycheck;	//���ο�

	char		m_nCross;		//ũ�ν�
	char		m_nShortpass;	//���н�
	char		m_nLongpass;	//���н�

	char		m_nHeading;		//���
	char		m_nShortshoot;	//�ܰɽ�
	char		m_nLongshoot;	//�߰ɽ�

	char		m_nKeeping;		//Ű��
	char		m_nTackle;		//��Ŭ
	char		m_nSteal;		//��ƿ

	char		m_nCatch;		//ĳġ
	char		m_nPunch;		//��ġ
	char		m_nGuard;		//����

	char		m_nMove;		//�̵��ɷ�
	char		m_nBody;		//��ü�ɷ�
	char		m_nPass;		//�н��ɷ�
	char		m_nShoot;		//���ôɷ�
	char		m_nDefense;		//���ɷ�
	char		m_nGoalkeep;	//��Ű�δɷ�

	char		m_nTalent;		//���
};

//��� ���� Ŭ����
class CRecord
{
public:
	int			m_nMatch;
	int			m_nWin;
	int			m_nDraw;
	int			m_nMvp;
	int			m_nGoal;
	int			m_nAssist;
	int			m_nCut;
	int			m_nShoot;
	int			m_nSteal;
	int			m_nTackle;
	int			m_nPass;
	int			m_nTryShoot;
	int			m_nTrySteal;
	int			m_nTryTackle;
	int			m_nTryPass;
	int			m_nMark;
};

//��ŷ ���� Ŭ����
class CRanking
{
public:
	short		m_nMatch;
	short		m_nWin;
	short		m_nWinPoint; //����
	short		m_nMvp;
	short		m_nGoal;
	short		m_nAssist;
	short		m_nCut;
	short		m_nShoot;
	short		m_nSteal;
	short		m_nTackle;
	short		m_nPass;
	short		m_nGoalAverage;
	short		m_nAssistAverage;
	short		m_nCutAverage;
	short		m_nShootAverage;
	short		m_nStealAverage;
	short		m_nTackleAverage;
	short		m_nPassAverage;
	short		m_nMarkAverage;
	short		m_nWinRate;
	short		m_nShootRate;
	short		m_nStealRate;
	short		m_nTackleRate;
	short		m_nPassRate;
	short		m_nMark;
};

//��� ���� Ŭ����
class CResult
{
public:
	short		m_nWin;
	short		m_nGoal;
	short		m_nAssist;
	short		m_nCut;
	short		m_nShoot;
	short		m_nSteal;
	short		m_nTackle;
	short		m_nPass;
	short		m_nTryShoot;
	short		m_nTrySteal;
	short		m_nTryTackle;
	short		m_nTryPass;
	short		m_nMark;
	short		m_nService;
	short		m_nPossession;
};
*/

//���� ��� ���� Ŭ����
class CEachResult
{
public:
	int			m_nPlayerSeq;
	char		m_nTeam;
	short		m_nLevel;
	char		m_nPosition;
	char		m_sName[PLAYER_NAME_SIZE];
	int			m_nExp;
	int			m_nPoint;
	char		m_nIsMvp;
	char		m_nIsExpItem;
	char		m_nIsPointItem;
	char		m_nIsGoldenTime;
	char		m_nIsNumber;
	char		m_nIsEvent;
	char		m_nIsLevelUp;
	float		m_fPilotTime;	// rebuild: present in binary CEachResult (offset 0x28)
	short		m_nPlayCount;	// rebuild: present in binary CEachResult (offset 0x2c)
	CResult		m_cPlayerResult;
};

//���� ������ ���� Ŭ����
class CReserveSeat
{
public:
	char		m_nReservePosition[TEAM_SIZE];		//���� ������
	char		m_nUsingPosition[TEAM_SIZE];		//�ڸ����� ������
	int			m_nPlayerSeq[TEAM_SIZE];			//�ڸ����� �÷��̾�
	char		m_nLevel[TEAM_SIZE];				// rebuild: binary CReserveSeat (offset 0x24)
	char		m_sName[TEAM_SIZE][PLAYER_NAME_SIZE];// rebuild: binary CReserveSeat (offset 0x2a) -> total 132 bytes
};


// Sale log record. Layout pinned to the binary (20 bytes): note m_nBuyKind was
// missing in the old header, and field order differs from the comment block.
class CSale
{
public:
	int			m_nObjectSeq;	// 0
	int			m_nObjectCode;	// 4
	char		m_nObjectKind;	// 8   enum OBJECT_KIND
	char		m_nBuyKind;		// 9   enum BUY_KIND
	char		m_nSaleKind;	// 10  enum SALE_KIND
	int			m_nPrice;		// 12
	int			m_nAmount;		// 16
};

// NOTE: the runtime inventory item `CItem` is SERVER-SPECIFIC and therefore not
// defined in this shared header: Certify's CItem is 36 bytes (certify/include/Item.h)
// while the Game's is a wider 80-byte object (game/include/GameItem.h). Only the
// on-the-WIRE item row, CItemInfo (Protocol.h), is shared by both servers.

//���� ���� ���� Ŭ����
class CGift
{
public:
	char		m_nKind;	//enum OBJECT_KIND
	int			m_nCode;    //�������ڵ�(����Ʈ�� �ִ°��� 0�̴�)
	int			m_nAmount;  //����
};

//������ ���� ����//////////////////////////////////////////////////////////////////
//������ ���� Ŭ����

//���� ���� ����////////////////////////////////////////////////////////////////
class CTeamSeat
{
public:
	char		m_nTeam;
	char		m_nSeat;
};


//////////////////////////////////////////////////////////////////////////////////
// �⺻ ��Ŷ
class CHeadPacket
{ public:
	int				m_nCommand;		// rebuild: non-const (matches binary; allows packet copy)
	int				m_nBodySize;
	int				m_nSequence;
	CHeadPacket() : m_nCommand(0), m_nBodySize(0), m_nSequence(0) {}
	CHeadPacket( int nCommand ) : m_nCommand(nCommand)
	{
		m_nBodySize = 0;
		m_nSequence = 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////
// ������ : ���Ÿ�ӵ�
class CScheduleData
{ public:
	char m_nTimeType;
	char m_nDate;
	int  m_nServerCode;
	int  m_nTimeSeq;
	int  m_nStart;
	int  m_nEnd;
};

////////////////////////////////////////////////////////////////////////////////////
// ���� ����ü
class CWeatherData
{ public:
	char m_nWeatherType;
	char m_nDate;
	int  m_nWeatherSeq;
	int  m_nStart;
	int  m_nEnd;
};

////////////////////////////////////////////////////////////////////////////////////
// ����� �α� ����ü
class CPlayLog
{ public:
	int	 m_nExp;
	int  m_nMatch;
	int  m_nPoint;
	int  m_nCash;
	int  m_nDate;

	CPlayLog() { m_nExp = 0;m_nMatch = 0; m_nPoint = 0; m_nCash = 0;  m_nCash = 0; m_nDate = 0; }
};



// String Array (CYG)
typedef std::map<int, char*> CStringArray;
// rebuild: CScheduleArray/CWeatherArray are redefined for Certify in Main.h as
// maps to CSchedule*/CEvent* (the Aug-2010 CLoad layout). The CScheduleData*/
// CWeatherData* forms below are unused, so they are dropped to avoid a
// conflicting-typedef clash with Main.h.
// typedef std::map<int, CScheduleData*> CScheduleArray;
// typedef std::map<int, CWeatherData*> CWeatherArray;

#pragma pack(pop)    // rebuild: close the pack(4) region; restore default packing
#endif
