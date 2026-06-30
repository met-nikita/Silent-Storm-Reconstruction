#ifndef __MAPBUILD_H__
#define __MAPBUILD_H__

#include "PolyUtils.h"
#include "aiPosition.h"
#include "aiWaypoint.h"
#include "TerrainInfo.h"
#include "MapBuildingInfo.h"
#include "..\Misc\RandomGen.h"

struct SRandomSeed;
namespace NBuilding
{
	class CBuildingGrid;
}
namespace NDb
{
	class CObject;
	class CTemplVariant;
	class CRPGPers;
	class CAmbientLightReal;
	class CWaypointName;
	class CRPGItem;
	class CScript;
	class CRPGGrenade;
	enum EScenarioClueType;
	enum EUnitPose;
	enum EUnitLogic;
	class CAnimation;
}
namespace NAI
{
	class IPathNetwork;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMapElement
{
	CPtr<NDb::CObject> pObject;
	SMapPosition pos;
	int nRelFloor;
	bool bLightmap;								// тип источника света в контейнере
	CVec2 ptAlignTo;
	bool bOpen;
	int nPassageZoneID;
	int nPassageObjectID;
	int nAPRadius;
	string szName;
	int nObjectPhase;
	vector<int> flags;
	bool bBorder;
	int nDC;
	CPtr<NDb::CRPGGrenade> pGrenade;
	SMapElement(): bOpen(false), nObjectPhase(0), bBorder( false ) {}
	SMapElement( NDb::CObject *_pObject, SMapPosition _pos, bool _bBorder = false ):
		pObject( _pObject ), pos( _pos ), bOpen( false ), nObjectPhase( 0 ),
		ptAlignTo( CVec2( _pos.ptPos.x, _pos.ptPos.y ) ), bBorder( _bBorder ), nDC(0) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMapRPGElement
{
	CPtr<NDb::CRPGItem> pItem;
	SMapPosition pos;
	int nRelFloor;
	CVec2 ptAlignTo;
	bool bOpen;
	string szName;
	int nDC;
	bool bArmed;
	SMapRPGElement(): bOpen(false), nDC(0) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMapWaypoint: public CObjectBase
{
	OBJECT_BASIC_METHODS(CMapWaypoint);
public:
	CDBPtr<NDb::CWaypointName> pName;
	vector<NAI::SCommand> commands;
	CVec2 ptAlignTo;
	bool bExists;
	SMapPosition pos;

	CMapWaypoint(): bExists(false) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMapUnit
{
	int nUnitID; // for route generation;
	CDBPtr<NDb::CRPGPers> pPers;
	SMapPosition pos;
	vector<CPtr<CMapWaypoint> > route;
	bool bSlot;
	int nDiplomacy;
	int nScenarioPlayer;
	int nRelativeLevel;
	string szName;
	NDb::EUnitPose eInitialPose;
	NDb::EUnitLogic eLogic;
	int nRoamingRadius;
	bool bFearUseToHit;
	CVec2 ptAlignTo;
	CDBPtr<NDb::CAnimation> pGuardAnimation;
	SMapUnit() : bSlot(false), nDiplomacy(0), nScenarioPlayer(0), nRelativeLevel(0), nRoamingRadius(0) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMapWall
{
	ZDATA
	int nHeightMin;
	int nHeightMax;
	CVec2 vBeg;
	CVec2 vEnd;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nHeightMin); f.Add(3,&nHeightMax); f.Add(4,&vBeg); f.Add(5,&vEnd); return 0; }
};
struct SMapHole
{
	ZDATA
	int nFloor;
	int nHeight;
	bool bVisible;
	TPolygonsList polygonsList;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nFloor); f.Add(3,&nHeight); f.Add(4,&bVisible); f.Add(5,&polygonsList); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SDeploySpot
{
	SMapPosition pos;
	//CVec3 ptPos;
	//float fAngle;
	CVec2 ptAlignTo;
	//int nID;

	SDeploySpot() {}
	SDeploySpot( const SMapPosition &_pos ): pos(_pos) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SClueSlot
{
	SMapPosition pos;
	CVec2 ptAlignTo;
	//
	int nUnitID;
	CDBPtr<NDb::CRPGPers> pPers;
	bool bPersSlot;
	bool bInventorySlot;
	SClueSlot(): bInventorySlot(false), bPersSlot(false) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SUnitGroup
{
	vector<int> units;					// Unit IDs
	vector<CPtr<CMapWaypoint> > route;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMapInfo
{
	int nBaseTerrainFloor;
	list<SMapWall> wallsList;
	list<SMapHole> holesList;
	list<SMapElement> items;
	list<SMapRPGElement> rpgitems;
	list<SMapUnit> units;
	list< CObj<CMapWaypoint> > waypoints;
	list< CDBPtr<NDb::CScript> > scripts;
	vector<SMapBuilding> buildings;
	STerrainInfo terrain;
	CTRect<float> sMapSafeZone;
	CPtr<NDb::CAmbientLightReal> pDefaultLight;
	vector<SDeploySpot> deploySpots;
	vector<SClueSlot> slots;
	unordered_map<int, SUnitGroup> groups;
	bool bShowTerrain;

	SMapInfo() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void ConvertFlags( vector<int> *pFlags, const vector<string> &strParams );
bool BuildMap( int nPlacementID, const vector<string> &strParams, 
	NAI::IPathNetwork *pNet, SMapInfo *pInfo, int nDepth = -1, SRandomSeed sSeed = SRandomSeed() );
////////////////////////////////////////////////////////////////////////////////////////////////////
bool BuildTerrain( int nMapID, SMapInfo *pInfo, int nDepth, bool bResetPins, bool bShowHoles ); // MapEdit
bool BuildMapEditMap( int nMapID, NAI::IPathNetwork *pNet, SMapInfo *pInfo, int nDepth, const SMapPosition &pos, 
	const CVec2 &ptBuilding, bool bTerrAlign );
////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // __MAPBUILD_H__