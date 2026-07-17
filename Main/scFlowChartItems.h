#ifndef __FLOWCHARTITEMS_H_
#define __FLOWCHARTITEMS_H_
//
#include "../DBFormat/DataScenario.h"
//
namespace NScenario
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioClue;
class CScenarioObjective;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Task/goal completion state (release NScenario::ETaskState). Script goals/tasks carry it; the
// player-facing objectives journal that DISPLAYS it is a separate absent subsystem -- see the
// elision note in scriptScenario.cpp. The setter API is the lua Scenario{Add,SetGoal,SetTask} family.
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ETaskState
{
	TS_UNKNOWN = 0,
	TS_COMPLETED = 1,
	TS_FAILED = 2,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioTask -- runtime wrapper over an NDb::CScenarioTask DB record + a completion state +
// the clue-detection fields (retail ctor @0x2dfbc0 takes (pDBTask, pParentClue)).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioTask: public CObjectBase
{
	OBJECT_BASIC_METHODS( CScenarioTask );
	ZDATA
	CDBPtr<NDb::CScenarioTask> pDBTask;   // retail +0x0c
	CPtr<CScenarioClue> pParentClue;      // retail +0x10 (weak CPtr, exactly as retail)
	ETaskState eState;                    // retail +0x18
	bool bDetected;   // retail +0x14 (clue detection; consumers = the clue system, unwired -- W3)
	bool bVisible;    // retail +0x1c
	// retail @0x2e1560: 2=pDBTask, 3=pParentClue, 4=bDetected, 5=eState, 6=bVisible.
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pDBTask); f.Add(3,&pParentClue); f.Add(4,&bDetected); f.Add(5,&eState); f.Add(6,&bVisible); return 0; }
public:
	CScenarioTask(): eState( TS_UNKNOWN ), bDetected( false ), bVisible( true ) {}
	// retail @0x2dfbc0: pParentClue = the owning clue's runtime object (null for script-created tasks)
	CScenarioTask( NDb::CScenarioTask *_pDBTask, CScenarioClue *_pParentClue = 0 ): pDBTask( _pDBTask ), pParentClue( _pParentClue ), eState( TS_UNKNOWN ), bDetected( false ), bVisible( true ) {}
	NDb::CScenarioTask* GetDBTask() const { return pDBTask; }
	CScenarioClue* GetParentClue() const { return pParentClue; }
	ETaskState GetState() const { return eState; }
	void SetState( ETaskState _eState ) { eState = _eState; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioGoal -- runtime wrapper over an NDb::CScenarioGoal DB record + a per-goal completion state
// + the runtime tasks cloned from the DB goal's tasks. Appended to CScenarioZone::scriptGoals by the
// script ScenarioAddGoal API; goal/task state is set by ScenarioSetGoalComplete/ScenarioSetTaskComplete.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioGoal: public CObjectBase
{
	OBJECT_BASIC_METHODS( CScenarioGoal );
	ZDATA
	CDBPtr<NDb::CScenarioGoal> pGoal;
	vector< CObj<CScenarioTask> > tasks;
	ETaskState eState;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pGoal); f.Add(3,&tasks); f.Add(4,&eState); return 0; }
public:
	CScenarioGoal(): eState( TS_UNKNOWN ) {}
	// retail @0x2e0550: the parent clue is threaded into each cloned task's pParentClue
	CScenarioGoal( NDb::CScenarioGoal *_pGoal, CScenarioClue *pParentClue = 0 );
	NDb::CScenarioGoal* GetDBGoal() const { return pGoal; }
	const vector< CObj<CScenarioTask> >& GetTasks() const { return tasks; }
	ETaskState GetState() const { return eState; }
	void SetState( ETaskState _eState ) { eState = _eState; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioZone
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioZone: public CObjectBase
{
public:
	struct STemplate
	{
		ZDATA
		SRandomSeed sSeed;
		int nVariantID;
		int nItemSlots;
		int nPersonSlots;
		int nInventorySlots;
		int nEmptyItemSlots;
		int nEmptyPersonSlots;
		int nEmptyInventorySlots;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sSeed); f.Add(3,&nVariantID); f.Add(4,&nItemSlots); f.Add(5,&nPersonSlots); f.Add(6,&nInventorySlots); f.Add(7,&nEmptyItemSlots); f.Add(8,&nEmptyPersonSlots); f.Add(9,&nEmptyInventorySlots); return 0; }
		//
		STemplate(): sSeed( GetTickCount() ), nItemSlots( 0 ), nPersonSlots( 0 ), nInventorySlots( 0 ),
			nEmptyItemSlots( 0 ), nEmptyPersonSlots( 0 ), nEmptyInventorySlots( 0 ), nVariantID( 0 ) {}
	};
	OBJECT_BASIC_METHODS( CScenarioZone );
	ZDATA
	int nInnerID;
	int nDifficulty;
	bool bDifCalculated;
	bool bInaccessible;
	int nDistance;
	int nOpenOrder;
	bool bInitial;
	bool bInShortestPath;
	CDBPtr<NDb::CDBScenarioZone> pDBZone;
	vector< CPtr<CScenarioClue> > clues;
	vector< CPtr<CScenarioObjective> > blockers;
	unordered_map< int, STemplate > templates;
	bool bPassed;
	vector< CObj<CScenarioGoal> > scriptGoals;	// release-added: per-zone runtime goals appended by lua ScenarioAddGoal
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nInnerID); f.Add(3,&nDifficulty); f.Add(4,&bDifCalculated); f.Add(5,&bInaccessible); f.Add(6,&nDistance); f.Add(7,&nOpenOrder); f.Add(8,&bInitial); f.Add(9,&bInShortestPath); f.Add(10,&pDBZone); f.Add(11,&clues); f.Add(12,&blockers); f.Add(13,&templates); f.Add(14,&bPassed); f.Add(15,&scriptGoals); return 0; }
	//
private:
	int GetTemplateIDForClue( CScenarioClue *pClue );
	void GetVariantInfo( int nTemplateID, SRandomSeed sSeed, STemplate *templateData );
//
public:
	CScenarioZone() {}
	CScenarioZone( NDb::CDBScenarioZone *_pDBZone, int _nInnerID );
	//
	bool CanPlaceClue( NDb::EScenarioClueType type );
	void PlaceClue( CScenarioClue *pClue );
	void RemoveClue( CScenarioClue *pClue );
	void AddBlocker( CScenarioObjective *pBlocker );
	void AddScriptGoal( CScenarioGoal *pGoal ) { scriptGoals.push_back( pGoal ); }
	const vector< CObj<CScenarioGoal> >& GetScriptGoals() const { return scriptGoals; }
	//
	void DrawNode( fstream &file, int nID, bool bAvailable, bool bDrawBlockers = false );
	void DrawRelationship( fstream &file );
	//
	int GetDefaultTemplateID();
	int GetTemplateIDByVariantID( int nVariantID );
	int GetVariantIDForTemplate( int nTemplateID );
	SRandomSeed GetRandomSeedForTemplate( int nTemplateID ) { return templates[nTemplateID].sSeed; }
	void GetTemplatesIDs( vector<int> *pIDs );
	int GetInnerID() { return nInnerID; }
	int GetOpenOrder() { return nOpenOrder; }
	void SetOpenOrder( int _nOpenOrder ) { nOpenOrder = _nOpenOrder; }
	int GetDifficulty() { return nDifficulty; }
	void SetDifficulty( int _nDifficulty ) { nDifficulty = _nDifficulty; bDifCalculated = true; }
	bool IsDifCalculated() { return bDifCalculated; }
	void SetInaccessible( bool _bInaccessible ) { bInaccessible = _bInaccessible; }
	bool IsInaccessible() { return bInaccessible; }
	void SetCluesInaccessible( bool _bInaccessible );
	bool IsInitial() { return bInitial; }
	void SetInitial( bool _bInitial ) { bInitial = _bInitial; }
	bool IsInShortestPath() { return bInShortestPath; }
	void SetInShortestPath( bool _bInShortestPath ) {  bInShortestPath = _bInShortestPath; }
	void SetDistance( int _nDistance ) { nDistance = _nDistance; }
	bool IsPassed() { return bPassed; }
	void SetPassed( bool _bPassed = true ) { bPassed = _bPassed; }
	//
	const vector< CPtr<CScenarioClue> >& GetClues() { return clues; }
	const vector< CPtr<CScenarioObjective> >& GetBlockers() { return blockers; }
	NDb::CDBScenarioZone *GetDBZone() const { return pDBZone; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioClue
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioClue: public CObjectBase
{
	OBJECT_BASIC_METHODS( CScenarioClue );
	ZDATA
	CDBPtr<NDb::CDBScenarioClue> pDBClue;
	bool bPlaced;
	bool bCompound;
	bool bInaccessible;
	bool bJustFound;
	int nOpenOrder;
	int nInnerID;
	bool bInShortestPath;
	int nTemplateID; // which zone template it was placed on
	vector< CPtr<CScenarioObjective> > objectives;
	vector< CPtr<CScenarioObjective> > parentObjectives;
	vector< CPtr<CScenarioZone> > parentZones;
	bool bDestroyed;
	CObj<CScenarioGoal> pGoal;   // retail: runtime goal built from pDBClue->pGoal in the ctor @0x2e0770
	bool bDetected;              // retail: clue-detection flag (consumers = the clue system, unwired -- W3)
	// retail @0x2e2990: 2..14 as below + 15=pGoal (CObj), 16=bDetected
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pDBClue); f.Add(3,&bPlaced); f.Add(4,&bCompound); f.Add(5,&bInaccessible); f.Add(6,&bJustFound); f.Add(7,&nOpenOrder); f.Add(8,&nInnerID); f.Add(9,&bInShortestPath); f.Add(10,&nTemplateID); f.Add(11,&objectives); f.Add(12,&parentObjectives); f.Add(13,&parentZones); f.Add(14,&bDestroyed); f.Add(15,&pGoal); f.Add(16,&bDetected); return 0; }
public:
	//
	CScenarioClue(): bDetected( false ) {}
	CScenarioClue( NDb::CDBScenarioClue *_pDBClue, int _nInnerID );
	//
	bool CanPlaceObjective( CScenarioObjective *pObjective );
	void PlaceObjective( CScenarioObjective *pObjective );
	void SetPlaced( bool _bPlaced = true ) { bPlaced = _bPlaced; }
	bool IsPlaced() { return bPlaced; }
	//
	void DrawNode( fstream &file, bool bAvailable );
	void DrawRelationship( fstream &file );
	//
	void CreateObjectives();
	void RemoveParentObjective( CScenarioObjective *pObjective );
	void RemoveChildObjective( CScenarioObjective *pObjective );
	void ClearLinks();
	//
	void AddChildObjective( CScenarioObjective *pObjective );
	void AddParentObjective( CScenarioObjective *pObjective );
	void AddParentZone( CScenarioZone *pZone );
	//
	void SetCompound( bool _bCompound = true ) { bCompound = _bCompound; }
	bool IsCompound() { return bCompound; }
	int GetTemplateID() { return nTemplateID; }
	void SetTemplateID( int _nTemplateID ) { nTemplateID = _nTemplateID; }
	bool IsInShortestPath() { return bInShortestPath; }
	void SetInShortestPath( bool _bInShortestPath ) {  bInShortestPath = _bInShortestPath; }
	void SetInaccessible( bool _bInaccessible ) { bInaccessible = _bInaccessible; }
	bool IsInaccessible() { return bInaccessible; }
	bool IsCorrect();
	CScenarioObjective* GetObjectiveByType( NDb::EScenarioObjectiveType type );
	bool IsJustFound() { return bJustFound; }
	void SetJustFound( bool _bJustFound ) { bJustFound = _bJustFound; }
	int GetOpenOrder() { return nOpenOrder; }
	void SetOpenOrder( int _nOpenOrder ) { nOpenOrder = _nOpenOrder; }
	int GetInnerID() { return nInnerID; }
	bool IsDestroyed() { return bDestroyed; }
	void SetDestroyed( bool _bDestroyed ) { bDestroyed = _bDestroyed;	}
	CScenarioGoal* GetGoal() const { return pGoal; }
	bool IsDetected() const { return bDetected; }
	void SetDetected( bool _bDetected ) { bDetected = _bDetected; }
	//
	const vector< CPtr<CScenarioObjective> >& GetObjectives() { return objectives; }
	const vector< CPtr<CScenarioObjective> >& GetParentObjectives() { return parentObjectives; }
	const vector< CPtr<CScenarioZone> >& GetParentZones() { return parentZones; }
	NDb::CDBScenarioClue *GetDBClue() { return pDBClue; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioObjective
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioObjective: public CObjectBase
{
	OBJECT_BASIC_METHODS( CScenarioObjective );
	ZDATA
	CDBPtr<NDb::CDBScenarioObjective> pDBObjective;
	bool bPlaced;
	CPtr<CScenarioClue> pParentClue;
	int nInnerID;
	vector< CPtr<CScenarioZone> > zones;
	vector< CPtr<CScenarioClue> > clues;
	vector< CPtr<CScenarioZone> > zonesToBlock;
	vector< CPtr<CScenarioClue> > possibleParentClues;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pDBObjective); f.Add(3,&bPlaced); f.Add(4,&pParentClue); f.Add(5,&nInnerID); f.Add(6,&zones); f.Add(7,&clues); f.Add(8,&zonesToBlock); f.Add(9,&possibleParentClues); return 0; }
	//
public:
	CScenarioObjective() {}
	CScenarioObjective( NDb::CDBScenarioObjective *_pDBObjective, int _nInnerID );
	//
	void RemoveZone( CScenarioZone *pZone );
	void RemoveClue( CScenarioClue *pClue );
	void AddChildZone( CScenarioZone *pZone );
	void AddChildClue( CScenarioClue *pClue );
	void AddZoneToBlock( CScenarioZone *pZone );
	void AddPossibleParentClue( CScenarioClue *pClue );
	void ClearClues() { clues.clear(); }
	void ClearZones() { zones.clear(); }
	void ClearZonesToBlock() { zonesToBlock.clear(); }
	//
	void SetPlaced( bool _bPlaced = true ) { bPlaced = _bPlaced; }
	bool IsPlaced() { return bPlaced; }
	bool IsCorrect() { return IsPlaced() && ( !zones.empty() || !clues.empty() ); }
	bool IsZoneBlocker() { return !zonesToBlock.empty(); }
	CScenarioClue* GetParentClue() { return pParentClue; }
	void SetParentClue( CScenarioClue *pClue ) { pParentClue = pClue; }
	int GetInnerID() { return nInnerID; }
	//
	const vector< CPtr<CScenarioZone> >& GetZones() { return zones; };
	const vector< CPtr<CScenarioClue> >& GetClues() { return clues; };
	const vector< CPtr<CScenarioClue> >& GetPossibleParentClues() { return possibleParentClues; };
	const vector< CPtr<CScenarioZone> >& GetZonesToBlock() { return zonesToBlock; }
	NDb::CDBScenarioObjective* GetDBObjective() { return pDBObjective; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioZone* CreateScenarioZone( NDb::CDBScenarioZone *pDBZone, int nInnerID );
CScenarioClue* CreateScenarioClue( NDb::CDBScenarioClue *pDBZone, int nInnerID );
CScenarioObjective* CreateScenarioObjective( NDb::CDBScenarioObjective *pDBObjective, int nInnerID );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __FLOWCHARTITEMS_H_