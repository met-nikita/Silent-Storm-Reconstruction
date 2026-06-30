#include "StdAfx.h"
#include "MapEdit.h"
#include "Unit&ObjDraw.h"
#include "Layers.h"
#include "RectsLayer.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
const float UNIT_SIZE = SELECTION_ACCURACY;

extern void RotatePt( CPoint &pt, int nAngle );

////////////////////////////////////////////////////////////////////////////////////////////////////
inline void operator*= ( CPoint &pt, double f )
{
  pt.x = long( f * pt.x );
  pt.y = long( f * pt.y );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static COLORREF CurrentColor( COLORREF col )
{
	const int PERIOD = 1700;
	const float NORM = FP_2PI / PERIOD;
	float fScale = 1.0f + 0.4 * (cos( NORM * (GetTickCount() % PERIOD) ) - 0.0);
	int r = GetRValue( col );
	int g = GetGValue( col );
	int b = GetBValue( col );
	return RGB( r * fScale, g * fScale, b * fScale );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitTracker::CUnitTracker( ITemplateView *pView, CRectsLayer *pLayer, const CPoint &ptCenter, int nRotation, int _nSpacing )
 : CMERectTracker( pView, pLayer, ptCenter, 2 * UNIT_SIZE * _nSpacing, 2 * UNIT_SIZE * _nSpacing, nRotation, _nSpacing, CRectTracker::solidLine )
{
	const int dAng = 150;
	CPoint ptDir( 1.5f * nSpacing, 0 );
	RotatePt( ptDir, -nRotation );
	
  dirArrow.ptBase.x =  nSpacing;
  dirArrow.ptBase.y = -nSpacing;
  dirArrow.ptDir = ptDir;
  dirArrow.ptLeft = ptDir;
  RotatePt( dirArrow.ptLeft, -dAng );
  dirArrow.ptRight = ptDir;
  RotatePt( dirArrow.ptRight, dAng );
	
  dirArrow.ptLeft *= 0.8;
  dirArrow.ptRight *= 0.8;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::Draw( CDC *pDC, int nThickness, bool bGrayed )
{
	if ( IsTracked() )
		return;
	int sz = Max( 1, int( UNIT_SIZE * nSpacing ) );
	COLORREF col = m_nStyle & CRectTracker::hatchedBorder ? CurrentColor( UNIT_SELCOLOR ) : UNIT_COLOR;
	if ( bGrayed )
		col	= GetGray( col );
	CPen pen( PS_SOLID | PS_INSIDEFRAME, sz / 2, col );
	CPen *pOldPen = pDC->SelectObject( &pen );
	CBrush brush;
	LOGBRUSH lb;
	lb.lbStyle = BS_HOLLOW;
	brush.CreateBrushIndirect( &lb );		
	CBrush *pOldBrush = pDC->SelectObject( &brush );
	//
	CRect r( ptCenter.x - sz, ptCenter.y - sz, ptCenter.x + sz, ptCenter.y + sz );
	pDC->Ellipse( &r );
	//
	pDC->SelectObject( pOldPen );
	pDC->SelectObject( pOldBrush );
	brush.DeleteObject();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::Rotate( CDC *pDC, int nAng )
{
	CMERectTracker::Rotate( pDC, nAng );
	CMERectTracker::Draw( pDC, 1, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::AdjustRect( CPoint *pCenter )
{
	pLayer->GetNearestPos( pCenter, pView );
	float x, y;
	pView->ScreenToTemplate( *pCenter, &x, &y );
	pView->TemplateToScreen( pCenter, CPoint( x + 0.5f, y + 0.5f ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitTracker::HitTest( CPoint pt )
{
	CPoint vec = pt - ptCenter;
	if (  sqr( vec.x ) + sqr( vec.y ) < sqr( UNIT_SIZE * nSpacing ) )
		return 1;
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::GetBoundsRect( CRect *pRect )
{
	const int w2 = nWidth / 4;
	const int h2 = nHeight / 4;
	pRect->left = ptCenter.x - w2;
	pRect->right = ptCenter.x + w2;
	pRect->bottom = ptCenter.y + h2;
	pRect->top = ptCenter.y - h2;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjTracker::CObjTracker( ITemplateView *pView, CRectsLayer *pLayer, const CPoint &ptCenter, 
	const CVec2 &ptSize, int nRotation, int nSpacing, COLORREF _crDraw )
: CUnitTracker( pView, pLayer, ptCenter, nRotation, nSpacing )
{
	crDraw = _crDraw;
	crSelection = OBJ_SELCOLOR;
	SetSize( Max( 1, Max( nSpacing / 2, (int)ptSize.x ) ), Max( 1, Max( nSpacing / 2, (int)ptSize.y ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjTracker::Draw( CDC *pDC, int nThickness, bool bGrayed )
{
	if ( IsTracked() )
		return;

	int sz = Max( 1, int( UNIT_SIZE * nSpacing ) );
	int sz2 = sz / 2;
	COLORREF col  = m_nStyle & CRectTracker::hatchedBorder ? CurrentColor( crSelection ) : crDraw;
	if ( bGrayed )
		col	= GetGray( col );
	CPen pen( PS_SOLID | PS_INSIDEFRAME, sz2, col );
	CPen *pOldPen = pDC->SelectObject( &pen );
	CBrush brush;
	LOGBRUSH lb;
	lb.lbStyle = BS_HOLLOW;
	brush.CreateBrushIndirect( &lb );		
	CBrush *pOldBrush = pDC->SelectObject( &brush );		
	//
	CRect r( ptCenter.x - sz, ptCenter.y - sz, ptCenter.x + sz, ptCenter.y + sz );
	pDC->RoundRect( &r, CPoint( sz2, sz2 ) );

/*
	COLORREF col  = m_nStyle & CRectTracker::hatchedBorder ? CurrentColor( crSelection ) : crDraw;
	if ( bGrayed )
		col	= GetGray( col );
	CPen pen( PS_SOLID | PS_INSIDEFRAME, nSpacing / 6, col );
	CPen *pOldPen = pDC->SelectObject( &pen );
	CBrush brush;
	LOGBRUSH lb;
	lb.lbStyle = BS_HOLLOW;
	brush.CreateBrushIndirect( &lb );		
	CBrush *pOldBrush = pDC->SelectObject( &brush );		
	//
	int nx = nWidth /2, ny = nHeight / 2;
	CRect r( ptCenter.x - nx, ptCenter.y - ny, ptCenter.x + nx, ptCenter.y + ny );
	pDC->RoundRect( &r, CPoint( nx / 2, ny / 2 ) );
	//
*/
	pDC->SelectObject( pOldPen );
	pDC->SelectObject( pOldBrush );
	brush.DeleteObject();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjTracker::AdjustRect( CPoint *pCenter )
{
//	CMERectTracker::AdjustRect( pCenter );
	pLayer->GetNearestPos( pCenter, pView );
	
	if ( !(0x8000 & GetAsyncKeyState( VK_MENU )) )
	{
		float x, y;
		pView->ScreenToTemplate( *pCenter, &x, &y );
		x = Float2Int(x);
		y = Float2Int(y);
		pView->TemplateToScreen( pCenter, x, y );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*

////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CMERectTracker::DrawArrow( CDC *pDC, const SArrow &ar, int nDrawMode )
{

  int oldMode = pDC->SetROP2( nDrawMode );
  CPen pen( PS_SOLID, 1, RGB( 230, 100, 90 ) );
  CPen *pOldPen = pDC->SelectObject( &pen );

  CPoint pt( m_rect.left, m_rect.bottom );
	
  pt += ar.ptBase;
  pDC->MoveTo( pt );
  pt += ar.ptDir;
  pDC->LineTo( pt );
  pDC->LineTo( pt + ar.ptLeft );
  pDC->MoveTo( pt );
  pDC->LineTo( pt + ar.ptRight );
	
  pDC->SelectObject( pOldPen );
  pDC->SetROP2( oldMode );

}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMERectTracker::DrawDirectionArrow( CDC *pDC )
{
  if ( !pCurDC )
    DrawArrow( pDC, dirArrow );
  else
    DrawArrow( pDC, dirArrow, R2_XORPEN );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMERectTracker::ArrowBeginRotate( CDC *pDC, int angDiscrete  )
{
  oldDir = dirArrow;
  pCurDC = pDC;
  nAngDiscrete = angDiscrete;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMERectTracker::Rotate( int nAng )
{
  if ( !pCurDC )
    return;
	SetRotation( nAng );
	pView->Invalidate();
	return;
  nAng = GetNearestPos( nAng, nAngDiscrete );
  DrawArrow( pCurDC, oldDir, R2_XORPEN );
	
  oldDir.ptDir   = dirArrow.ptDir;
  oldDir.ptLeft  = dirArrow.ptLeft;
  oldDir.ptRight = dirArrow.ptRight;
  RotatePt( oldDir.ptDir, -nAng );
  RotatePt( oldDir.ptLeft, -nAng );
  RotatePt( oldDir.ptRight, -nAng );
	
  DrawArrow( pCurDC, oldDir, R2_XORPEN );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMERectTracker::ArrowEndRotate()
{
  if ( !pCurDC )
    return;
  DrawArrow( pCurDC, oldDir, R2_XORPEN );
  DrawArrow( pCurDC, dirArrow );
  pCurDC = 0;
}
*/