#include "StdAfx.h"
#include "RectTrack.h"
#include "MapEdit.h"
#include "Layers.h"
#include "RectsLayer.h"

extern void RotatePt( CPoint &pt, int nAngle );
void DrawDragPoly( CDC *pDC, LPPOINT lpPoints, int nCount, int size, LPPOINT lpPointsLast, int nCountLast );

HBRUSH _afxHatchBrush = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////
CMERectTracker::CMERectTracker( ITemplateView *_pView, 
															 CRectsLayer *_pLayer, 
															 const CPoint &_ptCenter, 
															 int _nWidth, 
															 int _nHeight, 
															 int _nRotation,
															 int spacing, 
															 UINT nStyle )
    : nWidth( _nWidth ), nHeight( _nHeight ), m_nStyle(nStyle), 
		pView(_pView), 
		pLayer( _pLayer ),
		nSpacing(spacing), ptCenter( _ptCenter ), bTracked( false )
{
	nRotation = _nRotation;
	SetRotation( nRotation );

	memset( ptPolyOld, 0, sizeof(ptPolyOld) );

	if (_afxHatchBrush == NULL)
	{
		// create the hatch pattern + bitmap
		WORD hatchPattern[8];
		WORD wPattern = 0x1111;
		for (int i = 0; i < 4; i++)
		{
			hatchPattern[i] = wPattern;
			hatchPattern[i+4] = wPattern;
			wPattern <<= 1;
		}
		HBITMAP hatchBitmap = CreateBitmap(8, 8, 1, 1, &hatchPattern);
		
		// create black hatched brush
		_afxHatchBrush = CreatePatternBrush(hatchBitmap);
		DeleteObject(hatchBitmap);
	}
	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CMERectTracker::MakePoly()
{
	for ( int i = 0; i < 4; ++i )
		ptPoly[i] = ptCorners[i] + ptCenter;
	ptPoly[4] = ptPoly[0];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
UINT CMERectTracker::GetHandleMask( ) const
{
	//  return 0xF;
  return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMERectTracker::AdjustRect( CPoint *pCenter )
{
	pLayer->GetNearestPos( pCenter, pView );
	
	if ( !(0x8000 & GetAsyncKeyState( VK_MENU )) )
	{
		float x, y;
		pView->ScreenToTemplate( *pCenter, &x, &y );
		x = int( nWidth / nSpacing ) & 0x1 ? int(x) + 0.5 : int(x + 0.5f);
		y = int( nHeight / nSpacing ) & 0x1 ? int(y) + 0.5 : int(y + 0.5f);
		pView->TemplateToScreen( pCenter, x, y );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMERectTracker::Rotate( CDC *pDC, int nAng )
{
	SetRotation( nAng );
	pView->Repaint();
  nAng = GetNearestPos( nAng, nAngDiscrete );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMERectTracker::Draw( CDC *pDC, int nThickness, bool bGrayed )
{
	int i;

	if ( CRectTracker::hatchedBorder & m_nStyle )
	{
		DrawDragPoly( pDC, ptPoly, 4, 3, 0, 0 );
	}
	else
	{
		COLORREF cr = bGrayed ? RGB( 140, 140, 140 ) : RGB( 0, 0, 0 );
		CPen pen( PS_SOLID, nThickness, cr );
		CPen *pOld = pDC->SelectObject( &pen );
		pDC->Polyline( ptPoly, 5 );
		pDC->SelectObject( pOld );
	}
	for ( i = 0; i < 4; ++i )
		ptPolyOld[i] = ptPoly[i];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMERectTracker::SetRotation( int nAngle )
{
	nRotation = nAngle % 360;
	ptCorners[0] = CPoint( -nWidth / 2, -nHeight / 2 );
	ptCorners[1] = CPoint(  nWidth / 2, -nHeight / 2 );
	ptCorners[2] = CPoint(  nWidth / 2, nHeight / 2 );
	ptCorners[3] = CPoint( -nWidth / 2, nHeight / 2 );
	
	for ( int i = 0; i < 4; ++i )
	{
		RotatePt( ptCorners[i], -nRotation );
	}
	MakePoly();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMERectTracker::SetSize( int nX, int nY )
{
	nWidth  = nX;
	nHeight = nY;
	SetRotation( nRotation );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMERectTracker::Track( CWnd* pWnd, CPoint point, bool bAllowInvert )
{
  // don't handle if capture already set
  if (::GetCapture() != NULL)
    return 0;
  
  // set capture to the window which received this message
  pWnd->SetCapture();
  
  // get DC for drawing
  CDC* pDrawDC = pWnd->GetDC();
  ASSERT_VALID(pDrawDC);

  pWnd->Invalidate( false );
  pWnd->UpdateWindow();
	bTracked = true;
	
  CPoint pt, ptDelta;
  GetCursorPos( &ptDelta );
  pWnd->ScreenToClient( &ptDelta );
	ptDelta = ptCenter - ptDelta;

	int i;
	DrawDragPoly( pDrawDC, ptPoly, 4, 3, ptPolyOld, 4 );
	CPoint ptSave = ptCenter;	
	
  // get messages until capture lost or cancelled/accepted
  for (;;)
  {
    MSG msg;
    VERIFY(::GetMessage(&msg, NULL, 0, 0));
    
    if ( CWnd::GetCapture() != pWnd || WM_LBUTTONUP == msg.message )
      break;
		
    switch (msg.message)
    {
      // handle movement/accept messages
    case WM_MOUSEMOVE:
      pt.y = (int)(short)HIWORD(msg.lParam);
			pt.x = (int)(short)LOWORD(msg.lParam);
			ptCenter = pt + ptDelta;
			AdjustRect( &ptCenter );
      break;
      // handle cancel messages
    case WM_KEYDOWN:
      if (msg.wParam != VK_ESCAPE)
        break;
    case WM_RBUTTONDOWN:
			ptCenter = ptSave;
			AdjustRect( &ptCenter );
			pWnd->ReleaseDC(pDrawDC);
			ReleaseCapture();
			MakePoly();
			bTracked = false;
      return false;
      // just dispatch rest of the messages
    default:
      DispatchMessage(&msg);
      break;
    }
		MakePoly();
		DrawDragPoly( pDrawDC, ptPoly, 4, 4, ptPolyOld, 4 );
		for ( i = 0; i < 4; ++i )
			ptPolyOld[i] = ptPoly[i];
  }
  pWnd->ReleaseDC(pDrawDC);
  ReleaseCapture();
	MakePoly();
	bTracked = false;
	return ptSave != ptCenter;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool FindInternalPt( CPoint *ppt, float width, const CVec2 &pt1, const CVec2 &pt2, const CVec2 &pt3 )
{
	CVec2 line1 = pt2 - pt1;
	CVec2 line2 = pt3 - pt2;
	
	Normalize( &line1 );
	Normalize( &line2 );
	
	CVec2 pt2_1 = pt2 + width * CVec2( -line1.y, line1.x );
	CVec2 pt2_2 = pt2 + width * CVec2( -line2.y, line2.x );
	
	float det = line1.x * line2.y - line1.y * line2.x;
	if ( fabs( det ) < FP_EPSILON )
		return false;
	float t = ( pt2_1.y * line2.x - pt2_1.x * line2.y  + pt2_2.x * line2.y - pt2_2.y * line2.x ) / det;
	
	ppt->x = pt2_1.x + line1.x * t;
	ppt->y = pt2_1.y + line1.y * t;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void DrawDragPoly( CDC *pDC, LPPOINT lpPoints, int nCount, int size, LPPOINT lpPointsLast, int nCountLast )
{
	// first, determine the update region and select it
	vector<CPoint> insidePts( nCount );
	int i;
	int p1 = nCount - 1;
	int p2 = 0;
	int p3 = 1;
	for ( i = 0; i < nCount; ++i )
	{
		p1 %= nCount;
		p3 %= nCount;
		FindInternalPt( &insidePts[i], size, 
			CVec2( lpPoints[p1].x, lpPoints[p1].y ),
			CVec2( lpPoints[p2].x, lpPoints[p2].y ),
			CVec2( lpPoints[p3].x, lpPoints[p3].y ) );
		++p1, ++p2, ++p3;
	}
	CRgn rgnNew;
	{
		CRgn rgnOutside, rgnInside;
		rgnOutside.CreatePolygonRgn( lpPoints, nCount, WINDING );
		rgnInside.CreatePolygonRgn( &insidePts[0], nCount, WINDING );
		rgnNew.CreateRectRgn(0, 0, 0, 0);
		rgnNew.CombineRgn(&rgnOutside, &rgnInside, RGN_XOR);
		//
		rgnOutside.DeleteObject();
		rgnInside.DeleteObject();
	}
	
	CRgn rgnLast, rgnUpdate;

	if (lpPointsLast != NULL)
	{
		// find difference between new region and old region
		rgnLast.CreateRectRgn(0, 0, 0, 0);
		int p1 = nCountLast - 1;
		int p2 = 0;
		int p3 = 1;
		for ( i = 0; i < nCountLast; ++i )
		{
			p1 %= nCountLast;
			p3 %= nCountLast;
			FindInternalPt( &insidePts[i], size, 
				CVec2( lpPointsLast[p1].x, lpPointsLast[p1].y ),
				CVec2( lpPointsLast[p2].x, lpPointsLast[p2].y ),
				CVec2( lpPointsLast[p3].x, lpPointsLast[p3].y ) );
			++p1, ++p2, ++p3;
		}
		CRgn rgnOutside, rgnInside;
		rgnOutside.CreatePolygonRgn( lpPointsLast, nCount, WINDING );
		rgnInside.CreatePolygonRgn( &insidePts[0], nCount, WINDING );
		rgnLast.CombineRgn(&rgnOutside, &rgnInside, RGN_XOR);
		rgnUpdate.CreateRectRgn(0, 0, 0, 0);
		rgnUpdate.CombineRgn(&rgnLast, &rgnNew, RGN_XOR);
		//
		rgnOutside.DeleteObject();
		rgnInside.DeleteObject();
	}

	// draw into the update/new region
	CRect rect;
	pDC->SelectClipRgn(rgnUpdate.m_hObject != NULL ? &rgnUpdate : &rgnNew);
	pDC->GetClipBox(&rect);
	CBrush *pBrush = CDC::GetHalftoneBrush();
//	CBrush *pBrush = CBrush::FromHandle(_afxHatchBrush);
	CBrush *pBrushOld = pDC->SelectObject(pBrush);
	pDC->PatBlt(rect.left, rect.top, rect.Width(), rect.Height(), PATINVERT);
	
	// cleanup DC
	if (pBrushOld != NULL)
		pDC->SelectObject(pBrushOld);
	pDC->SelectClipRgn(NULL);

	rgnNew.DeleteObject();
	rgnLast.DeleteObject();
	rgnUpdate.DeleteObject();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMERectTracker::HitTest( CPoint pt )
{
	CPoint pt1 = ptPoly[1] - ptPoly[0];
	CPoint pt2 = ptPoly[2] - ptPoly[1];
	CPoint pt3 = ptPoly[3] - ptPoly[2];
	CPoint pt4 = ptPoly[0] - ptPoly[3];

	if ( pt1.x * (pt.y - ptPoly[0].y) - pt1.y * ( pt.x - ptPoly[0].x ) > 0
		&& pt2.x * (pt.y - ptPoly[1].y) - pt2.y * ( pt.x - ptPoly[1].x ) > 0
		&& pt3.x * (pt.y - ptPoly[2].y) - pt3.y * ( pt.x - ptPoly[2].x ) > 0
		&& pt4.x * (pt.y - ptPoly[3].y) - pt4.y * ( pt.x - ptPoly[3].x ) > 0
		)
		return 1;
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMERectTracker::Move( const CPoint &ptDelta )
{
	ptCenter += ptDelta;
	MakePoly();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMERectTracker::MoveTo( const CPoint &_ptCenter )
{
	ptCenter = _ptCenter;
	MakePoly();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMERectTracker::GetBoundsRect( CRect *pRect )
{
	pRect->left   = pRect->right = ptPoly[0].x;
	pRect->bottom = pRect->top   = ptPoly[0].y;
	//
	for ( int i = 1; i < 4; ++i )
	{
		pRect->left   = Min( pRect->left, ptPoly[i].x );
		pRect->right  = Max( pRect->right, ptPoly[i].x );
		pRect->top    = Min( pRect->top, ptPoly[i].y );
		pRect->bottom = Max( pRect->bottom, ptPoly[i].y );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMERectTracker::SetCursor( CWnd* pWnd, UINT nHitTest ) const
{
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMERectTracker::TrackRotate( CWnd* pWnd, int angDiscrete )
{
  const float ROTATE_SPEED = 1.0f;
  // don't handle if capture already set
  if (::GetCapture() != NULL)
    return 0;
  
  // set capture to the window which received this message
  pWnd->SetCapture();

  // get DC for drawing
  CDC* pDrawDC =pWnd-> GetDC();
  ASSERT_VALID(pDrawDC);
	bTracked = true;
	nAngDiscrete = angDiscrete;
  BeginRotate( pDrawDC );

  pWnd->Invalidate( false );
  pWnd->UpdateWindow();
	
  float fAng = GetRotation();
	float fAngOld = GetRotation();
	float fAngCur = fAng;
  CPoint pt, ptOld;
  GetCursorPos( &ptOld );
  pWnd->ScreenToClient( &ptOld );
	
  // get messages until capture lost or cancelled/accepted
  for (;;)
  {
    MSG msg;
    VERIFY(::GetMessage(&msg, NULL, 0, 0));
    
    if ( CWnd::GetCapture() != pWnd || WM_LBUTTONUP == msg.message )
      break;
		
    switch (msg.message)
    {
      // handle movement/accept messages
    case WM_MOUSEMOVE:
      pt.y = (int)(short)HIWORD(msg.lParam);
      fAngCur += ROTATE_SPEED * (ptOld.y - pt.y);
			if ( 0x8000 & GetAsyncKeyState( VK_CONTROL ) )
				fAng = ::GetNearestPos( (int)fAngCur, angDiscrete );
			else
				fAng = fAngCur;
      Rotate( pDrawDC,(int)fAng );
      ptOld.y = pt.y;
      break;
      // handle cancel messages
    case WM_KEYDOWN:
      if (msg.wParam != VK_ESCAPE)
        break;
    case WM_RBUTTONDOWN:
			EndRotate( pDrawDC );
			pWnd->ReleaseDC(pDrawDC);
			ReleaseCapture();
			Rotate( pDrawDC, (int)fAngOld );
			bTracked = false;
      return fAngOld;
      // just dispatch rest of the messages
    default:
      DispatchMessage(&msg);
      break;
    }
  }
  EndRotate( pDrawDC );
  pWnd->ReleaseDC(pDrawDC);
  ReleaseCapture();
	bTracked = false;
  return fAng;
}
