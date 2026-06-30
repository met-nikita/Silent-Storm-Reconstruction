// GameFrm.cpp : implementation of the CGameFrame class
//

#include "stdafx.h"
#include "MapEdit.h"

#include "GameFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool bWYSIWYGActive;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGameFrame

IMPLEMENT_DYNCREATE(CGameFrame, SECWorksheet)

BEGIN_MESSAGE_MAP(CGameFrame, SECWorksheet)
	//{{AFX_MSG_MAP(CGameFrame)
	ON_WM_CREATE()
	ON_WM_MDIACTIVATE()
	ON_WM_CLOSE()
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_KILLFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CGameFrame construction/destruction

CGameFrame::CGameFrame() : nCurPlacementID(-1)
{
	// TODO: add member initialization code here
	
}

CGameFrame::~CGameFrame()
{
}

int CGameFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECWorksheet::OnCreate(lpCreateStruct) == -1)
		return -1;
	// create a view to occupy the client area of the frame
	if (!m_wndView.Create(NULL, "D3DView", AFX_WS_DEFAULT_VIEW, 
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}

	return 0;
}



BOOL CGameFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	if( !SECWorksheet::PreCreateWindow(cs) )
		return FALSE;

	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.lpszClass = AfxRegisterWndClass(0);

	return TRUE;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// CGameFrame diagnostics

#ifdef _DEBUG
void CGameFrame::AssertValid() const
{
	SECWorksheet::AssertValid();
}

void CGameFrame::Dump(CDumpContext& dc) const
{
	SECWorksheet::Dump(dc);
}

#endif //_DEBUG

BOOL CGameFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
	// let the view have first crack at the command
	if (m_wndView.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
		return TRUE;
	
	// otherwise, do default handling
	return SECWorksheet::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void CGameFrame::OnMDIActivate(BOOL bActivate, CWnd* pActivateWnd, CWnd* pDeactivateWnd) 
{
  if ( !bActivate )
    m_wndView.ShowWindow( SW_HIDE );
	else
	{
		m_wndView.ShowWindow( SW_SHOW );
		theApp.GetGameView()->Activate();
	}
	SECWorksheet::OnMDIActivate(bActivate, pActivateWnd, pDeactivateWnd);
  
  if ( !bActivate )
    return;
  /*
  CGameView *pGV = theApp.GetGameView();
  if ( pGV )
  {
    pGV->SetRootPlacement( nCurPlacementID );
    pGV->SetParent( this );
    RECT r;
    GetClientRect( &r );
    pGV->MoveWindow( &r );
    theApp.bGameActive = true;
  }
  */
}

void CGameFrame::SetCurPlacement( int nPlacementID )
{
  nCurPlacementID = nPlacementID;
  m_wndView.SetRootPlacement( nCurPlacementID );
}

void CGameFrame::OnClose() 
{
	return;
	SECWorksheet::OnClose();
}

void CGameFrame::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	DrawFrame();
	// Do not call SECWorksheet::OnPaint() for painting messages
}

void CGameFrame::OnSize(UINT nType, int cx, int cy) 
{
	SECWorksheet::OnSize(nType, cx, cy);
	
	m_wndView.MoveWindow( 1, 1, cx - 2, cy - 2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameFrame::DrawFrame()
{
	CDC *pDC = GetDC();

	bWYSIWYGActive = GetFocus() == &m_wndView;
	COLORREF cr =  bWYSIWYGActive? RGB( 255, 255, 200 ) : RGB( 0, 0, 0 );
	CPen pen( PS_SOLID, 1, cr );
	CPen *pOldPen = pDC->SelectObject( &pen );
	CBrush *pOldBrush = pDC->SelectObject( CBrush::FromHandle( (HBRUSH)GetStockObject( HOLLOW_BRUSH ) ) );

	CRect r;
	GetClientRect( &r );
	pDC->Rectangle( &r );
	pDC->SelectObject( pOldBrush );

	ReleaseDC( pDC );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGameFrame::OnKillFocus(CWnd* pNewWnd) 
{
	SECWorksheet::OnKillFocus(pNewWnd);
	
	DrawFrame();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CGameFrame::PreTranslateMessage(	MSG *pMsg )
{
	switch ( pMsg->message )
	{
		case WM_KEYDOWN:
			switch ( pMsg->lParam )
			{
				case VK_MENU:
					//DrawFrame();
					return true;
			}
			break;
	}
	return SECWorksheet::PreTranslateMessage( pMsg );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
