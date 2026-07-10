#ifndef __SCENARIOTRACKER_H_
#define __SCENARIOTRACKER_H_
//
#include "..\MiscDll\Commands.h"
#include "../DBFormat/DataFormat.h"
#include "../DBFormat/DataScenario.h"
#include "../DBFormat/DataRPG.h"
//
struct SRandomSeed;
namespace NRPG
{
	class CUnit;
	class CGlobalPlayer;
}
namespace NWorld
{
	class CUnitServer;
}
//
namespace NGlobal
{
	class CCmd;
}
//
namespace NScenario
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioClue;
class CScenarioZone;
class CScenarioObjective;
class CScenarioFlowChart;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioTracker: public CObjectBase
{
	OBJECT_BASIC_METHODS( CScenarioTracker );
	//
	NGlobal::CCmd cmdScenario;
	NGlobal::CCmd cmdZone;
	//
	ZDATA
	bool bScenarioAvailable;
	CObj<CScenarioFlowChart> pScenarioFlowChart;
	list< CPtr<CScenarioZone> > availableZones; // available zones
	list< CPtr<CScenarioObjective> > finishedObjectives; // completed objectives
	list< CPtr<CScenarioZone> > blockedZones; // blocked zones
	list< CPtr<CScenarioClue> > takenClues;
	list< CPtr<CScenarioClue> > destroyedClues;
	int nZonesOpenOrder;
	int nCluesOpenOrder;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bScenarioAvailable); f.Add(3,&pScenarioFlowChart); f.Add(4,&availableZones); f.Add(5,&finishedObjectives); f.Add(6,&blockedZones); f.Add(7,&takenClues); f.Add(8,&destroyedClues); f.Add(9,&nZonesOpenOrder); f.Add(10,&nCluesOpenOrder); return 0; }
	//
	void PostCreateScenario();
	bool IsObjectiveFinished( CScenarioObjective *pObjective ) const;
	bool IsZoneContainsSomeClue( CScenarioZone *pZone ) const;
	bool IsZoneAvailable( CScenarioZone *pZone ) const;
	bool IsZoneBlocked( CScenarioZone *pZone ) const;
	void ExpandFlowChart();
	void ProcessCluesList( const list< CPtr<CScenarioClue> > &clues,
		NDb::EScenarioObjectiveType type );
	void JustFoundClue( CScenarioClue *pClue );
	void JustOpenZone( CScenarioZone *pZone );
	//
public:
	CScenarioTracker();
	//
	void CreateScenario( int nScenarioID );
	void CreateScenario( string szScenarioName );
	bool IsScenarioAvailable() const { return bScenarioAvailable; }
	void DrawScenario();
	void PrintScenarioList();
	int GetTemplateIDByVariantID( int nVariantID ) const;
	CScenarioZone* GetZone( int nTemplateID ) const;
	CScenarioZone* GetZoneByDBZone( NDb::CDBScenarioZone *pDBZone ) const;
	CScenarioZone* GetZoneByName( string szName ) const;
	CScenarioClue* GetClueByName( string szName ) const;
	CScenarioClue* GetClueByPersID( int nPersID ) const;
	bool IsClueFound( CScenarioClue *pClue ) const;
	//
	void GetPlacedClues( NScenario::CScenarioZone *pZone, 
		int nTemplateID, list< CPtr<CScenarioClue> > *clues ) const;
	void GetAvailableZones( list< CPtr<CScenarioZone> > *pZones ) const;
	void GetAvailableClues( list< CPtr<CScenarioClue> > *pClues ) const;
	CScenarioZone* GetRecommendedZone( NRPG::CGlobalPlayer *pPlayer ) const;
	void OpenZone( CScenarioZone *pZone );
	void BlockZone( CScenarioZone *pZone );
	void RevealZone( CScenarioZone *pZone ); // analog of OpenZone for accidental discovery of a zone
	void CheatOpenZone( CScenarioZone *pZone );
	//
	NDb::CString* GetClueDescriptionFromObjective( CScenarioClue *pClue ) const;
	//
	void OnObjectiveComplete( CScenarioObjective *pObjective );
	void CheatTakeClue( CScenarioClue *pClue, bool bImmediately = false );
	void CheatDestroyClue( CScenarioClue *pClue, bool bImmediately = false );
	bool OnScenarioClueTaken( int nID, bool bUnit );
	void OnScenarioClueDestroyed( int nID, bool bUnit );
	void ProcessScenario( const vector< CPtr<NRPG::CUnit> > &units );
	SRandomSeed GetRandomSeedForTemplate( int nTemplateID ) const;
	CScenarioZone* GetZoneInWhichClueWasFound( CScenarioClue *pClue ) const;
	void GetZonesWhichCanBeOpened( CScenarioClue *pClue, list< CPtr<CScenarioZone> > *pZones ) const;
	int GetScenarioID() const;
	int GetMaxDifficulty() const;
	//
	// script-goal/task API (lua Scenario{AddGoal,SetGoalComplete,SetTaskComplete}). Goals are stored
	// per-zone in CScenarioZone::scriptGoals; the player-facing journal that displays them is a
	// separate absent subsystem (documented in scriptScenario.cpp).
	void AddScriptGoal( CScenarioZone *pZone, int nGoalID );
	bool ScriptGoalSetComplete( CScenarioZone *pZone, int nGoalID, bool bComplete );
	bool ScriptTaskSetComplete( CScenarioZone *pZone, int nGoalID, int nTaskIdx, bool bComplete );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CSide *GetSideForScenario( CScenarioTracker *pScenario );
CScenarioTracker *CreateScenarioTracker( int nID );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __SCENARIOTRACKER_H_