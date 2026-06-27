
#ifndef _LOAD_H_
#define  _LOAD_H_

#include "Global.h"

struct TAPLevelRec
{
	int Level;
	int TargetExp;
	int NeedExp;
	char Zone;
};

struct TAPItemRec
{
	int ItemCode;
	short ItemType;
	short Company;
	short Gender;
	short Limit;
	short Divest;
	char Add;
	char New;
	char Sell;
	char Exchange;
	char Default;
	int Cash[5];
	int Point[5];
	int Pack;
};

struct TAPPackOption
{
	short Limit;
	int SOptionCode;
	int EOptionCode;
	short Default;
};

struct TAPPackRec
{
	int PackCode;
	TAPPackOption PackOption[3];
};

struct TAPOptionRec
{
	int OptionCode;
	short Type;
	short Gap;
	short Value;
	short CLimit;
	short PLimit;
	int Cash[5];
	int Point[5];
};

struct TAPPositionRec
{
	short PositionCode;
	short Run;
	short Endurance;
	short Quick;
	short Keeping;
	short Dribble;
	short Steal;
	short Tackle;
	short Heading;
	short ShortShoot;
	short LongShoot;
	short Cross;
	short ShortPass;
	short LongPass;
	short Bodycheck;
	short Catch;
	short Punch;
	short Defense;
	short Auto[4];
	short Plus[3];
	short Minus[3];
};

struct TAPLearnRec
{
	short LearnCode;
	short Type;
	short Limit;
	short Value;
	char Sell;
	char Default;
	int Cash[5];
	int Point[5];
	int Pack;
};

struct TAPSkillRec
{
	int SkillCode;
	short Position;
	short Section;
	short Limit;
	char Sell;
	char Default;
	int Cash[5];
	int Point[5];
	int Pack;
};

struct TAPCeleRec
{
	short CeleCode;
	short Limit;
	char Sell;
	char Default;
	int Cash[5];
	int Point[5];
	int Pack;
};

struct TAPMissionRec
{
	short Index;
	short MissionCode;
	short Gift;
};

struct TAPTip
{
	int Kind;
	char Tip[TIP_SIZE];
};

class CQuestTable
{
public:
		int					m_nCode;							//퀘스트 코드
		char				m_nPosition;						//포지션 전용 퀘스트
//		char				m_nLevel;							//AI 레벨 수준
		char				m_nDiff;   			                // 차등분배 유무( 0, 1(차등) )
		char				m_nMainType;
		char				m_nType[3];
		char				m_nLimit;          			        //레벨제한
		char				m_nJoin;                   			//참가하는 인원수
		char				m_nRepeat;		     			    //반복여부
		int					m_nCondition;	           			//이 퀘스트가 수행하기 위한 전제조건(선행퀘스트)
		CGift				m_cGift[3];                			//선물
		char				m_sTitle[TITLE_NAME_SIZE];
};


class CAPLoad
{
	public:
		std::map<int, TAPLevelRec *>		m_LevelTable;
		std::map<int, TAPItemRec *>			m_ItemTable;
		std::map<int, TAPPackRec *>			m_PackTable;
		std::map<int, TAPOptionRec *>		m_OptionTable;
		std::map<int, TAPPositionRec *>		m_PositionTable;
		std::map<short, TAPLearnRec *>		m_LearnTable;
		std::map<short, TAPSkillRec *>		m_SkillTable;
		std::map<short, TAPCeleRec *>		m_CeleTable;
		std::map<short, TAPMissionRec *>	m_MissionTable;
		std::map<short, TAPMissionRec *>	m_MissionIndex;
		std::map<int, CQuestTable*>			m_cQuestTable;
		TAPTip								m_Tip[MAX_TIP];

	public:
		CAPLoad();
		~CAPLoad();
		void LoadLevelTable(void);
		void LoadItemTable(void);
		void LoadPackTable(void);
		void LoadOptionTable(void);
		void LoadPositionTable(void);
		void LoadQuestTable(void);
		void LoadLearnTable(void);
		void LoadSkillTable(void);
		void LoadCeleTable(void);
		void LoadMissionTable(void);
		TAPLevelRec *GetLevelTable(int code);
		TAPItemRec *GetItemTable(int code);
		TAPPackRec *GetPackTable(int code);
		TAPOptionRec *GetOptionTable(int code);
		TAPPositionRec *GetPositionTable(int code);
		TAPLearnRec *GetLearnTable(int code);
		TAPSkillRec *GetSkillTable(int code);
		TAPCeleRec *GetCeleTable(int code);
		TAPMissionRec *GetMissionTable(int code);
		TAPMissionRec *GetMissionIndex(int code);
		//void GetTip(CPlayer *pPly, char tip[]);
};


#endif
