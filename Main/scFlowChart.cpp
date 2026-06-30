#include "StdAfx.h"
//
#include "..\MiscDll\Commands.h"
#include "..\Misc\RandomGen.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\LogStream.h"
//
#include "..\DBFormat\DataScenario.h"
//
#include "scFlowChartItems.h"
#include "scFlowChart.h"
//
#include <fstream>
//
namespace NScenario
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioFlowChartBase
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioFlowChartBase::CScenarioFlowChartBase( int _nScenarioID, bool _bFull ):
	nScenarioID( _nScenarioID ), bFull( _bFull )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioFlowChartBase::CheckDataCorrectness()
{
	bool bRes = true;
	//
	if ( !IsValid( GetZoneByName( "BASE" ) ) )
		bRes = false;
	if ( !IsValid( GetZoneByName( "FFIGHT" ) ) )
		bRes = false;
	//
	if ( !bRes )
		OutputDebugString( "[SCENARIO TRACKER] incorrect data for scenario graph.\n" );
	//
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::ClearItems()
{
	zones.clear();
	clues.clear();
	objectives.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CScenarioFlowChartBase::GetTemplateIDByVariantID( int nVariantID )
{
	for ( vector< CObj<CScenarioZone> >::iterator i = zones.begin(); i != zones.end(); ++i )
	{
		int nRes = (*i)->GetTemplateIDByVariantID( nVariantID );
		if ( nRes > 0 )
			return nRes;
	}
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioZone* CScenarioFlowChartBase::GetZoneByTemplateID( int nTemplateID )
{
	for ( vector< CObj<CScenarioZone> >::iterator zone = zones.begin(); zone != zones.end(); ++zone )
	{
		vector<int> &templatesIDs = (*zone)->GetDBZone()->templatesIDs;
		if ( find( templatesIDs.begin(), templatesIDs.end(), nTemplateID ) != templatesIDs.end() )
			return *zone;
	}
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioObjective* CScenarioFlowChartBase::GetObjectiveByDBObjective( NDb::CDBScenarioObjective *pDBObjective )
{
	ASSERT( IsValid( pDBObjective ) );
	if ( !IsValid( pDBObjective ) )
		return 0;
	//
	for ( vector< CObj<CScenarioObjective> >::iterator objective = objectives.begin(); 
		objective != objectives.end(); ++objective )
		if ( (*objective)->GetDBObjective() == pDBObjective )	
			return *objective;
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioZone *CScenarioFlowChartBase::GetZoneByName( string _szName )
{
	string szName = _szName;
	NStr::ToUpper( szName );
	for ( vector< CObj<CScenarioZone> >::iterator zone = zones.begin(); zone != zones.end(); ++zone )
	{
		string szZoneName = (*zone)->GetDBZone()->sSmallDescription;
		NStr::ToUpper( szZoneName );
		if ( szZoneName == szName )
			return *zone;
	}
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioZone *CScenarioFlowChartBase::GetZoneByDBZone( NDb::CDBScenarioZone *pDBZone )
{
	ASSERT( IsValid( pDBZone ) );
	if ( !IsValid( pDBZone ) )
		return 0;
	//
	for ( vector< CObj<CScenarioZone> >::iterator zone = zones.begin(); zone != zones.end(); ++zone )
		if ( (*zone)->GetDBZone() == pDBZone )
			return *zone;
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioClue *CScenarioFlowChartBase::GetClueByName( string _szName )
{
	string szName = _szName;
	NStr::ToUpper( szName );
	for ( vector< CObj<CScenarioClue> >::iterator clue = clues.begin(); clue != clues.end(); ++clue )
	{
		string szClueName = (*clue)->GetDBClue()->sSmallDescription;
		NStr::ToUpper( szClueName );
		if ( szClueName == szName )
			return *clue;
	}
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioClue *CScenarioFlowChartBase::GetClueByItemID( int nItemID )
{
	for ( vector< CObj<CScenarioClue> >::iterator clue = clues.begin(); clue != clues.end(); ++clue )
		if ( (*clue)->GetDBClue()->nItemID == nItemID )
			return *clue;
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioClue *CScenarioFlowChartBase::GetClueByPersID( int nPersID )
{
	for ( vector< CObj<CScenarioClue> >::iterator clue = clues.begin(); clue != clues.end(); ++clue )
		if ( (*clue)->GetDBClue()->nPersID == nPersID )
			return *clue;
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioClue *CScenarioFlowChartBase::GetClueByDBClue( NDb::CDBScenarioClue *pDBClue )
{
	ASSERT( IsValid( pDBClue ) );
	if ( !IsValid( pDBClue ) )
		return 0;
	//
	for ( vector< CObj<CScenarioClue> >::iterator clue = clues.begin(); clue != clues.end(); ++clue )
		if ( (*clue)->GetDBClue() == pDBClue )
			return *clue;
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::PlaceClue( CScenarioClue *pClue )
{
	ASSERT( IsValid( pClue ) );
	if ( pClue->IsPlaced() )
		return;
	//
	vector< CPtr<CScenarioZone> > possibleZones;
	CDBPtr<NDb::CDBScenarioClue> pDBClue = pClue->GetDBClue();
	for ( vector< CPtr<NDb::CDBScenarioZone> >::iterator dbZone = pDBClue->zonesToPlace.begin(); 
		dbZone != pDBClue->zonesToPlace.end(); ++dbZone )
	{
		if ( IsValid( *dbZone ) )
		{
			CPtr<CScenarioZone> pZone = GetZoneByDBZone( *dbZone );
			if ( !IsValid( pZone ) )
				continue;

			if ( bFull || pDBClue->bPermanent || pZone->CanPlaceClue( pDBClue->clueType ) )
				possibleZones.push_back( pZone );
		}
	}
	//
	int nSize = possibleZones.size();
	if ( nSize > 0 )
	{
		if ( bFull)
			for ( int i = 0; i < nSize; ++i )
				possibleZones[i]->PlaceClue( pClue );
		else
			possibleZones[ random.Get( 0, nSize ) ]->PlaceClue( pClue );
		//
		possibleZones.clear();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::PlaceObjective( CScenarioObjective *pObjective )
{
	ASSERT( IsValid( pObjective ) );
	ASSERT( !pObjective->IsPlaced() );
	//
	for ( vector< CObj<CScenarioClue> >::iterator clue = clues.begin(); clue != clues.end(); ++clue )
	{
		vector< CPtr<NDb::CDBScenarioObjective> > &objectives = (*clue)->GetDBClue()->objectives;
		if ( find( objectives.begin(), objectives.end(), pObjective->GetDBObjective() ) !=
			objectives.end() )
		{
			pObjective->AddPossibleParentClue( *clue );
		}
		//
		if ( (*clue)->IsPlaced() && (*clue)->CanPlaceObjective( pObjective ) )
		{
			(*clue)->PlaceObjective( pObjective );
			//
			for ( vector< CPtr<CScenarioClue> >::const_iterator childClue = pObjective->GetClues().begin();
				childClue != pObjective->GetClues().end(); ++childClue )
			{
				(*childClue)->SetPlaced();
				(*childClue)->SetCompound();
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::SetupLinks( CScenarioObjective *pObjective )
{
	ASSERT( IsValid( pObjective ) );
	//
	if ( pObjective->IsPlaced() )
	{
		// размещаем указатели на открываемые зоны
		pObjective->ClearZones();
		vector< CPtr<NDb::CDBScenarioZone> > &zonesToOpen = pObjective->GetDBObjective()->zonesToOpen;
		for ( vector< CPtr<NDb::CDBScenarioZone> >::iterator zone = zonesToOpen.begin();
			zone != zonesToOpen.end(); ++zone )
		{
			if ( IsValid( *zone ) )
			{
				CPtr<CScenarioZone> pZone = GetZoneByDBZone( *zone );
				if ( IsValid( pZone ) )
				{
					pZone->SetInitial( false );
					pObjective->AddChildZone( pZone );
				}
			}
		}
		// размещаем указатели на блокируемые зоны
		pObjective->ClearZonesToBlock();
		vector< CPtr<NDb::CDBScenarioZone> > &zonesToBlock = pObjective->GetDBObjective()->zonesToBlock;
		for ( vector< CPtr<NDb::CDBScenarioZone> >::iterator zone = zonesToBlock.begin();
			zone != zonesToBlock.end(); ++zone )
		{
			if ( IsValid( *zone ) )
			{
				CPtr<CScenarioZone> pZone = GetZoneByDBZone( *zone );
				if ( IsValid( pZone ) )
				{
					pObjective->AddZoneToBlock( pZone );
					pZone->AddBlocker( pObjective );
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::Generate()
{
	ClearItems();
	LoadItems();
	if ( !CheckDataCorrectness() )
	{
		ASSERT( false );
		return;
	}
	// размещаем clue-сы
	vector< CPtr<CScenarioClue> > cluesToPlace;
	for ( vector< CObj<CScenarioClue> >::const_iterator i = clues.begin(); i != clues.end(); ++i )
		cluesToPlace.push_back( (*i).GetPtr() );
	while ( !cluesToPlace.empty() )
	{
		int n = 0;
		if ( !bFull )
			n = random.Get( 0, cluesToPlace.size() );
		PlaceClue( cluesToPlace[ n ] );
		cluesToPlace.erase( cluesToPlace.begin() + n );
	}
	// размещаем objective-ы
	for ( vector< CObj<CScenarioObjective> >::iterator objective = objectives.begin(); 
		objective != objectives.end(); ++objective )
			PlaceObjective( *objective );
	// размещаем objective-ы для состовных clues-ов
	for ( vector< CObj<CScenarioObjective> >::iterator objective = objectives.begin(); 
		objective != objectives.end(); ++objective )
			if ( !(*objective)->IsPlaced() )
				PlaceObjective( *objective );
	//
	for ( vector< CObj<CScenarioObjective> >::iterator objective = objectives.begin(); 
		objective != objectives.end(); ++objective )
			SetupLinks( *objective );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::RemoveClue( CScenarioClue *pClue )
{
	pClue->SetPlaced( false );
	pClue->ClearLinks();
	OutputDebugString( "[SCENARIO TRACKER] clue was removed\n" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::LoadItems()
{
	zones.clear();
	clues.clear();
	objectives.clear();
	// загружаем зоны
	int nInnerID = 0;
	CDBTable<NDb::CDBScenarioZone> *pZoneDBTable = NDatabase::GetTable<NDb::CDBScenarioZone>();
	CDBIterator<NDb::CDBScenarioZone> zone(*pZoneDBTable);
	while ( pZoneDBTable && zone.MoveNext() )
	{
		CDBPtr<NDb::CDBScenarioZone> pDBZone = zone.Get();
		if ( IsValid( pDBZone ) && IsValid( pDBZone->pScenario ) && 
			pDBZone->pScenario->GetRecordID() == nScenarioID )
		{
			zones.push_back( CreateScenarioZone( pDBZone, nInnerID ) );
			++nInnerID;
		}
	}
	// загружаем улики
	nInnerID = 0;
	CDBTable<NDb::CDBScenarioClue> *pClueDBTable = NDatabase::GetTable<NDb::CDBScenarioClue>();
	CDBIterator<NDb::CDBScenarioClue> clue(*pClueDBTable);
	while ( pClueDBTable && clue.MoveNext() )
	{
		CDBPtr<NDb::CDBScenarioClue> pDBClue = clue.Get();
		if ( IsValid( pDBClue ) && IsValid( pDBClue->pScenario ) && 
			pDBClue->pScenario->GetRecordID() == nScenarioID )
		{
			clues.push_back( CreateScenarioClue( pDBClue, nInnerID ) );
			++nInnerID;
		}
	}
	// загружаем действия
	nInnerID = 0;
	CDBTable<NDb::CDBScenarioObjective> *pObjectiveDBTable = NDatabase::GetTable<NDb::CDBScenarioObjective>();
	CDBIterator<NDb::CDBScenarioObjective> objective(*pObjectiveDBTable);
	while ( pObjectiveDBTable && objective.MoveNext() )
	{
		CDBPtr<NDb::CDBScenarioObjective> pDBObjective = objective.Get();
		if ( IsValid( pDBObjective ) && IsValid( pDBObjective->pScenario ) && 
			pDBObjective->pScenario->GetRecordID() == nScenarioID )
		{
			objectives.push_back( CreateScenarioObjective( pDBObjective, nInnerID ) );
			++nInnerID;
		}
	}
	// загружаем ссылки на составные clue
	CDBTable<NDb::CDBScenarioObjective2Clue> *pObjective2ClueDBTable = NDatabase::GetTable<NDb::CDBScenarioObjective2Clue>();
	CDBIterator<NDb::CDBScenarioObjective2Clue> objective2clue(*pObjective2ClueDBTable);
	while ( pObjective2ClueDBTable && objective2clue.MoveNext() )
	{
		CDBPtr<NDb::CDBScenarioObjective2Clue> pDBObjective = objective2clue.Get();
		CPtr<CScenarioObjective> pObjective = GetObjectiveByDBObjective( pDBObjective->pObjective );
		CPtr<CScenarioClue> pClue = GetClueByDBClue( pDBObjective->pClue );
		if ( !IsValid( pObjective ) || !IsValid( pClue ) )
			continue; // не относятся к загружаемому сценарию
		//
		bool bNoError = find( pObjective->GetClues().begin(), pObjective->GetClues().end(), pClue ) == 
			pObjective->GetClues().end();
		ASSERT( bNoError ); // ошибка в базе
		if ( !bNoError )
			continue;
		bNoError = find( pClue->GetParentObjectives().begin(), pClue->GetParentObjectives().end(), pObjective ) ==
			pClue->GetParentObjectives().end();
		ASSERT( bNoError ); // ошибка в базе
		if ( !bNoError )
			continue;
		//
		pObjective->AddChildClue( pClue );
		pClue->AddParentObjective( pObjective );
	}
	//
	//DEBUG{
	char szStr[128];
	sprintf( szStr, "[SCENARIO TRACKER] %d zones loaded\n", zones.size() );
	OutputDebugString( szStr );
	sprintf( szStr, "[SCENARIO TRACKER] %d clues loaded\n", clues.size() );
	OutputDebugString( szStr );
	sprintf( szStr, "[SCENARIO TRACKER] %d objectives loaded\n", objectives.size() );
	OutputDebugString( szStr );
	//DEBUG}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::Header( fstream &file, string szFileName )
{
	string str = szFileName + ".dot";
	file.open( str.c_str(), ios_base::out | ios_base::trunc );
	file << "digraph g\n{\n  concentrate=true;\n  nodesep=.3;\n  ranksep=.3;\n";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::Footer( fstream &file, string szFileName )
{
	file << "}";
	file.close();
	//
	string str = "scengraph\\dot.exe -Tjpg " + szFileName + ".dot -o " + szFileName + ".jpg";
	system( str.c_str() );
}		
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::DrawZones( fstream &file, 
	const list< CPtr<CScenarioZone> > &availableZones )
{
	for ( vector< CObj<CScenarioZone> >::iterator zone = zones.begin(); zone != zones.end(); ++zone )
	{
		bool bAvailable = ( find( availableZones.begin(), 
			availableZones.end(), (*zone).GetPtr() ) != availableZones.end() );
		(*zone)->DrawNode( file, 0, bAvailable, true );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::DrawClues( fstream &file, 
	const list< CPtr<CScenarioClue> > &availableClues )
{
	for ( vector< CObj<CScenarioClue> >::iterator clue = clues.begin(); clue != clues.end(); ++clue )
		if ( (*clue)->IsPlaced() )
		{
			bool bAvailable = ( find( availableClues.begin(), 
				availableClues.end(), (*clue).GetPtr() ) != availableClues.end() );
			(*clue)->DrawNode( file, bAvailable );
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::DrawZonesRelationships( fstream &file )
{
	for ( vector< CObj<CScenarioZone> >::iterator zone = zones.begin(); zone != zones.end(); ++zone )
		(*zone)->DrawRelationship( file );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::DrawCluesRelationships( fstream &file )
{
	for ( vector< CObj<CScenarioClue> >::iterator clue = clues.begin(); clue != clues.end(); ++clue )
		if ( (*clue)->IsPlaced() )
			(*clue)->DrawRelationship( file );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::Draw( const list< CPtr<CScenarioZone> > &availableZones, 
	const list< CPtr<CScenarioClue> > &availableClues, const char *pszOutputFile )
{
	fstream file;
	string szFile = pszOutputFile ? pszOutputFile : "scengraph\\scenario";
	Header( file, szFile );
	DrawZones( file, availableZones );
	DrawClues( file, availableClues );
	DrawZonesRelationships( file );
	DrawCluesRelationships( file );
	Footer( file, szFile );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::GetClues( vector< CPtr<CScenarioClue> > *pClues )
{
	ASSERT( pClues != 0 );
	if ( pClues == 0 )
		return;
	//
	pClues->clear();
	for ( vector< CObj<CScenarioClue> >::iterator i = clues.begin(); i != clues.end(); ++i )
		pClues->push_back( (*i).GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::GetObjectives( vector< CPtr<CScenarioObjective> > *pObjectives )
{
	ASSERT( pObjectives != 0 );
	if ( pObjectives == 0 )
		return;
	//
	pObjectives->clear();
	for ( vector< CObj<CScenarioObjective> >::iterator i = objectives.begin(); 
		i != objectives.end(); ++i )
			pObjectives->push_back( (*i).GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartBase::GetZones( vector< CPtr<CScenarioZone> > *pZones )
{
	ASSERT( pZones != 0 );
	if ( pZones == 0 )
		return;
	//
	pZones->clear();
	for ( vector< CObj<CScenarioZone> >::iterator i = zones.begin(); i != zones.end(); ++i )
		pZones->push_back( (*i).GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioFlowChart
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioFlowChart::CScenarioFlowChart( int _nScenarioID, bool _bFull ):
	CScenarioFlowChartBase( _nScenarioID, _bFull )
{
	int n = 0;
	while ( true )
	{
		OutputDebugString( "[SCENARIO TRACKER] Generate scenario flowchart\n" );
		Generate();
		pPathFinder = new CScenarioFlowChartPathFinder(	this );
		RemoveIncorrectClues();
		if ( IsFull() || CheckCorrectness() )
			break;
		//
		if ( ++n > 10 )
		{
			ASSERT( 0 );
			OutputDebugString( "[SCENARIO TRACKER] Can't create correct scenario flowchart\n" );
			#ifdef _MAPEDIT
			MessageBox( 0, "Can't create correct scenario flowchart", "Warning", MB_OK | MB_ICONWARNING );
			#endif _MAPEDIT
			break;
		}
	}
	//
	if ( CheckCorrectness() )
	{
		OutputDebugString( "[SCENARIO TRACKER] Scenario flowchart is correct\n" );
		CalculateDifficulties();
		MarkDistance();
		MarkInShortestPath();
		MarkInaccessible();
	}
	else
		OutputDebugString( "[SCENARIO TRACKER] [ ERROR ] Scenario flowchart is incorrect\n" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChart::MarkInaccessible()
{
	CObj<CScenarioFlowChartState> pPath = 
		GetPathFinder()->FindPath( GetZoneByName( "BASE" ), GetZoneByName( "FFIGHT" ) );
	if ( IsValid( pPath ) )
		GetPathFinder()->MarkAsAccessible();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChart::MarkDistance()
{
	CObj<CScenarioFlowChartState> pPath = 
		GetPathFinder()->FindPath( GetZoneByName( "BASE" ), GetZoneByName( "FFIGHT" ) );
	if ( IsValid( pPath ) )
		pPath->MarkDistance();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChart::MarkInShortestPath()
{
	CObj<CScenarioFlowChartState> pPath = 
		GetPathFinder()->FindPath( GetZoneByName( "BASE" ), GetZoneByName( "FFIGHT" ) );
	if ( IsValid( pPath ) )
		pPath->MarkAsShortestPath();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChart::RemoveIncorrectClues()
{
	if ( IsFull() )
		return;
	//
	while ( true )
	{
		vector< CPtr<CScenarioClue> > cluesToErase;
		for ( vector< CObj<CScenarioClue> >::iterator clue = clues.begin(); clue != clues.end(); ++clue )
		if ( (*clue)->IsPlaced() )
		{
			list< CPtr<CScenarioClue> > parentClues;
			GetPathFinder()->GetParentClues( *clue, &parentClues ); 
			if ( parentClues.size() < GetPathFinder()->GetMinParentToOpen( *clue ) )
				cluesToErase.push_back( (*clue).GetPtr() );
		} 
		//
		for ( vector< CPtr<CScenarioClue> >::iterator clue = cluesToErase.begin(); clue != cluesToErase.end(); ++clue )
		{
			(*clue)->SetPlaced( false );
			OutputDebugString( "[SCENARIO TRACKER] Incorrect clue was removed: " );
			OutputDebugString( (*clue)->GetDBClue()->sSmallDescription.c_str() );
			OutputDebugString( "\n" );
		}
		//
		if ( cluesToErase.empty() )
			return;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChart::InitDifficulties()
{
	for ( vector< CObj<CScenarioZone> >::iterator zone = zones.begin(); zone != zones.end(); ++zone )
	{
		list< CPtr<CScenarioZone> > childZones;
		list<SScenarioFlowChartPassage> passages;
		GetPathFinder()->GetChildZones( 0, *zone, &passages );
		GetPathFinder()->GetZonesFromPassages( passages, &childZones );
		if ( childZones.empty() && !(*zone)->IsInitial() )
			(*zone)->SetDifficulty( 13 );
	}
	//
	GetZoneByName( "FFIGHT" )->SetDifficulty( 15 );
	GetZoneByName( "BASE" )->SetInitial( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChart::SetPathDifficulty( const list< CPtr<CScenarioZone> > &path, bool bCheckBegin )
{
	float fBegin;
	float fEnd = path.back()->GetDifficulty();
	if ( bCheckBegin )
		fBegin = path.front()->GetDifficulty();
	else
		fBegin = max( 1, fEnd - path.size() + 1 );
	float fStep = ( fEnd - fBegin ) / ( path.size() - 1 );
	float fCurrent = fBegin;
	for ( list< CPtr<CScenarioZone> >::const_iterator zone = path.begin(); zone != path.end(); ++zone )
	{
		(*zone)->SetDifficulty( max( 1, fCurrent ) );
		fCurrent += fStep;
	}
	//
	path.back()->SetDifficulty( fEnd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioFlowChart::GetEdgeForDifCalculation( CScenarioZone **ppBegin, CScenarioZone **ppEnd )
{
	for ( vector< CObj<CScenarioZone> >::const_iterator zone = zones.begin(); 
		zone != zones.end(); ++zone )
	{
		if ( (*zone)->IsDifCalculated() )
		{
			list< CPtr<CScenarioZone> > childZones;
			list<SScenarioFlowChartPassage> passages;
			GetPathFinder()->GetChildZones( 0, *zone, &passages );
			GetPathFinder()->GetZonesFromPassages( passages, &childZones );
			for ( list< CPtr<CScenarioZone> >::iterator child = childZones.begin();
				child != childZones.end(); ++child )
			{
				if ( !(*child)->IsDifCalculated() )
				{
					*ppBegin = *zone;
					*ppEnd = *child;
					return true;
				}
			}
		}
	}
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChart::CalculateDifficulties()
{
	InitDifficulties();
	//
	CObj<CScenarioFlowChartState> pPath = 
		GetPathFinder()->FindPath( GetZoneByName( "BASE" ), GetZoneByName( "FFIGHT" ) );
	if ( IsValid( pPath ) )
		SetPathDifficulty( pPath->GetZones()->GetItems(), true );
	//
	for ( vector< CObj<CScenarioZone> >::iterator zone = zones.begin(); zone != zones.end(); ++zone )
		if ( (*zone)->IsInitial() )
		{
			pPath = GetPathFinder()->FindPath( *zone );
			if ( IsValid( pPath ) )
				SetPathDifficulty( pPath->GetZones()->GetItems(), false );
		}
	//
	int n = 0;
	CScenarioZone *pBegin, *pEnd;
	while ( GetEdgeForDifCalculation( &pBegin, &pEnd ) )
	{
		if ( ++n > 100 )
		{
			ASSERT( false ); // can't set difficulties
			return;
		}
		pPath = GetPathFinder()->FindPath( pEnd );
		if ( IsValid( pPath ) )
		{
			list< CPtr<CScenarioZone> > path = pPath->GetZones()->GetItems();
			path.push_front( pBegin );
			SetPathDifficulty( path, true );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioFlowChart::CheckCorrectness()
{
	bool bCorrect = true;
	CPtr<CScenarioFlowChartState> pPath = 
		GetPathFinder()->FindPath( GetZoneByName( "BASE" ), GetZoneByName( "FFIGHT" ) );
	bCorrect &= IsValid( pPath );
	//
	if ( !IsFull() && bCorrect )
		bCorrect &= pPath->GetZones()->GetItems().size() >= 6;
	return bCorrect;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChart::GetZonesWhichCanBeOpened( CScenarioClue *pClue, 
	list< CPtr<CScenarioZone> > *pZones )
{
	ASSERT( IsValid( pClue ) );
	ASSERT( pZones != 0 );
	if ( !IsValid( pClue ) || pZones == 0 )
		return;
	//
	pZones->clear();
	for ( vector< CPtr<CScenarioObjective> >::
		const_iterator i = pClue->GetObjectives().begin(); i != pClue->GetObjectives().end(); ++i )
	{
		for ( vector< CPtr<CScenarioZone> >::
			const_iterator j = (*i)->GetZones().begin(); j != (*i)->GetZones().end(); ++j )
			if ( find( pZones->begin(), pZones->end(), *j ) == pZones->end() )
				pZones->push_back( *j );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioFlowChartItemsList
////////////////////////////////////////////////////////////////////////////////////////////////////
template < class T >
CScenarioFlowChartItemsList<T>::CScenarioFlowChartItemsList( int _nMaxSize ):
	bUpdateSignature( true ), nMaxSize( _nMaxSize )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template < class T >
void CScenarioFlowChartItemsList<T>::UpdateSignature()
{
	if ( bUpdateSignature )
	{
		signature.resize( nMaxSize / 32 + 1 );
		for ( int i = 0; i < signature.size(); ++i )
			signature[i] = 0;
		for (	list< CPtr<T> >::iterator i = items.begin(); i != items.end(); ++i )
			signature[ (*i)->GetInnerID() / 32 ] |= ( 1 << ( (*i)->GetInnerID() % 32 ) );
		bUpdateSignature = false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template < class T >
const list< CPtr<T> >& CScenarioFlowChartItemsList<T>::GetItems() const
{
	return items;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template < class T >
const vector<DWORD>& CScenarioFlowChartItemsList<T>::GetSignature()
{
	UpdateSignature();
	return signature;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template < class T >
int CScenarioFlowChartItemsList<T>::GetMaxSize() const
{
	return nMaxSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template < class T >
void CScenarioFlowChartItemsList<T>::Push( T *pItem )
{
	ASSERT( IsValid( pItem ) );
	if ( !IsValid( pItem ) )
		return;
	//
	items.push_back( pItem );
	bUpdateSignature = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template < class T >
void CScenarioFlowChartItemsList<T>::Erase( T *pItem )
{
	ASSERT( IsValid( pItem ) );
	if ( !IsValid( pItem ) )
		return;
	//
	if ( IsContainItem( pItem ) )
	{
		list< CPtr<T> >::iterator i = find( items.begin(), items.end(), pItem );
		if ( i != items.end() )
		{
			items.erase( i );
			bUpdateSignature = true;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template < class T >
bool CScenarioFlowChartItemsList<T>::IsContainItem( T *pItem )
{
	ASSERT( IsValid( pItem ) );
	if ( !IsValid( pItem ) )
		return false;
	//
	UpdateSignature();
	return ( signature[ pItem->GetInnerID() / 32 ] & ( 1 << ( pItem->GetInnerID() % 32 ) ) ) > 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template < class T >
bool CScenarioFlowChartItemsList<T>::IsContainList( CScenarioFlowChartItemsList<T> &itemsList )
{
	UpdateSignature();
	vector<DWORD> itemsListSignature = itemsList.GetSignature();
	if ( itemsListSignature.size() != signature.size() )
		return false;
  for ( int i = 0; i < itemsListSignature.size(); ++i )
		if ( ( itemsListSignature[i] & signature[i] ) != itemsListSignature[i] )
			return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template < class T >
void CScenarioFlowChartItemsList<T>::operator=( CScenarioFlowChartItemsList<T> &itemsList )
{
	CopyItemsList( itemsList );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template < class T >
bool CScenarioFlowChartItemsList<T>::operator==( CScenarioFlowChartItemsList<T> &itemsList )
{
	UpdateSignature();
	vector<DWORD> itemsListSignature = itemsList.GetSignature();
	if ( signature.size() != itemsListSignature.size() )
		return false;
	//
	for ( int i = 0; i < signature.size(); ++i )
		if ( signature[i] != itemsListSignature[i] )
			return false;
	//
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template < class T >
void CScenarioFlowChartItemsList<T>::CopyItemsList( CScenarioFlowChartItemsList<T> &itemsList )
{
	nMaxSize = itemsList.GetMaxSize();
	signature = itemsList.GetSignature();
	items = itemsList.GetItems();
	bUpdateSignature = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioFlowChartState
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioFlowChartState::CScenarioFlowChartState( CScenarioFlowChartPathFinder *_pPathFinder ):
	pPathFinder( _pPathFinder ), bFinal( false )
{
	ASSERT( IsValid( pPathFinder ) );
	if ( !IsValid( pPathFinder ) )
		return;
	//
	CreateLists();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioFlowChartState::CScenarioFlowChartState( CScenarioFlowChartState *_pState )
{
	ASSERT( IsValid( _pState ) );
	if ( !IsValid( _pState ) )
		return;
	//
	CopyState( *_pState );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioFlowChartPathFinder* CScenarioFlowChartState::GetPathFinder() const
{
	return pPathFinder;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartState::CreateLists()
{
	ASSERT( IsValid( pPathFinder ) );
	if ( !IsValid( pPathFinder ) )
		return;
	//
	vector< CPtr<CScenarioZone> > zones;
	vector< CPtr<CScenarioClue> > clues;
	pPathFinder->GetFlowChart()->GetZones( &zones );
	pPathFinder->GetFlowChart()->GetClues( &clues );
	int nZonesSize = zones.size();
	int nCluesSize = clues.size();
	pZones = new CScenarioFlowChartItemsList<CScenarioZone>( nZonesSize );
	pBranchingZones = new CScenarioFlowChartItemsList<CScenarioZone>( nZonesSize );
	pLockedZones = new CScenarioFlowChartItemsList<CScenarioZone>( nZonesSize );
	pClues = new CScenarioFlowChartItemsList<CScenarioClue>( nCluesSize );
	pUsedClues = new CScenarioFlowChartItemsList<CScenarioClue>( nCluesSize );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartState::CopyState( CScenarioFlowChartState state )
{
	pPathFinder = state.GetPathFinder();
	CreateLists();
	bFinal = state.IsFinal();
	*pZones = *state.GetZones();
	*pClues = *state.GetClues();
	*pBranchingZones = *state.GetBranchingZones();
	*pLockedZones = *state.GetLockedZones();
	*pUsedClues = *state.GetUsedClues();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioFlowChartState::IsContain( CScenarioZone *pZone )
{
	ASSERT( IsValid( pZone ) );
	if ( !IsValid( pZone ) )
		return false;
	//
	return pZones->IsContainItem( pZone );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioFlowChartState::IsContainState( CScenarioFlowChartState *pState )
{
	ASSERT( IsValid( pState ) );
	if ( !IsValid( pState ) )
		return false;
	//
	return pZones->IsContainList( *pState->GetZones() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartState::operator=( CScenarioFlowChartState &state )
{
	CopyState( state );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioFlowChartState::operator==( CScenarioFlowChartState &state )
{
	return *state.GetZones() == *pZones;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartState::operator+=( CScenarioZone *pZone )
{
	ASSERT( IsValid( pZone ) );
	ASSERT( !IsContain( pZone ) );
	if ( !IsValid( pZone ) || IsContain( pZone ) )
		return;
	//
	pZones->Push( pZone );
	if ( IsBranchingZone( pZone ) )
		pBranchingZones->Push( pZone );
	UpdateClues( pZone );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartState::UpdateClues( CScenarioZone *pZone )
{
	ASSERT( IsValid( pZone ) );
	if ( !IsValid( pZone ) )
		return;
	//
	for ( vector< CPtr<CScenarioClue> >::const_iterator i = pZone->GetClues().begin();
		i != pZone->GetClues().end(); ++i )
	{
		if ( !pClues->IsContainItem( *i ) )
		{
			pClues->Push( *i );
			UpdateClues( *i );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartState::UpdateClues( CScenarioClue *pClue )
{
	ASSERT( IsValid( pClue ) );
	if ( !IsValid( pClue ) )
		return;
	//
	for ( vector< CPtr<CScenarioObjective> >::const_iterator i = pClue->GetObjectives().begin();
		i != pClue->GetObjectives().end(); ++i )
	{
		const CPtr<CScenarioObjective> &pObj = *i;
		for ( vector< CPtr<CScenarioClue> >::const_iterator o = pObj->GetClues().begin();
			o != pObj->GetClues().end(); ++o )
		{
			if ( !pClues->IsContainItem( *o ) && CanUnlockClue( *o ) )
			{
				pClues->Push( *o );
				UpdateClues( *o );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioFlowChartState::CanUnlockClue( CScenarioClue *pClue )
{
	list< CPtr<CScenarioClue> > parentClues;
	GetPathFinder()->GetParentClues( pClue, &parentClues );
	int nUnLocked = 0;
	for ( list< CPtr<CScenarioClue> >::iterator i = parentClues.begin(); i != parentClues.end(); ++i )
		if ( !IsClueLocked( *i ) )
			++nUnLocked;
	//
	return nUnLocked >= GetPathFinder()->GetMinParentToOpen( pClue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioFlowChartState::IsBranchingZone( CScenarioZone *pZone )
{
	ASSERT( IsValid( pZone ) );
	if ( !IsValid( pZone ) )
		return false;
	//
	list< CPtr<CScenarioZone> > childZones;
	list<SScenarioFlowChartPassage> passages;
	GetPathFinder()->GetChildZones( 0, pZone, &passages );
	GetPathFinder()->GetZonesFromPassages( passages, &childZones );
	for ( list< CPtr<CScenarioZone> >::iterator i = childZones.begin(); i != childZones.end(); ++i )
		if ( !pZones->IsContainItem( *i ) )
			return true;
	//
	return false;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartState::UpdateBranchingZones()
{
	list< CPtr<CScenarioZone> > zonesToErase;
	for ( list< CPtr<CScenarioZone> >::const_iterator i = pBranchingZones->GetItems().begin();
		i != pBranchingZones->GetItems().end(); ++i )
	{
		if ( !IsBranchingZone( *i ) )
			zonesToErase.push_back( *i );
	}
	//
	for ( list< CPtr<CScenarioZone> >::iterator i = zonesToErase.begin(); i != zonesToErase.end(); ++i )
		pBranchingZones->Erase( *i );
	//
	zonesToErase.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioZone* CScenarioFlowChartState::GetFrontZone()
{
	UpdateBranchingZones();
	if ( pBranchingZones->GetItems().empty() )
		return 0;
	else
		return pBranchingZones->GetItems().back();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CScenarioFlowChartState::GetHashKey()
{
	return pZones->GetItems().size();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioFlowChartState::IsClueLocked( CScenarioClue *pClue )
{
	ASSERT( IsValid( pClue ) );
	if ( !IsValid( pClue ) )
		return true;
	//
	return !pClues->IsContainItem( pClue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartState::OutputDebugInfo()
{
	for ( list< CPtr<CScenarioZone> >::const_iterator i = GetZones()->GetItems().begin();
		i != GetZones()->GetItems().end(); ++i )
	{
		OutputDebugString( "  " );
		if ( IsZoneLocked( *i ) )
			OutputDebugString( "!" );
		OutputDebugString( (*i)->GetDBZone()->sSmallDescription.c_str() );
		OutputDebugString( "  " );
	}
	OutputDebugString( "\n" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartState::GetNextStates( list< CPtr<CScenarioFlowChartState> > *pNextStates, 
	bool ( * IsFinal )( CScenarioZone * ), bool bUseLocks )
{
	ASSERT( pNextStates != 0 );
	if ( pNextStates == 0 )
		return;
	//
	pNextStates->clear();
	CPtr<CScenarioZone> pFront = GetFrontZone();
	if ( IsValid( pFront ) )
	{
		list< CPtr<CScenarioZone> > childZones;
		list<SScenarioFlowChartPassage> passages;
		CScenarioFlowChartState *pCurState = 0;
		if ( bUseLocks )
			pCurState = this;
		GetPathFinder()->GetChildZones( pCurState, pFront, &passages );
		GetPathFinder()->GetZonesFromPassages( passages, &childZones );
		for ( list< CPtr<CScenarioZone> >::iterator z = childZones.begin(); z != childZones.end(); ++z )
			if ( !IsContain( *z ) )
			{
				CPtr<CScenarioFlowChartState> pNextState = new CScenarioFlowChartState( this );
				pNextState->UsePassage( GetPathFinder()->GetPassageForZone( passages, *z ) );
				if ( !bUseLocks || !pNextState->IsZoneLocked( *z ) )
				{
					*pNextState += *z;
					if ( IsFinal( *z ) )
						pNextState->SetFinal( true );
					pNextStates->push_back( pNextState );
				}
			}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartState::LockZone( CScenarioZone *pZone )
{
	ASSERT( IsValid( pZone ) );
	if ( !IsValid( pZone ) )
		return;
	//
	if ( !pLockedZones->IsContainItem( pZone ) )
		pLockedZones->Push( pZone );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioFlowChartState::IsZoneLocked( CScenarioZone *pZone )
{	
	ASSERT( IsValid( pZone ) );
	if ( !IsValid( pZone ) )
		return true;
	//
	return pLockedZones->IsContainItem( pZone );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartState::UsePassage( const SScenarioFlowChartPassage &passage )
{
	for ( list< CPtr<CScenarioClue> >::const_iterator i = passage.clues.begin();
		i != passage.clues.end(); ++i )
	{
		if ( !pUsedClues->IsContainItem( *i ) )
			pUsedClues->Push( *i );
	}
	//
	for ( list< CPtr<CScenarioObjective> >::const_iterator i = passage.objectives.begin();
		i != passage.objectives.end(); ++i )
	{
		for ( vector< CPtr<CScenarioZone> >::const_iterator z = (*i)->GetZonesToBlock().begin();
			z != (*i)->GetZonesToBlock().end(); ++z )
		{
			LockZone( *z );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartState::MarkDistance()
{
	int n = 0;
	for ( list< CPtr<CScenarioZone> >::const_iterator i = pZones->GetItems().begin();
		i != pZones->GetItems().end(); ++i )
			(*i)->SetDistance( n++ );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartState::MarkAsShortestPath()
{
	for ( list< CPtr<CScenarioZone> >::const_iterator i = pZones->GetItems().begin();
		i != pZones->GetItems().end(); ++i )
			(*i)->SetInShortestPath( true );
	for ( list< CPtr<CScenarioClue> >::const_iterator i = pClues->GetItems().begin();
		i != pClues->GetItems().end(); ++i )
			(*i)->SetInShortestPath( true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartState::MarkAsAccessible()
{
	for ( list< CPtr<CScenarioZone> >::const_iterator i = pZones->GetItems().begin();
		i != pZones->GetItems().end(); ++i )
			(*i)->SetInaccessible( false );
	for ( list< CPtr<CScenarioClue> >::const_iterator i = pClues->GetItems().begin();
		i != pClues->GetItems().end(); ++i )
			(*i)->SetInaccessible( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioFlowChartPathFinder
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioFlowChartPathFinder::CScenarioFlowChartPathFinder(	CScenarioFlowChartBase *_pFlowChart ):
	pFlowChart( _pFlowChart )
{
	ASSERT( IsValid( pFlowChart ) );
	CalculateMinParentToOpen();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioFlowChartPathFinder::IsStatePassed( CScenarioFlowChartState *pState )
{
	ASSERT( IsValid( pState ) );
	if ( !IsValid( pState ) )
		return true;
	//
	list< CObj<CScenarioFlowChartState> > &states = passedStates[ pState->GetHashKey() ];
	for ( list< CObj<CScenarioFlowChartState> >::iterator i = states.begin(); i != states.end(); ++i )
		if ( (*i)->IsContainState( pState ) )
			return true;
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartPathFinder::PassState( CScenarioFlowChartState *pState )
{
	ASSERT( IsValid( pState ) );
	if ( !IsValid( pState ) )
		return;
	//
	if ( !IsStatePassed( pState ) )
		passedStates[ pState->GetHashKey() ].push_back( new CScenarioFlowChartState( pState ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartPathFinder::FinalState( CScenarioFlowChartState *pState )
{
	ASSERT( IsValid( pState ) );
	if ( !IsValid( pState ) )
		return;
	//
	finalStates[ pState->GetHashKey() ].push_back( new CScenarioFlowChartState( pState ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartPathFinder::FindPathA( CScenarioZone *pFrom, 
	bool (* IsFinal)( CScenarioZone * ), bool bUseLocks )
{
	passedStates.clear();
	finalStates.clear();
	states.clear();
	//
	ASSERT( IsValid( pFrom ) );
	ASSERT( !IsFinal( pFrom ) );
	if ( !IsValid( pFrom ) || IsFinal( pFrom ) )
		return;
	//
	CPtr<CScenarioFlowChartState> pBaseState = new CScenarioFlowChartState( this );
	*pBaseState += pFrom;
	states.push_back( pBaseState.GetPtr() );
	PassState( pBaseState );
	//
	bool bPathFound = false;
	while ( !bPathFound && !states.empty() )
	{
		list< CObj<CScenarioFlowChartState> > nextStates;
		for ( list< CObj<CScenarioFlowChartState> >::iterator i = states.begin(); i != states.end(); ++i )
		{
			list< CPtr<CScenarioFlowChartState> > thisStateNextStates;
			(*i)->GetNextStates( &thisStateNextStates, IsFinal, bUseLocks );
			for ( list< CPtr<CScenarioFlowChartState> >::iterator s = thisStateNextStates.begin();
				s != thisStateNextStates.end(); ++s )
					if ( !IsStatePassed( *s ) )
					{
						PassState( *s );
						if ( (*s)->IsFinal() )
							FinalState( *s );
						else
							nextStates.push_back( (*s).GetPtr() );
					}
		}
		//
		states = nextStates;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static CScenarioZone *pFinalZone;
static bool IsFinal_Zone( CScenarioZone *pZone )
{
	ASSERT( IsValid( pZone ) );
	if ( !IsValid( pZone ) )
		return true;
	//
	return pZone == pFinalZone;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsFinal_DifCalculated( CScenarioZone *pZone )
{
	ASSERT( IsValid( pZone ) );
	if ( !IsValid( pZone ) )
		return true;
	//
	return pZone->IsDifCalculated();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioFlowChartState* CScenarioFlowChartPathFinder::
	FindPath( CScenarioZone *pFrom, CScenarioZone *pTo, bool bUseLocks )
{
	ASSERT( IsValid( pFrom ) );
	ASSERT( IsValid( pTo ) );
	ASSERT( pFrom != pTo );
	if ( !IsValid( pFrom ) || !IsValid( pTo ) || pFrom == pTo )
		return 0;
	//
	pFinalZone = pTo;
	FindPathA( pFrom, IsFinal_Zone, bUseLocks );
	return GetBestState( finalStates, &CScenarioFlowChartPathFinder::CompareSize );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioFlowChartState* CScenarioFlowChartPathFinder::FindPath( CScenarioZone *pFrom )
{
	ASSERT( IsValid( pFrom ) );
	if ( !IsValid( pFrom ) )
		return 0;
	//
	FindPathA( pFrom, IsFinal_DifCalculated, false );
	return GetBestState( finalStates, &CScenarioFlowChartPathFinder::CompareSize);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenarioFlowChartPathFinder::CompareSize( CScenarioFlowChartState *pState1, 
	CScenarioFlowChartState *pState2 )
{
	if ( !IsValid( pState1 ) )
		return false;
	if ( !IsValid( pState2 ) )
		return true;
	return pState1->GetZones()->GetItems().size() < pState2->GetZones()->GetItems().size();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioFlowChartState* CScenarioFlowChartPathFinder::
	GetBestState( const unordered_map< int, list< CObj<CScenarioFlowChartState> > > &states,
	bool ( CScenarioFlowChartPathFinder::* Compare )( CScenarioFlowChartState *, CScenarioFlowChartState * ) )
{
	CObj<CScenarioFlowChartState> pBestState = 0;
	for ( unordered_map< int, list< CObj<CScenarioFlowChartState> > >::const_iterator i = states.begin();
		i != states.end(); ++i )
	{
		for ( list< CObj<CScenarioFlowChartState> >::const_iterator j = i->second.begin();
			j != i->second.end(); ++j )
		{
			if ( (this->*Compare)( *j, pBestState ) )
				pBestState = *j;
		}
	}
	if ( IsValid( pBestState ) )
		return new CScenarioFlowChartState( pBestState );
	else
		return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioFlowChartBase* CScenarioFlowChartPathFinder::GetFlowChart() const
{
	return pFlowChart;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartPathFinder::GetZonesFromPassages( 
	const list<SScenarioFlowChartPassage> &passages, list< CPtr<CScenarioZone> > *pZones )
{
	ASSERT( pZones != 0 );
	if ( pZones == 0 )
		return;
	//
	pZones->clear();
	for ( list<SScenarioFlowChartPassage>::const_iterator i = passages.begin(); i!= passages.end(); ++i )
	{
		if ( find( pZones->begin(), pZones->end(), (*i).pTo ) == pZones->end() )
			pZones->push_back( (*i).pTo );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SScenarioFlowChartPassage CScenarioFlowChartPathFinder::GetPassageForZone( 
	const list<SScenarioFlowChartPassage> &passages, CScenarioZone *pZone )
{
	ASSERT( IsValid( pZone ) );
	if ( !IsValid( pZone ) )
		return SScenarioFlowChartPassage( 0, 0 );
	//
	for ( list<SScenarioFlowChartPassage>::const_iterator i = passages.begin(); i != passages.end(); ++i )
		if ( (*i).pTo == pZone )
			return *i;
	//
	return SScenarioFlowChartPassage( 0, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartPathFinder::GetChildZones( CScenarioFlowChartState *pState, 
	CScenarioObjective *pObjective, list<SScenarioFlowChartPassage> *pPassages )
{
	ASSERT( pPassages != 0 );
	ASSERT( IsValid( pObjective ) );
	if ( !IsValid( pObjective ) || pPassages == 0 )
		return;
	//
	if ( !pObjective->IsPlaced() )
		return;
	//
	pPassages->clear();
	for (	vector< CPtr<CScenarioZone> >::const_iterator i = pObjective->GetZones().begin();
		i != pObjective->GetZones().end(); ++i )
	{
		if ( ( !IsValid( pState ) || !pState->IsContain( *i ) ) &&
			( !IsValid( pState ) || !pState->IsZoneLocked( *i ) ) )
				pPassages->push_back( SScenarioFlowChartPassage( 0, *i ) + pObjective );
	}
	//
	for (	vector< CPtr<CScenarioClue> >::const_iterator i = pObjective->GetClues().begin();
		i != pObjective->GetClues().end(); ++i )
	{
		list<SScenarioFlowChartPassage> passages;
		GetChildZones( pState, *i, &passages );
		for ( list<SScenarioFlowChartPassage>::iterator p = passages.begin(); p != passages.end(); ++p )
			pPassages->push_back( *p + pObjective );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartPathFinder::GetChildZones( CScenarioFlowChartState *pState, 
	CScenarioClue *pClue,	list<SScenarioFlowChartPassage> *pPassages )
{
	ASSERT( pPassages != 0 );
	ASSERT( IsValid( pClue ) );
	if ( !IsValid( pClue ) || pPassages == 0 )
		return;
	//
	if ( !pClue->IsPlaced() || 
		( IsValid( pState) && pState->IsClueLocked( pClue ) ) )
			return;
	//
	pPassages->clear();
	for (	vector< CPtr<CScenarioObjective> >::const_iterator i = pClue->GetObjectives().begin();
		i != pClue->GetObjectives().end(); ++i )
	{
		list<SScenarioFlowChartPassage> passages;
		GetChildZones( pState, *i, &passages );
		for ( list<SScenarioFlowChartPassage>::iterator p = passages.begin(); p != passages.end(); ++p )
			pPassages->push_back( *p + pClue );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartPathFinder::GetChildZones( CScenarioFlowChartState *pState, 
	CScenarioZone *pZone, list<SScenarioFlowChartPassage> *pPassages )
{
	ASSERT( pPassages != 0 );
	ASSERT( IsValid( pZone ) );
	if ( !IsValid( pZone ) || pPassages == 0 )
		return;
	//
	if ( IsValid( pState ) && pState->IsZoneLocked( pZone ) )
		return;
	//
	pPassages->clear();
	for (	vector< CPtr<CScenarioClue> >::const_iterator i = pZone->GetClues().begin();
		i != pZone->GetClues().end(); ++i )
	{
		list<SScenarioFlowChartPassage> passages;
		GetChildZones( pState, *i, &passages );
		for ( list<SScenarioFlowChartPassage>::iterator p = passages.begin(); p != passages.end(); ++p )
		{
			SScenarioFlowChartPassage passage = *p;
			passage.pFrom = pZone;
			pPassages->push_back( passage );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartPathFinder::CalculateMinParentToOpen()
{
	vector< CPtr<CScenarioClue> > clues;
	pFlowChart->GetClues( &clues );
	for ( vector< CPtr<CScenarioClue> >::iterator i = clues.begin(); i != clues.end(); ++i )
	{
		int nMinParentToOpen = 0;
		if ( (*i)->GetDBClue()->nMinParentToOpen >= 0 )
			nMinParentToOpen = (*i)->GetDBClue()->nMinParentToOpen;
		else
		{
			list< CPtr<CScenarioClue> > parentClues;
			GetPossibleParentClues( *i, &parentClues ); 
			nMinParentToOpen = parentClues.size();
		}
		//
		minParentToOpen[ *i ] = nMinParentToOpen;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CScenarioFlowChartPathFinder::GetMinParentToOpen( CScenarioClue *pClue )
{
	ASSERT( IsValid( pClue ) );
	if ( !IsValid( pClue ) )
		return 0;
	//
	return minParentToOpen[ pClue ];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartPathFinder::
	GetParentClues( CScenarioClue *pClue, list< CPtr<CScenarioClue> > *pClues )
{
	ASSERT( IsValid( pClue ) );
	ASSERT( pClues != 0 );
	if ( !IsValid( pClue ) || pClues == 0 )
		return;
	//
	pClues->clear();
	vector< CPtr<CScenarioObjective> > parentObjectives;
	for ( vector< CPtr<CScenarioObjective> >::const_iterator 
		i = pClue->GetParentObjectives().begin(); i != pClue->GetParentObjectives().end(); ++i )
	{
		if ( find( (*i)->GetClues().begin(), (*i)->GetClues().end(), pClue ) != (*i)->GetClues().end() )
			parentObjectives.push_back( (*i).GetPtr() );
	}
	//
	for ( vector< CPtr<CScenarioObjective> >::
		iterator i = parentObjectives.begin(); i != parentObjectives.end(); ++i )
	{
		CPtr<CScenarioClue> pParent = (*i)->GetParentClue();
		if ( IsValid( pParent ) && pParent->IsPlaced() &&
			find( pClues->begin(), pClues->end(), pParent ) == pClues->end() )
				pClues->push_back( pParent );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartPathFinder::
	GetPossibleParentClues( CScenarioClue *pClue, list< CPtr<CScenarioClue> > *pClues )
{
	ASSERT( IsValid( pClue ) );
	ASSERT( pClues != 0 );
	if ( !IsValid( pClue ) || pClues == 0 )
		return;
	//
	pClues->clear();
	vector< CPtr<CScenarioObjective> > objectives;
	GetFlowChart()->GetObjectives( &objectives );
	vector< CPtr<CScenarioObjective> > parentObjectives;
	for ( vector< CPtr<CScenarioObjective> >::
		const_iterator i = objectives.begin(); i != objectives.end(); ++i )
	{
		if ( find( (*i)->GetClues().begin(), (*i)->GetClues().end(), pClue ) != (*i)->GetClues().end() )
			parentObjectives.push_back( (*i).GetPtr() );
	}
	//
	for ( vector< CPtr<CScenarioObjective> >::
		const_iterator i = parentObjectives.begin(); i != parentObjectives.end(); ++i )
	{
		for ( vector< CPtr<CScenarioClue> >::const_iterator 
			j = (*i)->GetPossibleParentClues().begin(); j != (*i)->GetPossibleParentClues().end(); ++j )
		{
			if ( find( pClues->begin(), pClues->end(), *j ) == pClues->end() )
					pClues->push_back( *j );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFlowChartPathFinder::MarkAsAccessible()
{
	for ( unordered_map< int, list< CObj<CScenarioFlowChartState> > >::iterator i = passedStates.begin();
		i != passedStates.end(); ++i )
	{
		for ( list< CObj<CScenarioFlowChartState> >::iterator j = i->second.begin();
			j != i->second.end(); ++j )
				(*j)->MarkAsAccessible();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SScenarioFlowChartPassage
////////////////////////////////////////////////////////////////////////////////////////////////////
SScenarioFlowChartPassage::SScenarioFlowChartPassage( CScenarioZone *_pFrom, CScenarioZone *_pTo ):
	pFrom( _pFrom ), pTo( _pTo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SScenarioFlowChartPassage::operator=( SScenarioFlowChartPassage &passage )
{
	clues = passage.clues;
	objectives = passage.objectives;
	pFrom = passage.pFrom;
	pTo = passage.pTo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SScenarioFlowChartPassage::operator+=( SScenarioFlowChartPassage &passage )
{
	clues.insert( clues.begin(), passage.clues.begin(), passage.clues.end() );
	objectives.insert( objectives.begin(), passage.objectives.begin(), passage.objectives.end() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SScenarioFlowChartPassage SScenarioFlowChartPassage::operator+( SScenarioFlowChartPassage &passage )
{
	SScenarioFlowChartPassage result = *this;
	result.clues.insert( result.clues.begin(), 
		passage.clues.begin(), passage.clues.end() );
	result.objectives.insert( result.objectives.begin(), 
		passage.objectives.begin(), passage.objectives.end() );
	return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SScenarioFlowChartPassage SScenarioFlowChartPassage::operator+( CScenarioClue *pClue )
{
	ASSERT( IsValid( pClue ) );
	if ( !IsValid( pClue ) )
		return *this;
	//
	SScenarioFlowChartPassage result = *this;
	result.clues.push_back( pClue );
	return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SScenarioFlowChartPassage SScenarioFlowChartPassage::operator+( CScenarioObjective *pObjective )
{
	ASSERT( IsValid( pObjective ) );
	if ( !IsValid( pObjective ) )
		return *this;
	//
	SScenarioFlowChartPassage result = *this;
	result.objectives.push_back( pObjective );
	return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioFlowChart *CreateScenarioFlowChart( int nScenarioID, bool bFull )
{
	return new CScenarioFlowChart( nScenarioID, bFull );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioFlowChart *CreateScenarioFlowChart( string szScenarioName, bool bFull )
{
	int nScenarioID = 0;
	//
	CDBTable<NDb::CDBScenario> *pDBScenarioTable = NDatabase::GetTable<NDb::CDBScenario>();
	CDBIterator<NDb::CDBScenario> scenario(*pDBScenarioTable);
	while ( pDBScenarioTable && scenario.MoveNext() )
	{
		CDBPtr<NDb::CDBScenario> pDBScenario = scenario.Get();
		string szUpperScenarioName = szScenarioName; NStr::ToUpper( szUpperScenarioName );
		string szUpperName = pDBScenario->szName; NStr::ToUpper( szUpperName );
		if ( IsValid( pDBScenario ) && szUpperName == szUpperScenarioName )
			return CreateScenarioFlowChart( pDBScenario->GetRecordID(), bFull );
	}
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NScenario;
//
REGISTER_SAVELOAD_CLASS( 0x50302100, CScenarioFlowChartBase );
REGISTER_SAVELOAD_CLASS( 0x50982100, CScenarioFlowChart );
REGISTER_SAVELOAD_CLASS( 0x52792140, CScenarioFlowChartState );
REGISTER_SAVELOAD_CLASS( 0x53092110, CScenarioFlowChartPathFinder );
REGISTER_SAVELOAD_TEMPL_CLASS( 0x50102140, CScenarioFlowChartItemsList<CScenarioZone>, CScenarioFlowChartItemsList );
REGISTER_SAVELOAD_TEMPL_CLASS( 0x50102141, CScenarioFlowChartItemsList<CScenarioClue>, CScenarioFlowChartItemsList );