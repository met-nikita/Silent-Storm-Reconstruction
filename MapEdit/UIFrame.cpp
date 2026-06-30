// UIFrame.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "UIFrame.h"
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CUIFrame

IMPLEMENT_DYNCREATE(CUIFrame, SECWorksheet)

CUIFrame::CUIFrame()
{
}

CUIFrame::~CUIFrame()
{
}


BEGIN_MESSAGE_MAP(CUIFrame, SECWorksheet)
	//{{AFX_MSG_MAP(CUIFrame)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CUIFrame message handlers

int CUIFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECWorksheet::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	// create a view to occupy the client area of the frame
	if (!m_View.Create(NULL, "UIView", AFX_WS_DEFAULT_VIEW, 
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create params view window\n");
		return -1;
	}
	
	return 0;
}

void CUIFrame::OnClose() 
{
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIFrame::OnSetFocus(CWnd* pOldWnd) 
{
	SECWorksheet::OnSetFocus(pOldWnd);
	
	m_View.SetFocus();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIFrame::OnKillFocus(CWnd* pNewWnd) 
{
	SECWorksheet::OnKillFocus(pNewWnd);	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
