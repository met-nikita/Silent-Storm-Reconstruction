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
// I2DScene -- immediate-mode 2D quad scene, exactly the retail NGScene::C2DScene method set
// (2DScene.obj: ctor @0xafb0, StartNewFrame @0xabf0, Flush @0xaca0, CreateDynamicRects @0xad40,
// CreateDynamicClearRects @0xabc0). The dev-only retained path (CreateRects/CreateClearRects over
// CRects/CPosNode nodes, ids 0xF2005175/0xF2005170) had no retail counterpart and no live caller;
// removed in the W5 convergence wave.
class I2DScene: public CObjectBase
{
public:
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