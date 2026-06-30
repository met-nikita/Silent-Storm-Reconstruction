#ifndef __GTERRAININFO_H_
#define __GTERRAININFO_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "..\Misc\2Darray.h"
#include "../DBFormat/DataFormat.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CTSound;
	class CRPGArmor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct STerrainHole
{
	int nHeight;
	bool bVisible;
	vector<CVec2> vPolygon;

	int operator&( CStructureSaver &f );
};
//////////////////////////////////////////////////////////////////////////////////////	
const float FP_GRASS_COLOR_SCALE = 0.5f;
struct SGrassLayer
{
	ZDATA
	int nGrassID;
	CArray2D<BYTE> grass;
	float fMaxDensity;
	CArray2D<DWORD> grassColor;
	vector<CVec2> blades;
	int nID;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nGrassID); f.Add(3,&grass); f.Add(4,&fMaxDensity); f.Add(5,&grassColor); f.Add(6,&blades); f.Add(7,&nID); return 0; }
};
//////////////////////////////////////////////////////////////////////////////////////	
struct SStepSound
{
	ZDATA
	CDBPtr<NDb::CRPGArmor> pArmor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pArmor); return 0; }

	SStepSound() {}
	SStepSound( NDb::CRPGArmor *pA )
		: pArmor(pA){}
};
//////////////////////////////////////////////////////////////////////////////////////	
struct STerrainSpot
{
	ZDATA
	CDBPtr<NDb::CMaterial> pMaterial;
	CVec2 ptPos, ptSize;
	float fAngle; // in radians;
	SStepSound stepSound;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pMaterial); f.Add(3,&ptPos); f.Add(4,&ptSize); f.Add(5,&fAngle); f.Add(6,&stepSound); return 0; }
	STerrainSpot() {}
	STerrainSpot( NDb::CMaterial *_pM, const CVec2 &_ptPos, const CVec2 &_ptSize, float _fAngle, const SStepSound _stepSound ) 
		: pMaterial(_pM), ptPos(_ptPos), ptSize(_ptSize), fAngle(_fAngle), stepSound(_stepSound) {}
};
//////////////////////////////////////////////////////////////////////////////////////	
struct STerrainInfo
{
	int nWidth;
	int nHeight;
	CArray2D<unsigned char> typeMap;
	CArray2D<unsigned short> heightMap;
	vector<STerrainHole> holes;
	vector<SGrassLayer>  grass;
	CArray2D<DWORD> color;
	vector<STerrainSpot> spots;
	int nMaxGrassLayerID;
	CArray2D<SStepSound> soundmap;
	CArray2D<DWORD> avrgColor;
	STerrainInfo() : nWidth( 0 ), nHeight( 0 ), nMaxGrassLayerID( 1 ) { typeMap[0][0] = 0; heightMap[0][0] = 0; color[0][0] = 0xffffffff; }
	int operator&( CStructureSaver &f );

	SStepSound GetStepSound( int x, int y ) const
	{ 
		if ( !( x < 0 || y < 0 || x >= soundmap.GetXSize() || y >= soundmap.GetYSize() ) )
			return soundmap[y][x];
		return SStepSound();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTerrainPart: public CObjectBase
{
	OBJECT_BASIC_METHODS( CTerrainPart );
public:
	ZDATA
	bool bInverseNormals;
	vector<CVec3> verts;
	vector<STriangle> faces;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bInverseNormals); f.Add(3,&verts); f.Add(4,&faces); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTerrainInfoHolder: public CFuncBase<STerrainInfo>
{
	OBJECT_BASIC_METHODS(CTerrainInfoHolder);
	CArray2D<CObj<CVersioningBase> > geometryGrid;
	CArray2D<CObj<CVersioningBase> > textureGrid;
	CArray2D<CObj<CVersioningBase> > grassGrid;
	
	void UpdateRegion( CArray2D<CObj<CVersioningBase> > *pGrid, const CTRect<int> &sRegion );
	CVersioningBase* GetRegion( const CArray2D<CObj<CVersioningBase> > &grid, const CTRect<int> &sRegion );

public:

	CTerrainInfoHolder() {}
	CTerrainInfoHolder( const STerrainInfo &info );

	int operator&( CStructureSaver &f );

	void UpdateRegionGeometry( const CTRect<int> &sRegion );
	void UpdateRegionTexture( const CTRect<int> &sRegion );
	void UpdateRegionGrass( const CTRect<int> &sRegion );
	CVersioningBase* GetRegionGeometry( const CTRect<int> &sRegion );
	CVersioningBase* GetRegionTexture( const CTRect<int> &sRegion );
	CVersioningBase* GetRegionGrass( const CTRect<int> &sRegion );
	STerrainInfo& GetWritableInfo();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
DEFINE_DG_CONSTANT_NODE( CCTerrainInfo, STerrainInfo );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
