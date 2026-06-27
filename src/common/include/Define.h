#ifndef _DEFINE_H_
#define _DEFINE_H_

// rebuild: the inline helpers below (ATOI/CheckLimit) use printf/atoi/NULL, so
// make this header self-sufficient instead of relying on include order.
#include <cstdio>
#include <cstdlib>
#include <cstddef>

#define SAFE_DELETE(p)       { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#define COUNTOF(_Array)		(sizeof(_Array) / sizeof(_Array[0]))


#define OK					0
#define NOK					-1
#define YES					1
#define NO					0

#define MAX_SERVER			50
#define MAX_PLAYER			500			//�ִ� �����
#define MAX_ROOM			150			//�ִ� ���
#define MAX_TEAM			150			//�ִ� ����

#define MAX_VIEWER			4
#define MAX_ATHLETE			10
#define MAX_ITEM			70
#define MAX_LEVEL			50 // 55->50 (CYG)
#define MAX_LEVEL_SECTION	(MAX_LEVEL/5)+1	// �������� ����
#define LEVEL_SECTION(x)	((x/5)-1)	// ���� ��� ��ũ��
#define MAX_TIP			50
#define MAX_MISSION		46 //����:46,�̺�Ʈ:84
#define MAX_HOLIDAY		50
#define MAX_CARDSKILL		3
#define MAX_CARDRANK		4
#define MAX_BUDDY			30
#define MAX_BLACKLIST		30 // �ִ� ��������Ʈ ī��Ʈ


#define EMBLEM_TYPE_GOLD	604101101
#define EMBLEM_TYPE_SILVER 604101102

#define MAX_PATH			255

#define GAME_PLAY_TIME			300

//���� ũ�� ������
#define ID_NAME_SIZE			(15 * 2)	+ 1
#define PLAYER_NAME_SIZE		(7 * 2)		+ 1
#define OBJECT_NAME_SIZE		(20 * 2)	+ 1
#define SERVER_NAME_SIZE		(15 * 2)	+ 1
#define TITLE_NAME_SIZE			(23 * 2)	+ 1
#define CLUB_NAME_SIZE			(10 * 2)	+ 1
#define PLAYER_MENT_SIZE		(22 * 2)	+ 1
#define MESSAGE_SIZE			(40 * 2)	+ 1
#define PASS_SIZE				(10 * 2)	+ 1 // �� ��й�ȣ ������
#define LOGIN_PASS_SIZE		(32 * 2)	+ 1 // �α��� ��й�ȣ ������
#define TIP_SIZE				(60 * 2)	+ 1
#define MISSTION_TEXT_SIZE		(60 * 2)	+ 1

#define MAX_MUSIC_COUNT			3


#define MAX_EQUIP				17			//�ִ� ������
#define MAX_FACULTY				130			//�ִ� �ɷ�ġ��
#define MAX_CHARACTER			3			//�ִ� ĳ���ͼ�
#define MAX_INVEN				80			//�ִ� �κ���
#define MAX_SKILL				50
#define MAX_CEREMONY			5
#define MAX_CARD				100			//�ִ� ī���κ�
#define MAX_SCHEDULE_LIST		10			// ������ ����Ʈ (���Ÿ��)
#define MAX_BUDDY_LIST			10


#define MAX_ITEM_LIST			10
#define MAX_ITEM_LIST			10
#define MAX_TRAINING_LIST		10
#define MAX_CEREMONEY_LIST		5
#define MAX_SKILL_LIST			50
#define MAX_QUEST_LIST			10
#define MAX_CARD_LIST			30

#define PACKET_SIZE				2048


#define HEAD_SIZE				(int)sizeof(CHeadPacket)
#define IP_SIZE					20
#define TEAM_SIZE				6

#define ITEM_OPTION_SIZE		5
#define OPTION_SIZE				ITEM_OPTION_SIZE	// rebuild: Protocol.h CSC*Item use OPTION_SIZE

#define SLOT_SIZE				4
#define BASE_CHARACTER_SIZE		1
#define BASE_INVEN_SIZE			20
#define BASE_SKILL_SIZE			8
#define ITEM1_MIX_SIZE				5
#define ITEM2_MIX_SIZE				5
#define CARD_MIX_SIZE				12



//����Ʈ ũ��
#define LIST5_SIZE				5
#define LIST6_SIZE				6
#define LIST8_SIZE				8
#define LIST10_SIZE			10

#define TEAM_SIZE				6

#define SHOP_BIT_EXIST			0
#define SHOP_BIT_NONE			1


//������ ����
#define ITEM_FACE				100	//��
#define ITEM_HAIR				101 //�Ӹ�
#define ITEM_SHIRTS			102 //����
#define ITEM_PANTS				103 //����
#define ITEM_GLOVE				104 //�尩
#define ITEM_SHOES				105 //�Ź�

#define ITEM_DIGIT				1000000				// ������ �ڸ��� (CYG)
#define ITEMTYPE(nCode)		(nCode/ITEM_DIGIT)  // Ÿ�Ծ�����
#define ITEMCATEGORY(nCode)	(ITEMTYPE(nCode)-(ITEMTYPE(nCode)%100))  // Ÿ�Ծ�����

#define FACULTYTYPE(nCode)		(nCode/100)*100    // Ÿ�Ծ�����


#define OPTION_DIGIT			100000				// �ɼ� �ڸ��� (CYG)
#define OPTIONTYPE(nCode)			(nCode/OPTION_DIGIT)  // Ÿ�Ծ�����



//������ ����
#define POSITION_NONE			0	//������ ����
#define POSITION_ALL			1	//��� ������

#define POSITION_FW				10
#define POSITION_ST				11	//��Ʈ����Ŀ
#define POSITION_CF				12	//����������
#define POSITION_WF				13	//��������
#define POSITION_SS				14	//�����콺Ʈ����Ŀ
#define POSITION_FW_RANGE			4

#define POSITION_MF				20	//�̵�
#define POSITION_AM				21	//�������̵�
#define POSITION_CM				22	//�߾ӹ̵�
#define POSITION_SM				23	//���̵�̵�
#define POSITION_DM				24	//�������̵�
#define POSITION_MF_RANGE			4


#define POSITION_DF				30	//����
#define POSITION_SW				31	//������
#define POSITION_CB				32	//�߾ӹ�
#define POSITION_SB				33	//������
#define POSITION_DF_RANGE			3


#define POSITION_GK				40
#define POSITION_BG				50  // ����

//�����Ǻ� �ڵ�����
#define AUTO_FACULTY_LEVEL1		10  // 10���� 1������
#define AUTO_FACULTY_LEVEL2		20  // 20���� 2������

#define MAX_AI_COSTUME				61


#define PC_EXIT01			1			//read�� ���� ����
#define PC_EXIT02			2			//write�� ���� ����
#define PC_EXIT03			3			//nread ������
#define PC_EXIT04			4			//Ping�� ���� ������
#define PC_EXIT11			11			//TCP �������� ���Ἲ(Header) ������
#define PC_EXIT12			12			//TCP �������� ���Ἲ(Command) ������
#define PC_EXIT20			20			//UDP ������ ����
#define PC_EXIT21			21			//UDP �������� ���Ἲ(Header) ������
#define PC_EXIT22			22			//UDP �������� ���Ἲ(Command) ������
#define PC_EXIT30			30			//���� ������ ����� ���� ����
#define PC_EXIT31			31			//������� ���� ���ῡ ���� ����
#define PC_EXIT32			32			//������� �泪���⿡ ���� ����
#define PC_EXIT33			33			//���� ���α׷� �̻� �������� ���� ����
#define PC_EXIT90			90			//���� ���� �̻� �������� ���� ��ü ����� ����

#define ROOT2				1.414
#define PROTOCOL_MAGIC		(('O'<<24)+('G'<<16)+('R'<<8)+'E')

//���� ��ü
#define ENTERPLAY				100
#define AAA						200
#define BBB						300

///////////////////////////////////////////////////////////////////
// Player ����
enum PLAYER_STATE
{
	PLAYER_STATE_EMPTY		= 0,
	PLAYER_STATE_USE
};
enum SERVER_STATE
{
	SERVER_STATE_EMPTY		= 0,
	SERVER_STATE_CHECKUP,
	SERVER_STATE_RUN
};

/*
enum PLAYER_MODE
{
	PLAYER_MODE_NONE		= 0,
	PLAYER_MODE_READY,
	PLAYER_MODE_COUNTDOWN,
	PLAYER_MODE_LOADING,
	PLAYER_MODE_GAME,
	PLAYER_MODE_RESULT
};
*/

enum PLAYER_POWER
{
	PLAYER_POWER_NORMAL  	= 1,
	PLAYER_POWER_POWERUSER,
	PLAYER_POWER_MANAGER,
	PLAYER_POWER_ADMIN
};

enum PLAYER_KIND
{
	PLAYER_KIND_USER		= 0,
	PLAYER_KIND_ROBOT,
	PLAYER_KIND_TUTORIALROBOT,
	PLAYER_KIND_USERBOT
};

enum PLAYER_CONDITION
{
	PLAYER_CONDITION_NONE	= 0,
	PLAYER_CONDITION_ENTRANCE,		//�Թ��� ����
	PLAYER_CONDITION_AWKWARD,		//�̼��� ����
	PLAYER_CONDITION_ADAPTATION,	//������ ����
	PLAYER_CONDITION_EXPERT,		//���õ� ����
	PLAYER_CONDITION_VETERAN,		//����� ����
	PLAYER_CONDITION_PERFECTION,	//�Ϻ��� ����
};

enum PLAYER_ALIAS
{
	PLAYER_ALIAS_NONE		= 0,
	PLAYER_ALIAS_LIBERO,
};

enum PLAYER_TEAM
{
	PLAYER_TEAM_NONE		= 0,
	PLAYER_TEAM_HOME,
	PLAYER_TEAM_AWAY,
	PLAYER_TEAM_VIEW,
	PLAYER_TEAM_STAY,
	PLAYER_TEAM_ALL
};

enum PLAYER_GENDER
{
	PLAYER_GENDER_NONE		= 0,
	PLAYER_GENDER_MAN,
	PLAYER_GENDER_WOMAN,
	PLAYER_GENDER_COUNT,
};

enum PLAYER_UNIFORM
{
	PLAYER_UNIFORM_NONE		= 0,
	PLAYER_UNIFORM_HOME,
	PLAYER_UNIFORM_AWAY
};

enum PLAYER_RELAY
{
	PLAYER_RELAY_NONE		= 0,
	PLAYER_RELAY_USE
};

enum PLAYER_EXIT
{
	PLAYER_EXIT_END			= 0,
	PLAYER_EXIT_TRANSPORT,
	PLAYER_EXIT_DRAWFORCE
};

enum PLAYER_READY
{
	PLAYER_READY_QUESTION	= 0,
	PLAYER_READY_REQUESTION,
	PLAYER_READY_NO,
	PLAYER_READY_YES,
	PLAYER_READY_COMPLETE,
	PLAYER_READY_CANCEL
};

enum PLAYER_FACULTY
{
	PLAYER_FACULTY_RUN		= 10,
	PLAYER_FACULTY_DRIBBLE,
	PLAYER_FACULTY_QUICK,
	PLAYER_FACULTY_STAMINA,
	PLAYER_FACULTY_ELASTICITY,
	PLAYER_FACULTY_BODYCHECK,
	PLAYER_FACULTY_CROSS,
	PLAYER_FACULTY_SHORTPASS,
	PLAYER_FACULTY_LONGPASS,
	PLAYER_FACULTY_HEADSHOOT,
	PLAYER_FACULTY_SHORTSHOOT,
	PLAYER_FACULTY_LONGSHOOT,
	PLAYER_FACULTY_KEEPING,
	PLAYER_FACULTY_STEAL,
	PLAYER_FACULTY_TACKLE,
	PLAYER_FACULTY_CATCH,
	PLAYER_FACULTY_PUNCH,
	PLAYER_FACULTY_GUARD,
	PLAYER_FACULTY_MOVE,
	PLAYER_FACULTY_BODY,
	PLAYER_FACULTY_PASS,
	PLAYER_FACULTY_SHOOT,
	PLAYER_FACULTY_DEFENSE,
	PLAYER_FACULTY_GOALKEEP,
	PLAYER_FACULTY_TALENT
};

enum ARRAY_FACULTY
{
	ARRAY_FACULTY_RUN		= 0,
	ARRAY_FACULTY_DRIBBLE,
	ARRAY_FACULTY_QUICK,
	ARRAY_FACULTY_STAMINA,
	ARRAY_FACULTY_ELASTICITY,
	ARRAY_FACULTY_BODYCHECK,
	ARRAY_FACULTY_CROSS,
	ARRAY_FACULTY_SHORTPASS,
	ARRAY_FACULTY_LONGPASS,
	ARRAY_FACULTY_HEADSHOOT,
	ARRAY_FACULTY_SHORTSHOOT,
	ARRAY_FACULTY_LONGSHOOT,
	ARRAY_FACULTY_KEEPING,
	ARRAY_FACULTY_STEAL,
	ARRAY_FACULTY_TACKLE,
	ARRAY_FACULTY_CATCH,
	ARRAY_FACULTY_PUNCH,
	ARRAY_FACULTY_GUARD,
	ARRAY_FACULTY_MOVE,
	ARRAY_FACULTY_BODY,
	ARRAY_FACULTY_PASS,
	ARRAY_FACULTY_SHOOT,
	ARRAY_FACULTY_DEFENSE,
	ARRAY_FACULTY_GOALKEEP,
	ARRAY_FACULTY_TALENT,
	ARRAY_FACULTY_SIZE
};

enum FOOD_FACULTY
{
	FOOD_FACULTY_TYPE		= 300,
	FOOD_FACULTY_EXP,
	FOOD_FACULTY_POINT
};

#define ITEM_TYPE_FACULTY(A) 			((A-(A/100000)*100000)/100)
#define ITEM_TYPE_SITUATION(A) 		(A/100000)
#define ITEM_TYPE_VALUE(A)				(A-(A/100)*100)
#define MAX_ITEM_FACULTY_SIZE 			16*5+5
#define ITEM_FACULTY_REST_SIZE( A ) 	((MAX_ITEM_FACULTY_SIZE-A.m_nFacultyCnt)*sizeof(unsigned int))

#define COPY_ITEM_OPTION( A, B ) 		\
		{\
			for( int MACRO_OPTION_LOOP = 0 ; MACRO_OPTION_LOOP < ITEM_OPTION_SIZE; MACRO_OPTION_LOOP++ ) \
			{\
				A[MACRO_OPTION_LOOP] = B[MACRO_OPTION_LOOP];\
			}\
		}


const int ARRAY_FACULTY_BASE_SIZE  = ARRAY_FACULTY_MOVE;

enum ARRAY_RESULT
{
	ARRAY_RESULT_WIN		= 0,
	ARRAY_RESULT_GOAL,
	ARRAY_RESULT_ALLOW,
	ARRAY_RESULT_ASSIST,
	ARRAY_RESULT_CUT,
	ARRAY_RESULT_SHOOT,
	ARRAY_RESULT_STEAL,
	ARRAY_RESULT_TACKLE,
	ARRAY_RESULT_PASS,
	ARRAY_RESULT_GUARD,
	ARRAY_RESULT_GOOD,
	ARRAY_RESULT_POSSESSION,
	ARRAY_RESULT_TRYSHOOT,
	ARRAY_RESULT_TRYSTEAL,
	ARRAY_RESULT_TRYTACKLE,
	ARRAY_RESULT_TRYPASS,
	ARRAY_RESULT_TRYGUARD,
	ARRAY_RESULT_MARK,
	ARRAY_RESULT_SIZE
};

enum ARRAY_RECORD
{
	ARRAY_RECORD_MATCH		= 0,
	ARRAY_RECORD_WIN,
	ARRAY_RECORD_DRAW,
	ARRAY_RECORD_MARK,
	ARRAY_RECORD_MVP,
	ARRAY_RECORD_GOAL,
	ARRAY_RECORD_ALLOW,
	ARRAY_RECORD_ASSIST,
	ARRAY_RECORD_CUT,
	ARRAY_RECORD_SHOOT,
	ARRAY_RECORD_STEAL,
	ARRAY_RECORD_TACKLE,
	ARRAY_RECORD_PASS,
	ARRAY_RECORD_GUARD,
	ARRAY_RECORD_TRYSHOOT,
	ARRAY_RECORD_TRYSTEAL,
	ARRAY_RECORD_TRYTACKLE,
	ARRAY_RECORD_TRYPASS,
	ARRAY_RECORD_TRYGUARD,
	ARRAY_RECORD_SIZE
};

enum ARRAY_RANKING
{
	ARRAY_RANKING_MATCH		= 0,
	ARRAY_RANKING_WIN,
	ARRAY_RANKING_WINPOINT,
	ARRAY_RANKING_MARK,
	ARRAY_RANKING_MVP,
	ARRAY_RANKING_GOAL,
	ARRAY_RANKING_ALLOW,
	ARRAY_RANKING_ASSIST,
	ARRAY_RANKING_CUT,
	ARRAY_RANKING_SHOOT,
	ARRAY_RANKING_STEAL,
	ARRAY_RANKING_TACKLE,
	ARRAY_RANKING_PASS,
	ARRAY_RANKING_GUARD,
	ARRAY_RANKING_MARKAVERAGE,
	ARRAY_RANKING_GOALAVERAGE,
	ARRAY_RANKING_ALLOWAVERAGE,
	ARRAY_RANKING_ASSISTAVERAGE,
	ARRAY_RANKING_CUTAVERAGE,
	ARRAY_RANKING_SHOOTAVERAGE,
	ARRAY_RANKING_STEALAVERAGE,
	ARRAY_RANKING_TACKLEAVERAGE,
	ARRAY_RANKING_PASSAVERAGE,
	ARRAY_RANKING_GUARDAVERAGE,
	ARRAY_RANKING_WINRATE,
	ARRAY_RANKING_SHOOTRATE,
	ARRAY_RANKING_STEALRATE,
	ARRAY_RANKING_TACKLERATE,
	ARRAY_RANKING_PASSRATE,
	ARRAY_RANKING_GUARDRATE,
	ARRAY_RANKING_SIZE
};

//////////////////////////////////////////////////////////////////////////
// BUDDY
// ģ�� ��������
enum BUDDY_STATE
{
	BUDDY_STATE_OFFLINE	= 0,
	BUDDY_STATE_LOBBY,
	BUDDY_STATE_ROOM,
	BUDDY_STATE_PLAY
};


// ������ ��ȭ ����
enum ENCHANT_TYPE
{
	ENCHANT_TYPE_NORMAL		= 0,	// �Ϲ�
	ENCHANT_TYPE_RARE,				// ���ȭ
	ENCHANT_TYPE_SPECIAL,
	ENCHANT_TYPE_COUNT
};



//////////////////////////////////////////////////////////////////////////
// ITEM
enum ITEM_TYPE
{
	ITEM_TYPE_CLASS			= 0,	//�з�
	ITEM_TYPE_ALL,					//��ü
	ITEM_TYPE_NEW,					//�Ż�
	ITEM_TYPE_HIT,					//��Ʈ
	ITEM_TYPE_INTEREST,				//��
	ITEM_TYPE_PARCEL,				//����

	ITEM_TYPE_COSMETIC		= 100,	//�̿�
	ITEM_TYPE_FACE,					//��
	ITEM_TYPE_HAIR,					//�Ӹ�(�����, ���)
	ITEM_TYPE_TATTOO,				//����(�ٷ�, ź��, ���ο�)

	ITEM_TYPE_CLOTH			= 200,	//�ǻ�
	ITEM_TYPE_SHIRTS,				//����(�ܰɽ�, ĳĪ)
	ITEM_TYPE_PANTS,				//����(�߰ɽ�, ��Ī)
	ITEM_TYPE_GLOVE,				//�尩(�ٷ�, ź��, ���ο�)
	ITEM_TYPE_SHOES,				//�Ź�(�޸���, �帮��, ���߷�)
	ITEM_TYPE_SOCKS,				//�縻(�޸���, �帮��, ���߷�)

	ITEM_TYPE_ACCESSORY		= 300,	//�Ǽ�����
	ITEM_TYPE_EYE,					//�Ȱ�(ũ�ν�, ���н�, ���н�)
	ITEM_TYPE_EAR,					//�Ͱ���(ũ�ν�, ���н�, ���н�)
	ITEM_TYPE_NECK,					//�����(ũ�ν�, ���н�, ���н�)

	ITEM_TYPE_MASK,					//����ũ(�̺�Ʈ��)
	ITEM_TYPE_MUFFLER,				//���÷�(�̺�Ʈ��)
	ITEM_TYPE_BAG,					//����(�̺�Ʈ��)

	ITEM_TYPE_PROTECT		= 400,	//��ȣ��
	ITEM_TYPE_WRIST,				//�ո�ȣ��(Ű��, ��ƿ, ��Ŭ)
	ITEM_TYPE_ARM,					//�ȸ�ȣ��(Ű��, ��ƿ, ��Ŭ)
	ITEM_TYPE_KNEE,					//������ȣ��(Ű��, ��ƿ, ��Ŭ)

	ITEM_TYPE_FOOD			= 500,	//����
	ITEM_TYPE_DRINK,				//�����
	ITEM_TYPE_RUN			= 510,  //�ɷ�ġ ���
	ITEM_TYPE_DRIBBLE,
	ITEM_TYPE_QUICK,
	ITEM_TYPE_STAMINA,
	ITEM_TYPE_ELASTICITY,
	ITEM_TYPE_BODYCHECK,
	ITEM_TYPE_CROSS,
	ITEM_TYPE_SHORTPASS,
	ITEM_TYPE_LONGPASS,
	ITEM_TYPE_HEADSHOOT,
	ITEM_TYPE_SHORTSHOOT,
	ITEM_TYPE_LONGSHOOT,
	ITEM_TYPE_KEEPING,
	ITEM_TYPE_STEAL,
	ITEM_TYPE_TACKLE,
	ITEM_TYPE_CATCH,
	ITEM_TYPE_PUNCH,
	ITEM_TYPE_GUARD,
	ITEM_TYPE_MOVE,
	ITEM_TYPE_BODY,
	ITEM_TYPE_PASS,
	ITEM_TYPE_SHOOT,
	ITEM_TYPE_DEFENSE,
	ITEM_TYPE_GOALKEEP,
	ITEM_TYPE_TALENT,

	ITEM_TYPE_SPECIAL		= 600,	//Ư��
	ITEM_TYPE_SLOT,
	ITEM_TYPE_RESET,
	ITEM_TYPE_UPGRADE,
	ITEM_TYPE_EMBLEM,

	ITEM_TYPE_CLUB			= 700,	//Ŭ��
	ITEM_TYPE_UNIFORM,
	ITEM_TYPE_NUMBER
};

enum ITEM_EQUIP
{
	ITEM_EQUIP_FACE			= 0,//��� ����
	ITEM_EQUIP_HAIR,
	ITEM_EQUIP_TATTOO,
	ITEM_EQUIP_SHIRTS,
	ITEM_EQUIP_PANTS,
	ITEM_EQUIP_GLOVE,
	ITEM_EQUIP_SHOES,
	ITEM_EQUIP_SOCKS,
	ITEM_EQUIP_EYE,
	ITEM_EQUIP_EAR,
	ITEM_EQUIP_NECK,
	ITEM_EQUIP_MASK,
	ITEM_EQUIP_MUFFLER,
	ITEM_EQUIP_BAG,
	ITEM_EQUIP_WRIST,
	ITEM_EQUIP_ARM,
	ITEM_EQUIP_KNEE,
	ITEM_EQUIP_SIZE
};


#define CARD_BOOSTER_TYPE_ALL 		 10
#define CARD_BOOSTER_TYPE_POSITION  20
#define CARD_BOOSTER_TYPE_TYPE		 30

// ī�� �ͽ�
enum CARD_MIX
{
	CARD_MIX_POSITION		= 0,
	CARD_MIX_RANK,
	CARD_MIX_LEVEL,
	CARD_MIX_12CARD,
};

// Ʈ���̴� ���� (CYG_
enum TRAINING_KIND
{
	TRAINING_BASE		= 0, // �⺻ Ʈ���̴�
	TRAINING_EXTRA			 // Ȯ�� Ʈ���̴� (����, �߰������� Ȱ���)
};
///////////////////////////////////////////////////////////////////
// Room ����
enum ROOM_STATE
{
	ROOM_STATE_EMPTY		= 0,
	ROOM_STATE_NORMAL,
	ROOM_STATE_SECRET
};

enum ROOM_MODE
{
	ROOM_MODE_NORMAL		= 0,
	ROOM_MODE_BET,
	ROOM_MODE_TRAINING,
	ROOM_MODE_LADDER,
	ROOM_MODE_QUEST,
	ROOM_MODE_TOURNAMENT,
	ROOM_MODE_REPLAY,
	ROOM_MODE_SINGLE

};

enum ROOM_COURCE
{
	ROOM_COURCE_NONE		= 0,
	ROOM_COURCE_READY,
	ROOM_COURCE_COUNT,
	ROOM_COURCE_LOAD,
	ROOM_COURCE_PLAY,
	ROOM_COURCE_RESULT
};

enum ROOM_KIND
{
	ROOM_KIND_NORMAL		= 0,
	ROOM_KIND_TRAINING,
	ROOM_KIND_MISSION,
	ROOM_KIND_TUTORIAL
};

enum ROOM_LEAVE
{
	ROOM_LEAVE_NONE			= 0,
	ROOM_LEAVE_SELF,
	ROOM_LEAVE_FORCE,
	ROOM_LEAVE_DISCONNECT
};

enum ROOM_TIME
{
        ROOM_TIME_AUTO             = 0,
	ROOM_TIME_SUNRISE,
	ROOM_TIME_DAYLIGHT,
	ROOM_TIME_SUNSET,
	ROOM_TIME_NIGHT
};

enum ROOM_WEATHER
{
        ROOM_WEATHER_AUTO         = 0,
	ROOM_WEATHER_FINE,
	ROOM_WEATHER_CLOUDY,
	ROOM_WEATHER_RAIN,
	ROOM_WEATHER_SNOW
};

enum ROOM_ATTACK
{
	ROOM_ATTACK_ALTERNATE	= 0,
	ROOM_ATTACK_RANDOM,
	ROOM_ATTACK_HANDICAP,
	ROOM_ATTACK_HOME,
	ROOM_ATTACK_AWAY
};

enum ROOM_SCALE
{
	ROOM_SCALE_ONE			= 1,
	ROOM_SCALE_TWO,
	ROOM_SCALE_THREE,
	ROOM_SCALE_FOUR,
	ROOM_SCALE_FIVE,
	ROOM_SCALE_SIX
};

enum ROOM_AI
{
	ROOM_AI_NONE			= 0,
	ROOM_AI_ALL,
	ROOM_AI_KEEPER
};

enum ROOM_POINT
{
        ROOM_POINT_NONE                 = 0,
        ROOM_POINT_300                  = 300,
        ROOM_POINT_500                  = 500,
        ROOM_POINT_1000                 = 1000,
        ROOM_POINT_1500                 = 1500,
        ROOM_POINT_2000                 = 2000
};

enum ROOM_PIT
{
		ROOM_PIT_OUT					= 0,
		ROOM_PIT_IN
};
///////////////////////////////////////////////////////////////////
// SKILL
enum SKILL_TYPE
{
	SKILL_TYPE_MYSKILL		= 0,	//���ǽ�ų
	SKILL_TYPE_OTHERSKILL			//��뽺ų
};
///////////////////////////////////////////////////////////////////
// ETC
enum OBJECT_STATE
{
	OBJECT_STATE_EMPTY		= 0,
	OBJECT_STATE_NORMAL,
	OBJECT_STATE_CHANGE,
	OBJECT_STATE_TEMPORARY
};

enum LOBBY_STATE
{
	LOBBY_STATE_EMPTY		= 0,
	LOBBY_STATE_NORMAL
};

enum LIST_KIND
{
	LIST_KIND_ALL			= 0,
	LIST_KIND_NORMAL,
	LIST_KIND_WAIT,
	LIST_KIND_QUEST,
	LIST_KIND_LADDER
};

enum CHAT_KIND
{
	CHAT_KIND_NORMAL		= 0,	//�Ϲ� ä��
	CHAT_KIND_PLAY,					//������ ä��
	CHAT_KIND_TEAM,					//�� ä��
	CHAT_KIND_SECRET,				//��� ä��(������ ���)
	CHAT_KIND_SECRET2,				//��� ä��(�޴� ���)
	CHAT_KIND_CLUB,					//Ŭ�� ä��
	CHAT_KIND_ANNOUNCE,				//�˸� �޼���
	CHAT_KIND_NOTICE,				//��������
	CHAT_KIND_OPERATOR,				//��� ä��(�Ϲ� ä�ð� ������ ���� �ٸ���)
	CHAT_KIND_WHOLE					//��ü ä��
};

enum EQUIP_KIND
{
	EQUIP_KIND_NO			= 0,
	EQUIP_KIND_YES
};

enum BUY_KIND
{
	BUY_KIND_NONE			= 0,
	BUY_KIND_CASH,
	BUY_KIND_POINT,
	BUY_KIND_BOTH,
	BUY_KIND_CLUBPOINT,
	BUY_KIND_QUEST,
	BUY_KIND_MISSION,
	BUY_KIND_GIFT,
	BUY_KIND_ENCHANT
};

enum OBJECT_KIND
{
	OBJECT_KIND_NONE		= 0,
	OBJECT_KIND_ITEM,		// 1
	OBJECT_KIND_SKILL,		// 2
	OBJECT_KIND_CEREMONY,	// 3
	OBJECT_KIND_TRAINING,	// 4
	OBJECT_KIND_EXCHANGE,	// 5
	OBJECT_KIND_CASH,		// 6
	OBJECT_KIND_POINT,		// 7
	OBJECT_KIND_EXP,		// 8
	OBJECT_KIND_SKILLBONUS,	// 9
	OBJECT_KIND_GIFT,		// 10
	OBJECT_KIND_ENCHANT,	// 11
	OBJECT_KIND_SPECIAL 	// 12

};

enum CARD_ENTRY
{
	CARD_ENTRY_1 	  = 0,
	CARD_ENTRY_2 ,
	CARD_ENTRY_3 ,
	CARD_ENTRY_GET
};
enum UPDATE_KIND
{
	UPDATE_KIND_NONE		= 0,
	UPDATE_KIND_CREATE,
	UPDATE_KIND_DELETE,
	UPDATE_KIND_CHANGE
};

enum WIN_TYPE
{
	WIN_TYPE_NOGAME			= 0,
	WIN_TYPE_WIN,
	WIN_TYPE_DRAW,
	WIN_TYPE_LOSE
};

#define ARRAY_SKILL_KEY_SIZE 5

enum ARRAY_KEY
{
	ARRAY_KEY_UP			= 0,
	ARRAY_KEY_DOWN,
	ARRAY_KEY_LEFT,
	ARRAY_KEY_RIGHT,
	ARRAY_KEY_LEFTSHOOT,
	ARRAY_KEY_RIGHTSHOOT,
	ARRAY_KEY_LONGPASS,
	ARRAY_KEY_SHORTPASS,
	ARRAY_KEY_SCREEN,
	ARRAY_KEY_TACKLE,
	ARRAY_KEY_STEAL,
	ARRAY_KEY_QUICK,
	ARRAY_KEY_QUICK2,
	ARRAY_KEY_SKILL1,
	ARRAY_KEY_SKILL2,
	ARRAY_KEY_SKILL3,
	ARRAY_KEY_SKILL4,
	ARRAY_KEY_SKILL5,
	ARRAY_KEY_SIZE
};


enum SALE_KIND
{
	SALE_KIND_NONE				= 0,
	SALE_KIND_CASH,
	SALE_KIND_POINT,
	SALE_KIND_CASHPOINT,
	SALE_KIND_CLUBPOINT,
	SALE_KIND_QUEST_REWARD,
	SALE_KIND_SPECIAL_REWARD
};

enum DAY_KIND
{
	DAY_ALL				= 1,
	DAY_MON,
	DAY_TUE,
	DAY_WED,
	DAY_THU,
	DAY_FRI,
	DAY_SAT,
	DAY_SUN
};

enum TIME_KIND
{
	TIME_KIND_GOLDEN			= 1,
	TIME_KIND_TOURNAMENT,
	TIME_KIND_CLUB
};


// ��ŷ���� (CYG)
enum HACKUSER_REASON
{
	HACKUSER_REASON_HIJACKING = 1,  // ��Ŷ ������ŷ
	HACKUSER_REASON_INVALID,		// ��ȿ���� ���� �� ����
	HACKUSER_REASON_CLIENTREQ		// Ŭ���̾�Ʈ�� ã�� ����
};



// ���� ��ũ�� (CYG)
const char STR_OK[] = "\x1b[32m[OK]\n\x1b[0m";
const char STR_FAIL[] = "\x1b[31m[FAIL]\n\x1b[0m";

// atoi NULL �˻� (CYG)
inline int ATOI(char const* strText)
{
	if( strText == NULL )
	{
		printf( "\nERROR : NULL Found for calling atoi()!!\n" );
		return 0;
	}
	return atoi(strText);
}


// WHILE Limit ��ũ�� (CYG)
#define UNIQUE_VAR(x) safety_limit ## x
inline bool CheckLimit( bool a, const char * b, const char * c, const int d ) { if(!a) printf("\x1b[31m[ASSERT] : %s - \t%s(%d) \x1b[0m", b, c, d); return a; }
#define WHILE(a,b) \
	             int UNIQUE_VAR(__LINE__) = b; \
		          while(a && CheckLimit(--UNIQUE_VAR(__LINE__)>=0, __FILE__, __FUNCTION__, __LINE__))

//ū ������尡 �߻����� ������, ����������� ���� ���� ȿ���� ���ϴ� ��� �Ʒ��� ��ũ�η� ��ü�� ��.
//#define WHILE(a,b)  while(a)

#define	MAX_REWARD	1000
#define REWARD_OVER_CHECK(a) if( a > MAX_REWARD ) { a = 0;
#endif


// �����׽�Ʈ ��ũ�� (CYG)
#define INIT_TIMER() struct timeval MEASURE_TIMER; long MEASURE_TIME1, MEASURE_TIME2;
#define START_TIMER() 	gettimeofday(&MEASURE_TIMER, NULL); MEASURE_TIME1 = MEASURE_TIMER.tv_sec * 1000 + MEASURE_TIMER.tv_usec / 1000; // ����
#define END_TIMER() 		gettimeofday(&MEASURE_TIMER, NULL); MEASURE_TIME2 = MEASURE_TIMER.tv_sec * 1000 + MEASURE_TIMER.tv_usec / 1000; printf( "MEASURE TIME : %ldmsec \n", MEASURE_TIME2-MEASURE_TIME1 );

