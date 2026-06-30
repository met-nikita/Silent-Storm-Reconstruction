#include "StdAfx.h"
#include "OISubPartsEdit.h"
#include "resource.h"
#include "SubPartsView.h"
#include "CtrlObjectInspector.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// COIColorEdit
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(COISubPartsEdit, COIBrowseEdit)
//{{AFX_MSG_MAP(COISubPartsEdit)
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

COISubPartsEdit::COISubPartsEdit(): nItemID(-1)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COISubPartsEdit::OnBrowse()
{
	CString szMask;
  m_Edit.GetWindowText( szMask );	
	int mask = atoi( szMask );

	CSabPartsDlg dlg( nItemID );

	if ( IDOK != dlg.DoModal() )
		return;
	mask = dlg.GetMask();
	char buf[32];
	itoa( mask, buf, 10 );
	m_Edit.SetWindowText( buf );
	GetParent()->PostMessage( WM_USER_LOST_FOCUS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////

int COISubPartsEdit::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (COIBrowseEdit::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_Edit.SetReadOnly();
	return 0;
}
