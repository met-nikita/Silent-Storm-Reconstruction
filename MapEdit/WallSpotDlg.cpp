#include "stdafx.h"
#include "mapedit.h"
#include "WallSpotDlg.h"
#include "..\Main\BuildingInfo.h"
#include "MESerialize.h"
#include "..\Main\iMain.h"
#include "..\Input\Bind.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CWallSpotDlg dialog
CWallSpotDlg::CWallSpotDlg( NBuilding::SProjectedSpot *_pSpot, CWnd* pParent /*=NULL*/)
	: CDialog(CWallSpotDlg::IDD, pParent), pSpot(_pSpot)
{
}


void CWallSpotDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CBaseObjDlg)
	DDX_Check(pDX, IDC_MATERIAL0, m_bMaterial0);
	DDX_Check(pDX, IDC_MATERIAL1, m_bMaterial1);
	DDX_Check(pDX, IDC_MATERIAL2, m_bMaterial2);
	DDX_Check(pDX, IDC_MATERIAL3, m_bMaterial3);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CWallSpotDlg, CDialog)
	//{{AFX_MSG_MAP(CWallSpotDlg)
	ON_BN_CLICKED(IDC_SELECTFRAGMENTS, OnSelectFragments)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CBaseObjDlg message handlers

////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CWallSpotDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_bMaterial0 = pSpot->IsMaterialEnabled( 0 );
	m_bMaterial1 = pSpot->IsMaterialEnabled( 1 );
	m_bMaterial2 = pSpot->IsMaterialEnabled( 2 );
	m_bMaterial3 = pSpot->IsMaterialEnabled( 3 );
	UpdateData( FALSE );
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallSpotDlg::OnOK()
{
	UpdateData();

	bool bUpdate = false;

	if ( m_bMaterial0 != (BOOL)pSpot->IsMaterialEnabled( 0 ) ) bUpdate = true;
	if ( m_bMaterial1 != (BOOL)pSpot->IsMaterialEnabled( 1 ) ) bUpdate = true;
	if ( m_bMaterial2 != (BOOL)pSpot->IsMaterialEnabled( 2 ) ) bUpdate = true;
	if ( m_bMaterial3 != (BOOL)pSpot->IsMaterialEnabled( 3 ) ) bUpdate = true;
	pSpot->EnableMaterial( 0, m_bMaterial0 );
	pSpot->EnableMaterial( 1, m_bMaterial1 );
	pSpot->EnableMaterial( 2, m_bMaterial2 );
	pSpot->EnableMaterial( 3, m_bMaterial3 );

	if ( bUpdate )
	{
		NInput::PostEvent( "serialize_building" );
		NMainLoop::StepApp( false, true );
	}
	CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallSpotDlg::OnSelectFragments()
{
	NInput::PostEvent( "select_spot_fragments" );
	OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
