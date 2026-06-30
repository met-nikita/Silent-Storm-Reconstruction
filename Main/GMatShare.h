#ifndef __GMatShare_H_
#define __GMatShare_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "GScene.h"
namespace NGfx
{
	class CCubeTexture;
}
namespace NDb
{
	class CMaterial;
}
namespace NGScene
{
const int N_MIN_FLOOR = -3;
const int N_MAX_FLOOR = 4;
inline int GetFloorMask( int nFloor ) { return ( 1 << (nFloor - N_MIN_FLOOR + 1) ) - 1; }
inline int GetParticlesRequireFlag( bool bShowParticles ) { return bShowParticles ? 0 : N_MASK_TREECROWN; }
inline int GetFloorBit( int nFloor, bool bShadowCast, bool bParticles ) 
{ 
	return ( 1 << Max( nFloor - N_MIN_FLOOR, 0 ) ) | ( bParticles ? 0 : N_MASK_TREECROWN ) | ( bShadowCast ? N_MASK_CAST_SHADOW : 0 ); 
}
class IMaterial;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SColorHash
{
	int operator()( const CVec3 &color ) const { int *p = (int*)&color; return p[0]^p[1]^p[2]; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSkyAdapter: public CPtrFuncBase<NGfx::CCubeTexture>
{
	OBJECT_BASIC_METHODS( CSkyAdapter );
	ZDATA
	CDGPtr<CPtrFuncBase<NGfx::CCubeTexture> > pTex;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTex); return 0; }
	virtual bool NeedUpdate();
	virtual void Recalc();
public:
	void SetSource( CPtrFuncBase<NGfx::CCubeTexture> *_pTex );
	CPtrFuncBase<NGfx::CCubeTexture>* GetSource() const { return pTex; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMaterialShare: public CObjectBase
{
	OBJECT_BASIC_METHODS( CMaterialShare );
	typedef unordered_map<int, CObj<IMaterial> > CMatHashmap;
	ZDATA
	CMatHashmap materials;
	CObj<CSkyAdapter> pSky;
	CObj<IMaterial> pOccluder;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&materials); f.Add(3,&pSky); f.Add(4,&pOccluder); return 0; }

	IMaterial* CreateMaterial( NDb::CMaterial *pMaterial, int key );
public:
	CMaterialShare(): pSky( new CSkyAdapter ) {}
	void SetSky( CPtrFuncBase<NGfx::CCubeTexture> *_pSky ) { pSky->SetSource( _pSky ); }
	CPtrFuncBase<NGfx::CCubeTexture>* GetSky() const { return pSky->GetSource(); }
	CPtrFuncBase<NGfx::CCubeTexture>* GetSkyBinder() const { return pSky; }
	IMaterial* CreateMaterial( NDb::CMaterial *pMaterial );
	IMaterial* CreateOccluderMaterial();
//	IMaterial* CreateMaterial( NDb::CMaterial *pMaterial, int nLGroup, int nOGroup );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CColorMaterialShare
{
	typedef unordered_map<CVec3, CObj<IMaterial>, SColorHash> CMatHashmap;
	CMatHashmap materials;
public:
	IMaterial* CreateMaterial( const CVec3 &color );
	int operator&( CStructureSaver &f )
	{
		f.Add( 1, &materials );
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct STransparentHash
{
	int operator()( const CVec4 &color ) const { int *p = (int*)&color; return p[0]^p[1]^p[2]^p[3]; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTransparentMaterialShare
{
	typedef unordered_map<CVec4, CObj<IMaterial>, STransparentHash> CMatHashmap;
	CMatHashmap materials;
public:
	IMaterial* CreateMaterial( const CVec4 &color );
	int operator&( CStructureSaver &f )
	{
		f.Add( 1, &materials );
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
