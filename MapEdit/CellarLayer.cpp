#include "StdAfx.h"
#include "MapEdit.h"
#include "Placement.h"
#include "CellarLayer.h"
#include "MEUserSettings.h"

COLORREF CELLAR_COLOR = RGB( 100, 75, 60 );
COLORREF WHITE_COLOR = RGB(255,230,215);
////////////////////////////////////////////////////////////////////////////////////////////////////
CCellarLayer::CCellarLayer()
{
	bImageShift = false;
	SetLayerID( LID_CELLAR, 0 );
	SetLayerName( "Cellar" );
	Reset();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCellarLayer::Reset()
{
	activeBrush = SBrush( -1, -1, 1, 1, 0, CELLAR_COLOR );
	SetBrush( CELLAR_COLOR );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCellarLayer::BrowseBrush()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCellarLayer::SetPlacement( CPlacement *pPlacement ) 
{
	if ( !pPlacement )
		return;
	CArray2D<bool> cellar;
	pPlacement->GetCellar( &cellar );
	int nX = pPlacement->GetWidth();
	int nY = pPlacement->GetHeight();
	if ( cellar.GetXSize() < nX || cellar.GetYSize() < nY )
	{
		cellar.SetSizes( nX, nY );
		cellar.FillEvery( 0 );
	}
	editor.SetImage( cellar.GetXSize(), cellar.GetYSize() );
	for ( int i = 0; i < nX; ++i )
		for ( int j = 0; j < nY; ++j )
			editor.SetPixelV( i, j, cellar[j][i] ? CELLAR_COLOR : WHITE_COLOR );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCellarLayer::GetCellar( CArray2D<bool> *pCellar )
{
	ASSERT( pCellar );
	pCellar->SetSizes( editor.GetWidth(), editor.GetHeight() );
	for ( int i = 0; i < editor.GetWidth(); ++i )
		for ( int j = 0; j < editor.GetHeight(); ++j )
			(*pCellar)[j][i] = editor.GetPixel( i, j ) == WHITE_COLOR? false : true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCellarLayer::OnLButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView )
{
	ASSERT( pView );

	Track( nFlags, pt, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCellarLayer::OnMouseMove(UINT nFlags, CPoint pt, ITemplateView *pView )
{
	ASSERT( pView );
	if ( nFlags & MK_LBUTTON )
		Track( nFlags, pt, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCellarLayer::Track( UINT nFlags, CPoint pt, ITemplateView *pView )
{
	ITemplateView::EPaintMode penWidth;
	EEditMode mode = pView->GetPaintMode( (int*)&penWidth );
	COLORREF cr = mode == EM_ERASE ? WHITE_COLOR : CELLAR_COLOR;
	float x, y;

	pView->ScreenToTemplate( pt, &x, &y );
	//x += 0.5f;
	//y += 0.5f;
	switch ( mode )
	{
		case EM_FILL:
			editor.Fill( x, y, cr );
			pView->Repaint();
			pView->SetModifiedFlag();
			break;
		case EM_SELECT:	
		case EM_ERASE:
			switch ( penWidth )
			{
			case ITemplateView::PM_PEN_W1:
				if ( editor.SetPixel( x, y, cr ) )
				{
					pView->SetModifiedFlag();
					pView->Repaint();
				}
				break;
			case ITemplateView::PM_PEN_W2:
				{
					x -= 0.5f;
					y -= 0.5f;
					bool bch = editor.SetPixel( x, y, cr );
					bch = editor.SetPixel( x + 1, y + 1, cr ) || bch;
					bch = editor.SetPixel( x + 1, y, cr ) || bch;
					bch = editor.SetPixel( x, y + 1, cr ) || bch;
					if ( bch )
					{
						pView->Repaint();
						pView->SetModifiedFlag();
					}
				}
				break;
			case ITemplateView::PM_PEN_W3:
				{
					x -= 2.0f;
					y -= 2.0f;
					bool bch = false;
					for ( int j = 0; j < 5; ++j )
						for ( int i = 0; i < 5; ++i )
							bch = editor.SetPixel( x + i, y + j, cr ) || bch;
						if ( bch )
						{
							pView->Repaint();
							pView->SetModifiedFlag();
						}
				}
				break;
			}	
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
