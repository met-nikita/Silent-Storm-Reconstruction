#include "StdAfx.h"
#include "DG.h"
#include "MapBuild.h"
#include "Grid.h"
#include "Transform.h"
#include "BuildingGrid.h"
#include "..\Misc\BasicShare.h"
#include "BuildingInfo.h"
#include "BuildingClip.h"
#include "MapBuildTerrain.h"
#include "MakeBuilding.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\DBFormat\DataScenario.h"
#include "..\MiscDll\LogStream.h"
#include "aiWaypoint.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataLight.h"
#include "..\DBFormat\DataObject.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataAI.h"
#include "aiGrid.h"

const float WALL_HEIGHT = 2.5f;  // высота этажа
const int CLUE_SLOT_ID = 188;
const int EXPLOSION_ID = 189;

namespace NGScene
{
	externA5 CBasicShare<int, NBuilding::CBuildInfoLoader> shareBuildings;
}
CBasicShare<int, NAI::CWaypointLoader> shareWaypoints(133);
CBasicShare<int, NAI::CUnitAIInfoLoader> shareUnits(134);
CBasicShare<int, NAI::CUnitGroupAIInfoLoader> shareUnitGroups(144);
////////////////////////////////////////////////////////////////////////////////////////////////////
void ConvertFlags( vector<int> *pFlags, const vector<string> &strParams )
{
	CDBTable<NDb::CAttribute> *pAttrTable = NDatabase::GetTable<NDb::CAttribute>();
	if ( pAttrTable )
	{
		unordered_map<string, int> attrmap;
		CDBIterator<NDb::CAttribute> it( *pAttrTable );
		while ( it.MoveNext() )
			attrmap[it.Get()->szName] = it.Get()->GetRecordID();
		for ( int i = 0; i < strParams.size(); ++i )
		{
			unordered_map<string, int>::const_iterator it = attrmap.find( strParams[i] );
			if ( it != attrmap.end() )
				pFlags->push_back( it->second );
		}
	}
	sort( pFlags->begin(), pFlags->end() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{
struct SExplosion
{
	SMapPosition pos;
	float fPower;
	float fRadius;
	int nObjStageDelta;
	float fObjRadius;
	CVec2 ptAlignTo;
	SExplosion(): fRadius(10),nObjStageDelta(0), fObjRadius(0) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMapBuilder
{
	struct SLayerPlace
	{
		int nWidth, nHeight;
		CVec2 vOrigin;
		float fRotation;
	};
	struct SLayerRotationInfo
	{
		float fSinR;
		float fCosR;
		CVec2 vOrigin;
		SLayerRotationInfo() {}
		SLayerRotationInfo( float fAngle, const CVec2 &vOrig ) : vOrigin(vOrig)
		{
			fSinR = sin( fAngle );
			fCosR = cos( fAngle );
		}
	};

	SMapPosition posInitial;
	bool bTerrAlignmentInitial;
	CVec2 ptParentBuildingInitial;

	SMapInfo &info;
	vector<SExplosion> explosions;
	vector<int> initialFlags;
	CPtr<NAI::IPathNetwork> pNet;
	int nRoom;
	SRand rand;
	unordered_map<int, CObj<CMapWaypoint> > waypoints;
	int nMaxDepth;
	SLayerPlace rootPlace;
	NAI::CLayersGroup *pRootLayersGroup;
	vector<SLayerRotationInfo> parentLayerRotations;
	bool bBuildTerrain;
	bool bResetPins;
	bool bShowHoles;

	void AddSimpleElements( SMapInfo *pDst, SMapBuilding *pB, NDb::CTemplVariant *pVar, const SMapPosition &parent, 
		bool bTerrAlign, const CVec2 &ptAlignTo, const vector<int> &flags );
	void AddWaypoints( SMapInfo *pDst, NDb::CTemplVariant *pVar, const SMapPosition &parent,
		bool bTerrAlign, const CVec2 &ptAlignTo );
	void WriteLightRooms( SMapInfo *pInfo, NBuilding::CBuildingGrid *pGrid );
	void AddBuildingObjects( int *pMinFloor, int *pMaxFloor, SRand *pRand, SMapInfo *pDst, 
		const vector<NBuilding::SBuildFragment> &frags, const SMapPosition &pos, const CVec2 &ptAlignTo, 
		const vector<int> &flags, bool bSolids );
	bool CreateBuilding( SMapInfo *pDst, SMapBuilding *pRes, NBuilding::CBuildInfo *pInfo, 
		const SMapPosition &pos, const CVec2 &ptBuildingCenter, const vector<int> &flags );
	void CalcLayerPlace( SLayerPlace *pRes, const SMapPosition &pos, const NDb::CTemplate* pTemplate, int nBorder );
	int CreateLayersGroup( const SLayerPlace &place, int nFirstFloor );
	void CreateGrids( SMapBuilding *pRes, const vector<NBuilding::SBuildFragment> &fragments, 
		const SMapPosition &pos );
	void TraverseTemplateTree( NDb::CTemplate* pTemplate,	SMapInfo *pFree, 
		const SMapPosition &pos, int nDepth, NAI::SAlternativeGridInfo *pAltGrid,
		bool bTerrAlign, const CVec2 &ptParentBuilding, const vector<int> &flags, int nPlacementID = -1 );
	void TraverseTerrainTree( NDb::CTemplate* pTemplate, float fDZ,	const SMapPosition &pos, int nDepth, const vector<int> &flags, int nPlacementID = -1 );
	void ResolveRoutes( SMapInfo *pInfo );
	void ResolveRoute( CPtrFuncBase<NAI::CUnitAIInfo> *pUnitLoader, vector<CPtr<CMapWaypoint> > *pRoute );
	int GetGlobalRoomID( NBuilding::CBuildingGrid *pGrid, int nFloor, int nLocal )
	{
		if ( nLocal == 0 )
			return 0;
		int nTest = pGrid->GetRoomGlobal( nFloor, nLocal );
		if ( nTest == 0 )
		{
			nTest = nRoom;
			pGrid->AddRoom( nFloor, nLocal, nRoom++ );
		}
		return nTest;
	}

	void AddMapHole( list<SMapHole> *pList, const TPolygonsList &polygonsList, int nHeight, bool bVisible );
	void BuildHolesList();
	void GenerateWalls();

public:
	CMapBuilder( SMapInfo *pRes, NAI::IPathNetwork *_pNet, SRandomSeed sSeed = SRandomSeed() ): 
			info(*pRes), nRoom(1), pNet(_pNet), nMaxDepth(-1)
	{
		rand.seed = sSeed;

		posInitial.nFloor = 0;
		posInitial.fRotation = 0;
		posInitial.ptPos = VNULL3;

		bTerrAlignmentInitial = true;

		ptParentBuildingInitial = VNULL2;

		bBuildTerrain = true;
		bResetPins = false;
		bShowHoles = true;
	}
	void SetParams( const vector<string> &strParams );
	void SetMaxDepth( int nDepth ) { nMaxDepth = nDepth; }
	void SetPos( const SMapPosition &pos ) { posInitial = pos; }
	void SetTerrAlignment( bool bTerr ) { bTerrAlignmentInitial = bTerr; }
	void SetParentBuildingPos( const CVec2 &ptPos ) { ptParentBuildingInitial = ptPos; }
	void SetBuildTerrain( bool bBuild ) { bBuildTerrain = bBuild; }
	void SetResetPins( bool bReset ) { bResetPins = bReset; }
	void SetShowHoles( bool bShow ) { bShowHoles = bShow; }

	bool BuildMap( int nPlacementID );
	bool BuildTerrain( int nPlacementID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Map generation
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> // can be CVec2 or CVec3
inline void RotatePt( T *pVec, float fAngle )
{
  const float fAng = -ToRadian( fAngle );
  float fc = cos( fAng );
  float fs = sin( fAng );
	
  float x = fc * pVec->x + fs * pVec->y;
  pVec->y = -fs * pVec->x + fc * pVec->y;
  pVec->x = x;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
inline void CalcPosition( SMapPosition *pRes, T *pFin, const SMapPosition &parent )
{
	CVec3 ptLocal( FP_GRID_STEP * pFin->ptPos.x, FP_GRID_STEP * pFin->ptPos.y, pFin->nFloor * WALL_HEIGHT );
	RotatePt( &ptLocal, parent.fRotation );
	pRes->nFloor = parent.nFloor + pFin->nFloor;
	pRes->ptPos = parent.ptPos + ptLocal;
	pRes->fRotation = parent.fRotation + pFin->fRotation;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
inline void CalcPositionSubElement( SMapPosition *pRes, CVec3 ptSubElement, T *pFin, const SMapPosition &parent )
{
	CVec2 ptSrcPos = pFin->ptPos;
	RotatePt( &ptSubElement, pFin->nRotation );
	pFin->ptPos += CVec2( ptSubElement.x, ptSubElement.y );
	CalcPosition( pRes, pFin, parent );
	pFin->ptPos = ptSrcPos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapBuilder::AddBuildingObjects( int *pMinFloor, int *pMaxFloor, SRand *pRand, SMapInfo *pDst, 
	const vector<NBuilding::SBuildFragment> &frags, const SMapPosition &pos, const CVec2 &ptAlignTo, 
	const vector<int> &flags, bool bSolids )
{
	for ( int i = 0; i < frags.size(); ++i )
	{
		const NBuilding::SBuildFragment &fr = frags[i];
		NDb::CTConstructionPart *pTCP = NDb::GetTConstructionPart( fr.nConstructionPartID );
		if ( !pTCP )
			continue;
		CPtr<NDb::CConstructionPart> pCP = pTCP->CreateConstructionPart( pRand, flags );
		if ( !IsValid( pCP ) )
			continue;
		*pMaxFloor = Max( *pMaxFloor, int(fr.ptPos.z + 0.5f) + pCP->nSizeZ );
		*pMinFloor = Min( *pMinFloor, int(floor(fr.ptPos.z)) );
		if ( !IsValid( pCP->pObject ) || (pCP->nSizeY == 0 && fr.nSubBlockID != NBuilding::GetPartHashID( 1, 1, 1 )) )
			continue;
		//
		CVec3 ptShift;
		if ( bSolids )
		{
			switch ( fr.nRotationID )
			{
				case SDiscretePos::TURN_90:
					ptShift = CVec3( 2 * pCP->nSizeY, 0, 0 );
					break;
				case SDiscretePos::TURN_180:
					ptShift = CVec3( 2 * pCP->nSizeX, 2 * pCP->nSizeY, 0 );
					break;
				case SDiscretePos::TURN_270:
					ptShift = CVec3( 0, 2 * pCP->nSizeX, 0 );
					break;
			}
		}
		CVec3 ptLocal( FP_GRID_STEP * fr.ptPos.x, FP_GRID_STEP * fr.ptPos.y, fr.ptPos.z * WALL_HEIGHT );
		ptLocal += FP_GRID_STEP * ptShift;
		RotatePt( &ptLocal, pos.fRotation );
		SMapElement me;
		me.pos.nFloor = pos.nFloor + floor( fr.ptPos.z );
		me.pos.ptPos  = pos.ptPos + ptLocal;
		me.pos.fRotation = pos.fRotation + RotationIDToAngle( fr.nRotationID );
		me.pos.ptScale = CVec3(1,1,1);
		me.pObject = pCP->pObject;
		me.nRelFloor = floor( fr.ptPos.z );
		me.ptAlignTo = ptAlignTo;
		me.bOpen = fr.nObjectFlags & NBuilding::OF_OPEN;
		pDst->items.push_back( me );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapBuilder::CreateBuilding( SMapInfo *pDst, SMapBuilding *pRes, NBuilding::CBuildInfo *pInfo,	
	const SMapPosition &pos, const CVec2 &ptBuildingCenter, const vector<int> &flags )
{
	if ( !pInfo || (pInfo->wallFragments.empty() && pInfo->solidFragments.empty()) || ( 0 == pInfo->nMaxX && 0 == pInfo->nMaxY ) )
		return false;
	pRes->mpos = pos;
	pRes->ptAlignTo = ptBuildingCenter;
	// матрицу трансформации для здания посчитаем позже через mpos
	pRes->pGrid = new NBuilding::CBuildingGrid;
	pRes->pSWMap = NBuilding::MakeSWMap( pRes->pVariant->GetRecordID(), pRes->pGrid->GetSeed() );
	SRand brnd( pRes->pGrid->GetSeed() );
	// создание объектов, встроенных в здание (окна\двери..)
	pInfo->nMinFloor = 100;
	pInfo->nMaxFloor = -100;
	AddBuildingObjects( &pInfo->nMinFloor, &pInfo->nMaxFloor, &brnd, pDst, pInfo->solidFragments, pos, ptBuildingCenter, flags, true );
	AddBuildingObjects( &pInfo->nMinFloor, &pInfo->nMaxFloor, &brnd, pDst, pInfo->wallFragments, pos, ptBuildingCenter, flags, false );
	pInfo->nMinFloor = pInfo->nMinFloor == 100 ? 0 : pInfo->nMinFloor;
	pInfo->nMaxFloor = pInfo->nMaxFloor == -100 ? 0 : pInfo->nMaxFloor;
	//
	pRes->pGrid->Setup( pInfo->nMaxX, pInfo->nMaxY, // CRAP rotations are not accounted in max size calcs
		pInfo->nMinFloor, pInfo->nMaxFloor, VNULL2, pRes->pos );
	NBuilding::BuildingHP( pInfo, pRes->pGrid, pRes->pSWMap );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsSuitableLight( const vector<int> &vInputParams, int nLightFlag )
{
	if ( vInputParams.empty() )
		return true;

	for ( int i = 0; i < vInputParams.size(); ++i )
		if ( vInputParams[i] == nLightFlag )
			return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapBuilder::AddSimpleElements( SMapInfo *pDst, SMapBuilding *pB, NDb::CTemplVariant *pVar, 
	const SMapPosition &parent, bool bTerrAlign, const CVec2 &ptAlignTo, const vector<int> &flags )
{
  if ( !IsValid( pVar ) )
    return;
  {
		const int n = pVar->pFinalElements.size();
		for ( int i = 0; i < n; ++i )
		{
			NDb::CFinalElement *pFin = pVar->pFinalElements[i];
			if ( IsValid( pFin ) && IsValid( pFin->pObject ) )
			{
				//
				if ( IsValid( pFin->pObject->pObject ) )
				{
					CPtr<NDb::CObject> pObject( pFin->pObject->pObject->CreateObject( &rand, flags ) );
					if ( !pObject )
						continue;
					if ( pObject->bIsDeploySpot )
					{
						SMapPosition pos;
						CalcPosition( &pos, pFin, parent );
						pos.ptPos.z += pFin->fDZ;
						SDeploySpot ds( pos );
						if ( !bTerrAlign ) 
							ds.ptAlignTo = ptAlignTo;
						else
							ds.ptAlignTo = CVec2( pos.ptPos.x, pos.ptPos.y );
						pDst->deploySpots.push_back( ds );
					}
					if ( IsValid( pObject->pModels[0] ) )
					{
						SMapElement me;
						CalcPosition( &me.pos, pFin, parent );
						me.pos.ptPos.z += pFin->fDZ;
						me.pos.ptScale = pFin->ptScale;
						me.nRelFloor = pFin->nFloor;
						me.bLightmap = pFin->bLightmap;
						me.bOpen = pFin->bOpen;
						if ( !bTerrAlign ) 
							me.ptAlignTo = ptAlignTo;
						else
							me.ptAlignTo = CVec2( me.pos.ptPos.x, me.pos.ptPos.y );
						me.pObject = pObject;
						me.nPassageZoneID = pFin->nPassageZoneID;
						me.nPassageObjectID = pFin->nPassageObjectID;
						me.nAPRadius = pFin->nAPRadius;
						me.szName = pFin->szName;
						me.nObjectPhase = pFin->nObjectPhase;
						me.nDC = Float2Int( pFin->fPower );
						me.pGrenade = pFin->pGrenade;
						if ( fabs( pFin->vLightCr ) > 0 )
						{
							NDb::CContainerModel *p = pObject->pModels[0];
							p->ptPLightCr = pFin->vLightCr;
							p->ptPLightPos = pFin->ptLightPos;
							p->fPLightRadius = pFin->fLightRadius;
							p->fPFlareRadius = pFin->fFlareRadius;
							p->pPFlareTexture = pFin->pFlareTexture;
							p->ptPLightFlarePos = pFin->ptFlarePos;
							if ( !pFin->szLightParams.empty() )
							{
								vector<int> lflags;
								ConvertFlags( &lflags, vector<string>( 1, pFin->szLightParams ) );
								if ( !lflags.empty() && !IsSuitableLight( flags, lflags.front() ) )
									p->ptPLightCr = VNULL3;
							}
						}
						pDst->items.push_back( me );
						if ( fabs2( pObject->pModels[0]->ptAmbientColor ) > FP_EPSILON )
						{
							SMapBuilding::SAmbientLight l;
							l.color = pObject->pModels[0]->ptAmbientColor;
							l.nFloor = pFin->nFloor;
							l.bLightmap = pFin->bLightmap;
							pB->lights.push_back( l );
						}
					}
				}
				else if ( IsValid( pFin->pObject->pRPGItem ) )
				{
					if ( CLUE_SLOT_ID == pFin->pObject->pRPGItem->GetRecordID() )
					{
						SClueSlot cs;
						CalcPosition( &cs.pos, pFin, parent );
						cs.pos.ptPos.z += pFin->fDZ;
						if ( !bTerrAlign ) 
							cs.ptAlignTo = ptAlignTo;
						else
							cs.ptAlignTo = CVec2( cs.pos.ptPos.x, cs.pos.ptPos.y );
						pDst->slots.push_back( cs );
					}
					else if ( EXPLOSION_ID == pFin->pObject->pRPGItem->GetRecordID() )
					{
						SExplosion me;
						CalcPosition( &me.pos, pFin, parent );
						me.pos.ptPos.z += pFin->fDZ;
						me.fPower = pFin->fPower;
						me.fRadius = pFin->fRadius;
						me.nObjStageDelta = pFin->nObjStageDelta;
						me.fObjRadius = pFin->fObjRadius;
						if ( !bTerrAlign ) 
							me.ptAlignTo = ptAlignTo;
						else
							me.ptAlignTo = CVec2( me.pos.ptPos.x, me.pos.ptPos.y );
						explosions.push_back( me );
					}
					else
					{
						SMapRPGElement me;
						CalcPosition( &me.pos, pFin, parent );
						me.pos.ptPos.z += pFin->fDZ;
						me.pos.ptScale = pFin->ptScale;
						me.nRelFloor = pFin->nFloor;
						me.bOpen = pFin->bOpen;
						if ( !bTerrAlign ) 
							me.ptAlignTo = ptAlignTo;
						else
							me.ptAlignTo = CVec2( me.pos.ptPos.x, me.pos.ptPos.y );
						me.pItem = pFin->pObject->pRPGItem;
						me.szName = pFin->szName;
						me.nDC = Float2Int( pFin->fPower );
						me.bArmed = pFin->bArmed;
						pDst->rpgitems.push_back( me );
					}
				}
			}
		}
  }
  //
  {
		const int n = pVar->pUnits.size();
		for ( int i = 0; i < n; ++i )
		{
			NDb::CUnit *pUnit = pVar->pUnits[i];
			if ( IsValid( pUnit ) )
			{
				SMapUnit mu;
				CalcPosition( &mu.pos, pUnit, parent );
				if ( !bTerrAlign ) 
					mu.ptAlignTo = ptAlignTo;
				else
					mu.ptAlignTo = CVec2( mu.pos.ptPos.x, mu.pos.ptPos.y );
				mu.nUnitID = pUnit->GetRecordID();
				mu.pPers = pUnit->pMonster;
				mu.bSlot = pUnit->bClueSlot;
				mu.nRelativeLevel = pUnit->nRelativeLevel;
				mu.szName = pUnit->szName;
				mu.nDiplomacy = pUnit->nDiplomacy;
				mu.nScenarioPlayer = pUnit->nPlayer;
				mu.eInitialPose = pUnit->eInitialPose;
				mu.eLogic = pUnit->eLogic;
				mu.nRoamingRadius = pUnit->nRoamingRadius;
				mu.bFearUseToHit = pUnit->bFearUseToHit;
				mu.pGuardAnimation = pUnit->pGuardAnimation.GetPtr();
				if ( pUnit->bClueSlot || pUnit->bClueInventorySlot )
				{
					SClueSlot cs;
					CalcPosition( &cs.pos, pUnit, parent );
					cs.nUnitID = pUnit->GetRecordID();
					cs.pPers = pUnit->pMonster;
					cs.bPersSlot = pUnit->bClueSlot;
					cs.bInventorySlot = pUnit->bClueInventorySlot;
					pDst->slots.push_back( cs );
				}
				if ( IsValid( pUnit->pGroup ) )
				{
					pDst->groups[pUnit->pGroup->GetRecordID()].units.push_back( mu.nUnitID );
				}
				pDst->units.push_back( mu );
			}
		}
  }
  //
  {
		const int n = pVar->explosions.size();
		for ( int i = 0; i < n; ++i )
		{
			NDb::CExplosion *pExpl = pVar->explosions[i];
			if ( IsValid( pExpl ) )
			{
				SExplosion me;
				CalcPosition( &me.pos, pExpl, parent );
				me.pos.ptPos.z += pExpl->fDZ;
				me.fPower = pExpl->fPower;
				explosions.push_back( me );
			}
		}
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapBuilder::WriteLightRooms( SMapInfo *pInfo, NBuilding::CBuildingGrid *pGrid )
{
//	for ( list<SMapElement>::iterator i = pInfo->items.begin(); i != pInfo->items.end(); ++i )
//		if ( i->bTerrAlignment ) // для окон и дверей комнаты не прописываем
//			i->nRoomID = GetGlobalRoomID( pGrid, i->nRelFloor, i->nRoomID );		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
inline void SetStuffOnLayer( T *pDst, T *pSrc, int nMaxFloor )
{
	for ( T::iterator i = pSrc->begin(); i != pSrc->end(); )
	{
		T::iterator k = i++;
		int nFloor = k->pos.nFloor;
		ASSERT( nFloor >= nMaxFloor || nMaxFloor == 1000 );
		if ( nFloor <= nMaxFloor )
			pDst->splice( pDst->end(), *pSrc, k );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
inline void SetObjectsOnLayer( T *pDst, T *pSrc, int nMaxFloor )
{
	for ( T::iterator i = pSrc->begin(); i != pSrc->end(); )
	{
		T::iterator k = i++;
		if ( !IsValid( k->pObject ) )
			continue;
		int nFloor = k->pos.nFloor;
		//ASSERT( nFloor >= nMaxFloor );
		if ( nFloor <= nMaxFloor )
			pDst->splice( pDst->end(), *pSrc, k );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
inline void SetWaypointsOnLayer( T *pDst, T *pSrc, int nMaxFloor )
{
	for ( T::iterator i = pSrc->begin(); i != pSrc->end(); )
	{
		T::iterator j = i++;
		T::reference k = *j;
		int nFloor = k->pos.nFloor;
		ASSERT( nFloor >= nMaxFloor );
		if ( nFloor <= nMaxFloor )
			pDst->splice( pDst->end(), *pSrc, j );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetOnLayer( SMapInfo *pInfo, SMapInfo *pFree, int nMaxFloor )
{
	for ( int k = 0; k < pFree->deploySpots.size(); ++k )
		pInfo->deploySpots.push_back( pFree->deploySpots[k] );
	pFree->deploySpots.clear();
	for ( list<SMapRPGElement>::const_iterator i = pFree->rpgitems.begin(); i != pFree->rpgitems.end(); ++i )
		pInfo->rpgitems.push_back( *i );
	for ( vector<SClueSlot>::const_iterator i = pFree->slots.begin(); i != pFree->slots.end(); ++i )
		pInfo->slots.push_back( *i );
	for (unordered_map<int, SUnitGroup>::const_iterator i = pFree->groups.begin(); i != pFree->groups.end(); ++i )
	{
		SUnitGroup &g = pInfo->groups[i->first];
		g.units.insert( g.units.end(), i->second.units.begin(), i->second.units.end() );
		g.route.insert( g.route.end(), i->second.route.begin(), i->second.route.end() );
	}
	pFree->slots.clear();
	pFree->rpgitems.clear();
	pInfo->scripts.insert( pInfo->scripts.end(), pFree->scripts.begin(), pFree->scripts.end() );
	pFree->scripts.clear();
	pFree->groups.clear();
	SetObjectsOnLayer( &pInfo->items, &pFree->items, nMaxFloor );
	SetStuffOnLayer( &pInfo->units, &pFree->units, nMaxFloor );
	SetWaypointsOnLayer( &pInfo->waypoints, &pFree->waypoints, nMaxFloor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void Transfer( SMapInfo *pDst, SMapInfo *pSrc )
{
	pDst->items.splice( pDst->items.end(), pSrc->items );
	pDst->units.splice( pDst->units.end(), pSrc->units );
	pDst->deploySpots.insert( pDst->deploySpots.end(), pSrc->deploySpots.begin(), pSrc->deploySpots.end() );
	pDst->rpgitems.insert( pDst->rpgitems.end(), pSrc->rpgitems.begin(), pSrc->rpgitems.end() );
	pDst->scripts.insert( pDst->scripts.end(), pSrc->scripts.begin(), pSrc->scripts.end() );
	pDst->slots.insert( pDst->slots.end(), pSrc->slots.begin(), pSrc->slots.end() );
	for (unordered_map<int, SUnitGroup>::const_iterator i = pSrc->groups.begin(); i != pSrc->groups.end(); ++i )
	{
		SUnitGroup &g = pDst->groups[i->first];
		g.units.insert( g.units.end(), i->second.units.begin(), i->second.units.end() );
		g.route.insert( g.route.end(), i->second.route.begin(), i->second.route.end() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapBuilder::CalcLayerPlace( SLayerPlace *pRes, const SMapPosition &pos, const NDb::CTemplate* pTemplate, int nBorder )
{
	float fBorder = nBorder * FP_GRID_STEP;
	CVec2 ptOrigin(pos.ptPos.x + fBorder, pos.ptPos.y + fBorder );
	CVec3 ptShift( -FP_GRID_STEP, -FP_GRID_STEP, 0);
	RotatePt( &ptShift, pos.fRotation );
	pRes->nWidth = pTemplate->nWidth + 1 + 2 - 2 * nBorder;
	pRes->nHeight = pTemplate->nHeight + 1 + 2 - 2 * nBorder;
	pRes->vOrigin = ptOrigin + CVec2(ptShift.x, ptShift.y);
	pRes->fRotation = ToRadian( pos.fRotation );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMapBuilder::CreateLayersGroup( const SLayerPlace &place, int nFirstFloor )
{
	OutputDebugString(" CREATING GROUP \n");
	return pNet->CreateLayersGroup(
		place.nWidth, place.nHeight,
		place.vOrigin, place.fRotation, nFirstFloor
		);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapBuilder::CreateGrids( SMapBuilding *pRes, const vector<NBuilding::SBuildFragment> &fragments, 
	const SMapPosition &pos )
{
	SRand crapSeed;
	for ( int i = 0; i < fragments.size(); ++i )
	{
		const NBuilding::SBuildFragment &frag = fragments[i];
		if ( frag.nSubBlockID != NBuilding::GetPartHashID( 1, 1, 1 ) && frag.nSubBlockID != 0 )
			continue;
		NDb::CTConstructionPart *pTCP = NDb::GetTConstructionPart( frag.nConstructionPartID );
		if ( !pTCP )
			continue;
		CPtr<NDb::CConstructionPart> pCP = pTCP->CreateConstructionPart( &crapSeed );
		if ( !IsValid( pCP ) || !IsValid( pCP->pGeometry ) || !IsValid( pCP->pGeometry->pAIGeometry ) )
			continue;
		CPtr<NDb::CAIGeometry> pAIG = pCP->pGeometry->pAIGeometry;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsGridNumber( float f )
{
	float fNorm = f > 0? f / FP_GRID_STEP : -f / FP_GRID_STEP;
	int nNorm = fNorm;
	return ( nNorm - fNorm ) < 1e-4 && ( nNorm - fNorm ) > -1e-4;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool AreAlignedGrids( float fSin, float fCos, const CVec2 &vOrig1, const CVec2 &vOrig2 )
{
	float fDiffX = ( vOrig1.x - vOrig2.x ) * fCos + ( vOrig1.y - vOrig2.y ) * fSin;
	float fDiffY = ( vOrig1.x - vOrig2.x ) * fSin - ( vOrig1.y - vOrig2.y ) * fCos;
	return IsGridNumber( fDiffX ) && IsGridNumber( fDiffY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapBuilder::TraverseTemplateTree( NDb::CTemplate* pTemplate, SMapInfo *pFree, 
	const SMapPosition &pos, int nDepth, NAI::SAlternativeGridInfo *pAltGrid, 
	bool bParentTerrAlign, const CVec2 &ptParentBuilding, const vector<int> &flags, int nPlacementID )
{
  if ( !IsValid( pTemplate ) || (nMaxDepth >= 0 && nDepth > nMaxDepth ) )
    return;
  ASSERT( !pTemplate->variants.empty() );
  
  NDb::CTemplVariant *pVar = NDb::GetTemplVariant( pTemplate, flags, nPlacementID, &rand );
	if ( !IsValid( pVar ) )
		return;

	// ландшафт
  const int nRecW = pTemplate->nWidth;
  const int nRecH = pTemplate->nHeight;
	
	if ( bBuildTerrain && IsValid( pNet ) && nDepth > 1 )
		BlendTerrainInfo( &info.terrain, pVar->GetRecordID(), pos.ptPos, ToRadian( pos.fRotation ), &rand, flags );

	const float fWHalf = 0.5f * FP_GRID_STEP * nRecW;
	const float fHHalf = 0.5f * FP_GRID_STEP * nRecH;

	// есть ли необходимость выравнивать по террейну объекты?
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pBuildInfo = NGScene::shareBuildings.Get( pVar->GetRecordID() );
	pBuildInfo.Refresh();
	NBuilding::CBuildInfo *pBInfo = pBuildInfo->GetValue();
	bool bTerrAlign = bParentTerrAlign && pBInfo->wallFragments.empty() && pBInfo->solidFragments.empty();
	CVec2 ptBuilding( fWHalf, fHHalf );
	if ( bParentTerrAlign )
	{
		RotatePt( &ptBuilding, pos.fRotation );
		ptBuilding.x += pos.ptPos.x;
		ptBuilding.y += pos.ptPos.y;
	}
	else
		ptBuilding = ptParentBuilding;

	// Calc layer place
	SLayerPlace layerPlace;
	int nTemplateBorder = nDepth == 1 ? Min( Max(pVar->nBorder,0), Min( nRecW / 2, nRecH / 2 ) )  : 0;
	CalcLayerPlace( &layerPlace, pos, pTemplate, nTemplateBorder );
	parentLayerRotations.push_back( SLayerRotationInfo( layerPlace.fRotation, layerPlace.vOrigin ) );
	if ( nDepth == 1 )
	{
		rootPlace = layerPlace;
		if ( IsValid( pNet ) )
			CreateLayersGroup( layerPlace, -2 );
	}
	// traverse childs
  const int nmax = pVar->rects.size();
	SMapInfo nested;
	NAI::SAlternativeGridInfo nestedAltGrids;
	for ( int i = 0; i < nmax; ++i )
	{
		// преобразование координат при поворотах
		NDb::CRectangle *pRec = pVar->rects[i];
		if ( !IsValid( pRec ) )
			continue;
		SMapPosition child;
		CVec3 ptLocal( pRec->ptCenter.x, pRec->ptCenter.y, pRec->nFloor * WALL_HEIGHT + pRec->fDZ );
		RotatePt( &ptLocal, pos.fRotation );
		child.ptPos = pos.ptPos + ptLocal;
		child.nFloor = pos.nFloor + pRec->nFloor;
		child.fRotation = int( pos.fRotation + pRec->fRotation + 360 * 100 ) % 360;
		vector<int> childFlags( flags );
		ConvertFlags( &childFlags, pRec->vszParams );
		TraverseTemplateTree( pRec->pTemplate, &nested, child, nDepth + 1, &nestedAltGrids, bTerrAlign, ptBuilding, childFlags );
	}
	// скрипты
	if ( IsValid( pVar->pScript ) )
		nested.scripts.push_back( pVar->pScript.GetPtr() );
	// здания
	SMapBuilding b;
  // объекты и юниты
	SMapPosition posObjects = pos;
  AddSimpleElements( &nested, &b, pVar, posObjects, bTerrAlign, ptBuilding, flags );
	AddWaypoints( &nested, pVar, posObjects, bTerrAlign, ptBuilding );
	ResolveRoutes( &nested );

	int nNewGroup = -1;
	b.pVariant = pVar;
	if ( IsValid( pNet ) && CreateBuilding( &nested, &b, pBInfo, pos, ptBuilding, flags ) )
	{
		b.pGrid->SetBaseFloor( pos.nFloor );
		// create AI layers and mark rooms on them
		WriteLightRooms( &nested, b.pGrid );
		float fSinL = sin( layerPlace.fRotation );
		const float fEpsilon = 1e-5f;
		bool bSkip = true;
		if ( nDepth > 1 )
		{
			bSkip = false;
			for ( int i = 0; i < parentLayerRotations.size() - 1; ++i )
			{
				float fSinR = parentLayerRotations[i].fSinR;
				float fCosR = parentLayerRotations[i].fCosR;
				bool bSameRotation = fabs( fSinL - fSinR ) < fEpsilon || fabs( fSinL + fSinR ) < fEpsilon ||
						 fabs( fSinL - fCosR ) < fEpsilon || fabs( fSinL + fCosR ) < fEpsilon;		
				if ( !bSameRotation )
					continue;
				bool bSameOrigin = AreAlignedGrids( fSinR, fCosR, parentLayerRotations[i].vOrigin, layerPlace.vOrigin );
				if ( !bSameOrigin )
					continue;
				bSkip = true;
				break;
			}
		}
		if ( !bSkip )
		{
			int nLayersCount = pBInfo->nMaxFloor + 1;
			if ( pBInfo->nMinFloor < 0 )
				nLayersCount -= pBInfo->nMinFloor;
			nNewGroup = CreateLayersGroup( layerPlace, - 2 ); 
			if ( !pVar->bGrid )
			{
				csSystem << CC_RED << " WARNING! Template #" << pTemplate->GetRecordID() << " was not marked as grid-creating";
				csSystem << " and has building fragments, unaligned rotation and/or position " << endl;
			}
		}
		for ( int nRelFloor = pBInfo->nMinFloor; nRelFloor <= pBInfo->nMaxFloor; ++nRelFloor )
		{
			int nZ = nRelFloor + pos.nFloor; // абсолютный номер этажа
			SetOnLayer( &info, &nested, nZ );	
			//
			b.stories.push_back( SMapBuilding::SStorey( nRelFloor, nZ ) );
		}
		CreateGrids( &b, pBInfo->solidFragments, pos );
		info.buildings.push_back( b );
		//ladders
		vector<NBuilding::SLadder> &lads = pBInfo->ladders;
		if( !lads.empty() )
		{
			for ( int i = 0; i < lads.size(); ++i )
			{
				NBuilding::SLadder &lad = lads[i];
				if ( lad.nID <= 0 )
					continue;
				//const float fLadFloors = lad.nHeight * NAI::F_LADDER_STEP / NBuilding::WALL_HEIGHT;
				int x = lad.pos.ptMove.x + 1; // CRAP вычитается в aiGrid
				int y = lad.pos.ptMove.y + 1;
				if ( nNewGroup >= 0 )
					pNet->CreateLadder( x, y, lad.nHeight, lad.pos.nRotation, nNewGroup, lad.pos.ptMove.z + pos.nFloor );
				else
				{
					CVec2 vDisplacement = layerPlace.vOrigin - rootPlace.vOrigin;
					x += vDisplacement.x / FP_GRID_STEP;
					y += vDisplacement.y / FP_GRID_STEP;
					// CRAP{
					if ( x < 0 )
						x = 0;
					if ( y < 0 )
						y = 0;
					if ( x >= pTemplate->nWidth )
						x = pTemplate->nWidth - 1;
					if ( y >= pTemplate->nWidth )
						y = pTemplate->nWidth - 1;
					// }CRAP
					pNet->CreateLadder( x, y, lad.nHeight, lad.pos.nRotation, 0, lad.pos.ptMove.z + pos.nFloor );
				}
			}
		}
	}
	// add layer to exceptions list
	if ( nDepth != 1 && nNewGroup != -1 )
	{
		nestedAltGrids.nLayersGroup = nNewGroup;
		pAltGrid->children.push_back( nestedAltGrids );
	}
	else
		pAltGrid->children.insert( pAltGrid->children.end(), nestedAltGrids.children.begin(), nestedAltGrids.children.end() );
	// if marked as grid creating then should create grid
	if ( nDepth == 1 )
	{
		//SetOnLayer( &info, &nested, 0 );
		SetOnLayer( &info, &nested, 1000 );
		ResolveRoutes( &info );
		info.bShowTerrain = pVar->bShowTerrain;
	}
	Transfer( pFree, &nested );
	if ( nDepth == 1 )
	{
		ASSERT( pFree->items.empty() );
		ASSERT( pFree->units.empty() );
	}
	///////////////
	parentLayerRotations.pop_back();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapBuilder::TraverseTerrainTree( NDb::CTemplate* pTemplate, float fDZ,	const SMapPosition &pos, int nDepth, const vector<int> &flags, int nPlacementID )
{
	if ( !IsValid( pTemplate ) || (nMaxDepth >= 0 && nDepth > nMaxDepth ) )
		return;
	NDb::CTemplVariant *pVar = NDb::GetTemplVariant( pTemplate, flags, nPlacementID, &rand );
	if ( !IsValid( pVar ) )
		return;
	if ( nDepth > 1 )
		BlendTerrainInfo( &info.terrain, pVar->GetRecordID(), pos.ptPos, ToRadian( pos.fRotation ), &rand, flags );
	for ( int i = 0; i < pVar->rects.size(); ++i )
	{
		// преобразование координат при поворотах
		NDb::CRectangle *pRec = pVar->rects[i];
		if ( !IsValid( pRec ) )
			continue;
		SMapPosition child;
		CVec3 ptLocal( pRec->ptCenter.x, pRec->ptCenter.y, pRec->nFloor * WALL_HEIGHT + pRec->fDZ );
		RotatePt( &ptLocal, pos.fRotation );
		child.ptPos = pos.ptPos + ptLocal;
		child.nFloor = pos.nFloor + pRec->nFloor;
		child.fRotation = int( pos.fRotation + pRec->fRotation + 360 * 100 ) % 360;
		vector<int> childFlags( flags );
		ConvertFlags( &childFlags, pRec->vszParams );
		// необходимо чтобы сгенеренный террейн соответствовал варианту подтемплейта, который сгенерится в редакторе
		if ( bResetPins || pRec->nPinID < 0 )
		{
			NDb::CTemplVariant *pPin = NDb::GetTemplVariant( pRec->pTemplate, childFlags, -1, &rand );
			if ( pPin )
				pRec->nPinID = pPin->GetRecordID();
		}
		TraverseTerrainTree( pRec->pTemplate, pRec->fDZ, child, nDepth + 1, childFlags, pRec->nPinID );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapBuilder::AddMapHole( list<SMapHole> *pList, const TPolygonsList &polygonsList, int nHeight, bool bVisible )
{
	SMapHole &sHole = *pList->insert( pList->end(), SMapHole());
	sHole.nFloor = info.nBaseTerrainFloor;
	sHole.nHeight = nHeight;
	sHole.bVisible = bVisible;
	sHole.polygonsList = polygonsList;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapBuilder::BuildHolesList()
{
	info.nBaseTerrainFloor = 0;
	
	list<SMapHole> &holesList = info.holesList;
	const STerrainInfo &sInfo = info.terrain;
	for( vector<STerrainHole>::const_iterator iTempHole = sInfo.holes.begin(); iTempHole != sInfo.holes.end(); iTempHole++ )
	{
		bool bHandled = false;
		list<SMapHole> newHolesList;

		TPolygonsList sourcePolygonsList;
		if ( IsPolygonInverse( iTempHole->vPolygon ) )
		{
			vector<CVec2> reversed( iTempHole->vPolygon );
			reverse( reversed.begin(), reversed.end() );
			sourcePolygonsList.push_back( reversed );
		}
		else
			sourcePolygonsList.push_back( iTempHole->vPolygon );
		DumpPolyList( sourcePolygonsList );
		for ( list<SMapHole>::iterator iTempGroupHole = holesList.begin(); iTempGroupHole != holesList.end(); iTempGroupHole++ )
		{
			TPolygonsList intPolygonsList, subPolygonsList, outClippedPolygonsList;
			ClipPolygon( iTempGroupHole->polygonsList, sourcePolygonsList, &intPolygonsList, &subPolygonsList );
			ClipPolygon( sourcePolygonsList, iTempGroupHole->polygonsList, 0, &outClippedPolygonsList );

			if ( !intPolygonsList.empty() )
				AddMapHole( &newHolesList, intPolygonsList, iTempGroupHole->nHeight + iTempHole->nHeight, iTempGroupHole->bVisible == iTempHole->bVisible );
			if ( !subPolygonsList.empty() )
				AddMapHole( &newHolesList, subPolygonsList, iTempGroupHole->nHeight, iTempGroupHole->bVisible );

			sourcePolygonsList = outClippedPolygonsList;
		}

		if ( !sourcePolygonsList.empty() )
			AddMapHole( &newHolesList, sourcePolygonsList, iTempHole->nHeight, iTempHole->bVisible );
		holesList = newHolesList;
	}

	GenerateWalls();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float FP_EPS = 1e-05f;
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline bool IsEqual( const CVec2 &v1, const CVec2 &v2, float fEps = FP_EPS )
{
	return fabs( v1 - v2 ) < fEps;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SEdge
{
	CVec2 vBeg;
	CVec2 vEnd;

	SEdge() {}
	SEdge( const CVec2 &_vBeg, const CVec2 &_vEnd ): vBeg( _vBeg ), vEnd( _vEnd ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapBuilder::GenerateWalls()
{
	for ( list<SMapHole>::const_iterator iHole = info.holesList.begin(); iHole != info.holesList.end(); iHole++ )
	{
		int nBaseHeight = iHole->nHeight;
		list<SEdge> edgesList;
		const SMapHole &sHole = *iHole;

		for ( TPolygonsList::const_iterator iPolygon = sHole.polygonsList.begin(); iPolygon != sHole.polygonsList.end(); iPolygon++ )
		{
			const vector<CVec2> &pointsSet = *iPolygon;
			for ( int nTemp = 0; nTemp < pointsSet.size(); nTemp++ )
			{
				const CVec2 &vBeg = pointsSet[nTemp];
				const CVec2 &vEnd = pointsSet[( nTemp + 1 ) % pointsSet.size()];

				edgesList.push_back( SEdge( vBeg, vEnd ) );
			}
		}

		list<SMapHole>::const_iterator iNext = iHole;
		iNext++;

		for ( list<SMapHole>::const_iterator iHole = info.holesList.begin(); iHole != iNext; iHole++ )
		{
			const SMapHole &sHole = *iHole;
			for ( TPolygonsList::const_iterator iPolygon = sHole.polygonsList.begin(); iPolygon != sHole.polygonsList.end(); iPolygon++ )
			{
				const vector<CVec2> &pointsSet = *iPolygon;
				for ( int nTemp = 0; nTemp < pointsSet.size(); nTemp++ )
				{
					const CVec2 &vBeg = pointsSet[nTemp];
					const CVec2 &vEnd = pointsSet[( nTemp + 1 ) % pointsSet.size()];

					for ( list<SEdge>::iterator iEdge = edgesList.begin(); iEdge != edgesList.end(); )
					{
						SEdge &sEdge = *iEdge;
						if ( IsEqual( sEdge.vBeg, vEnd ) && IsEqual( sEdge.vEnd, vBeg ) )
							iEdge = edgesList.erase( iEdge );
						else
							iEdge++;
					}
				}
			}
		}
		for ( list<SMapHole>::const_iterator iHole = iNext; iHole != info.holesList.end(); iHole++ )
		{
			const SMapHole &sHole = *iHole;
			for ( TPolygonsList::const_iterator iPolygon = sHole.polygonsList.begin(); iPolygon != sHole.polygonsList.end(); iPolygon++ )
			{
				const vector<CVec2> &pointsSet = *iPolygon;
				for ( int nTemp = 0; nTemp < pointsSet.size(); nTemp++ )
				{
					const CVec2 &vBeg = pointsSet[nTemp];
					const CVec2 &vEnd = pointsSet[( nTemp + 1 ) % pointsSet.size()];

					for ( list<SEdge>::iterator iEdge = edgesList.begin(); iEdge != edgesList.end(); )
					{
						SEdge &sEdge = *iEdge;
						if ( IsEqual( sEdge.vBeg, vEnd ) && IsEqual( sEdge.vEnd, vBeg ) )
						{
							SMapWall &sWall = *info.wallsList.insert( info.wallsList.end(), SMapWall());

							sWall.nHeightMin = sHole.nHeight;
							sWall.nHeightMax = nBaseHeight;
							sWall.vBeg = sEdge.vBeg;
							sWall.vEnd = sEdge.vEnd;

							iEdge = edgesList.erase( iEdge );
						}
						else
							++iEdge;
					}
				}
			}
		}

		for ( list<SEdge>::iterator iEdge = edgesList.begin(); iEdge != edgesList.end(); iEdge++ )
		{
			SEdge &sEdge = *iEdge;

			SMapWall &sWall = *info.wallsList.insert( info.wallsList.end(), SMapWall());

			sWall.nHeightMin = 0;
			sWall.nHeightMax = nBaseHeight;
			sWall.vBeg = sEdge.vBeg;
			sWall.vEnd = sEdge.vEnd;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
inline void FixHeights( STerrainInfo &terrain, T *pSet )
{
	for ( T::iterator i = pSet->begin(); i != pSet->end(); ++i )
		i->pos.ptPos.z += GetMeterHeightCheck( terrain, i->ptAlignTo.x, i->ptAlignTo.y );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
inline void FixHeightsPtr( STerrainInfo &terrain, T *pSet )
{
	for ( T::iterator i = pSet->begin(); i != pSet->end(); ++i )
			(*i)->pos.ptPos.z += GetMeterHeightCheck( terrain, (*i)->ptAlignTo.x, (*i)->ptAlignTo.y );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapBuilder::SetParams( const vector<string> &strParams )
{
	ConvertFlags( &initialFlags, strParams );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapBuilder::AddWaypoints( SMapInfo *pDst, NDb::CTemplVariant *pVar, const SMapPosition &parent, bool bTerrAlign,
	const CVec2 &ptAlignTo )
{
	if ( !IsValid( pVar ) )
		return;
	// добавляем waypoints из текущей расстановки
	for ( int i = 0; i < pVar->waypoints.size(); ++i )
	{
		NDb::CWaypoint *pdbW = pVar->waypoints[i];
		if ( !IsValid( pdbW ) || !IsValid( pdbW->pName ) )
		{
			ASSERT(0);
			continue;
		}
		CDGPtr< CPtrFuncBase<NAI::CWaypoint> > pWPLoader = shareWaypoints.Get( pdbW->GetRecordID() );
		if ( !IsValid( pWPLoader ) )
			continue;
		pWPLoader.Refresh();
		CPtr<NAI::CWaypoint> pW = pWPLoader->GetValue();
		if ( !IsValid( pW ) )
			continue;
		CMapWaypoint *pMW = new CMapWaypoint;
		pMW->bExists = true;
		pMW->pName = pdbW->pName;
		pMW->commands = pW->commands;
		CalcPosition( &pMW->pos, pW.GetPtr() , parent );
		if ( bTerrAlign ) 
			pMW->ptAlignTo = ptAlignTo;
		else
			pMW->ptAlignTo = CVec2( pMW->pos.ptPos.x, pMW->pos.ptPos.y );
		info.waypoints.push_back( pMW );
		waypoints[pdbW->pName->GetRecordID()] = pMW;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapBuilder::ResolveRoute( CPtrFuncBase<NAI::CUnitAIInfo> *pLoader, vector<CPtr<CMapWaypoint> > *pRoute )
{
	vector<CPtr<CMapWaypoint> > &route = *pRoute;
	CDGPtr< CPtrFuncBase<NAI::CUnitAIInfo> > pUnitLoader = pLoader;
	if ( !IsValid( pUnitLoader ) )
		return;
	pUnitLoader.Refresh();
	CPtr<NAI::CUnitAIInfo> pU = pUnitLoader->GetValue();
	if ( !IsValid( pU ) || pU->routes.empty() )
		return;
	//
	unordered_map<int, CPtr<CMapWaypoint> > routeHash;
	for ( int j = 0; j < route.size(); ++j )
		if ( route[j]->bExists )
			routeHash[route[j]->pName->GetRecordID()] = route[j];
	route.clear();
	//
	for ( int j = 0; j < pU->routes.front().waypoints.size(); ++j )
	{
		NDb::CWaypointName *pWN = NDb::GetWaypointName( pU->routes.front().waypoints[j] );
		if ( !IsValid( pWN ) )
			continue;
		CPtr<CMapWaypoint> &pOldMW = routeHash[pWN->GetRecordID()];
		if ( IsValid( pOldMW ) )
			route.push_back( pOldMW );
		else
		{
			CObj<CMapWaypoint> &pMW = waypoints[pWN->GetRecordID()];
			if ( IsValid( pMW ) )
				route.push_back( pMW.GetPtr() );
			else
			{
				CMapWaypoint *p = new CMapWaypoint();
				p->pName = pWN;
				route.push_back( p );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapBuilder::ResolveRoutes( SMapInfo *pInfo )
{
	// обходим всех добавленных юнитов и проверяем их маршруты, уже установленные флажки пропускаем, 
	// если на карте появились новые подходящие флажки - вставляем в нужном порядке в маршрут
	for ( list<SMapUnit>::iterator i = pInfo->units.begin(); i != pInfo->units.end(); ++i )
		ResolveRoute( shareUnits.Get( i->nUnitID ), &i->route );
	for (unordered_map<int, SUnitGroup>::iterator i = pInfo->groups.begin(); i != pInfo->groups.end(); ++i )
		ResolveRoute( shareUnitGroups.Get( i->first ), &i->second.route );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapBuilder::BuildMap( int nPlacementID )
{
  CDBTable<NDb::CTemplVariant> *pVarTable = NDatabase::GetTable<NDb::CTemplVariant>();
  if ( !pVarTable )
    return false;
	NDb::CTemplVariant *pVar = pVarTable->GetRecord( nPlacementID );
	if ( !pVar || !pVar->pTemplate )
	{
		ASSERT( 0 );
		return false;
	}
	bool bRet = true;

	if ( bBuildTerrain )
	{
		info.terrain.nWidth  = pVar->pTemplate->nWidth;// + 1;
		info.terrain.nHeight = pVar->pTemplate->nHeight;// + 1;
		bRet = LoadRootTerrain( nPlacementID, &info.terrain, &rand, initialFlags );
	}

	if ( IsValid( pVar->pLight ) )
		info.pDefaultLight = pVar->pLight->GetLight( &rand, initialFlags );
	
	SMapInfo freeInfo;
	NAI::SAlternativeGridInfo altGrids;
	TraverseTemplateTree( pVar->pTemplate, &freeInfo, posInitial, 1, &altGrids, 
		bTerrAlignmentInitial, ptParentBuildingInitial, initialFlags, nPlacementID );
	altGrids.nLayersGroup = 0;
	if ( IsValid( pNet ) )
	{
		FixHeights( info.terrain, &info.items );
		FixHeights( info.terrain, &explosions );
		FixHeights( info.terrain, &info.deploySpots );
		FixHeights( info.terrain, &info.rpgitems );
		FixHeights( info.terrain, &info.slots );
		FixHeights( info.terrain, &info.units );
		FixHeightsPtr( info.terrain, &info.waypoints );
	}
	
	vector<SMapBuilding>::iterator it;
	for ( it = info.buildings.begin(); it != info.buildings.end(); ++it )
	{
		SMapBuilding &b = *it;
		CVec3 pos( b.mpos.ptPos );
		pos.z += GetMeterHeightCheck( info.terrain, b.ptAlignTo.x, b.ptAlignTo.y );
		MakeMatrix( &b.pos, CVec3(1,1,1), pos, ToRadian( b.mpos.fRotation ) );
		b.pGrid->SetPos( b.pos );
	}

	// Заданные в редакторе разрушения 
	for ( int j = 0; j < explosions.size(); ++j )
	{
		const SExplosion &ex = explosions[j];
		for ( int i = 0; i < info.buildings.size(); ++i )
			info.buildings[i].pGrid->Explode( ex.pos.ptPos, ex.fPower, ex.fRadius );
		if ( ex.nObjStageDelta > 0 && ex.fObjRadius > FP_EPSILON )
		{
			const float fRadius2 = sqr( ex.fObjRadius * FP_INV_GRID_STEP );
			for ( list<SMapElement>::iterator i = info.items.begin(); i != info.items.end(); ++i )
			{
				SMapElement &m = *i;
				if ( fabs2( m.pos.ptPos - ex.pos.ptPos ) < fRadius2 )
					m.nObjectPhase += ex.nObjStageDelta;
			}
		}
	}
	// Build holes, add HG for each terrain part
	if ( IsValid( pNet ) )
		BuildHolesList();
	// 
	if ( IsValid( pNet ) )
	{
		// create special layers to resolve walking on different grids troubles
		pNet->CreateAlternativeGrids( altGrids );
	}
	//
	ClearTerrainCache();

	if ( bBuildTerrain )
	{
		MakeSoundMap( &info.terrain );
		CalcAverageColor( &info.terrain );
	}

	// output borders
	CVec2 vMapSize( (rootPlace.nWidth - 1) * FP_GRID_STEP, (rootPlace.nHeight - 1) * FP_GRID_STEP );
	CVec2 vMapBorder( rootPlace.vOrigin );
	info.sMapSafeZone = CTRect<float>( vMapBorder.x, vMapBorder.y, vMapSize.x + vMapBorder.x, vMapSize.y + vMapBorder.y );

	// add borders to terrain
	if ( bBuildTerrain )
	{
		float fX1 = rootPlace.vOrigin.x - FP_GRID_STEP;
		float fX2 = rootPlace.vOrigin.x + rootPlace.nWidth * FP_GRID_STEP;
		float fY1 = rootPlace.vOrigin.y - FP_GRID_STEP;
		float fY2 = rootPlace.vOrigin.y + rootPlace.nHeight * FP_GRID_STEP;
		NDb::CMaterial *pSpot = NDb::GetMaterial(1904); // CRAP
		for ( int k = 0; k < rootPlace.nWidth + 1; ++k )
		{
			float fX = rootPlace.vOrigin.x + (k - 1) * FP_GRID_STEP;
			info.terrain.spots.push_back( STerrainSpot( pSpot, CVec2( fX, fY1 ), CVec2(FP_GRID_STEP, FP_GRID_STEP*0.5f), ToRadian(0.0f), SStepSound() ) );
			info.terrain.spots.push_back( STerrainSpot( pSpot, CVec2( fX + FP_GRID_STEP, fY2 ), CVec2(FP_GRID_STEP, FP_GRID_STEP*0.5f), ToRadian(180.0f), SStepSound() ) );
		}
		for ( int k = 0; k < rootPlace.nHeight + 1; ++k )
		{
			float fY = rootPlace.vOrigin.y + (k - 1) * FP_GRID_STEP;
			info.terrain.spots.push_back( STerrainSpot( pSpot, CVec2( fX1, fY + FP_GRID_STEP ), CVec2(FP_GRID_STEP, FP_GRID_STEP*0.5f), ToRadian(270.0f), SStepSound() ) );
			info.terrain.spots.push_back( STerrainSpot( pSpot, CVec2( fX2, fY ), CVec2(FP_GRID_STEP, FP_GRID_STEP*0.5f), ToRadian(90.0f), SStepSound() ) );
		}
	}
	// add border
	if ( bBuildTerrain )
	{
		const float F_HALF_BLOCK_SIZE = 2.5f;
		float fX1 = rootPlace.vOrigin.x - F_HALF_BLOCK_SIZE + FP_GRID_STEP;
		float fX2 = rootPlace.vOrigin.x + ( rootPlace.nWidth - 1 ) * FP_GRID_STEP + F_HALF_BLOCK_SIZE - FP_GRID_STEP;
		float fY1 = rootPlace.vOrigin.y - F_HALF_BLOCK_SIZE + FP_GRID_STEP;
		float fY2 = rootPlace.vOrigin.y + ( rootPlace.nHeight - 1 ) * FP_GRID_STEP + F_HALF_BLOCK_SIZE - FP_GRID_STEP;
		//
		SMapPosition pos;
		pos.nFloor = 100;
		pos.ptPos.z = 0;
		pos.ptScale = CVec3( 1, 1, 1 );
		CDBPtr<NDb::CRndObject> pRndObject = NDb::GetDBRndObject( 1836 );
		CPtr<NDb::CObject> pObject = pRndObject->CreateObject( &SRand(), vector<int>() );
		for ( pos.ptPos.x = fX1; pos.ptPos.x <= fX2; pos.ptPos.x += 2 * F_HALF_BLOCK_SIZE )
		{
			pos.ptPos.y = fY1;
			info.items.push_back( SMapElement( pObject, pos, true ) );
			pos.ptPos.y = fY2;
			info.items.push_back( SMapElement( pObject, pos, true ) );
		}
		for ( pos.ptPos.y = fY1; pos.ptPos.y <= fY2; pos.ptPos.y += 2 * F_HALF_BLOCK_SIZE )
		{
			pos.ptPos.x = fX1;
			info.items.push_back( SMapElement( pObject, pos, true ) );
			pos.ptPos.x = fX2;
			info.items.push_back( SMapElement( pObject, pos, true ) );
		}
	}

	//
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapBuilder::BuildTerrain( int nPlacementID )
{
	CDBTable<NDb::CTemplVariant> *pVarTable = NDatabase::GetTable<NDb::CTemplVariant>();
	if ( !pVarTable )
		return false;
	NDb::CTemplVariant *pVar = pVarTable->GetRecord( nPlacementID );
	if ( !pVar || !pVar->pTemplate )
	{
		ASSERT( 0 );
		return false;
	}
	bool bRet = true;
	info.terrain.nWidth  = pVar->pTemplate->nWidth;// + 1;
	info.terrain.nHeight = pVar->pTemplate->nHeight;// + 1;

	bRet = LoadRootTerrain( nPlacementID, &info.terrain, &rand, initialFlags );

	CVec3 ptCenter( 0, 0, 0 );
	SMapPosition pos;
	pos.nFloor = 0;
	pos.fRotation = 0;
	pos.ptPos = ptCenter;
	TraverseTerrainTree( pVar->pTemplate, 0, pos, 1, initialFlags, nPlacementID );
	if ( bShowHoles )
		BuildHolesList();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool BuildMap( int nPlacementID, const vector<string> &strParams, 
	NAI::IPathNetwork *pNet, SMapInfo *pInfo, int nDepth, SRandomSeed sSeed )
{
	if ( nDepth == 0 )
		return false;
	CMapBuilder builder( pInfo, pNet, sSeed );
	builder.SetParams( strParams );
	builder.SetMaxDepth( nDepth );
	return builder.BuildMap( nPlacementID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool BuildTerrain( int nPlacementID, SMapInfo *pInfo, int nDepth, bool bResetPins, bool bShowHoles )
{
	if ( nDepth == 0 )
		return false;
	CMapBuilder builder( pInfo, 0, SRandomSeed() );
	builder.SetMaxDepth( nDepth );
	builder.SetResetPins( bResetPins );
	builder.SetShowHoles( bShowHoles );
	return builder.BuildTerrain( nPlacementID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool BuildMapEditMap( int nMapID, NAI::IPathNetwork *pNet, SMapInfo *pInfo, int nDepth, const SMapPosition &pos, 
	const CVec2 &ptBuilding, bool bTerrAlign )
{
	if ( nDepth == 0 )
		return false;
	CMapBuilder builder( pInfo, pNet, SRandomSeed() );
	builder.SetMaxDepth( nDepth );
	builder.SetPos( pos );
	builder.SetTerrAlignment( bTerrAlign );
	builder.SetParentBuildingPos( ptBuilding );
	builder.SetBuildTerrain( false );
	return builder.BuildMap( nMapID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
