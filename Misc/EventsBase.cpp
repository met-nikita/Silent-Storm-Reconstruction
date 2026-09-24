#include "StdAfx.h"
#include "EventsBase.h"
//
namespace NGlobal
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static void FreeEventHandlersHashMap();
//
static struct SExecutionTracker
{
	bool bIsRunning;
	SExecutionTracker(): bIsRunning(true) {}
	~SExecutionTracker() { bIsRunning = false; FreeEventHandlersHashMap(); }
} tracker;
//
typedef vector<IEventRegister*> CCallInfoHash;
typedef unordered_map< const type_info *, CCallInfoHash > CEventHandlers;
static CEventHandlers *pEventHandlers = 0;
static int nEventHandlersCount = 0;
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CEventHandlers &GetEventHandlers()
{
	if ( pEventHandlers == 0 )
		pEventHandlers = new CEventHandlers();
	return *pEventHandlers;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ThrowEventInner( const type_info &eventID, const void *pStuff )
{
	CCallInfoHash &handlers = GetEventHandlers()[ &eventID ];
	for ( CCallInfoHash::iterator i = handlers.begin(); i != handlers.end(); ++i )
		(*i)->Call( pStuff );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void RegisterEventHandler( IEventRegister *pReg, const type_info &eventID )
{
	GetEventHandlers()[ &eventID ].push_back( pReg );
	++nEventHandlersCount;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void UnregisterEventHandler( IEventRegister *pReg, const type_info &eventID )
{
	vector<IEventRegister*> &handlers = GetEventHandlers()[ &eventID ];
	vector<IEventRegister*>::iterator i = find( handlers.begin(), handlers.end(), pReg );
	if ( i != handlers.end() )
	{
		handlers.erase( i );
		--nEventHandlersCount;
		ASSERT( nEventHandlersCount >= 0 );
	}
	FreeEventHandlersHashMap();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void FreeEventHandlersHashMap()
{
	if ( !tracker.bIsRunning && pEventHandlers != 0 && nEventHandlersCount == 0 )
	{
		delete pEventHandlers;
		pEventHandlers = 0;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
