// MaterialSetEditDlg.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "MaterialSetEditDlg.h"
#include "IWysiwyg.h"
#include "dbDefs.h"
#include "TreeSelItemDlg.h"
#include "ItemsMgr.h"
#include "MEUserSettings.h"
#include "UserSettingsSetup.h"
#include "..\Misc\StrProc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CMaterialSetEditDlg dialog


CMaterialSetEditDlg::CMaterialSetEditDlg( EMaterialSet set, CWnd* pParent /*=NULL*/)
	: CDialog(CMaterialSetEditDlg::IDD, pParent)
{
	eSet = set;
	//{{AFX_DATA_INIT(CMaterialSetEditDlg)
	//}}AFX_DATA_INIT
}


void CMaterialSetEditDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMaterialSetEditDlg)
	DDX_Control(pDX, IDC_MATERIAL3, m_mat3);
	DDX_Control(pDX, IDC_MATERIAL2, m_mat2);
	DDX_Control(pDX, IDC_MATERIAL0, m_mat0);
	DDX_Control(pDX, IDC_MATERIAL1, m_mat1);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CMaterialSetEditDlg, CDialog)
	//{{AFX_MSG_MAP(CMaterialSetEditDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CMaterialSetEditDlg message handlers

string GetMSetString( EMaterialSet set, int nIndex )
{
	return string( "MaterialSet_" ) + IToA( set ) + '_' + IToA( nIndex );
}
bool GetMSetID( const string &str, EMaterialSet *pSet, int *pnIndex )
{
	vector<string> vstrs;
	NStr::SplitString( str, vstrs, '_' );
	if ( vstrs.size() != 3 || vstrs.front() != "MaterialSet" )
		return false;
	*pSet = (EMaterialSet)atoi( vstrs[1].c_str() );
	*pnIndex = atoi( vstrs[2].c_str() );
	return false;
}
void CMaterialSetEditDlg::OnOK() 
{
	ASSERT( N_MATERIALSET_SIZE == 4 );
	vector<NBuilding::SRawMaterialApply> mats( N_MATERIALSET_SIZE );
	mats[0].nTMaterialID = m_mat0.nMaterialID;
	mats[1].nTMaterialID = m_mat1.nMaterialID;
	mats[2].nTMaterialID = m_mat2.nMaterialID;
	mats[3].nTMaterialID = m_mat3.nMaterialID;
	GetUserSettingsSetup().SetMaterial( eSet, mats );
	//GetUserSettingsSetup().SetActiveMaterialSet( eSet );

	for ( int i = 0; i < 4; ++i )
		theApp.WriteProfileInt( REG_MATERIALS, GetMSetString( eSet, i ).c_str(), mats[i].nTMaterialID );
	CDialog::OnOK();
}

BOOL CMaterialSetEditDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	ASSERT( 4 == N_MATERIALSET_SIZE );
	vector<NBuilding::SRawMaterialApply> mats;
	GetUserSettingsSetup().GetMaterial( eSet, &mats );
	const int n = Max( N_MATERIALSET_SIZE - mats.size(), 0u );
	NBuilding::SRawMaterialApply m;
	m.nTMaterialID = -1;
	for ( int i = 0; i < n; ++i )
		mats.push_back( m );
	m_mat0.nMaterialID = mats[0].nTMaterialID;
	m_mat1.nMaterialID = mats[1].nTMaterialID;
	m_mat2.nMaterialID = mats[2].nTMaterialID;
	m_mat3.nMaterialID = mats[3].nTMaterialID;	
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMaterialEditCtrl

CMaterialEditCtrl::CMaterialEditCtrl(): nMaterialID(-1)
{
}

CMaterialEditCtrl::~CMaterialEditCtrl()
{
}


BEGIN_MESSAGE_MAP(CMaterialEditCtrl, CEdit)
	//{{AFX_MSG_MAP(CMaterialEditCtrl)
	ON_WM_LBUTTONDOWN()
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CMaterialEditCtrl message handlers
void CMaterialEditCtrl::OnLButtonDown(UINT nFlags, CPoint point) 
{
	const SResTree *pTree = theApp.GetResTree( IDC_MATERIALS_TREE );
	if ( !pTree )
		return;
	pTree->pTreeDlg->SetSelectedItemID( IDC_MATERIALS_TREE, nMaterialID );
	if ( IDOK != pTree->pTreeDlg->DoModal() )
		return;
	int nTree, nItemID;
	pTree->pTreeDlg->GetSelectedItemID( &nTree, &nItemID );
	if ( -1 == nItemID )
		return;
	UpdateData();
	pTree->pTreeDlg->GetSelectedItemID( &nTree, &nItemID );
	nMaterialID = nItemID;
	string szMaterial = pTree->pItemsTree->GetItemName( nMaterialID );
	SetWindowText( szMaterial.c_str() );
	UpdateData( FALSE );
	//CEdit::OnLButtonDown(nFlags, point);
}

void CMaterialEditCtrl::OnPaint() 
{
	const SResTree *pTree = theApp.GetResTree( IDC_MATERIALS_TREE );
	if ( pTree )
	{
		string szMaterial = pTree->pItemsTree->GetItemName( nMaterialID );
		CString str;
		GetWindowText( str );
		if ( szMaterial != (LPCSTR)str )
		{
			SetWindowText( szMaterial.c_str() );
			UpdateData( FALSE );
		}
	}	
	CEdit::OnPaint();
}
