#include "StdAfx.h"
//
#include "aiReaction.h"
#include "aiLogic.h"       // IAILogic
#include "aiUnit.h"        // IAIUnit
#include "wUnitServer.h"   // NWorld::CUnitServer
#include "aiMisc.h"           // NAI::GetAIUnit( CUnitServer* )
#include "aiGuardReaction.h"  // NAI::CreateAIGuardReaction
#include "aiFearReaction.h"   // NAI::CreateAIFearReaction
#include "aiReactions.h"      // NAI::CAINormalReaction
#include "MapBuild.h"         // ::SMapUnit ( eLogic / nRoamingRadius / pGuardAnimation )
#include "..\DBFormat\DataMap.h"  // NDb::EUnitLogic ( UL_EMPTY / UL_DEFAULT / UL_ROAMING / UL_FEAR )
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIReaction - base reflex bodies. The helpers forward through the (weak-back-ref) unit, null-guarded.
// Release vtbl offsets: GetAIState = pUnit vtbl[0x78], SetLogic = vtbl[0x58], GetUnitServer = vtbl[0x00].
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
SAIUnitState* CAIReaction::GetAIUnitState() const
{
	return IsValid( pUnit ) ? pUnit->GetAIUnitState() : 0;
}
IAIState* CAIReaction::GetAIState() const
{
	return IsValid( pUnit ) ? pUnit->GetAIState() : 0;
}
bool CAIReaction::SetLogic( IAILogic *pLogic )
{
	if ( !IsValid( pUnit ) )
		return false;
	pUnit->SetLogic( pLogic );
	return true;
}
NWorld::CUnitServer* CAIReaction::GetUnitServer() const
{
	return IsValid( pUnit ) ? pUnit->GetUnitServer() : 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NAI::CreateUnitReaction @0x919d0 (release __fastcall: ecx=pUnitServer, edx=&sMapUnit) -- the map-driven
// reaction factory. Resolve the server's AI-unit (GetAIUnit) and attach the reaction sMapUnit.eLogic
// prescribes via SetReaction (release vtbl 0x64). Offsets/args disasm-verified @0x4919d0: eLogic +0x54,
// nRoamingRadius +0x58, pGuardAnimation +0x68; FEAR passes (true,true) (edx==1 and the pushed edx==1).
// Dead/missing unit, AI object, or reaction -> no-op. Landed dead-until-consumed: the live caller
// (CreateUnitAI/CreateUnitRoute) is the separate, not-yet-ported aiCommander comp.
void CreateUnitReaction( NWorld::CUnitServer *pUnitServer, const SMapUnit &sMapUnit )
{
	if ( !IsValid( pUnitServer ) )
		return;
	IAIUnit *pAI = GetAIUnit( pUnitServer );
	if ( !IsValid( pAI ) )
		return;
	CPtr<CAIReaction> pReaction;
	switch ( sMapUnit.eLogic )
	{
	case NDb::UL_EMPTY:
		pReaction = CreateAIGuardReaction( pAI, sMapUnit.pGuardAnimation.GetPtr(), sMapUnit.nRoamingRadius );
		break;
	case NDb::UL_DEFAULT:
	case NDb::UL_ROAMING:
		pReaction = new CAINormalReaction( pAI );   // no CreateAINormalReaction factory in dev -- direct public ctor (cf. aiAssassinReaction.cpp:139)
		break;
	case NDb::UL_FEAR:
		pReaction = CreateAIFearReaction( pAI, true, true, sMapUnit.nRoamingRadius );
		break;
	default:
		return;
	}
	if ( IsValid( pReaction ) )
		pAI->SetReaction( pReaction.GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
