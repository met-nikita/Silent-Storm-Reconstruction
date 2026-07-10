#ifndef __MAPBUILD_H__
#define __MAPBUILD_H__

#include "PolyUtils.h"
#include "aiPosition.h"
#include "aiWaypoint.h"
#include "TerrainInfo.h"
#include "MapBuildingInfo.h"
#include "..\Misc\RandomGen.h"
#include "..\DBFormat\DataChest.h"	// NDb::CRPGChestReal (SMapUnit::pBackpack), NDb::CTRPGChest

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
	bool bLightmap;								// light-source type in the container
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
	// release door/chest lock params (retail folds pGrenade + these into a nested SMapDoorParams;
	// kept flat here so existing consumers of pGrenade stay untouched):
	bool bIsLocked;
	int nKeyID;
	int nLockHardness;
	bool bIsChest;				// the element carries a loot chest (CFinalElement::pChest)
	bool bIsTransparentIfOpen;	// chest containers turn transparent when open
	SMapElement(): bOpen(false), nObjectPhase(0), bBorder( false ), nDC(0),
		bIsLocked(false), nKeyID(0), nLockHardness(0), bIsChest(false), bIsTransparentIfOpen(false) {}
	SMapElement( NDb::CObject *_pObject, SMapPosition _pos, bool _bBorder = false ):
		pObject( _pObject ), pos( _pos ), bOpen( false ), nObjectPhase( 0 ),
		ptAlignTo( CVec2( _pos.ptPos.x, _pos.ptPos.y ) ), bBorder( _bBorder ), nDC(0),
		bIsLocked(false), nKeyID(0), nLockHardness(0), bIsChest(false), bIsTransparentIfOpen(false) {}
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
	// release chest-loot tail (retail SMapRPGElement +69/+70; ctor @0x279c10 inits both false):
	bool bVertical;		// items stand upright inside the chest (CRPGChestLayout::bVertical)
	bool bFromChest;	// this placed item was spilled out of a loot chest at map build
	SMapRPGElement(): bOpen(false), nDC(0), bVertical(false), bFromChest(false) {}
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
	// release loot-chest tail (retail SMapUnit +108/+112; copy ctor @0x27af50, default ctor @0x27b420):
	CDBPtr<NDb::CRPGItem> pInHandItem;			// rolled from CRPGPers::pHandWeapon at map build
	CObj<NDb::CRPGChestReal> pBackpack;			// rolled from CRPGPers::pBackpackWeapon (+ clips from the hand chest)
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
	// retail SMapInfo carries the variant's NoAttack flag next to bShowTerrain (TraverseTemplateTree
	// @0x276c60 copies both at the nDepth==1 root); CWorld::CreateRandom @0x36d0b0 turns it into
	// CWorld::bAttackAllowed = !bNoAttack (combat prohibited inside base zones, e.g. Gbase4 NoAttack=1).
	bool bNoAttack = false;

	SMapInfo() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void ConvertFlags( vector<int> *pFlags, const vector<string> &strParams );
// nRelativeLevel: the map/mission relative level (retail free BuildMap @0x2788b0 stores it into the
// builder; it gates chest-loot rolls [max(0,lvl-4)..lvl, or 0..100 when lvl==0] and lock hardness).
bool BuildMap( int nPlacementID, const vector<string> &strParams,
	NAI::IPathNetwork *pNet, SMapInfo *pInfo, int nDepth = -1, SRandomSeed sSeed = SRandomSeed(),
	int nRelativeLevel = 0 );
////////////////////////////////////////////////////////////////////////////////////////////////////
bool BuildTerrain( int nMapID, SMapInfo *pInfo, int nDepth, bool bResetPins, bool bShowHoles ); // MapEdit
bool BuildMapEditMap( int nMapID, NAI::IPathNetwork *pNet, SMapInfo *pInfo, int nDepth, const SMapPosition &pos, 
	const CVec2 &ptBuilding, bool bTerrAlign );
////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // __MAPBUILD_H__