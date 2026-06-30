// OIEdit.cpp : implementation file
//

#include "stdafx.h"
#include "OIEdit.h"
#include "resource.h"
#include "TextEditor.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// COIEdit

COIEdit::COIEdit()
{
	bCheckSyntax = false;
}

COIEdit::~COIEdit()
{
}


BEGIN_MESSAGE_MAP(COIEdit, CEdit)
	//{{AFX_MSG_MAP(COIEdit)
	ON_WM_KEYDOWN()
	ON_WM_KILLFOCUS()
	ON_WM_SETFOCUS()
	ON_WM_LBUTTONDBLCLK()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// COIEdit message handlers

void COIEdit::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	switch ( nChar )
	{
		case VK_DOWN:
		case VK_RETURN:
		case VK_UP:
			GetParent()->PostMessage( WM_USER + 1, nChar );
			break;

		default:
			CEdit::OnKeyDown(nChar, nRepCnt, nFlags);			
			GetParent()->PostMessage( WM_USER + 3, nChar );
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIEdit::OnKillFocus(CWnd* pNewWnd) 
{
	CEdit::OnKillFocus(pNewWnd);
	if ( pNewWnd && pNewWnd != GetParent() )
		GetParent()->PostMessage( WM_USER + 2, (WPARAM)pNewWnd->m_hWnd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIEdit::OnSetFocus(CWnd* pNewWnd) 
{
	CEdit::OnSetFocus(pNewWnd);
	SetSel( 0, -1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIEdit::OnLButtonDblClk( UINT nFlags, CPoint pt)
{
	ShowEditDlg();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIEdit::ShowEditDlg()
{
	CTextEditor dlg;

	dlg.CheckSyntax( bCheckSyntax );
	CString sz;
	GetWindowText( sz );
	dlg.SetText( (LPCSTR)sz );
	if ( IDOK == dlg.DoModal() )
	{
		SetWindowText( dlg.GetText().c_str() );
		GetParent()->PostMessage( WM_USER + 1, VK_RETURN );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
