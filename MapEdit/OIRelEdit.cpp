#include "StdAfx.h"
#include "MapEdit.h"
#include "OIRelEdit.h"
#include "TreeSelItemDlg.h"
#include "CtrlObjectInspector.h"
#include "ItemsMgr.h"
#include "dbDefs.h"
#include "DiplomacyEditDlg.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// COIRelEdit
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(COIRelEdit, COIBrowseEdit)
//{{AFX_MSG_MAP(COIRelEdit)
ON_WM_CREATE()
	ON_WM_KEYDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
COIRelEdit::COIRelEdit()
{
	bReadOnly = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelEdit::OnBrowse()
{
	if ( 0x8000 & GetAsyncKeyState( VK_CONTROL ) )
	{
		theApp.SetActiveItem( nTableID, nItemID );
		return;
	}
	const SResTree *pTree = theApp.GetResTree( nTableID );
	if ( !pTree )
		return;
//	CTreeSelItemDlg dlg( pTree, nItemID );

	if ( nTableID == IDC_DIPLOMACY_TREE )
	{
		CDiplomacyEditDlg dlg;
		dlg.SetItem( nItemID );
		if ( IDOK != dlg.DoModal() )
			return;
		nItemID = dlg.GetSelectedID();
	}
	else
	{
		pTree->pTreeDlg->SetSelectedItemID( nTableID, nItemID );
		pTree->pTreeDlg->SetReadOnly( bReadOnly );
		if ( IDOK != pTree->pTreeDlg->DoModal() )
			return;
		int nTree;
		pTree->pTreeDlg->GetSelectedItemID( &nTree, &nItemID );
	}
	if ( -1 == nItemID )
		return;
	bNewValue = true;
	GetParent()->PostMessage( WM_USER_LOST_FOCUS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int COIRelEdit::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (COIBrowseEdit::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	m_Edit.SetReadOnly();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelEdit::SetTableItemIDs( int _nTableID, int _nItemID )
{
	bNewValue = false;
	nTableID = _nTableID;
	nItemID = _nItemID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int COIRelEdit::GetItemID()
{
	bNewValue = false;
	return nItemID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelEdit::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if ( VK_DELETE == nChar )
	{
		nItemID = EMPTY_VALUE;
		bNewValue = true;
		GetParent()->PostMessage( WM_USER_LOST_FOCUS );
	}
	COIBrowseEdit::OnKeyDown(nChar, nRepCnt, nFlags);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelEdit::SetReadOnly( bool _bReadOnly )
{
	bReadOnly = _bReadOnly;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
