#ifndef __WIN32HELPER_H__
#define __WIN32HELPER_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWin32Helper
{
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CEvent
{
	HANDLE h;
	CEvent( const CEvent& ) {}
	CEvent& operator=( const CEvent& ) {}
public:
	CEvent( bool bInitState = false, bool bManualReset = true ) { h = CreateEvent(0, bManualReset, bInitState, 0 ); }
	~CEvent() { CloseHandle(h); }
	bool Set() { return SetEvent(h) != 0; }
	bool Pulse() { return SetEvent(h) != 0; }
	bool Reset() { return ResetEvent(h) != 0; }
	void Wait() { WaitForSingleObject(h, INFINITE ); }
	bool IsSet() { return WaitForSingleObject( h, 0 ) == WAIT_OBJECT_0; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CCriticalSection
{
	CRITICAL_SECTION sect;
	CCriticalSection( const CCriticalSection & ) {}
	CCriticalSection& operator=( const CCriticalSection &) {}
	//
	void Enter() { EnterCriticalSection( &sect ); }
	void Leave() { LeaveCriticalSection( &sect ); }
public:
	CCriticalSection() { InitializeCriticalSection( &sect ); }
	~CCriticalSection() { DeleteCriticalSection( &sect ); }
	friend class CCriticalSectionLock;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CCriticalSectionLock
{
	CCriticalSection &lock;
	bool bInsideCriticalSection;
public:
	CCriticalSectionLock( CCriticalSection &_lock ): lock(_lock) { bInsideCriticalSection = true; lock.Enter(); }
	~CCriticalSectionLock() { if ( bInsideCriticalSection ) lock.Leave(); }

	void Enter() { lock.Enter(); bInsideCriticalSection = true; }
	void Leave() { if ( bInsideCriticalSection ) lock.Leave(); bInsideCriticalSection = false; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
class com_ptr
{
	T *pData;
	void Assign( T *_pData ) { if ( _pData ) { _pData->AddRef(); } pData = _pData; }
	void Free() { if ( pData ) pData->Release(); }
public:
	com_ptr( T *_pData = 0 ) { Assign( _pData ); }
	~com_ptr() { Free(); }
	com_ptr( const com_ptr &a ) { Assign( a.pData ); }
	com_ptr& operator=( const com_ptr &a ) { if ( pData == a.pData ) return *this; Free(); Assign( a.pData ); return *this; }
	com_ptr& operator=( T *pObj ) { if ( pData == pObj ) return *this; Free(); Assign( pObj ); return *this; }
	operator T*() const { return pData; }
	T* operator->() const { return pData; }
	// not fair play
	void Create( T *_pData ) { Free(); pData = _pData; }
	T** GetAddr() { Free(); pData = 0; return &pData; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CDLLHandle
{
	HMODULE handle;												// DLL handle
	std::string szName;										// file name
	// disable copying...
	CDLLHandle( const CDLLHandle &dll ) {  }
	CDLLHandle& operator=( const CDLLHandle &dll ) { return *this; }
	CDLLHandle() : handle( 0 ) {  }
public:
	CDLLHandle( const char *pszFileName ) : szName( pszFileName ) { handle = ::LoadLibrary( pszFileName ); }
	CDLLHandle( const std::string &szFileName ) : szName( szFileName ) { handle = ::LoadLibrary( szFileName.c_str() ); }
	~CDLLHandle() { if ( handle ) ::FreeLibrary( handle ); }
	// success loading check
	bool IsLoaded() const { return handle != 0; }
	// proc loading. 
	// NOTE: 2nd parameter are a fake - just for return template argument resolving (because of MSVC6.0 can't do it)
	// one can pass just a '(TProc)0' here
	template <class TProc> 
		TProc GetProcAddress( const char *pszProcName, TProc )
	{
		return IsLoaded() ? (TProc)::GetProcAddress( handle, pszProcName ) : (TProc)0;
	}
	template <class TProc> 
		TProc GetProcAddress( int nProcID, TProc )
	{
		return IsLoaded() ? (TProc)::GetProcAddress( handle, (const char *)nProcID ) : (TProc)0;
	}
	// access & casting
	HMODULE GetHMdule() const { return handle; }
	const std::string& GetModuleName() const { return szName; }
	operator HMODULE() const { return handle; }
	operator const char*() const { return szName.c_str(); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
