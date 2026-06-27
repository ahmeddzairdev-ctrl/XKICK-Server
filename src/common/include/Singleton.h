
#ifndef __SINGLETON_H__
#define __SINGLETON_H__

#include <cassert>
#include "Define.h"
//*****************************************************************************
//class: CSingleton 탬플랫으로 된 유닛의 부분
//상속을 통해서 클레스를 단일체화 시킨다. 생성부분과 소멸부분을 추가 
//*****************************************************************************
template <typename T> 
class CSingleton
{
    static T* ms_pSingleton;		// 단일체로 생성된 객체의 주소값

public:
			
	//단일체 생성
 	static void createSingleton();	
				  
	//단일체 소멸
    void releaseSingleton();

	//이함수를 통해서 단일체를 접근	 
    static T& Get( void );

	//이함수를 통해서 단일체를 접근	 
    static T* GetPtr( void );
	//생성자
	CSingleton();
	//소멸자
	~CSingleton();
};

template <typename T> T* CSingleton <T>::ms_pSingleton;		// 멤버 변수 초기화

//CSingleton Class를 상속받아서 단일체 생성시 자손 클레스와 부모 클레스의
//this 주소값의 차이를 구해서 주소값을 저장함
template <typename T> 
CSingleton<T>::CSingleton()
{
    assert( !ms_pSingleton );
    int offset = (int)(T*)1 - (int)(CSingleton <T>*)(T*)1;
    ms_pSingleton = (T*)((int)this + offset);
}
template <typename T> 
CSingleton<T>::~CSingleton()
{
	assert( ms_pSingleton );  
	ms_pSingleton = 0L;
}

//단일체 생성
template <typename T> 
void CSingleton<T>::createSingleton()
{
	if(ms_pSingleton == NULL) 
	{
		ms_pSingleton = new T;
	}
}  
//단일체 소멸
template <typename T> 
void CSingleton<T>::releaseSingleton()
{
	SAFE_DELETE(ms_pSingleton); 
}
//이함수를 통해서 단일체를 접근
template <typename T> 	 
T& CSingleton<T>::Get( void )
{
	return ( *ms_pSingleton );  
}
//이함수를 통해서 단일체를 접근	 
template <typename T> 
T* CSingleton<T>::GetPtr( void )
{
	return ( ms_pSingleton );  
}

#endif //__SINGLETON_H__

