#include "StdAfx.h"
#include "IconView.h"
#include "PropMap.h"
#include "MapEdit.h"
#include "ItemsMgr.h"

const int ICON_WIDTH  = 100;
const int ICON_HEIGHT = 100;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjIcon
BEGIN_MESSAGE_MAP(CObjIcon, CWnd)
//{{AFX_MSG_MAP(CObjIcon)
	ON_WM_PAINT()
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjIcon construction/destruction
CObjIcon::CObjIcon()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjIcon::~CObjIcon()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CObjIcon::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	CDC *pDC = GetDC();
	m_bitmap.CreateCompatibleBitmap( pDC, ICON_WIDTH, ICON_HEIGHT );
	//
	CBitmap* pOldBitmap = pDC->SelectObject( &m_bitmap );
	pDC->FillSolidRect( 0, 0, ICON_WIDTH, ICON_HEIGHT, 0 );
	pDC->SelectObject( pOldBitmap );
	//
	ReleaseDC( pDC );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjIcon::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	CRect r;
	GetClientRect( &r );
	dc.FillSolidRect( &r, RGB( 10, 0, 0 ) );
	//
	CDC dcMem;
	dcMem.CreateCompatibleDC( &dc );

	CBitmap* pOldBitmap = dcMem.SelectObject( &m_bitmap );
	dc.BitBlt( 0, 0, ICON_WIDTH, ICON_HEIGHT, &dcMem, 0, 0, SRCCOPY );
	dcMem.SelectObject( pOldBitmap );
	// Do not call CWnd::OnPaint() for painting messages
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// CIconView

BEGIN_MESSAGE_MAP(CIconView, SECControlBar)
//{{AFX_MSG_MAP(CIconView)
	ON_WM_SIZE()
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CIconView construction/destruction

CIconView::CIconView()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CIconView::~CIconView()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CIconView::SetObject( const SResTree *pTree, int nObjID )
{
	CDC *pDC = m_view.GetDC();
	CDC dcMem;
	dcMem.CreateCompatibleDC( pDC );
	m_view.ReleaseDC( pDC );
	
	CBitmap* pOldBitmap = dcMem.SelectObject( &m_view.m_bitmap );
	CRect r( 0, 0, ICON_WIDTH, ICON_HEIGHT );
	CBrush brush( RGB( 100, 100, 100 ) );
	dcMem.FillRect( &r, &brush );
	string szText = pTree->szTabName + ":";
	dcMem.TextOut( 10, 30, szText.c_str() );
	const char *pName = pTree->pItemsTree->GetItemName( nObjID );
	if ( pName )
	{
		CSize sz = dcMem.GetTextExtent( pName );
		dcMem.TextOut( 10, 30 + sz.cy, pName );	
	}
	dcMem.SelectObject( pOldBitmap );
	m_view.Invalidate();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CIconView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECControlBar::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	CRect r( 0, 0, ICON_WIDTH, ICON_HEIGHT );
	m_view.Create( 0, "obj icon", WS_CHILD | WS_VISIBLE, r, this, 100 );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CIconView::OnSize(UINT nType, int cx, int cy) 
{
	//CSize sz = CalcDynamicLayout( 0, 0 );
	SECControlBar::OnSize(nType, cx, cy);
	//MoveWindow( 0, 0, sz.cx, sz.cy );
	
	CRect rInside;
	GetInsideRect( rInside );
	m_view.SetWindowPos( 0, rInside.left, rInside.top, rInside.Width(), rInside.Height(),	SWP_NOZORDER|SWP_NOACTIVATE);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSize CIconView::CalcDynamicLayout( int nLength, DWORD dwMode )
{
	return SECControlBar::CalcDynamicLayout( nLength, dwMode );//CSize( ICON_WIDTH + 20, ICON_HEIGHT + 20 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
