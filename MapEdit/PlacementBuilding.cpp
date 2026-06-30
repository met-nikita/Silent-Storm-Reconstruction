#include "StdAfx.h"
#include "ItemsMgr.h"
#include "Placement.h"
#include "PlacementDefs.h"
#include "floor.h"
#include "MapEdit.h"
#include "Walls.h"
#include "dbDefs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeBuildingInfo( const CPlacement *pPl, NBuilding::CBuildInfo *pInfo )
{
	const SResTree *pRes = theApp.GetResTree( IDC_CONSTRUCTIONPARTS_TREE );
	if ( !pRes || !pRes->pItemsTree )
		return;
	CItemsMgr *pItems = pRes->pItemsTree;
	int nFragmentID = 0;
	// заносим все имеющиеся строительные блоки в pIfno
	pInfo->wallFragments.clear();
	pInfo->solidFragments.clear();
	pInfo->nMaxFloor = MIN_FLOOR;
	pInfo->nMinFloor = MAX_FLOOR;
	pInfo->nMaxY = pPl->GetHeight();
	pInfo->nMaxX = pPl->GetWidth();

	for ( int nf = MIN_FLOOR; nf < MAX_FLOOR; ++nf )
	{
		for ( int ftype = FT_FLOOR; ftype < FT_ROOM; ++ftype )
		{
			const int nLayers = pPl->GetFloorLayersNum( (EFloorType)ftype );
			for ( int nl = 0; nl < nLayers; ++nl )
			{
				const CFloorPlan *pPlan = pPl->GetFloor( (EFloorType)ftype, nf, nl );
				if ( !pPlan )
					continue;
				const float fFloor = FT_FLOOR_INTERMEDIATE == ftype || FT_SOLID_INTERMEDIATE == ftype ? nf + 0.5f : nf;
				const CFloorPlan::CCellInfo* pcells = pPlan->GetFloor();
				for ( CFloorPlan::CCellInfo::const_iterator it = pcells->begin(); it != pcells->end(); ++it )
				{
					int nModelID = it->first;
					const CPropMap *pProps = pItems->GetPropList( nModelID );
					if ( !pProps )
						continue;
					CPropMap::const_iterator cit = pProps->find( "SizeZ" );
					if ( cit == pProps->end() )
						continue;
					pInfo->nMinFloor = Min( pInfo->nMinFloor, nf );
					const int nSizeZ = int( cit->second->GetValue() );
					for ( int i = 0; i < it->second.size(); ++i )
					{
						const CFloorPlan::SCell &cell = it->second[i];
						if ( cell.x < 0 )
							continue;
						NBuilding::SBuildFragment fr;
						fr.nConstructionPartID = nModelID;
						fr.ptPos = CVec3( cell.x, cell.y, fFloor );
						fr.nRotationID = cell.nRotationID;
						for ( int j = 0; j < NDb::N_CONSTRUCTION_MATERIALS; ++j )
							fr.materials[j] = cell.materials[j];
						fr.nFragmentID = NBuilding::MakeFragmentID( ELayer( ftype ), nl );
						pInfo->solidFragments.push_back( fr );
						pInfo->nMaxFloor = Max( pInfo->nMaxFloor, int(fFloor + 0.5f) + nSizeZ );
					}
					pItems->ReleasePropList( pProps );
				}
			}
		}
		//
		const CWallsPlan *pFloor = pPl->GetFloorWalls( nf );
		if ( pFloor )
		{
			pFloor->FillWallFragments( pInfo );
			//PlaceWalls( pInfo, pFloor );
		}
	}
	//
	for ( int i = 0; i < pInfo->wallFragments.size(); ++i )
	{
		const int nf = pInfo->wallFragments[i].ptPos.z;
		pInfo->nMaxFloor = Max( pInfo->nMaxFloor, nf );
		pInfo->nMinFloor = Min( pInfo->nMinFloor, nf );
	}
	if ( pInfo->nMaxFloor == MIN_FLOOR && pInfo->nMinFloor == MAX_FLOOR || pInfo->nMaxFloor < pInfo->nMinFloor )
		pInfo->nMaxFloor = pInfo->nMinFloor = 0;
	//
	int i, j;
	const int nFloors = pInfo->nMaxFloor - pInfo->nMinFloor + 1;
	if ( pInfo->roomMap.size() != nFloors )
		pInfo->roomMap.resize( nFloors );
	CTPoint<int> rsize( pInfo->nMaxX + 2, pInfo->nMaxY + 2 );  // добавляем по краям полоску в 1 тайл
	for ( i = pInfo->nMinFloor, j = 0; i < pInfo->nMaxFloor + 1; ++i, ++j )
	{
		CArray2D<BYTE> &rooms = pInfo->roomMap[j];
		if ( rooms.GetXSize() < rsize.x || rooms.GetYSize() < rsize.y )
			rooms.SetSizes( rsize.x, rsize.y );
		rooms.FillZero();
		const CFloorPlan *pRooms = pPl->GetFloor( FT_ROOM, i, 0 );
		if ( !pRooms )
			continue;
		const CFloorPlan::CCellInfo* pcells = pRooms->GetFloor();
		for ( CFloorPlan::CCellInfo::const_iterator it = pcells->begin(); it != pcells->end(); ++it )
		{
			for ( int k = 0; k < it->second.size(); ++k )
			{
				const CFloorPlan::SCell &cell = it->second[k];
				if ( cell.x >= 0)
					rooms[cell.y + 1][cell.x + 1] = it->first;
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
