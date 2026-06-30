#include "stdafx.h"
#include "A5Script.h"
#include "scriptCommon.h"
#include "wMain.h"
#include "wUnitServer.h"
#include "rpgGlobal.h"
#include "rpgDiplomacy.h"
#include "rpgUnitMission.h"
//
#include "scriptDiplomacy.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( GetDiplomacy, "nn" )
	int nPlayer1 = luaParams[ 0 ].n;
	int nPlayer2 = luaParams[ 1 ].n;
	NDb::EDiplomacyState state = 
		pScript->pWorld->GetDiplomacy()->GetDiplomacyState( nPlayer1, nPlayer2 );
	pScript->PushNumber( (int)state );
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( SetDiplomacy, "nnn" )
	int nPlayer1 = luaParams[ 0 ].n;
	int nPlayer2 = luaParams[ 1 ].n;
	NDb::EDiplomacyState state = (NDb::EDiplomacyState)luaParams[ 2 ].n; 
	CPtr<NWorld::CPlayer> pPlayer = pScript->pWorld->GetPlayerByID( nPlayer1 );
	if ( IsValid( pPlayer ) )
	{
		vector< CPtr<NRPG::IUnitMission> > player1Units;
		pPlayer->GetUnitsRPGs( &player1Units );
		pScript->pWorld->GetDiplomacy()->SetDiplomacyState( nPlayer1, player1Units, nPlayer2, state );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitGetDiplomacy, "un" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		int nPlayer = luaParams[ 1 ].n;
		NDb::EDiplomacyState state = 
			pUS->GetUnitRPG()->GetDiplomacy().GetDiplomacyState( nPlayer );
		pScript->PushNumber( (int)state );
	}
	else
		pScript->PushNil();
	return 1;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( UnitSetDiplomacy, "unn" )
	CDynamicCast<NWorld::CUnitServer> pUS(luaParams[0].p);
	if (pUS)
	{
		int nPlayer = luaParams[ 1 ].n;
		NDb::EDiplomacyState state = (NDb::EDiplomacyState)luaParams[ 2 ].n; 
		NRPG::SDiplomacy dip = pUS->GetUnitRPG()->GetDiplomacy();
		dip.SetDiplomacyState( nPlayer, state );
		pUS->GetUnitRPG()->SetDiplomacy( dip );
	}
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
}