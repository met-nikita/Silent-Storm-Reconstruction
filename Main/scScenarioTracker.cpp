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
		// ��������� ����
		for ( vector< CPtr<CScenarioZone> >::const_iterator i = pObjective->GetZonesToBlock().begin();
			i != pObjective->GetZonesToBlock().end(); ++i )
		{
			BlockZone( *i );
		}
		// ��������� ����
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
		// ��������� ��������� clue
		for ( vector< CPtr<CScenarioClue> >::const_iterator i = pObjective->GetClues().begin();
			i != pObjective->GetClues().end(); ++i )
		{
			// ������� �������� �����
			int nCluesFound = 0;
			vector< CPtr<CScenarioObjective> >::const_iterator p;
			for ( p = (*i)->GetParentObjectives().begin(); p != (*i)->GetParentObjectives().end(); ++p )
			{
				if ( IsClueFound( (*p)->GetParentClue() ) )
					++nCluesFound;
			}
			// ���� �� ���������� ��� ��������� ���������� clue, �� ��������� ��� objectives
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
		// �����
		for ( int n = 0; n < NDb::N_SLOTS; ++n )
		{
			CPtr<NRPG::IInventoryItem> pItem = pInventory->Get( (NDb::ESlot)n );
			if ( IsValid( pItem ) && IsValid( pItem->GetDBItem() ) &&
				OnScenarioClueTaken( pItem->GetDBItem()->GetRecordID(), false ) )
					pInventory->TakeOff( (NDb::ESlot)n );
		}
		// ������
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
		// ����� :)
		CPtr<NRPG::IInventoryItem> pItem  = pInventory->GetHandItem();
		if ( IsValid( pItem ) && IsValid( pItem->GetDBItem() ) &&
			OnScenarioClueTaken( pItem->GetDBItem()->GetRecordID(), false ) )
		{
			pInventory->SetHandItem( 0 );
		}
	}
	//
	ExpandFlowChart();
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
// retail also clears a bHasCalcedCanLeaveZone cache (absent in this tracker) and falls back to a
// clue-attached goal via GetGoalByID -- both omitted (script goals live only in scriptGoals here).
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
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NScenario;
//
REGISTER_SAVELOAD_CLASS( 0x51582120, CScenarioTracker );