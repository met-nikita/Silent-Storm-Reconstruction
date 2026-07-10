#include "StdAfx.h"
#include "DG.h"
#include "GfxBuffers.h"
#include "2DSceneSW.h"
#include "SWTexture.h"
#include "GTerrainTexture.h"
#include "GSceneUtils.h"
#include "Grid.h"
#include "..\Misc\RandomGen.h"
#include "..\DBFormat\DataTerrain.h"
#include "..\DBFormat\DataFormat.h"
#include "GGrass.h"
#include "..\Misc\HPTimer.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool bAllWasReady = true;
static bool bIsStress = false;
static bool bIsLoading = false;
CPtrFuncBase<CSWTextureData>* GetSWTexture( NDb::CTexture *pTex ) 
{
	CSWTexture *pResult = (CSWTexture*)GetSWTex( pTex );
	ASSERT( typeid(*pResult) == typeid(CSWTexture) );
	if ( !pResult->IsReady() )
	{
		bAllWasReady = false;
		return 0;
	}
	return pResult;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetTerrainStressMode( bool bStress )
{
	bIsStress = bStress;
}
void SetTerrainLoadingMode( bool bLoad )
{
	bIsLoading = bLoad;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Tile sorting
struct STerrainTileSort
{
	bool operator()( const NDb::CTerrainTile* psTile1, NDb::CTerrainTile* psTile2 ) const 
	{ 
		return ( psTile1->nPriority > psTile2->nPriority ); 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// terrain texture cache
class CTerrainTexturesCache
{
	struct SHolder
	{
		CObj<NGfx::CTexture> pTexture;
		CPtr<CTerrainTexture> pOwner;
	};
	vector<SHolder> textures;
	int nResolution;
public:
	CTerrainTexturesCache( int _nSize, int _nRes ): nResolution(_nRes) { textures.resize( _nSize ); }
	NGfx::CTexture* GetTexture( CTerrainTexture *_pOwner )
	{
		int nBestMRU = 0x7fffffff, nBest = -1;
		for ( int i = 0; i < textures.size(); ++i )
		{
			const SHolder &test = textures[i];
			if ( !IsValid( test.pTexture ) || !IsValid( test.pOwner ) )
			{
				nBest = i;
				break;
			}
			CDynamicCast<NGfx::I2DBuffer> p2DBuffer( test.pTexture.GetPtr() );
			int nMRU = p2DBuffer->GetFrameMRU();
			if ( nMRU < nBestMRU )
			{
				nBestMRU = nMRU;
				nBest = i;
			}
		}
		ASSERT( nBest >= 0 );
		SHolder &best = textures[nBest];
		if ( IsValid( best.pTexture ) && IsValid( best.pOwner ) )
		{
			best.pOwner->FreeTexture( best.pTexture );
			//best.pTexture = 0;
//			ASSERT( !best.pTexture.IsValid() ); // CRAP
		}
		NGfx::CTexture *pRes;
		if ( !IsValid( best.pTexture ) )
		{
			pRes = NGfx::MakeTexture( nResolution, nResolution, 5, NGfx::SPixel8888::ID, NGfx::REGULAR, NGfx::CLAMP );
			best.pTexture = pRes;
		}
		else
			pRes = best.pTexture;
		CDynamicCast<NGfx::I2DBuffer> pResBuffer( best.pTexture.GetPtr() );
		pResBuffer->UserTouch();
		best.pOwner = _pOwner;
		return pRes;
	}
};
#ifdef _MAPEDIT
static CTerrainTexturesCache cache128( 1000, 128 );
static CTerrainTexturesCache cache256( 70, 256 );
#else
static CTerrainTexturesCache cache128( 300, 128 );
static CTerrainTexturesCache cache256( 70, 256 );
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////
static CObj<NGfx::CTexture> pGreenTexture, pDefaultBump;
static NGfx::CTexture* GetGenericBuffer( CObj<NGfx::CTexture> *pBuf, NGfx::SPixel8888 &color )
{
	if ( IsValid(*pBuf) )
		return *pBuf;
	*pBuf = NGfx::MakeTexture( 1, 1, 1, NGfx::SPixel8888::ID, NGfx::REGULAR, NGfx::CLAMP );
	NGfx::CTextureLock<NGfx::SPixel8888> t( *pBuf, 0, NGfx::INPLACE );
	t[0][0] = color;
	return *pBuf;
}
NGfx::CTexture* GetGreenTexture()
{
	return GetGenericBuffer( &pGreenTexture, NGfx::SPixel8888( 5, 30, 5, 255 ) );
}
NGfx::CTexture* GetDefaultBump()
{
	return GetGenericBuffer( &pDefaultBump, NGfx::SPixel8888( 128, 128, 255, 0 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTerrainTexture
////////////////////////////////////////////////////////////////////////////////////////////////////
//! number of texels per tile variant in texture
const int N_TILE_SIZE = 64;
//! number of texels per one mask variant
const int N_MASK_SIZE = 64;
//! number of texels per game tile
const int N_TEXEL_TILE = 32; 
//! number of masks in tile
const int N_MASKS_PER_TILE = N_TILE_SIZE / N_TEXEL_TILE;
//! number of texels per tile variant in texture
const int N_VSPACE_SIZE = 256;
//!
const float FP_BUMPMAPPINGSIZE = 2;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Terrain texture
////////////////////////////////////////////////////////////////////////////////////////////////////
CTerrainTexture::CTerrainTexture( bool _bBump, SRandomSeed _sSeed, const CTRect<int> &_nrRegion, 
	CFuncBase<int> *_pLOD, CFuncBase<STerrainInfo> *_pInfo, CVersioningBase *_pUpdateRegion, CGrassTracker *_pGrass )
	: bBumpTexture(_bBump), sSeed(_sSeed), nrRegion(_nrRegion), pLOD(_pLOD), pInfo(_pInfo), pUpdateRegion(_pUpdateRegion),
	pGrass(_pGrass), fWorldToScreen( FP_INV_GRID_STEP / nrRegion.Width() * N_VSPACE_SIZE )
{
	ASSERT( nrRegion.Width() == nrRegion.Height() ); // used in mapping world to screen
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec2 CTerrainTexture::GetScreenCoords( const CVec2 &ptWorld )
{
	return CVec2( 
		( ptWorld.x - nrRegion.x1 * FP_GRID_STEP ) * fWorldToScreen,
		( ptWorld.y - nrRegion.y1 * FP_GRID_STEP ) * fWorldToScreen );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec2 CTerrainTexture::GetScreenSize( const CVec2 &ptWorld )
{
	return CVec2( 
		ptWorld.x * fWorldToScreen,
		ptWorld.y * fWorldToScreen );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static CObj<CBilinearTexture> pBlackTexture, pZeroBump;
static CBilinearTexture* GetBlackTexture()
{
	if ( IsValid( pBlackTexture ) )
		return pBlackTexture;
	CArray2D<NGfx::SPixel8888> black;
	black.SetSizes( 2, 2 );
	black.FillZero();
	pBlackTexture = new CBilinearTexture( black, 1, 1 );
	return pBlackTexture;
}
static CBilinearTexture* GetZeroBumpTexture()
{
	if ( IsValid( pZeroBump ) )
		return pZeroBump;
	CArray2D<NGfx::SPixel8888> zero;
	zero.SetSizes( 2, 2 );
	NGfx::SPixel8888 z;
	z.r = 128; z.g = 128; z.b = 255; z.a = 0;
	zero.FillEvery( z );
	pZeroBump = new CBilinearTexture( zero, 1, 1 );
	return pZeroBump;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainTexture::CreateColorMask( ISW2DScene *p2DScene, int nSize )
{
	const STerrainInfo &info = pInfo->GetValue();
	CArray2D<NGfx::SPixel8888> pix;
	pix.SetSizes( nrRegion.Width() + 1, nrRegion.Height() + 1 );
	for ( int y = 0; y < pix.GetYSize(); ++y )
	{
		for ( int x = 0; x < pix.GetXSize(); ++x )
		{
			pix[y][x].color = ~info.color[ y + nrRegion.y1 ][ x + nrRegion.x1 ];
			//if ( ( x + y ) & 1 ) 	pix[y][x].color = 0xffffffff; else pix[y][x].color = 0;
		}
	}
	CBilinearTexture *pTex = GetBlackTexture();
	CBilinearTexture *pMaskTex = new CBilinearTexture( pix, nSize, nSize );
	CSWRectLayout rLayout;
	CSWRectLayout::STextureCoord sTex( CTRect<short>( 0, 0, N_VSPACE_SIZE, N_VSPACE_SIZE ), CSWRectLayout::NORMAL );
	CSWRectLayout::STextureCoord sMask( CTRect<short>( 0, 0, nSize, nSize ), CSWRectLayout::NORMAL );
	rLayout.AddRect( 0, 0, sTex, sMask );
	p2DScene->CreateRects( pTex, pMaskTex, new CCSWRectLayout( rLayout ) );
	//p2DScene->CreateRects( pTex, new CCSWRectLayout( rLayout ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrainTexture::GetSpotTextures( NDb::CMaterial *pMat, SSpotTextures *pRes )
{
	if ( !IsValid( pMat ) || !IsValid( pMat->pTexture ) )
	{
		pRes->pTex = 0;
		pRes->pBump = 0;
		return false;
	}
	pRes->pTex = GetSWTexture( pMat->pTexture );
	if ( bBumpTexture && pMat->pBump )
		pRes->pBump = GetSWTexture( pMat->pBump );
	else
		pRes->pBump = 0;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainTexture::DrawSpot( ISW2DScene *p2DScene, const SSpotTextures &textures,
	const CVec2 &ptPos, const CVec2 &ptSize, float fAngle )
{
	CPtrFuncBase<CSWTextureData> *pTex = textures.pTex, *pBump = textures.pBump;
	if ( bBumpTexture )
	{
		if ( pBump )
			p2DScene->CreateSpot( pBump, pTex, GetScreenCoords(ptPos), GetScreenSize(ptSize), fAngle );
		else
			p2DScene->CreateSpot( GetZeroBumpTexture(), pTex, GetScreenCoords(ptPos), GetScreenSize(ptSize), fAngle );
	}
	else
		p2DScene->CreateSpot( pTex, 0, GetScreenCoords(ptPos), GetScreenSize(ptSize), fAngle );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGrassSpotInfo
{
	NDb::CMaterial *pMat;
	float fScale;
	float fScaleRange;
	SGrassSpotInfo( NDb::CMaterial *_pMat, float _fScale, float _fScaleRange )
		: pMat(_pMat), fScale(_fScale), fScaleRange(_fScaleRange) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline float GetFrac( float f ) { int n = (int)f; return f - n; }
void CTerrainTexture::DrawGrassSpots( ISW2DScene *p2DScene )
{
	int nGrassLayers = pGrass->GetNumLayers();
	int nXSize = pGrass->GetNumSectorsX();
	int nYSize = pGrass->GetNumSectorsY();
	vector<SGrassSpotInfo> layerInfo;
	for ( int k = 0; k < nGrassLayers; ++k )
	{
		NDb::CGrass *pDBGrass = pGrass->GetGrass( k );
		layerInfo.push_back( SGrassSpotInfo( pGrass->GetSpotMaterial(k), pDBGrass->fSpotScale, pDBGrass->fScaleRange ) );
	}
	for ( int nY = 0; nY < nYSize; ++nY )
	{
		int nYStart = nY * N_GRASS_SECTOR_SIZE, nYFinish = nYStart + N_GRASS_SECTOR_SIZE;
		if ( nYStart > nrRegion.y2 + 1 )
			continue;
		if ( nYFinish < nrRegion.y1 - 1 )
			continue;
		for ( int nX = 0; nX < nXSize; ++nX )
		{
			int nXStart = nX * N_GRASS_SECTOR_SIZE, nXFinish = nXStart + N_GRASS_SECTOR_SIZE;
			if ( nXStart > nrRegion.x2 + 1 )
				continue;
			if ( nXFinish < nrRegion.x1 - 1 )
				continue;
			for ( int k = 0; k < nGrassLayers; ++k )
			{
				CDGPtr<CPtrFuncBase<CGrassPosition> > pCalc;
				const SGrassSpotInfo &info = layerInfo[k];
				SSpotTextures spotTextures;
				if ( !GetSpotTextures( info.pMat, &spotTextures ) )
					continue;
				pCalc = pGrass->GetGrassPosCalcer( k, nX, nY );
				if ( pCalc )
				{
					pCalc.Refresh();
					const CGrassPosition &gp = *pCalc->GetValue();
					for ( int i = 0; i < gp.positions.size(); ++i )
					{
						CVec2 pos( gp.positions[i].x, gp.positions[i].y );
						float fAngle = GetFrac( ( pos.x + pos.y ) * 10 ) * FP_2PI;
						float fCos = cos( fAngle ), fSin = sin( fAngle );
						float fScale = info.fScale * (1 - GetPseudoRandomForBlade(i) * info.fScaleRange);
						pos.x -= fCos * 0.5f * fScale;
						pos.y -= fSin * 0.5f * fScale;
						pos.x += fSin * 0.5f * fScale;
						pos.y -= fCos * 0.5f * fScale;
						DrawSpot( p2DScene, spotTextures, pos, CVec2( fScale, fScale ), fAngle );
					}
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainTexture::DrawSpots( ISW2DScene *p2DScene, const STerrainInfo &info )
{
	SSpotTextures spotTextures;
	NDb::CMaterial *pMat = 0;
	for ( int k = 0; k < info.spots.size(); ++k )
	{
		const STerrainSpot &s = info.spots[k];
		if ( pMat != s.pMaterial )
		{
			GetSpotTextures( s.pMaterial, &spotTextures );
			pMat = s.pMaterial;
		}
		if ( spotTextures.pTex )
			DrawSpot( p2DScene, spotTextures, s.ptPos, s.ptSize, s.fAngle );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrainTexture::CalcNewTexture( int nSize )
{
	bAllWasReady = true;

	pInfo.Refresh();
	const STerrainInfo *ptiInfo = &pInfo->GetValue();

	vector<NDb::CTerrainTile*> tilesList;
	{
		CDBTable<NDb::CTerrainTile> *pTileTable = NDatabase::GetTable<NDb::CTerrainTile>();
		CDBIterator<NDb::CTerrainTile> iTempTile( *pTileTable );
		while( iTempTile.MoveNext() )
		{
			NDb::CTerrainTile *pTile = iTempTile.Get();
			if ( pTile->pMask == 0 || ( bBumpTexture && pTile->pBump == 0 ) || ( !bBumpTexture && pTile->pTexture == 0 ) )
				continue;
			tilesList.push_back( pTile );
		}
		sort( tilesList.begin(), tilesList.end(), STerrainTileSort() );
	}

	CPtr<ISW2DScene> p2DScene = Make2DSWScene();

	CArray2D<bool> tileMap;
	tileMap.SetSizes( nrRegion.Width(), nrRegion.Height() );
	tileMap.FillZero();

	SRand sRandom( sSeed );
	vector<int> vRandomMasks( nrRegion.Width() );
	vector<int> vRandomTextures( nrRegion.Width() );
	vector<CSWRectLayout::ERectOrient> vRandomTextureOrients( nrRegion.Width() );

	//for ( unordered_map<int, NDb::CTerrainTile*>::const_iterator iTempTile = tiles.begin(); iTempTile != tiles.end(); iTempTile++ )
	for ( vector<NDb::CTerrainTile*>::const_iterator iTempTile = tilesList.begin(); iTempTile != tilesList.end(); iTempTile++ )
	{
		NDb::CTerrainTile *pTile = *iTempTile;
		int nTileIndex = pTile->GetRecordID();
		int nRandMaskVariant = 0;
		int nRandTextureVariant = 0;
		CSWRectLayout rLayout, rLayoutMasked;
		CSWRectLayout::ERectOrient eTextureOrient = CSWRectLayout::NORMAL;
				
		for ( int nTempY = 0; nTempY < nrRegion.Height(); nTempY++ )
		{
			if ( ( nTempY & (N_MASKS_PER_TILE-1) ) == 0 )
			{
				for ( int nTempX = 0; nTempX < nrRegion.Width(); nTempX++ )
				{
					vRandomMasks[nTempX] = sRandom.Get( pTile->nMaskVariants );
					vRandomTextures[nTempX] = sRandom.Get( pTile->nTextureVariants );

					switch( sRandom.Get( 3 ) )
					{
						case 1:
							vRandomTextureOrients[nTempX] = CSWRectLayout::ROTATE_90;
							break;
						case 2:
							vRandomTextureOrients[nTempX] = CSWRectLayout::ROTATE_180;
							break;
						case 3:
							vRandomTextureOrients[nTempX] = CSWRectLayout::ROTATE_270;
							break;
						default:
							vRandomTextureOrients[nTempX] = CSWRectLayout::NORMAL;
							break;
					}
				}
				
			}
				
			for ( int nTempX = 0; nTempX < nrRegion.Width(); nTempX++ )
			{
				DWORD dwZone = 0;
				
				dwZone |= (ptiInfo->typeMap[nrRegion.y1 + nTempY    ][nrRegion.x1 + nTempX    ] == nTileIndex);
				dwZone |= (ptiInfo->typeMap[nrRegion.y1 + nTempY    ][nrRegion.x1 + nTempX + 1] == nTileIndex) << 1;
				dwZone |= (ptiInfo->typeMap[nrRegion.y1 + nTempY + 1][nrRegion.x1 + nTempX + 1] == nTileIndex) << 2;
				dwZone |= (ptiInfo->typeMap[nrRegion.y1 + nTempY + 1][nrRegion.x1 + nTempX    ] == nTileIndex) << 3;

				if ( ( nTempX & (N_MASKS_PER_TILE-1) ) == 0 )
				{
					nRandMaskVariant = vRandomMasks[nTempX];
					nRandTextureVariant = vRandomTextures[nTempX];
					eTextureOrient = vRandomTextureOrients[nTempX];
				}
				volatile int nScreenX = nTempX * N_TEXEL_TILE;
				volatile int nScreenY = nTempY * N_TEXEL_TILE;
				volatile int nTexOffsetX = 0;
				volatile int nTexOffsetY = 0;
				volatile int nMaskOffsetX = nRandMaskVariant * N_MASK_SIZE;
				volatile int nMaskOffsetY = 0;

				switch( eTextureOrient )
				{
				case CSWRectLayout::ROTATE_90:
					nTexOffsetX = ( ( nrRegion.Height() - nTempY - 1 ) & (N_MASKS_PER_TILE-1) ) * N_TEXEL_TILE + nRandTextureVariant * N_TILE_SIZE;
					nTexOffsetY = ( nTempX & (N_MASKS_PER_TILE-1) ) * N_TEXEL_TILE;
					break;
				case CSWRectLayout::ROTATE_180:
					nTexOffsetX = ( ( nrRegion.Width() - nTempX - 1 ) & (N_MASKS_PER_TILE-1) ) * N_TEXEL_TILE + nRandTextureVariant * N_TILE_SIZE;
					nTexOffsetY = ( ( nrRegion.Height() - nTempY - 1 ) & (N_MASKS_PER_TILE-1) ) * N_TEXEL_TILE;
					break;
				case CSWRectLayout::ROTATE_270:
					nTexOffsetX = ( nTempY & (N_MASKS_PER_TILE-1) ) * N_TEXEL_TILE + nRandTextureVariant * N_TILE_SIZE;
					nTexOffsetY = ( ( nrRegion.Width() - nTempX - 1 ) & (N_MASKS_PER_TILE-1) ) * N_TEXEL_TILE;
					break;
				default: // normal
					nTexOffsetX = ( nTempX & (N_MASKS_PER_TILE-1) ) * N_TEXEL_TILE + nRandTextureVariant * N_TILE_SIZE;
					nTexOffsetY = ( nTempY & (N_MASKS_PER_TILE-1) ) * N_TEXEL_TILE;
					break;
				}

				CSWRectLayout::STextureCoord sTex( CTRect<short>( nTexOffsetX, nTexOffsetY, nTexOffsetX + N_TEXEL_TILE, nTexOffsetY + N_TEXEL_TILE ), eTextureOrient );

				if ( tileMap[nTempY][nTempX] == false )
				{
					if ( dwZone )
					{
						rLayout.AddRect( nScreenX, nScreenY, sTex );
						tileMap[nTempY][nTempX] = true;
					}
				}
				else
				{
					switch ( dwZone )
					{
// ,---,
// |*  |
// |___|

						case 1:
							{
								CSWRectLayout::STextureCoord sMask( CTRect<short>( 0 + nMaskOffsetX, N_MASK_SIZE*3, N_MASK_SIZE + nMaskOffsetX, N_MASK_SIZE*4 ), CSWRectLayout::ROTATE_270 );
								rLayoutMasked.AddRect( nScreenX, nScreenY, sTex, sMask );
								break;
							}
						case 2:
							{
								CSWRectLayout::STextureCoord sMask( CTRect<short>( 0 + nMaskOffsetX, N_MASK_SIZE*3, N_MASK_SIZE + nMaskOffsetX, N_MASK_SIZE*4 ), CSWRectLayout::ROTATE_180 );
								rLayoutMasked.AddRect( nScreenX, nScreenY, sTex, sMask );
								break;
							}
						case 4:
							{
								CSWRectLayout::STextureCoord sMask( CTRect<short>( 0 + nMaskOffsetX, N_MASK_SIZE*3, N_MASK_SIZE + nMaskOffsetX, N_MASK_SIZE*4 ), CSWRectLayout::ROTATE_90 );
								rLayoutMasked.AddRect( nScreenX, nScreenY, sTex, sMask );
								break;
							}
						case 8:
							{
								CSWRectLayout::STextureCoord sMask( CTRect<short>( 0 + nMaskOffsetX, N_MASK_SIZE*3, N_MASK_SIZE + nMaskOffsetX, N_MASK_SIZE*4 ), CSWRectLayout::NORMAL );
								rLayoutMasked.AddRect( nScreenX, nScreenY, sTex, sMask );
								break;
							}


// ,---,
// |* *|
// |___|
						case 3:
							{
								CSWRectLayout::STextureCoord sMask( CTRect<short>( 0 + nMaskOffsetX, N_MASK_SIZE*2, N_MASK_SIZE + nMaskOffsetX, N_MASK_SIZE*3 ), CSWRectLayout::ROTATE_180 );
								rLayoutMasked.AddRect( nScreenX, nScreenY, sTex, sMask );
								break;
							}
						case 6:
							{
								CSWRectLayout::STextureCoord sMask( CTRect<short>( 0 + nMaskOffsetX, N_MASK_SIZE*2, N_MASK_SIZE + nMaskOffsetX, N_MASK_SIZE*3 ), CSWRectLayout::ROTATE_90 );
								rLayoutMasked.AddRect( nScreenX, nScreenY, sTex, sMask );
								break;
							}
						case 12:
							{
								CSWRectLayout::STextureCoord sMask( CTRect<short>( 0 + nMaskOffsetX, N_MASK_SIZE*2, N_MASK_SIZE + nMaskOffsetX, N_MASK_SIZE*3 ), CSWRectLayout::NORMAL );
								rLayoutMasked.AddRect( nScreenX, nScreenY, sTex, sMask );
								break;
							}
						case 9:
							{
								CSWRectLayout::STextureCoord sMask( CTRect<short>( 0 + nMaskOffsetX, N_MASK_SIZE*2, N_MASK_SIZE + nMaskOffsetX, N_MASK_SIZE*3 ), CSWRectLayout::ROTATE_270 );
								rLayoutMasked.AddRect( nScreenX, nScreenY, sTex, sMask );
								break;
							}

// ,---,
// |*  |
// |__*|
						case 5:
							{
								CSWRectLayout::STextureCoord sMask( CTRect<short>( 0 + nMaskOffsetX, 0, N_MASK_SIZE + nMaskOffsetX, N_MASK_SIZE ), CSWRectLayout::ROTATE_180 );
								rLayoutMasked.AddRect( nScreenX, nScreenY, sTex, sMask );
								break;
							}
						case 10:
							{
								CSWRectLayout::STextureCoord sMask( CTRect<short>( 0 + nMaskOffsetX, 0, N_MASK_SIZE + nMaskOffsetX, N_MASK_SIZE ), CSWRectLayout::ROTATE_90 );
								rLayoutMasked.AddRect( nScreenX, nScreenY, sTex, sMask );
								break;
							}

// ,---,
// |***|
// |__*|
						case 7:
							{
								CSWRectLayout::STextureCoord sMask( CTRect<short>( 0 + nMaskOffsetX, N_MASK_SIZE, N_MASK_SIZE + nMaskOffsetX, N_MASK_SIZE*2 ), CSWRectLayout::ROTATE_180 );
								rLayoutMasked.AddRect( nScreenX, nScreenY, sTex, sMask );
								break;
							}
						case 14:
							{
								CSWRectLayout::STextureCoord sMask( CTRect<short>( 0 + nMaskOffsetX, N_MASK_SIZE, N_MASK_SIZE + nMaskOffsetX, N_MASK_SIZE*2 ), CSWRectLayout::ROTATE_90 );
								rLayoutMasked.AddRect( nScreenX, nScreenY, sTex, sMask );
								break;
							}
						case 13:
							{
								CSWRectLayout::STextureCoord sMask( CTRect<short>( 0 + nMaskOffsetX, N_MASK_SIZE, N_MASK_SIZE + nMaskOffsetX, N_MASK_SIZE*2 ), CSWRectLayout::NORMAL );
								rLayoutMasked.AddRect( nScreenX, nScreenY, sTex, sMask );
								break;
							}
						case 11:
							{
								CSWRectLayout::STextureCoord sMask( CTRect<short>( 0 + nMaskOffsetX, N_MASK_SIZE, N_MASK_SIZE + nMaskOffsetX, N_MASK_SIZE*2 ), CSWRectLayout::ROTATE_270 );
								rLayoutMasked.AddRect( nScreenX, nScreenY, sTex, sMask );
								break;
							}

						default:
							break;
					}
				}
			}
		}

		if ( !rLayout.rects.empty() )
		{
			CCSWRectLayout *prLayout = new CCSWRectLayout( rLayout );
			
			if ( !bBumpTexture )
				p2DScene->CreateRects( GetSWTexture( pTile->pTexture ), prLayout );
			else
				p2DScene->CreateRects( GetSWTexture( pTile->pBump ), prLayout );
		}
		if ( !rLayoutMasked.rects.empty() )
		{
			CCSWRectLayout *prLayout = new CCSWRectLayout( rLayoutMasked );

			if ( !bBumpTexture )
				p2DScene->CreateRects( GetSWTexture( pTile->pTexture ), GetSWTexture( pTile->pMask ), prLayout );
			else
				p2DScene->CreateRects( GetSWTexture( pTile->pBump ), GetSWTexture( pTile->pMask ), prLayout );
		}
	}
	DrawSpots( p2DScene, *ptiInfo );
	DrawGrassSpots( p2DScene );
	if ( !bBumpTexture )
		CreateColorMask( p2DScene, nSize );//nSize < 256 ? nSize / 4 : nSize );
	
	if ( bAllWasReady )
	{
		// create texture buffer
		if ( !IsValid(pValue) )
		{
			if ( nSize == 256 )
			{
				pValue = cache256.GetTexture( this );
				pTex256 = pValue;
			}
			else
			{
				pValue = cache128.GetTexture( this );
				pTex128 = pValue;
			}
		}
		if ( !IsValid( pValue ) )
			return false;
		if ( !bBumpTexture )
			p2DScene->Draw( pValue, CTPoint<int>( N_VSPACE_SIZE, N_VSPACE_SIZE ) );
		else
			p2DScene->DrawBump( pValue, CTPoint<int>( N_VSPACE_SIZE, N_VSPACE_SIZE ), N_VSPACE_SIZE / ( FP_BUMPMAPPINGSIZE * 512 ) );
		return true;
	}
	else
		return false;
	//
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainTexture::UseFake()
{
	pValue = pTex128;
	nDetail = 1;
	if ( !IsValid( pValue ) )
	{
		if ( bBumpTexture )
			pValue = GetDefaultBump();
		else
		{
			pInfo.Refresh();
			const STerrainInfo &info = pInfo->GetValue();
			if ( nrRegion.x2 <= info.avrgColor.GetXSize() && nrRegion.y2 <= info.avrgColor.GetYSize() )
			{
				pValue = NGfx::MakeTexture( nrRegion.Width(), nrRegion.Height(), 1, NGfx::SPixel8888::ID, NGfx::REGULAR, NGfx::CLAMP );
				NGfx::CTextureLock<NGfx::SPixel8888> t( pValue, 0, NGfx::INPLACE );
				for ( int y = 0; y < nrRegion.Height(); ++y )
				{
					for ( int x = 0; x < nrRegion.Width(); ++x )
						t[y][x].color = info.avrgColor[ y + nrRegion.y1 ][ x + nrRegion.x1 ];
				}
			}
			else
				pValue = GetGreenTexture();
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_RELAXATION_TIME = 0.5f;
#ifdef _MAPEDIT
const float F_LIMIT_TIME_RECALC = 0.2f;
#else
const float F_LIMIT_TIME_RECALC = 0.02f;
#endif
static int nPrevDGFrame;
static float fElapsedTime;
void CTerrainTexture::Recalc()
{
	ASSERT( nDetail == pLOD->GetValue() );
	bool bFastRecalc = false, bDoRecalc = true;
	NHPTimer::STime tLast;
	NHPTimer::GetTime( &tLast );
	if ( nDGCurrentFrame != nPrevDGFrame )
	{
		nPrevDGFrame = nDGCurrentFrame;
		fElapsedTime = 0;
	}
	if ( !bIsLoading && ( fElapsedTime > F_LIMIT_TIME_RECALC || ( bIsStress && bBumpTexture ) || HasFileRequestsInFly() ) )
	{
		// out of time, get some result anywhere
		UseFake();
		return;
	}
	if ( bIsStress )
	{
		pValue = pTex128;
		nDetail = 1;
		if ( IsValid( pValue ) )
			return;
	}
	if ( nDetail == 0 )
		bDoRecalc = bHasToRecalc256;
	else
		bDoRecalc = bHasToRecalc128;
	int nSize = nDetail == 0 ? 256 : 128;
	ASSERT( bDoRecalc );

	// do one of the recalcs
	if ( bDoRecalc )//!IsValid( pValue ) )
	{
		if ( CalcNewTexture( nSize ) )
		{
			if ( nDetail == 0 )
				bHasToRecalc256 = false;
			else
				bHasToRecalc128 = false;
		}
		else
		{
			UseFake();
			fElapsedTime += 0.1f;
		}
	}
	float fTimePassed = NHPTimer::GetTimePassed( &tLast );
	//char szBuf[1000];
	//int nBump = bBumpTexture;
	//sprintf( szBuf, "%d size bump=%d %g ms per calc\n", nSize, nBump, fTimePassed * 1000 );
	//OutputDebugString( szBuf );
	fElapsedTime += fTimePassed;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrainTexture::NeedUpdate()
{
	// pLOD (the camera-bound CLODCalcer) is intentionally excluded from the save format (operator&
	// skips tag 5), so a terrain texture RESTORED FROM A SAVE has a null pLOD; only fresh map-gen
	// supplies one (GView.cpp). The release's base CTerrainTexture::NeedUpdate (@0x17b060) does not
	// touch pLOD at all -- guard it so a loaded texture keeps its persisted nDetail (tag 10) instead
	// of dereferencing the null LOD node (which crashed on load in PrecacheMaterials -> DG.H Refresh).
	if ( IsValid( pLOD ) )
	{
		pLOD.Refresh();
		nDetail = pLOD->GetValue();
	}
	pInfo.Refresh();
	if ( pUpdateRegion.Refresh() )
	{
		bHasToRecalc256 = true;
		bHasToRecalc128 = true;
	}
	bool bHasToRecalc = true;
	if ( nDetail == 0 )
	{
		pValue = pTex256;
		if ( !IsValid(pValue) )
			bHasToRecalc256 = true;
		bHasToRecalc = bHasToRecalc256;
	}
	else
	{
		pValue = pTex128;
		if ( !IsValid(pValue) )
			bHasToRecalc128 = true;
		bHasToRecalc = bHasToRecalc128;
	}
	return bHasToRecalc;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainTexture::FreeTexture( NGfx::CTexture *_pTex )
{
	if ( _pTex == pTex128 )
	{
		pTex128 = 0;
		bHasToRecalc128 = true;
	}
	if ( _pTex == pTex256 )
	{
		pTex256 = 0;
		bHasToRecalc256 = true;
	}
	if ( _pTex == pValue )
		pValue = 0;
}	
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x18051160, CTerrainTexture );
