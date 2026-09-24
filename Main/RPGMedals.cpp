#include "StdAfx.h"
#include "RPGMedals.h"
#include "RPGUnit.h"
#include "RPGGlobal.h"
#include "..\MiscDll\LogStream.h"   // CLogStream / csSystem / EConsoleColor (CC_RED=1, CC_GREEN=2)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// v1.1 0x6ac9b0 / v1.2 0x6aca40: only roster units collect medals.
CMedalsGainer::CMedalsGainer( NDb::CRPGPers *_pPers, bool _bDisabled ):
	pPers( _pPers ), bDisabled( _bDisabled )
{
	if ( !IsValid( pPers ) || bDisabled )
		return;
	NDb::CSide *pSide = pPers->pSide;
	if ( !IsValid( pSide ) )
		return;
	medalInfos.resize( pSide->medals.size() );
	for ( int i = 0; i < medalInfos.size(); ++i )
	{
		SMedalInfo &info = medalInfos[i];
		info.bIsCollectingPoints = !IsValid( pSide->medals[i]->pPrecedingMedal );
		info.bIsGained = false;
		info.bWillBeGiven = false;
		info.fPoints = 0;
		info.nProbability = 0;
		info.pMedal = pSide->medals[i];
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail @0x6a6530. The inclusive comparison is intentional: probability 0 can win only when the
// generator returns 0, matching the shipped executable.
void CMedalsGainer::ThrowCheck( int nMedal )
{
	SMedalInfo &info = medalInfos[nMedal];
	SRand rand;
	csSystem << CC_GREEN << "Medal probability: " << info.nProbability << endl;
	if ( rand.Get( 100 ) <= info.nProbability )
	{
		info.bWillBeGiven = true;
		csSystem << CC_GREEN << "Medal check succeeded" << endl;
	}
	else
		csSystem << CC_RED << "Medal check failed" << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail @0x6a6600. Progress is retained after each threshold, so one event performs at most one
// probability check for a medal; this is observable with large lock/trap difficulty awards.
void CMedalsGainer::AddMedalPoints( CGlobalGame *pGame, EMedalPointCases eCase, float fAmount )
{
	if ( bDisabled || !IsValid( pGame ) || !IsValid( pPers ) || !IsValid( pPers->pSide ) )
		return;

	const float fMedalPoints = GetMedalPoints( eCase, fAmount );
	for ( int n = 0; n < medalInfos.size(); ++n )
	{
		SMedalInfo &info = medalInfos[n];
		if ( !info.bIsCollectingPoints || info.bIsGained || info.bWillBeGiven || !IsValid( info.pMedal ) )
			continue;
		NDb::CMedal *pMedal = info.pMedal;
		if ( pMedal->bIsRussianOnly && !pGame->IsActiveZoneRussian() )
			continue;

		info.fPoints += fMedalPoints;
		if ( info.nProbability == 0 )
		{
			if ( info.fPoints >= pMedal->nPointsToStart )
			{
				info.fPoints -= pMedal->nPointsToStart;
				info.nProbability = pMedal->nStartingProbability;
				ThrowCheck( n );
			}
		}
		else if ( info.fPoints >= pMedal->nPointsToAddProbability )
		{
			info.fPoints -= pMedal->nPointsToAddProbability;
			info.nProbability += pMedal->nAddToProbability;
			ThrowCheck( n );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail @0x6a6810. A newly awarded medal unlocks every medal that directly follows it in the
// side's award chain. The just-found bit survives save/load until the presentation code consumes it.
void CMedalsGainer::GainMedalsAfterMissionEnd()
{
	if ( bDisabled || !IsValid( pPers ) || !IsValid( pPers->pSide ) )
		return;

	for ( int n = 0; n < medalInfos.size(); ++n )
	{
		SMedalInfo &awarded = medalInfos[n];
		if ( !awarded.bWillBeGiven || !IsValid( awarded.pMedal ) )
			continue;

		awarded.bWillBeGiven = false;
		awarded.bIsGained = true;
		awarded.bJustFound = true;
		for ( int next = 0; next < medalInfos.size(); ++next )
			if ( IsValid( medalInfos[next].pMedal ) &&
				medalInfos[next].pMedal->pPrecedingMedal == awarded.pMedal )
				medalInfos[next].bIsCollectingPoints = true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail @0x6a6a70: output uses the side's ordered medal vector, not the cached pointer in SMedalInfo.
void CMedalsGainer::GetGainedMedals( vector<CDBPtr<NDb::CMedal> > *pOut )
{
	if ( !pOut || !IsValid( pPers ) || !IsValid( pPers->pSide ) )
		return;
	const vector< CPtr<NDb::CMedal> > &medals = pPers->pSide->medals;
	pOut->reserve( medalInfos.size() );
	for ( int n = 0; n < medalInfos.size() && n < medals.size(); ++n )
		if ( medalInfos[n].bIsGained )
			pOut->push_back( CDBPtr<NDb::CMedal>( medals[n].GetPtr() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail @0x6a6b80: querying notifications consumes only bJustFound, never the gained state.
void CMedalsGainer::GetJustFoundMedals( vector<CDBPtr<NDb::CMedal> > *pOut )
{
	if ( !pOut || !IsValid( pPers ) || !IsValid( pPers->pSide ) )
		return;
	const vector< CPtr<NDb::CMedal> > &medals = pPers->pSide->medals;
	pOut->reserve( medalInfos.size() );
	for ( int n = 0; n < medalInfos.size() && n < medals.size(); ++n )
	{
		SMedalInfo &info = medalInfos[n];
		if ( info.bIsGained && info.bJustFound )
		{
			info.bJustFound = false;
			pOut->push_back( CDBPtr<NDb::CMedal>( medals[n].GetPtr() ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::GetMedalPoints @ RPGMedals.obj (release VA 0x6ac1e0)
//
// switch(eCase) over the 19-entry jump table. Each recognised case logs one
// "<colour><label>" line to csSystem then returns its point value. The constant
// cases ignore fAmount; the five scaled cases (13/14/15/17 -> *1.1, 16 -> *1.2)
// multiply the caller magnitude by a fixed double factor. The original default
// branch reads an unresolved engine global (unreachable for any valid case id);
// faithfully reconstructed as 0.0f -- no value is fabricated and no line is logged.
////////////////////////////////////////////////////////////////////////////////////////////////////
float GetMedalPoints( EMedalPointCases eCase, float fAmount )
{
	switch( eCase )
	{
	case MPC_ATTACK_ENEMY:
		csSystem << CC_GREEN << "Attacked enemy";
		return 1.0f;
	case MPC_ATTACK_ENEMY_IN_PK:
		csSystem << CC_GREEN << "Attacked enemy in PK";
		return 2.0f;
	case MPC_HIT_ENEMY:
		csSystem << CC_GREEN << "Hit enemy";
		return 3.0f;
	case MPC_HIT_PK:
		csSystem << CC_GREEN << "Hit enemy PK";
		return 2.0f;
	case MPC_HIT_ENEMY_IN_PK:
		csSystem << CC_GREEN << "Hit enemy in PK";
		return 5.0f;
	case MPC_HIT_ALLY:
		csSystem << CC_RED << "Hit ally";
		return -5.0f;
	case MPC_CRITICAL_HIT_ENEMY_IN_PK:
		csSystem << CC_GREEN << "Critical hit enemy in PK";
		return 10.0f;
	case MPC_CRITICAL_HIT_ALLY:
		csSystem << CC_RED << "Critical hit ally enemy";
		return -15.0f;
	case MPC_KILLED_ENEMY:
		csSystem << CC_GREEN << "Killed enemy";
		return 15.0f;
	case MPC_DISABLED_ENEMY_PK:
		csSystem << CC_GREEN << "Disabled enemy PK";
		return 20.0f;
	case MPC_KILLED_ENEMY_NOT_DISABLED_PK:
		csSystem << CC_GREEN << "Killed enemy w/o disabling PK";
		return 40.0f;
	case MPC_KILLED_ALLY:
		csSystem << CC_RED << "Killed ally";
		return -50.0f;
	case MPC_DISABLED_ALLY_PK:
		csSystem << CC_RED << "Disabled ally PK";
		return -30.0f;
	case MPC_HEALED_CRITICAL:
		csSystem << CC_GREEN << "Healed critical";
		return (float)( fAmount * 1.1 );
	case MPC_HEALED_WOUND:
		csSystem << CC_GREEN << "Healed wounds";
		return (float)( fAmount * 1.1 );
	case MPC_PICK_LOCK:
		csSystem << CC_GREEN << "Picked a lock";
		return (float)( fAmount * 1.1 );
	case MPC_DISARM_TRAP:
		csSystem << CC_GREEN << "Disarmed trap";
		return (float)( fAmount * 1.2 );
	case MPC_NOTICE_TRAP:
		csSystem << CC_GREEN << "Noticed trap";
		return (float)( fAmount * 1.1 );
	case MPC_CLUE_GAINED:
		csSystem << CC_GREEN << "Gained clue";
		return 40.0f;
	default:
		return 0.0f;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}; // namespace NRPG
////////////////////////////////////////////////////////////////////////////////////////////////////
