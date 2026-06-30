#ifndef __GGrass_H_
#define __GGrass_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "TerrainInfo.h"
#include "Time.h"
#include "../DBFormat/DataFormat.h"
#include "../DBFormat/DataTerrain.h"
#include "aiMap.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
const int N_GRASS_SECTOR_SIZE = 8;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGrassPosition: public CObjectBase
{
	OBJECT_BASIC_METHODS( CGrassPosition );
public:
	ZDATA
	vector<CVec3> positions;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&positions); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGrassPosCalcer: public CPtrFuncBase<CGrassPosition>
{
	OBJECT_BASIC_METHODS( CGrassPosCalcer );
	ZDATA
	int nX, nY, nOriginLayer;
	CPtr<NAI::IAIMap> pAIMap;
	CDGPtr< CFuncBase< STerrainInfo > > pInfo;
	CDGPtr<CVersioningBase> pSectorUpdate;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nX); f.Add(3,&nY); f.Add(4,&nOriginLayer); f.Add(5,&pAIMap); f.Add(6,&pInfo); f.Add(7,&pSectorUpdate); return 0; }
protected:
	virtual bool NeedUpdate();
	virtual void Recalc();
public:
	CGrassPosCalcer() {}
	CGrassPosCalcer( int _nLayer, int _nX, int _nY, NAI::IAIMap *_pAIMap, CFuncBase<STerrainInfo> *_pInfo, CVersioningBase *pUpdate )
		: nX(_nX), nY(_nY), nOriginLayer(_nLayer), pAIMap(_pAIMap), pInfo(_pInfo), pSectorUpdate(pUpdate) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline float GetPseudoRandomForBlade( int nIndex )
{
	int tmp = (nIndex * 0x24B) & 0x3FF;
	return tmp * (1.f / 1024.f);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CParticleWaveTexture
{
	ZDATA
	CArray2D<unsigned char> times;
	int nCurTime;
	float fCurTime;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&times); f.Add(3,&nCurTime); f.Add(4,&fCurTime); return 0; }
private:
	void ShiftTime();
public:
	CParticleWaveTexture() : nCurTime(0), fCurTime(0) {}
	void SetSizes( int nXSize, int nYSize ) { times.SetSizes( nXSize, nYSize ); times.FillZero(); }
	void Wave( int nX, int nY );
	void UpdateTime( float fNewTime );
	float GetAmplitude( float x, float y );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGrassTracker: public CObjectBase
{
	OBJECT_BASIC_METHODS( CGrassTracker );
	struct SGrassLayerInfo
	{
		ZDATA
		CArray2D<CObj<CPtrFuncBase<CGrassPosition> > > grass;
		CDBPtr<NDb::CGrass> pGrass;
		int nOriginLayer;
		CDBPtr<NDb::CMaterial> pSpotMaterial;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&grass); f.Add(3,&pGrass); f.Add(4,&nOriginLayer); f.Add(5,&pSpotMaterial); return 0; }
	};
	ZDATA
	CDGPtr< CFuncBase< STerrainInfo > > pInfo;
	vector<SGrassLayerInfo> layers;
	CParticleWaveTexture waves;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pInfo); f.Add(3,&layers); f.Add(4,&waves); return 0; }

	bool IsSectorFilled( int nLayer, int nX, int nY );
public:
	CGrassTracker() {}
	CGrassTracker( CTerrainInfoHolder *_pInfo, NAI::IAIMap *_pAIMap );
	CPtrFuncBase<CGrassPosition>* GetGrassPosCalcer( int nLayer, int nX, int nY );
	int GetNumLayers(); // might not match info.grass.size()
	int GetNumSectorsX();
	int GetNumSectorsY();
	int GetTextureLayerID( int nLayer );
	void GetSectorBound( int nLayer, int nX, int nY, SBound *pBound );
	void GetBoundTransform( int nX, int nY, SFBTransform *pTransform );
	NDb::CGrass* GetGrass( int nLayer ) { return layers[nLayer].pGrass; }
	NDb::CMaterial* GetSpotMaterial( int nLayer ) { return layers[nLayer].pSpotMaterial; }
	const SGrassLayer& GetGrassLayer( int nLayer );
	float GetWaveAmp( const CVec3 &pt );
	void Wave( const CVec3 &pt );
	void Update( STime currentTime );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGrass: public CObjectBase
{
	OBJECT_BASIC_METHODS(CGrass);
private:
	typedef unordered_map<CPtr<CFuncBase<STerrainInfo> >,CObj<CGrassTracker>,SPtrHash > TTrackersMap;
	ZDATA
	CPtr<NAI::IAIMap> pAIMap;
	ZSKIP
	TTrackersMap trackersMap;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pAIMap); f.Add(4,&trackersMap); return 0; }

public:
	CGrass() {}
	CGrass( NAI::IAIMap *_pAIMap ): pAIMap( _pAIMap ) {}

	CGrassTracker* CreateTracker( CTerrainInfoHolder *_pInfo );
	void Wave( const CVec3 &pt );
	void Update( STime currentTime );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
