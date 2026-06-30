#include "StdAfx.h"
#include "DG.h"
#include "FontFormat.h"
#include "GClipper.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
void ClipRects( CRectLayout *pRes, const CRectLayout &sLayout, const CTRect<int> &sWindow, const CVec2 &sPosition )
{
	pRes->rects.resize( 0 );
	pRes->scale = sLayout.scale;

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
