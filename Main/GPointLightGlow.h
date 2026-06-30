#ifndef __POINTLIGHTGLOW_H_
#define __POINTLIGHTGLOW_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "Time.h"

namespace NGfx
{
	class CTexture;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
class CParticleEffect;
class IGScene;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPointGlowAnimator : public CPtrFuncBase<CParticleEffect>
{
	OBJECT_BASIC_METHODS(CPointGlowAnimator );
protected:
	virtual bool NeedUpdate() { return pTime.Refresh() | pPlacement.Refresh(); }
	virtual void Recalc();
private:	
	ZDATA
	CDGPtr< CFuncBase<STime> > pTime;
	CDGPtr< CFuncBase<CVec3> > pPlacement;
	CObj<CPtrFuncBase<NGfx::CTexture> > pTexture;
	CPtr<IGScene> pScene;
	STime tPrev;
	float fSize, fLightSize;
	float fOnTime, fOffTime;
	CDGPtr<CFuncBase<CVec3> > pCamera;
	int nLastMaskAny;
	STime tNextCheck;
	bool bIsVisible;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTime); f.Add(3,&pPlacement); f.Add(4,&pTexture); f.Add(5,&pScene); f.Add(6,&tPrev); f.Add(7,&fSize); f.Add(8,&fLightSize); f.Add(9,&fOnTime); f.Add(10,&fOffTime); f.Add(11,&pCamera); f.Add(12,&nLastMaskAny); f.Add(13,&tNextCheck); f.Add(14,&bIsVisible); return 0; }

	void CalcSize();
public:
	CPointGlowAnimator() {}
	CPointGlowAnimator( IGScene *_pScene, CFuncBase<STime> *_pTime, CFuncBase<CVec3> *pPlace, 
		CPtrFuncBase<NGfx::CTexture> *pTexture, float _fLightSize, float fOnTime, float fOffTime );
	friend class CPointGlowEffect;
};
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif