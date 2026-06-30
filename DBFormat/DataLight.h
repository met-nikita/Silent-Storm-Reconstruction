#ifndef __DATALIGHT_H_
#define __DATALIGHT_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
#include "DataConst.h"
#include "DataFormat.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
class CCubeTexture;
class CTAmbientLight;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAmbientLightReal : public CObjectBase
{
	OBJECT_BASIC_METHODS( CAmbientLightReal );
public:
	ZDATA
	CVec3 vAmbientColor, vLightColor, vGlossColor, vFogColor, vVapourColor, vShadowColor, vBackColor;
	float fPitch, fYaw, fFogDistance, fVapourHeight, fVapourDensity;
	float fVapourNoiseParam, fVapourSpeed, fVapourSwitchTime;
	float fFogStartDistance, fVapourStartHeight, fBlurStrength;
	CDBPtr<NDb::CCubeTexture> pSky;
	bool bInGameUse;
	CPtr<CAmbientLightReal> pGF2Light;
	CVec3 vGroundAmbientColor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&vAmbientColor); f.Add(3,&vLightColor); f.Add(4,&vGlossColor); f.Add(5,&vFogColor); f.Add(6,&vVapourColor); f.Add(7,&vShadowColor); f.Add(8,&vBackColor); f.Add(9,&fPitch); f.Add(10,&fYaw); f.Add(11,&fFogDistance); f.Add(12,&fVapourHeight); f.Add(13,&fVapourDensity); f.Add(14,&fVapourNoiseParam); f.Add(15,&fVapourSpeed); f.Add(16,&fVapourSwitchTime); f.Add(17,&fFogStartDistance); f.Add(18,&fVapourStartHeight); f.Add(19,&fBlurStrength); f.Add(20,&pSky); f.Add(21,&bInGameUse); f.Add(22,&pGF2Light); f.Add(23,&vGroundAmbientColor); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAmbientLight: public CDBRecord
{
	OBJECT_BASIC_METHODS( CAmbientLight );
public:
	CVec3 vAmbientColor, vLightColor, vGlossColor, vFogColor, vVapourColor, vShadowColor, vBackColor;
	float fPitch, fYaw, fFogDistance, fVapourHeight, fVapourDensity;
	float fVapourNoiseParam, fVapourSpeed, fVapourSwitchTime;
	float fFogStartDistance, fVapourStartHeight, fBlurStrength;
	CPtr<NDb::CCubeTexture> pSky;
	bool bInGameUse;
	CPtr<CTAmbientLight> pGF2Light;
	vector<SVariantFlags> flags;
	CPtr<CTAmbientLight> pTemplate;
	CVec3 vGroundAmbientColor;
	//
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTAmbientLight : public CRndPtr<CAmbientLight>
{
	OBJECT_BASIC_METHODS(CTAmbientLight);
public:
	CAmbientLightReal* GetLight( SRand *pRand ) const;
	CAmbientLightReal* GetLight( SRand *pRand, const vector<int> &params ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAmbientLight* GetAmbientLight( int nID );
CTAmbientLight* GetTAmbientLight( int nID );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DATALIGHT_H_