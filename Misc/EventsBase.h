#ifndef __EVENTS_BASE_H__
#define __EVENTS_BASE_H__
//
namespace NGlobal
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class IEventRegister
{
public:
	virtual void Call( const void *pStuff ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void ThrowEventInner( const type_info &eventID, const void *pStuff );
void RegisterEventHandler( IEventRegister *pReg, const type_info &eventID );
void UnregisterEventHandler( IEventRegister *pReg, const type_info &eventID );
//
template<class T> void ThrowEvent( const T &event ) 
{ 
	ThrowEventInner( typeid(T), &event ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T, class TParam>
class CEventRegister : public IEventRegister
{
	typedef void (T::*Member)( const TParam &p );
	Member func;
	T *pRealObj;
	//
	CEventRegister() {ASSERT(0);}
	void operator=( const CEventRegister &) {ASSERT(0);}
	int operator&( CStructureSaver &f ) { ASSERT(0); return 0; }
	void Call( const void *pStuff ) { if ( IsValid( pRealObj ) ) (pRealObj->*func)( *(TParam*)pStuff ); }
public:
	template<class TFunc>
	CEventRegister( T *_pRealObj, TFunc _func )
	{ 
		pRealObj = _pRealObj;
		func = (Member)_func;
		RegisterEventHandler( this, typeid(TParam) );
		if ( 0 )
			(pRealObj->*func)( *(TParam*)0 );
	}
	~CEventRegister() 
	{
		UnregisterEventHandler( this, typeid(TParam) );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __EVENTS_BASE_H__