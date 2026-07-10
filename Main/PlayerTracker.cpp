#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "iMain.h"
#include "wInterface.h"
#include "RPGGame.h"
#include "RPGMerc.h"
#include "RPGGlobal.h"
#include "RPGUnitInfo.h"
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
#include "aiPath.h"
#include "Interface.h"
#include "iMission.h"
#include "UnitTracker.h"
#include "PlayerTracker.h"
#include "aiCommander.h"     // NAI::CSequenceCommander (the retail human-player commander, @0x287d70)
#include "wMain.h"           // NWorld::CWorld (complete type for the commander ctor dyncast)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPlayerTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
CPlayerTracker::CPlayerTracker( IMission *_pMission, NRPG::CGlobalPlayer *_pGlobalPlayer, const wstring &_wsName ): 
	pMission( _pMission ), pGlobalPlayer( _pGlobalPlayer ), wsName( _wsName )
{
	// retail CPlayerTracker ctor @0x287d70: the HUMAN player's commander is a NAI::CSequenceCommander
	// (a CAICommander that only auto-drives during cinematic sequences, GenerateCommand @0x358d0).
	// This is the retail infrastructure that makes NAI::GetAIUnit @0x742c0 resolve for the human's
	// units (CAICommander::GetAIUnit map @0x33eb0): every AI perception event about a player unit
	// (CreateAIEnemyEvent / CreateAIPossibleEnemyEvent from the threat tracker's OnSeeEnemy / OnBullet /
	// OnHearEnemy / OnGrenade handlers) carries that wrapper as payload. With the old plain
	// NWorld::CCommander the resolve returned 0, every player-payload event was null, and the whole
	// stimulus layer (heard gunfire -> possibleEnemy -> HUNT, mid-turn sighting -> pEnemy) was inert.
	CDynamicCast<NWorld::CWorld> pWorld( pMission->GetWorld() );
	NAI::CSequenceCommander *pSeqCommander = new NAI::CSequenceCommander( pWorld );
	pCommander = pSeqCommander;
	pPlayer =  pMission->GetWorld()->AddPlayer( wsName, pGlobalPlayer, pCommander );
	pSeqCommander->SetPlayer( pPlayer.GetPtr() );   // retail @0x287d70 tail: CAICommander::SetPlayer @0x33e60

	NAI::SPosition sPos;
	pPlayer->GetDeploySpot( &sPos.p );
	sPos.SetNetwork( pMission->GetWorld()->GetPathNetwork() );

	sCamPlacement = ICamera::SCameraPos( sPos.GetCP(), 25.0f, ToRadian( -65.0f ), ToRadian( -65.0f ), 0 );
	sCamPlacement.ptAnchor.z = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerTracker::AddUnit( NRPG::CUnit *pMerc )
{
	NAI::SPosition sPos;
	pPlayer->GetDeploySpot( &sPos.p );
	sPos.SetNetwork( pMission->GetWorld()->GetPathNetwork() );

	pMission->Command( new NWorld::CCmdAddUnit( pPlayer, sPos, pMerc ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerTracker::RemoveUnit( IUnitTracker *pUnit )
{
	pMission->Command( new NWorld::CCmdRemoveUnit( pPlayer, pUnit->GetUnit() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlayerTracker::IsPlayerLoser()
{
	bool bAllDead = true;
	for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
	{
		if ( unitsSet[nTemp]->GetUnit()->IsDead() )
			continue;
		if ( unitsSet[nTemp]->GetUnit()->IsUnconscious() )
			continue;

		bAllDead = false;
	}

	if ( bAllDead )
		return true;

	for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
	{
		if ( unitsSet[nTemp]->GetUnit()->GetRPG()->IsHero() )
			return false;
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlayerTracker::IsPlayerWinner()
{
	return pMission->GetWorld()->IsWinnerPlayer( pPlayer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlayerTracker::IsUnitVisible( NWorld::CUnit *pUnit ) const
{
	list<CPtr<NWorld::CUnit> > visibleUnits;
	pPlayer->GetVisible( &visibleUnits );
	if ( find( visibleUnits.begin(), visibleUnits.end(), pUnit ) == visibleUnits.end() )
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::EDiplomacyState CPlayerTracker::GetUnitDiplomacy( NWorld::CUnit *pUnit ) const
{
	// retail @0x287830: MY player's stance toward the unit's player (world (IPlayer,IPlayer)
	// overload) -- the old reversed (unit -> my player) query mis-colored asymmetric diplomacy
	// (neutral civilians whose own row marks the player ENEMY for their fear-AI).
	return pMission->GetWorld()->GetDiplomacyState( pPlayer, pUnit->GetPlayer() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerTracker::GetUnits( vector< CPtr<IUnitTracker> > *pUnits ) const
{
	pUnits->resize( unitsSet.size() );
	for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
		(*pUnits)[nTemp] = unitsSet[nTemp];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerTracker::GetSelectedUnits( vector< CPtr<IUnitTracker> > *pUnits ) const
{
	pUnits->reserve( unitsSet.size() );
	for ( vector< CObj<CUnitTracker> >::const_iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
	{
		if ( (*iTemp)->IsSelected() )
			pUnits->push_back( (*iTemp).GetPtr() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPlayerTracker::CountSelected()
{
	vector< CPtr<IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );
	return unitsSet.size();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerTracker::Select( NWorld::CUnit *pUnit, bool bAdditive )
{
	if ( unitsSet.size() == 0 )
		return;

	for ( int nTemp = 0; nTemp < unitsSet.size(); ++nTemp )
	{
		if ( ( unitsSet[nTemp]->GetUnit() == pUnit ) && !unitsSet[nTemp]->IsActive() )
			return;
	}

	for ( int nTemp = 0; nTemp < unitsSet.size(); ++nTemp )
	{
		if ( unitsSet[nTemp]->GetUnit() == pUnit )
			unitsSet[nTemp]->SetSelected( true );
		else if ( !bAdditive )
			unitsSet[nTemp]->SetSelected( false );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerTracker::Select( int nDir )
{
	if ( unitsSet.size() == 0 )
		return;

	int nSelected = -1, nAccessible = -1;
	for ( int nTemp = 0; nTemp < unitsSet.size(); ++nTemp )
	{
		if ( ( nSelected == -1 ) && unitsSet[nTemp]->IsSelected() )
			nSelected = nTemp;
		if ( ( nAccessible == -1 ) && unitsSet[nTemp]->IsActive() )
			nAccessible = nTemp;
	}

	for ( int nTemp = 0; nTemp < unitsSet.size(); ++nTemp )
		unitsSet[nTemp]->SetSelected( false );

	if ( ( nSelected == -1 ) && ( nAccessible == -1 ) )
		return;

	if ( nSelected != -1 )
	{
		for ( int nTemp = 0; nTemp < unitsSet.size(); ++nTemp )
		{
			nSelected += nDir;
			if ( nSelected < 0 ) nSelected = unitsSet.size() - 1;
			if ( nSelected >= unitsSet.size() ) nSelected = 0;

			if ( unitsSet[nSelected]->IsActive() )
			{
				unitsSet[nSelected]->SetSelected( true );
				break;
			}
		}
	}
	else
		unitsSet[nAccessible]->SetSelected( true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerTracker::SelectNext()
{
	Select( 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerTracker::SelectPrev()
{
	Select( -1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerTracker::Activate()
{
	Update( true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerTracker::Deactivate()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::IPlayer* CPlayerTracker::GetPlayer() const
{
	return pPlayer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const ICamera::SCameraPos& CPlayerTracker::GetCamera() const
{
	return sCamPlacement;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerTracker::SetCamera( const ICamera::SCameraPos &sPosition )
{
	sCamPlacement = sPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CCommander* CPlayerTracker::GetCommander() const
{
	return pCommander;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::CGlobalPlayer* CPlayerTracker::GetGlobalPlayer() const
{
	return pGlobalPlayer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerTracker::Update( bool bActive )
{
	list<NWorld::CUnit*> activeUnitsList;
	vector< CPtr<NWorld::CUnit> > playerUnitsSet;
	pPlayer->GetUnits( &playerUnitsSet );
	pMission->GetWorld()->GetActiveUnits( pPlayer, &activeUnitsList );

	int nCount = 0;
	vector< CObj<CUnitTracker> > newUnitsSet( playerUnitsSet.size() );
	for ( int nTemp = 0; nTemp < playerUnitsSet.size(); nTemp++ )
	{
		CObj<CUnitTracker> pNewUnit = 0;
		CPtr<NWorld::CUnit> pPlayerUnit = playerUnitsSet[nTemp];

		if ( pPlayerUnit->IsDead() )
			continue;

		for ( int nUnit = 0; nUnit < unitsSet.size(); nUnit++ )
		{
			if ( pPlayerUnit == unitsSet[nUnit]->GetUnit() )
				pNewUnit = unitsSet[nUnit];
		}

		if ( !IsValid( pNewUnit ) )
			pNewUnit = new CUnitTracker( pMission, pPlayerUnit );

		if ( !bActive || ( find( activeUnitsList.begin(), activeUnitsList.end(), playerUnitsSet[nTemp].GetPtr() ) != activeUnitsList.end() ) )
			pNewUnit->SetActive( true );
		else
			pNewUnit->SetActive( false );

		pNewUnit->Update();

		newUnitsSet[nCount] = pNewUnit;
		nCount++;
	}
	newUnitsSet.resize( nCount );

	if ( ( unitsSet.size() == 0 ) && ( newUnitsSet.size() != 0 ) )
		newUnitsSet[0]->SetSelected( true );

	unitsSet = newUnitsSet;

//	if ( bActive )
	{
		int nSelectedCount = 0;
		for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
		{
			if ( unitsSet[nTemp]->IsSelected() && !unitsSet[nTemp]->IsActive() && bActive )
				unitsSet[nTemp]->SetSelected( false );

			if ( unitsSet[nTemp]->IsSelected() )
				nSelectedCount++;
		}
		if ( nSelectedCount == 0 )
			Select( 1 );
//		if ( !pMission->IsRealTime() && ( nSelectedCount > 1 ) )
//			Select( 0 );
	}

	for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
		unitsSet[nTemp]->Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB2022800, CPlayerTracker )
