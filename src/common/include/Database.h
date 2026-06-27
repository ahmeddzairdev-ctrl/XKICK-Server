///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DESC : �����ͺ��̽� ������ ���� �⺻ Ŭ����
// DATE : 2009.04
// AUTH : MUTTER(CYG)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _DATABASE_H_
#define _DATABASE_H_

#include "Include.h"
#include "Define.h"

#define MAINDBFLAG		0
#define LOGDBFLAG		1
#define SAMPLEDBFLAG	2
#define PCBANGDBFLAG	3

#define QUERY_SIZE		1024
#define QUERY_SIZE2		2048

#define DB_SIZE			6

class CTime;
typedef std::map<string, char *> MY_ROW; // ROW���� �ڷᱸ�� (CYG)
typedef std::map<string, int> MY_FIELD; // FIELD���� �ڷᱸ�� (CYG)
typedef std::map<int,string>  DB_LIST; // DB���� �ڷᱸ�� (CYG)

#define	FREE_RESULT( a ) {if( a != NULL ){	mysql_free_result(a);	a = NULL;} }


class CDatabase
{
private:
	MYSQL				m_hDatabase[DB_SIZE];
	int					m_nDBSel[DB_SIZE];
	bool				m_bUse[DB_SIZE];
	bool				m_bConn[DB_SIZE];
	bool				m_bInit;
	bool				m_bTrace;
	std::mutex			m_tMutex;	// was pthread_mutex_t (portable)
	string				m_strIP;
	string				m_strUser;
	string				m_strPass;
	string				m_strChar;

	int					InitDB(MYSQL *pSql, string strDBName ); // �ʱ�ȭ
public:
	CDatabase();
	~CDatabase();

	DB_LIST				m_hDBList;
	int					m_nBaseDBID;

	void				SetConnectInfo( string strIP, string strUser, string strPass, string strChar); // �⺻���� ����
	void				AddDB( int nDBID, string strDBName );	// DB �߰�
	void				SetBaseDB( int nDBID );				// �⺻DB ����
	MYSQL*				GetDB( const int nFlag );				// DB ����
	void				ReleaseDB(MYSQL * pMysql);				// DB ����
	void				CloseDB(MYSQL *pSql);					// DB ����
	int					PingDB(MYSQL *pSql, const char sDB[]  ); // ���°˻�
	int					SendQuery(MYSQL *pSql, char sQuery[], int nSize); // ���������� ���� �⺻�Լ�

	MYSQL_RES*			Query( const char * strQuery, ... );		// Query   : Select��, ����� ���ϵ� ���ҽ��� �����ؾ� �Ѵ�.
	MYSQL*				Execute( const char * strQuery, ... );		// Execute : �����, ���ҽ� ������ �ʿ������ ����� ��� ���� MYSQL�ڵ鷯�� �����Ѵ�.
	bool				FetchRow( MYSQL_RES* pRes, MY_ROW * pRow );// FetchRow: �ʵ������ �����ϱ� ���� ������� �Լ� / Query�� ������ �����Ѵ�.
	int 				GetDBTime( CTime * pTime );				// DB���� �ð� ��������

	void				TRACE_ENABLE();
	void				TRACE_DISABLE();


};


class CTime
{
public:
	int			m_nYear;	char		m_nQuarter;	char		m_nMonth;	char		m_nDay;
	char		m_nWeek;	char		m_nHour;	char		m_nMinute;	char		m_nSecond;
public:
	CTime()
	{
		time_t	t;
		time(&t);
		std::tm pTime = LocalTime(t);

		m_nYear		= pTime.tm_year + 1900;
		m_nMonth	= pTime.tm_mon + 1;
		m_nDay		= pTime.tm_mday;
		m_nWeek		= pTime.tm_wday;

		m_nHour		= pTime.tm_hour;
		m_nMinute	= pTime.tm_min;
		m_nSecond	= pTime.tm_sec;

		switch(m_nMonth)
		{
			case 1:  case 2:  case 3:	m_nQuarter = 1; break;
			case 4:  case 5:  case 6:	m_nQuarter = 2; break;
			case 7:  case 8:  case 9:	m_nQuarter = 3; break;
			case 10: case 11: case 12:	m_nQuarter = 4; break;
		}
	}
	~CTime() {}
};


#endif
