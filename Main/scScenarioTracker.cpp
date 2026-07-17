#include "StdAfx.h"
//
#include "wInterface.h"
#include "wUnitServer.h"
//
#include "RPGUnitMission.h"
#include "RPGMerc.h"
#include "RPGItem.h"
#include "RPGItemInfo.h"
#include "RPGGlobal.h"
//
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataScenario.h"
#include "..\DBFormat\DataConst.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataMap.h"
//
#include "..\MiscDll\LogStream.h"
//
#include "scFlowChart.h"
#include "scFlowChartItems.h"
#include "scScenarioTracker.h"
#include "scCommands.h"
//
namespace NScenario
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioTracker::CScenarioTracker():
	bScenarioAvailable( false ),
	cmdScenario( "scenario", CommandScenario, this ),
	cmdZone( "open_zone", CommandZone, this ),
	nZonesOpenOrder( 0 ), nCluesOpenOrder( 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CScenarioTracker::GetTemplateIDByVariantID( int nVariantID ) const
{
	if ( bScenarioAvailable )
		return pScenarioFlowChart->GetTemplateIDByVariantID( nVariantID );
	else
		return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SRandomSeed CScenarioTracker::GetRandomSeedForTemplate( int nTemplateID ) const
{
	if ( bScenarioAvailable )
	{
		CPtr<CScenarioZone> pZone = GetZone( nTemplateID );
		if ( IsValid( pZone ) )
			return pZone->GetRandomSeedForTemplate( nTemplateID );
	}
	//
	return SRandomSeed( GetTickCount() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CString *CScenarioTracker::GetClueDescriptionFromObjective( CScenarioClue *pClue ) const
{
	for ( vector< CPtr<CScenarioObjective> >::const_iterator i = pClue->GetObjectives().begin();
		i != pClue->GetObjectives().end(); ++i )
	{
		list< CPtr<CScenarioObjective> >::const_iterator f = find( finishedObjectives.begin(),
			finishedObjectives.end(), (*i).GetPtr() );
		if ( f != finishedObjectives.end() )
			return (*f)->GetDBObjective()->pDescription;
	}
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioZone *CScenarioTracker::GetZone( int nTemplateID ) const
{
	if ( bScenarioAvailable )
		return pScenarioFlowChart->GetZoneByTemplateID( nTemplateID );
	else
		return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioZone* CScenarioTracker::GetZoneByDBZone( NDb::CDBScenarioZone *pDBZone ) const
{
	if ( !bScenarioAvailable )
		return 0;
	//
	return pScenarioFlowChart->GetZoneByDBZone( pDBZone );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioZone *CScenarioTracker::GetZoneByName( string szName ) const
{
	if ( !bScenarioAvailable )
		return 0;
	//
	return pScenarioFlowChart->GetZoneByName( szName );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioClue *CScenarioTracker::GetClueByName( string szName ) const
{
	if ( !bScenarioAvailable )
		return 0;
	//
	return pScenarioFlowChart->GetClueByName( szName );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::BlockZone( CScenarioZone *pZone )
{
	ASSERT( IsValid( pZone ) );
	if ( !IsValid( pZone ) )
		return;
	//
	if ( !IsZoneBlocked( pZone ) )
		blockedZones.push_back( pZone );
	InvalidateLeaveZoneCache();	// retail @0x301070 (every retail tracker mutator clears the CanLeaveZone cache)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioTracker::IsZoneBlocked( CScenarioZone *pZone ) const
{
	ASSERT( IsValid( pZone ) );
	if ( !IsValid( pZone ) )
		return false;
	else
		return find( blockedZones.begin(), blockedZones.end(), pZone ) != blockedZones.end();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioTracker::IsZoneAvailable( CScenarioZone *pZone ) const
{
	ASSERT( IsValid( pZone ) );
	if ( !IsValid( pZone ) )
		return false;
	else
		return find( availableZones.begin(), availableZones.end(), pZone ) != availableZones.end();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::CheatTakeClue( CScenarioClue *pClue, bool bImmediately )
{
	ASSERT( IsValid( pClue ) );
	if ( IsValid( pClue ) )
	{
		if ( bImmediately )
		{
			list< CPtr<CScenarioClue> > cluesToProcess;
			cluesToProcess.push_back( pClue );
			ProcessCluesList( cluesToProcess, NDb::OT_CAPTURE );
		}
		else
		{
			takenClues.push_back( pClue );
		}
		InvalidateLeaveZoneCache();	// retail @0x301d60
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::CheatDestroyClue( CScenarioClue *pClue, bool bImmediately )
{
	ASSERT( IsValid( pClue ) );
	if ( IsValid( pClue ) )
	{
		if ( bImmediately )
		{
			list< CPtr<CScenarioClue> > cluesToProcess;
			cluesToProcess.push_back( pClue );
			ProcessCluesList( cluesToProcess, NDb::OT_DESTROY );
			pClue->SetDestroyed( true );
		}
		else
		{
			destroyedClues.push_back( pClue );
		}
		InvalidateLeaveZoneCache();	// retail @0x301d80
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::CheatOpenZone( CScenarioZone *pZone )
{
	ASSERT( pZone );
	if ( !IsValid( pZone ) )
		return;

	if ( !IsZoneAvailable( pZone ) && !IsZoneBlocked( pZone ) )
		availableZones.push_back( pZone );
	//
	pZone->SetPassed( true );
	//
	for ( vector< CPtr<CScenarioClue> >::const_iterator i = pZone->GetClues().begin(); 
		i != pZone->GetClues().end(); ++i )
	{
		if ( IsClueFound( *i ) )
			continue;

		JustFoundClue( *i );
		CPtr<CScenarioObjective> pTake = (*i)->GetObjectiveByType( NDb::OT_CAPTURE );
		CPtr<CScenarioObjective> pDestroy = (*i)->GetObjectiveByType( NDb::OT_DESTROY );
		if ( IsValid( pTake ) )
			OnObjectiveComplete( pTake );
		else if ( IsValid( pDestroy ) )
			OnObjectiveComplete( pDestroy );
	}
	InvalidateLeaveZoneCache();	// retail @0x301da0
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::JustFoundClue( CScenarioClue *pClue )
{
	++nCluesOpenOrder;
	pClue->SetJustFound( true );
	pClue->SetOpenOrder( nCluesOpenOrder );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::JustOpenZone( CScenarioZone *pZone )
{
	++nZonesOpenOrder;
	pZone->SetOpenOrder( nZonesOpenOrder );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::OnObjectiveComplete( CScenarioObjective *pObjective )
{
	if ( !IsValid( pObjective ) )
		return;
	//
	if ( IsValid( pObjective->GetParentClue() ) && !IsClueFound( pObjective->GetParentClue() ) )
	{
		finishedObjectives.push_back( pObjective );
		// block zones
		for ( vector< CPtr<CScenarioZone> >::const_iterator i = pObjective->GetZonesToBlock().begin();
			i != pObjective->GetZonesToBlock().end(); ++i )
		{
			BlockZone( *i );
		}
		// add zones
		for ( vector< CPtr<CScenarioZone> >::const_iterator i = pObjective->GetZones().begin();
			i != pObjective->GetZones().end(); ++i )
		{
			if ( !IsZoneAvailable( *i ) && !IsZoneBlocked( *i ) )
			{
				JustOpenZone( *i );
				availableZones.push_back( *i );
				csSystem << "Zone " << (*i)->GetDBZone()->sSmallDescription.c_str() << " was opened" << endl;
			}
		}
		// open compound clues
		for ( vector< CPtr<CScenarioClue> >::const_iterator i = pObjective->GetClues().begin();
			i != pObjective->GetClues().end(); ++i )
		{
			// count incoming links
			int nCluesFound = 0;
			vector< CPtr<CScenarioObjective> >::const_iterator p;
			for ( p = (*i)->GetParentObjectives().begin(); p != (*i)->GetParentObjectives().end(); ++p )
			{
				if ( IsClueFound( (*p)->GetParentClue() ) )
					++nCluesFound;
			}
			// if there are enough to obtain the compound clue, execute its objectives
			if ( nCluesFound >= pScenarioFlowChart->GetPathFinder()->GetMinParentToOpen( *i ) )
			{
				csSystem << "Compound clue " << (*i)->GetDBClue()->sSmallDescription.c_str() << " was given" << endl;
				JustFoundClue( *i );
				vector< CPtr<CScenarioObjective> >::const_iterator c;
				for ( c = (*i)->GetObjectives().begin(); c != (*i)->GetObjectives().end(); ++c )
					OnObjectiveComplete( *c );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::PostCreateScenario()
{
	if ( !IsValid( pScenarioFlowChart ) )
		return;
	//
	bScenarioAvailable = true;
	availableZones.clear();
	finishedObjectives.clear();
	blockedZones.clear();
	//
	CPtr<CScenarioZone> pBase = pScenarioFlowChart->GetZoneByName( "BASE" );
	if ( IsValid( pBase ) )
	{
		availableZones.push_back( pBase );
		//
		vector< CPtr<CScenarioClue> >::const_iterator i;
		for ( i = pBase->GetClues().begin(); i != pBase->GetClues().end(); ++i )
		{
			if ( (*i)->IsPlaced() && !(*i)->GetObjectives().empty() )
				OnObjectiveComplete( (*i)->GetObjectives().front() );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::CreateScenario( int nScenarioID )
{
	CObj<CScenarioFlowChart> pFlowChart = CreateScenarioFlowChart( nScenarioID, false );
	if ( IsValid( pFlowChart ) )
	{
		pScenarioFlowChart = pFlowChart;
		PostCreateScenario();
	}
	InvalidateLeaveZoneCache();	// retail @0x301460
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::CreateScenario( string szScenarioName )
{
	CObj<CScenarioFlowChart> pFlowChart = CreateScenarioFlowChart( szScenarioName, false );
	if ( IsValid( pFlowChart ) )
	{
		pScenarioFlowChart = pFlowChart;
		PostCreateScenario();
	}
	InvalidateLeaveZoneCache();	// retail @0x301500
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::GetAvailableZones( list<CPtr<CScenarioZone> > *pZones ) const
{
	pZones->clear();
	for ( list< CPtr<CScenarioZone> >::const_iterator i = availableZones.begin();	i != availableZones.end(); ++i )
	{
		if ( !IsZoneBlocked( *i ) )
			pZones->push_back( *i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::GetAvailableClues( list<CPtr<CScenarioClue> > *pClues ) const
{
	if ( !IsValid( pScenarioFlowChart ) )
		return;
	//
	pClues->clear();
	vector< CPtr<CScenarioClue> > clues;
	pScenarioFlowChart->GetClues( &clues );
	for ( vector< CPtr<CScenarioClue> >::const_iterator i = clues.begin(); i != clues.end(); ++i )
		if ( (*i)->IsPlaced() && !(*i)->IsDestroyed() && IsClueFound( *i ) )
			pClues->push_back( (*i).GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioTracker::IsObjectiveFinished( CScenarioObjective *pObjective ) const
{
	return find( finishedObjectives.begin(), 
		finishedObjectives.end(), pObjective ) !=	finishedObjectives.end();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioTracker::IsClueFound( CScenarioClue *pClue ) const
{
	if ( !IsValid( pClue ) )
		return false;
	//
	for ( vector< CPtr<CScenarioObjective> >::const_iterator i = pClue->GetObjectives().begin();
		i != pClue->GetObjectives().end(); ++i )
			if ( IsObjectiveFinished( *i ) )
				return true;
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioTracker::IsZoneContainsSomeClue( CScenarioZone *pZone ) const
{
	vector< CPtr<CScenarioClue> >::const_iterator i;
	for ( i = pZone->GetClues().begin(); i != pZone->GetClues().end(); ++i )
		if ( (*i)->IsPlaced() && !(*i)->IsDestroyed() && !IsClueFound( *i ) )
			return true;
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioZone *CScenarioTracker::GetRecommendedZone( NRPG::CGlobalPlayer *pPlayer ) const
{
	ASSERT( IsValid( pPlayer ) );
	if ( !IsValid( pPlayer ) )
		return 0;
	//
	float fMinDistance = 0xFFFF;
	CPtr<CScenarioZone> pZone = 0;
	float fAvrLevel = pPlayer->GetAverageLevel();
	list< CPtr<CScenarioZone> >::const_iterator i;
	for ( i = availableZones.begin(); i != availableZones.end(); ++i )
	{
		if ( !IsZoneBlocked( *i) && IsZoneContainsSomeClue( *i ) )
		{
			float fTmpDistance = (*i)->GetDifficulty() - fAvrLevel + 0.001;
			if ( fTmpDistance < fMinDistance )
			{
				fMinDistance = fTmpDistance;
				pZone = *i;
			}
		}
	}
	//
	return pZone;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::RevealZone( CScenarioZone *pZone )
{
	ASSERT( pZone );
	if ( !IsValid( pZone ) )
		return;
	//
	if ( pZone->GetDBZone()->bCanBeRevealed )
		OpenZone( pZone );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::OpenZone( CScenarioZone *pZone )
{
	ASSERT( pZone );
	if ( !IsValid( pZone ) )
		return;
	//
	if ( find( availableZones.begin(), availableZones.end(), pZone ) == availableZones.end() )
		availableZones.push_back( pZone );
	InvalidateLeaveZoneCache();	// retail @0x301770 (RevealZone @0x301f20 reaches this too)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::DrawScenario()
{
	if ( IsScenarioAvailable() )
	{
		list<CPtr<CScenarioZone> > zones;
		list<CPtr<CScenarioClue> > clues;
		GetAvailableZones( &zones );
		GetAvailableClues( &clues );
		pScenarioFlowChart->Draw( zones, clues );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::PrintScenarioList()
{
	csSystem << "Available scenarios:" << endl;
	CDBTable<NDb::CDBScenario> *pDBScenarioTable = NDatabase::GetTable<NDb::CDBScenario>();
	CDBIterator<NDb::CDBScenario> scenario(*pDBScenarioTable);
	while ( pDBScenarioTable && scenario.MoveNext() )
		csSystem << "\t" << scenario.Get()->szName << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::GetPlacedClues( NScenario::CScenarioZone *pZone, 
	int nTemplateID, list< CPtr<CScenarioClue> > *clues ) const
{
	ASSERT( clues != 0 );
	ASSERT( IsValid( pZone ) );
	if ( !IsValid( pZone ) )
		return;
	//
	if ( !bScenarioAvailable )
		return;
	//
	clues->clear();
	for ( vector< CPtr<CScenarioClue> >::const_iterator i = pZone->GetClues().begin();
		i != pZone->GetClues().end(); ++i )
			if ( (*i)->IsPlaced() && !(*i)->IsDestroyed() && (*i)->GetTemplateID() == nTemplateID )
				clues->push_back( *i );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioClue* CScenarioTracker::GetClueByPersID( int nPersID ) const
{
	if ( !bScenarioAvailable )
		return 0;
	//
	return pScenarioFlowChart->GetClueByPersID( nPersID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioTracker::OnScenarioClueTaken( int nID, bool bUnit )
{
	if ( !bScenarioAvailable )
		return false;
	//
	CPtr<CScenarioClue> pClue = 0;
	if ( bUnit )
		pClue = pScenarioFlowChart->GetClueByPersID( nID );
	else
		pClue = pScenarioFlowChart->GetClueByItemID( nID );
	//
	if ( IsValid( pClue ) )
	{
		if ( pClue->GetDBClue()->bGiveImmediately )
			CheatTakeClue( pClue, true );
		else
			takenClues.push_back( pClue );
		InvalidateLeaveZoneCache();	// retail @0x301f50
		return true;
	}
	else
		return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::OnScenarioClueDestroyed( int nID, bool bUnit )
{
	if ( !bScenarioAvailable )
		return;
	//
	CPtr<CScenarioClue> pClue = 0;
	if ( bUnit )
		pClue = pScenarioFlowChart->GetClueByPersID( nID );
	else
		pClue = pScenarioFlowChart->GetClueByItemID( nID );
	if ( IsValid( pClue ) )
	{
		if ( pClue->GetDBClue()->bGiveImmediately )
			CheatDestroyClue( pClue, true );
		else
			destroyedClues.push_back( pClue );
		InvalidateLeaveZoneCache();	// retail @0x303880
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::ProcessCluesList( const list< CPtr<CScenarioClue> > &clues,
	NDb::EScenarioObjectiveType type )
{
	for ( list< CPtr<CScenarioClue> >::const_iterator i = clues.begin(); i != clues.end(); ++i )
	{
		CPtr<CScenarioObjective> pObjective = (*i)->GetObjectiveByType( type );
		if ( IsValid( pObjective ) )
		{
			JustFoundClue( *i );
			OnObjectiveComplete( pObjective );
		}
		//
		if ( type == NDb::OT_DESTROY )
		{
			(*i)->SetDestroyed( true );
			csSystem << "Clue " << (*i)->GetDBClue()->sSmallDescription.c_str() << " was destroyed" << endl;
		}
		else
			csSystem << "Clue " << (*i)->GetDBClue()->sSmallDescription.c_str() << " was taken" << endl;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::ExpandFlowChart()
{
	ProcessCluesList( takenClues, NDb::OT_CAPTURE );
	ProcessCluesList( destroyedClues, NDb::OT_DESTROY );
	takenClues.clear();
	destroyedClues.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioZone *CScenarioTracker::GetZoneInWhichClueWasFound( CScenarioClue *pClue ) const
{
	ASSERT( IsValid( pClue ) );
	if ( !IsValid( pClue ) )
		return 0;
	//
	if ( pClue->IsCompound() )
		return 0;
	ASSERT( !pClue->GetParentZones().empty() );
	if ( pClue->GetParentZones().empty() )
		return 0;
	//
	return pClue->GetParentZones()[0];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::GetZonesWhichCanBeOpened( CScenarioClue *pClue, 
	list< CPtr<CScenarioZone> > *pZones ) const
{
	ASSERT( IsValid( pClue ) );
	ASSERT( pZones != 0 );
	if ( !IsValid( pClue ) || pZones == 0 )
		return;
	//
	pScenarioFlowChart->GetZonesWhichCanBeOpened( pClue, pZones );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::ProcessScenario( const vector< CPtr<NRPG::CUnit> > &units )
{
	if ( !bScenarioAvailable )
		return;
	//
	for ( vector< CPtr<NRPG::CUnit> >::const_iterator i = units.begin();
		i != units.end(); ++i )
			OnScenarioClueTaken( (*i)->GetRPGPersID(), true );
	//
	for ( vector< CPtr<NRPG::CUnit> >::const_iterator i = units.begin();
		i != units.end(); ++i )
	{
		CPtr<NRPG::IInventory> pInventory = (*i)->pInventory;
		// slots
		for ( int n = 0; n < NDb::N_SLOTS; ++n )
		{
			CPtr<NRPG::IInventoryItem> pItem = pInventory->Get( (NDb::ESlot)n );
			if ( IsValid( pItem ) && IsValid( pItem->GetDBItem() ) &&
				OnScenarioClueTaken( pItem->GetDBItem()->GetRecordID(), false ) )
					pInventory->TakeOff( (NDb::ESlot)n );
		}
		// backpack
		const vector<NRPG::SBackPackItem> &items = pInventory->GetItems();
		vector<NRPG::SBackPackItem> itemsToRemove;
		for ( vector<NRPG::SBackPackItem>::const_iterator b = items.begin(); b != items.end(); ++b )
		{
			ASSERT( IsValid( (*b).pItem ) );
			ASSERT( IsValid( (*b).pItem->GetDBItem() ) );
			if ( IsValid( (*b).pItem ) && IsValid( (*b).pItem->GetDBItem() ) &&
				OnScenarioClueTaken( (*b).pItem->GetDBItem()->GetRecordID(), false ) )
			{
				itemsToRemove.push_back( *b );
			}
		}
		//
		for ( vector<NRPG::SBackPackItem>::iterator b = itemsToRemove.begin(); b != itemsToRemove.end(); ++b )
		{
			pInventory->Take( (*b).pItem );
		}
		// The Jan03-era in-hand clue leg that used to sit here is GONE, and its absence is retail's:
		// retail restructured this whole function (@0x303000) onto GetCluesFromPers @0x302a20 +
		// ProcessCluesList, and GetCluesFromPers scans ONLY the slots (Get, vtbl+0x20) and the
		// backpack (GetItems, vtbl+0x18) -- there is no hand scan anywhere in it. It could not have
		// one: this pass is handed NRPG::CUnit (RPG-side) with no CUnitServer/CPlayer to reach a
		// hand through, and retail's CInventory has no hand member. So no behaviour is lost here.
		// (The rest of this function is still the Jan03 shape -- e.g. it scans all N_SLOTS where
		// retail scans only slots 0..1. That divergence pre-dates this change and is left alone.)
	}
	//
	ExpandFlowChart();
	InvalidateLeaveZoneCache();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x302a20
void CScenarioTracker::GetCluesFromPers( NRPG::CUnit *pPers, NRPG::CUnit *pCorpsePers,
	list< CPtr<CScenarioClue> > *pClues, bool bTake ) const
{
	if ( !IsValid( pPers ) )
		return;
	// the pers himself is a clue while alive
	CPtr<CScenarioClue> pClue = bScenarioAvailable ? pScenarioFlowChart->GetClueByPersID( pPers->GetRPGPersID() ) : 0;
	if ( IsValid( pClue ) && !pPers->IsDead() )
		pClues->push_back( pClue );
	//
	CPtr<NRPG::IInventory> pInventory = pPers->pInventory;
	// hand slots (retail scans slots 0..1 only)
	for ( int n = 0; n < NDb::N_SLOTS; ++n )
	{
		CPtr<NRPG::IInventoryItem> pItem = pInventory->Get( (NDb::ESlot)n );
		if ( IsValid( pItem ) && IsValid( pItem->GetDBItem() ) )
		{
			pClue = bScenarioAvailable ? pScenarioFlowChart->GetClueByItemID( pItem->GetDBItem()->GetRecordID() ) : 0;
			if ( IsValid( pClue ) )
			{
				pClues->push_back( pClue );
				if ( bTake )
					pInventory->TakeOff( (NDb::ESlot)n );
			}
		}
	}
	// backpack
	const vector<NRPG::SBackPackItem> &items = pInventory->GetItems();
	vector<NRPG::SBackPackItem> itemsToRemove;
	for ( vector<NRPG::SBackPackItem>::const_iterator b = items.begin(); b != items.end(); ++b )
	{
		if ( IsValid( (*b).pItem ) && IsValid( (*b).pItem->GetDBItem() ) )
		{
			pClue = bScenarioAvailable ? pScenarioFlowChart->GetClueByItemID( (*b).pItem->GetDBItem()->GetRecordID() ) : 0;
			if ( IsValid( pClue ) )
			{
				pClues->push_back( pClue );
				if ( bTake )
					itemsToRemove.push_back( *b );
			}
		}
	}
	for ( vector<NRPG::SBackPackItem>::iterator b = itemsToRemove.begin(); b != itemsToRemove.end(); ++b )
		pInventory->Take( (*b).pItem );
	// clues on the carried corpse count too (never taken off it here)
	if ( IsValid( pCorpsePers ) )
		GetCluesFromPers( pCorpsePers, 0, pClues, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x302e40
void CScenarioTracker::GetCluesInHand( const vector< CPtr<NWorld::CUnit> > &units,
	list< CPtr<CScenarioClue> > *pClues ) const
{
	pClues->clear();
	for ( vector< CPtr<NWorld::CUnit> >::const_iterator i = units.begin(); i != units.end(); ++i )
	{
		CPtr<NRPG::CUnit> pPers = (*i)->GetRPG()->GetRPGUnit();
		NRPG::CUnit *pCorpsePers = 0;
		NWorld::CUnitServer *pServer = CDynamicCast<NWorld::CUnitServer>( (*i).GetPtr() );
		if ( pServer && IsValid( pServer->GetCorpse() ) )
			pCorpsePers = pServer->GetCorpse()->GetUnitRPG()->GetRPGUnit();
		GetCluesFromPers( pPers, pCorpsePers, pClues, false );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3030b0
bool CScenarioTracker::CanLeaveZone( const vector< CPtr<NWorld::CUnit> > &units,
	CScenarioZone *pZone, bool *pbGameOver )
{
	if ( !bScenarioAvailable )
		return true;
	//
	list< CPtr<CScenarioClue> > cluesInHand;
	GetCluesInHand( units, &cluesInHand );
	// the zone's placed clues not yet found and not in hand -- leaving would strand them
	list< CPtr<CScenarioClue> > cluesToFind;
	for ( vector< CPtr<CScenarioClue> >::const_iterator i = pZone->GetClues().begin(); i != pZone->GetClues().end(); ++i )
	{
		bool bFound = IsClueFound( *i );
		bool bInHand = find( cluesInHand.begin(), cluesInHand.end(), *i ) != cluesInHand.end();
		if ( (*i)->IsPlaced() && !bFound && !bInHand )
			cluesToFind.push_back( *i );
	}
	// unchanged inputs -> cached result (retail compares all three lists)
	if ( bHasCalcedCanLeaveZone && prevCluesToFind == cluesToFind &&
		prevCluesInHands == cluesInHand && prevDestroyedClues == destroyedClues )
	{
		*pbGameOver = bPrevGameOver;
		return bPrevCanLeaveZone;
	}
	// can leave = BASE->FFIGHT still reachable with this zone's unfound clues hidden
	for ( list< CPtr<CScenarioClue> >::iterator i = cluesToFind.begin(); i != cluesToFind.end(); ++i )
		(*i)->SetPlaced( false );
	CObj<CScenarioFlowChartState> pPath = pScenarioFlowChart->GetPathFinder()->FindPath(
		GetZoneByName( "BASE" ), GetZoneByName( "FFIGHT" ), true );
	bool bCanLeave = IsValid( pPath );
	for ( list< CPtr<CScenarioClue> >::iterator i = cluesToFind.begin(); i != cluesToFind.end(); ++i )
		(*i)->SetPlaced( true );
	// game over = unreachable even hiding only the DESTROYED unfound clues
	vector< CPtr<CScenarioClue> > destroyedToHide;
	for ( list< CPtr<CScenarioClue> >::iterator i = cluesToFind.begin(); i != cluesToFind.end(); ++i )
	{
		if ( !IsValid( *i ) || !(*i)->IsPlaced() || IsClueFound( *i ) )
			continue;
		if ( (*i)->IsDestroyed() || find( destroyedClues.begin(), destroyedClues.end(), *i ) != destroyedClues.end() )
		{
			destroyedToHide.push_back( *i );
			(*i)->SetPlaced( false );
		}
	}
	pPath = pScenarioFlowChart->GetPathFinder()->FindPath( GetZoneByName( "BASE" ), GetZoneByName( "FFIGHT" ), true );
	bool bGameOver = !IsValid( pPath );
	for ( vector< CPtr<CScenarioClue> >::iterator i = destroyedToHide.begin(); i != destroyedToHide.end(); ++i )
		(*i)->SetPlaced( true );
	//
	bPrevGameOver = bGameOver;
	*pbGameOver = bGameOver;
	bPrevCanLeaveZone = bCanLeave;
	bHasCalcedCanLeaveZone = true;
	prevCluesToFind = cluesToFind;
	prevCluesInHands = cluesInHand;
	prevDestroyedClues = destroyedClues;
	return bCanLeave;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CScenarioTracker::GetScenarioID() const
{
	if ( !bScenarioAvailable )	// retail @0x300230: no flow chart yet (e.g. a mission loaded directly by
		return 0;				// template) -> return 0 instead of dereferencing the null pScenarioFlowChart
	return pScenarioFlowChart->GetScenarioID();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CScenarioTracker::GetMaxDifficulty() const
{
	int nMaxDif = 0;
	list< CPtr<CScenarioZone> > zones;
	GetAvailableZones( &zones );
	for ( list< CPtr<CScenarioZone> >::const_iterator i = zones.begin(); i != zones.end(); ++i )
		nMaxDif = Max( nMaxDif, (*i)->GetDifficulty() );
	return nMaxDif;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioTracker *CreateScenarioTracker( int nID )
{
	CScenarioTracker *pScenario = new CScenarioTracker();
	if ( nID >= 0 )
		pScenario->CreateScenario( nID );
	return pScenario;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CSide *GetSideForScenario( CScenarioTracker *pScenario )
{
	ASSERT( IsValid( pScenario ) );
	if ( !IsValid( pScenario ) )
		return 0;
	if ( !pScenario->IsScenarioAvailable() )
		return 0;
	//
	int nScenarioID = pScenario->GetScenarioID();
	//
	CDBTable<NDb::CSide> *pSidesTable = NDatabase::GetTable<NDb::CSide>();
	CDBIterator<NDb::CSide> side(*pSidesTable);
	while ( pSidesTable && side.MoveNext() )
	{
		CDBPtr<NDb::CSide> pSide = side.Get();
		if ( IsValid( pSide ) )
		{
			CDBPtr<NDb::CGlobalMap> pGlobalMap = NDb::GetGlobalMap( pSide->nGlobalMapID );
			if ( IsValid( pGlobalMap ) )
			{
				if ( pGlobalMap->pScenario->GetRecordID() == nScenarioID )
					return pSide;
			}
		}
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// script-goal/task API (retail @0x302990 / @0x300720 / @0x3007a0). Goals are appended to the zone's
// scriptGoals; their completion state is set on the runtime CScenarioGoal/CScenarioTask. ELISION: the
// retail falls back to a clue-attached goal via GetGoalByID -- omitted (script goals live only in
// scriptGoals here). The bHasCalcedCanLeaveZone cache-clear IS ported (InvalidateLeaveZoneCache).
////////////////////////////////////////////////////////////////////////////////////////////////////
// First scriptGoal in the zone whose DB goal record id matches nGoalID, or null.
static CScenarioGoal* FindScriptGoal( CScenarioZone *pZone, int nGoalID )
{
	const vector< CObj<CScenarioGoal> > &goals = pZone->GetScriptGoals();
	for ( int i = 0; i < goals.size(); ++i )
	{
		CScenarioGoal *pGoal = goals[ i ];
		if ( IsValid( pGoal ) && IsValid( pGoal->GetDBGoal() ) && pGoal->GetDBGoal()->GetRecordID() == nGoalID )
			return pGoal;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTracker::AddScriptGoal( CScenarioZone *pZone, int nGoalID )
{
	if ( !IsValid( pZone ) )
		return;
	NDb::CScenarioGoal *pTmpl = NDb::GetScenarioGoal( nGoalID );
	if ( IsValid( pZone ) && IsValid( pTmpl ) )
		pZone->AddScriptGoal( new CScenarioGoal( pTmpl ) );
	InvalidateLeaveZoneCache();	// retail @0x302990
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioTracker::ScriptGoalSetComplete( CScenarioZone *pZone, int nGoalID, bool bComplete )
{
	if ( !IsValid( pZone ) )
		return false;
	CScenarioGoal *pGoal = FindScriptGoal( pZone, nGoalID );
	if ( !IsValid( pGoal ) )
		return false;
	pGoal->SetState( bComplete ? TS_COMPLETED : TS_FAILED );
	InvalidateLeaveZoneCache();	// retail @0x300720
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioTracker::ScriptTaskSetComplete( CScenarioZone *pZone, int nGoalID, int nTaskIdx, bool bComplete )
{
	if ( !IsValid( pZone ) )
		return false;
	CScenarioGoal *pGoal = FindScriptGoal( pZone, nGoalID );
	if ( !IsValid( pGoal ) )
		return false;
	if ( nTaskIdx < 0 || nTaskIdx >= (int)pGoal->GetTasks().size() )
		return false;
	pGoal->GetTasks()[ nTaskIdx ]->SetState( bComplete ? TS_COMPLETED : TS_FAILED );
	InvalidateLeaveZoneCache();	// retail @0x3007a0
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NScenario;
//
REGISTER_SAVELOAD_CLASS( 0x51582120, CScenarioTracker );