#ifndef __GS2DCENE_H_
#define __GS2DCENE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "RectLayout.h"
namespace NGfx
{
	class CTexture;
	CVec2 GetScreenRect();
}
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRects;
class I2DScene: public CObjectBase
{
public:
	virtual CRects* CreateRects( CPtrFuncBase<NGfx::CTexture> *pTexture, CFuncBase<CRectLayout> *pLayout, CFuncBase< CTRect<int> > *pSize = 0 ) = 0;
	virtual CRects* CreateClearRects( CFuncBase<CRectLayout> *pLayout, CFuncBase< CTRect<int> > *pSize = 0 ) = 0;

	virtual void CreateDynamicRects( CPtrFuncBase<NGfx::CTexture> *pTexture, const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sClipWindow ) = 0;
	virtual void CreateDynamicClearRects( const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sClipWindow, float fZ = 1.0f ) = 0;

	virtual void StartNewFrame( NGfx::CTexture *pTarget, const CVec2 &vSize ) = 0;
	virtual void Flush() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
I2DScene* Make2DScene();
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif