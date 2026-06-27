///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DESC : �α� ������ ���� �⺻ Ŭ����
// DATE : 2009.06
// AUTH : MUTTER(CYG)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#include "LogManager.h"
#include "Util.h"
#include <fstream>

int	   CLogManager::LOG_LEVEL_FILE[LOG_LEVEL_ALL] 	= {true, true, true, true, true, false, true };
int	   CLogManager::LOG_LEVEL_SCREEN[LOG_LEVEL_ALL] = {true, true, true, true, true, true , false };
char * CLogManager::LOG_SYSTEM_NAME = NULL;

CLogManager::CLogManager(const char *strFileName, int nLineNo, int nLogLevel)
	: m_strFileName(strFileName)
	, m_nLineNo(nLineNo)
	, m_nLogLevel(nLogLevel){}
CLogManager::CLogManager(int nLogLevel)
	: m_strFileName(NULL)
	, m_nLineNo(0)
	, m_nLogLevel(nLogLevel){}
CLogManager::~CLogManager(){ }

// �α� ���
void CLogManager::	LogOut(const char *strFormat, ...)
{
	va_list args;
	va_start(args, strFormat);
	time_t t; std::tm hTimeVal; std::tm* hTime = &hTimeVal;
	char strBuffer[MAX_LOG_BUFFER];
    char strLogPath[MAX_PATH];
    char strFileBuf[MAX_LOG_BUFFER];
    time(&t); hTimeVal = LocalTime(t);
	memset(strBuffer, 0, MAX_LOG_BUFFER);
	vsnprintf(strBuffer, MAX_LOG_BUFFER, strFormat, args);
	va_end(args);

	memset(strLogPath, 0x00, MAX_PATH);
	memset(strFileBuf, 0x00,  MAX_LOG_BUFFER);

	char strTmpName[]    = "UNDEFINED";
	char strNameFormat[] = "%s/%s_%04d_%02d_%02d.log";
	if( LOG_SYSTEM_NAME == NULL ) LOG_SYSTEM_NAME = strTmpName;

	if(  LOG_LEVEL_FILE[m_nLogLevel] == true )
	{
		switch( m_nLogLevel )
		{
			case LOG_LEVEL_ERROR: 		snprintf(strLogPath, MAX_PATH, strNameFormat, LOG_ERROR_PATH, LOG_SYSTEM_NAME, hTime->tm_year+1900, hTime->tm_mon+1, hTime->tm_mday ); break;
			case LOG_LEVEL_DATABASE: 	snprintf(strLogPath, MAX_PATH, strNameFormat, LOG_DATABASE_PATH, LOG_SYSTEM_NAME, hTime->tm_year+1900, hTime->tm_mon+1, hTime->tm_mday ); break;
			case LOG_LEVEL_OPERATE: 	snprintf(strLogPath, MAX_PATH, strNameFormat, LOG_OPERATE_PATH, LOG_SYSTEM_NAME, hTime->tm_year+1900, hTime->tm_mon+1, hTime->tm_mday ); break;
			case LOG_LEVEL_PACKET: 		snprintf(strLogPath, MAX_PATH, strNameFormat, LOG_PACKET_PATH, LOG_SYSTEM_NAME, hTime->tm_year+1900, hTime->tm_mon+1, hTime->tm_mday ); break;
			case LOG_LEVEL_ECT:
			default:					snprintf(strLogPath, MAX_PATH, strNameFormat, LOG_ETC_PATH, LOG_SYSTEM_NAME, hTime->tm_year+1900, hTime->tm_mon+1, hTime->tm_mday ); break;
		}
	    std::ofstream ofs(strLogPath, std::ios::app | std::ios::binary);
	    if( ofs.is_open() )
	    {
	    	ofs.seekp(0, std::ios::end);
	    	std::streamoff nFileSize = ofs.tellp();
	    	// ����ũ�Ⱑ 50MB�� ������ ��ũ ��ȣ�� ���Ͽ� ������� �ʴ´�.
	    	if(  nFileSize < 1024*1024*50 )
	    	{
	    		// ������ DB�� ��츸 ���� ǥ��
				if( m_nLogLevel < LOG_LEVEL_OPERATE ) snprintf(strFileBuf, MAX_LOG_BUFFER, "%02d:%02d:%02d %s \t=> %s (%d)\n", hTime->tm_hour, hTime->tm_min, hTime->tm_sec, strBuffer, m_strFileName, m_nLineNo );
				else						    	  snprintf(strFileBuf, MAX_LOG_BUFFER, "%02d:%02d:%02d %s", hTime->tm_hour, hTime->tm_min, hTime->tm_sec, strBuffer);
				ofs << strFileBuf;   // (was fprintf(fp, strFileBuf) - also fixes a format-string hazard)
	    	}
	    }
	}

	if(  LOG_LEVEL_SCREEN[m_nLogLevel] == true  )
	{
		if( m_nLogLevel < LOG_LEVEL_OPERATE ) 	printf("\x1b[31m%s \t=> %s (%d)\x1b[0m\n", strBuffer, m_strFileName, m_nLineNo);
		else									printf("%s", strBuffer );
	}
}
