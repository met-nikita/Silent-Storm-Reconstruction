#include "StdAfx.h"
#include "DG.h"
#include "FontFormat.h"
#include "GClipper.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Dev-side DG helper (CClipper / id 0x02931400 has no retail registry entry). The clip math is the
// retail RenderRectLayoutClipped @0x148190 math over the retail 12-byte CRectLayout (per-rect
// fSizeX/fSizeY, no layout scale): Min/Max-normalized source corners, ratio-based tex-coord remap.
void ClipRects( CRectLayout *pRes, const CRectLayout &sLayout, const CTRect<int> &sWindow, const CVec2 &sPosition )
{
	pRes->rects.resize( 0 );

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

		// tex-coord remap by (tex extent / source extent) per axis -- retail @0x148190
		const CTRect<float> &t = sRect.sTex.rcTexRect;
		const float fRatioX = ( t.x2 - t.x1 ) / ( fSrcX2 - fSrcX1 );
		const float fRatioY = ( t.y2 - t.y1 ) / ( fSrcY2 - fSrcY1 );

		// NOTE: like the pre-rework code, the output rect keeps the position baked in (screen space)
		CRectLayout::SRect sClipped( sRect );
		sClipped.fX = fClipX1;
		sClipped.fY = fClipY1;
		sClipped.fSizeX = fClipX2 - fClipX1;
		sClipped.fSizeY = fClipY2 - fClipY1;
		sClipped.sTex.rcTexRect.x1 = ( fClipX1 - fSrcX1 ) * fRatioX + t.x1;
		sClipped.sTex.rcTexRect.x2 = ( fClipX2 - fSrcX2 ) * fRatioX + t.x2;
		sClipped.sTex.rcTexRect.y1 = ( fClipY1 - fSrcY1 ) * fRatioY + t.y1;
		sClipped.sTex.rcTexRect.y2 = ( fClipY2 - fSrcY2 ) * fRatioY + t.y2;

		pRes->rects.push_back( sClipped );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CClipper::Recalc()
{
	const CRectLayout &sLayout = pRects->GetValue();
	const CTRect<int> &sWindow = pWindow->GetValue();
	const CTPoint<int> &sPosition = pPosition->GetValue();

	ClipRects( &value, sLayout, sWindow, CVec2( sPosition.x, sPosition.y ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x02931400, CClipper );
