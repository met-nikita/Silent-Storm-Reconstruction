// ChapterFrame.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "ChapterFrame.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterFrame

IMPLEMENT_DYNCREATE(CChapterFrame, SECWorksheet)

CChapterFrame::CChapterFrame()
{
}

CChapterFrame::~CChapterFrame()
{
}


BEGIN_MESSAGE_MAP(CChapterFrame, SECWorksheet)
	//{{AFX_MSG_MAP(CChapterFrame)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_SETFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterFrame message handlers

int CChapterFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECWorksheet::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	// create a view to occupy the client area of the frame
	if (!m_View.Create(NULL, "ChapterView", AFX_WS_DEFAULT_VIEW, 
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create params view window\n");
		return -1;
	}
	
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterFrame::OnClose() 
{
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterFrame::OnSetFocus(CWnd* pOldWnd) 
{
	SECWorksheet::OnSetFocus(pOldWnd);
	
	m_View.SetFocus();
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
// CGlobalMapFrame

IMPLEMENT_DYNCREATE(CGlobalMapFrame, SECWorksheet)
CGlobalMapFrame::CGlobalMapFrame()
{
}
CGlobalMapFrame::~CGlobalMapFrame()
{
}

BEGIN_MESSAGE_MAP(CGlobalMapFrame, SECWorksheet)
	//{{AFX_MSG_MAP(CGlobalMapFrame)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_SETFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CGlobalMapFrame message handlers
int CGlobalMapFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECWorksheet::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	// create a view to occupy the client area of the frame
	if (!m_View.Create(NULL, "GlobalMapView", AFX_WS_DEFAULT_VIEW, 
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create params view window\n");
		return -1;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalMapFrame::OnClose() 
{
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalMapFrame::OnSetFocus(CWnd* pOldWnd) 
{
	SECWorksheet::OnSetFocus(pOldWnd);
	m_View.SetFocus();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
