#include "StdAfx.h"
#include "MapBuildTerrain.h"
#include "TerrainInfo.h"
#include "Grid.h"
#include "GResource.h"
#include "Interpolate.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataTerrain.h"
#include "..\DBFormat\DataFormat.h"
#include "..\Misc\BasicShare.h"
#include "BuildingInfo.h"
#include "PolyUtils.h"
#include "METerrain.h"
#include "GPixelFormat.h"

const BYTE  TRANSPARENT_TILE = 11;
namespace NGScene
{
	externA5 CBasicShare<int, NBuilding::CBuildInfoLoader> shareBuildings;
}
CBasicShare<int, CMETerrainLoader> shareTerrains( 138 );
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRotateInfo
{
	float fXPos, fYPos;
	CVec2 ptShift;
	float fSin, fCos, fAngle;

	SRotateInfo( const CVec2 &_ptShift, float _fAngle )
		: fAngle(_fAngle), fSin( sin(_fAngle) ), fCos( cos(_fAngle) )
		, fXPos(_ptShift.x), fYPos(_ptShift.y) {}
	CVec2 Rotate( const CVec2 &_p ) const
	{
		return CVec2( fCos * _p.x - fSin * _p.y + fXPos, fSin * _p.x + fCos * _p.y + fYPos );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
float GetMeterHeightCheck( const STerrainInfo &info, float x, float y )
{
	float fX = x * FP_INV_GRID_STEP;
	float fY = y * FP_INV_GRID_STEP;
	float fRes = GetBilinear( info.heightMap, fX, fY, TLinearInterpolate() );
	return FP_TERRAIN_H_SCALE * fRes;
}
typedef CTPoint<int> CPt;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SEdge
{
	CPt  ptStart;
	CPt  ptEnd;
	SEdge( const CPt &s, const CPt &e ): ptStart(s), ptEnd(e) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static void GenerateEdges( list<SEdge> *pEdges, const CArray2D<bool> &cellar )
{
	const int nX = cellar.GetXSize();
	const int nY = cellar.GetYSize();
	for ( int x = 0; x < nX; ++x )
		for ( int y = 0; y < nY; ++y )
		{
			if ( !cellar[y][x] )
				continue;
			if ( x == 0 || !cellar[y][x-1] )
				pEdges->push_back( SEdge( CPt( x, y + 1 ), CPt( x, y ) ) );
			if ( x == nX - 1 || !cellar[y][x+1] )
				pEdges->push_back( SEdge( CPt( x + 1, y ), CPt( x + 1, y + 1 ) ) );
			if ( y == 0 || !cellar[y-1][x] )
				pEdges->push_back( SEdge( CPt( x, y ), CPt( x + 1, y ) ) );
			if ( y == nY - 1 || !cellar[y+1][x] )
				pEdges->push_back( SEdge( CPt( x + 1, y + 1 ), CPt( x, y + 1 ) ) );
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
list<SEdge>::iterator FindNextEdge( const SEdge &e, list<SEdge> *pEdges )
{
	list<SEdge>::iterator iret = pEdges->begin();
	for ( ; iret != pEdges->end(); ++iret )
		if ( iret->ptStart == e.ptEnd )
			break;
	return iret;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetNewContour( STerrainHole *pHole, list<SEdge> *pEdges )
{
	pHole->bVisible = false;
	pHole->nHeight  = -200;
	SEdge last = *pEdges->begin();
	list<SEdge>::iterator i;
	for ( i = pEdges->begin(); i != pEdges->end(); i = FindNextEdge( last, pEdges ) )
	{
		last = *i;
		pHole->vPolygon.push_back( CVec2( last.ptStart.x, last.ptStart.y ) );
		pEdges->erase( i );
	}
	if ( !pHole->vPolygon.empty() && pHole->vPolygon.front() == pHole->vPolygon.back() )
		pHole->vPolygon.pop_back();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void GenerateCellarHoles( STerrainInfo *pTerr, int nVarID )
{
	ASSERT( pTerr );
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( nVarID );
	pLoader.Refresh();
	NBuilding::CBuildInfo *pBInfo = pLoader->GetValue();
	//
	list<SEdge> edges;

	if ( pBInfo->cellar.GetXSize() != pTerr->nWidth || pBInfo->cellar.GetYSize() != pTerr->nHeight )
		return;
	GenerateEdges( &edges, pBInfo->cellar );
	while ( !edges.empty() )
	{
		STerrainHole &h = *pTerr->holes.insert( pTerr->holes.end(), STerrainHole());
		GetNewContour( &h, &edges );
#ifdef _DEBUG
		TPolygonsList l;
		l.push_back( pTerr->holes.back().vPolygon );
		DumpPolyList( l );
#endif
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
class CVarTerrain : public CObjectBase
{
	OBJECT_BASIC_METHODS(CVarTerrain)
public:
	STerrainInfo info;
	CArray2D<unsigned short> alpha;
};

//typedef unordered_map<int, CObj<CVarTerrain> > CTerrainCache;
//static CTerrainCache terrCache;
////////////////////////////////////////////////////////////////////////////////////////////////////
static CMETerrainInfo* LoadTerrInfo( int nVarID, SRand *pRand, const vector<int> &flags )
{
	CDGPtr< CPtrFuncBase<CMETerrainInfo> > pLoader = shareTerrains.Get( nVarID );
	pLoader.Refresh();
	CMETerrainInfo *pmeInfo = pLoader->GetValue();
	CMETerrainInfo *pRet = 0;
	if ( IsValid( pmeInfo ) )
	{
		pRet = new CMETerrainInfo;
		*pRet = *pmeInfo;
		GenerateCellarHoles( &pRet->info, nVarID );
	}
	return pRet;
/*
	CTerrainCache::iterator i = terrCache.find( nVarID );
	if ( i != terrCache.end() )//IsValid( terrCache[nVarID] ) )
		return i->second;//terrCache[nVarID];
	//
	CObj<CVarTerrain> pTerr = 0;
	try
	{
		NGScene::CResourceOpener file( "Terrain", nVarID );
		CObj<CVarTerrain> pTempTerr = new CVarTerrain;
		CStructureSaver *pSS = file.operator->();
		pSS->Add( 21, &pTempTerr->info );
		pSS->Add( 6, &pTempTerr->alpha );
		GenerateCellarHoles( &pTempTerr->info, nVarID );
		pTerr = pTempTerr;
	}
	catch(...)
	{
	}
	terrCache[nVarID] = pTerr;
	// copy holes
	return pTerr;
	*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool SpotCmp( const NDb::CRndTerrainSpot *p1, const NDb::CRndTerrainSpot *p2 )
{
	if ( !p1 || !p2 )
		return false;
	return p1->nPriority < p2->nPriority;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void LoadTerrSpots( int nVarID, STerrainInfo *pTerr, SRand *pRand, const vector<int> &flags )
{
	pTerr->spots.clear();
	NDb::CTemplVariant *pVariant = NDb::GetTemplVariant( nVarID );
	vector< CPtr<NDb::CRndTerrainSpot> > spots = pVariant->terrainSpots;
	sort( spots.begin(), spots.end(), SpotCmp );

	for ( int k = 0; k < spots.size(); ++k )
	{
		if ( !IsValid( spots[k] ) )
			continue;

		CPtr<NDb::CTerrainSpot> p = spots[k]->CreateTerrainSpot( pRand, flags );
		if ( !IsValid( p ) )
			continue;

		pTerr->spots.push_back( STerrainSpot( p->pMaterial, p->ptPos, p->ptSize, ToRadian((float)p->nRotation), SStepSound( p->pArmor ) ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline void FixSizes( CArray2D<T> *pRes, int nXSize, int nYSize, T fill )
{
	if ( pRes->GetXSize() != nXSize || pRes->GetYSize() != nYSize )
	{
		CArray2D<T> copy = *pRes;
		
		pRes->SetSizes( nXSize, nYSize );
		pRes->FillEvery( fill );
		if ( copy.GetXSize() <= 1 && copy.GetYSize() <= 1 )
			return;
		int nX = Min( nXSize, copy.GetXSize() );
		int nY = Min( nYSize, copy.GetYSize() );
		for ( int y = 0; y < nY; ++y )
			for ( int x = 0; x < nX; ++x )
				(*pRes)[y][x] = copy[y][x];
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool LoadRootTerrain( int nVariantID, STerrainInfo *pTerrain, SRand *pRand, const vector<int> &flags )
{
	CObj<CMETerrainInfo> pTerrInfo = LoadTerrInfo( nVariantID, pRand, flags );
	if ( pTerrInfo )
		*pTerrain = pTerrInfo->info;
	LoadTerrSpots( nVariantID, pTerrain, pRand, flags );
	pTerrain->nWidth  = Max( 2, pTerrain->nWidth );
	pTerrain->nHeight = Max( 2, pTerrain->nHeight );

	int w = pTerrain->nWidth;// + 1;
	int h = pTerrain->nHeight;// + 1;
	w = ( ( w + 7 ) & ~7 );// + 1;
	h = ( ( h + 7 ) & ~7 );// + 1;
	pTerrain->nWidth = w;
	pTerrain->nHeight = h;
	w += 1;
	h += 1;
	FixSizes( &pTerrain->heightMap, w, h, (unsigned short)0 );
	FixSizes( &pTerrain->typeMap, w, h, TRANSPARENT_TILE );
	FixSizes( &pTerrain->color, w, h, (DWORD)0xffffffff );
	int nGrassW = Float2Int( w * FP_GRASS_COLOR_SCALE );
	int nGrassH = Float2Int( h * FP_GRASS_COLOR_SCALE );
	for ( int k = 0; k < pTerrain->grass.size(); ++k )
	{
		SGrassLayer &l = pTerrain->grass[k];
		FixSizes( &l.grass, w, h, (BYTE)0 );
		FixSizes( &l.grassColor, nGrassW, nGrassH, (DWORD)0xffffffff );
	}
/*
	STerrainHole tHole;
	tHole.bVisible = true;
	tHole.nHeight = -20;
	tHole.vPolygon.push_back( CVec2( 10.5f + 0, 10.5f + 0 ) );
	tHole.vPolygon.push_back( CVec2( 10.5f + 3, 10.5f + 0 ) );
	tHole.vPolygon.push_back( CVec2( 10.5f + 8, 10.5f + 5 ) );
	tHole.vPolygon.push_back( CVec2( 10.5f + 5, 10.5f + 5 ) );
	tHole.vPolygon.push_back( CVec2( 10.5f + 5, 10.5f + 8 ) );
	tHole.vPolygon.push_back( CVec2( 10.5f + 0, 10.5f + 3 ) );
	pTerrain->holes.push_back( tHole );
*/
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ClearTerrainCache()
{
//	terrCache.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T, class TBlend>
static void Blend( CArray2D<T> *pRes, const SRotateInfo &r,
	float fXSize, float fYSize, const TBlend &blend )
{
	for ( int nY = 0; nY < pRes->GetYSize(); ++nY )
	{
		for ( int nX = 0; nX < pRes->GetXSize(); ++nX )
		{
			float xa = nX - r.fXPos + 0.5f;
			float ya = nY - r.fYPos + 0.5f;
			float x =  xa * r.fCos + ya * r.fSin - 0.5f;
			float y = -xa * r.fSin + ya * r.fCos - 0.5f;
			if ( x >= -0.49f && y >= -0.49f && x <= fXSize - 0.51f && y <= fYSize - 0.51f )
				blend( &(*pRes)[nY][nX], x, y );
		}
	}	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct STerrainBlend
{
	const CArray2D<BYTE> &src;
	STerrainBlend( const CArray2D<BYTE> &_src ): src(_src) {}
	void operator()( BYTE *pa, float x, float y ) const 
	{ 
		BYTE b = src[Float2Int(y)][Float2Int(x)];
		if ( TRANSPARENT_TILE != b )
			*pa = b;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGrassBlend
{
	const CArray2D<BYTE> &src;
	SGrassBlend( const CArray2D<BYTE> &_src ): src(_src) {}
	void operator()( BYTE *pa, float x, float y ) const 
	{ 
		BYTE b = (BYTE)GetBilinear( src, x, y, TLinearInterpolate() );
		*pa = b; 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SColorBlend
{
	const CArray2D<DWORD> &src;
	SColorBlend( const CArray2D<DWORD> &_src ): src(_src) {}
	void operator()( DWORD *pa, float x, float y ) const 
	{ 
		DWORD b = src[Float2Int(y)][Float2Int(x)]; // bilinear?
		*pa = b; 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SHeightBlend
{
	const CArray2D<unsigned short> &h, &alpha;
	SHeightBlend( const CArray2D<unsigned short> &_h, const CArray2D<unsigned short> &_alpha )
		:h(_h), alpha(_alpha) {}
	void operator()( unsigned short *pa, float x, float y ) const 
	{ 
		float fA = GetBilinear( alpha, x, y, TLinearInterpolate() );
		float fH = GetBilinear( h, x, y, TLinearInterpolate() );
		fA *= 1.0f / WORD(0xffff);
		*pa = Float2Int( ( 1.0f - fA ) * *pa + fA * fH );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAddHeightBlend
{
	const CArray2D<unsigned short> &h, &alpha;
	SAddHeightBlend( const CArray2D<unsigned short> &_h, const CArray2D<unsigned short> &_alpha )
		:h(_h), alpha(_alpha) {}
		void operator()( unsigned short *pa, float x, float y ) const 
		{ 
			float fA = GetBilinear( alpha, x, y, TLinearInterpolate() );
			float fH = GetBilinear( h, x, y, TLinearInterpolate() );
			fA *= 1.0f / WORD(0xffff);
			*pa += Float2Int( fA * fH );
		}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSubtractHeightBlend
{
	const CArray2D<unsigned short> &h, &alpha;
	SSubtractHeightBlend( const CArray2D<unsigned short> &_h, const CArray2D<unsigned short> &_alpha )
		:h(_h), alpha(_alpha) {}
		void operator()( unsigned short *pa, float x, float y ) const 
		{ 
			float fA = GetBilinear( alpha, x, y, TLinearInterpolate() );
			float fH = GetBilinear( h, x, y, TLinearInterpolate() );
			fA *= 1.0f / WORD(0xffff);
			*pa -= Float2Int( fA * fH );
		}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
struct SReplaceBlend
{
	T src;
	SReplaceBlend( const T &_src ): src(_src) {}
	void operator()( T *pa, float x, float y ) const 
	{ 
		*pa = src;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef SReplaceBlend<SStepSound> SStepSoundBlend;
typedef SReplaceBlend<DWORD> SDWORDReplaceBlend;
////////////////////////////////////////////////////////////////////////////////////////////////////
static void MergeGrassLayers( STerrainInfo *pTerrain, const STerrainInfo &info, 
	const SRotateInfo &r )
{
	SRotateInfo grassRotate(r);
	grassRotate.fXPos *= FP_GRASS_COLOR_SCALE;
	grassRotate.fYPos *= FP_GRASS_COLOR_SCALE;
	for ( int nLayer = 0; nLayer < info.grass.size(); ++nLayer )
	{
		const SGrassLayer &srcLayer = info.grass[nLayer];
		SGrassLayer *pTargetLayer = 0;
		for ( int i = 0; i < pTerrain->grass.size(); ++i )
			if ( pTerrain->grass[i].nGrassID == srcLayer.nGrassID )
			{
				pTargetLayer = &pTerrain->grass[i];
				break;
			}
		if ( !pTargetLayer )
		{
			// create new layer
			pTerrain->grass.resize( pTerrain->grass.size() + 1 );//push_back();
			pTargetLayer = &pTerrain->grass[ pTerrain->grass.size() - 1 ];
			int nGrassW = Float2Int( pTerrain->nWidth * FP_GRASS_COLOR_SCALE );
			int nGrassH = Float2Int( pTerrain->nHeight * FP_GRASS_COLOR_SCALE );
			pTargetLayer->grass.SetSizes( pTerrain->nWidth + 1, pTerrain->nHeight + 1 );
			pTargetLayer->grass.FillZero();
			pTargetLayer->grassColor.SetSizes( nGrassW, nGrassH );
			pTargetLayer->grassColor.FillEvery( 0xffffffff );
			pTargetLayer->nGrassID = srcLayer.nGrassID;
			pTargetLayer->fMaxDensity = srcLayer.fMaxDensity;
		}
		{
			// blend grass density
			const CArray2D<BYTE> &src = srcLayer.grass;
			Blend( &pTargetLayer->grass, r, src.GetXSize(), src.GetYSize(), SGrassBlend( src ) );
		}
		if ( srcLayer.grassColor.GetXSize() > 1 )
		{
			const CArray2D<DWORD> &src = srcLayer.grassColor;
			Blend( &pTargetLayer->grassColor, grassRotate, src.GetXSize(), src.GetYSize(), SColorBlend( src ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void TransferTerrainHoles( STerrainInfo *pTerrain, const STerrainInfo &info, 
	const SRotateInfo &r )
{
	for ( int i = 0; i < info.holes.size(); ++i )
	{
		STerrainHole &hole = *pTerrain->holes.insert( pTerrain->holes.end(), STerrainHole());
		hole.bVisible = info.holes[i].bVisible;
		hole.nHeight  = info.holes[i].nHeight;
		vector<CVec2> &dstPoly = hole.vPolygon;
		const vector<CVec2> &srcPoly = info.holes[i].vPolygon;
		dstPoly.resize( srcPoly.size() );
		for ( int j = 0; j < srcPoly.size(); ++j )
			dstPoly[j] = r.Rotate( srcPoly[j] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void TransferSpots( STerrainInfo *pTerrain, const STerrainInfo &info, 
	const SRotateInfo &r )
{
	SRotateInfo rot = r;
	rot.fXPos *= FP_GRID_STEP;
	rot.fYPos *= FP_GRID_STEP;
	for ( int i = 0; i < info.spots.size(); ++i )
	{
		const STerrainSpot &src = info.spots[i];
		STerrainSpot res( src.pMaterial, rot.Rotate( src.ptPos ), src.ptSize, src.fAngle + rot.fAngle, src.stepSound );
		pTerrain->spots.push_back( res );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void BlendTerrainInfo( STerrainInfo *pRes, int nVariantID, const CVec3 &ptCenter, float fRotation, SRand *pRand, const vector<int> &flags )
{
	NDb::CTemplVariant *pVar = NDb::GetTemplVariant( nVariantID );
	if ( !pVar )
		return;
	SRotateInfo r( CVec2( ptCenter.x / FP_GRID_STEP, ptCenter.y / FP_GRID_STEP ), fRotation );
	CObj<CMETerrainInfo> pTerrInfo = LoadTerrInfo( nVariantID, pRand, flags );
	if ( pTerrInfo )
	{
		if ( pTerrInfo->info.nWidth > pRes->nWidth && pTerrInfo->info.nHeight > pRes->nHeight )
		{
			ASSERT(0);
			return;
		}
		if ( pTerrInfo->info.typeMap.GetXSize() > 1 || pTerrInfo->info.typeMap.GetYSize() > 1 )
		{
			const CArray2D<BYTE> &src = pTerrInfo->info.typeMap;
			Blend( &pRes->typeMap, r, src.GetXSize(), src.GetYSize(), STerrainBlend( src ) );
		}
		if ( pTerrInfo->info.color.GetXSize() > 1 || pTerrInfo->info.color.GetYSize() > 1 )
		{
			// Надо будет смешивать по альфе!
		//	const CArray2D<DWORD> &src = pTerrInfo->info.color;
		//	Blend( &pRes->color, r, src.GetXSize(), src.GetYSize(), SColorBlend( src ) );
		}
		MergeGrassLayers( pRes, pTerrInfo->info, r );
		ASSERT( pTerrInfo->alphaMap.GetXSize() == pTerrInfo->info.heightMap.GetXSize() 
			&& pTerrInfo->alphaMap.GetYSize() == pTerrInfo->info.heightMap.GetYSize() );
		if ( pTerrInfo->info.heightMap.GetXSize() > 1 && pTerrInfo->info.heightMap.GetYSize() > 1 )
		{
			const CArray2D<unsigned short> &src = pTerrInfo->info.heightMap;
			const CArray2D<unsigned short> &srcAlpha = pTerrInfo->alphaMap;
			switch ( pVar->eHMBlendType )
			{
				case NDb::BT_NORMAL:
					Blend( &pRes->heightMap, r, src.GetXSize(), src.GetYSize(), SHeightBlend( src, srcAlpha ) );
					break;
				case NDb::BT_ADD:
					Blend( &pRes->heightMap, r, src.GetXSize(), src.GetYSize(), SAddHeightBlend( src, srcAlpha ) );
					break;
				case NDb::BT_SUBTRACT:
					Blend( &pRes->heightMap, r, src.GetXSize(), src.GetYSize(), SSubtractHeightBlend( src, srcAlpha ) );
					break;
			}
		}
		TransferTerrainHoles( pRes, pTerrInfo->info, r );
	}
	STerrainInfo terri;
	LoadTerrSpots( nVariantID, &terri, pRand, flags );
	TransferSpots( pRes, terri, r );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeSoundMap( STerrainInfo *pInfo )
{
	const CArray2D<unsigned char> &typemap = pInfo->typeMap;
	CArray2D<SStepSound> &smap = pInfo->soundmap;

	const int xSize = typemap.GetXSize();
	const int ySize = typemap.GetYSize();
	smap.SetSizes( xSize, ySize );
	//
	for ( int x = 0; x < xSize; ++x )
		for ( int y = 0; y < ySize; ++y )
		{
			NDb::CTerrainTile *pTile = NDb::GetTerrainTile( typemap[y][x] );
			if ( IsValid( pTile ) )
				smap[y][x] = SStepSound( pTile->pArmor );
		}
	//
	for ( int i = 0; i < pInfo->spots.size(); ++i )
	{
		const STerrainSpot &s = pInfo->spots[i];
		SRotateInfo r( CVec2( s.ptPos.x / FP_GRID_STEP, s.ptPos.y / FP_GRID_STEP ), s.fAngle );
		Blend( &smap, r, s.ptSize.x * FP_INV_GRID_STEP, s.ptSize.y * FP_INV_GRID_STEP, SStepSoundBlend( s.stepSound ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CalcAverageColor( STerrainInfo *pInfo )
{
	const CArray2D<unsigned char> &typemap = pInfo->typeMap;
	CArray2D<DWORD> &avrgColor = pInfo->avrgColor;

	const int xSize = typemap.GetXSize();
	const int ySize = typemap.GetYSize();
	avrgColor.SetSizes( xSize, ySize );

	for ( int y = 0; y < ySize; ++y )
	{
		for ( int x = 0; x < xSize; ++x )
		{
			NDb::CTerrainTile *pTile = NDb::GetTerrainTile( typemap[y][x] );
			CVec4 v = NGfx::GetCVec4Color( pInfo->color[y][x] );
			if ( IsValid( pTile ) )
			{
				CVec4 vColor = NGfx::GetCVec4Color( pTile->pTexture->dwAverageColor );
				v.x *= vColor.x; v.y *= vColor.y; v.z *= vColor.z; v.w *= vColor.w;
			}
			avrgColor[y][x] = NGfx::GetDWORDColor( v );
		}
	}

	for ( int i = 0; i < pInfo->spots.size(); ++i )
	{
		const STerrainSpot &s = pInfo->spots[i];
		if ( !IsValid( s.pMaterial ) || !IsValid( s.pMaterial->pTexture ) )
			continue;
		float fX = Float2Int( s.ptPos.x * 1024 ) / 1024.0f;
		float fY = Float2Int( s.ptPos.y * 1024 ) / 1024.0f;
		SRotateInfo r( CVec2( fX / FP_GRID_STEP, fY / FP_GRID_STEP ), s.fAngle );
		SDWORDReplaceBlend colorRep( s.pMaterial->pTexture->dwAverageColor );
		Blend( &avrgColor, r, s.ptSize.x * FP_INV_GRID_STEP, s.ptSize.y * FP_INV_GRID_STEP, colorRep );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
REGISTER_SAVELOAD_CLASS( 0xA2852170, CMETerrainLoader );
