
#include "Database.h"
#include "LogManager.h"

CDatabase::CDatabase()
{
	m_bInit		= false;
	m_nBaseDBID = 0;

	memset(m_hDatabase, 0x00, sizeof(MYSQL) * DB_SIZE);
	for(int i=0;i<DB_SIZE;++i)
	{
		m_nDBSel[i] = 0;
		m_bUse[i] = false;
		m_bConn[i] = false;
	}
	m_bTrace = false;
}


CDatabase::~CDatabase() {}

void CDatabase::SetConnectInfo( string strIP, string strUser, string strPass, string strChar = ""  )
{
	m_strIP  = strIP;
	m_strUser= strUser;
	m_strPass= strPass;
	m_strChar= strChar;
}

void CDatabase::AddDB( int nDBID, string strDBName )
{
	m_hDBList[nDBID] = strDBName;
}

void CDatabase::SetBaseDB( int nDBID )
{
	m_nBaseDBID = nDBID;
}

int CDatabase::InitDB(MYSQL *pSql, string strDBName )
{
	char sQuery[QUERY_SIZE];
	mysql_init(pSql);
    uint8_t verify = 0; 
    // Disable server certificate verification
    mysql_optionsv(pSql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verify);

	if(!mysql_real_connect(pSql, m_strIP.c_str(), m_strUser.c_str(), m_strPass.c_str(), strDBName.c_str(), 0, (char *)NULL, 0))
	{
		LOGOUT_DATABASE("WARNING: Happened Mysql connect error(error:%s).\n", mysql_error(pSql));
		return -1;
	}

	if( m_strChar[0] != '\0')
	{
		snprintf(sQuery, QUERY_SIZE, "set names %s", m_strChar.c_str());
		if(mysql_real_query(pSql, sQuery, strlen(sQuery)) != 0) return -1;
	}
	m_bInit  = true;
	return OK;
}

MYSQL* CDatabase::GetDB(const int nFlag)
{
	MYSQL*			pMysql = NULL;

	string strDBName;
	strDBName = m_hDBList[nFlag];

	m_tMutex.lock();

	int i = 3;
	if(!m_bUse[nFlag])
	{
		//�ֿ� ������ ������
		pMysql = &m_hDatabase[nFlag];
		if(!m_bConn[nFlag])
		{
			//�񿬰���
			if(InitDB(pMysql, strDBName.c_str()) != OK){
	        	m_tMutex.unlock();
				return NULL;
			}
	        m_bConn[nFlag] = true;
	    }
		m_nDBSel[nFlag] = nFlag;
	    m_bUse[nFlag] = true;
	}
	else
	{
		//�ֿ� ������ �����
		for(;i<DB_SIZE;i++)
		{
	    	if(!m_bUse[i])
			{
				//���� ������ ������
				pMysql = &m_hDatabase[i];
				if(InitDB(pMysql, strDBName.c_str()) != OK){
		            m_tMutex.unlock();
					return NULL;
				}
		        m_bConn[i] = true;

				//printf("GetDB(i:%d)\n", i);

				m_nDBSel[i] = nFlag;
		        m_bUse[i] = true;
		        break;
		    }
		}
	}

	m_tMutex.unlock();

	if(i >= DB_SIZE){
		LOGOUT_DATABASE("Mysql Connect Fail!!(i:%d)\n", i);
		exit(1);
	    //return NULL;
	}

	return pMysql;
}
void CDatabase::ReleaseDB(MYSQL * pMysql)
{
	if(pMysql == NULL) return;
	m_tMutex.lock();

	for(int i=0;i<DB_SIZE;i++)
	{
		if( (pMysql == &m_hDatabase[i]) && (m_bUse[i] == true) )
		{
			m_bUse[i] = false;

			//0~2°�� �̿ܿ��� Disconnect�Ѵ�.
			if(i > 2)
			{
				if(m_bConn[i])
				{
					CloseDB(pMysql);
					m_bConn[i] = false;
					break;
				}
			}
		}
	}
	pMysql = NULL;

	m_tMutex.unlock();
}

void CDatabase::CloseDB(MYSQL *pSql)
{
	if( pSql != NULL ) mysql_close(pSql);
}

int CDatabase::PingDB(MYSQL *pSql, const char sDB[] )
{
	if(pSql == NULL) return -1;
	if(mysql_ping(pSql))
	{
		CloseDB(pSql);
		if(InitDB(pSql, sDB) != OK)
		{
			ReleaseDB(pSql);
			return -1;
		}
	}
	return OK;
}

int CDatabase::SendQuery(MYSQL *pSql, char sQuery[], int nSize)
{
	if(pSql == NULL) return -1;
	int check = mysql_real_query(pSql, sQuery, nSize);
	if(check != 0)
	{
		ReleaseDB(pSql);
		return check;
	}
	return OK;
}


// SendQuery�Լ��� �ܼ�ȭ��Ų Wrapper - SELECT��(CYG)
MYSQL_RES * CDatabase::Query( const char * strQuery, ... )
{
	MYSQL	 *	tMainSql;
	MYSQL_RES*	res;
	tMainSql = GetDB(m_nBaseDBID);		// Query�Լ��� �⺻ DB�� �����Ѵ�.
	if( tMainSql == NULL ) return NULL;
	char strBuf[QUERY_SIZE2];
	memset(strBuf, 0x00, QUERY_SIZE2);
	va_list args;	va_start(args, strQuery);
	vsnprintf( strBuf,QUERY_SIZE2, strQuery, args );
	va_end(args);

	if( m_bTrace ) printf( "QUERY : %s\n", strBuf );
	if(PingDB(tMainSql, m_hDBList[m_nBaseDBID].c_str()) < 0) {  LOGOUT_DATABASE( "QUERY ERROR: %s\n", strBuf ); return NULL; }
	if(SendQuery(tMainSql, strBuf, strlen(strBuf)) != 0) { LOGOUT_DATABASE( "QUERY ERROR: %s\n", strBuf ); return NULL; }
	res = mysql_store_result(tMainSql);
	ReleaseDB(tMainSql);
	return res;
}

// SendQuery�Լ��� �ܼ�ȭ��Ų Wrapper - DML��(CYG)
MYSQL * CDatabase::Execute( const char * strQuery, ... )
{
	MYSQL	 *	tMainSql;
	tMainSql = GetDB(m_nBaseDBID);		// Execute�Լ��� �⺻ DB�� �����Ѵ�.
	if( tMainSql == NULL ) return NULL;
	char strBuf[QUERY_SIZE2];
	memset(strBuf, 0x00, QUERY_SIZE2);
	va_list args;	va_start(args, strQuery);
	vsnprintf( strBuf,QUERY_SIZE2, strQuery, args );
	va_end(args);
	if( m_bTrace ) printf( "QUERY : %s\n", strBuf );
	if(PingDB(tMainSql, m_hDBList[m_nBaseDBID].c_str()) < 0)  { LOGOUT_DATABASE( "QUERY ERROR: %s\n", strBuf ); return NULL; }
	if(SendQuery(tMainSql, strBuf, strlen(strBuf)) != 0) { LOGOUT_DATABASE( "QUERY ERROR: %s\n", strBuf ); return NULL; }
	ReleaseDB(tMainSql);
	return tMainSql;
}

// mysql_fetch_row�� �ʵ�������� ���� �Լ� (CYG)
bool CDatabase::FetchRow( MYSQL_RES* pRes, MY_ROW * pRow )
{
	if( pRes == NULL ) return false;

	MYSQL_FIELD* pField;
	MYSQL_ROW hRow = mysql_fetch_row(pRes);
	if( hRow == NULL ) { pRow = NULL; return false; }
	int nField = 0;
	mysql_field_seek( pRes, 0 );
	string strFieldName;
	WHILE( 1, 256 )
	{
		pField = mysql_fetch_field( pRes );
		if( pField == NULL ) break;
		strFieldName = pField->name;
		(*pRow)[strFieldName] = (hRow)[nField];
		nField++;
	}
	return true;
}

// DB���� �ð� �������� (CYG)
int CDatabase::GetDBTime( CTime * pTime )
{
	if( pTime == NULL ) return -1;
    MYSQL_RES  *res = Query( "SELECT DATE_FORMAT(now(), '%%Y') as time_year, DATE_FORMAT(now(), '%%m') as time_month, "
							 " DATE_FORMAT(now(), '%%d') as time_date, DATE_FORMAT(now(), '%%h') as time_hour,"
							 " DATE_FORMAT(now(), '%%i') as time_minute, DATE_FORMAT(now(), '%%s') as time_second,"
							 " DAYOFWEEK(now()) as time_day"  );
    if( res == NULL ) return -1;
    MY_ROW  row;    FetchRow(res, &row);

    pTime->m_nYear  = ATOI(row["time_year"]);
    pTime->m_nMonth = ATOI(row["time_month"]);
    pTime->m_nDay   = ATOI(row["time_date"]);
    pTime->m_nHour  = ATOI(row["time_hour"]);
    pTime->m_nMinute= ATOI(row["time_minute"]);
    pTime->m_nSecond= ATOI(row["time_second"]);
    pTime->m_nWeek  = ATOI(row["time_day"]);

	switch(pTime->m_nMonth)
	{
		case 1:  case 2:  case 3:	pTime->m_nQuarter = 1; break;
		case 4:  case 5:  case 6:	pTime->m_nQuarter = 2; break;
		case 7:  case 8:  case 9:	pTime->m_nQuarter = 3; break;
		case 10: case 11: case 12:	pTime->m_nQuarter = 4; break;
	}

	return 0;
}


void CDatabase::TRACE_ENABLE() {	printf("��\n");m_bTrace = true; }
void CDatabase::TRACE_DISABLE(){	m_bTrace = false; }

