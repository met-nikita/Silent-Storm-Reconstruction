// ParamsFrm.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "ParamsFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CParamsFrame

IMPLEMENT_DYNCREATE(CParamsFrame, SECWorksheet)

CParamsFrame::CParamsFrame()
{
}

CParamsFrame::~CParamsFrame()
{
}


BEGIN_MESSAGE_MAP(CParamsFrame, SECWorksheet)
	//{{AFX_MSG_MAP(CParamsFrame)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CParamsFrame message handlers

int CParamsFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECWorksheet::OnCreate(lpCreateStruct) == -1)
		return -1;
	// create a view to occupy the client area of the frame
	if (!m_View.Create(NULL, "ParamsView", AFX_WS_DEFAULT_VIEW, 
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create params view window\n");
		return -1;
	}
	
	return 0;
}

void CParamsFrame::OnClose() 
{
	return;
	SECWorksheet::OnClose();
}
