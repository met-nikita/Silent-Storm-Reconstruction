// OICombo.cpp : implementation file
//

#include "stdafx.h"
#include "OICombo.h"
#include "CtrlObjectInspector.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// COICombo

COICombo::COICombo()
{
}

COICombo::~COICombo()
{
}


BEGIN_MESSAGE_MAP(COICombo, CComboBox)
	//{{AFX_MSG_MAP(COICombo)
	ON_WM_CREATE()
	ON_CONTROL_REFLECT(CBN_SELCHANGE, OnSelchange)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// COICombo message handlers

int COICombo::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CComboBox::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	return 0;
}

void COICombo::OnSelchange() 
{
	GetParent()->PostMessage( WM_USER_LOST_FOCUS );
}
