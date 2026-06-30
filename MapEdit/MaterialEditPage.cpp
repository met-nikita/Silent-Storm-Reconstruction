// MaterialEditPage.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "MaterialEditPage.h"
#include "IWysiwyg.h"
#include "dbDefs.h"
#include "TreeSelItemDlg.h"
#include "MaterialSetEditDlg.h"
#include "ItemsMgr.h"
#include "MEUserSettings.h"
#include "UserSettingsSetup.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CMaterialEditPage property page

IMPLEMENT_DYNCREATE(CMaterialEditPage, CDialog)

CMaterialEditPage::CMaterialEditPage(): CDialog(CMaterialEditPage::IDD) //: CPropertyPage(CMaterialEditPage::IDD)
{
	//{{AFX_DATA_INIT(CMaterialEditPage)
	m_nRotation = 0;
	m_fScale = 1.0f;
	m_fShiftX = 0.0f;
	m_fShiftY = 0.0f;
	m_szMaterial = _T("");
	//}}AFX_DATA_INIT
	nMaterialID = -1;
	nRPGArmorID = -1;
}

CMaterialEditPage::~CMaterialEditPage()
{
}

void CMaterialEditPage::DoDataExchange(CDataExchange* pDX)
{
	//CPropertyPage::DoDataExchange(pDX);
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMaterialEditPage)
	DDX_Control(pDX, IDC_MATERIAL, m_material);
	DDX_Control(pDX, IDC_MATERIAL_SET, m_materialSet);
	//	DDX_Text(pDX, IDC_MATERIAL_ROTATION, m_nRotation);
	//	DDX_Text(pDX, IDC_MATERIAL_SCALE, m_fScale);
	//	DDX_Text(pDX, IDC_MATERIAL_SHIFTX, m_fShiftX);
	//	DDX_Text(pDX, IDC_MATERIAL_SHIFTY, m_fShiftY);
	//	DDX_Text(pDX, IDC_MATERIAL_SZAPPEARANCE, m_szMaterial);
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_MATERIAL_SET2, m_materialSet2);
	DDX_Control(pDX, IDC_MATERIAL_SET3, m_materialSet3);
	DDX_Control(pDX, IDC_MATERIAL_SET4, m_materialSet4);
	DDX_Control(pDX, IDC_MATERIAL_SET5, m_materialSet5);
}

void CMaterialEditPage::GetValues()
{
	UpdateData();
	vector<NBuilding::SRawMaterialApply> materials(1);
	NBuilding::SRawMaterialApply &m = materials.front();
	m.mapping = NBuilding::SMaterialApply::NORMAL;
	m.nTMaterialID = nMaterialID;
	m.fScale    = m_fScale;
	m.ptShift.x = m_fShiftX;
	m.ptShift.y = m_fShiftY;
	m.nRotation = m_nRotation;
	GetUserSettingsSetup().SetMaterial( MSET_FIRST, materials );
	//GetUserSettingsSetup().SetActiveMaterialSet( MSET_FIRST );
}

//BEGIN_MESSAGE_MAP(CMaterialEditPage, CDialog)
BEGIN_MESSAGE_MAP(CMaterialEditPage, CDialog)
	//{{AFX_MSG_MAP(CMaterialEditPage)
	ON_BN_CLICKED(IDC_MATERIAL_APPEARANCE, OnMaterialAppearance)
	ON_BN_CLICKED(IDC_MATERIAL_SET, OnMaterialSet)
	ON_BN_CLICKED(IDC_MATERIAL, OnMaterial)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_RADIO1, OnBnClickedRadio1)
	ON_BN_CLICKED(IDC_RADIO2, OnBnClickedRadio2)
	ON_BN_CLICKED(IDC_RADIO3, OnBnClickedRadio3)
	ON_BN_CLICKED(IDC_RADIO4, OnBnClickedRadio4)
	ON_BN_CLICKED(IDC_MATERIAL_SET2, OnBnClickedMaterialSet2)
	ON_BN_CLICKED(IDC_MATERIAL_SET3, OnBnClickedMaterialSet3)
	ON_BN_CLICKED(IDC_RADIO5, OnBnClickedRadio5)
	ON_BN_CLICKED(IDC_RADIO6, OnBnClickedRadio6)
	ON_BN_CLICKED(IDC_MATERIAL_SET4, OnBnClickedMaterialSet4)
	ON_BN_CLICKED(IDC_MATERIAL_SET5, OnBnClickedMaterialSet5)
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CMaterialEditPage message handlers
BOOL CMaterialEditPage::PreTranslateMessage(MSG* pMsg) 
{
	switch ( pMsg->message )
	{
		case WM_KEYDOWN:
			if ( VK_RETURN == pMsg->wParam )
			{
				GetValues();
			}
			break;
	}
	return CDialog::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::OnMaterialAppearance() 
{
	const SResTree *pTree = theApp.GetResTree( IDC_MATERIALS_TREE );
	if ( !pTree )
		return;
	pTree->pTreeDlg->SetSelectedItemID( IDC_MATERIALS_TREE, nMaterialID );
	if ( IDOK != pTree->pTreeDlg->DoModal() )
		return;
	//m_material.SetState( TRUE );
	//m_materialSet.SetState( FALSE );
	int nTree, nItem;
	pTree->pTreeDlg->GetSelectedItemID( &nTree, &nItem );
	if ( -1 == nItem )
		return;
	UpdateData();
	pTree->pTreeDlg->GetSelectedItemID( &nTree, &nItem );
	nMaterialID = nItem;
	m_szMaterial = pTree->pItemsTree->GetItemName( nMaterialID );
	UpdateData( FALSE );
	GetValues();
	theApp.WriteProfileInt( REG_MATERIALS, GetMSetString( MSET_FIRST, 0 ).c_str(), nMaterialID );
	SetName( MSET_FIRST, m_material );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::OnSetfocusMaterialArmor() 
{
	/*
	static bool bLock = false;
	if ( bLock )
		return;
	const SResTree *pTree = theApp.GetResTree( IDC_RPG_ARMORS_TREE );
	if ( !pTree )
		return;
	pTree->pTreeDlg->SetSelectedItemID( nRPGArmorID );
	bLock = true;
	if ( IDOK == pTree->pTreeDlg->DoModal() && -1 != pTree->pTreeDlg->GetSelectedItemID() )
	{
		UpdateData();
		nRPGArmorID = pTree->pTreeDlg->GetSelectedItemID();
		m_szArmor = pTree->pItemsTree->GetItemName( nRPGArmorID );
		UpdateData( FALSE );
		GetValues( &wysiwygBrush );
	}
	bLock = false;
	NextDlgCtrl();
	*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::OnMaterial() 
{
	OnMaterialAppearance();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::SetCheck( int nControlID )
{
	CButton *pB = (CButton*)GetDlgItem( nControlID );
	if ( pB )
		pB->SetCheck( 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CMaterialEditPage::OnInitDialog()
{
	CDialog::OnInitDialog();

	const SResTree *pTree = theApp.GetResTree( IDC_MATERIALS_TREE );
	if ( pTree )
	{
		nMaterialID = theApp.GetProfileInt( REG_MATERIALS, GetMSetString( MSET_FIRST, 0 ).c_str(), -1 );
		m_szMaterial = pTree->pItemsTree->GetItemName( nMaterialID );
		UpdateData( FALSE );
	}

	SetCheck( IDC_RADIO1 );
	SetNames();
	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::SetNames()
{
	SetName( MSET_FIRST, m_material );
	SetName( MSET_SECOND, m_materialSet );
	SetName( MSET_THIRD, m_materialSet2 );
	SetName( MSET_FOURTH, m_materialSet3 );
	SetName( MSET_FIFTH, m_materialSet4 );
	SetName( MSET_SIXTH, m_materialSet5 );
	UpdateData( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::OnBnClickedRadio1()
{
	GetUserSettingsSetup().SetActiveMaterialSet( MSET_FIRST );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::OnBnClickedRadio2()
{
	GetUserSettingsSetup().SetActiveMaterialSet( MSET_SECOND );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::OnBnClickedRadio3()
{
	GetUserSettingsSetup().SetActiveMaterialSet( MSET_THIRD );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::OnBnClickedRadio4()
{
	GetUserSettingsSetup().SetActiveMaterialSet( MSET_FOURTH );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::OnBnClickedRadio5()
{
	GetUserSettingsSetup().SetActiveMaterialSet( MSET_FIFTH );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::OnBnClickedRadio6()
{
	GetUserSettingsSetup().SetActiveMaterialSet( MSET_SIXTH );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::OnMaterialSet() 
{
	CMaterialSetEditDlg	dlg( MSET_SECOND );
	dlg.DoModal();
	SetName( MSET_SECOND, m_materialSet );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::OnBnClickedMaterialSet2()
{
	CMaterialSetEditDlg	dlg( MSET_THIRD );
	dlg.DoModal();
	SetName( MSET_THIRD, m_materialSet2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::OnBnClickedMaterialSet3()
{
	CMaterialSetEditDlg	dlg( MSET_FOURTH );
	dlg.DoModal();
	SetName( MSET_FOURTH, m_materialSet3 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::OnBnClickedMaterialSet4()
{
	CMaterialSetEditDlg	dlg( MSET_FIFTH );
	dlg.DoModal();
	SetName( MSET_FIFTH, m_materialSet4 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::OnBnClickedMaterialSet5()
{
	CMaterialSetEditDlg	dlg( MSET_SIXTH );
	dlg.DoModal();
	SetName( MSET_SIXTH, m_materialSet5 );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterialEditPage::SetName( EMaterialSet set, CButton &btn )
{
	const SResTree *pTree = theApp.GetResTree( IDC_MATERIALS_TREE );
	if ( !pTree )
		return;
	IUserSettingsSetup &s = GetUserSettingsSetup();
	vector<NBuilding::SRawMaterialApply> mats;

	s.GetMaterial( set, &mats );
	if ( !mats.empty() )
	{
		string szMat = pTree->pItemsTree->GetItemName( mats.front().nTMaterialID );
		szMat += "...";
		btn.SetWindowText( szMat.c_str() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
