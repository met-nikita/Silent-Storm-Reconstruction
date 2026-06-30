#include "StdAfx.h"
#include "MapEdit.h"
#include "SubPartsView.h"
#include "ItemsMgr.h"
#include "dbDefs.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSubPartsView

CSubPartsView::CSubPartsView(): m_dlg(-1)
{
}

CSubPartsView::~CSubPartsView()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSubPartsView::SetConstructionPartID( int nID )
{
//	if ( ::IsWindow( m_dlg.m_hWnd ) )
//		m_dlg.SetConstructionPart( nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CSubPartsView, CWnd)
	//{{AFX_MSG_MAP(CSubPartsView)
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


////////////////////////////////////////////////////////////////////////////////////////////////////
// CSubPartsView message handlers
int CSubPartsView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	m_dlg.Create( IDD_SABPARTS_DIALOG, this );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
// CSabPartsDlg dialog


CSabPartsDlg::CSabPartsDlg( int nID, CWnd* pParent /*=NULL*/)
	: CDialog(CSabPartsDlg::IDD, pParent), nConstructionPartID( nID )
{
	//{{AFX_DATA_INIT(CSabPartsDlg)
	//}}AFX_DATA_INIT
	memset( m_parts, 0, sizeof(m_parts) );
}


static int spControls[NDb::CConstructionPart::N_SUBPARTS][NDb::CConstructionPart::N_SUBPARTS] = 
{
	{ IDC_SUBPART_11, IDC_SUBPART_12, IDC_SUBPART_13, IDC_SUBPART_14, IDC_SUBPART_15	},
	{ IDC_SUBPART_21, IDC_SUBPART_22, IDC_SUBPART_23, IDC_SUBPART_24, IDC_SUBPART_25	},
	{ IDC_SUBPART_31, IDC_SUBPART_32, IDC_SUBPART_33, IDC_SUBPART_34, IDC_SUBPART_35	},
	{ IDC_SUBPART_41, IDC_SUBPART_42, IDC_SUBPART_43, IDC_SUBPART_44, IDC_SUBPART_45	},
	{ IDC_SUBPART_51, IDC_SUBPART_52, IDC_SUBPART_53, IDC_SUBPART_54, IDC_SUBPART_55	},
};

void CSabPartsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSabPartsDlg)
	//}}AFX_DATA_MAP
	for ( int j = 0; j < NDb::CConstructionPart::N_SUBPARTS; ++j )
		for ( int i = 0; i < NDb::CConstructionPart::N_SUBPARTS; ++i )
			DDX_Check( pDX, spControls[j][i], m_parts[j][i] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSabPartsDlg::SetConstructionPart( int nID )
{
	nMask = -1;
	for ( int j = 0; j < NDb::CConstructionPart::N_SUBPARTS; ++j )
		for ( int i = 0; i < NDb::CConstructionPart::N_SUBPARTS; ++i )
		{
			CWnd *p = GetDlgItem( spControls[j][i] );
			if ( p )
				p->ModifyStyle( 0, WS_DISABLED );
		}	
	//
	const SResTree *pTree = theApp.GetResTree( IDC_CONSTRUCTIONPARTS_TREE );
	if ( !pTree )
		return;
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nID );
	if ( !pProps )
		return;
	CPropMap::const_iterator ix = pProps->find( "SizeX" );
	CPropMap::const_iterator iy = pProps->find( "SizeY" );
	CPropMap::const_iterator im = pProps->find( "SubPartMask" );
	if ( pProps->end() == ix || pProps->end() == iy || pProps->end() == im )
		return;
	const int nSizeX = Min( (int)NDb::CConstructionPart::N_SUBPARTS, (int)ix->second->GetValue() );
	const int nSizeY = Min( (int)NDb::CConstructionPart::N_SUBPARTS, (int)iy->second->GetValue() );
	nMask = im->second->GetValue();
	for ( int j = 0; j < NDb::CConstructionPart::N_SUBPARTS; ++j )
		for ( int i = 0; i < NDb::CConstructionPart::N_SUBPARTS; ++i )
		{
			CWnd *p = GetDlgItem( spControls[j][i] );
			if ( p && j < nSizeY && i < nSizeX )
			{
				p->ModifyStyle( WS_DISABLED, 0 );
				CheckDlgButton( spControls[j][i], NDb::CConstructionPart::IsPrimaryPart( nMask, i, j ) );
			}
		}
	pTree->pItemsTree->ReleasePropList( pProps );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSabPartsDlg::GetMask()
{
	if ( !::IsWindow( m_hWnd ) )
		return nMask;
	UpdateData();
	nMask = 0;
	for ( int j = 0; j < NDb::CConstructionPart::N_SUBPARTS; ++j )
		for ( int i = 0; i < NDb::CConstructionPart::N_SUBPARTS; ++i )
		{
			CWnd *p = GetDlgItem( spControls[j][i] );
			if ( !p || p->GetStyle() & WS_DISABLED )
				continue;
			//NDb::CConstructionPart::SetPrimaryPart( &nMask, j, i, IsDlgButtonChecked( spControls[j][i] ) );
			NDb::CConstructionPart::SetPrimaryPart( &nMask, i, j, m_parts[j][i] );
		}
	return nMask;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CSabPartsDlg, CDialog)
	//{{AFX_MSG_MAP(CSabPartsDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CSabPartsDlg message handlers
BOOL CSabPartsDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	SetConstructionPart( nConstructionPartID );
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSabPartsDlg::OnOK() 
{
	GetMask();
	CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
