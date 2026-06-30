#include "StdAfx.h"
#include "Basic2.h"
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
