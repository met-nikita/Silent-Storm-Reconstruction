#include "StdAfx.h"
#include "MapEdit.h"
#include "OIParamsEdit.h"
#include "SubTemplateFlagsDlg.h"
#include "CtrlObjectInspector.h"


BEGIN_MESSAGE_MAP(COIParamsEdit, COIBrowseEdit)
	ON_WM_CREATE()
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
int COIParamsEdit::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (COIBrowseEdit::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_Edit.SetReadOnly();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIParamsEdit::OnBrowse()
{
	CSubTemplateFlagsDlg dlg;

	CString str;
	GetWindowText( str );
	dlg.szFlags = (LPCSTR)str;
	if ( dlg.DoModal() != IDOK )
		return;
	SetWindowText( dlg.szFlags.c_str() );
	GetParent()->PostMessage( WM_USER_LOST_FOCUS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////