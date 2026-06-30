#include "StdAfx.h"
#include "OIBrowEdit.h"
#include "MapEdit.h"
#include "CtrlObjectInspector.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
// COIBrowseButton
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(COIBrowseButton, CButton)
//{{AFX_MSG_MAP(COIBrowseButton)
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

COIBrowseButton::COIBrowseButton( COIBrowseEdit *pPrnt, CEdit* pEdtBrowse )
{
  ASSERT( pPrnt );
  ASSERT( pEdtBrowse );
  //{{AFX_DATA_INIT(COIBrowseButton)
  //}}AFX_DATA_INIT
  
  m_pEdtBrowse = pEdtBrowse;
  m_pParentWnd = pPrnt;
  m_uiID = 0;
} 

COIBrowseButton::~COIBrowseButton()
{
} 

BOOL COIBrowseButton::Create()
{
  // Make sure we have an edit control.  
  ASSERT(m_pEdtBrowse != NULL);
  
  // Get the parent edit control and shrink it by the width
  // of the button to be created.
  CRect rc;

  m_pEdtBrowse->GetWindowRect(&rc);
  m_pEdtBrowse->SetWindowPos(NULL, 0, 0, rc.Width() - (BTN_WIDTH + 1),
    rc.Height(), SWP_NOZORDER | SWP_NOMOVE);
  
  // Now calculate the size and location of the button, get an
  // unused control ID, and create it.
  
  m_pParentWnd->ScreenToClient(&rc);
  rc.left = rc.right - BTN_WIDTH;

  const UINT MAX_DLGID = 32767;
  const UINT MIN_DLGID = 1;
  for (m_uiID = MAX_DLGID; m_uiID != MIN_DLGID; m_uiID--)
    if (::GetDlgItem(m_pParentWnd->GetSafeHwnd(), m_uiID) == NULL)
      break;
    ASSERT(m_uiID != MIN_DLGID);
    
  return CButton::Create(_T("..."), WS_VISIBLE | WS_CHILD, rc, m_pParentWnd, m_uiID);  
}

BOOL COIBrowseButton::OnChildNotify( UINT uiMsg, WPARAM wParam, LPARAM lParam,
                                    LRESULT* pLResult )
{
  if ((WM_COMMAND == uiMsg) && (m_uiID == LOWORD(wParam)) && (BN_CLICKED == HIWORD(wParam)))
  {
    m_pParentWnd->OnBrowse();
    return TRUE;
  }
  
  return FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// COIBrowseButton
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(COIBrowseEdit, CWnd)
//{{AFX_MSG_MAP(COIBrowseEdit)
  ON_WM_ENABLE()
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_SHOWWINDOW()
	ON_WM_SETFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

COIBrowseEdit::COIBrowseEdit() : m_BrowseBtn( this, &m_Edit )
{
}

COIBrowseEdit::~COIBrowseEdit()
{
	m_fntDef.DeleteObject();
}

int COIBrowseEdit::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
  if (CWnd::OnCreate(lpCreateStruct) == -1)
    return -1;
  
  // Create font
	if ( !m_fntDef.m_hObject )
	{
		LOGFONT lf;
		memset(&lf, 0, sizeof(LOGFONT));			// zero out structure
		lf.lfHeight = 15;							// request a ?-pixel-height font
		strcpy( lf.lfFaceName, "MS Sans Serif" );	// request a face name "Arial", "Courier", "MS Sans Serif"
		m_fntDef.CreateFontIndirect(&lf);			// create the fonts
		//lf.lfWeight = FW_BOLD;
	}

  CRect rect;
  m_Edit.Create( WS_CHILD | ES_LEFT | ES_AUTOHSCROLL | ES_MULTILINE | ES_WANTRETURN, rect, this, 1 );
  m_Edit.ModifyStyleEx( 0, WS_EX_STATICEDGE );
  m_Edit.SetFont( &m_fntDef );

  m_BrowseBtn.Create();
  m_BrowseBtn.SetFont( &m_fntDef );
  return 0;
}

void COIBrowseEdit::OnEnable(BOOL bEnable)
{
  m_Edit.EnableWindow(bEnable);
	m_BrowseBtn.EnableWindow(bEnable);
	ASSERT(m_BrowseBtn.IsWindowEnabled() == bEnable);
}

void COIBrowseEdit::OnShowWindow( BOOL bShow, UINT nState )
{
	CWnd::OnShowWindow( bShow, nState );
	if( bShow )
  {
    m_Edit.ShowWindow( SW_SHOW );
		m_BrowseBtn.ShowWindow( SW_SHOW );
  }
	else
  {
    m_Edit.ShowWindow( SW_HIDE );    
		m_BrowseBtn.ShowWindow( SW_HIDE );
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void COIBrowseEdit::OnSetFocus(CWnd* pNewWnd) 
{
	CWnd::OnSetFocus(pNewWnd);
	m_Edit.SetFocus();
}

void COIBrowseEdit::OnBrowse()
{
  CString szFile;
  m_Edit.GetWindowText( szFile );
  CString szStart = m_szDirPrefix + szFile;

  char szWD[512];
  GetCurrentDirectory( sizeof(szWD) - 1, szWD );

	char cLast = szStart[szStart.GetLength() - 1];
	if ( '\\' ==  cLast || '/' == cLast )
		szStart += "_";
  CFileDialog dlg( TRUE, 0, szStart );
 
  if ( dlg.DoModal() == IDOK )
  {
    CString str = dlg.GetPathName();

    if ( !m_szDirPrefix.IsEmpty() )
    {
      if ( 0 != str.Find( m_szDirPrefix ) )
      {
        MessageBox( GetResString( IDS_ERR_DIR_PREFIX ).c_str(), 0, MB_OK | MB_ICONWARNING );
        SetCurrentDirectory( szWD );
        return;
      }
      else
      {
        str = str.Right( str.GetLength() - m_szDirPrefix.GetLength() );
      }
    }
    SetWindowText( str );
		GetParent()->PostMessage( WM_USER_LOST_FOCUS );
  }
  
  SetCurrentDirectory( szWD );
}

void COIBrowseEdit::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);	
  m_Edit.MoveWindow( 0, 0, cx, cy );

  // Get the parent edit control and shrink it by the width
  // of the button to be created.
  CRect rc;
  m_Edit.GetWindowRect(&rc);
  m_Edit.SetWindowPos(NULL, 0, 0, rc.Width() - (BTN_WIDTH + 1),
    rc.Height(), SWP_NOZORDER | SWP_NOMOVE);
  
  ScreenToClient(&rc);
  rc.left = rc.right - BTN_WIDTH;
  
  m_BrowseBtn.MoveWindow( &rc );
  m_BrowseBtn.EnableWindow( IsWindowEnabled() );
}

void COIBrowseEdit::SetWindowText( LPCTSTR lpszString )
{
  m_Edit.SetWindowText( lpszString );
  CWnd::SetWindowText( lpszString );
}

void COIBrowseEdit::SetDirPrefix( LPCTSTR lpszDirPref )
{
  m_szDirPrefix = MakeOneSlash( lpszDirPref ).c_str();
}

void COIBrowseEdit::GetWindowText( CString &rString ) const
{
  m_Edit.GetWindowText( rString );
}

BOOL COIBrowseEdit::PreTranslateMessage( MSG* pMsg )
{
  if ( pMsg->message > WM_USER )
  {
    if ( WM_USER + 2 == pMsg->message && pMsg->wParam == (WPARAM)m_BrowseBtn.m_hWnd )
      return true;
		if ( WM_USER + 3 == pMsg->message )
		{
			CString s;
			m_Edit.GetWindowText( s );
			CWnd::SetWindowText( s );
			return true;
		}
		if ( WM_USER + 1 == pMsg->message )
		{
			CString s;
			m_Edit.GetWindowText( s );
			CWnd::SetWindowText( s );
		}
    GetParent()->PostMessage( pMsg->message, pMsg->wParam, pMsg->lParam );
    return true;
  }
  return CWnd::PreTranslateMessage( pMsg );
}

void COIBrowseEdit::SetCheckSyntax( bool bCheck )
{
	m_Edit.bCheckSyntax = bCheck;
}
