// ChapterSectorDlg.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "ChapterSectorDlg.h"
#include "TreeSelItemDlg.h"
#include "ItemsMgr.h"
#include "dbDefs.h"
#include "..\Main\ChapterInfo.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterSectorDlg dialog


CChapterSectorDlg::CChapterSectorDlg( bool _bGlobalSector, CWnd* pParent /*=NULL*/)
	: CDialog(CChapterSectorDlg::IDD, pParent)
	, m_szID(_T("")), bGlobalSector(_bGlobalSector)
{
	//{{AFX_DATA_INIT(CChapterSectorDlg)
	m_nProbability = 0;
	//}}AFX_DATA_INIT
	pTemplates = 0;
	const SResTree *pTree = theApp.GetResTree( IDC_TEMPLATE_TREE );
	const SResTree *pStrTree = theApp.GetResTree( IDC_STRINGS_TREE );
	const SResTree *pChTree = theApp.GetResTree( IDC_CHAPTERS_TREE );
	const SResTree *pImTree = theApp.GetResTree( IDC_INTERFACES_TREE );
	const SResTree *pSZTree = theApp.GetResTree( IDC_SCENARIO_ZONES );
	if ( pTree )
		pTemplates = pTree->pItemsTree;
	if ( pStrTree )
		pStrings = pStrTree->pItemsTree;
	if ( pChTree )
		pChapters = pChTree->pItemsTree;
	if ( pImTree )
		pImages = pImTree->pItemsTree;
	if ( pSZTree )
		pScenarioZones = pSZTree->pItemsTree;
}


void CChapterSectorDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CChapterSectorDlg)
	DDX_Text(pDX, IDC_PROBABILITY_EDIT, m_nProbability );
	DDX_Control(pDX, IDC_TEMPLATESELECT, m_btnTemplate );
	DDX_Control(pDX, IDC_DESCRSELECT, m_btnDescr );
	DDX_Control(pDX, IDC_IMAGESELECT, m_btnImg );
	//}}AFX_DATA_MAP
	DDX_Text(pDX, IDC_SECTOR_ID, m_szID);
	DDX_Text(pDX, IDC_STATIC_TEMPLATE, m_szTemplateCaption);
	DDX_Control(pDX, IDC_SECTOR_ID, m_ctrlStrID);
	DDX_Control(pDX, IDC_PROBABILITY_EDIT, m_ctrlProbability);
	DDX_Control(pDX, IDC_RANDOM_SECTOR, m_ctrlRandom);
	DDX_Control(pDX, IDC_ZONE, m_ctrlZone);
	DDX_Control(pDX, IDC_EXITZONE, m_ctrlExitZone);
	DDX_Control(pDX, IDC_SECTOR_X, m_ctrlX);
	DDX_Control(pDX, IDC_SECTOR_Y, m_ctrlY);
	DDX_Text(pDX, IDC_SECTOR_X, m_nX);
	DDX_Text(pDX, IDC_SECTOR_Y, m_nY);
}


BEGIN_MESSAGE_MAP(CChapterSectorDlg, CDialog)
	//{{AFX_MSG_MAP(CChapterSectorDlg)
	ON_BN_CLICKED(IDC_TEMPLATESELECT, OnTemplateSelect)
	ON_BN_CLICKED(IDC_DESCRSELECT, OnDescrSelect)
	ON_BN_CLICKED(IDC_IMAGESELECT, OnImageSelect)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_RANDOM_SECTOR, OnBnClickedRandomSector)
	ON_BN_CLICKED(IDC_ZONE, OnBnClickedZone)
	ON_BN_CLICKED(IDC_EXITZONE, OnBnClickedExitzone)
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterSectorDlg message handlers

BOOL CChapterSectorDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	m_btnDescr.SetWindowText( pStrings->GetItemName( m_nDescrID ) );
	if ( bGlobalSector )
	{
		m_btnTemplate.SetWindowText( pChapters->GetItemName( m_nTemplteID ) );
		m_btnImg.SetWindowText( pImages->GetItemName( m_nImageID ) );
		m_ctrlStrID.EnableWindow( false );
		m_ctrlProbability.EnableWindow( false );
		m_ctrlRandom.EnableWindow( false );
		m_ctrlZone.EnableWindow( false );
		m_ctrlExitZone.EnableWindow( false );
		m_szTemplateCaption = "Chapter:";
	}
	else
	{
		UpdateSectorType( false );
		m_btnImg.EnableWindow( false );
		m_ctrlX.EnableWindow( false );
		m_ctrlY.EnableWindow( false );
	}
	UpdateData( FALSE );
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterSectorDlg::OnTemplateSelect()
{
	int nTreeID = -1;
	if ( bGlobalSector )
		nTreeID = IDC_CHAPTERS_TREE;
	else if ( eSectorType == RANDOM )
		nTreeID = IDC_TEMPLATE_TREE;
	else
		nTreeID = IDC_SCENARIO_ZONES;
	UpdateData();
	const SResTree *pTree = theApp.GetResTree( nTreeID );

	if ( !pTree )
		return;
	pTree->pTreeDlg->SetSelectedItemID( nTreeID, m_nTemplteID );
	if ( IDOK != pTree->pTreeDlg->DoModal() )
		return;
	int nTree;
	pTree->pTreeDlg->GetSelectedItemID( &nTree, &m_nTemplteID );
	m_btnTemplate.SetWindowText( pTree->pItemsTree->GetItemName( m_nTemplteID ) );
	UpdateData( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterSectorDlg::OnDescrSelect()
{
	UpdateData();
	const SResTree *pTree = theApp.GetResTree( IDC_STRINGS_TREE );

	if ( !pTree )
		return;
	pTree->pTreeDlg->SetSelectedItemID( IDC_STRINGS_TREE, m_nDescrID );
	if ( IDOK != pTree->pTreeDlg->DoModal() )
		return;
	int nTree;
	pTree->pTreeDlg->GetSelectedItemID( &nTree, &m_nDescrID );
	m_btnDescr.SetWindowText( pStrings->GetItemName( m_nDescrID ) );
	UpdateData( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterSectorDlg::OnImageSelect()
{
	UpdateData();
	const SResTree *pTree = theApp.GetResTree( IDC_INTERFACES_TREE );

	if ( !pTree )
		return;
	pTree->pTreeDlg->SetSelectedItemID( IDC_INTERFACES_TREE, m_nImageID );
	if ( IDOK != pTree->pTreeDlg->DoModal() )
		return;
	int nTree;
	pTree->pTreeDlg->GetSelectedItemID( &nTree, &m_nImageID );
	m_btnImg.SetWindowText( pImages->GetItemName( m_nImageID ) );
	UpdateData( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterSectorDlg::OnOK()
{
	UpdateData();
	CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterSectorDlg::OnBnClickedRandomSector()
{
	UpdateSectorType( true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterSectorDlg::OnBnClickedZone()
{
	UpdateSectorType( true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterSectorDlg::OnBnClickedExitzone()
{
	UpdateSectorType( true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterSectorDlg::UpdateSectorType( bool bSave )
{
	CButton *pRnd = (CButton*)GetDlgItem( IDC_RANDOM_SECTOR );
	CButton *pZone = (CButton*)GetDlgItem( IDC_ZONE );
	CButton *pEZone = (CButton*)GetDlgItem( IDC_EXITZONE );

	if ( !pRnd || !pZone || !pEZone )
		return;

	if ( bSave )
	{
		if ( pRnd->GetCheck() > 0 )
			eSectorType = RANDOM;
		else if ( pZone->GetCheck() > 0 )
			eSectorType = ZONE;
		else
			eSectorType = EXITZONE;
	}
	else
	{
		switch( eSectorType ) 
		{
			case RANDOM:
				pRnd->SetCheck( 2 );
				pZone->SetCheck( 0 );
				pEZone->SetCheck( 0 );
				break;
			case ZONE:
				pRnd->SetCheck( 0 );
				pZone->SetCheck( 2 );
				pEZone->SetCheck( 0 );
				break;
			case EXITZONE:
				pRnd->SetCheck( 0 );
				pZone->SetCheck( 0 );
				pEZone->SetCheck( 2 );
				break;
			default:
				ASSERT(0);
				break;
		}
	}
	//
	if ( eSectorType == RANDOM )
	{
		m_btnTemplate.SetWindowText( "" );
		m_szTemplateCaption = "Template:";
	}
	else
	{
		m_btnTemplate.SetWindowText( "" );
		m_szTemplateCaption = "ScenarioZone:";
	}
	UpdateData(FALSE);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
