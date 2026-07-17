#ifndef __DATAMAP_H_
#define __DATAMAP_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
#include "..\Misc\Geom.h"
#include "DataConst.h"
#include "DataRPG.h"
#include "DataFormat.h"
#include "DataSound.h"
struct SRand;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CTRndModel;
	class CTAmbientLight;
	class CTemplVariant;
	class CRectangle;
	class CFinalElement;
	class CUnit;
	class CRPGPers;
	class CRPGArmor;
	class CWall;
	class CFloor;
	class CIntermediateFloor;
	class CSolid;
	class CIntermediateSolid;
	class CContainer;
	class CEffect;
	class CRoom;
	class CTexture;
	class CExplosion;
	class CTemplVariant;
	class CMaterial;
	class CPlacableObject;
	class CTMaterial;
	class CTSound;
	class CSoundEffect;
	class CGeometry;
	class CObject;
	class CModel;
	class CScript;
	class CSpot;
	class CDBScenarioZone;
	class CUnitGroup;
	class CMusic;
	class CDBDiplomacy;
	class CDBScenario;
	class CDBScenarioZone;
	class CUITexture;
	class CAnimation;
	class CRPGGrenade;
	class CTRPGChest;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGlobalMap: public CDBRecord
{
	OBJECT_BASIC_METHODS(CGlobalMap);
public:
	ZDATA_(CDBRecord)
	CPtr<CUITexture> pBackground;
	CPtr<CDBScenario> pScenario;
	CPtr<CDBScenarioZone> pBaseZone;
	CPtr<CDBScenarioZone> pStartZone;            // release pStartZone (GlobalMaps.StartZoneID): the faction intro-mission zone
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pBackground); f.Add(3,&pScenario); f.Add(4,&pBaseZone); f.Add(5,&pStartZone); return 0; }
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CChapterMap: public CDBRecord
{
	OBJECT_BASIC_METHODS(CChapterMap);
public:
	ZDATA_(CDBRecord)
	vector<int> campZonesSet;
	CPtr<CUITexture> pPWLImage;		// retail NDb::CChapterMap +0x20, col "PWLImageID" (place-where-lost / loading splash)
	CPtr<CUITexture> pBackground;	// +0x24, col "Background" (the chapter-MAP screen background -- a different resource)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&campZonesSet); f.Add(3,&pBackground); f.Add(4,&pPWLImage); return 0; }
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTemplate : public CRndPtr<CTemplVariant>
{
	OBJECT_BASIC_METHODS(CTemplate);
public:
	int nWidth;
	int nHeight;

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTerrainSpot: public CObjectBase
{
	OBJECT_BASIC_METHODS(CTerrainSpot);
public:
	ZDATA
	CPtr<CMaterial> pMaterial;
	CDBPtr<CRPGArmor> pArmor;
	CVec2 ptPos;
	CVec2 ptSize;
	int nRotation;
	int nPriority;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pMaterial); f.Add(3,&pArmor); f.Add(4,&ptPos); f.Add(5,&ptSize); f.Add(6,&nRotation); f.Add(7,&nPriority); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRndTerrainSpot: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRndTerrainSpot);
public:
	CPtr<CSpot> pSpot;
	CVec2 ptPos;
	CVec2 ptSize;
	int nRotation;
	int nPriority;
	CPtr<CTemplVariant> pVar;
	int nLayer; //     

	virtual void Import();
	int operator&( CStructureSaver &f );
	CTerrainSpot* CreateTerrainSpot( SRand *pRand, const vector<int> &params ) const;
	CTerrainSpot* CreateTerrainSpot( SRand *pRand ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWaypointName: public CDBRecord
{
	OBJECT_BASIC_METHODS(CWaypointName);
public:
	ZDATA_(CDBRecord)
	string szName;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&szName); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWaypoint: public CDBRecord
{
	OBJECT_BASIC_METHODS(CWaypoint);
public:
	ZDATA_(CDBRecord)
	CPtr<CTemplVariant> pVar;
	CPtr<CWaypointName> pName;
	bool b3DPoint;		// db "Is3DPoint": waypoint hangs in the air -- fly-snapped at map build (retail @0x496000)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pVar); f.Add(3,&pName); f.Add(4,&b3DPoint); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EHMBlendType;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTemplVariant: public CDBRecord
{
	OBJECT_BASIC_METHODS(CTemplVariant);
public:
	ZDATA_(CDBRecord)
	bool bGrid;
	CPtr<CTAmbientLight> pLight;
	CPtr<CTexture> pColorMap;
	vector< CPtr<CRectangle> > rects;
	vector< CPtr<CFinalElement> > pFinalElements;
	vector< CPtr<CUnit> > pUnits;
	CPtr<CTemplate> pTemplate;
	ZSKIP
	vector< CPtr<CExplosion> >  explosions;
	vector< CPtr<CRndTerrainSpot> > terrainSpots;
	vector<SVariantFlags> flags;
	vector< CPtr<CWaypoint> > waypoints;
	CPtr<CScript> pScript;
	int nBorder;
	// retail CTemplVariant::Import @0x423dd0: AmbientMusic/CombatMusic are ImportField<NDb::CTMusic>
	// -- MusicTemplates POOL refs (+0x7c/+0x80, serializer @0x41ebf0 tags 16/17), NOT Music records.
	// The old CPtr<CMusic> typing resolved the template id in the MUSIC table: the allies base
	// (variant 5246, AmbientMusic=template 124 = the ambient07 pool) picked up MUSIC record 124 =
	// Combat13.wav and launched a combat track as the base's load ambient.
	CPtr<CTMusic> pAmbientMusic;
	CPtr<CTMusic> pCombatMusic;
	CPtr<CDBDiplomacy> pDiplomacy;
	EHMBlendType eHMBlendType;
	bool bShowTerrain;
	// retail CTemplVariant tail (serializer @0x41ebf0 is authoritative for the game.db chunk tags):
	//   tag 21 bNoAttack (1 byte, "NoAttack" column), tags 22/23 MinCutFloor/MaxCutFloor,
	//   tag 24 weatherType ("Weather"), tag 25 nExitBorder.
	// The cut-floor range is consumed by retail CMission::Initialize @0x200690:
	//   if (nMinCutFloor < nMaxCutFloor) camera->SetCutFloorRange(nMinCutFloor, nMaxCutFloor - 1)
	// (max is EXCLUSIVE in the db; unset variants are 0/0 and keep the engine default [-3,4]).
	// NOTE: dev previously read MinCutFloor at tag 21 / MaxCutFloor at 22 -- i.e. retail's
	// bNoAttack chunk as the min floor -- so retail-db variants got garbage cut-floor ranges.
	bool bNoAttack = false;
	int nMinCutFloor = 0;
	int nMaxCutFloor = 0;
	int weatherType = 0;
	int nExitBorder = 0;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&bGrid); f.Add(3,&pLight); f.Add(4,&pColorMap); f.Add(5,&rects); f.Add(6,&pFinalElements); f.Add(7,&pUnits); f.Add(8,&pTemplate); f.Add(10,&explosions); f.Add(11,&terrainSpots); f.Add(12,&flags); f.Add(13,&waypoints); f.Add(14,&pScript); f.Add(15,&nBorder); f.Add(16,&pAmbientMusic); f.Add(17,&pCombatMusic); f.Add(18,&pDiplomacy); f.Add(19,&eHMBlendType); f.Add(20,&bShowTerrain); f.Add(21,&bNoAttack); f.Add(22,&nMinCutFloor); f.Add(23,&nMaxCutFloor); f.Add(24,&weatherType); f.Add(25,&nExitBorder); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRectangle: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRectangle);
public:
	ZDATA_(CDBRecord)
	CPtr<CTemplVariant> pVariant;
	CVec2 ptCenter;
	float fWidth;
	float fHeight;
	CPtr<CTemplate> pTemplate;
	float fRotation;
	int nFloor;
	float fDZ;
	vector<string> vszParams;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pVariant); f.Add(3,&ptCenter); f.Add(4,&fWidth); f.Add(5,&fHeight); f.Add(6,&pTemplate); f.Add(7,&fRotation); f.Add(8,&nFloor); f.Add(9,&fDZ); f.Add(10,&vszParams); return 0; }
	int nPinID; // ""   
	CRectangle(): nPinID(-1) {}

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFinalElement: public CDBRecord
{
  OBJECT_BASIC_METHODS(CFinalElement);
public:
	ZDATA_(CDBRecord)
  CPtr<CPlacableObject> pObject;
  CPtr<CTemplVariant> pVariant;
  float fRotation;
	int nFloor;
	CVec2 ptPos;
	float fDZ;
	CVec3 ptScale;
	bool bOpen;
	bool bLightmap;
	float fPower; //  
	float fRadius; //  
	int nPassageZoneID;
	int nPassageObjectID;
	int nAPRadius;
	string szName;
	CVec3 vLightCr;
	CVec3 ptLightPos;
	float fLightRadius;
	float fFlareRadius;
	CPtr<CTexture> pFlareTexture;
	CVec3 ptFlarePos;
	int eTimeOfDay;			// retail CFinalElement tag 23 (release @0x413820): a 4-byte enum/int. Read it
							// so the chunk isn't misdecoded as a string (see szLightParams below).
	string szLightParams;	// dev-only field; retail has none. Relocated to tag 34 (retail-unused) so
							// reading the retail game.db leaves it default-empty (MapBuild skips the
							// point-light suppression branch) instead of grabbing eTimeOfDay's 4 bytes.
	int nObjectPhase;
	int nObjStageDelta; //  
	float fObjRadius; //  
	CPtr<CRPGGrenade> pGrenade;
	bool bArmed;
	// release chest/lock tail (retail CFinalElement::operator& @0x413820 tags 29-33; Import @0x4235c0):
	bool bIsLocked;
	int nKeyID;
	int nLockHardness;
	CPtr<CTRPGChest> pChest;	// the loot-chest template this placed element spills at map build
	bool bLightShadow;

	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pObject); f.Add(3,&pVariant); f.Add(4,&fRotation); f.Add(5,&nFloor); f.Add(6,&ptPos); f.Add(7,&fDZ); f.Add(8,&ptScale); f.Add(9,&bOpen); f.Add(10,&bLightmap); f.Add(11,&fPower); f.Add(12,&fRadius); f.Add(13,&nPassageZoneID); f.Add(14,&nPassageObjectID); f.Add(15,&nAPRadius); f.Add(16,&szName); f.Add(17,&vLightCr); f.Add(18,&ptLightPos); f.Add(19,&fLightRadius); f.Add(20,&fFlareRadius); f.Add(21,&pFlareTexture); f.Add(22,&ptFlarePos); f.Add(23,&eTimeOfDay); f.Add(34,&szLightParams); f.Add(24,&nObjectPhase); f.Add(25,&nObjStageDelta); f.Add(26,&fObjRadius); f.Add(27,&pGrenade); f.Add(28,&bArmed); f.Add(29,&bIsLocked); f.Add(30,&nKeyID); f.Add(31,&nLockHardness); f.Add(32,&pChest); f.Add(33,&bLightShadow); return 0; }
  
  virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EUnitPose
{
	UP_CRAWL = 0,
	UP_CROUCH,
	UP_STAND
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EUnitLogic
{
	UL_EMPTY,
	UL_DEFAULT,
	UL_ROAMING,
	UL_FEAR,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnit: public CDBRecord
{
  OBJECT_BASIC_METHODS(CUnit);
public:
	ZDATA_(CDBRecord)
  CPtr<CTemplVariant> pVariant;
	int nFloor;
	CTPoint<int> ptPos;
	CPtr<CRPGPers> pMonster;
  float fRotation;
	bool bClueSlot;
	bool bClueInventorySlot;
	int nPlayer;
	int nDiplomacy;
	int nRelativeLevel;
	CPtr<CUnitGroup> pGroup;
	string szName;
	EUnitPose eInitialPose;
	EUnitLogic eLogic;
	int  nRoamingRadius;
	bool bFearUseToHit;
	CPtr<CAnimation> pGuardAnimation;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pVariant); f.Add(3,&nFloor); f.Add(4,&ptPos); f.Add(5,&pMonster); f.Add(6,&fRotation); f.Add(7,&bClueSlot); f.Add(8,&bClueInventorySlot); f.Add(9,&nPlayer); f.Add(10,&nDiplomacy); f.Add(11,&nRelativeLevel); f.Add(12,&pGroup); f.Add(13,&szName); f.Add(14,&eInitialPose); f.Add(15,&eLogic); f.Add(16,&nRoamingRadius); f.Add(17,&bFearUseToHit); f.Add(18,&pGuardAnimation); return 0; }
  
  virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EDiplomacyState
{
	DS_ENEMY = 0,
	DS_NEUTRAL,
	DS_ALLY,
	N_DS_COUNT
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWallModel: public CDBRecord
{
  OBJECT_BASIC_METHODS(CWallModel);
public:
  CPtr<CTRndModel> pModel;
	int   nLength;
	float fWidth;
	float fHeight;
  
  virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFloorModel: public CDBRecord
{
  OBJECT_BASIC_METHODS(CFloorModel);
public:
  CPtr<CTRndModel> pModel;
	int nLength;
	int nWidth;
	int nRotation;
  
  virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSolidModel: public CDBRecord
{
	OBJECT_BASIC_METHODS(CSolidModel);
public:
	CPtr<CTRndModel> pModel;
	int nLength;
	int nWidth;
	int nHeight;
	int nRotation;
	
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWall: public CDBRecord
{
  OBJECT_BASIC_METHODS(CWall);
public:
  CPtr<CWallModel> pModel;
	CPtr<CTemplVariant> pVariant;
	int    nFloor;
	CTPoint<int>  ptStart;
	CTPoint<int>  ptEnd;
  
  virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFloor: public CDBRecord
{
  OBJECT_BASIC_METHODS(CFloor);
public:
  CPtr<CFloorModel> pModel;
	CPtr<CTemplVariant> pVariant;
	int nFloor;
	string szCoords;
  
  virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CIntermediateFloor: public CFloor
{
  OBJECT_BASIC_METHODS(CIntermediateFloor);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSolid: public CDBRecord
{
  OBJECT_BASIC_METHODS(CSolid);
public:
  CPtr<CSolidModel> pModel;
	CPtr<CTemplVariant> pVariant;
	int nFloor;
	string szCoords;
  
  virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CIntermediateSolid: public CSolid
{
  OBJECT_BASIC_METHODS(CIntermediateSolid);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRoom: public CDBRecord
{
  OBJECT_BASIC_METHODS(CRoom);
public:
	int nRoomID;
	CPtr<CTemplVariant> pVariant;
	int nFloor;
	string szCoords;
  
  virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ESoundType;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CContainerModel: public CObjectBase
{
  OBJECT_BASIC_METHODS(CContainerModel);
public:
	ZDATA
  CPtr<CModel> pModel;
	CDBPtr<CEffect> pEffect;
	CDBPtr<CTexture> pSLightMask;
	CVec3 ptPLightCr;
	CVec3 ptSLightCr;
	float fPLightRadius;
	float fSLightFOV;
	float fSLightRadius;
	CVec3 ptEffectPos;
	CVec3 ptPLightPos;
	CVec3 ptSLightPos;
	CVec3 ptSLightDir;
	CVec3 ptAmbientColor;
	CDBPtr<CTSound> pSound, pDestroySound;
	CDBPtr<CSoundEffect> pSoundEffect;
	CVec3 ptSoundPos;
	ESoundType eSoundType;
	float fSoundAvgInterval; // for SoundType = ST_RANDOM
	float fPFlareRadius;
	CDBPtr<CTexture> pPFlareTexture;
	CVec3 ptPLightFlarePos;

	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pModel); f.Add(3,&pEffect); f.Add(4,&pSLightMask); f.Add(5,&ptPLightCr); f.Add(6,&ptSLightCr); f.Add(7,&fPLightRadius); f.Add(8,&fSLightFOV); f.Add(9,&fSLightRadius); f.Add(10,&ptEffectPos); f.Add(11,&ptPLightPos); f.Add(12,&ptSLightPos); f.Add(13,&ptSLightDir); f.Add(14,&ptAmbientColor); f.Add(15,&pSound); f.Add(16,&pDestroySound); f.Add(17,&pSoundEffect); f.Add(18,&ptSoundPos); f.Add(19,&eSoundType); f.Add(20,&fSoundAvgInterval); f.Add(21,&fPFlareRadius); f.Add(22,&pPFlareTexture); f.Add(23,&ptPLightFlarePos); f.Add(24,&bPLightShadow); f.Add(25,&pAttachedGrenade); return 0; }
	bool bPLightShadow = false;
	CDBPtr<CRPGGrenade> pAttachedGrenade;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRndContainerModel;
class CContainer: public CDBRecord
{
  OBJECT_BASIC_METHODS(CContainer);
public:
  CPtr<CRndContainerModel> pModel;
  CPtr<CTemplVariant> pVariant;
  int nRotation;
	int nFloor;
	int nRoomID;
	CVec2 ptPos;
	float fDZ;
  
  virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAttribute: public CDBRecord
{
  OBJECT_BASIC_METHODS(CAttribute);
public:
	string szName;

  virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExplosion: public CDBRecord
{
  OBJECT_BASIC_METHODS(CExplosion);
public:
	int nFloor;
	CVec2 ptPos;
	float fRotation;
	float fDZ;
	float fPower;
	CPtr<CTemplVariant> pVariant;

  virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRndConstructionPart;
class CConstructionPart;
class CTConstructionPart: public CRndPtr<CRndConstructionPart>
{
	OBJECT_BASIC_METHODS(CTConstructionPart);
public:
	CConstructionPart* CreateConstructionPart( SRand *pRand ) const;
	CConstructionPart* CreateConstructionPart( SRand *pRand, const vector<int> &params ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CConstructionPart: public CObjectBase
{
	OBJECT_BASIC_METHODS(CConstructionPart);
public:
	CPtr<CGeometry> pGeometry;
	CPtr<CGeometry> p2ndGeometry;
	int nSizeX;			// SizeX,Y    ; SizeZ  
	int nSizeY;
	int nSizeZ;

	enum { N_SUBPARTS = 5 };
	int nSubPartsMask;
	static bool IsPrimaryPart( int nMask, int x, int y ) {return !(0x1 & nMask >> (y * N_SUBPARTS + x));}
	static void SetPrimaryPart( int *pnMask, int x, int y, bool bPrimary );

	float fThickness;
	CPtr<CMaterial> pDefMaterials[N_CONSTRUCTION_MATERIALS];
	CPtr<CObject> pObject;
	CPtr<CRPGArmor> pArmor;
	int  nClipGroup;

	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBDiplomacy: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDBDiplomacy );
public:
	int diplomasies[ 16 ];
	//
	int operator&( CStructureSaver &f );
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CGlobalMap* GetGlobalMap( int nID );
CChapterMap* GetChapterMap( int nID );
CContainerModel* GetContainer( int nContainerID );	
CAttribute* GetAttribute( int nAttributeID );
CTConstructionPart* GetTConstructionPart( int nID );
CConstructionPart* CreateConstructionPartVariant( int nVarID );
CTemplVariant* GetTemplVariant( int nID );
CWaypointName* GetWaypointName( int nID );
CWaypoint* GetWaypoint( int nID );
CFinalElement* GetFinalElement( int nID );
CRndTerrainSpot* GetRndTerrainSpot( int nID );
CRectangle* GetRectangle( int nID );
CUnit* GetUnit( int nID );
CDBDiplomacy* GetDBDiplomacy( int nID );
void BuildMapLinks( bool bTranslate = false );
void UnpackFloorTiles( const string &src, vector<CVec2> *pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DATAMAP_H_