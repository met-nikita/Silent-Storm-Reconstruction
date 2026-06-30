#include "StdAfx.h"
#include "MakeBuilding.h"
#include "BuildingInfo.h"
#include "BuildingGrid.h"
#include <limits>
#include "Grid.h"
#include "Transform.h"
#include "..\Misc\2Darray.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataRPG.h"
#include "BuildingSchema.h"
#include "..\Misc\BasicShare.h"
#include "GGeometry.h"
#include "aiobject.h"
#include "aiobjectloader.h"
#include "..\Misc\HPTimer.h"
#include "MELayers.h"
#include "..\MiscDll\LogStream.h"
#include "BSPTree.h"
#include "MakeBuildingInternal.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
	externA5 CBasicShare<int, NAI::CLoadGeometryInfo> shareAIModel;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	externA5 CBasicShare<int, NBuilding::CBuildInfoLoader> shareBuildings;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBuilding
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int   FLOOR_MAX_SIZE = 1;
const int   WALL_MAX_LEN_2 = WALL_MAX_LEN + 2;
////////////////////////////////////////////////////////////////////////////////////////////////////
using std::numeric_limits;
typedef numeric_limits<int> LIM_INT;
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
float ID2Width( int nWidthID )
{
	switch( nWidthID )
	{
		case WALL_THIN_ID:
			return WALL_THIN;
			break;
		case WALL_MED_ID:
			return WALL_MED;
			break;
	}	
	return WALL_THICK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int Width2ID( float fWidth )
{
	if ( fabs( fWidth - WALL_THIN ) < FP_EPSILON )
		return WALL_THIN_ID;
	if ( fabs( fWidth - WALL_MED ) < FP_EPSILON )
		return WALL_MED_ID;
	return WALL_THICK_ID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool topLookupTbl[4][7] = 
{
	{1, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 1, 0, 0},
	{0, 0, 0, 0, 1, 0, 0},
	{0, 0, 0, 0, 1, 0, 0}
};
// true, если для стенки iThis входящая стенка (определяемая по delta: i = iThis - delta)
// перпендикулярна и расположена в положительной части Y плоскости
// Соответсвие между индексом и направлением:
// 0-left, 1-top, 2-right, 3-bottom
static inline bool GetBTop( int iThis, int delta )
{
	return topLookupTbl[iThis][3 + delta];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// bit map для клип ID:
// 1st b   - bPerpendicular
// 2-3 b   - nWidth (incom width)
// 4 b     - bTop
// 5-8 b   - cull sides
static inline short MakeClipID( int nWidth, bool bPerpendicular, bool bTop, short cullSides )
{
	return (cullSides & 0xf) << 4 | (int)bTop << 3 | nWidth << 1 | (int)bPerpendicular;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline short MakeCullID( const vector<char> &hideSides )
{
	ASSERT( 4 == hideSides.size() );

	return int( hideSides[0] ) | int( hideSides[1] << 1 ) |
		int( hideSides[2] << 2 ) | int( hideSides[3] << 3 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static short GetWallClipInfo( const CNodeMap<SGridNode> &grid, 
	const SBuildFragment &fr, 
	const NDb::CConstructionPart *pCP, 
	const SGridNode *pNode )
{
	if ( !pNode )
		return 0;
	int   iThis  = -1;
	int   iThick = -1;
	SGridElement geThis, geThick;
	vector<pair<int, float> > indwidth;
	int nClipGroup = pCP->nClipGroup;
	float fThickness = pCP->fThickness;

	{
		// находим iThis
		for ( int i = 0; i < 4 && iThis == -1; ++i )
		{
			for ( int j = 0; j < pNode->elems[i].size(); ++j )
			{
				SBuildFragment *pFr = pNode->elems[i][j].pFragment;
				if ( &fr == pFr )
				{
					iThis = i;
					geThis = pNode->elems[i][j];
					indwidth.push_back( pair<int, float>( i, geThis.fThickness ) );
					break;
				}
			}
		}
		// Находим самую толстую стенку в узле
		float fThick = 0;
		for ( int i = 0; i < 4; ++i )
		{
			if ( i == iThis || pNode->elems[i].empty() )
				continue;
			float fW = 0;
			bool bIn = false;
			for ( int j = 0; j < pNode->elems[i].size(); ++j )
			{
				if ( !pNode->elems[i][j].pFragment )
					continue;
				SGridElement e = pNode->elems[i][j];
				SBuildFragment *pFr = e.pFragment;
				fW = Max( fW, e.fThickness );
				bIn = bIn || (nClipGroup == e.nClipGroup);
				if ( e.fThickness > fThick && nClipGroup == e.nClipGroup )
				{
					iThick = i;
					fThick = e.fThickness;
					geThick = e;
				}
			}
			if ( bIn )
				indwidth.push_back( pair<int, float>( i, fW ) );
		}
	}
	if ( -1 == iThis )
		return 0;
	// ASSERT( -1 != iThis );
	//
	vector<char> hideSides( 4, false ); 
	const int iLEFT   = 0; // индексы сторон в массиве hideSides
	const int iRIGHT  = 1;
	const int iTOP    = 2;
	const int iBOTTOM = 3;
	//
	if ( -1 == iThick )
		return MakeClipID( 0, 0, 0, MakeCullID( hideSides ) );;
	//
	int nInWidth = Width2ID( geThick.fThickness );
	int nWidth = Width2ID( fThickness );
	// пересечение с длинной стенкой ?
	if ( INTERNAL == geThick.side )
		return MakeClipID( nInWidth, 0, 0, MakeCullID( hideSides ) );	
	// Возможно это особый случай - стыкуются более 2х стенок одинаковой толщины, надо проверить
	if ( nInWidth <= nWidth && indwidth.size() > 2 )
	{
		int k, cnt = 0;
		const float fw = fThickness;
		for ( k = 0; k < indwidth.size(); ++k )
			if ( fw == indwidth[k].second )
				++cnt;
		if ( cnt > 2 )
		{
			int iOpposite = (iThis+2) % 4;
			if ( !pNode->elems[iOpposite].empty() )
			{
				// так и есть! это особый случай
				const SGridElement &eopposite = pNode->elems[iOpposite].front();
				if ( eopposite.pFragment && nClipGroup == eopposite.nClipGroup )
				{
					if ( LEFT == geThis.side )
						hideSides[iLEFT] = true;
					else
						hideSides[iRIGHT] = true;
				}
			}
			short cullId = MakeCullID( hideSides );
			int nInW = Width2ID( fw );
			//
			return MakeClipID( nInW, 0, 0, cullId );
		}
	}
	//
	bool bTop = false;
	bool bPerpendicular = false;
	if ( (iThis - iThick) & 0x1 )
	{
		bPerpendicular = true;
		bTop = GetBTop( iThis, iThis - iThick );
	}
	else
	{
		bPerpendicular = false;
		if ( nInWidth == Width2ID( fThickness ) )
		{
			// в этом случае надо выбрать какую из 2х стенок клипать
			
			if ( geThis.side == geThick.side )
			{
				///if ( 0 == iThis || 1 == iThis )
					//nInWidth = 0;
			}
			else if ( LEFT == geThis.side )
			{
				hideSides[iLEFT] = true;
				//nInWidth = 0;
			}
			else
				hideSides[iRIGHT] = true;
		}
	}
	if ( nInWidth < nWidth )
		nInWidth = 0;
	return MakeClipID( nInWidth, bPerpendicular, bTop, MakeCullID( hideSides ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_SMALL_VOLUME = 0.01f;
int FixSmallPieceID( const NAI::CGeometryInfo::CPieceMap &pieces, int nPieceID )
{
	return nPieceID;

	NAI::CGeometryInfo::CPieceMap::const_iterator it = pieces.find( nPieceID );
	if ( it == pieces.end() )
	{
//		ASSERT(0);
		return nPieceID;
	}
	const NAI::CGeometryInfo::SPiece &pc = it->second;
	if ( pc.fVolume > F_SMALL_VOLUME )
		return nPieceID;

	int x, y, z;
	int partx, party, partz;
	GetPieceCoords( nPieceID, &x, &y, &z );
	GetPartCoords( nPieceID, &partx, &party, &partz );
	int nPartID = GetPartHashID( partx, party, partz );

	float fMinVol = 1e30f;
	int nMinDist = 1e6f;
	int nID = -1;
	for ( int dx = -1; dx <= 1; ++dx )
		for ( int dy = -1; dy <= 1; ++dy )
			for ( int dz = -1; dz <= 1; ++dz )
			{
				it = pieces.find( nPartID | GetPieceHashID( x+dx, y+dy, z+dz ) );
				if ( it != pieces.end() )
				{
					const float fVol = it->second.fVolume;
					if ( fVol > F_SMALL_VOLUME )
					{
						const int nDist = dx * dx + dy * dy + dz * dz;
						if ( nDist < nMinDist || ( nDist == nMinDist && fVol < fMinVol ) )
						{
							fMinVol = fVol;
							nID = it->first;
							nMinDist = nDist;
						}
					}
				}
			}

	if ( nID != -1 )
		return nID;
	return nPieceID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline SPoint3 GridCoords( const CVec3 &ptPos, int nRotationID, int x, int y, int z )
{
	SDiscretePos dpos( 0, VNULL3, nRotationID );
	CVec3 pt( x, y, z );
	dpos.MoveAndRotate( &pt );
	pt += CVec3( ptPos.x, ptPos.y, 0 );
	return SPoint3( pt );
}
static bool SetVisibility( DWORD *pVisible, const CBuildingGrid &grid, 
													const CVec3 &ptPos, int nRotationID, const SPoint3 &pt )
{
	bool bRes = !grid.IsDestroyed( GridCoords( ptPos, nRotationID, pt.x, pt.y, Float2Int( ptPos.z * 4 + pt.z ) ) );
	if ( bRes )
		*pVisible |= GetPartBit( pt.x, pt.y, pt.z );
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Прописывание в clipinfo видимых частей сплошного объекта на основе данных в pBuildingGrid
static void SetSolidVisibleParts( SClipInfo *pClip, CVec3 ptPos, const CBuildingGrid &grid, bool bParts, NAI::CGeometryInfo *pGI )
{
	DWORD nVisible = 0;
	bool bAllPieces = true;
	if ( !pGI )
	{
		for ( int z = 0; z <= 4; ++z )
		{
			if ( !(z & 0x1) )
			{
				bAllPieces &= SetVisibility( &nVisible, grid, ptPos, pClip->nRotationID, SPoint3( 1, 0, z ) );
				bAllPieces &= SetVisibility( &nVisible, grid, ptPos, pClip->nRotationID, SPoint3( 0, 1, z ) );
				bAllPieces &= SetVisibility( &nVisible, grid, ptPos, pClip->nRotationID, SPoint3( 2, 1, z ) );
				bAllPieces &= SetVisibility( &nVisible, grid, ptPos, pClip->nRotationID, SPoint3( 1, 2, z ) );
			}
			else
			{
				bAllPieces &= SetVisibility( &nVisible, grid, ptPos, pClip->nRotationID, SPoint3( 0, 0, z ) );
				bAllPieces &= SetVisibility( &nVisible, grid, ptPos, pClip->nRotationID, SPoint3( 2, 0, z ) );
				bAllPieces &= SetVisibility( &nVisible, grid, ptPos, pClip->nRotationID, SPoint3( 1, 1, z ) );
				bAllPieces &= SetVisibility( &nVisible, grid, ptPos, pClip->nRotationID, SPoint3( 0, 2, z ) );
				bAllPieces &= SetVisibility( &nVisible, grid, ptPos, pClip->nRotationID, SPoint3( 2, 2, z ) );
			}
		}
	}
	else
	{
		for ( NAI::CGeometryInfo::CPieceMap::const_iterator i = pGI->pieces.begin(); i != pGI->pieces.end(); ++i )
		{
			int nPieceID = FixSmallPieceID( pGI->pieces, i->first );
			int x, y, z;
			GetPartCoords( nPieceID, &x, &y, &z );
			if ( GetPartHashID( x, y, z ) != pClip->nSubBlockID )
				continue;
			GetPieceCoords( nPieceID, &x, &y, &z );
			if ( GetPieceHashID( x,y,z) == 0 )
				continue;
			bAllPieces &= SetVisibility( &nVisible, grid, ptPos, pClip->nRotationID, SPoint3( x-1, y-1, z-1 ) );
		}
	}
	// если все куски видны, рисуем блок как цельный
	if ( bAllPieces && !bParts )
		nVisible = UNBROKEN_BLOCK32;
	pClip->dwParts = nVisible;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMixedMaterial* CreatePartMaterial( SRand *pRand, const SRawMixedMaterial &src, NDb::CMaterial *pDefMaterial )
{
	CMixedMaterial *pRet = src.CreateMixedMaterial( pRand );
	if ( pRet )
		return pRet;
	pRet = new CMixedMaterial;
	SMaterialApply a;
	a.mapping   = SMaterialApply::NORMAL;
	a.fScale    = 1;
	a.ptShift   = VNULL2;
	a.nRotation = 0;
	if ( pDefMaterial )
		a.pMaterial = pDefMaterial;
	else
		a.pMaterial = NDb::GetMaterial( NDb::N_DEF_MATERIAL_ID );
	pRet->layers.push_back( a );
	return pRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddFragment( vector<SStoreyInfo::SFragment> *pRes, const CBuildingGrid &grid, 
	const SClipInfo &_clip, const SBuildFragment &fr, const NDb::CConstructionPart *pCP, 
	bool bWall, bool bParts, int nFragID, const vector<SProjectedSpot> &spots, SRand *pRand )
{
	if ( !IsValid( pCP->pGeometry ) )
		return;
	NAI::CGeometryInfo *pGI = 0;
	if ( IsValid( pCP->pGeometry->pAIGeometry ) )
	{
		CDGPtr< CPtrFuncBase<NAI::CGeometryInfo> > pSrc = NAI::shareAIModel.Get( pCP->pGeometry->pAIGeometry->GetRecordID() );
		pSrc.Refresh();
		pGI = pSrc->GetValue();
	}
	SClipInfo clip(_clip);
	SetSolidVisibleParts( &clip, clip.ptPos, grid, bParts, pGI );
	//
	for ( int i = 0; i < NDb::N_FIRST_GEOMMATERIALS; ++i )
		clip.pMaterials[i] = CreatePartMaterial( pRand, fr.materials[i], pCP->pDefMaterials[i] );
	vector<SStoreyInfo::SSpot> &fspots = pRes->insert( pRes->end(), SStoreyInfo::SFragment( clip, nFragID, pCP->pArmor ) )->spots;
	//for ( int i = 0; i < spots.size(); ++i )
	for ( int i = 0; i < fr.spots.size(); ++i )
	{
		for ( int j = 0 ; j < spots.size(); ++j )
			if ( spots[j].nID == fr.spots[i] )
			{
				const SProjectedSpot &s = spots[j];
				NDb::CTMaterial *pM = NDb::GetTMaterial( s.nMaterialID );
				if ( pM )
					fspots.push_back( SStoreyInfo::SSpot( s.ptOrigin, s.ptNormal, s.ptSize, s.nRotation, pM->GetMaterial( pRand ), s.nMaterialMask ) );
			}
	}

	if ( IsValid( pCP->p2ndGeometry ) )
	{
		clip.pGeometry = pCP->p2ndGeometry;
		clip.nClip = 0;
		for ( int i = 0; i < NDb::N_MODEL_MATERIALS; ++i )
			clip.pMaterials[i] = 0;
		for ( int i = 0; i < NDb::N_SECOND_GEOMMATERIALS; ++i )
		{
			int nMIndex = i + NDb::N_FIRST_GEOMMATERIALS;
			clip.pMaterials[i] = CreatePartMaterial( pRand, fr.materials[nMIndex], pCP->pDefMaterials[nMIndex] );
		}
		pRes->push_back( SStoreyInfo::SFragment( clip, 0x20000000 | nFragID, pCP->pArmor  ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline SPart Point2Part( const CVec3 &pt, int nRotationID )
{
	SDiscretePos dpos( 0, pt, nRotationID );

	CVec3 dpt(1, 0, 0);
	dpos.MoveAndRotate( &dpt );
	return SPart( floor( pt.z ), dpt.x / 2, dpt.y / 2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline SPart Point2PartSolid( const CVec3 &pt, int nRotationID )
{
	SDiscretePos dpos( 0, pt, nRotationID );

	CVec3 dpt(1, 1, 0);
	dpos.MoveAndRotate( &dpt );
	return SPart( floor( pt.z ), dpt.x / 2, dpt.y / 2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool operator==( const SProjectedSpot &a, const SProjectedSpot &b )
{
	return a.nMaterialID == b.nMaterialID && a.nRotation == b.nRotation && a.ptOrigin == b.ptOrigin && 
		a.ptNormal == b.ptNormal && a.ptSize == b.ptSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void MakeBuilding( SBuildingInfo *pInfo, const CBuildingGrid &grid, CBuildInfo *pBuildInfo, 
	CSolidAndWallMap *_pSWMap, bool bParts, unordered_map<SPart, bool, SPart> *pSelect = 0 )
{
	ASSERT( pInfo && pBuildInfo );
	CDGPtr<CSolidAndWallMap> pSWMap( _pSWMap );
	pSWMap.Refresh();
	const CSolidAndWallMap &swMap = *_pSWMap;
	SRand rand(0), randStart( grid.GetSeed() );
	if ( pSelect )
	{
		for (unordered_map<SPart, bool, SPart>::iterator i = pSelect->begin(); i != pSelect->end(); ++i )
			pInfo->Erase( i->first );
	}
	else
		pInfo->Clear();
	const int nCutFloor = grid.GetCutFloor();

  // Сплошные объекты
	const unordered_map<int, CNodeMap<SSolidElement> > &solids = swMap.GetSolidMap();
	for (unordered_map<int, CNodeMap<SSolidElement> >::const_iterator it = solids.begin(); it != solids.end(); ++it )
	{
		const vector<SSolidElement> &frags = it->second.GetValues();
		for ( int i = 0; i < frags.size(); ++i )
		{
			const SSolidElement &e = frags[i];
			for ( int j = 0; j < e.fragments.size(); ++j )
			{
				const SFragmentPos &frp = e.fragments[j];
				if ( !frp.pFr || floor(frp.pFr->ptPos.z) > nCutFloor || !grid.IsLayerVisible( frp.pFr->nFragmentID ) )
					continue;
				SClipInfo clip;
				clip.ptPos = frp.nSubPos;
				clip.pGeometry  = frp.pCPart->pGeometry;
				clip.nRotationID  = frp.pFr->nRotationID;
				clip.nSubBlockID  = frp.nHashID;
				//clip.nRooms[0] = clip.nRooms[1] = GetSolidRoomID( clip, pBuildInfo ); // целиком находится в одной комнате
				//SStoreyInfo &storey = pInfo->GetStorey( floor( clip.ptPos.z ) );
				rand.seed.nSeed = randStart.Get( 0x6000 ) + randStart.Get( 0x6000 ) * 0x6000;
				const SPart &part = Point2PartSolid( clip.ptPos, clip.nRotationID );
				if ( pSelect == 0 || pSelect->find( part ) != pSelect->end() )
				{
					SStoreyInfo &storey = pInfo->GetPart( part );
					AddFragment( &storey.fragments, grid, clip, *frp.pFr, frp.pCPart, false, bParts, frp.nIndex, pBuildInfo->spots, &rand );
				}
			}
		}
	}
	// Стены
	int nWallLayer = MakeFragmentID( LID_WALLS, 0 );
	if ( !grid.IsLayerVisible( nWallLayer ) )
		return;
	const CNodeMap<SGridNode> &wallGrid = swMap.GetWallGrid();
	const vector<SLRNeighbs> &neighbs = swMap.GetNeighbs();

	for ( int i = 0; i < pBuildInfo->wallFragments.size(); ++i )
	{
		const SBuildFragment &fr = pBuildInfo->wallFragments[i];
		if ( neighbs.size() <= i )
		{
			ASSERT(0);
			break;
		}
		NDb::CConstructionPart *pCP = neighbs[i].pCPart;
		if ( !IsValid( pCP ) || !IsValid( pCP->pGeometry ) || floor(fr.ptPos.z) > nCutFloor )
			continue;

		SClipInfo clip;
		clip.ptPos = fr.ptPos;
		clip.pGeometry  = pCP->pGeometry;
		clip.nRotationID  = fr.nRotationID;
		clip.nSubBlockID  = fr.nSubBlockID;
		clip.nClip  = ((2 << 8) + Width2ID( pCP->fThickness )) << 16;
		clip.nClip += GetWallClipInfo( wallGrid, fr, pCP, neighbs[i].pLeft ) << 8; // левый край
		clip.nClip += GetWallClipInfo( wallGrid, fr, pCP, neighbs[i].pRight ); // правый край
		//clip.nRooms[0] = 0;//GetWallRoomID( 0, fr, pBuildInfo );
		//clip.nRooms[1] = 0;//GetWallRoomID( 1, fr, pBuildInfo );
		//SStoreyInfo &storey = pInfo->GetStorey( clip.ptPos.z );
		rand.seed.nSeed = randStart.Get( 0x6000 ) + randStart.Get( 0x6000 ) * 0x6000;
		const SPart &part = Point2Part( clip.ptPos, clip.nRotationID );
		if ( pSelect == 0 || pSelect->find( part ) != pSelect->end() )
		{
			SStoreyInfo &storey = pInfo->GetPart( part );
			AddFragment( &storey.walls, grid, clip, fr, pCP, true, bParts, i, pBuildInfo->spots, &rand );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void MarkCellarWalls( CArray2D<bool> *pWalls, const CArray2D<bool> &cellar, const CArray2D<float> &bottom )
{
	ASSERT( pWalls );
	pWalls->SetSizes( cellar.GetXSize() + 2, cellar.GetYSize() + 2 );
	pWalls->FillEvery( false );
	CArray2D<bool> &walls = *pWalls;
	const int nX = Min( cellar.GetXSize(), bottom.GetXSize() );
	const int nY = Min( cellar.GetYSize(), bottom.GetYSize() );
	for ( int x = 0; x < nX; ++x )
	{
		walls[0][x] = cellar[0][x];
		walls[nY][x] = cellar[nY-1][x];
	}
	for ( int y = 0; y < nY; ++y )
	{
		walls[y][0] = cellar[y][0];
		walls[y][nX] = cellar[y][nX-1];
	}
	for ( int y = 1; y < nY; ++y )
		for ( int x = 1; x < nX; ++x )
		{
			const bool val = cellar[y][x];
			//bool b1 = x < nX - 1 ? val != cellar[y][x+1] : false;
			//bool b2 = y < nY - 1 ? val != cellar[y+1][x] : false;
			walls[y][x] = val != cellar[y][x-1] || val != cellar[y-1][x] || val != cellar[y-1][x-1];// || b1 || b2;
			//
			const int nx = x;
			const int ny = y;
			const float fval = bottom[ny][nx];
			const float fval0_1 = bottom[ny][nx-1];
			const float fval_10 = bottom[ny-1][nx];
			const float fval_1_1 = bottom[ny-1][nx-1];
			bool b = fval != fval0_1 || fval != fval_10 || fval != fval_1_1;
			//ASSERT( b == walls[y][x] );
			walls[y][x] |= b;
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsCellarWall( const CVec3 &pt, const CArray2D<bool> &cellarWalls )
{
	if ( pt.z > 0 || pt.x < 0 || pt.x >= cellarWalls.GetXSize() || pt.y < 0 || pt.y >= cellarWalls.GetYSize() )
		return false;
	return cellarWalls[(int)pt.y][(int)pt.x];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsGround( const CVec3 &pt )
{
	return pt.z <= 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void JuncsMoveRotate( const SDiscretePos &dpos, vector<SJunction> *pJuncs )
{
	for ( int i = 0; i < pJuncs->size(); ++i )
		dpos.MoveAndRotate( &(*pJuncs)[i].pt );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void AddPieces( CBuildingSchema *pSchema, const vector<int> &parts, const vector<int> &additionparts, NDb::CRPGArmor *pAr, int nAIGeomID, 
	const CArray2D<bool> &cellarWalls, bool bGround, const SDiscretePos &dpos, int nSubPartID )
{
	// определяем, какие узлы присутсвуют в блоке
	CDGPtr< CPtrFuncBase<NAI::CGeometryInfo> > pSrc = NAI::shareAIModel.Get( nAIGeomID );
	pSrc.Refresh();
	NAI::CGeometryInfo *pGI = pSrc->GetValue();
	NDb::CAIGeometry *pAIG = NDb::GetAIGeometry( nAIGeomID );
	if ( !pGI || !pAIG )
		return;
	unordered_map<int, bool> addedParts;
	//
	for ( vector<int>::const_iterator it = parts.begin(); it != parts.end(); ++it )
	{
		int nPart = *it;
		int x, y, z;
		GetPieceCoords( nPart, &x, &y, &z );
		if ( 0 == x && 0 == y && 0 == z )
			continue;
		vector<SJunction> juncs;
		const NAI::CGeometryInfo::SPiece &piece = pGI->pieces[nPart];
		const float fWeight = piece.fVolume * pAr->pMaterial->fWeight;

		CVec3 pt( x-1, y-1, z-1 );
		NDb::SPieceLinksHash::const_iterator i = pAIG->additionalLinks.find( nPart );
		if ( i != pAIG->additionalLinks.end() )
		{
			const vector<CVec3> &links = i->second.links;
			for ( int j = 0; j < links.size(); ++j )
			{
				const CVec3 ptJ = pt + links[j];
				juncs.push_back( SJunction( ptJ, bGround && fabs( ptJ.z ) < FP_EPSILON ) );
			}
		}
		else
		{
			for ( int i = 0; i < piece.juncs.size(); ++i )
				juncs.push_back( SJunction( piece.juncs[i].pt, bGround && piece.juncs[i].bGround ) );
		}
		JuncsMoveRotate( dpos, &juncs );
		dpos.MoveAndRotate( &pt );
		bool bCellarWall = IsCellarWall( pt, cellarWalls );
		pSchema->AddNode( pAr, pt, bCellarWall, fWeight, juncs );
		addedParts[ nPart ] = true;
	}
	//
	int partx, party, partz;
	GetPartCoords( nSubPartID, &partx, &party, &partz );
	for ( int j = 0; j < additionparts.size(); ++j )
//	for ( NDb::SPieceLinksHash::const_iterator i = pAIG->additionalLinks.begin(); i != pAIG->additionalLinks.end(); ++i )
	{
		NDb::SPieceLinksHash::const_iterator i = pAIG->additionalLinks.find( additionparts[j] );
		if ( i == pAIG->additionalLinks.end() || addedParts.find( i->first ) != addedParts.end() )
			continue;
		int x, y, z;
		int px, py, pz;
		GetPieceCoords( i->first, &x, &y, &z );
		GetPartCoords( i->first, &px, &py, &pz );
		if ( 0 == x && 0 == y && 0 == z || ( px != partx || py != party || pz != partz ) )
			continue;
		vector<SJunction> juncs;
		CVec3 pt( x-1, y-1, z-1 );
		const vector<CVec3> &links = i->second.links;
		for ( int j = 0; j < links.size(); ++j )
		{
			const CVec3 ptJ = pt + links[j];
			juncs.push_back( SJunction( ptJ, bGround && fabs( ptJ.z ) < FP_EPSILON ) );
		}
		JuncsMoveRotate( dpos, &juncs );
		dpos.MoveAndRotate( &pt );
		bool bCellarWall = IsCellarWall( pt, cellarWalls );
		pSchema->AddNode( pAr, pt, bCellarWall, 0.1f * pAr->pMaterial->fWeight, juncs ); // CRAP - объем для виртуального куска
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int SetAdditionalVisibleParts( int nSubBlockID, CVec3 ptPos, int nRotationID, const CBuildingGrid &grid, const NDb::SPieceLinksHash &info )
{
	DWORD nVisible = 0;
	for ( NDb::SPieceLinksHash::const_iterator i = info.begin(); i != info.end(); ++i )
	{
		const int nPieceID = i->first;
		int x, y, z;
		GetPartCoords( nPieceID, &x, &y, &z );
		if ( GetPartHashID( x, y, z ) != nSubBlockID )
			continue;
		GetPieceCoords( nPieceID, &x, &y, &z );
		if ( GetPieceHashID( x,y,z) == 0 )
			continue;
		SetVisibility( &nVisible, grid, ptPos, nRotationID, SPoint3( x-1, y-1, z-1 ) );
	}
	return nVisible;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SchemaAddSolid( CBuildingSchema *pSchema, const SFragmentPos &frp, 
	const CBuildingGrid &grid, int nAIGeomID, const CArray2D<bool> &cellarWalls )
{
	SClipInfo clip;
	clip.ptPos = frp.nSubPos;
	clip.nRotationID  = frp.pFr->nRotationID;
	clip.nSubBlockID = frp.nHashID; 
	NDb::CRPGArmor *pAr = frp.pCPart->pArmor;
	if ( !pAr )
		return;
	// определяем, какие узлы присутсвуют в блоке
	CDGPtr< CPtrFuncBase<NAI::CGeometryInfo> > pSrc = NAI::shareAIModel.Get( nAIGeomID );
	pSrc.Refresh();
	NAI::CGeometryInfo *pGI = pSrc->GetValue();
	NDb::CAIGeometry *pAIG = NDb::GetAIGeometry( nAIGeomID );
	if ( !pGI || !pAIG )
		return;
	//
	SetSolidVisibleParts( &clip, clip.ptPos, grid, true, pGI );
	int nAdd = SetAdditionalVisibleParts( clip.nSubBlockID, clip.ptPos, clip.nRotationID, grid, pAIG->additionalLinks );
	vector<int> parts, additionparts;
	UnpackParts( &parts, clip.dwParts, frp.nHashID );
	UnpackParts( &additionparts, nAdd, frp.nHashID );
	//
	CVec3 ptPos = clip.ptPos;
	ptPos.z *= 4;
	SDiscretePos dpos( 0, ptPos, clip.nRotationID );
	bool bIndestructible = pAr->pMaterial->nDR < 0;
	const bool bGround = /*bIndestructible ||*/ clip.ptPos.z <= 0;
	//
	AddPieces( pSchema, parts, additionparts, pAr, nAIGeomID, cellarWalls, bGround, dpos, clip.nSubBlockID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SchemaAddWall( CBuildingSchema *pSchema, const SBuildFragment &fr, const CBuildingGrid &grid, 
	int nAIGeomID, const CArray2D<bool> &cellarWalls, NDb::CRPGArmor *pAr )
{
	// определяем, какие узлы присутсвуют в блоке
	CDGPtr< CPtrFuncBase<NAI::CGeometryInfo> > pSrc = NAI::shareAIModel.Get( nAIGeomID );
	pSrc.Refresh();
	NAI::CGeometryInfo *pGI = pSrc->GetValue();
	NDb::CAIGeometry *pAIG = NDb::GetAIGeometry( nAIGeomID );
	if ( !pGI || !pAIG )
		return;

	SClipInfo clip;
	vector<int> parts, additionparts;

	clip.nRotationID = fr.nRotationID;
	clip.nSubBlockID = fr.nSubBlockID; 
	SetSolidVisibleParts( &clip, fr.ptPos, grid, true, pGI );
	int nAdd = SetAdditionalVisibleParts( clip.nSubBlockID, fr.ptPos, fr.nRotationID, grid, pAIG->additionalLinks );
	UnpackParts( &parts, clip.dwParts, fr.nSubBlockID );
	UnpackParts( &additionparts, nAdd, fr.nSubBlockID );
	CVec3 ptPos = fr.ptPos;
	ptPos.z *= 4;
	SDiscretePos dpos( 0, ptPos, fr.nRotationID );
	bool bIndestructible = pAr->pMaterial->nDR < 0 ;
	const bool bGround = /*bIndestructible ||*/ fr.ptPos.z <= 0;
	//
	AddPieces( pSchema, parts, additionparts, pAr, nAIGeomID, cellarWalls, bGround, dpos, clip.nSubBlockID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeBuildingSchema( CBuildingSchema *pSchema, CBuildingGrid *pGrid, CBuildInfo *pBuildInfo, CSolidAndWallMap *_pSWMap )
{
	if ( !pGrid || !pSchema || !pBuildInfo )
	{
		ASSERT( 0 );
		return;
	}
	SRand rand( pGrid->GetSeed() );
	int nJuncReserve = pBuildInfo->wallFragments.size() * 15 + pBuildInfo->solidFragments.size() * 9;
	int nRodReserve = pBuildInfo->wallFragments.size() * 22 + pBuildInfo->solidFragments.size() * 12;
	pSchema->Reserve( nJuncReserve, nRodReserve );

	CDGPtr<CSolidAndWallMap> pSWMap( _pSWMap );
	pSWMap.Refresh();
	const CSolidAndWallMap &swMap = *_pSWMap;

	// Сплошные объекты
	const unordered_map<int, CNodeMap<SSolidElement> > &solidMap = swMap.GetSolidMap();

	CArray2D<bool> cellarWalls;
	MarkCellarWalls( &cellarWalls, pBuildInfo->cellar, swMap.GetBottom() );

	for (unordered_map<int, CNodeMap<SSolidElement> >::const_iterator it = solidMap.begin(); it != solidMap.end(); ++it )
	{
		const vector<SSolidElement> &frags = it->second.GetValues();
		for ( int i = 0; i < frags.size(); ++i )
		{
			const SSolidElement &e = frags[i];
			for ( int j = 0; j < e.fragments.size(); ++j )
			{
				const SFragmentPos &frp = e.fragments[j];
				if ( !frp.pFr )
					continue;
				if ( IsValid( frp.pCPart->pGeometry ) && IsValid( frp.pCPart->pGeometry->pAIGeometry ) )
					SchemaAddSolid( pSchema, frp, *pGrid, frp.pCPart->pGeometry->pAIGeometry->GetRecordID(), cellarWalls );
				if ( IsValid( frp.pCPart->p2ndGeometry ) && IsValid( frp.pCPart->p2ndGeometry->pAIGeometry ) )
					SchemaAddSolid( pSchema, frp, *pGrid, frp.pCPart->p2ndGeometry->pAIGeometry->GetRecordID(), cellarWalls );
			}
		}
	}
	// Стены
	const CNodeMap<SGridNode> &wallGrid = swMap.GetWallGrid();
	const vector<SLRNeighbs> &neighbs = swMap.GetNeighbs();

	for ( int i = 0; i < pBuildInfo->wallFragments.size(); ++i )
	{
		const SBuildFragment &fr = pBuildInfo->wallFragments[i];
		NDb::CConstructionPart *pCP = neighbs[i].pCPart;
		if ( !IsValid( pCP ) || !IsValid( pCP->pArmor ) )
			continue;
		if ( IsValid( pCP->pGeometry ) && IsValid( pCP->pGeometry->pAIGeometry ) )
			SchemaAddWall( pSchema, fr, *pGrid, pCP->pGeometry->pAIGeometry->GetRecordID(), cellarWalls, pCP->pArmor );
		if ( IsValid( pCP->p2ndGeometry ) && IsValid( pCP->p2ndGeometry->pAIGeometry ) )
			SchemaAddWall( pSchema, fr, *pGrid, pCP->p2ndGeometry->pAIGeometry->GetRecordID(), cellarWalls, pCP->pArmor );
	}
	//
	const vector<CJunction> &juncs = pSchema->GetJuncs();
	for ( vector<CJunction>::const_iterator pJ = juncs.begin(); pJ != juncs.end(); ++pJ )
	{
		if ( /*pJ->IsFilled() &&*/ pJ->IsBottom() || pJ->IsCellarWall() )
			pGrid->SetCellar( SPoint3( pJ->ptJ.x, pJ->ptJ.y, pJ->ptJ.z ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool UpdateBuildingStability( int nBuildingID, CBuildingGrid *pGrid, CSolidAndWallMap *_pSWMap, int nMaxIt )
{
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( nBuildingID );
	pLoader.Refresh();
	NBuilding::CBuildInfo *pBuildInfo = pLoader->GetValue();
	if ( !pBuildInfo )
		return false;

	CDGPtr<CSolidAndWallMap> pSWMap( _pSWMap );
	pSWMap.Refresh();
	const CSolidAndWallMap &swMap = *_pSWMap;

	bool bExec = true;
	int  nIterations = -1;
	int nMaxIterations = nMaxIt - 1;
	NHPTimer::STime t;
	NHPTimer::GetTime( &t );

	CBuildingSchema *pSchema = pGrid->GetSchema();
	if ( !IsValid( pSchema ) )
	{
		NHPTimer::STime tschema;
		NHPTimer::GetTime( &tschema );
		pSchema = new CBuildingSchema();
		MakeBuildingSchema( pSchema, pGrid, pBuildInfo, _pSWMap );
		pSchema->Start();
		pGrid->SetSchema( pSchema );
		float fSchemaTime = NHPTimer::GetTimePassed( &tschema );
		csSystem << "<font size=16pt>";
		csSystem << "Building stability update: Schema: " << fSchemaTime << endl;
		if ( ++nIterations == nMaxIterations )
			return true;
	}
	while ( bExec )
	{
		bExec = pSchema->Recalc( pGrid );
		if ( bExec )
			pSchema->Reset();
		if ( ++nIterations == nMaxIterations )
			break;
	}
//#ifdef _DEBUG
	double fTime = NHPTimer::GetTimePassed( &t );
	csSystem << "<font size=16pt>";
	csSystem << "Building stability update: " << nIterations + 1 << "iterations; Time: " << fTime << endl;
//#endif
	if ( !bExec )
		pGrid->SetSchema( 0 );
	return bExec;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static float fTotalHP = 0;
static float fMaxHP = 0;
static int nTotalNodes = 0;
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddNodeHP( CBuildingGrid *pGrid, const SBuildFragment &fr, int nHashID, int nAGeometryID, 
											const CVec3 &ptPos, NDb::CRPGArmor *pArmor )
{
	// определяем, какие узлы присутсвуют в блоке
	CDGPtr< CPtrFuncBase<NAI::CGeometryInfo> > pSrc = NAI::shareAIModel.Get( nAGeometryID );
	pSrc.Refresh();
	NAI::CGeometryInfo *pGI = pSrc->GetValue();
	NDb::CAIGeometry *pAIG = NDb::GetAIGeometry( nAGeometryID );
	if ( !pGI || !pArmor || !pAIG )
	{
		ASSERT(0);
		return;
	}
	//
	vector<int> parts;
	parts.push_back( nHashID );
	SplitOptimized( &parts );
	//
	SDiscretePos dpos( 0, CVec3( ptPos.x, ptPos.y, ptPos.z * 4), fr.nRotationID );
	//
	for ( vector<int>::iterator it = parts.begin(); it != parts.end(); ++it )
	{
		int x, y, z;
		NAI::CGeometryInfo::CPieceMap::const_iterator ipiece = pGI->pieces.find( *it );
		if ( ipiece == pGI->pieces.end() )
		{
			NDb::SPieceLinksHash::const_iterator ip = pAIG->additionalLinks.find( *it );
			if ( ip != pAIG->additionalLinks.end() )
			{
				GetPieceCoords( *it, &x, &y, &z );
				CVec3 pt( x-1, y-1, z-1 );
				dpos.MoveAndRotate( &pt );
				pGrid->AddHP( pt, 1 );
			}
			continue;
		}
		int nID = FixSmallPieceID( pGI->pieces, *it );
		GetPieceCoords( nID, &x, &y, &z );
		CVec3 pt( x-1, y-1, z-1 );
		dpos.MoveAndRotate( &pt );
		//const float fVol = pow( ipiece->second.fVolume, 1.0f/3.0f );
		if ( pArmor->pMaterial->nDR > 0 )
		{
			float fHP = 2.0f * pArmor->pMaterial->nVP * ipiece->second.fVolume; //!
	//		ASSERT(fHP > 1);
			if ( !_isnan( fHP ) )
			{
#ifdef _DEBUG
				fTotalHP += fHP;
				fMaxHP = Max( fMaxHP, fHP );
				nTotalNodes++;
#endif
			}
			else
			{
	//			ASSERT(0);
				fHP = 10;
			}
			pGrid->AddHP( pt, ceil( fHP ) );
		}
		else
			pGrid->SetIndestructible( pt );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void BuildingHP( CBuildInfo *pBuildInfo, CBuildingGrid *pGrid, CSolidAndWallMap *_pSWMap )
{
	if ( !pGrid || !pBuildInfo )
	{
		ASSERT( 0 );
		return;
	}
	SRand rand( pGrid->GetSeed() );
	pGrid->Reset();
	if ( !pGrid->NeedComputeStability() )
		return;

	CDGPtr<CSolidAndWallMap> pSWMap( _pSWMap );
	pSWMap.Refresh();
	const CSolidAndWallMap &swMap = *_pSWMap;

	fMaxHP = 0;
	fTotalHP = 0;
	nTotalNodes = 0;
  // Сплошные объекты
	const unordered_map<int, CNodeMap<SSolidElement> > &solidMap = swMap.GetSolidMap();
	for (unordered_map<int, CNodeMap<SSolidElement> >::const_iterator it = solidMap.begin(); it != solidMap.end(); ++it )
	{
		const vector<SSolidElement> &frags = it->second.GetValues();
		for ( int i = 0; i < frags.size(); ++i )
		{
			const SSolidElement &e = frags[i];
			for ( int j = 0; j < e.fragments.size(); ++j )
			{
				const SFragmentPos &frp = e.fragments[j];
				if ( !frp.pFr ) 
					continue;
				if ( IsValid( frp.pCPart->pGeometry ) && IsValid( frp.pCPart->pGeometry->pAIGeometry ) )
					AddNodeHP( pGrid, *frp.pFr, frp.nHashID, frp.pCPart->pGeometry->pAIGeometry->GetRecordID(), frp.nSubPos, frp.pCPart->pArmor );
				if ( IsValid( frp.pCPart->p2ndGeometry ) && IsValid( frp.pCPart->p2ndGeometry->pAIGeometry ) )
					AddNodeHP( pGrid, *frp.pFr, frp.nHashID, frp.pCPart->p2ndGeometry->pAIGeometry->GetRecordID(), frp.nSubPos, frp.pCPart->pArmor );
			}
		}
	}
	// Стены
	const CNodeMap<SGridNode> &wallGrid = swMap.GetWallGrid();
	const vector<SLRNeighbs> &neighbs = swMap.GetNeighbs();
	for ( int i = 0; i < pBuildInfo->wallFragments.size(); ++i )
	{
		const SBuildFragment &fr = pBuildInfo->wallFragments[i];
		NDb::CConstructionPart *pCP = neighbs[i].pCPart;
		if ( !IsValid( pCP ) )
			continue;
		if ( IsValid( pCP->pGeometry ) && IsValid( pCP->pGeometry->pAIGeometry ) )
			AddNodeHP( pGrid, fr, fr.nSubBlockID, pCP->pGeometry->pAIGeometry->GetRecordID(), fr.ptPos, pCP->pArmor );
		if ( IsValid( pCP->p2ndGeometry ) && IsValid( pCP->p2ndGeometry->pAIGeometry ) )
			AddNodeHP( pGrid, fr, fr.nSubBlockID, pCP->p2ndGeometry->pAIGeometry->GetRecordID(), fr.ptPos, pCP->pArmor );
	}
#ifdef _DEBUG
	char buf[256];
	sprintf( buf, "Average building node HP = %f MaxHP = %f (%f,%d)\n", fTotalHP / nTotalNodes, fMaxHP, fTotalHP, nTotalNodes );
	OutputDebugString( buf );
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CBuildingInfoHold::CBuildingInfoHold( CBuildingGrid *pGrid, CSolidAndWallMap *_pSWMap, int nBuildingID, bool bSplit )
	:pBuildingGrid( pGrid ), bSplitParts(bSplit), pSWMap(_pSWMap)
{
	NBuilding::CBuildInfoLoader *pShareBuildInfo = NGScene::shareBuildings.Get( nBuildingID );
	pBuildInfo = pShareBuildInfo;
	pPrecacheBuilding = new NGScene::CResourcePrecache<NBuilding::CBuildInfoLoader>( pShareBuildInfo );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingInfoHold::RecalcInfo()
{
	pBuildInfo.Refresh();
	pBuildingGrid.Refresh();
	MakeBuilding( &info, *pBuildingGrid, pBuildInfo->GetValue(), pSWMap, bSplitParts );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingInfoHold::UpdateInfo()
{
	bool bInfo = pBuildInfo.Refresh();
	bool bGrid = pBuildingGrid.Refresh();
	if ( bInfo || bGrid )
		RecalcInfo();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingInfoHold::UpdateInfo( const vector<SPart> &_parts )
{
	if ( pBuildInfo.Refresh() )
		RecalcInfo();
	else
	{
		if ( _parts.empty() )
			return;
		bool bGrid = pBuildingGrid.Refresh();
#ifndef _MAPEDIT
		ASSERT( bGrid );
#endif
		unordered_map<SPart, bool, SPart> parts;
		for ( int i = 0; i < _parts.size(); ++i )
			parts[ _parts[i] ] = true;
		MakeBuilding( &info, *pBuildingGrid, pBuildInfo->GetValue(), pSWMap, bSplitParts, &parts );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSolidAndWallMap* MakeSWMap( int nBuildingID, const SRandomSeed &_seed )
{
	return new CSolidAndWallMap( NGScene::shareBuildings.Get( nBuildingID ), _seed );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NBuilding;
REGISTER_SAVELOAD_CLASS( 0xA0242120, CBuildingInfoHold )
using namespace NGScene;
REGISTER_SAVELOAD_TEMPL_CLASS( 0x028b2170, CResourcePrecache<NBuilding::CBuildInfoLoader>, CResourcePrecache )
