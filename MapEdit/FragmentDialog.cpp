#include "stdafx.h"
#include "mapedit.h"
#include "FragmentDialog.h"
#include "..\Main\BuildingInfo.h"
#include "MESerialize.h"
#include "FinDBCmd.h"
#include "..\DBFormat\DataMap.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace NWysiwyg
{
	extern bool UpdateBuildInfo( int nBuildingID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBaseObjDlg dialog

CBaseObjDlg::CBaseObjDlg( CWnd* pParent /*=NULL*/)
	: CDialog(CBaseObjDlg::IDD, pParent)
{
}


void CBaseObjDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CBaseObjDlg)
	DDX_Check(pDX, IDC_OPEN_OBJECT_CHECK, m_bOpen);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CBaseObjDlg, CDialog)
	//{{AFX_MSG_MAP(CBaseObjDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CBaseObjDlg message handlers

////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CBaseObjDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	UpdateData( FALSE );
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CFragmentDlg::CFragmentDlg(int nWID, int nData, CWnd* pParent /*=NULL*/)
	: CBaseObjDlg( pParent), nWorldID(nWID), pFr((NBuilding::SBuildFragment*)nData)
{
	ASSERT(pFr);
	m_bOpen = pFr->nObjectFlags & NBuilding::OF_OPEN;
	//{{AFX_DATA_INIT(CWaypointDlg)
	//}}AFX_DATA_INIT
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFragmentDlg::OnOK() 
{
	UpdateData();

	if ( m_bOpen )
		pFr->nObjectFlags |= NBuilding::OF_OPEN;
	else
		pFr->nObjectFlags &= ~NBuilding::OF_OPEN;
	NWysiwyg::UpdateBuildInfo( nWorldID );
	CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectDlg::CObjectDlg( NDb::CFinalElement *_pFin, CWnd* pParent /*=NULL*/)
	: CBaseObjDlg( pParent), pFin(_pFin)
{
	if ( !IsValid( pFin ) )
	{
		ASSERT( 0 );
		return;
	}
	m_bOpen = pFin->bOpen;
	//{{AFX_DATA_INIT(CWaypointDlg)
	//}}AFX_DATA_INIT
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CObjectDlg::OnInitDialog()
{
	CBaseObjDlg::OnInitDialog();
	if ( !IsValid( pFin ) )
	{
		ASSERT( 0 );
		return FALSE;
	}
	CButton *pNrm = (CButton*)GetDlgItem( IDC_LIGHT_NORMAL );
	CButton *pLm = (CButton*)GetDlgItem( IDC_LIGHT_LIGHTMAP );
	if ( pNrm && pLm )
	{
		pNrm->EnableWindow();
		pLm->EnableWindow();
		SetLightCheck();
	}
	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectDlg::SetLightCheck()
{
	CButton *pNrm = (CButton*)GetDlgItem( IDC_LIGHT_NORMAL );
	CButton *pLm = (CButton*)GetDlgItem( IDC_LIGHT_LIGHTMAP );
	if ( pNrm && pLm )
	{
		if ( pFin->bLightmap )
		{
			pLm->SetCheck( 2 );
			pNrm->SetCheck( 0 );
		}
		else
		{
			pLm->SetCheck( 0 );
			pNrm->SetCheck( 2 );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectDlg::GetLightCheck()
{
	CButton *pNrm = (CButton*)GetDlgItem( IDC_LIGHT_NORMAL );
	CButton *pLm = (CButton*)GetDlgItem( IDC_LIGHT_LIGHTMAP );
	if ( pNrm && pLm )
	{
		if ( pLm->GetCheck() > 0 )
			pFin->bLightmap = true;
		else
			pFin->bLightmap = false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectDlg::OnOK() 
{
	if ( !IsValid( pFin ) )
	{
		ASSERT( 0 );
		return;
	}
	static CFinPosDB db;
	UpdateData();
	GetLightCheck();

	pFin->bOpen = m_bOpen;
	db.SetOpen( pFin->GetRecordID(), m_bOpen );
	db.SetLightmap( pFin->GetRecordID(), pFin->bLightmap );
	CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
