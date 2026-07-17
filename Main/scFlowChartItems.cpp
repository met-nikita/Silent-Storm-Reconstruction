#include "StdAfx.h"
//
#include "..\DBFormat\DataScenario.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
//
#include "MapBuild.h"
//
#include "scFlowChartItems.h"
#include <fstream>
//
namespace NScenario
{
////////////////////////////////////////////////////////////////////////////////////////////////////
template< class T >
static void AddVectorUniqueItem( vector< CPtr<T> > *pVector, T *pItem )
{
	ASSERT( pVector != 0 );
	ASSERT( IsValid( pItem ) );
	if ( !IsValid( pItem ) || pVector == 0 )
		return;
	if ( find( pVector->begin(), pVector->end(), pItem ) != pVector->end() )
		return;
	pVector->push_back( pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template< class T >
static void RemoveVectorItem( vector< CPtr<T> > *pVector, T *pItem )
{
	ASSERT( pVector != 0 );
	ASSERT( IsValid( pItem ) );
	if ( !IsValid( pItem ) || pVector == 0 )
		return;
	vector< CPtr<T> >::iterator i = find( pVector->begin(), pVector->end(), pItem );
	if ( i != pVector->end() )
		pVector->erase( i );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioZone
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioZone::CScenarioZone( NDb::CDBScenarioZone *_pDBZone, int _nInnerID ):
	pDBZone( _pDBZone ), nDifficulty( 0 ), bDifCalculated( false ), bInaccessible( true ), 
	nDistance( 0 ), nInnerID( _nInnerID ), nOpenOrder( 0 ),	bInitial( true ), 
	bInShortestPath( false ), bPassed( false )
{
	ASSERT( IsValid( pDBZone ) );
	//
	for (	vector<int>::iterator i = pDBZone->templatesIDs.begin(); 
		i != pDBZone->templatesIDs.end(); ++i )
	{
		if ( (*i) == 0 )
			continue;
		//
		STemplate t;
		GetVariantInfo( *i, t.sSeed, &t );
		templates[ *i ] = t;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CScenarioZone::GetDefaultTemplateID()
{
	if ( pDBZone->templatesIDs.empty() )
		return -1;

	return pDBZone->templatesIDs.front();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CScenarioZone::GetTemplateIDByVariantID( int nVariantID )
{
	for ( unordered_map< int, STemplate >::iterator i = templates.begin();
		i != templates.end(); ++i )
			if ( i->second.nVariantID == nVariantID )
				return i->first;
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioZone::GetVariantInfo( int nTemplateID, SRandomSeed sSeed, STemplate *variantData )
{
	ASSERT( variantData != 0 )	;
	ASSERT( nTemplateID > 0 );
	if ( variantData == 0 || nTemplateID <= 0 )
		return;
	//
	CDBPtr<NDb::CTemplate> pTemplate = NDb::GetTemplate( nTemplateID );
	if ( !IsValid( pTemplate ) )
		return;
	//
	SRand rand( sSeed );
	vector<int> intParams;
	int nVariantID = NDb::GetTemplVariant( pTemplate, intParams, -1, &rand )->GetRecordID();
	variantData->nVariantID = nVariantID;
	//
	if ( GetDBZone()->templatesIDs[1] == 0 )
	{
		variantData->nItemSlots = GetDBZone()->nItemSlots;
		variantData->nEmptyItemSlots = GetDBZone()->nItemSlots;
		variantData->nPersonSlots = GetDBZone()->nPersonSlots;
		variantData->nEmptyPersonSlots = GetDBZone()->nPersonSlots;
		variantData->nInventorySlots = 0;
		variantData->nEmptyInventorySlots = 0;
		return;
	}
	//
	SMapInfo info;
	vector<string> strParams;
	if ( BuildMap( nVariantID, strParams, 0, &info, -1, sSeed ) )
	{
		int nItemSlots = 0;
		int nPersonSlots = 0;
		int nInventorySlots = 0;
		for ( vector<SClueSlot>::iterator i = info.slots.begin(); i != info.slots.end(); ++i )
		{
			if ( (*i).bPersSlot )
			{
				++nPersonSlots;
				if ( (*i).bInventorySlot )
					++nInventorySlots;
			}
			else
				++nItemSlots;
		}
		//
		variantData->nItemSlots = nItemSlots;
		variantData->nEmptyItemSlots = nItemSlots;
		variantData->nPersonSlots = nPersonSlots;
		variantData->nEmptyPersonSlots = nPersonSlots;
		variantData->nInventorySlots = nInventorySlots;
		variantData->nEmptyInventorySlots = nInventorySlots;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioZone::CanPlaceClue( NDb::EScenarioClueType type )
{
	int nPlaced = 0;
	int nPlacedAll = 0;
	for ( vector< CPtr<CScenarioClue> >::iterator clue = clues.begin(); clue != clues.end(); ++clue )
	{
		if ( !(*clue)->GetDBClue()->bPermanent )
		{
		++nPlacedAll;
		if ( (*clue)->GetDBClue()->clueType == type )
			++nPlaced;
		}
	}
	//
	int nSpace = 0;
	if ( type == NDb::CT_ITEM )
		nSpace = GetDBZone()->nItemSlots;
	else if ( type == NDb::CT_PERSON )
		nSpace = GetDBZone()->nPersonSlots;
	//else
	//	ASSERT( 0 && " Unknown clue type " );
	//
	return nPlaced < nSpace && nPlacedAll < GetDBZone()->nCluesMaxNumber;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CScenarioZone::GetTemplateIDForClue( CScenarioClue *pClue )
{
	ASSERT( IsValid( pClue ) );
	if ( !IsValid( pClue ) )
		return 0;
	//
	vector<int> &templatesIDs = GetDBZone()->templatesIDs;
	//
	NDb::EScenarioClueType type = pClue->GetDBClue()->clueType;
	int nMax = 0;
	for ( int i = 0; i < templatesIDs.size(); ++i )
		if ( templatesIDs[i] > 0 )
		{
			if ( type == NDb::CT_PERSON )
				nMax += templates[ templatesIDs[i] ].nEmptyPersonSlots;
			else
				nMax += templates[ templatesIDs[i] ].nEmptyItemSlots;
		}
	//
	if ( nMax <= 0 )
		return 0;
	//
	int k = 0;
	int n = random.Get( 0, nMax );
	while ( n > 0 )
	{
		if ( templatesIDs[k] > 0 )
		{
			if ( type == NDb::CT_PERSON )
				n -= templates[ templatesIDs[k] ].nEmptyPersonSlots;
			else
				n -= templates[ templatesIDs[k] ].nEmptyItemSlots;
		}
		if ( n >= 0 )
			++k;
	}
	//
	ASSERT( k >= 0 && k < templatesIDs.size() );
	if ( !( k >= 0 && k < templatesIDs.size() ) )
		return 0;
	//
	if ( type == NDb::CT_PERSON )
		--templates[ templatesIDs[k] ].nEmptyPersonSlots;
	else
		--templates[ templatesIDs[k] ].nEmptyItemSlots;
	//
	return templatesIDs[k];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioZone::PlaceClue( CScenarioClue *pClue )
{
	ASSERT( IsValid( pClue ) );
	//
	clues.push_back( pClue );
	pClue->SetPlaced();
	pClue->SetTemplateID( GetTemplateIDForClue( pClue ) );
	//
	ASSERT( find( pClue->GetParentZones().begin(), pClue->GetParentZones().end(), this ) ==
		pClue->GetParentZones().end() );
	//
	pClue->AddParentZone( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioZone::RemoveClue( CScenarioClue *pClue )
{
	ASSERT( IsValid( pClue ) );
	if ( !IsValid( pClue ) )
		return;
	//
	int nTemplateID = pClue->GetTemplateID();
	NDb::EScenarioClueType type = pClue->GetDBClue()->clueType;
	if ( type == NDb::CT_PERSON )
		++templates[ nTemplateID ].nEmptyPersonSlots;
	else
		++templates[ nTemplateID ].nEmptyItemSlots;
	//
	RemoveVectorItem( &clues, pClue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioZone::AddBlocker( CScenarioObjective *pBlocker )
{
	AddVectorUniqueItem( &blockers, pBlocker );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioZone::DrawNode( fstream &file, int nID, bool bAvailable, bool bDrawBlockers )
{
	file << "  " << pDBZone->sSmallDescription;
	if ( nID > 0 )
		file << "_" << nID;
	file << "[shape = ";
		file << "box";
	if ( bAvailable )
		file << ", fontcolor=darkgreen";
	if ( bInaccessible || nID > 0 )
		file << ", style=dotted";
	else if ( IsInShortestPath() )
		file << ", style=filled";
	file << ", label = \"";
	if ( !bInaccessible && nDistance > 0 )
		file << nDistance << "\\n";
	file << pDBZone->sSmallDescription << "\\n" << nDifficulty;
	//
	if ( bDrawBlockers )
	{
		if ( !blockers.empty() )
			file << "\\nBlockers:\\n";
		for ( vector< CPtr<CScenarioObjective> >::iterator i = blockers.begin();
			i != blockers.end(); ++i )
				file << (*i)->GetParentClue()->GetDBClue()->sSmallDescription << "\\n";
	}
	//
	file << "\"];" << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioZone::DrawRelationship( fstream &file )
{
	for ( vector< CPtr<CScenarioClue> >::iterator clue = clues.begin(); clue != clues.end(); ++clue )
		if ( (*clue)->IsPlaced() )
			file << "  " << pDBZone->sSmallDescription << " -> " << (*clue)->GetDBClue()->sSmallDescription << ";" << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioZone::SetCluesInaccessible( bool _bInaccessible )
{
	for ( vector< CPtr<CScenarioClue> >::iterator clue = clues.begin(); clue != clues.end(); ++clue )
		(*clue)->SetInaccessible( _bInaccessible );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioZone::GetTemplatesIDs( vector<int> *pIDs )
{
	ASSERT( pIDs != 0 );
	if ( pIDs == 0 )
		return;
	//
	pIDs->clear();
	for ( unordered_map< int, STemplate >::iterator i = templates.begin(); i != templates.end(); ++i )
		pIDs->push_back( i->first );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CScenarioZone::GetVariantIDForTemplate( int nTemplateID )
{
	ASSERT( nTemplateID > 0 );
	if ( nTemplateID <= 0 )
		return 0;
	//
	return templates[nTemplateID].nVariantID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioClue
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioClue::CScenarioClue( NDb::CDBScenarioClue *_pDBClue, int _nInnerID ):
	pDBClue( _pDBClue ), bPlaced( false ), bCompound( false ),
	bInaccessible( true ), bJustFound( false ), nOpenOrder( 0 ),
	bInShortestPath( false ),	nInnerID( _nInnerID ), bDestroyed( false ),
	bDetected( false )
{
	ASSERT( IsValid( pDBClue ) );
	// retail @0x2e0770 tail: a clue whose DB record carries a goal gets its runtime goal built
	// right in the ctor, with this clue as the tasks' parent.
	if ( IsValid( pDBClue ) && IsValid( pDBClue->pGoal ) )
		pGoal = new CScenarioGoal( pDBClue->pGoal, this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioObjective *CScenarioClue::GetObjectiveByType( NDb::EScenarioObjectiveType type )
{
	for (	vector< CPtr<CScenarioObjective> >::iterator i = objectives.begin();
		i != objectives.end(); ++i )
			if ( (*i)->GetDBObjective()->type == type )
				return *i;
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioClue::CanPlaceObjective( CScenarioObjective *pObjective )
{
	ASSERT( IsValid( pObjective ) );
	//
	for ( vector< CPtr<NDb::CDBScenarioObjective> >::iterator objective = pDBClue->objectives.begin();
		objective != pDBClue->objectives.end(); ++objective )
			if ( *objective == pObjective->GetDBObjective() )
				return TRUE;
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioClue::PlaceObjective( CScenarioObjective *pObjective )
{
	ASSERT( IsValid( pObjective ) );
	ASSERT( !pObjective->IsPlaced() );
	ASSERT( CanPlaceObjective( pObjective ) );
	//
	objectives.push_back( pObjective );
	pObjective->SetPlaced();
	pObjective->SetParentClue( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioClue::DrawNode( fstream &file, bool bAvailable )
{
	file << "  " << pDBClue->sSmallDescription << "[shape = ellipse";
	if ( bAvailable )
		file << ", fontcolor=darkgreen";
	else if ( IsDestroyed() )
		file << ", fontcolor=red";
	if ( bInaccessible )
		file << ", style=dotted";
	else if ( IsInShortestPath() )
		file << ", style=filled";
	file << ", label = \"" << pDBClue->sSmallDescription;
	file << "\"];" << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioClue::AddChildObjective( CScenarioObjective *pObjective )
{
	AddVectorUniqueItem( &objectives, pObjective );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioClue::AddParentObjective( CScenarioObjective *pObjective )
{
	AddVectorUniqueItem( &parentObjectives, pObjective );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioClue::AddParentZone( CScenarioZone *pZone )
{
	AddVectorUniqueItem( &parentZones, pZone );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioClue::IsCorrect()
{
	for (	vector< CPtr<CScenarioObjective> >::iterator objective = objectives.begin();
		objective != objectives.end(); ++objective )
			if ( (*objective)->IsCorrect() )
				return true;
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioClue::DrawRelationship( fstream &file )
{
	for (	vector< CPtr<CScenarioObjective> >::iterator objective = objectives.begin();
		objective != objectives.end(); ++objective )
		if ( (*objective)->IsCorrect() )
		{
			for ( vector< CPtr<CScenarioClue> >::const_iterator clue = (*objective)->GetClues().begin(); 
					clue != (*objective)->GetClues().end(); ++clue )
				if ( (*clue)->IsPlaced() )
					file << "  " << pDBClue->sSmallDescription << " -> " << (*clue)->GetDBClue()->sSmallDescription << endl;
			//
			for ( vector< CPtr<CScenarioZone> >::const_iterator zone = (*objective)->GetZones().begin(); 
				zone != (*objective)->GetZones().end(); ++zone )
					file << "  " << pDBClue->sSmallDescription << " -> " << (*zone)->GetDBZone()->sSmallDescription << endl;
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioClue::RemoveParentObjective( CScenarioObjective *pObjective )
{
	RemoveVectorItem( &parentObjectives, pObjective );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioClue::RemoveChildObjective( CScenarioObjective *pObjective )
{
	pObjective->SetPlaced( false );
	pObjective->ClearZones();
	//
	for ( vector< CPtr<CScenarioClue> >::const_iterator clue = pObjective->GetClues().begin();
		clue != pObjective->GetClues().end(); ++clue )
			(*clue)->RemoveParentObjective( pObjective );
	pObjective->ClearClues();
	//
	RemoveVectorItem( &objectives, pObjective );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioClue::ClearLinks()
{
	// child objectives
	while ( !objectives.empty() )
		RemoveChildObjective( objectives.front() );
	// parent zones
	for ( vector< CPtr<CScenarioZone> >::iterator i = parentZones.begin();
		i != parentZones.end(); ++i )
			(*i)->RemoveClue( this );
	// parent clues
	while ( !parentObjectives.empty() )
	{
		parentObjectives.front()->RemoveClue( this );
		RemoveParentObjective( parentObjectives.front() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioObjective
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioObjective::CScenarioObjective( NDb::CDBScenarioObjective *_pDBObjective, int _nInnerID ):
	pDBObjective( _pDBObjective ), bPlaced( false ), nInnerID( _nInnerID )
{
	ASSERT( IsValid( pDBObjective ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioObjective::RemoveZone( CScenarioZone *pZone )
{
	RemoveVectorItem( &zones, pZone );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioObjective::RemoveClue( CScenarioClue *pClue )
{
	RemoveVectorItem( &clues, pClue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioObjective::AddChildZone( CScenarioZone *pZone )
{
	AddVectorUniqueItem( &zones, pZone );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioObjective::AddChildClue( CScenarioClue *pClue )
{
	AddVectorUniqueItem( &clues, pClue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioObjective::AddPossibleParentClue( CScenarioClue *pClue )
{
	AddVectorUniqueItem( &possibleParentClues, pClue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioObjective::AddZoneToBlock( CScenarioZone *pZone )
{
	AddVectorUniqueItem( &zonesToBlock, pZone );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioZone *CreateScenarioZone( NDb::CDBScenarioZone *pDBZone, int nInnerID )
{
	return new CScenarioZone( pDBZone, nInnerID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioClue *CreateScenarioClue( NDb::CDBScenarioClue *pDBClue, int nInnerID )
{
	return new CScenarioClue( pDBClue, nInnerID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioObjective *CreateScenarioObjective( NDb::CDBScenarioObjective *pDBObjective, int nInnerID )
{
	return new CScenarioObjective( pDBObjective, nInnerID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioGoal -- clone the DB goal's tasks into runtime tasks (each starting TS_UNKNOWN).
// retail @0x2e0550: the parent clue (null for script-created goals) is threaded into each cloned
// task's pParentClue.
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioGoal::CScenarioGoal( NDb::CScenarioGoal *_pGoal, CScenarioClue *pParentClue ): pGoal( _pGoal ), eState( TS_UNKNOWN )
{
	if ( IsValid( _pGoal ) )
	{
		const vector< CPtr<NDb::CScenarioTask> > &dbTasks = _pGoal->tasks;
		for ( int i = 0; i < dbTasks.size(); ++i )
			tasks.push_back( new CScenarioTask( dbTasks[ i ], pParentClue ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NScenario;
//
REGISTER_SAVELOAD_CLASS( 0x50982101, CScenarioZone );
REGISTER_SAVELOAD_CLASS( 0x50982102, CScenarioClue );
REGISTER_SAVELOAD_CLASS( 0x50982103, CScenarioObjective );
REGISTER_SAVELOAD_CLASS( 0xA0243160, CScenarioGoal );
REGISTER_SAVELOAD_CLASS( 0xA1733130, CScenarioTask );