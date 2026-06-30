#include "StdAfx.h"
#include "MakeBuildingInternal.h"
#include "..\DBFormat\DataMap.h"


namespace NBuilding
{
////////////////////////////////////////////////////////////////////////////////////////////////////
EPriority GetPriority( const NDb::CConstructionPart *pPart, int x, int y )
{
	if ( NDb::CConstructionPart::IsPrimaryPart( pPart->nSubPartsMask, x, y ) )
		return SP_PRIMARY;
	// если одна из сторон является смежной для primary, то приоритет - SP_SECONDARY
	if ( x > 0 && NDb::CConstructionPart::IsPrimaryPart( pPart->nSubPartsMask, x - 1, y ) )
		return SP_SECONDARY;
	if ( x < pPart->nSizeX - 1 && NDb::CConstructionPart::IsPrimaryPart( pPart->nSubPartsMask, x + 1, y ) )
		return SP_SECONDARY;
	if ( y > 0 && NDb::CConstructionPart::IsPrimaryPart( pPart->nSubPartsMask, x, y - 1 ) )
		return SP_SECONDARY;
	if ( y < pPart->nSizeY - 1 && NDb::CConstructionPart::IsPrimaryPart( pPart->nSubPartsMask, x, y + 1 ) )
		return SP_SECONDARY;
	return SP_LOW;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const int DEF_FLOOR = -100;
struct SFloor
{
	int n;
	SFloor( int nFloor = DEF_FLOOR ): n(nFloor) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFloorGroup
{
	unordered_map<int, SFloor> floors;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeFloorGroupHash(unordered_map<int, SFloorGroup> *pGID2Floors, const vector<NBuilding::SLayerGroup> &groups )
{
	unordered_map<int, SFloorGroup> &hash = *pGID2Floors;
	for ( int i = 0; i < groups.size(); ++i )
	{
		const NBuilding::SLayerGroup g = groups[i];
		if ( g.layers.size() < 1 )
			continue;
		SFloorGroup &fg = hash[g.layers.front()];
		for ( int i = 1; i < g.floor.size(); ++i )
			fg.floors[g.floor[i]] = g.floor.front();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeLayerGroupHash(unordered_map<int, int> *pID2GID, const vector<NBuilding::SLayerGroup> &groups )
{
	unordered_map<int, int> &hash = *pID2GID;
	for ( int i = 0; i < groups.size(); ++i )
	{
		const NBuilding::SLayerGroup g = groups[i];
		if ( g.layers.size() <= 1 )
			continue;
		hash[g.layers.front()] = g.layers.front();
		for ( int i = 1; i < g.layers.size(); ++i )
			hash[g.layers[i]] = g.layers.front();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSolidAndWallMap::MakeSolidMap( SRand *pRand, const vector<SBuildFragment> &solids, 
	int nMinFloor, int nMaxFloor, int nXSize, int nYSize,
	const vector<NBuilding::SLayerGroup> &groups )
{
	ASSERT( pRand );
	CNodeMap<SSolidElement> pattern;
	pattern.Resize( nMinFloor, nMaxFloor, nXSize, nYSize );

	unordered_map<int, int> gids;
	unordered_map<int, SFloorGroup> linkedfloors; // linkedfloors[ gid ]

	bottom.SetSizes( nXSize, nYSize );
	bottom.FillZero();
	MakeLayerGroupHash( &gids, groups );
	MakeFloorGroupHash( &linkedfloors, groups );
	for ( int i = 0; i < solids.size(); ++i )
	{
		const SBuildFragment &fr = solids[i];
		NDb::CTConstructionPart *pTConstructionPart = NDb::GetTConstructionPart( fr.nConstructionPartID );
		if ( !IsValid( pTConstructionPart ) )
			continue;
		CPtr<NDb::CConstructionPart> pCP = pTConstructionPart->CreateConstructionPart( pRand );
		if ( !IsValid( pCP ) )
			continue;
		//
		CVec3 ptShift = VNULL3;
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
		for ( int k = 0; k < pCP->nSizeY; ++k )
			for ( int l = 0; l < pCP->nSizeX; ++l )
			{
				for ( int m = 0; m < pCP->nSizeZ; ++m )
				{
					const CVec3 nPos( l, k, m );
					SDiscretePos dpos( 0,  VNULL3, fr.nRotationID );
					SDiscretePos dposm( 0,  fr.ptPos, fr.nRotationID );
					CVec3 pos = nPos;
					pos.x *= 2;
					pos.y *= 2;
					dposm.MoveAndRotate( &pos );
					pos += ptShift;
					CVec3 vv( 1, 1, 0 );
					dpos.MoveAndRotate( &vv );
					CVec3 spos = pos;
					spos.x = vv.x < 0 ? pos.x - 1 : pos.x;
					spos.y = vv.y < 0 ? pos.y - 1 : pos.y;
					// объединение слоев для клиппинга
					int nClipGroupID = gids[fr.nFragmentID];
					nClipGroupID = 0 == nClipGroupID ? fr.nFragmentID : nClipGroupID;
					if ( solidMap.find( nClipGroupID ) == solidMap.end() )
						solidMap.insert( pair<int, CNodeMap<SSolidElement> >( nClipGroupID, pattern ) );
					CNodeMap<SSolidElement> &smap = solidMap.find( nClipGroupID )->second;
					// объединение этажей для клиппинга
					SFloorGroup &fgroup = linkedfloors[nClipGroupID];
					int nZ;
					if ( fgroup.floors.empty() || fgroup.floors[spos.z].n == DEF_FLOOR )
						nZ = spos.z;
					else
						nZ = fgroup.floors[spos.z].n;
					//
					if ( !smap.IsValidCoord( nZ, spos.x, spos.y ) )
						continue;
					SSolidElement &e = smap.At( nZ, spos.x, spos.y );
					EPriority pr = GetPriority( pCP, nPos.x, nPos.y );
					if ( e.nPrority > pr ) 
						continue; // в этом узле уже есть более приоритетный блок
					if ( pr > e.nPrority )
						e.fragments.clear();
					e.nPrority  = pr;
					SFragmentPos &frp = *e.fragments.insert( e.fragments.end(), SFragmentPos());
					frp.pFr = &fr;
					frp.nSubPos = pos;
					frp.pCPart  = pCP;
					frp.nIndex  = i;
					frp.nHashID = GetPartHashID( nPos.x + 1, nPos.y + 1, nPos.z + 1 );
					const int nsy = Float2Int(spos.y);
					const int nsx = Float2Int(spos.x);
					if ( nsx < nXSize && nsy < nYSize && spos.z < bottom[nsy][nsx] )
						bottom[Float2Int(spos.y)][Float2Int(spos.x)] = spos.z;
					// заполняем все тайлы занимаемые блоком
					vector<CVec3> tiles;
					tiles.push_back( CVec3( 1, 0, 0 ) );
					tiles.push_back( CVec3( 1, 1, 0 ) );
					tiles.push_back( CVec3( 0, 1, 0 ) );
					dpos.MoveAndRotate( &tiles );
					for ( int s = 0; s < tiles.size(); ++s )
					{
						const int nx = Float2Int( tiles[s].x + spos.x );
						const int ny = Float2Int( tiles[s].y + spos.y );
						if ( nx >= nXSize || ny >= nYSize )
						{
							ASSERT(0);
							continue;
						}
						if ( spos.z < bottom[ny][nx] )
							bottom[ny][nx] = spos.z;
						SSolidElement &e = smap.At( nZ, nx, ny );
						if ( e.nPrority < pr )
						{
							e.nPrority  = pr;
							e.fragments.clear();
						}
					}
				}
			}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline void AssignLRNode( const NDb::CConstructionPart *pCP, SGridNode *pLNode, SGridNode *pRNode,SBuildFragment *pFr, int nFragmentID, int il,	int ir )
{
	/*
	#ifdef _DEBUG
	if ( pLNode->elems[il].pFragment || pRNode->elems[ir].pFragment )
	{
	OutputDebugString( "CBuilding: Warning : bad wall map\n" );
	}
	#endif // _DEBUG
	*/
	int x, y, z;
	GetPartCoords( pFr->nSubBlockID, &x, &y, &z );
	SGridElement e;

	e.pFragment = pFr;
	e.nFragmentID = nFragmentID;
	e.fLength = 2;
	e.fThickness = pCP->fThickness;
	e.fHeight = pCP->nSizeZ;
	e.nClipGroup = pCP->nClipGroup;

	if ( 1 == x )
	{
		e.side = LEFT;
		pLNode->elems[il].push_back( e ) ;
	}
	if ( x == pCP->nSizeX )
	{
		e.side = RIGHT;
		pRNode->elems[ir].push_back( e );	
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// 0-left, 1-top, 2-right, 3-bottom
enum 
{
	ELEFT = 0,
	ETOP,
	ERIGHT,
	EBOTTOM,
};
static void FillGridNodes( CNodeMap<SGridNode> *pGrid, SBuildFragment *pf, int nFragmentID, SLRNeighbs *pNeibs )
{
	if ( !pGrid->IsValidCoord( pf->ptPos.z, pf->ptPos.x, pf->ptPos.y ) )
		return;
	SGridNode &node = pGrid->At( pf->ptPos.z, pf->ptPos.x, pf->ptPos.y );
	SGridNode *pRNode = 0;

	switch ( pf->nRotationID )
	{
	case SDiscretePos::TURN_0:
		{
			pRNode = &pGrid->At( pf->ptPos.z, pf->ptPos.x + 2, pf->ptPos.y );
			AssignLRNode( pNeibs->pCPart, &node, pRNode, pf, nFragmentID, 2, 0 );
		}
		break;
	case SDiscretePos::TURN_90:
		{
			pRNode = &pGrid->At( pf->ptPos.z, pf->ptPos.x, pf->ptPos.y + 2 );
			AssignLRNode( pNeibs->pCPart, &node, pRNode, pf, nFragmentID, 1, 3 );
		}
		break;
	case SDiscretePos::TURN_180:
		{
			pRNode = &pGrid->At( pf->ptPos.z, pf->ptPos.x - 2, pf->ptPos.y );
			AssignLRNode( pNeibs->pCPart, &node, pRNode, pf, nFragmentID, 0, 2 );
		}
		break;
	case SDiscretePos::TURN_270:
		{
			pRNode = &pGrid->At( pf->ptPos.z, pf->ptPos.x, pf->ptPos.y - 2 );
			AssignLRNode( pNeibs->pCPart, &node, pRNode, pf, nFragmentID, 3, 1 );
		}
		break;
	default:
		ASSERT(0);
		break;
	}
	pNeibs->pLeft = &node;
	pNeibs->pRight = pRNode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSolidAndWallMap::MakeWallMap( SRand *pRand, vector<SBuildFragment> *pWallFrags )
{
	ASSERT( pWallFrags->size() == neighbs.size() );

	for ( int i = 0; i < pWallFrags->size(); ++i )
	{
		SBuildFragment &fr = (*pWallFrags)[i];
		NDb::CTConstructionPart *pTConstructionPart = NDb::GetTConstructionPart( fr.nConstructionPartID );
		if ( !IsValid( pTConstructionPart ) )
			continue;
		CPtr<NDb::CConstructionPart> pCP = pTConstructionPart->CreateConstructionPart( pRand );
		if ( !IsValid( pCP ) )
			continue;

		SLRNeighbs *pLRN = &neighbs[i];
		pLRN->pCPart = pCP;
		FillGridNodes( &wallGrid, &fr, i, pLRN );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSolidAndWallMap::MakeMaps( SRand *pRand, const vector<SBuildFragment> &solids, vector<SBuildFragment> *pWallFrags,
	int nMinFloor, int nMaxFloor, int nXSize, int nYSize,
	const vector<NBuilding::SLayerGroup> &groups )
{
	MakeSolidMap( pRand, solids, nMinFloor, nMaxFloor, nXSize, nYSize, groups );
	wallGrid.Resize( nMinFloor, nMaxFloor, nXSize, nYSize );
	neighbs.resize( pWallFrags->size() );
	MakeWallMap( pRand, pWallFrags );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSolidAndWallMap::Recalc()
{
	CBuildInfo *pBI = pBuildInfo->GetValue();
	solidMap.clear();
	neighbs.clear();
	bottom.Clear();
	wallGrid.Clear();
	SRand rand( seed );
	MakeMaps( &rand, pBI->solidFragments, &pBI->wallFragments,
		pBI->nMinFloor, pBI->nMaxFloor, pBI->nMaxX + 2, pBI->nMaxY + 2,
		pBI->lgroups );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NBuilding;
REGISTER_SAVELOAD_CLASS( 0xA14b2190, CSolidAndWallMap )