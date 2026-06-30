#include "StdAfx.h"
#include "GRects.h"
#include "GfxUtils.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
void RenderRectLayout( NGfx::C2DQuadsRenderer *pRes, NGfx::CTexture *pTex, const CRectLayout &l, float fZ, ELayoutRenderMode lrm )
{
	for ( int i = 0; i < l.rects.size(); ++i )
	{
		const CRectLayout::SRect &r = l.rects[i];
		NGfx::SPixel8888 color = r.sColor;
		if ( lrm == LRM_CLEAR_RECT )
			color.a = 0;
		CTRect<float> rTarget( r.fX, r.fY, r.fX + abs( r.sTex.GetWidth() ) * l.scale.x, r.fY + abs( r.sTex.GetHeight() ) * l.scale.y );
		CTRect<float> rSrc( r.sTex.rcTexRect ); 
		pRes->AddRect( rTarget, pTex, rSrc, color, fZ );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void RenderRectLayoutClipped( NGfx::C2DQuadsRenderer *pRes, NGfx::CTexture *pTex, const CRectLayout &sLayout, const CTPoint<int> &sPosition, const CTRect<int> &sWindow, float fZ, ELayoutRenderMode lrm )
{
	for ( int nTemp = 0; nTemp < sLayout.rects.size(); nTemp++ )
	{
		const CRectLayout::SRect &sRect = sLayout.rects[nTemp];

		CTRect<float> sSourceRect;
		sSourceRect.x1 = sRect.fX + sPosition.x;
		sSourceRect.y1 = sRect.fY + sPosition.y;
		sSourceRect.x2 = sRect.fX + sPosition.x + /*Float2Int*/( fabsf( sRect.sTex.rcTexRect.Width() ) * sLayout.scale.x );
		sSourceRect.y2 = sRect.fY + sPosition.y + /*Float2Int*/( fabsf( sRect.sTex.rcTexRect.Height() ) * sLayout.scale.y );

		int nXSign = Sign( sRect.sTex.rcTexRect.Width() );
		int nYSign = Sign( sRect.sTex.rcTexRect.Height() );
		CTRect<float> sClippedRect ( sSourceRect );

		if ( sClippedRect.x1 < sWindow.x1 )
			sClippedRect.x1 = sWindow.x1;
		if ( sClippedRect.x1 > sWindow.x2 )
			sClippedRect.x1 = sWindow.x2;
		if ( sClippedRect.x2 < sWindow.x1 )
			sClippedRect.x2 = sWindow.x1;
		if ( sClippedRect.x2 > sWindow.x2 )
			sClippedRect.x2 = sWindow.x2;
		if ( sClippedRect.y1 < sWindow.y1 )
			sClippedRect.y1 = sWindow.y1;
		if ( sClippedRect.y1 > sWindow.y2 )
			sClippedRect.y1 = sWindow.y2;
		if ( sClippedRect.y2 < sWindow.y1 )
			sClippedRect.y2 = sWindow.y1;
		if ( sClippedRect.y2 > sWindow.y2 )
			sClippedRect.y2 = sWindow.y2;

		if ( ( sClippedRect.Width() <= 0 ) || ( sClippedRect.Height() <= 0 ) )
			continue;

		CRectLayout::SRect sClipped( sRect );
		sClipped.fX = Min( sClippedRect.x1, sClippedRect.x2 );
		sClipped.fY = Min( sClippedRect.y1, sClippedRect.y2 );
		sClipped.sTex.rcTexRect.x1 -= /*Float2Int*/( ( sSourceRect.x1 - sClippedRect.x1 ) * nXSign / sLayout.scale.x );
		sClipped.sTex.rcTexRect.y1 -= /*Float2Int*/( ( sSourceRect.y1 - sClippedRect.y1 ) * nYSign / sLayout.scale.y );
		sClipped.sTex.rcTexRect.x2 -= /*Float2Int*/( ( sSourceRect.x2 - sClippedRect.x2 ) * nXSign / sLayout.scale.x );
		sClipped.sTex.rcTexRect.y2 -= /*Float2Int*/( ( sSourceRect.y2 - sClippedRect.y2 ) * nYSign / sLayout.scale.y );

		NGfx::SPixel8888 color = sClipped.sColor;
		if ( lrm == LRM_CLEAR_RECT )
			color.a = 0;

//		CTRect<float> rSrc( r.sTex.rcTexRect ); 
//		CTRect<float> rTarget( r.fX, r.fY, r.fX + abs( r.sTex.GetWidth() ) * l.scale.x, r.fY + abs( r.sTex.GetHeight() ) * l.scale.y );
		pRes->AddRect( sClippedRect, pTex, sClipped.sTex.rcTexRect, color, fZ );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NGScene;
