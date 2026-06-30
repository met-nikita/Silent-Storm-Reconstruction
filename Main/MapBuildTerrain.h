#ifndef __MapBuildTerrain_H_
#define __MapBuildTerrain_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
struct STerrainInfo;
struct SRand;
bool LoadRootTerrain( int nVariantID, STerrainInfo *pTerrain, SRand *pRand, const vector<int> &flags );
void LoadTerrSpots( int nVarID, STerrainInfo *pTerr, SRand *pRand, const vector<int> &flags );
// fRotation in radians
void BlendTerrainInfo( STerrainInfo *pRes, int nVariantID, const CVec3 &ptCenter, 
	float fRotation, SRand *pRand, const vector<int> &flags );
void ClearTerrainCache();
float GetMeterHeightCheck( const STerrainInfo &info, float x, float y );
void MakeSoundMap( STerrainInfo *pInfo );
void CalcAverageColor( STerrainInfo *pInfo );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
