#include "StdAfx.h"
#include "Basic2.h"
// During a save-load object-table teardown a deserialized graph can still hold a dangling smart-ptr to a
// sibling that was already freed (resource/DG layer). Releasing through it faults on freed+unmapped memory.
// Only inside that teardown window (g_bSaveLoadTeardown), skip a release whose `this` is no longer mapped --
// the object is gone, so decrementing its refcount is a no-op anyway. NOT active during normal play (no cost
// and no behavior change there). See BasicChunk1.h. ROOT (the specific dangling ref) still open -- pin with
// PageHeap (gflags /p /enable Game.exe /full) then _loadtest.py; this guard keeps existing saves loadable.
extern bool g_bSaveLoadTeardown;
static bool UafCheck( CObjectBase *p )
{
	if ( !g_bSaveLoadTeardown ) return false;
	return IsBadReadPtr( p, 8 ) != 0;				// unmapped => already freed; skip the release
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjectBase
////////////////////////////////////////////////////////////////////////////////////////////////////
const char* CObjectBase::GetTypeName()
{
	const char *pszName = typeid(*this).name(), *p;
	p = strstr( pszName, "::" );
	if ( !p )
		return pszName + 6;
	return p + 2;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool bDestroyInProgress;
static std::list<CObjectBase*> *pDestroy, *pInvalidate;
////////////////////////////////////////////////////////////////////////////////////////////////////
inline std::list<CObjectBase*>& GetDestroy()
{
	if ( !pDestroy )
		pDestroy = new std::list<CObjectBase*>;
	return *pDestroy;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline std::list<CObjectBase*>& GetInvalidate()
{
	if ( !pInvalidate )
		pInvalidate = new std::list<CObjectBase*>;
	return *pInvalidate;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void FreeLists()
{
	if ( pDestroy && pDestroy->empty() )
	{
		delete pDestroy; 
		pDestroy = 0;
	}
	if ( pInvalidate && pInvalidate->empty() )
	{
		delete pInvalidate; 
		pInvalidate = 0;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static struct STracker
{
	bool bIsRunning;
	STracker(): bIsRunning(true) {}
	~STracker() { bIsRunning = false; FreeLists(); }
} tracker;
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectBase::ReleaseObj( int nRef, int nMask )
{
	if ( UafCheck( this ) ) return;
	nObjData -= nRef;
	if ( (nObjData & 0x7fffffff) == 0 && nRefData == 0 )
	{
		if ( bDestroyInProgress )
			GetDestroy().push_back( this );
		else
		{
			bDestroyInProgress = true;
			delete this;
			DestroyDelayed();
		}
	}
	else if ( (nObjData & nMask) == 0 )
	{
		nObjData |= 0x80000000;
		if ( bDestroyInProgress )
		{
			AddRef();
			GetInvalidate().push_back( this );
		}
		else
		{
			AddRef();
			bDestroyInProgress = true;
			DestroyContents(); 
			ReleaseRef();
			DestroyDelayed();
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectBase::ReleaseRef()
{
	if ( UafCheck( this ) ) return;
	--nRefData;
	if ( nRefData == 0 && (nObjData & 0x7fffffff) == 0 )
	{
		if ( bDestroyInProgress )
			GetDestroy().push_back( this );
		else
		{
			bDestroyInProgress = true;
			delete this;
			DestroyDelayed();
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectBase::DestroyDelayed()
{
	ASSERT( bDestroyInProgress );
	std::list<CObjectBase*> &toDestroy = GetDestroy();
	std::list<CObjectBase*> &toInvalidate = GetInvalidate();
	while ( !toDestroy.empty() || !toInvalidate.empty() )
	{
		while ( !toDestroy.empty() )
		{
			CObjectBase *pObj = toDestroy.front();
			toDestroy.pop_front();
			delete pObj;
		}
		while ( !toInvalidate.empty() )
		{
			CObjectBase *pObj = toInvalidate.front();
			toInvalidate.pop_front();
			pObj->DestroyContents(); 
			pObj->ReleaseRef();
		}
	}
	bDestroyInProgress = false;
	if ( !tracker.bIsRunning )
		FreeLists();		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
