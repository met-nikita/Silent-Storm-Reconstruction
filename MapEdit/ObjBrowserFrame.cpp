#include "StdAfx.h"
#include "ObjBrowserFrame.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
void COBView::SetSizes( const CSize &size )
{
	if ( !::IsWindow( m_hWnd ) )
	{
		SetScrollSizes( MM_TEXT, CSize(0,0) );
		return;
	}
	//CRect r;
	//GetWindowRect( &r );
	SetScrollSizes( MM_TEXT, size );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL COBView::PreCreateWindow( CREATESTRUCT &cs )
{
	if (!CScrollView::PreCreateWindow(cs))
		return FALSE;

	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS|CS_BYTEALIGNCLIENT, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_BTNFACE+1), NULL);

	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//IMPLEMENT_DYNCREATE(CObjBrowserFrame, CFrameWnd)

CObjBrowserFrame::CObjBrowserFrame()
{
}

CObjBrowserFrame::~CObjBrowserFrame()
{
}


BEGIN_MESSAGE_MAP(CObjBrowserFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CObjBrowserFrame)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjBrowserFrame message handlers

int CObjBrowserFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect r;
	GetClientRect( &r );
	// create a view to occupy the client area of the frame
	if (!m_View.Create(NULL, "COBView", AFX_WS_DEFAULT_VIEW, r, this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create params view window\n");
		return -1;
	}
	m_View.SetSizes( r.Size() );
	return 0;
}

void CObjBrowserFrame::PreSubclassWindow()
{
	if ( !::IsWindow( m_View.m_hWnd ) )
	{
		CRect r;
		GetClientRect( &r );
		// create a view to occupy the client area of the frame
		if (!m_View.Create(NULL, "COBView", AFX_WS_DEFAULT_VIEW, r, this, AFX_IDW_PANE_FIRST, NULL))
		{
			TRACE0("Failed to create params view window\n");
			ASSERT(0);
			return;
		}
		m_View.SetSizes( r.Size() );
	}
}

void CObjBrowserFrame::OnClose() 
{
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjBrowserFrame::OnSetFocus(CWnd* pOldWnd) 
{
	CFrameWnd::OnSetFocus(pOldWnd);

	m_View.SetFocus();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjBrowserFrame::OnSize(UINT nType, int cx, int cy)
{
	CFrameWnd::OnSize( nType, cx, cy );
	m_View.MoveWindow( 0, 0, cx, cy );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
