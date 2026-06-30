// ChapterFrame.cpp : implementation file
//
#include "stdafx.h"
#include "mapedit.h"
#include "DiplomacyFrame.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// CDiplomacyFrame

IMPLEMENT_DYNCREATE(CDiplomacyFrame, SECWorksheet)

CDiplomacyFrame::CDiplomacyFrame()
{
}

CDiplomacyFrame::~CDiplomacyFrame()
{
}


BEGIN_MESSAGE_MAP(CDiplomacyFrame, SECWorksheet)
	//{{AFX_MSG_MAP(CDiplomacyFrame)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_SETFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CDiplomacyFrame message handlers

int CDiplomacyFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECWorksheet::OnCreate(lpCreateStruct) == -1)
		return -1;

	// create a view to occupy the client area of the frame
	if (!m_View.Create(NULL, "DiplomacyView", AFX_WS_DEFAULT_VIEW, 
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create diplomacy view window\n");
		return -1;
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDiplomacyFrame::OnClose() 
{
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDiplomacyFrame::OnSetFocus(CWnd* pOldWnd) 
{
	SECWorksheet::OnSetFocus(pOldWnd);

	m_View.SetFocus();
}
////////////////////////////////////////////////////////////////////////////////////////////////////