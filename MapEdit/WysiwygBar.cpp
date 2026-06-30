// WysiwygBar.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "WysiwygBar.h"
#include "MaterialEditPage.h"
#include "GeometryPage.h"
#include "IWysiwyg.h"
#include "TexSpotPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CWysiwygBarWnd
IMPLEMENT_DYNAMIC(CWysiwygBarWnd, CPropertySheet)

CWysiwygBarWnd::CWysiwygBarWnd(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(nIDCaption, pParentWnd, iSelectPage), 
	m_pMaterial( new CMaterialEditPage ), m_pGeometry( new CGeometryPage )
{
//	AddPage( m_pMaterial );
//	AddPage( m_pGeometry );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWysiwygBarWnd::CWysiwygBarWnd(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(pszCaption, pParentWnd, iSelectPage), 
	m_pMaterial( new CMaterialEditPage ), m_pGeometry( new CGeometryPage )
{
//	AddPage( m_pMaterial );
//	AddPage( m_pGeometry );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWysiwygBarWnd::~CWysiwygBarWnd()
{
//	delete m_pMaterial;
//	delete m_pGeometry;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CWysiwygBarWnd, CPropertySheet)
	//{{AFX_MSG_MAP(CWysiwygBarWnd)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CWysiwygBarWnd message handlers
BOOL CWysiwygBarWnd::OnInitDialog() 
{
	BOOL bResult = CPropertySheet::OnInitDialog();
	
//	SetActivePage( m_pMaterial );
//	wysiwygBrush.type = BT_MATERIAL;
	return bResult;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// CWysiwygBar
CWysiwygBar::CWysiwygBar() : m_pMaterial( new CMaterialEditPage ), m_pGeometry( new CGeometryPage ), m_pTexSpot( new CTexSpotPage )
{
}

CWysiwygBar::~CWysiwygBar()
{
	delete m_pMaterial;
	delete m_pGeometry;
	delete m_pTexSpot;
}


BEGIN_MESSAGE_MAP(CWysiwygBar, SECControlBar)
	//{{AFX_MSG_MAP(CWysiwygBar)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_MESSAGE( TCM_TABSEL, OnTabSel )
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CWysiwygBar message handlers
int CWysiwygBar::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECControlBar::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	//m_sheet.Create( this, WS_CHILD | WS_VISIBLE, WS_EX_TOOLWINDOW );
	m_tab.Create( this, WS_CHILD | WS_VISIBLE |	WS_HSCROLL | WS_VSCROLL | TWS_TABS_ON_BOTTOM );
	m_pMaterial->Create( CMaterialEditPage::IDD, &m_tab );
	m_pGeometry->Create( CGeometryPage::IDD, &m_tab );
	m_pTexSpot->Create( CTexSpotPage::IDD, &m_tab );
	m_tab.AddTab( m_pMaterial, "Material" );
	//m_tab.AddTab( m_pGeometry, "Geometry" );
	//m_tab.AddTab( m_pTexSpot, "Projection" );
	m_tab.ActivateTab( m_pMaterial );

	//wysiwygBrush.type = BT_MATERIAL;

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygBar::OnSize(UINT nType, int cx, int cy) 
{
	SECControlBar::OnSize(nType, cx, cy);
	
  if ( ::IsWindow( m_tab.m_hWnd ) )
  {
    CRect rectInside;
    GetInsideRect(rectInside);
		//m_sheet.MoveWindow( &rectInside );
		m_tab.MoveWindow( &rectInside );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CWysiwygBar::PreTranslateMessage(MSG* pMsg) 
{
	return SECControlBar::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
LRESULT CWysiwygBar::OnTabSel(WPARAM wParam, LPARAM lParam)
{
	/*
	CWnd *p;
	if ( !m_tab.GetActiveTab( p ) )
		return 0;
	if ( p == m_pMaterial )
		wysiwygBrush.type = BT_MATERIAL;
	else if ( p == m_pGeometry )
		wysiwygBrush.type = BT_GEOMETRY;
	else
		wysiwygBrush.type = BT_TEXSPOT;
	*/
	return 0;
}
