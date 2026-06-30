// GeometryPage.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "GeometryPage.h"
#include "IWysiwyg.h"
#include "TreeSelItemDlg.h"
#include "dbDefs.h"
#include "ItemsMgr.h"
#include "MEUserSettings.h"
#include "UserSettingsSetup.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CGeometryPage property page
IMPLEMENT_DYNCREATE(CGeometryPage, CDialog)

CGeometryPage::CGeometryPage() : CDialog(CGeometryPage::IDD)
{
	//{{AFX_DATA_INIT(CGeometryPage)
	m_szGeometry = _T("");
	m_szGeomRotation = _T("0");
	//}}AFX_DATA_INIT
	nConstructionPartID = -1;
}

CGeometryPage::~CGeometryPage()
{
}

void CGeometryPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGeometryPage)
	DDX_Text(pDX, IDC_GEOMETRY_SZAPPEARANCE, m_szGeometry);
	DDX_CBString(pDX, IDC_GEOMETRY_ROTATION, m_szGeomRotation);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CGeometryPage, CDialog)
	//{{AFX_MSG_MAP(CGeometryPage)
	ON_BN_CLICKED(IDC_GEOMETRY_APPEARANCE, OnGeometryAppearance)
	ON_CBN_CLOSEUP(IDC_GEOMETRY_ROTATION, OnCloseupGeometryRotation)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CGeometryPage message handlers
void CGeometryPage::OnGeometryAppearance() 
{
	const SResTree *pTree = theApp.GetResTree( IDC_CONSTRUCTIONPARTS_TREE );
	if ( !pTree )
		return;
	pTree->pTreeDlg->SetSelectedItemID( IDC_CONSTRUCTIONPARTS_TREE, nConstructionPartID );
	if ( IDOK != pTree->pTreeDlg->DoModal() )
		return;
	int nTree, nItem;
	pTree->pTreeDlg->GetSelectedItemID( &nTree, &nItem );
	if ( -1 == nItem )
		return;
	UpdateData();
	pTree->pTreeDlg->GetSelectedItemID( &nTree, &nItem );
	nConstructionPartID = nItem;
	m_szGeometry = pTree->pItemsTree->GetItemName( nConstructionPartID );
	UpdateData( FALSE );
	//wysiwygBrush.nConstructionPartID = nConstructionPartID;
}

void CGeometryPage::OnCloseupGeometryRotation() 
{
	UpdateData();
	int nRotation = atoi( m_szGeomRotation );
	int nRotationID = AngleToRotationID( nRotation );
	nRotation = RotationIDToAngle( nRotationID );
	m_szGeomRotation = IToA( nRotation ).c_str();
	IUserSettingsSetup &setup = GetUserSettingsSetup();
	setup.SetActiveRotationID( nRotationID );
	UpdateData( FALSE );	
}
