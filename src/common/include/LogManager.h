///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DESC : �α� ������ ���� �⺻ Ŭ����
// DATE : 2009.06
// AUTH : MUTTER(CYG)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _LOGMANAGER_H_
#define _LOGMANAGER_H_

#include "Include.h"
#include "Define.h"
#include <cstdarg>
#include <cstdio>

#define MAX_LOG_BUFFER 	2048

#define LOG_ERROR_PATH		"../Log/ERROR"
#define LOG_DATABASE_PATH	"../Log/DATABASE"
#define LOG_OPERATE_PATH	"../Log/OPERATE"
#define LOG_PACKET_PATH		"../Log/PACKET"
#define LOG_ETC_PATH		"../Log/ETC"


enum LOG_LEVEL_DEFINE
{
	LOG_LEVEL_ERROR		= 0,	// �����α� �Լ�/���ι�ȣ/����
	LOG_LEVEL_DATABASE,			// ���α� �Լ�/���ι�ȣ/����
	LOG_LEVEL_OPERATE,			// ��α�
	LOG_LEVEL_PACKET,			// ��Ŷ�α�
	LOG_LEVEL_ECT,				// ��Ÿ�α�
	LOG_LEVEL_SCREEN,			// ȭ�����θ� ǥ��
	LOG_LEVEL_FILE,				// ���Ϸθ� ǥ��
	LOG_LEVEL_ALL
};

#define LOGOUT_ERROR   		CLogManager(__FILE__, __LINE__, LOG_LEVEL_ERROR ).LogOut
#define LOGOUT_DATABASE 	CLogManager(__FILE__, __LINE__, LOG_LEVEL_DATABASE).LogOut
#define LOGOUT_OPERATE 		CLogManager( LOG_LEVEL_OPERATE ).LogOut
#define LOGOUT_PACKET 		CLogManager( LOG_LEVEL_PACKET ).LogOut
#define LOGOUT_ECT	 		CLogManager( LOG_LEVEL_ECT ).LogOut
#define LOGOUT_SCREEN 		CLogManager( LOG_LEVEL_SCREEN).LogOut
#define LOGOUT_TEST			CLogManager( LOG_LEVEL_SCREEN).LogOut
#define LOGOUT_FILE			CLogManager( LOG_LEVEL_FILE).LogOut

class CLogManager
{
public:
	CLogManager(const char *strFileName, int nLineNo, int nLogLevel );
	CLogManager( int nLogLevel );
	~CLogManager();
	void		LogOut(const char *strFormat, ...);

	// Extended ctor used by the Game modules: captures file/line/level plus an
	// optional context object (player/room ptr, currently advisory) and the
	// message in one shot; the matching no-arg LogOut() flushes it. Additive.
	CLogManager(const char *strFileName, int nLineNo, int nLogLevel,
	            const void *pObject, const char *strFormat, ...)
		: m_strFileName(strFileName), m_nLineNo(nLineNo), m_nLogLevel(nLogLevel)
	{
		(void)pObject;
		va_list ap; va_start(ap, strFormat);
		vsnprintf(m_sPreformat, MAX_LOG_BUFFER, strFormat, ap);
		va_end(ap);
	}
	void		LogOut() { LogOut("%s", m_sPreformat); }
	static int	LOG_LEVEL_FILE[LOG_LEVEL_ALL];
	static int	LOG_LEVEL_SCREEN[LOG_LEVEL_ALL];
	static char*LOG_SYSTEM_NAME;	// ������� �ӽ�/�ý��� ���� �����ϰ� ����Ұ�
protected:
	const char*	m_strFileName;
	int			m_nLineNo;
	int			m_nLogLevel;
	char		m_sPreformat[MAX_LOG_BUFFER];   // message captured by the extended ctor
};
#endif
