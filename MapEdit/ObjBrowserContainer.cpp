// ObjBrowserContainer.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "ObjBrowserContainer.h"


// CObjBrowserContainer dialog

IMPLEMENT_DYNAMIC(CObjBrowserContainer, CDialog)
CObjBrowserContainer::CObjBrowserContainer( vector< CPtr<IObjectBrowser> > browsers, CWnd* pParent /*=NULL*/)
	: CDialog(CObjBrowserContainer::IDD, pParent)
{
	ptIndent = CSize(0,0);
	for ( int i = 0; i < browsers.size(); ++i )
		obDlgs.push_back( new CObjBrowserDlg( browsers[i] ) );
}

CObjBrowserContainer::~CObjBrowserContainer()
{
	for ( int i = 0; i < obDlgs.size(); ++i )
		delete obDlgs[i];
}

void CObjBrowserContainer::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_WND_PLACE, m_Frame);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CObjBrowserContainer::OnInitDialog()
{
	if ( !CDialog::OnInitDialog() )
		return false;

	CRect r;
	m_Frame.GetWindowRect( &r );
	ScreenToClient( &r );
	ptIndent.cx = r.left;
	ptIndent.cy = r.top;
	Create();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjBrowserContainer::Create()
{
	if ( !::IsWindow( m_hWnd ) )
		return;
	CSize size(0,0);
	for ( int i = 0; i < obDlgs.size(); ++i )
	{
		CObjBrowserDlg *pDlg = obDlgs[i];
		if ( !IsWindow( pDlg->m_hWnd ) )
			static_cast<CDialog*>( pDlg )->Create( CObjBrowserDlg::IDD, &m_Frame.m_View );
		CRect r;
		pDlg->GetClientRect( &r );
		r.top    += size.cy;
		r.bottom += size.cy;
		pDlg->MoveWindow( &r );
		CSize sz = pDlg->GetSize();
		size.cx = Max( size.cx, sz.cx );
		size.cy += sz.cy;
	}
	m_Frame.m_View.SetSizes( size );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjBrowserContainer::SetObject( int nObjectID, int nVarID )
{
	for ( int i = 0; i < obDlgs.size(); ++i )
		obDlgs[i]->SetObject( nObjectID, nVarID );
	Create();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CObjBrowserContainer, CDialog)
	ON_WM_CREATE()
	ON_WM_SIZE()
END_MESSAGE_MAP()


////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjBrowserContainer message handlers
int CObjBrowserContainer::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;

//	Create();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjBrowserContainer::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	if ( !::IsWindow( m_Frame.m_hWnd ) )
		return;
	CRect r;
	GetClientRect( &r );
	r.DeflateRect( ptIndent );
	m_Frame.MoveWindow( &r );
}
////////////////////////////////////////////////////////////////////////////////////////////////////