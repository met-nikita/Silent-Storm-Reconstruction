#ifndef __FLOWCHART_H_
#define __FLOWCHART_H_
//
namespace NDb
{
	class CDBScenarioClue;
	class CDBScenarioZone;
	class CDBScenarioObjective;
}
//
namespace NScenario
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioZone;
class CScenarioClue;
class CScenarioObjective;
class CScenarioFlowChartPathFinder;
struct SScenarioFlowChartPassage;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioFlowChartBase
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioFlowChartBase: public CObjectBase
{
	OBJECT_BASIC_METHODS( CScenarioFlowChartBase );
	ZDATA
	bool bFull;
	int nScenarioID;
protected:
	vector< CObj<CScenarioZone> > zones;
	vector< CObj<CScenarioClue> > clues;
	vector< CObj<CScenarioObjective> > objectives;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bFull); f.Add(3,&nScenarioID); f.Add(4,&zones); f.Add(5,&clues); f.Add(6,&objectives); return 0; }
	//
private:
	void PlaceClue( CScenarioClue *pClue );
	void PlaceObjective( CScenarioObjective *pObjective );
	void SetupLinks( CScenarioObjective *pObjective );
	bool CheckDataCorrectness();
	void LoadItems();
	void ClearItems();
	//
	void Header( fstream &file, string szFileName );
	void Footer( fstream &file, string szFileName );
	void DrawZones( fstream &file, const list< CPtr<CScenarioZone> > &availableZones );
	void DrawClues( fstream &file, const list< CPtr<CScenarioClue> > &availableClues );
	void DrawZonesRelationships( fstream &file );
	void DrawCluesRelationships( fstream &file );
	//
protected:
	void Generate();
	void RemoveClue( CScenarioClue *pClue );
	//
public:
	CScenarioFlowChartBase() {}
	CScenarioFlowChartBase( int _nScenarioID, bool _bFull );
	//
	void Draw( const list< CPtr<CScenarioZone> > &availableZones, 
		const list< CPtr<CScenarioClue> > &availableClues, const char *pszOutputFile = 0 );
	//
	CScenarioZone *GetZoneByDBZone( NDb::CDBScenarioZone *pDBZone );
	CScenarioZone *GetZoneByName( string szName );
	CScenarioZone *GetZoneByTemplateID( int nTemplateID );
		CScenarioClue *GetClueByItemID( int nItemID );
	CScenarioClue *GetClueByPersID( int nPersID );
	CScenarioClue *GetClueByName( string szName );
	CScenarioClue *GetClueByDBClue( NDb::CDBScenarioClue *pDBClue );
	CScenarioObjective *GetObjectiveByDBObjective( NDb::CDBScenarioObjective *pDBObjective );
	int GetTemplateIDByVariantID( int nVariantID );
	//
	void GetClues( vector< CPtr<CScenarioClue> > *pClues );
	void GetObjectives( vector< CPtr<CScenarioObjective> > *pObjectives );
	void GetZones( vector< CPtr<CScenarioZone> > *pZones );
	int GetScenarioID() { return nScenarioID; }
	bool IsFull() { return bFull; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioFlowChart
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioFlowChart: public CScenarioFlowChartBase
{
	OBJECT_BASIC_METHODS( CScenarioFlowChart );
	ZDATA
	ZPARENT( CScenarioFlowChartBase );
	CObj<CScenarioFlowChartPathFinder> pPathFinder;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CScenarioFlowChartBase *)this); f.Add(3,&pPathFinder); return 0; }
	//
private:
	bool CheckCorrectness();
	void CalculateDifficulties();
	void RemoveIncorrectClues();
	void MarkInaccessible();
	void MarkInShortestPath();
	void MarkDistance();
	void InitDifficulties();
	void SetPathDifficulty( const list< CPtr<CScenarioZone> > &path, bool bCheckBegin );
	bool GetEdgeForDifCalculation( CScenarioZone **ppBegin, CScenarioZone **ppEnd );
	//
public:
	//
	CScenarioFlowChart() {}
	CScenarioFlowChart( int _nScenarioID, bool _bFull );
	//
	CScenarioFlowChartPathFinder* GetPathFinder() { return pPathFinder; }
	void CScenarioFlowChart::GetZonesWhichCanBeOpened( CScenarioClue *pClue, 
		list< CPtr<CScenarioZone> > *pZones );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioFlowChartItemsList
////////////////////////////////////////////////////////////////////////////////////////////////////
template < class T >
class CScenarioFlowChartItemsList: public CObjectBase
{
	OBJECT_BASIC_METHODS( CScenarioFlowChartItemsList );
	ZDATA
	vector<DWORD> signature;
	list< CPtr<T> > items;
	bool bUpdateSignature;
	int nMaxSize;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&signature); f.Add(3,&items); f.Add(4,&bUpdateSignature); f.Add(5,&nMaxSize); return 0; }
	//
	void UpdateSignature();
	void CopyItemsList( CScenarioFlowChartItemsList<T> &pItemsList );
	//
public:
	CScenarioFlowChartItemsList( int _nMaxSize = 0 );
	//
	const list< CPtr<T> >& GetItems() const;
	const vector<DWORD>& GetSignature();
	int GetMaxSize() const;
	void Push( T *pItem );
	void Erase( T *pItem );
	bool IsContainItem( T *pItem );
	bool IsContainList( CScenarioFlowChartItemsList<T> &itemsList );
	void operator=( CScenarioFlowChartItemsList<T> &itemsList );
	bool operator==( CScenarioFlowChartItemsList<T> &itemsList );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioFlowChartState
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioFlowChartState: public CObjectBase
{
	OBJECT_BASIC_METHODS( CScenarioFlowChartState );
	ZDATA
	CPtr<CScenarioFlowChartPathFinder> pPathFinder;
	CObj< CScenarioFlowChartItemsList<CScenarioZone> > pZones;
	CObj< CScenarioFlowChartItemsList<CScenarioZone> > pBranchingZones;
	CObj< CScenarioFlowChartItemsList<CScenarioZone> > pLockedZones;
	CObj< CScenarioFlowChartItemsList<CScenarioClue> > pClues;
	CObj< CScenarioFlowChartItemsList<CScenarioClue> > pUsedClues;
	bool bFinal;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pPathFinder); f.Add(3,&pZones); f.Add(4,&pBranchingZones); f.Add(5,&pLockedZones); f.Add(6,&pClues); f.Add(7,&pUsedClues); f.Add(8,&bFinal); return 0; }
	//
	void CopyState( CScenarioFlowChartState state );
	void CreateLists();
	void UpdateBranchingZones();
	bool IsBranchingZone( CScenarioZone *pZone );
	void UpdateClues( CScenarioZone *pZone );
	void UpdateClues( CScenarioClue *pClue );
	bool CanUnlockClue( CScenarioClue *pClue );
	CScenarioZone* GetFrontZone();
	void LockZone( CScenarioZone *pZone );
	void UsePassage( const SScenarioFlowChartPassage &passage );
	//
public:
	CScenarioFlowChartState() {}
	CScenarioFlowChartState( CScenarioFlowChartPathFinder *_pPathFinder );
	CScenarioFlowChartState( CScenarioFlowChartState *_pState );
	//
	CScenarioFlowChartItemsList<CScenarioZone>* GetZones() const { return pZones; }
	CScenarioFlowChartItemsList<CScenarioZone>* GetBranchingZones() const { return pBranchingZones; }
	CScenarioFlowChartItemsList<CScenarioZone>* GetLockedZones() const { return pLockedZones; }
	CScenarioFlowChartItemsList<CScenarioClue>* GetClues() const { return pClues; }
	CScenarioFlowChartItemsList<CScenarioClue>* GetUsedClues() const { return pUsedClues; }
	//
	void OutputDebugInfo();
	//
	int GetHashKey();
	bool IsContain( CScenarioZone *pZone );
	bool IsContainState( CScenarioFlowChartState *pState );
	bool IsClueLocked( CScenarioClue *pClue );
	bool IsZoneLocked( CScenarioZone *pZone );
	void operator=( CScenarioFlowChartState &state );
	bool operator==( CScenarioFlowChartState &state );
	void operator+=( CScenarioZone *pZone );
	CScenarioFlowChartPathFinder* GetPathFinder() const;
	void GetNextStates( list< CPtr<CScenarioFlowChartState> > *pNextStates, 
		bool ( * IsFinal )( CScenarioZone * ), bool bUseLocks );
	void MarkAsShortestPath();
	void MarkAsAccessible();
	void MarkDistance();
	bool IsFinal() { return bFinal; }
	void SetFinal( bool _bFinal ) { bFinal = _bFinal; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioFlowChartPathFinder
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SScenarioFlowChartPassage
{
	ZDATA
	CPtr<CScenarioZone> pFrom;
	CPtr<CScenarioZone> pTo;
	list< CPtr<CScenarioClue> > clues;
	list< CPtr<CScenarioObjective> > objectives;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pFrom); f.Add(3,&pTo); f.Add(4,&clues); f.Add(5,&objectives); return 0; }
	//
	void operator+=( SScenarioFlowChartPassage &passage );
	void operator=( SScenarioFlowChartPassage &passage );
	SScenarioFlowChartPassage operator+( SScenarioFlowChartPassage &passage );
	SScenarioFlowChartPassage operator+( CScenarioClue *pClue );
	SScenarioFlowChartPassage operator+( CScenarioObjective *pObjective );
	SScenarioFlowChartPassage() {}
	SScenarioFlowChartPassage( CScenarioZone *_pFrom, CScenarioZone *_pTo );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioFlowChartPathFinder: public CObjectBase
{
	OBJECT_BASIC_METHODS( CScenarioFlowChartPathFinder );
	ZDATA
	CPtr<CScenarioFlowChartBase> pFlowChart;
	unordered_map< int, list< CObj<CScenarioFlowChartState> > > passedStates;
	unordered_map< int, list< CObj<CScenarioFlowChartState> > > finalStates;
	list< CObj<CScenarioFlowChartState> > states;
	unordered_map< CPtr<CScenarioClue>, int, SPtrHash > minParentToOpen;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pFlowChart); f.Add(3,&passedStates); f.Add(4,&finalStates); f.Add(5,&states); f.Add(6,&minParentToOpen); return 0; }
	//
	bool IsStatePassed( CScenarioFlowChartState *pState );
	void PassState( CScenarioFlowChartState *pState );
	void FinalState( CScenarioFlowChartState *pState );
	void CalculateMinParentToOpen();
	CScenarioFlowChartState* GetBestState( const unordered_map< int, list< CObj<CScenarioFlowChartState> > > &states,
		bool ( CScenarioFlowChartPathFinder::* Compare )( CScenarioFlowChartState *, CScenarioFlowChartState * ) );
	bool CompareSize( CScenarioFlowChartState *pState1, CScenarioFlowChartState *pState2 );
	//
	void FindPathA( CScenarioZone *pFrom, bool (* IsFinal)( CScenarioZone * ), bool bUseLocks );
	//
public:
	CScenarioFlowChartPathFinder() {}
	CScenarioFlowChartPathFinder(	CScenarioFlowChartBase *_pFlowChart );
	//
	CScenarioFlowChartBase* GetFlowChart() const;
	//
	void GetChildZones( CScenarioFlowChartState *pState,
		CScenarioObjective *pObjective, list<SScenarioFlowChartPassage> *pPassages );
	void GetChildZones( CScenarioFlowChartState *pState,
		CScenarioClue *pClue, list<SScenarioFlowChartPassage> *pPassages );
	void GetChildZones( CScenarioFlowChartState *pState,
		CScenarioZone *pZone, list<SScenarioFlowChartPassage> *pPassages );
	void GetPossibleParentClues( CScenarioClue *pClue, list< CPtr<CScenarioClue> > *pClues );
	void GetParentClues( CScenarioClue *pClue, list< CPtr<CScenarioClue> > *pClues );
	void GetZonesFromPassages( const list<SScenarioFlowChartPassage> &passages,
		list< CPtr<CScenarioZone> > *pZones );
	SScenarioFlowChartPassage GetPassageForZone( 
		const list<SScenarioFlowChartPassage> &passages, CScenarioZone *pZone );
	int GetMinParentToOpen( CScenarioClue *pClue );
	//
	CScenarioFlowChartState* FindPath( CScenarioZone *pFrom, 
		CScenarioZone *pTo, bool bUseLocks = true );
	CScenarioFlowChartState* FindPath( CScenarioZone *pFrom );
	void MarkAsAccessible();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioFlowChart* CreateScenarioFlowChart( int nScenarioID, bool bFull );
CScenarioFlowChart* CreateScenarioFlowChart( string szScenarioName, bool bFull );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __FLOWCHART_H_