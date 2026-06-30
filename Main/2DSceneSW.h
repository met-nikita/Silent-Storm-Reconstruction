#ifndef __G2DSCENESOFTWARE_H_
#define __G2DSCENESOFTWARE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "Transform.h"
#include "SWRectLayout.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CTexture;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
class ISWRects;
class CSWTextureData;
CPtrFuncBase<CSWTextureData>* GetSWTex( NDb::CTexture *pTex );
////////////////////////////////////////////////////////////////////////////////////////////////////
class ISW2DScene: public CObjectBase
{
public:
	virtual ISWRects* CreateRects( CPtrFuncBase<CSWTextureData> *pTexture, CFuncBase<CSWRectLayout> *pLayout ) = 0;
	virtual ISWRects* CreateRects( CPtrFuncBase<CSWTextureData> *pTexture, CPtrFuncBase<CSWTextureData> *pMask, CFuncBase<CSWRectLayout> *pLayout ) = 0;
	virtual ISWRects* CreateSpot( CPtrFuncBase<CSWTextureData> *pTexture, CPtrFuncBase<CSWTextureData> *pMask, const CVec2 &_ptPos, const CVec2 &_ptSize, float _fAngle ) = 0;

	virtual void Draw( NGfx::CTexture *pTarget, const CTPoint<int> &vViewport ) = 0;
	virtual void DrawFog( CArray2D<int> *pFogMap, const CTPoint<int> &vViewport ) = 0;
	virtual void DrawBump( NGfx::CTexture *pTarget, const CTPoint<int> &vViewport, float fScale ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
ISW2DScene* Make2DSWScene();
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif