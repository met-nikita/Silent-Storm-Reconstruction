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
static unordered_map< int, CCallInfoHash > *pEventHandlers = 0;
static int nEventHandlersCount = 0;
////////////////////////////////////////////////////////////////////////////////////////////////////
inline unordered_map< int, CCallInfoHash > &GetEventHandlers()
{
	if ( pEventHandlers == 0 )
		pEventHandlers = new unordered_map< int, CCallInfoHash >();
	return *pEventHandlers;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ThrowEventInner( const type_info &eventID, const void *pStuff )
{
	int nEventID = (int)&eventID;
	CCallInfoHash &handlers = GetEventHandlers()[ nEventID ];
	for ( CCallInfoHash::iterator i = handlers.begin(); i != handlers.end(); ++i )
		(*i)->Call( pStuff );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void RegisterEventHandler( IEventRegister *pReg, const type_info &eventID )
{
	int nEventID = (int)&eventID;
	GetEventHandlers()[ nEventID ].push_back( pReg );
	++nEventHandlersCount;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void UnregisterEventHandler( IEventRegister *pReg, const type_info &eventID )
{
	int nEventID = (int)&eventID;
	vector<IEventRegister*> &handlers = GetEventHandlers()[ nEventID ];
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