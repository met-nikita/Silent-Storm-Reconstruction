#include "StdAfx.h"// D:\Home\a5\MapEdit\OIList.cpp : implementation file
//
#include "MapEdit.h"
#include "OIList.h"
#include "afxcmn.h"
#include "TreeView.h"
#include "dbDefs.h"
#include "afxwin.h"
#include "OIDlg.h"
#include "..\DBFormat\DataFormat.h"
#include "..\Misc\StrProc.h"

static const char szCF_NAME[] = "A5_PARTICLE_INSTANCE";
static int CF_PINSTANCE = 0;

class COIRelListEdit : public CDialog
{
	DECLARE_DYNAMIC(COIRelListEdit)

public:
	COIRelListEdit(CListProp *pProp, CWnd* pParent = NULL);   // standard constructor
	virtual ~COIRelListEdit();

// Dialog Data
	enum { IDD = IDD_LISTPROP };

	virtual BOOL PreTranslateMessage(MSG* pMsg);

protected:
	CPtr<CListProp> pProp;
	vector<CVariant> items;
	const SResTree *pResTree;
	const CPropMap *pPropMap;
	COIDlg m_OIDlg;

	void InsertItems();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	void RefreshDB();

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	CStatic m_propPlace;
	CListCtrl m_list;
	CMenu m_menu;
	
	int  GetSelectedItem(); // -1 = никто не поселекчен
	void UpdateSelectedItem();
	void SetMenuStates();
	afx_msg void OnHdnItemdblclickItemlist(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedOk();
	afx_msg void OnNMClickItemlist(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnKeydownItemlist(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnNMRclickItemlist(NMHDR *pNMHDR, LRESULT *pResult);
	void OnNewItem();
	void OnDelItem();
	void OnCopy();
	void OnPaste();
	void OnRename();
	afx_msg void OnLvnEndlabeleditItemlist(NMHDR *pNMHDR, LRESULT *pResult);
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// COIRelList
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(COIRelList, COIBrowseEdit)
//{{AFX_MSG_MAP(COIRelList)
	ON_WM_CREATE()
	ON_WM_KEYDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelList::OnBrowse()
{
	if ( pProp->GetRelation() <= 0 )
		return;
	COIRelListEdit dlg( pProp );

	dlg.DoModal();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelList::SetList( CListProp *p )
{
	pProp = p;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int COIRelList::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (COIBrowseEdit::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	m_Edit.SetReadOnly();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelList::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	COIBrowseEdit::OnKeyDown(nChar, nRepCnt, nFlags);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

// COIRelListEdit dialog


IMPLEMENT_DYNAMIC(COIRelListEdit, CDialog)
COIRelListEdit::COIRelListEdit( CListProp *_pProp, CWnd* pParent /*=NULL*/)
	: CDialog(COIRelListEdit::IDD, pParent), pProp(_pProp)
{
	pResTree = 0;
	pPropMap = 0;
}

COIRelListEdit::~COIRelListEdit()
{
	m_menu.DestroyMenu();
}

void COIRelListEdit::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TREE_PLACE, m_propPlace);
	DDX_Control(pDX, IDC_ITEMLIST, m_list);
}


BEGIN_MESSAGE_MAP(COIRelListEdit, CDialog)
	ON_WM_CREATE()
	ON_NOTIFY(NM_DBLCLK, IDC_ITEMLIST, OnHdnItemdblclickItemlist)
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_NOTIFY(NM_CLICK, IDC_ITEMLIST, OnNMClickItemlist)
	ON_NOTIFY(LVN_KEYDOWN, IDC_ITEMLIST, OnLvnKeydownItemlist)
	ON_NOTIFY(NM_RCLICK, IDC_ITEMLIST, OnNMRclickItemlist)
	ON_COMMAND(ID_NEWITEM, OnNewItem)
	ON_COMMAND(ID_DELETEITEM, OnDelItem)
	ON_COMMAND(ID_EDIT_COPY, OnCopy)
	ON_COMMAND(ID_EDIT_PASTE, OnPaste)
	ON_COMMAND(ID_RENAMEITEM, OnRename)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_ITEMLIST, OnLvnEndlabeleditItemlist)
END_MESSAGE_MAP()


// COIRelListEdit message handlers

BOOL COIRelListEdit::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	pResTree = theApp.GetResTree( pProp->GetRelation() );
	if ( !IsValid( pProp ) || !pResTree )
		return FALSE;

	static CPoint pt( 0, 0 );
	CRect r;
	
	//
	pt += CPoint( 10, 10 );
	pt.x %= 50;
	pt.y %= 50;
	::GetClientRect( theApp.GetMainWnd()->m_hWnd, &r );
	CPoint ptLT( r.Width() / 2, r.Height() / 2 );
	GetWindowRect( &r );
	ptLT -= CPoint( r.Width() / 2, r.Height() / 2 );
	r -= r.TopLeft();
	MoveWindow( r + pt + ptLT, FALSE );

	  // Init object inspector
	if ( !::IsWindow( m_OIDlg.m_hWnd ) )
	{
	  m_propPlace.GetWindowRect( r );
		ScreenToClient( &r );
		m_OIDlg.Create( IDD_OBJINSPECTOR, this );
		m_OIDlg.ModifyStyleEx( 0, WS_EX_CLIENTEDGE );
		//m_OIDlg.CreateEx( WS_EX_CLIENTEDGE, 0, "props", WS_CHILD | WS_VISIBLE | WS_TABSTOP, r, this, IDD_OBJINSPECTOR );
		::SetWindowPos( m_OIDlg.m_hWnd, NULL, r.left, r.top, r.Width(), r.Height(), SWP_NOZORDER|SWP_NOACTIVATE);
	}

	m_menu.CreatePopupMenu();
	m_menu.AppendMenu( MF_STRING, ID_NEWITEM, "New item" );
	m_menu.AppendMenu( MF_STRING, ID_RENAMEITEM, "Rename" );
	m_menu.AppendMenu( MF_STRING, ID_DELETEITEM, "Delte" );
	m_menu.AppendMenu( MF_SEPARATOR );
	m_menu.AppendMenu( MF_STRING, ID_EDIT_COPY, "Copy" );
	m_menu.AppendMenu( MF_STRING, ID_EDIT_PASTE, "Paste" );
	InsertItems();
//	m_okButton.EnableWindow( false );
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL COIRelListEdit::PreTranslateMessage(MSG* pMsg) 
{
	if ( WM_KEYDOWN == pMsg->message )
	{
		bool bCtrl = 0x8000 & GetAsyncKeyState( VK_CONTROL );
		bool bShift = 0x8000 & GetAsyncKeyState( VK_SHIFT );
		switch ( pMsg->wParam )
		{
			case VK_INSERT:
				if ( bCtrl ) OnCopy();
				if ( bShift ) OnPaste();
				break;
			case 'C':
				if ( bCtrl ) OnCopy();
				break;
			case 'V':
				if ( bCtrl ) OnPaste();
				break;
		}
	}
	return CDialog::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelListEdit::InsertItems()
{
	m_list.DeleteAllItems();
	if ( !IsValid( pProp ) )
		return;
	const SResTree *pTree = theApp.GetResTree( pProp->GetRelation() );
	if ( !pTree )
		return;
	CItemsMgr *pItems = pTree->pItemsTree;
	//
	if ( pProp->GetValues( &items ) )
	{
		for ( int i = 0; i < items.size(); ++i )
			m_list.InsertItem( LVIF_TEXT | LVIF_PARAM, i, pItems->GetItemPath( items[i] ).c_str(), 0, 0, 0, (int)items[i] );
	}
	UpdateSelectedItem();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelListEdit::OnHdnItemdblclickItemlist(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMHEADER phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelListEdit::OnBnClickedOk()
{
	if ( !IsValid( pProp ) )
		return;
	vector<int> instances;
	for ( int i = 0; i < m_list.GetItemCount(); ++i )
		instances.push_back( (int)m_list.GetItemData( i ) );
	// удал€ем итема, которых нет в текущем списке
	for ( int i = 0; i < items.size(); ++i )
	{
		if ( instances.end() == find( instances.begin(), instances.end(), (int)items[i] ) )
		{
			pResTree->pItemsTree->DeleteItem( -1, items[i] );
		}
	}
	//
	CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelListEdit::OnNMClickItemlist(NMHDR *pNMHDR, LRESULT *pResult)
{
	UpdateSelectedItem();
	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelListEdit::OnLvnKeydownItemlist(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLVKEYDOWN pLVKeyDow = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);
	if ( VK_UP == pLVKeyDow->wVKey || VK_DOWN == pLVKeyDow->wVKey )
		UpdateSelectedItem();
	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int COIRelListEdit::GetSelectedItem()
{
	POSITION pos = m_list.GetFirstSelectedItemPosition();
	if (pos)
		return m_list.GetNextSelectedItem(pos);
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelListEdit::UpdateSelectedItem()
{
	int nItem = GetSelectedItem();
	if ( nItem >= 0 )
	{
		int nItemID = m_list.GetItemData( nItem );

		const CPropMap *pNewProps = pResTree->pItemsTree->GetPropList( nItemID );
		m_OIDlg.SetPropMap( pResTree->nTreeID, pNewProps );
		if ( pPropMap )
			pResTree->pItemsTree->ReleasePropList( pPropMap );
		pPropMap = pNewProps;
	}
	else
		m_OIDlg.SetPropMap( pResTree->nTreeID, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelListEdit::OnNMRclickItemlist(NMHDR *pNMHDR, LRESULT *pResult)
{
	CPoint pt;

	GetCursorPos( &pt );
	//ScreenToClient( &pt );
	SetMenuStates();
	m_menu.TrackPopupMenu( TPM_LEFTBUTTON, pt.x, pt.y, this );
	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelListEdit::SetMenuStates()
{
	int nItem = GetSelectedItem();

  UINT nItemEnable = nItem >= 0 ? MF_ENABLED : MF_GRAYED;
	m_menu.EnableMenuItem( ID_DELETEITEM, nItemEnable );
  m_menu.EnableMenuItem( ID_EDIT_COPY, nItemEnable );
	m_menu.EnableMenuItem( ID_EDIT_PASTE, IsClipboardFormatAvailable(CF_PINSTANCE) ? MF_ENABLED : MF_GRAYED );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelListEdit::OnNewItem()
{
	int nID = pResTree->pItemsTree->AddItem( -1, 0, "New item" );
	if ( nID <= 0 )
		return;
	pProp->AddValue( nID );
	items.push_back( nID );
	int ni = m_list.InsertItem( LVIF_TEXT | LVIF_PARAM, m_list.GetItemCount(), "New item", 0, 0, 0, nID );
	UpdateSelectedItem();
	m_list.EditLabel( ni );
	RefreshDB();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelListEdit::OnRename()
{
	int nItem = GetSelectedItem();
	if ( nItem < 0 )
		return;
	m_list.EditLabel( nItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelListEdit::OnDelItem()
{
	int nItem = GetSelectedItem();
	if ( nItem < 0 )
		return;
	m_list.DeleteItem( nItem );
	RefreshDB();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string PropsToStr( const string &szUserName, const CPropMap *pProps )
{
	if ( !pProps )
	{
		ASSERT(0);
		return "";
	}
	string sz = "UserName=";
	sz += szUserName + '\n';
	for ( CPropMap::const_iterator i = pProps->begin(); i != pProps->end(); ++i )
	{
		sz += i->first;
		sz += '=';
		sz += (string)i->second->GetValue();
		sz += "\n";
	}
	return sz;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelListEdit::OnCopy()
{
	if ( !::OpenClipboard( theApp.GetMainWnd()->m_hWnd ) )
  {
    AfxMessageBox( "Cannot open the Clipboard" );
    return;
  }
  // Remove the current Clipboard contents
	if( !::EmptyClipboard() )
  {
    AfxMessageBox( "Cannot empty the Clipboard" );
    return;
  }
	if ( 0 == CF_PINSTANCE )
	{
		CF_PINSTANCE = ::RegisterClipboardFormat( szCF_NAME );
		if ( 0 == CF_PINSTANCE )
		{
			AfxMessageBox( "Unable to register Clipboard format" );
			::CloseClipboard();
			return;
		}
	}
	int nID = m_list.GetItemData( GetSelectedItem() );
	const CPropMap *pProps = pResTree->pItemsTree->GetPropList( nID );
	string sz = PropsToStr( pResTree->pItemsTree->GetItemName( nID ), pProps );
	pResTree->pItemsTree->ReleasePropList( pProps );
	//
	HGLOBAL hGlobal = ::GlobalAlloc( GMEM_SHARE|GMEM_FIXED, sz.size() + 1 );
	LPTSTR lpszDbObjPtr = (LPTSTR)::GlobalLock( hGlobal );
	strcpy( lpszDbObjPtr, sz.c_str() );
	//Put reference to global memory into clipboard
	::GlobalUnlock( hGlobal );

  // For the appropriate data formats...
	if ( ::SetClipboardData( CF_TEXT, hGlobal ) == NULL || ::SetClipboardData( CF_PINSTANCE, hGlobal ) == NULL )
  {
    AfxMessageBox( "Unable to set Clipboard data" );
    CloseClipboard();
    return;
  }
  // ...
	::CloseClipboard();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string StrToProps( const CPropMap *pProps, const string &szStr )
{
	vector<string> szVector;
	NStr::SplitString( szStr, szVector, '\n' );
	string szName;

	for ( int i = 0; i < szVector.size(); ++i )
	{
		vector<string> str;
		NStr::SplitString( szVector[i], str, '=' );
		if ( str.size() < 2 )
			continue;
		if ( str[0] == "UserName" )
		{
			szName = str[1];
			continue;
		}
		CPropMap::const_iterator it = pProps->find( str[0] );
		if ( it != pProps->end() )
			it->second->SetValue( str[1], false );
	}
	return szName;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelListEdit::OnPaste()
{
  if ( !IsClipboardFormatAvailable(CF_PINSTANCE) )
      return; 
  if ( !OpenClipboard() )
      return; 

  HANDLE hglb = GetClipboardData(CF_PINSTANCE); 
  if (hglb != NULL) 
  { 
    LPCSTR lptstr = (LPCSTR)GlobalLock(hglb); 
    if (lptstr != NULL) 
    {
			string szName = "";
			int nID = pResTree->pItemsTree->AddItem( -1, 0, szName );
			if ( nID <= 0 )
				return;
			const CPropMap *pProps = pResTree->pItemsTree->GetPropList( nID );
			if ( pProps )
			{
				szName = StrToProps( pProps, lptstr );
				pResTree->pItemsTree->SetItemProps( nID, pProps );
				pResTree->pItemsTree->ReleasePropList( pProps );
				pResTree->pItemsTree->SetItemName( -1, nID, szName );
			}
			pProp->AddValue( nID );
			items.push_back( nID );
			int ni = m_list.InsertItem( LVIF_TEXT | LVIF_PARAM, m_list.GetItemCount(), szName.c_str(), 0, 0, 0, nID );
			UpdateSelectedItem();
			RefreshDB();
    } 
    GlobalUnlock(hglb); 
  } 
  CloseClipboard(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelListEdit::OnLvnEndlabeleditItemlist(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	CEdit *pE = m_list.GetEditControl();
	if ( pE )
	{
		CString str;
		pE->GetWindowText( str );
		pResTree->pItemsTree->SetItemName( -1, pDispInfo->item.lParam, (LPCSTR)str );
		m_list.SetItemText( pDispInfo->item.iItem, 0, str );
	}
	//NDatabase::Refresh<NDb::CParticleInstance>();
	UpdateSelectedItem();
	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIRelListEdit::RefreshDB()
{
	return;
	/*
	switch ( pResTree->nTreeID )
	{
		case IDC_PARTICLEINSTANCES_TREE:
			NDatabase::Refresh<NDb::CEffect>();
			NDatabase::Refresh<NDb::CParticleInstance>();
			break;
		case IDC_LIGHTINSTANCES_TREE:
			NDatabase::Refresh<NDb::CEffect>();
			NDatabase::Refresh<NDb::CLightInstance>();
			break;
		case IDC_SOUNDINSTANCES_TREE:
			NDatabase::Refresh<NDb::CSoundEffect>();
			NDatabase::Refresh<NDb::CSoundInstance>();
			break;
	}
	*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
