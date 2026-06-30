#include "StdAfx.h"
#include "mapedit.h"
#include "ScenarioFrame.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioFrame

IMPLEMENT_DYNCREATE(CScenarioFrame, SECWorksheet)

CScenarioFrame::CScenarioFrame()
{
}

CScenarioFrame::~CScenarioFrame()
{
}


BEGIN_MESSAGE_MAP(CScenarioFrame, SECWorksheet)
	//{{AFX_MSG_MAP(CScenarioFrame)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_SETFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioFrame message handlers

int CScenarioFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECWorksheet::OnCreate(lpCreateStruct) == -1)
		return -1;

	// create a view to occupy the client area of the frame
	if (!m_View.Create(NULL, "ScenarioView", AFX_WS_DEFAULT_VIEW, 
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create params view window\n");
		return -1;
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFrame::OnClose() 
{
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioFrame::OnSetFocus(CWnd* pOldWnd) 
{
	SECWorksheet::OnSetFocus(pOldWnd);

	m_View.SetFocus();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
