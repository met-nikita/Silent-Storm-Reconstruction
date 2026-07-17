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
	nCurrentAction( 0 ), info( 10 ), nAPToReserve( _nAPToReserve )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// One step: advance the per-action place scan, keeping the best place for the current action. @0x004753f0
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIChoosePlaceJob::DoJob()
{
	if ( nCurrentAction >= (int)actions.size() )
	{
		Finish();   // retail @0x4753f0: sets the base CAIJob finished byte (+0xc) directly
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
	ReArm();    // retail @0x475210: clears the base CAIJob finished byte (+0xc)
	nCurrentAction = 0;
	for ( unordered_map< CPtr<CAIAction>, SPlaceSourceInfo, SPtrHash >::iterator i = info.begin(); i != info.end(); ++i )
	{
		SPlaceSourceInfo &si = (*i).second;
		si.nCurrentPlace = 0;
		si.nBestPlace = -1;
		si.bPlacesPrepared = false;
		// @0x475210 - retail does NOT clear the places vector here. It pings the map KEY
		// (the action)'s ResetInfoHash (vtbl slot8/+0x20) so the action's cached SActionInfo
		// is dropped and CanDo/GetInfo recompute on the next scan. bPlacesPrepared=false
		// re-fetches the candidates (GetPlaces' operator= REPLACES the vector), so the old
		// filtered places need not be (and are not) cleared.
		(*i).first->ResetInfoHash();
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
// retail @0x475ea0: the interface level serializes only the CAIJob base at tag 2, giving the release
// wire its 2.2.2 nesting (byte-walked: 2.2 = 11-byte chunk holding ONLY the 9-byte CAIJob chunk
// {2 pParentJob CPtr4, 3 bJobFinished 1B}).
int IAIChoosePlaceJob::operator&( CStructureSaver &f )                  // @0x00475ea0
{
	f.Add( 2, (CAIJob*)this );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x476c80 -- retail wire (byte-walk-confirmed, slot with 4 attack + 2 retreat jobs):
// {2 IAIChoosePlaceJob{2 CAIJob{2 pParentJob, 3 bJobFinished}}, 3 nCurrentAction int4,
//  4 actions DoVector<CPtr<CAIAction>>, 5 info DoHashMap<CPtr<CAIAction>,SPlaceSourceInfo,SPtrHash>,
//  6 nAPToReserve int4}.
// The old dev wire was {2 CAIJob DIRECT, 3 nAPToReserve, 4, 5, 6 bChoosingFinished 1B}: the missing
// interface level shifted the base subtree (wire-audit RAWSZ 2.2.2.2 9v4 + MISS 2.2.3), tag 3 loaded
// retail's nCurrentAction into nAPToReserve, and tag 6 read retail's 4-byte nAPToReserve as a 1-byte
// bool (SIZE 2.6 4v1). nCurrentAction is now serialized like retail, so a mid-scan chooser resumes.
int CAIChoosePlaceJob::operator&( CStructureSaver &f )                  // @0x00476c80
{
	f.Add( 2, (IAIChoosePlaceJob*)this );
	f.Add( 3, &nCurrentAction );
	f.Add( 4, &actions );
	f.Add( 5, &info );
	f.Add( 6, &nAPToReserve );
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
	// @0x476470 - retreat ranking: FEWER AP left wins (more AP spent to reach the place =>
	// farther from the enemy => safer). Decomp: `return param_2->nUnitAP < param_3->nUnitAP;`.
	// pAction is unused here (the retail body ignores param_1), matching the decode.
	return pCandidate->nUnitAP < pBest->nUnitAP;
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
REGISTER_SAVELOAD_CLASS( 0x51313170, CAIChoosePlaceForAttackJob )
REGISTER_SAVELOAD_CLASS( 0x52443130, CAIChoosePlaceForRetreatJob )
