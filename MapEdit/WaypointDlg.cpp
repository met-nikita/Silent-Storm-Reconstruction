// WaypointDlg.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "WaypointDlg.h"
#include "..\Main\aiWaypoint.h"
#include "..\Main\aiPosition.h"
#include "..\Misc\BasicShare.h"
#include "dbDefs.h"
#include "ItemsMgr.h"
#include "MESerialize.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern CBasicShare<int, NAI::CWaypointLoader> shareWaypoints;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EParam
{
	P_CMDTYPE = 0,
	P_TIME,
	P_POSE,
	P_DIR,
};
const int WM_USER_LOST_FOCUS = WM_USER + 1;
const int WM_USER_UPDATE = WM_USER + 2;
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CCombo, CComboBox)
	//{{AFX_MSG_MAP(CCombo)
	ON_CONTROL_REFLECT(CBN_SELCHANGE, OnSelchange)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCombo message handlers
void CCombo::OnSelchange() 
{
	GetParent()->PostMessage( WM_USER_LOST_FOCUS, (WPARAM)this );
}
string CCombo::GetText()
{
	CString str;
	GetLBText( GetCurSel(), str );
	return (LPCTSTR)str;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CEd, CEdit)
	//{{AFX_MSG_MAP(CEd)
	ON_CONTROL_REFLECT(EN_CHANGE, OnChange)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCombo message handlers
BOOL CEd::PreTranslateMessage(MSG* pMsg)
{
	switch (pMsg->message)
	{
		case WM_KEYDOWN:
			switch (pMsg->wParam)
			{
				case VK_RETURN:
					GetParent()->PostMessage( WM_USER_LOST_FOCUS, (WPARAM)this );
					return true;
			}
			break;
	}
	return CEdit::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEd::OnChange() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CEdit::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	GetParent()->PostMessage( WM_USER_UPDATE, (WPARAM)this );
}
string CEd::GetText()
{
	CString str;
	GetWindowText( str );
	return (LPCTSTR)str;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWaypointDlg dialog

CWaypointDlg::CWaypointDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CWaypointDlg::IDD, pParent)
{
	nWaypointID = -1;
	//{{AFX_DATA_INIT(CWaypointDlg)
	m_szName = _T("");
	//}}AFX_DATA_INIT
}


void CWaypointDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CWaypointDlg)
	DDX_Control(pDX, IDC_COMMANDS, m_commands);
	DDX_Text(pDX, IDC_WAYPOINTNAME, m_szName);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CWaypointDlg, CDialog)
	//{{AFX_MSG_MAP(CWaypointDlg)
	ON_NOTIFY(NM_CLICK, IDC_COMMANDS, OnClickCommands)
	ON_NOTIFY(NM_DBLCLK, IDC_COMMANDS, OnDblclkCommands)
	ON_COMMAND(ID_ADD_COMMAND, OnAddCommand)
	ON_COMMAND(ID_DEL_COMMAND, OnDelCommand)
	ON_WM_CONTEXTMENU()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CWaypointDlg message handlers

string CmdID2String( NAI::ECommand cmd )
{
	switch ( cmd )
	{
		case NAI::CMD_GOTO:	return "Goto";
		case NAI::CMD_POSE:	return "Pose";
		case NAI::CMD_WAIT:	return "Wait";
		case NAI::CMD_DIR:	return "Dir";
	}
	ASSERT(0);
	return "";
}
NAI::ECommand CmdString2ID( const string &cmd )
{
	if ( cmd == "Goto" ) return NAI::CMD_GOTO;
	else if ( cmd == "Pose" ) return NAI::CMD_POSE;
	else if ( cmd == "Wait" ) return NAI::CMD_WAIT;
	else if ( cmd == "Dir" )  return NAI::CMD_DIR;
	ASSERT( 0 );
	return NAI::CMD_POSE;
}
string PoseID2String( NAI::EPose pose )
{
	switch ( pose )
	{
		case NAI::CRAWL: return "Crawl";
		case NAI::CROUCH: return "Crouch";
		case NAI::WALK: return "Walk";
		case NAI::RUN: return "Run";
	}
	ASSERT(0);
	return "";
}
NAI::EPose PoseString2ID( const string &pose )
{
	if ( pose == "Crawl" ) return NAI::CRAWL;
	else if ( pose == "Crouch" ) return NAI::CROUCH;
	else if ( pose == "Walk" ) return NAI::WALK;
	else if ( pose == "Run" )  return NAI::RUN;
	ASSERT( 0 );
	return NAI::RUN;
}
string DirID2String( NAI::EDirection dir )
{
	switch ( dir )
	{
		case NAI::RIGHT: return "Right";
		case NAI::UPRIGHT: return "Upright";
		case NAI::UP: return "Up";
		case NAI::UPLEFT: return "Upleft";
		case NAI::LEFT: return "Left";
		case NAI::DOWNLEFT: return "Downleft";
		case NAI::DOWN: return "Down";
		case NAI::DOWNRIGHT: return "Downright";
		case NAI::NONE: return "None";
	}
	ASSERT(0);
	return "";
}
NAI::EDirection DirString2ID( const string &dir )
{
	if ( dir == "Right" ) return NAI::RIGHT;
	else if ( dir == "Upright" ) return NAI::UPRIGHT;
	else if ( dir == "Up" ) return NAI::UP;
	else if ( dir == "Upleft" )  return NAI::UPLEFT;
	else if ( dir == "Left" ) return NAI::LEFT;
	else if ( dir == "Downleft" ) return NAI::DOWNLEFT;
	else if ( dir == "Down" )  return NAI::DOWN;
	else if ( dir == "Downright" ) return NAI::DOWNRIGHT;
	else if ( dir == "None" )  return NAI::NONE;
	ASSERT( 0 );
	return NAI::UP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CWaypointDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_commands.SetExtendedStyle( LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT /*| LVS_EX_ONECLICKACTIVATE*/ );
	if ( !IsValid( pWaypoint ) )
		return false;
	// вставляем первую колонку
	LV_COLUMN  column;
	memset( &column, 0, sizeof( LV_COLUMN) );
	column.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	column.pszText = "Command";
	column.cx = 100;
	column.iSubItem = P_CMDTYPE;
	m_commands.InsertColumn( P_CMDTYPE, &column );
	column.pszText = "Time";
	column.cx = 50;
	column.iSubItem = P_TIME;
	m_commands.InsertColumn( P_TIME, &column );
	column.pszText = "Pose";
	column.iSubItem = P_POSE;
	m_commands.InsertColumn( P_POSE, &column );
	column.pszText = "Direction";
	column.cx = 70;
	column.iSubItem = P_DIR;
	m_commands.InsertColumn( P_DIR, &column );
	m_commands.InsertItem( 0, "ffffff" );
	// Create font
	LOGFONT lf;
	memset(&lf, 0, sizeof(LOGFONT));			// zero out structure
	lf.lfHeight = -10;							// request a ?-pixel-height font
	strcpy( lf.lfFaceName, "Microsoft Sans Serif" );	// request a face name "Arial", "Courier", "MS Sans Serif"
	m_font.CreateFontIndirect(&lf);			// create the fonts
	//
	CRect r;
  r.SetRectEmpty();
  r.right = 70;
  r.bottom = 120;
	DWORD dwStyle = CBS_DROPDOWNLIST | WS_CHILD | WS_VSCROLL | CBS_NOINTEGRALHEIGHT;
	cmd_combo.Create( dwStyle, r, &m_commands, 100 );
	cmd_combo.SetItemHeight( -1, 10 );
	cmd_combo.AddString( CmdID2String( NAI::CMD_POSE ).c_str() );
	cmd_combo.AddString( CmdID2String( NAI::CMD_WAIT ).c_str() );
	cmd_combo.AddString( CmdID2String( NAI::CMD_DIR ).c_str() );
	cmd_combo.SetCurSel(0);
	cmd_combo.SetFont( &m_font );
	//
	pose_combo.Create( dwStyle, r, &m_commands, 101 );
	pose_combo.SetItemHeight( -1, 10 );
	pose_combo.AddString( PoseID2String( NAI::CRAWL ).c_str() );
	pose_combo.AddString( PoseID2String( NAI::CROUCH ).c_str() );
	pose_combo.AddString( PoseID2String( NAI::WALK ).c_str() );
	pose_combo.AddString( PoseID2String( NAI::RUN ).c_str() );
	pose_combo.SetCurSel(0);
	pose_combo.SetFont( &m_font );
	//
	dir_combo.Create( dwStyle, r, &m_commands, 102 );
	dir_combo.SetItemHeight( -1, 10 );
	dir_combo.AddString( DirID2String( NAI::RIGHT ).c_str() );
	dir_combo.AddString( DirID2String( NAI::UPRIGHT ).c_str() );
	dir_combo.AddString( DirID2String( NAI::UP ).c_str() );
	dir_combo.AddString( DirID2String( NAI::UPLEFT ).c_str() );
	dir_combo.AddString( DirID2String( NAI::LEFT ).c_str() );
	dir_combo.AddString( DirID2String( NAI::DOWNLEFT ).c_str() );
	dir_combo.AddString( DirID2String( NAI::DOWN ).c_str() );
	dir_combo.AddString( DirID2String( NAI::DOWNRIGHT ).c_str() );
	dir_combo.AddString( DirID2String( NAI::NONE ).c_str() );
	dir_combo.SetCurSel(0);
	dir_combo.SetFont( &m_font );
	//
	edit.Create( WS_CHILD, r, &m_commands, 103 );
	edit.SetFont( &m_font );
	//
	m_menu.CreatePopupMenu();
	m_menu.AppendMenu( MF_STRING, ID_ADD_COMMAND, "New command" );
	m_menu.AppendMenu( MF_STRING, ID_DEL_COMMAND, "Delete command" );
	//
	SetWaypoint();
	m_szName = szWaypoint.c_str();
	UpdateData( FALSE );
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointDlg::OnClickCommands(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// TODO: Add your control notification handler code here
	EditParam( (NM_LISTVIEW*)pNMHDR );
	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointDlg::OnDblclkCommands(NMHDR* pNMHDR, LRESULT* pResult) 
{
	EditParam( (NM_LISTVIEW*)pNMHDR );
	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointDlg::ShowCombo( CCombo *pCombo, EParam column, int nItem )
{
	ASSERT( pCombo );
	CRect r;
	pCombo->column = column;
	pCombo->nItem = nItem;
	m_commands.GetSubItemRect( nItem, column, LVIR_LABEL, r );
	pCombo->MoveWindow( &r );
	pCombo->SelectString( -1, m_commands.GetItemText( nItem, column ) );
	pCombo->ShowWindow( SW_SHOW );
	pCombo->SetFocus();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointDlg::ShowEdit( CEd *pCtrl, EParam column, int nItem )
{
	ASSERT( pCtrl );
	CRect r;
	pCtrl->column = column;
	pCtrl->nItem = nItem;
	m_commands.GetSubItemRect( nItem, column, LVIR_LABEL, r );
	pCtrl->MoveWindow( &r );
	pCtrl->SetWindowText( m_commands.GetItemText( nItem, column ) );
	pCtrl->ShowWindow( SW_SHOW );
	pCtrl->SetFocus();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointDlg::HideEditBoxes()
{
	cmd_combo.ShowWindow( SW_HIDE );
	pose_combo.ShowWindow( SW_HIDE );
	dir_combo.ShowWindow( SW_HIDE );
	if ( edit.IsWindowVisible() )
	{
		GetParam( &edit );
		edit.ShowWindow( SW_HIDE );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointDlg::EditParam( NM_LISTVIEW *pNMListView )
{
//	if ( pNMListView->iSubItem == 0 )
//		return;
	HideEditBoxes();
	CPoint ptAction = pNMListView->ptAction;
	ptAction.x = 4;
	int iItem = m_commands.HitTest( ptAction );
	if ( iItem < 0 )
		return;
	
	//cmd_combo.ResetContent();
	CString str = m_commands.GetItemText( iItem, pNMListView->iSubItem );

	switch ( pNMListView->iSubItem )
	{
		case P_CMDTYPE:
			ShowCombo( &cmd_combo, (EParam)pNMListView->iSubItem, iItem );
			break;
		case P_DIR:
			ShowCombo( &dir_combo, (EParam)pNMListView->iSubItem, iItem );
			break;
		case P_POSE:
			ShowCombo( &pose_combo, (EParam)pNMListView->iSubItem, iItem );
			break;
		case P_TIME:
			ShowEdit( &edit, (EParam)pNMListView->iSubItem, iItem );
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CWaypointDlg::PreTranslateMessage(MSG* pMsg) 
{
	switch ( pMsg->message )
	{
		case WM_USER_UPDATE:
			{
				CWnd *p = (CWnd*)pMsg->wParam;
				CDynamicCast<CParam> pCtrl(p);
				if (pCtrl)
					GetParam( pCtrl );
			}
			return true;
		case WM_USER_LOST_FOCUS:
			{
				CWnd *p = (CWnd*)pMsg->wParam;
				CDynamicCast<CParam> pCtrl(p);
				if (pCtrl)
				{
					GetParam( pCtrl );
					p->ShowWindow( SW_HIDE );
				}
			}
			return true;
		case WM_KEYDOWN:
			switch (pMsg->wParam)
			{
				case VK_RETURN:
					return true;
			}
			break;
	}
	
	return CDialog::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointDlg::GetParam( CParam *pCtrl )
{
	ASSERT( pCtrl );
	string szText = pCtrl->GetText();
	switch ( pCtrl->column )
	{
		case P_TIME:
			szText = IToA( atoi( szText.c_str() ) );
			break;
	}
	m_commands.SetItemText( pCtrl->nItem, pCtrl->column, szText.c_str() );
	if ( IsValid( pWaypoint ) )
	{
		int nInd = m_commands.GetItemData( pCtrl->nItem );
		if ( nInd < 0 && nInd >= pWaypoint->commands.size() )
		{
			ASSERT(0);
			return;
		}
		NAI::SCommand &cmd = pWaypoint->commands[nInd];
		switch (pCtrl->column )
		{
			case P_CMDTYPE:
				cmd.cmd = CmdString2ID( szText );
				break;
			case P_TIME:
				cmd.time = atoi( szText.c_str() );
				break;
			case P_POSE:
				cmd.pose = PoseString2ID( szText );
				break;
			case P_DIR:
				cmd.dir = DirString2ID( szText );
				break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointDlg::SetWaypoint( int _nWaypointID )
{
	const SResTree *pTree = theApp.GetResTree( IDC_WAYPOINTS_TREE );
	const SResTree *pNTree = theApp.GetResTree( IDC_WAYPOINTNAMES_TREE );
	if ( !pTree || !pNTree )
		return;
	nWaypointID = _nWaypointID;
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nWaypointID );
	if ( !pProps )
		return;
	CPropMap::const_iterator i = pProps->find( "NameID" );
	if ( i == pProps->end() )
		return;
	//szWaypoint = string( "\\" ) + pTree->pItemsTree->GetItemPath( nWaypointID );;
	szWaypoint = string( "\\" ) + pNTree->pItemsTree->GetItemPath( i->second->GetValue() );
	CDGPtr< CPtrFuncBase<NAI::CWaypoint> > pWPLoader = shareWaypoints.Get( nWaypointID );
	pTree->pItemsTree->ReleasePropList( pProps );
	if ( !IsValid( pWPLoader ) )
		return;
	pWPLoader.Refresh();
	pWaypoint = pWPLoader->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointDlg::SetWaypoint()
{
	m_commands.DeleteAllItems();
	if ( !IsValid( pWaypoint ) )
	{
		ASSERT(0);
		return;
	}
	for ( int i = 0; i < pWaypoint->commands.size(); ++i )
		InsertCommand( i, &pWaypoint->commands[i] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointDlg::InsertCommand( int nID, const NAI::SCommand *pCmd )
{
	int i = m_commands.InsertItem( LVIF_TEXT | LVIF_PARAM, nID, CmdID2String( pCmd->cmd ).c_str(), 0, 0, 0, nID );
	m_commands.SetItemText( i, P_TIME, IToA( pCmd->time ).c_str() );
	m_commands.SetItemText( i, P_POSE, PoseID2String( pCmd->pose ).c_str() );
	m_commands.SetItemText( i, P_DIR, DirID2String( pCmd->dir ).c_str() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointDlg::OnAddCommand() 
{
	if ( IsValid( pWaypoint ) )
	{
		pWaypoint->commands.push_back( NAI::SCommand() );
		InsertCommand( pWaypoint->commands.size() - 1, &pWaypoint->commands.back() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointDlg::OnDelCommand() 
{
	POSITION pos = m_commands.GetFirstSelectedItemPosition();
	if (pos == NULL)
		 TRACE0("No items were selected!\n");
	else
	{
		int nItem = m_commands.GetNextSelectedItem(pos);
		int nInd = m_commands.GetItemData( nItem );
		if ( nInd < 0 && nInd >= pWaypoint->commands.size() )
		{
			ASSERT(0);
			return;
		}
		pWaypoint->commands.erase( pWaypoint->commands.begin() + nInd );
		SetWaypoint();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointDlg::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	HideEditBoxes();
	m_menu.TrackPopupMenu( TPM_LEFTBUTTON, point.x, point.y, this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWaypointDlg::OnOK() 
{
	SerializeWaypoint( pWaypoint, nWaypointID );
	CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
