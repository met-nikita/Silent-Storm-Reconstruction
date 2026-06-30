#include "StdAfx.h"
#include "..\Main\aiWaypoint.h"
#include "..\Main\BuildingInfo.h"
#include "MESerialize.h"
#include "MEUserSettings.h"
#include "..\Main\METerrain.h"
#include "..\MapEdit\ItemsMgr.h"
#include "..\MapEdit\dbDefs.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"

extern string GetExportDstDir();
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SerializeWaypoint( NAI::CWaypoint *pW, int nWaypointID )
{
	if ( !IsValid( pW ) )
		return false;
	try
	{
		char szPath[MAX_PATH];
		sprintf( szPath, "%sWaypoints\\%d", GetExportDstDir().c_str(), nWaypointID );
		CFileStream fp;
		fp.OpenWrite( szPath );
		{
			CStructureSaver file( fp, CStructureSaver::WRITE );
			pW->operator&( file ) ;
		}
		return true;
	}
	catch ( ... )
	{
		return false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CheckRoute( NAI::SRoute *pRoute )
{
	if ( !pRoute )
	{
		ASSERT(0);
		return;
	}
	CItemsMgr *pMgr = GetUserSettings().GetResourceManager( IDC_WAYPOINTNAMES_TREE );
	if ( !pMgr )
		return;
	for ( vector<int>::iterator i = pRoute->waypoints.begin(); i != pRoute->waypoints.end(); )
	{
		if ( !pMgr->IsExist( *i ) )
			i = pRoute->waypoints.erase( i );
		else
			++i;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool SaveRoute( const string &szType, NAI::CUnitAIInfo *pU, int nID )
{
	if ( !IsValid( pU ) )
		return false;
	for ( int i = 0; i < pU->routes.size(); ++i )
		CheckRoute( &pU->routes[i] );
	try
	{
		char szPath[MAX_PATH];
		sprintf( szPath, "%s%s\\%d", GetExportDstDir().c_str(), szType.c_str(), nID );
		CFileStream fp;
		fp.OpenWrite( szPath );
		{
			CStructureSaver file( fp, CStructureSaver::WRITE );
			pU->operator&( file ) ;
		}
		return true;
	}
	catch ( ... )
	{
		return false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SerializeUnitAIInfo( NAI::CUnitAIInfo *pU, int nUnitID )
{
	return SaveRoute( "Units", pU, nUnitID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SerializeUnitGroupAIInfo( NAI::CUnitAIInfo *pU, int nGroupID )
{
	return SaveRoute( "Groups", pU, nGroupID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsFragmentValid( const NBuilding::SBuildFragment &fr, NBuilding::CBuildInfo *pInfo )
{
	if ( fr.nConstructionPartID <= 0 || NDb::GetTConstructionPart( fr.nConstructionPartID ) == 0 )
		return false;
	if ( fr.ptPos.z > pInfo->nMaxFloor )
		pInfo->nMaxFloor = fr.ptPos.z;
	if ( fr.ptPos.z < pInfo->nMinFloor )
		pInfo->nMinFloor = fr.ptPos.z;
	return fr.ptPos.x >= 0 && fr.ptPos.y >= 0 && fr.ptPos.x <= pInfo->nMaxX && fr.ptPos.y <= pInfo->nMaxY;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CheckFragments( vector<NBuilding::SBuildFragment> *pFrags, NBuilding::CBuildInfo *pInfo )
{
	int nMaxID = -1;
	for ( vector<NBuilding::SBuildFragment>::iterator i = pFrags->begin(); i != pFrags->end(); )
		if ( !IsFragmentValid( *i, pInfo ) )
			i = pFrags->erase( i );
		else
		{
			if ( i->nID <= 0 )
				i->nID = pInfo->CreateNextFragmentID();
			nMaxID = Max( nMaxID, i->nID );
			for ( int j = 0; j < NDb::N_CONSTRUCTION_MATERIALS; ++j )
			{
				NBuilding::SRawMixedMaterial &r = i->materials[j];
				for ( vector<NBuilding::SRawMaterialApply>::iterator im = r.layers.begin(); im != r.layers.end(); )
				{
					if ( !NDb::GetTMaterial( im->nTMaterialID ) )
						im = r.layers.erase( im );
					else
						++im;
				}
			}
			++i;
		}
	if ( pInfo->nMaxFragmentID <= nMaxID )
	{
		ASSERT( 0 );
		pInfo->nMaxFragmentID = nMaxID + 1;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CheckSpots( vector<NBuilding::SProjectedSpot> *pSpots )
{
	for ( vector<NBuilding::SProjectedSpot>::iterator i = pSpots->begin(); i != pSpots->end(); )
	{
		NDb::CTMaterial *pM = NDb::GetTMaterial( i->nMaterialID );
		if ( !IsValid( pM ) )
			i = pSpots->erase( i );
		else
			++i;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CheckLayerGroups( vector<NBuilding::SLayerGroup> *pGroups )
{
	for ( vector<NBuilding::SLayerGroup>::iterator i = pGroups->begin(); i != pGroups->end(); )
		if ( i->layers.empty() || (i->layers.size() == 1 && i->floor.empty() ) )
			i = pGroups->erase( i );
		else
			++i;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void FixRoomMapSize( NBuilding::CBuildInfo *pInfo )
{
	const int nFloors = pInfo->nMaxFloor - pInfo->nMinFloor + 1;
	CTPoint<int> rsize( pInfo->nMaxX + 2, pInfo->nMaxY + 2 );  // добавляем по краям полоску в 1 тайл
	CArray2D<BYTE> patern( rsize.x, rsize.y );
	patern.FillZero();
	if ( pInfo->roomMap.size() != nFloors )
	{
		pInfo->roomMap.clear();
		pInfo->roomMap.resize( nFloors, patern );
	}
	for ( int i = 0; i < nFloors; ++i )
	{
		if ( pInfo->roomMap[i].GetXSize() != rsize.x || pInfo->roomMap[i].GetYSize() != rsize.y )
		{
			pInfo->roomMap[i].SetSizes( rsize.x, rsize.y );
			pInfo->roomMap[i].FillEvery( 0 );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void FixCellarSize( NBuilding::CBuildInfo *pInfo, int nX, int nY )
{
	int sx = pInfo->cellar.GetXSize();
	int sy = pInfo->cellar.GetYSize();
	if ( sx != nX || sy != nY )
	{
		CArray2D<bool> a( pInfo->cellar );
		pInfo->cellar.SetSizes( nX, nY );
		pInfo->cellar.FillEvery( 0 );
		sx = Min( sx, nX );
		sy = Min( sy, nY );
		for ( int x = 0; x < sx; ++x )
			for ( int y = 0; y < sy; ++y )
				pInfo->cellar[y][x] = a[y][x];
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CheckLadders( vector<NBuilding::SLadder> *pLadders )
{
	for ( vector<NBuilding::SLadder>::iterator i = pLadders->begin(); i != pLadders->end(); )
	{
		if ( i->nID <= 0 )
			i = pLadders->erase( i );
		else
			++i;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SerializeBuilding( NBuilding::CBuildInfo *pInfo, int nBuildingID )
{
	if ( !pInfo )
		return false;
	//
	NDb::CTemplVariant *pVar = NDb::GetTemplVariant( nBuildingID );
	if ( !pVar )
		return false;
	int nWidth = pInfo->nMaxX;
	int nHeight = pInfo->nMaxY;
	if ( IsValid( pVar->pTemplate ) )
	{
		nHeight = pVar->pTemplate->nHeight;
		nWidth = pVar->pTemplate->nWidth;
	}
	pInfo->nMinFloor = 0;
	pInfo->nMaxFloor = 0;
	if ( pInfo->solidFragments.empty() && pInfo->wallFragments.empty() )
	{
		pInfo->nMaxY = 0;
		pInfo->nMaxX = 0;
	}
	else
	{
		pInfo->nMaxY = nHeight;
		pInfo->nMaxX = nWidth;
	}
	// проверяем параметры фрагментов
	CheckFragments( &pInfo->wallFragments, pInfo );
	CheckFragments( &pInfo->solidFragments, pInfo );
	CheckSpots( &pInfo->spots );
	CheckLayerGroups( &pInfo->lgroups );
	CheckLadders( &pInfo->ladders );
	//
	FixRoomMapSize( pInfo );
	FixCellarSize( pInfo, pVar->pTemplate->nWidth, pVar->pTemplate->nHeight );
	//
	try
	{
		char szPath[MAX_PATH];
		sprintf( szPath, "%sBuildings\\%d", GetExportDstDir().c_str(), nBuildingID );
		CFileStream fp;
		fp.OpenWrite( szPath );
		{
			CStructureSaver file( fp, CStructureSaver::WRITE );
			pInfo->operator&( file ) ;
		}
		return true;
	}
	catch ( ... )
	{
		return false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool DeleteWaypoint( int nID )
{
	char szPath[MAX_PATH];
	sprintf( szPath, "%sWaypoints\\%d", GetExportDstDir().c_str(), nID );
	return DeleteFile( szPath );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SerializeTerrain( CMETerrainInfo *pTerr, int nTerrID )
{
	if ( !pTerr )
		return false;
	try
	{
		char szPath[MAX_PATH];
		sprintf( szPath, "%sTerrain\\%d", GetExportDstDir().c_str(), nTerrID );
		CFileStream fp;
		fp.OpenWrite( szPath );
		{
			CStructureSaver file( fp, CStructureSaver::WRITE );
			pTerr->operator&( file ) ;
		}
		return true;
	}
	catch ( ... )
	{
		return false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool DeleteTerrain( int nID )
{
	char szPath[MAX_PATH];
	sprintf( szPath, "%sTerrain\\%d", GetExportDstDir().c_str(), nID );
	return DeleteFile( szPath );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
