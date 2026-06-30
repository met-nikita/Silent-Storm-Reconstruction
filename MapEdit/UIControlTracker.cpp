#include "StdAfx.h"

class CPropMap;
#include "UIView.h"

static CBrush hatchedBrush;
AFX_STATIC_DATA HCURSOR _afxCursors[10] = { 0, };
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CreateBrush()
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
	if (hatchBitmap == NULL)
		return;

	hatchedBrush.CreatePatternBrush( CBitmap::FromHandle( hatchBitmap ) );

	// Note: all track cursors must live in same module
	HINSTANCE hInst = AfxFindResourceHandle(
		MAKEINTRESOURCE(AFX_IDC_TRACK4WAY), RT_GROUP_CURSOR);

	// initialize the cursor array
	_afxCursors[0] = ::LoadCursor(hInst, MAKEINTRESOURCE(AFX_IDC_TRACKNWSE));
	_afxCursors[1] = ::LoadCursor(hInst, MAKEINTRESOURCE(AFX_IDC_TRACKNESW));
	_afxCursors[2] = _afxCursors[0];
	_afxCursors[3] = _afxCursors[1];
	_afxCursors[4] = ::LoadCursor(hInst, MAKEINTRESOURCE(AFX_IDC_TRACKNS));
	_afxCursors[5] = ::LoadCursor(hInst, MAKEINTRESOURCE(AFX_IDC_TRACKWE));
	_afxCursors[6] = _afxCursors[4];
	_afxCursors[7] = _afxCursors[5];
	_afxCursors[8] = ::LoadCursor(hInst, MAKEINTRESOURCE(AFX_IDC_TRACK4WAY));
	_afxCursors[9] = ::LoadCursor(hInst, MAKEINTRESOURCE(AFX_IDC_MOVE4WAY));
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTracker::CTracker(): CRectTracker()
{
	if ( !hatchedBrush.m_hObject )
		CreateBrush();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTracker::CTracker( LPCRECT lpSrcRect, UINT nStyle, CString _szText, const CPoint &_ptSpacing, CScrollView *_pView )
: CRectTracker( lpSrcRect, nStyle )
{
	szText = _szText;
	ptSpacing = _ptSpacing;
	pView = _pView;
	if ( !hatchedBrush.m_hObject )
		CreateBrush();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTracker::Draw(CDC* pDC, bool bBk, int nDepth ) const
{
	// set initial DC state
	VERIFY(pDC->SaveDC() != 0);

	// get normalized rectangle
	CRect rect = m_rect;
	rect.NormalizeRect();

	CPen* pOldPen = NULL;
	CBrush* pOldBrush = NULL;
	CGdiObject* pTemp;
	int nOldROP;
	const COLORREF crDotted = RGB(200, 0, 0);
	CPen pen( PS_DOT, 0, crDotted );

	// draw lines
	if ((m_nStyle & (dottedLine|solidLine)) != 0)
	{
		if (m_nStyle & dottedLine)
		{

			pOldPen = pDC->SelectObject( &pen );
		}
		else
			pOldPen = (CPen*)pDC->SelectStockObject(BLACK_PEN);
		//pOldBrush = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
		COLORREF cr = GetSysColor( COLOR_BTNFACE );
		int dcr = -16 * nDepth;
		cr = RGB( GetRValue(cr) + dcr, GetGValue(cr) + dcr, GetBValue(cr) + dcr );
		CBrush br( cr );
		if ( bBk )
			pOldBrush = (CBrush*)pDC->SelectObject( &br );
		else
			pOldBrush = (CBrush*)pDC->SelectStockObject( NULL_BRUSH );
		nOldROP = pDC->SetROP2(R2_COPYPEN);
		rect.InflateRect(+1, +1);   // borders are one pixel outside
		pDC->Rectangle(rect.left, rect.top, rect.right, rect.bottom);
		pDC->SetROP2(nOldROP);
	}

	// if hatchBrush is going to be used, need to unrealize it
	if ((m_nStyle & (hatchInside|hatchedBorder)) != 0)
		hatchedBrush.UnrealizeObject();

	// hatch inside
	if ((m_nStyle & hatchInside) != 0)
	{
		pTemp = pDC->SelectStockObject(NULL_PEN);
		if (pOldPen == NULL)
			pOldPen = (CPen*)pTemp;
		pTemp = pDC->SelectObject( &hatchedBrush );
		if (pOldBrush == NULL)
			pOldBrush = (CBrush*)pTemp;
		pDC->SetBkMode(TRANSPARENT);
		nOldROP = pDC->SetROP2(R2_MASKNOTPEN);
		pDC->Rectangle(rect.left+1, rect.top+1, rect.right, rect.bottom);
		pDC->SetROP2(nOldROP);
	}

	// draw hatched border
	if ((m_nStyle & hatchedBorder) != 0)
	{
		pTemp = pDC->SelectObject( &hatchedBrush );
		if (pOldBrush == NULL)
			pOldBrush = (CBrush*)pTemp;
		pDC->SetBkMode(OPAQUE);
		CRect rectTrue;
		GetTrueRect(&rectTrue);
		pDC->PatBlt(rectTrue.left, rectTrue.top, rectTrue.Width(),
			rect.top-rectTrue.top, 0x000F0001 /* Pn */);
		pDC->PatBlt(rectTrue.left, rect.bottom,
			rectTrue.Width(), rectTrue.bottom-rect.bottom, 0x000F0001 /* Pn */);
		pDC->PatBlt(rectTrue.left, rect.top, rect.left-rectTrue.left,
			rect.Height(), 0x000F0001 /* Pn */);
		pDC->PatBlt(rect.right, rect.top, rectTrue.right-rect.right,
			rect.Height(), 0x000F0001 /* Pn */);
	}

	// draw resize handles
	if ((m_nStyle & (resizeInside|resizeOutside)) != 0)
	{
		UINT mask = GetHandleMask();
		COLORREF cr = m_nStyle & dottedLine ? crDotted : RGB(0, 0, 0);
		for (int i = 0; i < 8; ++i)
		{
			if (mask & (1<<i))
			{
				GetHandleRect((TrackerHit)i, &rect);
				pDC->FillSolidRect(rect, cr);
			}
		}
	}
	const CPoint ptCenter = m_rect.CenterPoint();
	TEXTMETRIC tm;
	Zero( tm );
	pDC->GetTextMetrics( &tm );
	COLORREF crOld = pDC->SetTextColor( m_nStyle & dottedLine ? crDotted : pDC->GetTextColor() );
	int nOldMode = pDC->SetBkMode( OPAQUE );
	CBrush br( GetSysColor( COLOR_BTNFACE ) );
	pOldBrush = (CBrush*)pDC->SelectObject( &br );
	pDC->SetBkColor( GetSysColor( COLOR_BTNFACE ) );
	pDC->ExtTextOut( ptCenter.x, ptCenter.y - tm.tmHeight / 2, ETO_CLIPPED /*| ETO_OPAQUE*/, m_rect, szText, 0 );
	pDC->SetBkMode( nOldMode );
	pDC->SetTextColor( crOld );
	pDC->SelectObject( pOldBrush );

	// cleanup pDC state
	if (pOldPen != NULL)
		pDC->SelectObject(pOldPen);
	if (pOldBrush != NULL)
		pDC->SelectObject(pOldBrush);
	VERIFY(pDC->RestoreDC(-1));
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CTracker::SetCursor(CPoint point, UINT nHitTest) const
{
	// trackers should only be in client area
	if (nHitTest != HTCLIENT)
		return FALSE;

	// do hittest and normalize hit
	int nHandle = HitTestHandles(point);
	if (nHandle < 0)
		return FALSE;

	// need to normalize the hittest such that we get proper cursors
	nHandle = NormalizeHit(nHandle);

	// handle special case of hitting area between handles
	//  (logically the same -- handled as a move -- but different cursor)
	if (nHandle == hitMiddle && !m_rect.PtInRect(point))
	{
		// only for trackers with hatchedBorder (ie. in-place resizing)
		if (m_nStyle & hatchedBorder)
			nHandle = (TrackerHit)9;
	}

	::SetCursor(_afxCursors[nHandle]);
	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CPoint CheckBoundRect( const CRect *lpRect, const CSize &size )
{
	int xmax = Max( lpRect->left, lpRect->right );
	int ymax = Max( lpRect->top, lpRect->bottom );
	return CPoint( xmax > size.cx ? size.cx - xmax : 0, ymax > size.cy ? size.cy - ymax : 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTracker::AdjustRect(int nHandle, CRect *lpRect)
{
	if ( !pView )
		return;
	//
	const CSize size = pView->GetTotalSize();

	if ( 0x8000 & GetAsyncKeyState( VK_MENU ) )
	{
		CPoint dpt = CheckBoundRect( lpRect, size );
		lpRect->OffsetRect( dpt );
		return;
	}

	CPoint dpt;

  switch( nHandle )
  {
  case 0:
    lpRect->left = GetNearestPos( lpRect->left, ptSpacing.x );
    lpRect->top  = GetNearestPos( lpRect->top, ptSpacing.y );
		dpt = CheckBoundRect( lpRect, size );
		lpRect->left += dpt.x;
		lpRect->top  += dpt.y;
    break;
  case 1:
    lpRect->right = GetNearestPos( lpRect->right, ptSpacing.x );
    lpRect->top   = GetNearestPos( lpRect->top, ptSpacing.y );
		dpt = CheckBoundRect( lpRect, size );
		lpRect->right += dpt.x;
		lpRect->top   += dpt.y;
    break;
  case 2:
    lpRect->right  = GetNearestPos( lpRect->right, ptSpacing.x );
    lpRect->bottom = GetNearestPos( lpRect->bottom, ptSpacing.y );
		dpt = CheckBoundRect( lpRect, size );
		lpRect->right   += dpt.x;
		lpRect->bottom  += dpt.y;
    break;
  case 3:
    lpRect->left   = GetNearestPos( lpRect->left, ptSpacing.x );
    lpRect->bottom = GetNearestPos( lpRect->bottom, ptSpacing.y );
		dpt = CheckBoundRect( lpRect, size );
		lpRect->left += dpt.x;
		lpRect->bottom  += dpt.y;
    break;
  case 4:
    lpRect->top    = GetNearestPos( lpRect->top, ptSpacing.y );
		dpt = CheckBoundRect( lpRect, size );
		lpRect->top  += dpt.y;
    break;
  case 5:
    lpRect->right  = GetNearestPos( lpRect->right, ptSpacing.x );
		dpt = CheckBoundRect( lpRect, size );
		lpRect->right += dpt.x;
    break;
  case 6:
    lpRect->bottom = GetNearestPos( lpRect->bottom, ptSpacing.y );
		dpt = CheckBoundRect( lpRect, size );
		lpRect->bottom += dpt.y;
    break;
  case 7:
    lpRect->left = GetNearestPos( lpRect->left, ptSpacing.x );
		dpt = CheckBoundRect( lpRect, size );
		lpRect->left += dpt.x;
    break;
  case 8:
    {
			CPoint dpt;

			dpt.x = GetNearestPos( lpRect->left, ptSpacing.x ) - lpRect->left;
			dpt.y = GetNearestPos( lpRect->top, ptSpacing.y ) - lpRect->top;
			lpRect->OffsetRect( dpt );
			dpt = CheckBoundRect( lpRect, size );
			lpRect->OffsetRect( dpt );
    }
    break;
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CTracker::Track(CWnd* pWnd, CPoint point, BOOL bAllowInvert,
	CWnd* pWndClipTo)
{
	// perform hit testing on the handles
	int nHandle = HitTestHandles(point);
	if (nHandle < 0)
	{
		// didn't hit a handle, so just return FALSE
		return FALSE;
	}

	// otherwise, call helper function to do the tracking
	m_bAllowInvert = bAllowInvert;
	return TrackHandle(nHandle, pWnd, point, pWndClipTo);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CTracker::TrackHandle(int nHandle, CWnd* pWnd, CPoint point,
	CWnd* pWndClipTo)
{
	ASSERT(nHandle >= 0);
	ASSERT(nHandle <= 8);   // handle 8 is inside the rect

	// don't handle if capture already set
	if (::GetCapture() != NULL)
		return FALSE;

	AfxLockTempMaps();  // protect maps while looping

	ASSERT(!m_bFinalErase);

	// save original width & height in pixels
	int nWidth = m_rect.Width();
	int nHeight = m_rect.Height();

	// set capture to the window which received this message
	pWnd->SetCapture();
	ASSERT(pWnd == CWnd::GetCapture());
	pWnd->UpdateWindow();
	if (pWndClipTo != NULL)
		pWndClipTo->UpdateWindow();
	CRect rectSave = m_rect;

	// find out what x/y coords we are supposed to modify
	int *px, *py;
	int xDiff, yDiff;
	GetModifyPointers(nHandle, &px, &py, &xDiff, &yDiff);
	xDiff = point.x - xDiff;
	yDiff = point.y - yDiff;
	CPoint ptScroll = pView ? pView->GetScrollPosition() : CPoint(0,0);

	// get DC for drawing
	CDC* pDrawDC;
	if (pWndClipTo != NULL)
	{
		// clip to arbitrary window by using adjusted Window DC
		pDrawDC = pWndClipTo->GetDCEx(NULL, DCX_CACHE);
	}
	else
	{
		// otherwise, just use normal DC
		pDrawDC = pWnd->GetDC();
	}
	ASSERT_VALID(pDrawDC);

	CRect rectOld;
	BOOL bMoved = FALSE;

	// get messages until capture lost or cancelled/accepted
	for (;;)
	{
		MSG msg;
		VERIFY(::GetMessage(&msg, NULL, 0, 0));

		if (CWnd::GetCapture() != pWnd)
			break;

		switch (msg.message)
		{
		// handle movement/accept messages
		case WM_LBUTTONUP:
		case WM_MOUSEMOVE:
			rectOld = m_rect;
			// handle resize cases (and part of move)
			if (px != NULL)
				*px = (int)(short)LOWORD(msg.lParam) - xDiff + ptScroll.x;
			if (py != NULL)
				*py = (int)(short)HIWORD(msg.lParam) - yDiff + ptScroll.y;

			// handle move case
			if (nHandle == hitMiddle)
			{
				m_rect.right = m_rect.left + nWidth;
				m_rect.bottom = m_rect.top + nHeight;
			}
			// allow caller to adjust the rectangle if necessary
			AdjustRect(nHandle, &m_rect);

			// only redraw and callback if the rect actually changed!
			m_bFinalErase = (msg.message == WM_LBUTTONUP);
			if (!rectOld.EqualRect(&m_rect) || m_bFinalErase)
			{
				if (bMoved)
				{
					m_bErase = TRUE;
					rectOld -= ptScroll;
					DrawTrackerRect(&rectOld, pWndClipTo, pDrawDC, pWnd);
					rectOld += ptScroll;
				}
				OnChangedRect(rectOld);
				if (msg.message != WM_LBUTTONUP)
					bMoved = TRUE;
			}
			if (m_bFinalErase)
				goto ExitLoop;

			if (!rectOld.EqualRect(&m_rect))
			{
				m_bErase = FALSE;
				m_rect -= ptScroll;
				DrawTrackerRect(&m_rect, pWndClipTo, pDrawDC, pWnd);
				m_rect += ptScroll;
			}
			break;

		// handle cancel messages
		case WM_KEYDOWN:
			if (msg.wParam != VK_ESCAPE)
				break;
		case WM_RBUTTONDOWN:
			if (bMoved)
			{
				m_bErase = m_bFinalErase = TRUE;
				DrawTrackerRect(&m_rect, pWndClipTo, pDrawDC, pWnd);
			}
			m_rect = rectSave;
			goto ExitLoop;

		// just dispatch rest of the messages
		default:
			DispatchMessage(&msg);
			break;
		}
	}

ExitLoop:
	if (pWndClipTo != NULL)
		pWndClipTo->ReleaseDC(pDrawDC);
	else
		pWnd->ReleaseDC(pDrawDC);
	ReleaseCapture();

	AfxUnlockTempMaps(FALSE);

	// restore rect in case bMoved is still FALSE
	if (!bMoved)
		m_rect = rectSave;
	m_bFinalErase = FALSE;
	m_bErase = FALSE;

	// return TRUE only if rect has changed
	return !rectSave.EqualRect(&m_rect);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
