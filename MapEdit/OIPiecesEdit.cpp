#include "StdAfx.h"
#include "MapEdit.h"
#include "OIPiecesEdit.h"
#include "CtrlObjectInspector.h"
#include "PiecesEditDlg.h"


BEGIN_MESSAGE_MAP(COIGeometryPiecesEdit, COIBrowseEdit)
	ON_WM_CREATE()
END_MESSAGE_MAP()

COIGeometryPiecesEdit::COIGeometryPiecesEdit(): nAIGeometryID(-1)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIGeometryPiecesEdit::SetAIGeometryID( int nID )
{
	nAIGeometryID = nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int COIGeometryPiecesEdit::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (COIBrowseEdit::OnCreate(lpCreateStruct) == -1)
		return -1;

	//m_Edit.SetReadOnly();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIGeometryPiecesEdit::OnBrowse()
{
	CPiecesEditDlg dlg( nAIGeometryID );

	CString str;
	GetWindowText( str );
	dlg.szData = (LPCSTR)str;
	if ( dlg.DoModal() != IDOK )
		return;
	SetWindowText( dlg.szData.c_str() );
	GetParent()->PostMessage( WM_USER_LOST_FOCUS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
