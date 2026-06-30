#include "StdAfx.h"
#include "GGrass.h"
#include "Grid.h"
#include "Transform.h"
#include "aiMap.h"
#include "aiRender.h"
#include "wTSFlags.h"
#include "GParticleInfo.h"
#include "..\Misc\randomGen.h"
#include "..\DBFormat\DataTerrain.h"
#include "..\DBFormat\DataFormat.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
const float WAVE_GRID_SIZE = FP_GRID_STEP;
const int N_SAMPLES_PER_TILE = 4;//16;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGrassPosCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGrassPosCalcer::NeedUpdate() 
{ 
	bool bUpd = IsValid( pSectorUpdate ) ? pSectorUpdate.Refresh() : false;
	return pInfo.Refresh() || bUpd;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrassPosCalcer::Recalc()
{
	const STerrainInfo &info = pInfo->GetValue();
	const SGrassLayer &layer = info.grass[nOriginLayer];
	pValue = new CGrassPosition;
	int nXBeg = nX * N_GRASS_SECTOR_SIZE;
	int nYBeg = nY * N_GRASS_SECTOR_SIZE;
	int nXTo = Min( nXBeg + N_GRASS_SECTOR_SIZE, layer.grass.GetXSize() - 1 );
	int nYTo = Min( nYBeg + N_GRASS_SECTOR_SIZE, layer.grass.GetYSize() - 1 );

	NAI::CFastRenderer res;
	res.InitParallel( CVec2( nXBeg * FP_GRID_STEP, nYBeg * FP_GRID_STEP ), 0, FP_GRID_STEP / N_SAMPLES_PER_TILE, CTRect<int>( 0, 0, N_GRASS_SECTOR_SIZE * N_SAMPLES_PER_TILE, N_GRASS_SECTOR_SIZE * N_SAMPLES_PER_TILE ) );
	pAIMap->TraceGrid( &res, NWorld::TS_TERRAINS );

	SRand rnd( (nOriginLayer << 24) ^ (nYBeg << 14) ^ nXBeg );
	for ( int j = nYBeg; j < nYTo; ++j )
		for ( int i = nXBeg; i < nXTo; ++i )
		{
			float lb = layer.grass[j][i] / 255.f;
			float lt = layer.grass[j+1][i] / 255.f;
			float rb = layer.grass[j][i+1] / 255.f;
			float rt = layer.grass[j+1][i+1] / 255.f;
			/*
			float lbh = info.heightMap[j][i] * FP_TERRAIN_H_SCALE;
			float lth = info.heightMap[j+1][i] * FP_TERRAIN_H_SCALE;
			float rbh = info.heightMap[j][i+1] * FP_TERRAIN_H_SCALE;
			float rth = info.heightMap[j+1][i+1] * FP_TERRAIN_H_SCALE;
			*/
			int nParticles = Float2Int((lb + lt + rb + rt) / 4 * layer.fMaxDensity);
			for ( int nP = 0; nP < nParticles; ++nP )
			{
				int nSamples = 64;
				float x, y, prob = 0, sample = 1;
				while ( sample > prob && --nSamples )
				{
					x = rnd.GetFloat( 0, 1 );
					y = rnd.GetFloat( 0, 1 );
					sample = rnd.GetFloat( 0, 1 );
					prob = (1-x) * (1-y) * lb + x * (1-y) * rb + (1-x) * y * lt + x * y * rt;
				}
				CVec3 pos;
				pos.x = (i + x) * FP_GRID_STEP;
				pos.y = (j + y) * FP_GRID_STEP;

				vector<CVec3> enters, exits;
				res.GetPoints( &enters, &exits, ( i + x - nXBeg ) * N_SAMPLES_PER_TILE, ( j + y - nYBeg ) * N_SAMPLES_PER_TILE );
				if ( !exits.empty() )
				{
					pos.z = exits.front().z;//(1-x) * (1-y) * lbh + x * (1-y) * rbh + (1-x) * y * lth + x * y * rth;// + scale.y * 0.5f;
					pValue->positions.push_back( pos );
				}
			}
		}
	for ( int i = 0; i < layer.blades.size(); ++i )
	{
		const CVec2 &blade = layer.blades[i];
		if ( blade.x < nXBeg || blade.x >= nXTo || blade.y < nYBeg || blade.y >= nYTo )
			continue;
		CVec3 pos;
		pos.x = blade.x * FP_GRID_STEP;
		pos.y = blade.y * FP_GRID_STEP;
		vector<CVec3> enters, exits;
		res.GetPoints( &enters, &exits, ( blade.x - nXBeg ) * N_SAMPLES_PER_TILE, ( blade.y - nYBeg ) * N_SAMPLES_PER_TILE );
		if ( !exits.empty() )
		{
			pos.z = exits.front().z;
			pValue->positions.push_back( pos );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CParticleWaveTexture
////////////////////////////////////////////////////////////////////////////////////////////////////
void CParticleWaveTexture::ShiftTime()
{
	for ( int j = 0; j < times.GetYSize(); ++j )
		for ( int i = 0; i < times.GetXSize(); ++i )
			times[j][i] = (times[j][i] < 128) ? 0 : times[j][i] - 128;
	nCurTime -= 128;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CParticleWaveTexture::Wave( int nX, int nY )
{
	if ( nX < 0 || nX > times.GetXSize() - 1 || nY < 0 || nY > times.GetYSize() - 1 )
		return;
	times[nY][nX] = (unsigned char)nCurTime;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CParticleWaveTexture::GetAmplitude( float x, float y )
{
	int nX = Float2Int(x), nY = Float2Int(y);
	if ( nX < 0 || nX > times.GetXSize() - 1 || nY < 0 || nY > times.GetYSize() - 1 )
		return 0;
	if ( !times[nY][nX] )
		return 0;
	return 0.15f * (1 - (nCurTime - times[nY][nX]) * (1.f / 128.f) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CParticleWaveTexture::UpdateTime( float fNewTime )
{
	float fStepTime = 0.02f;
	int nSteps = (int)((fNewTime - fCurTime) / fStepTime);
	nCurTime += nSteps;
	while ( nCurTime > 255 )
		ShiftTime();
	fCurTime += nSteps * fStepTime;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGrassTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
CGrassTracker::CGrassTracker( CTerrainInfoHolder *pInfoHolder, NAI::IAIMap *_pAIMap )
: pInfo(pInfoHolder)
{
	int nXSize = GetNumSectorsX();
	int nYSize = GetNumSectorsY();
	waves.SetSizes( nXSize * N_GRASS_SECTOR_SIZE + 1, nYSize * N_GRASS_SECTOR_SIZE + 1 );
	pInfo.Refresh();
	const STerrainInfo &info = pInfo->GetValue();
	layers.resize( info.grass.size() );
	int nRes = 0;
	SRand rnd;
	for ( int i = 0; i < info.grass.size(); ++i )
	{
		NDb::CGrass *pGrass = NDb::GetGrass( info.grass[i].nGrassID );
		if ( !IsValid( pGrass ) )
			continue;
		SGrassLayerInfo &res = layers[nRes++];
		res.grass.SetSizes( nXSize, nYSize );
		res.nOriginLayer = i;
		res.pGrass = pGrass;
		if ( pGrass->pSpotMaterial )
			res.pSpotMaterial = pGrass->pSpotMaterial->GetMaterial( &rnd );
		for ( int y = 0; y < nYSize; ++y )
		{
			for ( int x = 0; x < nXSize; ++x )
			{
#ifndef _MAPEDIT
				if ( IsSectorFilled( nRes - 1, x, y ) )
#endif
				{
					CTRect<int> sector;
					sector.minx = x * N_GRASS_SECTOR_SIZE;
					sector.miny = y * N_GRASS_SECTOR_SIZE;
					sector.maxx = sector.minx + N_GRASS_SECTOR_SIZE;
					sector.maxy = sector.miny + N_GRASS_SECTOR_SIZE;
					res.grass[y][x] = new CGrassPosCalcer( i, x, y, _pAIMap, pInfo, pInfoHolder->GetRegionGrass( sector ) );
				}
			}
		}
	}
	layers.resize( nRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPtrFuncBase<CGrassPosition>* CGrassTracker::GetGrassPosCalcer( int nLayer, int nX, int nY )
{
	if ( nLayer < 0 || nLayer >= layers.size() )
		return 0;
	const SGrassLayerInfo &l = layers[nLayer];
	if ( nX < 0 || nY < 0 || nX >= l.grass.GetXSize() || nY >= l.grass.GetYSize() )
		return 0;
	return l.grass[nY][nX];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CGrassTracker::GetNumLayers()
{
	return layers.size();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CGrassTracker::GetNumSectorsX()
{
	pInfo.Refresh();
	int nTiles = pInfo->GetValue().nWidth - 1;
	if ( nTiles % N_GRASS_SECTOR_SIZE )
		return nTiles / N_GRASS_SECTOR_SIZE + 1;
	else
		return nTiles / N_GRASS_SECTOR_SIZE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CGrassTracker::GetNumSectorsY()
{
	pInfo.Refresh();
	int nTiles = pInfo->GetValue().nHeight - 1;
	if ( nTiles % N_GRASS_SECTOR_SIZE )
		return nTiles / N_GRASS_SECTOR_SIZE + 1;
	else
		return nTiles / N_GRASS_SECTOR_SIZE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CGrassTracker::GetTextureLayerID( int nLayer )
{
	NDb::CGrass *pGrass = layers[nLayer].pGrass;
	if ( !pGrass || !IsValid( pGrass->pTexture ) )
		return -1;
	return pGrass->pTexture->GetRecordID();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGrassTracker::IsSectorFilled( int nLayer, int nX, int nY )
{
	const SGrassLayer &layer = GetGrassLayer( nLayer );
	nX *= N_GRASS_SECTOR_SIZE;
	nY *= N_GRASS_SECTOR_SIZE;
	int nXTo = Min( nX + N_GRASS_SECTOR_SIZE + 1, layer.grass.GetXSize() );
	int nYTo = Min( nY + N_GRASS_SECTOR_SIZE + 1, layer.grass.GetYSize() );	
	for ( int j = nY; j < nYTo; ++j )
		for ( int i = nX; i < nXTo; ++i )
		{
			if ( layer.grass[j][i] != 0 )
				return true;
		}
	for ( int i = 0; i < layer.blades.size(); ++i )
	{
		const CVec2 &blade = layer.blades[i];
		if ( blade.x >= nX && blade.x < nXTo && blade.y >= nY && blade.y < nYTo )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrassTracker::GetSectorBound( int nLayer, int nX, int nY, SBound *pBound )
{
	pInfo.Refresh();
	const STerrainInfo &info = pInfo->GetValue();
	nX *= N_GRASS_SECTOR_SIZE;
	nY *= N_GRASS_SECTOR_SIZE;
	unsigned short nMin = 0xFFFF, nMax = 0;
	int nXTo = Min( nX + N_GRASS_SECTOR_SIZE + 1, info.heightMap.GetXSize() );
	int nYTo = Min( nY + N_GRASS_SECTOR_SIZE + 1, info.heightMap.GetYSize() );	
	for ( int j = nY; j < nYTo; ++j )
		for ( int i = nX; i < nXTo; ++i )
		{
			short nCur = info.heightMap[j][i];
			if ( nCur < nMin )
				nMin = nCur;
			if ( nCur > nMax )
				nMax = nCur;
		}
	CVec3 ptMin( - N_GRASS_SECTOR_SIZE / 2 * FP_GRID_STEP, - N_GRASS_SECTOR_SIZE / 2 * FP_GRID_STEP, nMin * FP_TERRAIN_H_SCALE );
	CVec3 ptMax(   N_GRASS_SECTOR_SIZE / 2 * FP_GRID_STEP,   N_GRASS_SECTOR_SIZE / 2 * FP_GRID_STEP, nMax * FP_TERRAIN_H_SCALE );
	pBound->BoxInit( ptMin, ptMax );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrassTracker::GetBoundTransform( int nX, int nY, SFBTransform *pTransform )
{
	CVec3 pos;
	pos.x = (nX * N_GRASS_SECTOR_SIZE + N_GRASS_SECTOR_SIZE / 2) * FP_GRID_STEP;
	pos.y = (nY * N_GRASS_SECTOR_SIZE + N_GRASS_SECTOR_SIZE / 2) * FP_GRID_STEP;
	pos.z = 0;
	*pTransform = MakeTransform( pos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SGrassLayer& CGrassTracker::GetGrassLayer( int nLayer )
{
	pInfo.Refresh();
	return pInfo->GetValue().grass[ layers[nLayer].nOriginLayer ];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrassTracker::Wave( const CVec3 &ps )
{
	// CRAP - check height difference between ps.z & actual terrain.z
	waves.Wave( Float2Int( ps.x / WAVE_GRID_SIZE ), Float2Int( ps.y / WAVE_GRID_SIZE ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CGrassTracker::GetWaveAmp( const CVec3 &pt )
{
	return waves.GetAmplitude( pt.x / WAVE_GRID_SIZE, pt.y / WAVE_GRID_SIZE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrassTracker::Update( STime currentTime ) 
{
	waves.UpdateTime( currentTime / 1000.f ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGrass
////////////////////////////////////////////////////////////////////////////////////////////////////
CGrassTracker* CGrass::CreateTracker( CTerrainInfoHolder *pInfo )
{
	if ( trackersMap.find( pInfo ) != trackersMap.end() )
		return trackersMap[pInfo];

	trackersMap[pInfo] = new NGScene::CGrassTracker( pInfo, pAIMap );
	return trackersMap[pInfo];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrass::Wave( const CVec3 &pt )
{
	for ( TTrackersMap::iterator iTemp = trackersMap.begin(); iTemp != trackersMap.end(); iTemp++ )
		iTemp->second->Wave( pt );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrass::Update( STime currentTime )
{
	for ( TTrackersMap::iterator iTemp = trackersMap.begin(); iTemp != trackersMap.end(); iTemp++ )
		iTemp->second->Update( currentTime );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x125A1190, CGrassTracker )
REGISTER_SAVELOAD_CLASS( 0x010c1170, CGrassPosCalcer )
REGISTER_SAVELOAD_CLASS( 0xB25A1190, CGrass )
