#include "StdAfx.h"
//
#include "aiUnit.h"          // NAI::IAIUnit (GetUnitServer / GetUnitPosition / GetLogic)
#include "aiUnitState.h"     // NAI::SAIUnitState (pEnemy / pPossibleEnemy / GetKnownEnemies)
#include "aiPosition.h"      // NAI::SPathPlace / SUnitPosition / EPose
#include "aiRouteLogic.h"    // NAI::CreateAILookRoundLogic / MoveToPositionLogic / LookToPositionLogic / RoamingLogic
#include "aiRouteMisc.h"     // NAI::GetFearPosition / GetSafePosition (the run-away / hidden place searches)
#include "wUnitServer.h"     // NWorld::CUnitServer
#include "aiEvent.h"
#include "RPGUnitMission.h"
#include "RPGUnit.h"
#include "..\DBFormat\DataRPG.h"
//
#include "aiFearReaction.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiFearReaction -- the panic/flee reaction. Reconstructed from the matched-release decode (oracle:
// decomp/src/s2_aifearreaction.h, Update disasm-verified @0x3ce10). See aiFearReaction.h for the
// architecture + the transient-reaction note. Every reach resolves to a real in-tree call: the flee-place
// searches are the genuine aiRouteMisc NAI::GetFearPosition @0xa0b20 / GetSafePosition @0xa0ca0 over the
// unit's known-enemy set, and the installed behaviours are the same CreateAI*Logic route factories the
// Guard/Retreat reactions use.
//
// Contact consumption and live AP reservation follow retail v1.2 0x43d650.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
namespace {
// Same tile ignoring the direction/pose/moving bits (the route layer's mask 0x1feffff). The dev tree has no
// shared masked form -- inlined like aiGuardReaction.cpp / aiDefenceReaction.cpp.
inline bool SamePlace( const SPathPlace &a, const SPathPlace &b )
{
	return ( ( a.GetData() ^ b.GetData() ) & 0x1feffff ) == 0;
}
}   // anonymous namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3ce10 -- the per-think panic dance (the three rungs map 1:1 to the oracle).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIFearReaction::Update()
{
	IAIUnit *pU = GetUnit();
	if ( !IsValid( pU ) )
		return;
	// Retail v1.2 0x43d650 does not clear the route here (unlike Guard/Retreat).
	// the per-unit threat state (populated by the commander each think).
	SAIUnitState *us = GetAIUnitState();   // plain struct ptr -- not a CObjectBase, so no IsValid()
	if ( us == 0 )
		return;
	// (1) a live known enemy -> flee. With cover, try a hidden place first (GetSafePosition); on its failure
	// (or without cover) fall back to the run-away place (GetFearPosition). Both search the unit's known-enemy
	// set from the unit server; an invalid server makes both return false, so the rung falls through.
	IAIUnit *pEnemy = us->pEnemy.GetPtr();
	if ( IsValid( pEnemy ) )
	{
		NWorld::CUnitServer *pUS = pU->GetUnitServer();
		if ( IsValid( pUS ) )
		{
			SPathPlace fearPos( (int)0xfdffffff );   // invalid seed (const @0x8b...)
			bool bGot = false;
			if ( bUseCover )
				bGot = GetSafePosition( pUS, us->GetKnownEnemies(), &fearPos );
			if ( !bGot )
				bGot = GetFearPosition( pUS, us->GetKnownEnemies(), &fearPos, 1.0f, 0x14 );
			if ( bGot )
			{
				if ( SamePlace( fearPos, pU->GetUnitPosition().pos.p ) )
					SetLogic( CreateAILookRoundLogic( pU, CROUCH ) );   // already there: hunker + watch
				else
					SetLogic( CreateAIMoveToPositionLogic( pU, fearPos, RUN, RUN, false ) );   // run to it
				return;
			}
		}
	}
	// (2) only a suspected contact -> glance toward its place from where we stand.
	IAIUnit *pPossible = us->pPossibleEnemy.GetPtr();
	if ( IsValid( pPossible ) )
	{
		if ( IsValid( pPossible->GetUnitServer() ) )
		{
			SPathPlace p = pPossible->GetUnitServer()->GetPosition().pos.p;
			SetLogic( CreateAILookToPositionLogic( pU, p ) );
			us->Notify( CreateAILostPossibleEnemyEvent( pPossible ) );
			// 0x43d7dd..0x43d837: leave only the rotation cost in the live RPG AP.
			// This is SpendAP, not a refund or a write to the planner's AP copy.
			NRPG::IUnitMission *pRPG = pU->GetUnitMission();
			int nAP = pU->GetRPGUnit()->Skills( NDb::ST_AP );
			int nLookAP = pRPG->GetActionAP( pU->GetUnitPosition().GetPose(), NRPG::AC_ROTATE );
			pRPG->SpendAP( nAP - nLookAP );
		}
		return;
	}
	// (3) nobody about: a roaming civilian wanders its radius; everyone else crouches and looks round.
	if ( !bRoaming )
		SetLogic( CreateAILookRoundLogic( pU, CROUCH ) );
	else
		SetLogic( CreateAIRoamingLogic( pU, pU->GetUnitPosition().pos.p, nRadius ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3cab0 -- a live unit -> a new fear reaction.
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIReaction* CreateAIFearReaction( IAIUnit *pUnit, bool bUseCover, bool bRoaming, int nRadius )
{
	if ( !IsValid( pUnit ) )
		return 0;
	return new CAIFearReaction( pUnit, bUseCover, bRoaming, nRadius );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3ca80 -- is this reaction a fear reaction? (RTTI probe, disasm __RTDynamicCast + valid check.)
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsFearReaction( CAIReaction *p )
{
	return IsValid( dynamic_cast<CAIFearReaction*>( p ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
REGISTER_SAVELOAD_CLASS( 0x53043120, CAIFearReaction )
