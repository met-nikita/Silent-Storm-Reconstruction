#pragma once
#include <stack>

const int N_MIN_CODE = 4;
typedef string CClueP;
typedef string CZoneP;
class CClue;
class CZone
{
public:
	string description;
	string country;
	string location;
	string code;
	int nItemSlot;
	int nPersonSlot;
	int nMinLvl, nMaxLvl;
	bool bUnachievable;
	vector<CClueP> placed_clues;

	int nDistance;
	CZoneP pComeFrom;
	bool isInShortPath;
	int nDifficulty;
	bool bProcessed;
	bool bDifCalculated;

	int *GetPlaceNum( string type );
	int GetPlaceNumByType( string type );
	bool CanPlaceClue( CClue* clue );
	string GetLabel();
};

class CObjective
{
public:
	string action;
	string description;
	vector<CZoneP> zone2open;
	vector<CClueP> cluesToOpen;
	vector<CZoneP> zone2destroy;
};

class CClue
{
public:
	string description;
	string code;
	string type;
	bool isPermanent;
	bool isPlaced;
	int  nDerClues, nTmpDerClues, nMinClueToOpen;
	vector<CZoneP> zone2place;
	vector<CObjective> objectives;
	vector<CClueP> whoOpenMe;
	int nItemID;
	int nPersID;

	int nMaxDistToOpen;
	bool IsClueOpened() const
	{
		if ( nMinClueToOpen <= 0 )
			return nTmpDerClues == 0;
		else
			return nDerClues - nMinClueToOpen <= nTmpDerClues;
	}
	void ResetTmpDerClues() { nTmpDerClues = nDerClues; }
};

class CScenGraph
{
	int nRandomSeed;
	void SaveOneZone( fstream &file, CZone &zone, const char chStrat = ' ' );
	void SaveZones( fstream &file );
	void SaveCheck( fstream &file );
	bool PlaceGroup( vector<CClue*> &clues );
	void PushZoneLinks( CZone &zone, stack<CZone*> &links );
	void PushClueLinks( CZone &zoneFrom, CClue &clue, stack<CZone*> &links );
	bool PlaceClue( CZone &zone, CClue* clue );
	CZone &GetZone( CZoneP zoneName );
	CClue &GetClue( CClueP clueName );
	void FindZonesOpenedByClue( CClueP clueName, list<CZoneP> *OpenedZones, CZoneP parent, bool bUseProcessed = true );
	void FindChildZones( CZoneP zoneName, list<CZoneP> *childZones, bool bUseProcessed = true );
	void GetPath( CZoneP start, list<CZoneP> *path, bool (* IsFinal)( CZone zone ) );
	bool FindUnprocessedBranch( CZoneP *start, CZoneP *finish );
	bool IsBeginZone( CZoneP zoneName );
	void InitDifficulties();
	void SetPathDifficulties( const list<CZoneP> &path );
public:
	unordered_map<string,CZone> zones;
	unordered_map<string,CClue> clues;
	vector<CClueP> clues_index;
	unordered_map< string, vector<CZoneP> > byLocation;
	unordered_map< string, vector<CClueP> > byGroup;

	CScenGraph() : bNeedItems(true), strSep(".3"), bUniteZones(false) {}
	bool LoadZones( string strFileName );

	// Export parametrs
	string strSep;
	bool bUniteZones;
	bool bNeedItems;
	void SaveGraph( string fileName );
	void SaveSeparateGraph( string fileName );

	void GenerateFullGraph();
	void GenerateGraph();
	void GenerateCorrectGraph();
	void CalculateDifficulties();
	bool Check();
	CZone *GZ( const string str )
	{
		if ( zones.find(str) == zones.end() )
		{
			printf( "Can't find zone: %s\n", str.c_str() );
			exit(0);
		}
		return &zones[str];
	}
	CClue *GC( const string str )
	{
		if ( clues.find(str) == clues.end() )
		{
			printf( "Can't find clue: %s\n", str.c_str() );
			exit(0);
		}
		return &clues[str];
	}
};
