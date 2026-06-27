#include "Secure.h"

CSecure::CSecure() : m_bInit(false), m_bEnable(true) {}
CSecure::~CSecure(){}

// 보안클래스 초기화
int	CSecure::InitSeq()
{
	time_t t; time(&t);
	srand((int)t);
	int nSeed = rand()%MAX_SECURE_SEQ;
	InitSeq( nSeed );
	return nSeed;
}

// 보안클래스 초기화
int	CSecure::InitSeq( int nSeed)
{
	m_bInit = true;
	m_nRecvSeq = nSeed;
	m_nSendSeq = nSeed;
	return nSeed;
}

// 시퀀스키 제작 (배열로 크로스 XOR한다)
// 각각의 Secure클래스는 순차적인 시퀀스를 보관하고 있지만
// 실제 전송되는 시퀀스는 비순차적인 시퀀스를 전송하여 비교한다.
int CSecure::MakeKey( int * pSeq )
{
	(*pSeq)++;
	if( *pSeq > MAX_SECURE_SEQ ) *pSeq = 0;
	int nArray1 = (*pSeq)%128;
	int nArray2 = (*pSeq/127)%128;
	int nResult = ((*pSeq)^(SECURE_ARRAY[nArray1]*SECURE_ARRAY[nArray2]));
	return nResult;
}

// 전송시퀀스 증가
int CSecure::MakeSendSeq(){ return MakeKey(&m_nSendSeq); }
// 송신시퀀스 증가
int	CSecure::MakeRecvSeq(){ return MakeKey(&m_nRecvSeq); }

// 전송 패킷 암호화
int	CSecure::MakeSendPacket( CHeadPacket * pPacket )
{
	if( !m_bEnable ) return 0;
	if( pPacket == NULL ) return -1;
	char * pBuffer = (char*)pPacket;
	// 비정형 시퀀스 생성
	int nSendSeq = MakeSendSeq();
	pPacket->m_nSequence = nSendSeq;
	// 시퀀스를 통한 암호화
	EncryptBuffer( &pBuffer[HEAD_SIZE], pPacket->m_nBodySize );
	return 0;
}

// 수신 패킷 암호화
int	CSecure::MakeRecvPacket( CHeadPacket * pPacket )
{
	if( !m_bEnable ) return 0;
	if( pPacket == NULL ) return -1;
	char * pBuffer = (char*)pPacket;
	int nRecvSeq = MakeRecvSeq();

	if( pPacket->m_nSequence != nRecvSeq) return -2; // 시퀀스가 일치하지 않음
	DecryptBuffer( &pBuffer[HEAD_SIZE], pPacket->m_nBodySize );
	return 0;
}

// 패킷을 암호화한다. (128쌍의 순차키로 XOR)
int	CSecure::EncryptBuffer( char * pBuffer, int nLen )
{
	int nArray1 = m_nSendSeq%128;
	int nArray2 = (m_nSendSeq/127)%128;
	if( nArray1 == nArray2 ) nArray2++; // 각 순차키는 일치할 수 없다.
	int i = 0;
	for( i = 0; i < nLen; i++ )
	{
		pBuffer[i] = pBuffer[i]^SECURE_ARRAY[nArray1];
		pBuffer[i] = pBuffer[i]^SECURE_ARRAY[nArray2];
		nArray1++; if( nArray1 > 127 ) nArray1 = 0;
		nArray2++; if( nArray2 > 127 ) nArray2 = 0;
	}
	return 0;
}

// 패킷을 복호화한다.
int	CSecure::DecryptBuffer( char * pBuffer, int nLen )
{
	int nArray1 = m_nRecvSeq%128;
	int nArray2 = (m_nRecvSeq/127)%128;
	if( nArray1 == nArray2 ) nArray2++; // 각 순차키는 일치할 수 없다.
	int i = 0;
	for( i = 0; i < nLen; i++ )
	{
		pBuffer[i] = pBuffer[i]^SECURE_ARRAY[nArray2];
		pBuffer[i] = pBuffer[i]^SECURE_ARRAY[nArray1];
		nArray1++; if( nArray1 > 127 ) nArray1 = 0;
		nArray2++; if( nArray2 > 127 ) nArray2 = 0;
	}
	return 0;
}

