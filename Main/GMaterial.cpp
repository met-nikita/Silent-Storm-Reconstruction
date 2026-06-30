#include "StdAfx.h"
#include "GMaterial.h"
#include "GfxEffects.h"
#include "GfxRender.h"
#include "GRenderCore.h"
#include "GRenderFactor.h"
#include "GSceneUtils.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShadowCastMaterial: public IMaterial
{
protected:
	ZDATA
	bool bDoesCastShadow;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bDoesCastShadow); return 0; }
	CShadowCastMaterial() {}
	CShadowCastMaterial( bool _bDoesCastShadow ): bDoesCastShadow(_bDoesCastShadow) {}
	bool DoesCastShadow() const { return bDoesCastShadow; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGenericMaterial: public CShadowCastMaterial
{
	OBJECT_BASIC_METHODS( CGenericMaterial );
	CVec4 vMirrorParam;
	ZDATA_(CShadowCastMaterial)
	SMaterialInfo info;
	CDGPtr<CFuncBase<CVec3> > pDiffuseColor, pSpecularColor;
	CDGPtr<CPtrFuncBase<NGfx::CTexture> > pDiffuseTex, pSpecularTex;
	CDGPtr<CPtrFuncBase<NGfx::CTexture> > pBump;
	// reflection props
	CDGPtr<CPtrFuncBase<NGfx::CCubeTexture> > pSky;
	CDGPtr<CPtrFuncBase<NGfx::CTexture> > pMirror;
	float fMetalMirror, fDielMirror;
	CObj<CGenericMaterial> pExactDecal;
	// release save-format tags 13/14: second diffuse texture layer + its blend (dead-in-dev, single-layer predecessor)
	CDGPtr<CPtrFuncBase<NGfx::CTexture> > pDiffuseTex1;
	CDGPtr<CFuncBase<float> > pDiffuseTex1Blend;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CShadowCastMaterial*)this); f.Add(2,&info); f.Add(3,&pDiffuseColor); f.Add(4,&pSpecularColor); f.Add(5,&pDiffuseTex); f.Add(6,&pSpecularTex); f.Add(7,&pBump); f.Add(8,&pSky); f.Add(9,&pMirror); f.Add(10,&fMetalMirror); f.Add(11,&fDielMirror); f.Add(12,&pExactDecal); f.Add(13,&pDiffuseTex1); f.Add(14,&pDiffuseTex1Blend); return 0; }
protected:
	void AddOperations( SOpGenContext *p, ERenderPath rp );
	void AddATOperations( SOpGenContext *p );
	const SMaterialInfo& GetMaterialInfo();
	CVec3 GetAverageColor() const { return info.diffuse.color; }
public:
	CGenericMaterial() {}
	CGenericMaterial( bool _bDoesCastShadow )
		: CShadowCastMaterial(_bDoesCastShadow), fMetalMirror(0), fDielMirror(0) {}
	virtual bool IsDecal() const { return info.IsDecal(); }
	virtual bool IsAlphaTest() const { return info.bAlphaTest; }
	virtual bool IsSolid() const { return info.IsSolid(); }
	virtual IMaterial* GetExactDecal();
	virtual void Precache() { GetMaterialInfo(); }
	void SetDiffuseColor( CFuncBase<CVec3> *_p ) { info.diffuse.type = SMaterialInfo::T_COLOR; pDiffuseColor = _p; }
	void SetDiffuseColor( const CVec3 &_vColor ) { info.diffuse.color = _vColor; }
	void SetSpecularColor( CFuncBase<CVec3> *_p, float _fPower ) { info.specular.type = SMaterialInfo::T_COLOR; pSpecularColor = _p; info.fSpecPower =_fPower; }
	void SetDiffuseTex( CPtrFuncBase<NGfx::CTexture> *_p ) { info.diffuse.type = SMaterialInfo::T_TEXTURE; pDiffuseTex = _p; }
	void SetSpecularTex( CPtrFuncBase<NGfx::CTexture> *_p, float _fPower ) { info.specular.type = SMaterialInfo::T_TEXTURE; pSpecularTex = _p; info.fSpecPower =_fPower; }
	void SetBump( CPtrFuncBase<NGfx::CTexture> *_p ) { pBump = _p; }
	void SetDecal() { info.mt = SMaterialInfo::DECAL; bDoesCastShadow = false; }
	void SetSelfIllum() { info.mt = SMaterialInfo::SELF_ILLUM; }
	void SetAlphaTest( bool _b ) { info.bAlphaTest = _b; }
	void SetReflectionInfo( CPtrFuncBase<NGfx::CCubeTexture> *_pSky, CPtrFuncBase<NGfx::CTexture> *_pMirror,
		float _fDielMirror, float _fMetalMirror );
	void Check()
	{
		ASSERT( info.diffuse.type != SMaterialInfo::T_NONE );
		ASSERT( !info.bAlphaTest || info.diffuse.type != SMaterialInfo::T_COLOR );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IMaterial* CGenericMaterial::GetExactDecal()
{
	if ( IsValid( pExactDecal ) )
		return pExactDecal;
	if ( info.mt == SMaterialInfo::EXACT_DECAL )
		return this;
	if ( info.mt != SMaterialInfo::DECAL )
		return 0;
	pExactDecal = Duplicate();
	pExactDecal->info.mt = SMaterialInfo::EXACT_DECAL;
	return pExactDecal;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGenericMaterial::SetReflectionInfo( CPtrFuncBase<NGfx::CCubeTexture> *_pSky, CPtrFuncBase<NGfx::CTexture> *_pMirrorTex,
	float _fDielMirror, float _fMetalMirror )
{
	fDielMirror = _fDielMirror;
	fMetalMirror = _fMetalMirror;
	if ( _pSky && ( fDielMirror > 0 || fMetalMirror > 0 ) )
	{
		pSky = _pSky;
		pMirror = _pMirrorTex;
	}
	else
		pSky = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGenericMaterial::AddOperations( SOpGenContext *p, ERenderPath rp )
{
	if ( !pSky )
		return;
	pSky.Refresh();
	if ( pSky->GetValue() == 0 )
		return;
	vMirrorParam = CVec4( 0, 0, fDielMirror, fMetalMirror );
	pSky.Refresh();
	if ( NGfx::GetHardwareLevel() >= NGfx::HL_GFORCE3 && rp >= RP_GF3_CL )
	{
		NGfx::CTexture *pRefBump = 0;
		if ( pBump )
		{
			pBump.Refresh();
			pRefBump = pBump->GetValue();
		}
		else
		{
			pRefBump = GetUniformBump();
		}
		if ( fMetalMirror == 1 && fDielMirror == 0 )
		{
			if ( pMirror )
			{
				pMirror.Refresh();
				p->AddOperation( RO_BUMPED_MIRROR, 40, DPM_EQUAL, 1, 
					pSky->GetValue(), pRefBump );
				p->AddOperation( RO_REG_MUL_TEXTURE, 45, ABM_ALPHA_BLEND|DPM_EQUAL, 0, 
					pMirror->GetValue() );
			}
			else
				p->AddOperation( RO_BUMPED_MIRROR, 45, ABM_ALPHA_BLEND|DPM_EQUAL, 0, 
					pSky->GetValue(), pRefBump );
		}
		else
		{
			if ( pMirror )
			{
				pMirror.Refresh();
				p->AddOperation( RO_BUMPED_MIRROR, 40, DPM_EQUAL, 1, 
					pSky->GetValue(), pRefBump );
				p->AddOperation( RO_TEXTURE_DECAL, 41, ABM_MUL|DPM_EQUAL, 1, 
					pMirror->GetValue() );
				p->AddOperation( RO_BUMPED_FRESNEL, 45, ABM_ALPHA_BLEND|DPM_EQUAL, 0, 
					pRefBump, &vMirrorParam );
			}
			else
			{
				p->AddOperation( RO_BUMPED_MIRROR, 40, DPM_EQUAL, 1, 
					pSky->GetValue(), pRefBump );
				p->AddOperation( RO_BUMPED_FRESNEL, 45, ABM_ALPHA_BLEND|DPM_EQUAL, 0, 
					pRefBump, &vMirrorParam );
			}
		}
	}
	else
	{
		if ( pMirror )
		{
			pMirror.Refresh();
			p->AddOperation( RO_GLOSSED_MIRROR, 45, ABM_ALPHA_BLEND|DPM_EQUAL, 0, 
				pSky->GetValue(), pMirror->GetValue(), &vMirrorParam );
		}
		else
			p->AddOperation( RO_MIRROR, 45, ABM_ALPHA_BLEND|DPM_EQUAL, 0, pSky->GetValue(), &vMirrorParam );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGenericMaterial::AddATOperations( SOpGenContext *p )
{
	if ( info.bAlphaTest )
	{
		ASSERT( pDiffuseTex );
		pDiffuseTex.Refresh();
		p->AddOperation( RO_TEXTURE_AT, 0, 0, 0, pDiffuseTex->GetValue() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void Refresh( SMaterialInfo::SColorInfo *p, CDGPtr<CFuncBase<CVec3> > *pColor, 
	CDGPtr<CPtrFuncBase<NGfx::CTexture> > *pTex )
{
	switch ( p->type )
	{
		case SMaterialInfo::T_NONE: break;
		case SMaterialInfo::T_COLOR: pColor->Refresh(); p->color = (*pColor)->GetValue(); break;
		case SMaterialInfo::T_TEXTURE: pTex->Refresh(); p->pTex = (*pTex)->GetValue(); break;
		case SMaterialInfo::T_VSPEC: pTex->Refresh(); p->pTex = (*pTex)->GetValue(); break;
		default: ASSERT(0); break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SMaterialInfo& CGenericMaterial::GetMaterialInfo()
{
	if ( pBump )
	{
		pBump.Refresh();
		info.pBump = pBump->GetValue();
	}
	Refresh( &info.diffuse, &pDiffuseColor, &pDiffuseTex );
	Refresh( &info.specular, &pSpecularColor, &pSpecularTex );
	return info;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTransparentMaterial: public IMaterial
{
	OBJECT_BASIC_METHODS( CTransparentMaterial );
	ZDATA
	CDGPtr< CPtrFuncBase<NGfx::CTexture> > pTexture;
	STransparentMaterialInfo transpInfo;
	bool b2Sided;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTexture); f.Add(3,&transpInfo); f.Add(4,&b2Sided); return 0; }
public:
	CTransparentMaterial() {}
	CTransparentMaterial( CPtrFuncBase<NGfx::CTexture> *_pTex, bool _b2Sided, float fMetalMirror, float fDielMirror )
		: pTexture(_pTex), b2Sided(_b2Sided) { transpInfo.fMetalMirror = fMetalMirror; transpInfo.fDielMirror = fDielMirror; }
	virtual EMaterialType GetType() const { return MT_TRANSPARENT; }
	virtual const STransparentMaterialInfo& GetTransparentInfo();
	bool DoesCastShadow() const { return false; }
	bool Is2Sided() const { return b2Sided; }
	virtual void Precache() { GetTransparentInfo(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const STransparentMaterialInfo& CTransparentMaterial::GetTransparentInfo() 
{ 
	pTexture.Refresh();
	transpInfo.pTex = pTexture->GetValue();
	return transpInfo; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class COccluderMaterial : public IMaterial
{
	OBJECT_NOCOPY_METHODS(COccluderMaterial);
public:
	virtual bool DoesCastShadow() const { return false; }
	virtual EMaterialType GetType() const { return MT_OCCLUDER; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAlienMaterial : public IMaterial
{
	OBJECT_NOCOPY_METHODS(CAlienMaterial);
	ZDATA
	CDGPtr<CPtrFuncBase<NGfx::CCubeTexture> > pSky;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pSky); return 0; }
protected:
	void AddOperations( SOpGenContext *p, ERenderPath rp );
public:
	CAlienMaterial() {}
	CAlienMaterial( CPtrFuncBase<NGfx::CCubeTexture> *_pSky ) : pSky(_pSky) {}
	virtual bool DoesCastShadow() const { return false; }
	virtual EMaterialType GetType() const { return MT_ALIEN; }
	virtual void Precache() { pSky.Refresh(); pSky->GetValue(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static CVec4 vAlienColor( 0.3f, 0.3f, 0.3f, 0.5f );
void CAlienMaterial::AddOperations( SOpGenContext *p, ERenderPath rp )
{
	if ( !pSky )
		return;
	pSky.Refresh();
	if ( pSky->GetValue() && rp > RP_FASTEST ) // NGfx::GetHardwareLevel() >= NGfx::HL_GFORCE3 && 
	{
		p->AddOperation( RO_ALIEN, 40, 0, 1, pSky->GetValue() );
		p->AddOperation( RO_REGISTER, 45, DPM_EQUAL, 0, 1 );
	}
	else
	{
		p->AddOperation( RO_SOLID_COLOR, 45, ABM_ALPHA_BLEND, 0, &vAlienColor );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExplosionDecalMaterial : public IMaterial
{
	OBJECT_NOCOPY_METHODS(CExplosionDecalMaterial);
	ZDATA
	CDGPtr<CPtrFuncBase<NGfx::CTexture> > pTexture;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTexture); return 0; }
protected:
	void AddOperations( SOpGenContext *p, ERenderPath rp );
public:
	CExplosionDecalMaterial() {}
	CExplosionDecalMaterial( CPtrFuncBase<NGfx::CTexture> *_pTex ) : pTexture(_pTex) {}
	virtual bool DoesCastShadow() const { return false; }
	virtual EMaterialType GetType() const { return MT_ALIEN; }
	virtual void Precache() { pTexture.Refresh(); pTexture->GetValue(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplosionDecalMaterial::AddOperations( SOpGenContext *p, ERenderPath rp )
{
	if ( !pTexture )
		return;
	pTexture.Refresh();
	if ( pTexture->GetValue() && rp != RP_TNL )
		p->AddOperation( RO_EXPLOSION_DECAL, 10, ABM_SMART|DPM_EQUAL, 0, pTexture->GetValue() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
IMaterial* CreateOccluderMaterial()
{
	return new COccluderMaterial;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IMaterial* CreateAlienMaterial( CPtrFuncBase<NGfx::CCubeTexture> *_pSky )
{
	return new CAlienMaterial( _pSky );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IMaterial* CreateExplosionDecal( CPtrFuncBase<NGfx::CTexture> *pTexture )
{
	return new CExplosionDecalMaterial( pTexture );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IMaterial* CreateMaterial( 
	const CVec3 &color,
	CPtrFuncBase<NGfx::CTexture> *pTexture, CPtrFuncBase<NGfx::CTexture> *pBump,
	float fSpecPower, const CVec3 &vGlossVolor,
	CPtrFuncBase<NGfx::CTexture> *pGlossTexture,
	CPtrFuncBase<NGfx::CTexture> *pMirrorTexture,
	CPtrFuncBase<NGfx::CCubeTexture> *pSky,
	float fMetalMirror, float fDielMirror,
	int alphaMode, bool bDoesCastShadow )
{
	CObj<CObjectBase> pHold1(pBump), pHold2(pGlossTexture), pHold3(pMirrorTexture);
	if ( ( alphaMode & MA_TYPE_MASK ) == MA_TRANSPARENT )
		return new CTransparentMaterial( pTexture, ( alphaMode & MA_2SIDED ) != 0, fMetalMirror, fDielMirror );
	CGenericMaterial *pRes = new CGenericMaterial( bDoesCastShadow );
	pRes->SetDiffuseColor( color );
	//alphaMode |= MA_SELF_ILLUM;
	if ( alphaMode & MA_SELF_ILLUM )
		pRes->SetSelfIllum();
	//const_cast<CVec3&>(vGlossVolor) = CVec3(1,1,1);
	//fSpecPower = 1;
	if ( pTexture == 0 )
	{
		pRes->SetDiffuseColor( new CCVec3( color ) );
		pRes->Check();
		pRes->SetReflectionInfo( pSky, pMirrorTexture, fDielMirror, fMetalMirror );
		return pRes;
	}
	if ( ( alphaMode & MA_TYPE_MASK ) == MA_OVERLAY )
		pRes->SetDecal();
	pRes->SetDiffuseTex( pTexture );
	if ( alphaMode & MA_ALPHA_TEST )
		pRes->SetAlphaTest( true );
	pRes->SetBump( pBump );

	if ( fabs2( vGlossVolor ) > 0 )
		pRes->SetSpecularColor( new CCVec3( vGlossVolor ), fSpecPower );
	if ( pGlossTexture )
		pRes->SetSpecularTex( pGlossTexture, fSpecPower );
	// fill reflection info
	//fDielMirror = 1;
	//fMetalMirror = 1;
	pRes->SetReflectionInfo( pSky, pMirrorTexture, fDielMirror, fMetalMirror );
	pRes->Check();
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
BASIC_REGISTER_CLASS( IMaterial )
REGISTER_SAVELOAD_CLASS( 0x02191160, CTransparentMaterial )
REGISTER_SAVELOAD_CLASS( 0x02191161, CGenericMaterial )
REGISTER_SAVELOAD_CLASS( 0x020A2170, COccluderMaterial )
REGISTER_SAVELOAD_CLASS( 0x023A2180, CAlienMaterial )
REGISTER_SAVELOAD_CLASS( 0x004c2180, CExplosionDecalMaterial )
