#include "stdafx.h"
//
#include "wMain.h"
#include "..\MiscDll\LogStream.h"
#include "aiRoute.h"
//
#include "A5Script.h"
#include "scriptCommon.h"
#include "scriptTemplate.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( PlaceTemplate, "ns" );
	CPtr<NWorld::CWorld> pWorld = pScript->pWorld;
	CPtr<NAI::CAIRouteWaypoint> pWaypoint = pWorld->GetWaypoint( luaParams[ 1 ].s );
	if ( !( IsValid( pWaypoint ) &&  pWorld->PlaceTemplate( luaParams[ 0 ].n, pWaypoint->ptPos ) ) )
		csSystem << CC_RED << "Error: can't load template [ " << 
			CC_YELLOW << luaParams[ 0 ].n << CC_RED <<  " ]" << endl;
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
}