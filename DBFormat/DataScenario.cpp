#include "StdAfx.h"
#include "DataFormat.h"
#include "..\Misc\StrProc.h"
//
#include "DataInterface.h"	// CUITexture, for CDBScenarioZone::pPWLImage import
#include "DataScenario.h"
//
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const char *pszScenarioClueTypes[N_CT_COUNT] = 
{
	"person",
	"item",
	"conclusion"
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const char *pszScenarioObjectiveTypes[N_OT_COUNT] = 
{
	"capture",
	"destroy"
};
////////////////////////////////////////////////////////////////////////////////////////////////////
EScenarioClueType GetClueTypeByName( const string &szName )
{
	string sz( szName );
	NStr::ToLower( sz );
	NStr::TrimBoth( sz );

	for ( int i = 0; i < N_CT_COUNT; ++i )
		if ( sz == pszScenarioClueTypes[i] )
			return (EScenarioClueType)i;
	return N_CT_COUNT;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EScenarioObjectiveType GetObjectiveTypeByName( const string &szName )
{
	string sz( szName );
	NStr::ToLower( sz );
	NStr::TrimBoth( sz );

	for ( int i = 0; i < N_OT_COUNT; ++i )
		if ( sz == pszScenarioObjectiveTypes[i] )
			return (EScenarioObjectiveType)i;
	return N_OT_COUNT;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBScenarioZone::Import()
{
	NDatabase::ImportField( "Scenario", &pScenario );
	templatesIDs.resize( 3 );
	NDatabase::ImportField( "TemplateID1", &templatesIDs[0] );
	NDatabase::ImportField( "TemplateID2", &templatesIDs[1] );
	NDatabase::ImportField( "TemplateID3", &templatesIDs[2] );
	NDatabase::ImportField( "ItemSlots", &nItemSlots );
	NDatabase::ImportField( "PersonSlots", &nPersonSlots );
	NDatabase::ImportField( "SmallDescription", &sSmallDescription );
	NDatabase::ImportField( "CluesMaxNumber", &nCluesMaxNumber );
	NDatabase::ImportField( "CanBeRevealed", &bCanBeRevealed );
	// --- release-added columns ---
	NDatabase::ImportField( "CanPKBeUsed", &bAllowPK );
	string szTimeOfDay;
	NDatabase::ImportField( "TimeOfDay", &szTimeOfDay );
	NDatabase::ImportField( "PWLImageID", &pPWLImage );
	NDatabase::ImportField( "Name", &pName );
	NDatabase::ImportField( "MaxDifficulty", &nMaxDifficulty );
	// release stores the single TimeOfDay token as the sole element of vszParams
	vszParams.clear();
	vszParams.push_back( szTimeOfDay );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBScenarioState::Import()
{
	NDatabase::ImportField( "Description", &sDescription );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBScenarioClue::Import()
{
	string szName;
	NDatabase::ImportField( "Type", &szName );
	clueType = GetClueTypeByName( szName );
	ASSERT( clueType < N_CT_COUNT );
	NDatabase::ImportField( "Scenario", &pScenario );
	NDatabase::ImportField( "State", &pState );
	zonesToPlace.resize( 3 );
	NDatabase::ImportField( "ZoneToPlace1", &zonesToPlace[0] );
	NDatabase::ImportField( "ZoneToPlace2", &zonesToPlace[1] );
	NDatabase::ImportField( "ZoneToPlace3", &zonesToPlace[2] );
	objectives.resize( 2 );
	NDatabase::ImportField( "Objective1", &objectives[0] );
	NDatabase::ImportField( "Objective2", &objectives[1] );
	NDatabase::ImportField( "SmallDescription", &sSmallDescription );
	NDatabase::ImportField( "ItemID", &nItemID );
	NDatabase::ImportField( "PersID", &nPersID );
	NDatabase::ImportField( "Permanent", &bPermanent );
	NDatabase::ImportField( "MinParentToOpen", &nMinParentToOpen );
	NDatabase::ImportField( "Description", &pDescription );
	NDatabase::ImportField( "GiveImmediately", &bGiveImmediately );
	NDatabase::ImportField( "GiveByScript", &bGiveByScript );
	NDatabase::ImportField( "GoalID", &pGoal );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioTask::Import()
{
	NDatabase::ImportField( "Tag", &szTag );
	NDatabase::ImportField( "Description", &pDescription );
	if ( szTag == "Find item" )
		eTag = TT_FIND_ITEM;
	else if ( szTag == "Destroy item carrier" )
		eTag = TT_DESTROY_ITEM_CARRIER;
	else if ( szTag == "Take item" )
		eTag = TT_TAKE_ITEM;
	else if ( szTag == "Carry out item" )
		eTag = TT_CARRY_OUT_ITEM;
	else if ( szTag == "Find person" )
		eTag = TT_FIND_PERSON;
	else if ( szTag == "Stun person" )
		eTag = TT_STUN_PERSON;
	else if ( szTag == "Take person" )
		eTag = TT_TAKE_PERSON;
	else if ( szTag == "Carry out person" )
		eTag = TT_CARRY_OUT_PERSON;
	else if ( szTag == "Complete by script" )
		eTag = TT_COMPLETE_BY_SCRIPT;
	else
		eTag = TT_FIND_ITEM;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioGoal::Import()
{
	NDatabase::ImportField( "GoalName", &pName );
	tasks.clear();
	for ( int i = 1; i < 7; ++i )
	{
		char szCol[64];
		sprintf( szCol, "Task%d", i );
		CPtr<CScenarioTask> pTask;
		NDatabase::ImportField( szCol, &pTask );
		if ( IsValid( pTask ) )
			tasks.push_back( pTask );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBScenarioObjective::Import()
{
	string szName;
	NDatabase::ImportField( "Type", &szName );
	type = GetObjectiveTypeByName( szName );
	ASSERT( type < N_OT_COUNT );
	zonesToOpen.resize( 3 );
	NDatabase::ImportField( "ZoneToOpen1", &zonesToOpen[0] );
	NDatabase::ImportField( "ZoneToOpen2", &zonesToOpen[1] );
	NDatabase::ImportField( "ZoneToOpen3", &zonesToOpen[2] );
	zonesToBlock.resize( 3 );
	NDatabase::ImportField( "ZoneToBlock1", &zonesToBlock[0] );
	NDatabase::ImportField( "ZoneToBlock2", &zonesToBlock[1] );
	NDatabase::ImportField( "ZoneToBlock3", &zonesToBlock[2] );
	NDatabase::ImportField( "Description", &pDescription );
	NDatabase::ImportField( "Scenario", &pScenario );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBScenario::Import()
{
	NDatabase::ImportField( "Name", &szName );
	NDatabase::ImportField( "Description", &szDescription );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBScenarioObjective2Clue::Import()
{
	NDatabase::ImportField( "ObjectiveID", &pObjective );
	NDatabase::ImportField( "ClueID", &pClue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NDb;
//
REGISTER_SAVELOAD_CLASS( 0x50882180, CDBScenarioZone );
REGISTER_SAVELOAD_CLASS( 0x50882181, CDBScenarioState );
REGISTER_SAVELOAD_CLASS( 0x50882182, CDBScenarioClue );
REGISTER_SAVELOAD_CLASS( 0x50982090, CDBScenarioObjective );
REGISTER_SAVELOAD_CLASS( 0x51582130, CDBScenario );
REGISTER_SAVELOAD_CLASS( 0x50392125, CDBScenarioObjective2Clue );