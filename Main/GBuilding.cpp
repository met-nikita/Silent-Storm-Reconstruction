#include "StdAfx.h"
#include "GScene.h"
#include "GSceneUtils.h"
#include "..\Misc\2DArray.h"
#include "MapBuildingInfo.h"
#include "GBuilding.h"
#include "MakeBuilding.h"
#include "GResource.h"
#include "..\Misc\BasicShare.h"
#include "GObjectInfo.h"
#include "BuildingInfo.h"
#include "BuildingGrid.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\DBFormat\DataFormat.h"
#include "Transform.h"
#include "Grid.h"
#include "GDecalGeometry.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NBuilding;
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static CBasicShare<SClipShare, CWallObjectInfoClipper, SClipInfoHash> shareWallClippers(116);
static CBasicShare<SClipShare, CSolidObjectInfoClipper, SClipInfoHash> shareSolidClippers(124);
CBasicShare<int, NBuilding::CBuildInfoLoader> shareBuildings(108);
CResourceTracker objChecker( "Geometries" );
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBuilding
////////////////////////////////////////////////////////////////////////////////////////////////////
CBuilding::CBuilding()
{
	bParts = false;
	bRefresh = false;
	pPlace = new CFBTransform;
	Identity( &pPlace->pos.forward );
	Identity( &pPlace->pos.backward );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SGroupInfo CBuilding::GetGroupInfo( /*int _nRoom, */int nFloor, bool bIndestructible )
{
	int nFloor2 = bIndestructible && nFloor < 0 ? NGScene::N_MIN_FLOOR : nFloor;
	//return SGroupInfo( pBuildingGrid->GetRoomGlobal( nFloor, _nRoom ), GetFloorBit( nFloor2 + pBuildingGrid->GetBaseFloor(), true, false ) );
	return SGroupInfo( 0, GetFloorBit( nFloor2 + pBuildingGrid->GetBaseFloor(), true, false ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuilding::Update( IGScene *pScene, CMaterialShare *pMaterials )
{
	if ( !IsValid( pBInfo ) )
		return false;
	bool bGridUpd = pBuildingGrid.Refresh();
	//bool bInfoUpd = pBuildInfo.Refresh();
	if ( bGridUpd || bRefresh )
	{
		if ( !bGridUpd )
		{
			// BuildInfo changed (in the MapEditor); need to verify the BuildingGrid dimensions still match
			CVec3 ptMin, ptMax;
			pBuildingGrid->GetSize( &ptMin, &ptMax );
			ASSERT(0);
			/*
			CBuildInfo *pBI = pBInfo->GetValue();
			if ( ptMax.x != pBI->nMaxX || ptMax.y != pBI->nMaxY || ptMax.z != pBI->nMaxFloor || ptMin.z != pBI->nMinFloor )
				pBuildingGrid->Setup( pBI->nMaxX, pBI->nMaxY, pBI->nMinFloor, pBI->nMaxFloor, VNULL2 ); // retail @0xc3a90: no transform param
			pBuildingGrid->Updated(); // to rebuild the AIMap
			*/
		}
		Build( pScene, pMaterials );
		bRefresh = false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsOccluder( CMixedMaterial *p )
{
	if ( p->layers.empty() )
		return false;
	NDb::CMaterial *pMat = p->layers[0].pMaterial;
	if ( pMat && ( pMat->alpha == NDb::CMaterial::A_OPAQUE || pMat->alpha == NDb::CMaterial::A_SELF_ILLUM ) )
		return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::Build( IGScene *pScene, CMaterialShare *pMaterials )
{
	IGScene &scene = *pScene;
	renderParts.clear();
	// build info - статические данные, поэтому возвращаемеое значение Refresh не проверяется
	const SBuildingInfo &info = pBInfo->GetInfo();
	NBuilding::SPart part;
	part.nID = nPartID;
	unordered_map<NBuilding::SPart, NBuilding::SStoreyInfo, NBuilding::SPart>::const_iterator i = info.info.find( part );
	if ( info.info.end() == i )
		return;
	{
		const SStoreyInfo &storey = i->second;
		for ( vector<SStoreyInfo::SFragment>::const_iterator k = storey.walls.begin(); k != storey.walls.end(); ++k )
		{
			const SStoreyInfo::SFragment &wall = *k;
			CVec3 ptPos = wall.info.ptPos;
			ptPos.x *= FP_GRID_STEP;
			ptPos.y *= FP_GRID_STEP;
			ptPos.z *= WALL_HEIGHT;
			SDiscretePos dPos( pPlace, ptPos, wall.info.nRotationID );
			SGroupInfo gi = GetGroupInfo( storey.nFloor );

			// add occluders
			NDb::CAIGeometry *pAIGeom = wall.info.pGeometry->pAIGeometry;
			if ( pAIGeom && IsOccluder( wall.info.pMaterials[0] ) )
			{
				vector<int> parts;
				NBuilding::UnpackParts( &parts, wall.info.dwParts, wall.info.nSubBlockID );
				if ( !parts.empty() )
				{
					CObjectBase *p = scene.CreateGeometry( new CAIGeometryConverter( pAIGeom->GetRecordID(), parts ), 
						pMaterials->CreateOccluderMaterial(), dPos, SFullGroupInfo( gi, 0, 0 ) );
					renderParts.push_back( p );
				}
			}

			// add normal geometry
			for ( int j = 0; j < NDb::N_MODEL_MATERIALS; ++j )
			{
				if ( !IsValid( wall.info.pMaterials[j] ) || wall.info.pMaterials[j]->layers.empty() )
					continue;
				
				SClipShare clip( wall.info );
				clip.src.nID = wall.info.pGeometry->GetRecordID();
				clip.src.nPart = j;

				if ( !objChecker.DoesExist( clip.src ) )
					continue;
				CPtrFuncBase<CObjectInfo> *pClipper = shareWallClippers.Get( clip );
				IMaterial *pMat = pMaterials->CreateMaterial( wall.info.pMaterials[j]->layers.begin()->pMaterial );
				/*short nRoom = j > 0 ? wall.info.nRooms[1] : wall.info.nRooms[0];
				if ( 0x20000000 & wall.nFragmentID )
					nRoom = 0;*/
				int nUserID = (wall.nFragmentID ^ (j<<20)) | 0x40000000;
				CObjectBase *pPart = scene.CreateGeometry( pClipper, pMat, dPos, SFullGroupInfo( gi, pBInfo, nUserID ) );
				if ( !pPart )
					continue;
				renderParts.push_back( pPart );

				for ( int is = 0; is < wall.spots.size(); ++is  )
				{
					const SStoreyInfo::SSpot &s = wall.spots[is];
					if ( s.nMaterialMask & (1 << j) )
						continue;
					CDecalGeometry *pDecal = new CDecalGeometry( pClipper, SDiscretePos( 0,  ptPos, wall.info.nRotationID ), s.ptOrigin, s.ptNormal, s.ptSize, ToRadian( (float)s.nRotation ) );
					IMaterial *pM = pMaterials->CreateMaterial( s.pMaterial );
					CObjectBase *pPart = scene.CreateGeometry( pDecal, pM, SDiscretePos( pPlace, VNULL3, 0 ), SFullGroupInfo( gi, 0, 0 ) );
					renderParts.push_back( pPart );
				}
			}
		}
		//
		for ( vector<SStoreyInfo::SFragment>::const_iterator k = storey.fragments.begin(); k != storey.fragments.end(); ++k )
		{
			const SStoreyInfo::SFragment &solid = *k;
			CVec3 ptPos = solid.info.ptPos;
			SDiscretePos dRotatePos( 0,  ptPos, solid.info.nRotationID );
			CVec3 ptC(1,1,0);
			dRotatePos.MoveAndRotate( &ptC );
			SPoint3 ptGrid( Float2Int( ptC.x ), Float2Int( ptC.y ), Float2Int( ptC.z * 4 ) );
			ptPos.x *= FP_GRID_STEP;
			ptPos.y *= FP_GRID_STEP;
			ptPos.z *= WALL_HEIGHT;
			SDiscretePos dPos( pPlace,  ptPos, solid.info.nRotationID );
			SGroupInfo gi = GetGroupInfo( /*solid.info.nRooms[0],*/ storey.nFloor, pBuildingGrid->IsCellar( ptGrid ) );
			
			// occluders
			NDb::CAIGeometry *pAIGeom = solid.info.pGeometry->pAIGeometry;
			if ( pAIGeom && IsOccluder( solid.info.pMaterials[0] ) )
			{
				vector<int> parts;
				NBuilding::UnpackParts( &parts, solid.info.dwParts, solid.info.nSubBlockID );
				if ( !parts.empty() )
				{
					CObjectBase *p = scene.CreateGeometry( new CAIGeometryConverter( pAIGeom->GetRecordID(), parts ), 
						pMaterials->CreateOccluderMaterial(), dPos, SFullGroupInfo( gi, 0, 0 ) );
					renderParts.push_back( p );
				}
			}
			// normal geometry
			for ( int j = 0; j < NDb::N_MODEL_MATERIALS; ++j )
			{
				SClipShare clip( solid.info );
				clip.src.nID = solid.info.pGeometry->GetRecordID();
				clip.src.nPart = j;
				if ( !objChecker.DoesExist( clip.src ) )
					continue;
				CPtrFuncBase<CObjectInfo> *pClipper = shareSolidClippers.Get( clip );
				if ( !IsValid( solid.info.pMaterials[j] ) || solid.info.pMaterials[j]->layers.empty() )
					continue;
				IMaterial *pMat = pMaterials->CreateMaterial( solid.info.pMaterials[j]->layers.begin()->pMaterial );
				int nUserID = solid.nFragmentID ^ (j<<20);
				CObjectBase *pPart = scene.CreateGeometry( pClipper, pMat, dPos, SFullGroupInfo( gi, pBInfo, nUserID ) );
				if ( !pPart )
					continue;
				renderParts.push_back( pPart );
				for ( int is = 0; is < solid.spots.size(); ++is  )
				{
					const SStoreyInfo::SSpot &s = solid.spots[is];
					if ( s.nMaterialMask & (1 << j) )
						continue;
					CDecalGeometry *pDecal = new CDecalGeometry( pClipper, SDiscretePos( 0,  ptPos, solid.info.nRotationID ), s.ptOrigin, s.ptNormal, s.ptSize, ToRadian( (float)s.nRotation ) );
					IMaterial *pM = pMaterials->CreateMaterial( s.pMaterial );
					CObjectBase *pPart = scene.CreateGeometry( pDecal, pM, SDiscretePos( pPlace, VNULL3, 0 ), SFullGroupInfo( gi, 0, 0 ) );
					renderParts.push_back( pPart );
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::GetRoomLights( vector<SRoomAmbient> *pLights )
{
	for ( CRoomLightsHash::iterator it = lights.begin(); it != lights.end(); ++it )
		pLights->push_back( SRoomAmbient( it->first, it->second ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::Setup( int _nPartID, const SMapBuilding &info, NBuilding::CBuildingInfoHold *pBI )
{
	nPartID = _nPartID;
	//nBuildInfoID = info.pVariant->GetRecordID();
	//pBuildInfo = shareBuildings.Get( nBuildInfoID );
	pBInfo = pBI;
	pPlace->pos = info.pos;
	pBuildingGrid = info.pGrid;
	for ( int i = 0; i < info.lights.size(); ++i )
	{
		const SMapBuilding::SAmbientLight &l = info.lights[i];
		int nRoomID = 0;//pBuildingGrid->GetRoomGlobal( l.nFloor, l.nRoomID );
		CRoomLightsHash::iterator it = lights.find( nRoomID );
		if ( it == lights.end() )
			lights[nRoomID] = new CLocalAmbientCalcer;
		lights[nRoomID]->AddLight( new CCVec3( l.color ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilding::CheckHP()
{
	ASSERT(0);
	/*
	if ( pBuildInfo.Refresh() )
	{
		NBuilding::BuildingHP( pBuildInfo->GetValue(), pBuildingGrid );
		pBuildingGrid->Updated();
	}
	*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS CLightCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLocalAmbientCalcer::AddLight( CFuncBase<CVec3> *pLight )
{
	if ( pLight )
		roomLights.push_back( pLight );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLocalAmbientCalcer::NeedUpdate()
{
	for ( int i = 0; i < roomLights.size(); ++i )
		if ( roomLights[i].Refresh() )
			return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLocalAmbientCalcer::Recalc()
{
	value = VNULL3;
	for ( int i = 0; i < roomLights.size(); ++i )
	{
		if ( !IsValid( roomLights[i] ) )
			continue;
		roomLights[i].Refresh();
		value += roomLights[i]->GetValue();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS CLightCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
CLightCalcer::CLightCalcer( int _nBuildingID, int nLightGroup, CFuncBase<CVec3> *pGlobalAmb, CFuncBase<CVec3> *pLocalAmb, NBuilding::CBuildingGrid *pGrid )
	:nBuildingID(_nBuildingID), nLightGroupID(nLightGroup), pGlobalAmbient(pGlobalAmb), pLocalAmbient(pLocalAmb), pBuildingGrid(pGrid)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLightCalcer::NeedUpdate()
{ 
	return pGlobalAmbient.Refresh() | pLocalAmbient.Refresh() | pBuildingGrid.Refresh(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsRoomEdge( const CArray2D<BYTE> &rooms, int nRoomID, int x, int y )
{
	return rooms[y-1][x] != nRoomID || rooms[y][x-1] != nRoomID || rooms[y-1][x-1] != nRoomID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightCalcer::Recalc()
{
	pGlobalAmbient.Refresh();
	pLocalAmbient.Refresh();
	pBuildingGrid.Refresh();
	value = pLocalAmbient->GetValue();

	if ( !IsValid( pBuildingGrid ) )
		return;
	int nFloor, nRoom;
	pBuildingGrid->GetRoomLocal( nLightGroupID, &nFloor, &nRoom );

	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = shareBuildings.Get( nBuildingID );
	pLoader.Refresh();
	NBuilding::CBuildInfo *pInfo = pLoader->GetValue();
	if ( !pInfo )
		return;
	const int nRLayer = nFloor - pInfo->nMinFloor;
	if ( nRLayer < 0 || nRLayer >= pInfo->roomMap.size() )
		return;
	const CArray2D<BYTE> &rooms = pInfo->roomMap[nRLayer];
	int nTotal = 0;
	int nDestroyed = 0;
	//
	for ( int j = 1; j < rooms.GetYSize() - 1; ++j )
		for ( int i = 1; i < rooms.GetXSize() - 1; ++i )
		{
			if ( rooms[j][i] == nRoom )
			{
			// разрушенный объем / весь объем
//				nTotal += 5;
//				for ( int k = 1; k <= 5; ++k )
//					nDestroyed += pBuildingGrid->IsDestroyed( nFloor, i, j, k );

			// разрушенная площадь / вся площадь
				nTotal += 2;
				const SPoint3 p( i -1, j - 1, nFloor * 4 - 1);
				nDestroyed += pBuildingGrid->IsDestroyed( p );
				nDestroyed += pBuildingGrid->IsDestroyed( p + SPoint3(0,0,4) );
				if ( IsRoomEdge( rooms, nRoom, i, j ) )
				{
					nTotal += 3;
					nDestroyed += pBuildingGrid->IsDestroyed( p + SPoint3(0,0,1) );
					nDestroyed += pBuildingGrid->IsDestroyed( p + SPoint3(0,0,2) );
					nDestroyed += pBuildingGrid->IsDestroyed( p + SPoint3(0,0,3) );
				}
			}
		}
	if ( !nTotal )
		return;
	float fScale = (float)nDestroyed / nTotal;
	value = fScale * pGlobalAmbient->GetValue() + (1 - fScale) * pLocalAmbient->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x02741120, CBuilding );
REGISTER_SAVELOAD_CLASS( 0x02741132, CBuildInfoLoader );
REGISTER_SAVELOAD_CLASS( 0x013c1180, CLightCalcer );
REGISTER_SAVELOAD_CLASS( 0x013c1181, CLocalAmbientCalcer );
