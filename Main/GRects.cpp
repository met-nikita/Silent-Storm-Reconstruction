#include "StdAfx.h"
#include "GRects.h"
#include "GfxUtils.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x1480b0: the quad is (fX, fY) .. (fX + fSizeX, fY + fSizeY) -- the per-rect size baked
// by CRectLayout::AddRect; there is no layout-level scale. LRM_CLEAR_RECT zeroes the WHOLE pixel
// (retail `color = 0`), not just the alpha channel.
void RenderRectLayout( NGfx::C2DQuadsRenderer *pRes, NGfx::CTexture *pTex, const CRectLayout &l, float fZ, ELayoutRenderMode lrm )
{
	for ( int i = 0; i < l.rects.size(); ++i )
	{
		const CRectLayout::SRect &r = l.rects[i];
		NGfx::SPixel8888 color = r.sColor;
		if ( lrm == LRM_CLEAR_RECT )
			color = NGfx::SPixel8888( 0, 0, 0, 0 );
		CTRect<float> rTarget( r.fX, r.fY, r.fX + r.fSizeX, r.fY + r.fSizeY );
		CTRect<float> rSrc( r.sTex.rcTexRect );
		pRes->AddRect( rTarget, pTex, rSrc, color, fZ );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x148190. Retail's signature carries CTPoint<float>/CTRect<float> position+window (the
// whole retail 2D pipeline is float); the int params are kept here because the dev I2DGameView /
// CreateDynamicRects chain (G2DView.h -- outside this rework) is still int. The math below is the
// retail float math verbatim (the ints promote on first use).
// Per rect: the source corners are Min/Max-normalized over (fX, fX+fSizeX) so a negative baked size
// still yields a well-formed quad; the texture rect is remapped by the ratio
// (tex extent / source extent) from the respective corner (no Sign()/scale division -- retail).
void RenderRectLayoutClipped( NGfx::C2DQuadsRenderer *pRes, NGfx::CTexture *pTex, const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sWindow, float fZ, ELayoutRenderMode lrm )
{
	for ( int nTemp = 0; nTemp < sLayout.rects.size(); nTemp++ )
	{
		const CRectLayout::SRect &sRect = sLayout.rects[nTemp];

		const float fSrcX1 = Min( sRect.fX, sRect.fX + sRect.fSizeX ) + sPosition.x;
		const float fSrcY1 = Min( sRect.fY, sRect.fY + sRect.fSizeY ) + sPosition.y;
		const float fSrcX2 = Max( sRect.fX, sRect.fX + sRect.fSizeX ) + sPosition.x;
		const float fSrcY2 = Max( sRect.fY, sRect.fY + sRect.fSizeY ) + sPosition.y;

		const float fClipX1 = Max( (float)sWindow.x1, Min( fSrcX1, (float)sWindow.x2 ) );
		const float fClipY1 = Max( (float)sWindow.y1, Min( fSrcY1, (float)sWindow.y2 ) );
		const float fClipX2 = Max( (float)sWindow.x1, Min( fSrcX2, (float)sWindow.x2 ) );
		const float fClipY2 = Max( (float)sWindow.y1, Min( fSrcY2, (float)sWindow.y2 ) );

		if ( ( fClipX2 - fClipX1 <= 0 ) || ( fClipY2 - fClipY1 <= 0 ) )
			continue;

		// retail 0x548326-0x548395 (VA): tex-coord remap by (tex extent / source extent) per axis
		const CTRect<float> &t = sRect.sTex.rcTexRect;
		const float fRatioX = ( t.x2 - t.x1 ) / ( fSrcX2 - fSrcX1 );
		const float fRatioY = ( t.y2 - t.y1 ) / ( fSrcY2 - fSrcY1 );
		CTRect<float> sTexRect;
		sTexRect.x1 = ( fClipX1 - fSrcX1 ) * fRatioX + t.x1;
		sTexRect.x2 = ( fClipX2 - fSrcX2 ) * fRatioX + t.x2;
		sTexRect.y1 = ( fClipY1 - fSrcY1 ) * fRatioY + t.y1;
		sTexRect.y2 = ( fClipY2 - fSrcY2 ) * fRatioY + t.y2;

		NGfx::SPixel8888 color = sRect.sColor;
		if ( lrm == LRM_CLEAR_RECT )
			color = NGfx::SPixel8888( 0, 0, 0, 0 );   // retail zeroes the whole pixel

		pRes->AddRect( CTRect<float>( fClipX1, fClipY1, fClipX2, fClipY2 ), pTex, sTexRect, color, fZ );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NGScene;
