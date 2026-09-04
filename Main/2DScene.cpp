#include "StdAfx.h"
#include "GRects.h"
#include "2DScene.h"
#include "GfxUtils.h"
#include "GSceneUtils.h"
#include "Transform.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// C2DScene -- retail NGScene::C2DScene (2DScene.obj, saveload id 0xF2005171): immediate-mode only.
// Retail ctor @0xafb0 initializes ONLY quadRender; there is no retained rects list and the object
// serializes NOTHING (registered, but the on-wire body is empty -- the inherited no-op serializer).
// The dev-only retained CRects/CPosNode path (ids 0xF2005175/0xF2005170, retail-absent) was removed
// in the W5 convergence wave. (The retail rects-node family that DOES serialize is the SOFTWARE
// scene: CSWRects 0xF3005171 / CSW2DScene 0xF3005172 in 2DSceneSW.cpp -- untouched.)
////////////////////////////////////////////////////////////////////////////////////////////////////
class C2DScene: public I2DScene
{
	OBJECT_NOCOPY_METHODS(C2DScene);

	NGfx::C2DQuadsRenderer quadRender;

public:
	void CreateDynamicRects( CPtrFuncBase<NGfx::CTexture> *pTexture, const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sClipWindow );
	void CreateDynamicClearRects( const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sClipWindow, float fZ = 1.0f );

	void StartNewFrame( NGfx::CTexture *pTarget, const CVec2 &vSize );
	void Flush();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static CRectLayout clippedTemp;
void C2DScene::CreateDynamicRects( CPtrFuncBase<NGfx::CTexture> *pTexture, const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sWindow )
{
//	ClipRects( &clippedTemp, sLayout, sWindow, CVec2(sPosition.x, sPosition.y) );
	if ( pTexture )
	{
		CDGPtr<CPtrFuncBase<NGfx::CTexture> > pTex( pTexture );
		pTex.Refresh();
		RenderRectLayoutClipped( &quadRender, pTex->GetValue(), sLayout, sPosition, sWindow, 0.0f, LRM_NORMAL );
	}
	else
		RenderRectLayoutClipped( &quadRender, 0, sLayout, sPosition, sWindow, 0.0f, LRM_NORMAL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DScene::CreateDynamicClearRects( const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sWindow, float fZ )
{
//	ClipRects( &clippedTemp, sLayout, sWindow, CVec2(sPosition.x, sPosition.y) );
	RenderRectLayoutClipped( &quadRender, 0, sLayout, sPosition, sWindow, fZ, LRM_CLEAR_RECT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DScene::StartNewFrame( NGfx::CTexture *pTarget, const CVec2 &vSize )
{
	NGfx::CRenderContext rc;
	if ( pTarget ) 
		rc.SetTextureRT( pTarget, 0 );
	// Retail @0x40ac37 selects mode 8 (COMBINE_SMART_ALPHA). UI rect colors are
	// premultiplied before submission, so the matching blend is ONE/INVSRCALPHA.
	rc.SetAlphaCombine( NGfx::COMBINE_SMART_ALPHA );
	rc.SetStencil( NGfx::STENCIL_NONE );
	quadRender.SetTarget( rc, vSize, NGfx::QRM_OVERWRITE );	// retail StartNewFrame @0xabf0 ends here
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C2DScene::Flush()
{
	quadRender.Flush();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Make scene
////////////////////////////////////////////////////////////////////////////////////////////////////
I2DScene* Make2DScene()
{
	return new C2DScene;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESOACE
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0xF2005171, C2DScene );
