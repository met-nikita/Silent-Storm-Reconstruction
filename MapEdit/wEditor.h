#ifndef __WEDITOR_H_
#define __WEDITOR_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Main\wAnimation.h"
#include "..\Main\wTerrain.h"
#include "..\Main\MapBuild.h"
#include "..\Main\terraininfo.h"
#include "weInterface.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
	class CGlobalPlayer;
	class CGlobalDiplomacy;
}
namespace NDb
{
	class CObject;
	class CFinalElement;
	class CTemplVariant;
}
namespace NBuilding
{
	class CBuildInfo;
	class CBuildingGrid;
	class CSolidAndWallMap;
	struct SProjectedSpot;
	struct SBuildFragment;
}
namespace NAI
{
	class IPathNetwork;
}
struct SMapBuilding;
class CCTerrainInfo;
namespace NWysiwyg
{
enum ESpotType;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEdtorMesh: public IVisObj
{
	OBJECT_NOCOPY_METHODS(CEdtorMesh);
	ZDATA
	CPtr<NDb::CModel> pModel;
	SFBTransform m;
	CDBPtr<NDb::CFinalElement> pFin;
public:
	CSyncSrcBind<IVisObj> bindGlobal;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pModel); f.Add(3,&m); f.Add(4,&pFin); f.Add(5,&bindGlobal); return 0; }

	CEdtorMesh() {}
	CEdtorMesh( CSyncSrc<IVisObj> *pShow, NDb::CModel *_pModel, const SFBTransform &m, NDb::CFinalElement *pFin = 0 );
	virtual void Visit( IRenderVisitor *p );
	virtual void Visit( IAIVisitor *p );

	NDb::CFinalElement* GetFinalElem() const { return pFin; }
	NDb::CModel* GetModel() const { return pModel; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMemMesh: public IVisObj
{
	OBJECT_NOCOPY_METHODS(CMemMesh);
	ZDATA
	CObj<CMemObject> pObject;
	SFBTransform m;
	int nUserID;
	CVec4 color;
public:
	CSyncSrcBind<IVisObj> bindGlobal;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pObject); f.Add(3,&m); f.Add(4,&nUserID); f.Add(5,&color); f.Add(6,&bindGlobal); return 0; }

	CMemMesh() {}
	CMemMesh( CSyncSrc<IVisObj> *pShow, CMemObject *_pObject, const SFBTransform &_m, int nUserID, const CVec4 &cr );
	virtual void Visit( IRenderVisitor *p );
	virtual void Visit( IAIVisitor *p );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEditorTerrain: public CTerrain
{
	OBJECT_NOCOPY_METHODS(CEditorTerrain);
	int nCutFloor;
	bool bVisible;
	CPtr<NAI::IAIMap> pAIMap;

public:
	CEditorTerrain() {}
	CEditorTerrain( CSyncSrc<IVisObj> *pShow, CTerrainInfoHolder *pTerrainInfo, NAI::IAIMap *pAIMap, CFuncBase<STime> *_pTime, int nDefaultFloor, const list<SMapHole> &holesList, const list<SMapWall> &wallsList, int nCutFloor );

	void Visit( IAIVisitor *pVisitor );
	void Visit( IRenderVisitor *pVisitors );
	void Update( int nCutFloor, bool bForceUpdate = false );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBuilding;
class CTerrain;
class IEditorSubTemplate;
class IEditorUnit;
class CEditorWorld: public IEditorWorld
{
	OBJECT_NOCOPY_METHODS(CEditorWorld);
	//
	CObj<CWorldSyncSrc> pShow, pShowUnits;
	CObj<CCTime> pTime;
	list<CObj<CMemMesh> > origin;
	list<CObj<CBuilding> > buildings;
	list<CObj<CMemMesh> > spots;
	list<CObj<CEdtorMesh> > frozen;
	list<CObj<IEditorUnit> > units;
	list<CObj<CMemMesh> > objects;
	list<CObj<CMemMesh> > ladders;
	list<CObj<CMemMesh> > waypoints;
	list<CObj<CMemMesh> > solids;
	list<CObj<IEditorSubTemplate> > subtemplates;
	list<CObj<IEditorObject> > iobjects;
	//CSyncSource<CWObject> showObjects; // sorted list
	//CSyncSource<CUnit> showUnits; // sorted list
	//CSyncSource<CBuilding> showBuildings;
	//CSyncSource<C2DSound> play2DSounds;
	//CSyncSource<C3DSound> play3DSounds;
	//CSyncSource<CItem> showItems;
	//CSyncSource<CFrozenItem> showFrozenItems;
	//CSyncSource<CMemObject> showMemObjects;
	
	CObj<NAI::IAIMap> pAIMap;
	CObj<NAI::IPathNetwork> pPathNetwork;
	CObj<NBuilding::CBuildingGrid> pBuildingGrid;
	CObj<NBuilding::CSolidAndWallMap> pSWMap;
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pBuildInfo;
	CObj<::CMemObject> pMemCube;
	CObj<::CMemObject> pMemFlag;
	list<CObj<::CMemObject> > memOrigin;
	vector< CObj<NDb::CObject> > vHoldObjects;
	SMapBuilding sMapBuildingInfo;
	CObj<CTerrainInfoHolder> pTerrainInfo;
	CObj< CEditorTerrain > pTerrain;
	SFBTransform posTerrain;
	bool bTerrainAlign;
	CPtr<NDb::CTemplVariant> pVar;
	vector<CTRect<float> > rectsToUpdateGeometry;
	vector<CTRect<float> > rectsToUpdateTexture;
	vector<CTRect<float> > rectsToUpdateGrass;
	bool bUpdateTerrain;
	bool bShowInfo;
	int nMaxFloor;
	SMapInfo sMapInfo;
	bool bHolesUpdated;
	//
	CEdtorMesh* AddFrozen( const SFBTransform &pos, NDb::CModel *pModel, NDb::CFinalElement *pFin = 0 );
	void AddUnits();
	void AddBuilding( const SMapBuilding &info );
	void AddMemObject( list<CObj<CMemMesh> > *pRes, ::CMemObject *pObject, const SFBTransform &pos, const CVec4 &color, int nUserID, bool bIgnoreTerr = false );
	void AddLines( vector<CVec3> *pPoints, const SFBTransform &pos, const CVec3 &color, bool bIgnoreTerr = false );
	void AddSpots();
	void AddTexSpot( NWysiwyg::ESpotType type, CVec3 color, const NBuilding::SProjectedSpot &spot, int nSpotIndex, bool bIgnoreTerr = false );
	void AddFragments( const vector<NBuilding::SBuildFragment> &frags );
	void AddMapObjects();
	void AddEmptySolids( NBuilding::CBuildInfo *pInfo );
	void AddWaypoints();
	void AddLadders();
	void AddSubTemplates();
	void CreateTerrain( bool bResetPins = false );
	void CreateOrigin();
	void UpdateExplosions();
		
public:
	CEditorWorld();
	
	virtual int GetWorldID() const;
	virtual CSyncSrc<IVisObj>* GetActive() const { return pShow; }
	virtual CSyncSrc<IVisObj>* GetUnits() const { return pShowUnits; }
	virtual CHitLocator* GetHitEvent() { return 0; }
	virtual NDb::CDBAckSequence *GetAcknowledgement( IPlayer *pPlayer ) { return 0; }
	virtual CGlobalAck *GetGlobalAck() const { return 0; }
	virtual void ClickOfDeath( const CRay &ray, int nMaxFloor ) {}
	virtual void GetUICmds( list<CPtr<CUICmd> > *pCmdsList ) {};
	//
	virtual void CreateRandom( int nTemplateID, const vector<string> &params, bool bBuildingStability, 
		const list< CPtr<NScenario::CScenarioClue> > &clues, int nMobsLevel, 
		CObj<CPostWorldCreateInfo> *pPostInfo, SRandomSeed sSeed, bool bLeanAndMean );
	virtual void CreateRestored() {}
	virtual void RunPostInit( CPostWorldCreateInfo *pPostInfo ) {}
	virtual IPlayer* AddPlayer( const wstring &wsName, NRPG::CGlobalPlayer *pGlobalPlayer, 
		CCommander *pCommander, bool bAddOnManyDeploySpots = false ) {return 0;}
	virtual IPlayer* GetCurrentPlayer() const { return 0; }
	virtual bool IsUnitActive( CUnit *pTest ) const { return false; }
	virtual bool IsValidPath( const NAI::CPath &path, CUnit *pWho ) { return true; }
	virtual CCTime* GetAimTime() const { return pTime; }
	virtual NRPG::IGame* GetGame() { return 0; }
	virtual NAI::IAIMap* GetAIMap() { return pAIMap; }
	virtual NAI::IPathNetwork* GetPathNetwork() { return pPathNetwork; }
	virtual void ConvertCommand( CCommand *pCmd, CCommander *pCommander ) {};
	virtual bool TraceTile( const CRay &ray, NAI::SPosition *pRes, int nMaxFloor = 100 ) {return false;}
	virtual CUnit* GetUnit( const NAI::SUnitPosition &pos ) { return 0; }
	virtual NDb::CAmbientLightReal* GetDefaultLight() { return 0; }
	virtual void GetInterrupts( vector< CPtr<IPlayer> > *pInterrups ) const {}
	virtual void FindCloseGroundItems( CUnit *pUnit, vector<SItem> *pRes ) {}
	virtual void TraceObjects( const CRay &ray, vector<CObjectBase*> *pRes, int nMaxFloor = 100 ) {};
	virtual bool IsFirstTurn() const { return false; }
	virtual bool IsInterrupt() const { return false; }
	virtual bool IsWinnerPlayer( IPlayer *pPlayer ) {return true;}
	virtual void GetActiveUnits( IPlayer *pPlayer, list<CUnit*> *pRes ) {};
	virtual bool GetAcknowledgement( IPlayer *pPlayer, vector<CPtr<CAckEvent> > *pEventsSet ) { return false; }
	virtual bool CanSeeAction( IPlayer *pPlayer ) { return false; }
	virtual int  GetEnemyWatchers( IPlayer *pPlayer ) const { return 0; }
	virtual CUnit* GetUnitInTile( const NAI::SUnitPosition &pos ) { return 0; }
	virtual const CTRect<float>& GetMapSafeZone() const { static CTRect<float> r; return r; }
	virtual void MakeExplosion( const CRay &ray, int nMaxFloor ) {}
	virtual NRPG::CGlobalDiplomacy *GetGlobalDiplomacy() const { return 0; }
	virtual void InitPlayersCorpseCarrying() {}
	virtual void GetAllUnits( vector< CPtr<NWorld::CUnit> > *pUnits ) {}
	virtual void RemovePlayer( IPlayer *pPlayer ) {};
	//
	virtual bool IsExecuting() const { return false; }
	virtual void UpdateWorld( STime tScene, IPlayer *pPlayer );
	virtual void CreateDefault() {}
	virtual IPlayer* GetNextPlayerForScript( IPlayer *pPrev ) { return 0; }
	virtual NDb::EDiplomacyState GetDiplomacyState( CUnit *pUnit, IPlayer *pPlayer ) const {return (NDb::EDiplomacyState)0;}
	virtual NDb::EDiplomacyState GetDiplomacyState( IPlayer *pPlayer1, IPlayer *pPlayer2 ) const {return (NDb::EDiplomacyState)0;}

	virtual CFuncBase<STerrainInfo>* GetTerrainInfo() const { return pTerrainInfo; }
	virtual void Explode( const CVec3 &ptEpicentre, int nPower );
	//
	NBuilding::CBuildingGrid* GetMainBuilding() { return pBuildingGrid; } // for WYSIWYG
	NBuilding::CSolidAndWallMap* GetMainBuildingSWMap() { return pSWMap; } // for WYSIWYG
	IBuilding* GetMainBuildingInterface(); // for WYSIWYG
	void ShowMainBuilding( bool bShow );
	void UpdateTerrain();
	void UpdateGrass( const CTRect<float> &sRegion );
	void UpdateTerrainGeometry( const CTRect<float> &sRegion );
	void UpdateTerrainTexture( const CTRect<float> &sRegion );
	virtual void UpdateTerrainSpots( const vector<NBuilding::SProjectedSpot> &spots );
	virtual void InvalidateTerrainGeometryRect( const CTRect<float> &sRect );
	virtual void InvalidateTerrainTextureRect( const CTRect<float> &sRect );
	virtual void InvalidateTerrainGrassRect( const CTRect<float> &sRect );
	void UpdateAll();
	void UpdateCutFloor();
	void UpdateAllBuildingParts();
	virtual void ResetBuilding();
	virtual void UpdateBuilding();
	void UpdateAIMap();
	void UpdateSubTemplates();
	virtual void UpdateSubTemplate( int nUserID );
	void UpdateUnits();
	virtual void UpdateUnit( int nDBUnitID );
	void UpdateObjects();
	virtual void UpdateObject( int nDBObjectID );
	virtual void UpdateWaypoints();
	virtual void UpdateWallSpot( int nSpotID );
	void UpdateLadders();
	SFBTransform GetTerrainTransform( float x, float y );
	SFBTransform GetTerrainTransform( const SFBTransform &posObj );
	CObjectBase* CreateMemObject( ::CMemObject *pObject, const SFBTransform &pos, const CVec4 &color, int nUserID, bool bIgnoreTerr = false );
	void RebuildTerrInfo( bool bResetPins );

	virtual const STerrainInfo& GetTerrainInfo();

	virtual bool GetTerrAlign() const { return bTerrainAlign; }
	bool SetTerrAlign( bool bAlign ) { bool bRet = bTerrainAlign; bTerrainAlign = bAlign; return bRet; }

	void EnableTerrainUpdate( bool bEnable );
	void ShowInfo( bool bShow );
	virtual void UpdateFakeSolids();
	virtual int  GetMaxFloor() { return nMaxFloor; }
	virtual void OpenObject( int nDBObjectID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __WEDITOR_H_