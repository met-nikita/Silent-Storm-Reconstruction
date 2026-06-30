#include "StdAfx.h"
//
#include "aiJob.h"
#include "aiActionBase.h"      // CAIAction (new substrate base - phase 3): CanPerform/CanDo/ComparePlaces
#include "aiActionPlaceSource.h"
//
#include "aiChoosePlace.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release CAICombatLogic substrate - place-chooser job bodies (structural port, Approach A).
// Reconstructed from reconstruction/exports/{placesource.c, prepare.c, vtable_placesource.txt}.
// WIP - NOT yet in Main.vcxproj. Depends on the phase-3 CAIAction (CanPerform @vtbl0x18-pre,
// ComparePlaces @vtbl+0x18, CanDo @vtbl+0x1c) and the place-source layer.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Drop every candidate place that cannot spare `nAPToReserve`; debit the rest. @0x004751d0
////////////////////////////////////////////////////////////////////////////////////////////////////
void FilterPlacesByAP( vector<SPlaceWithAP> *pPlaces, int nAPToReserve )
{
	for ( int i = 0; i < (int)pPlaces->size(); )
	{
		if ( nAPToReserve < (*pPlaces)[i].nUnitAP )
		{
			(*pPlaces)[i].nUnitAP -= nAPToReserve;
			++i;
		}
		else
			pPlaces->erase( pPlaces->begin() + i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIChoosePlaceJob
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIChoosePlaceJob::CAIChoosePlaceJob( IAIJob *_pParentJob, int _nAPToReserve ):   // @0x00475550
	IAIChoosePlaceJob( _pParentJob ),
	bChoosingFinished( false ), nCurrentAction( 0 ), info( 10 ), nAPToReserve( _nAPToReserve )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// One step: advance the per-action place scan, keeping the best place for the current action. @0x004753f0
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIChoosePlaceJob::DoJob()
{
	if ( nCurrentAction >= (int)actions.size() )
	{
		bChoosingFinished = true;
		return;
	}
	CPtr<CAIAction> pAction = actions[ nCurrentAction ];
	if ( IsValid( pAction ) && pAction->CanPerform() )
	{
		SPlaceSourceInfo &si = info[ pAction ];
		if ( !si.bPlacesPrepared )
		{
			// pull the candidate places from the bound source, reserving the action's AP
			si.pSource->GetPlaces( &si.places );
			FilterPlacesByAP( &si.places, nAPToReserve );
			si.bPlacesPrepared = true;
		}
		else if ( si.nCurrentPlace < (int)si.places.size() )
		{
			SPlaceWithAP &place = si.places[ si.nCurrentPlace ];
			// skip inactive/lay poses (the 2-bit pose category 0 and 3 are rejected)
			int nPoseCat = ( ECheckMove )( place.place.pos.p.GetPose() );
			if ( nPoseCat != CM_LAY && nPoseCat != CM_INACTIVE )
			{
				if ( pAction->CanDo( place ) &&
					( si.nBestPlace < 0 ||
					  IsPlaceBetter( pAction, &place, &si.places[ si.nBestPlace ] ) ) )
				{
					si.nBestPlace = si.nCurrentPlace;
				}
			}
			++si.nCurrentPlace;
		}
		else
			++nCurrentAction;
	}
	else
		++nCurrentAction;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Restart the scan: clear every action's chosen/scan state. @0x00475210
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIChoosePlaceJob::Reset()
{
	bChoosingFinished = false;
	nCurrentAction = 0;
	for ( unordered_map< CPtr<CAIAction>, SPlaceSourceInfo, SPtrHash >::iterator i = info.begin(); i != info.end(); ++i )
	{
		SPlaceSourceInfo &si = (*i).second;
		si.nCurrentPlace = 0;
		si.nBestPlace = -1;
		si.bPlacesPrepared = false;
		si.places.clear();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Register an action and the place source it draws candidate places from. @0x004752a0
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIChoosePlaceJob::AddAction( CAIAction *pAction, IAIActionPlaceSource *pSrc )
{
	if ( !IsValid( pAction ) || !IsValid( pSrc ) )
		return;
	// only register an action once
	for ( int i = 0; i < (int)actions.size(); ++i )
		if ( actions[i] == pAction )
			return;
	actions.push_back( pAction );
	SPlaceSourceInfo si;
	si.pSource = pSrc;
	info[ pAction ] = si;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Return the best place chosen for `pAction`, if any. @0x00475160 (slot8)
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIChoosePlaceJob::GetPlaceForAction( CAIAction *pAction, SPlaceWithAP *pPlace )
{
	if ( !IsValid( pAction ) )
		return false;
	unordered_map< CPtr<CAIAction>, SPlaceSourceInfo, SPtrHash >::iterator i = info.find( pAction );
	if ( i == info.end() )
		return false;
	SPlaceSourceInfo &si = (*i).second;
	if ( si.nBestPlace < 0 )
		return false;
	if ( pPlace )
		*pPlace = si.places[ si.nBestPlace ];
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIChoosePlaceJob::operator&( CStructureSaver &f )                  // @0x00476c80
{
	f.Add( 2, (CAIJob*)this );
	f.Add( 3, &nAPToReserve );
	f.Add( 4, &actions );
	f.Add( 5, &info );
	f.Add( 6, &bChoosingFinished );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIChoosePlaceJob::SPlaceSourceInfo::operator&( CStructureSaver &f )
{
	f.Add( 2, &pSource );
	f.Add( 3, &places );
	f.Add( 4, &bPlacesPrepared );
	f.Add( 5, &nBestPlace );
	f.Add( 6, &nCurrentPlace );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIChoosePlaceForAttackJob - attack place policy: defer entirely to the action's ComparePlaces. @0x004762b0
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIChoosePlaceForAttackJob::CAIChoosePlaceForAttackJob( IAIJob *_pParentJob, int _nAPToReserve ):
	CAIChoosePlaceJob( _pParentJob, _nAPToReserve )
{
}
int CAIChoosePlaceForAttackJob::operator&( CStructureSaver &f ) { f.Add( 2, (CAIChoosePlaceJob*)this ); return 0; }
bool CAIChoosePlaceForAttackJob::IsPlaceBetter( CAIAction *pAction,
	const SPlaceWithAP *pCandidate, const SPlaceWithAP *pBest )
{
	return pAction->ComparePlaces( *pCandidate, *pBest );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIChoosePlaceForRetreatJob - retreat place policy.
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIChoosePlaceForRetreatJob::CAIChoosePlaceForRetreatJob( IAIJob *_pParentJob, int _nAPToReserve ):
	CAIChoosePlaceJob( _pParentJob, _nAPToReserve )
{
}
int CAIChoosePlaceForRetreatJob::operator&( CStructureSaver &f ) { f.Add( 2, (CAIChoosePlaceJob*)this ); return 0; }
bool CAIChoosePlaceForRetreatJob::IsPlaceBetter( CAIAction *pAction,
	const SPlaceWithAP *pCandidate, const SPlaceWithAP *pBest )
{
	// RECONSTRUCTION PENDING (build-settle): retreat prefers places farther from the enemy / safer;
	// CreateAIChoosePlaceForRetreatJob @0x00475680, IsPlaceBetter body to decompile.
	return pAction->ComparePlaces( *pCandidate, *pBest );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIChoosePlaceJob* CreateAIChoosePlaceForAttackJob( IAIJob *pParentJob, int nAPToReserve )  // @0x00475610
{
	return new CAIChoosePlaceForAttackJob( pParentJob, nAPToReserve );
}
IAIChoosePlaceJob* CreateAIChoosePlaceForRetreatJob( IAIJob *pParentJob )                   // @0x00475680
{
	return new CAIChoosePlaceForRetreatJob( pParentJob, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
BASIC_REGISTER_CLASS( CAIChoosePlaceForAttackJob )
BASIC_REGISTER_CLASS( CAIChoosePlaceForRetreatJob )
