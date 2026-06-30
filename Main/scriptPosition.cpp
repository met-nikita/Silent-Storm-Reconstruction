#include "stdafx.h"
#include "A5Script.h"
#include "scriptCommon.h"
#include "scriptPtr.h"
#include "aiPosition.h"
#include "wUnitServer.h"
#include "wOSBase.h"
#include "aiRoute.h"
#include "grid.h"
#include "wMain.h"
//
#include "scriptPosition.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetPos, "u" )
	CLUAObjectPosition *pRes = new CLUAObjectPosition();
	CObjectBase *pObj = luaParams[ 0 ].p;
	CDynamicCast<NWorld::CUnitServer> pUnit(pObj);
	if (pUnit)
		pRes->ptPos = pUnit->GetPosition().pos.GetCP();
	else {
		CDynamicCast<NWorld::CObjectServerBase> pObject(pObj);
		if (pObject)
			pRes->ptPos = pObject->GetPosition().ptPos;
	}
	luaPushCObj( pState, pRes  );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetWaypointPos, "s" )
	CLUAObjectPosition *pRes = new CLUAObjectPosition();
	CPtr<NAI::CAIRouteWaypoint> pWaypoint = pScript->pWorld->GetWaypoint( luaParams[ 0 ].s );
	if ( IsValid( pWaypoint ) )
		pRes->ptPos = pWaypoint->pos.GetCP();	// retail @0x2e9bd0 uses pos.GetCP() (grid CP), NOT the authored
											// ptPos -- so it matches GetPos(unit) (also GetCP); else WaitForUnitWaypoint
											// never reaches 0 when the waypoint's authored height != its grid floor height.
	luaPushCObj( pState, pRes  );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetDistance, "uu" )
	CDynamicCast<CLUAObjectPosition> pPos1( luaParams[ 0 ].p );
	CDynamicCast<CLUAObjectPosition> pPos2( luaParams[ 1 ].p );
	if ( IsValid( pPos1 ) && IsValid( pPos2 ) )
		lua_pushnumber( pState, fabs( pPos1->ptPos - pPos2->ptPos ) / FP_GRID_STEP );
	else
		lua_pushnumber( pState, 0 );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NScript;
//
REGISTER_SAVELOAD_CLASS( 0x52122170, CLUAObjectPosition );