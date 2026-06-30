// TerrainExportDlg.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "TerrainExportDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTerrainExportDlg dialog


CTerrainExportDlg::CTerrainExportDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CTerrainExportDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CTerrainExportDlg)
	m_szDirectory = _T("");
	m_bExportAll = FALSE;
	//}}AFX_DATA_INIT
}


void CTerrainExportDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTerrainExportDlg)
	DDX_Text(pDX, IDC_DIRECTORY, m_szDirectory);
	DDX_Check(pDX, IDC_EXPORT_ALLLAYERS, m_bExportAll);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTerrainExportDlg, CDialog)
	//{{AFX_MSG_MAP(CTerrainExportDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTerrainExportDlg message handlers

BOOL CTerrainExportDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_directory.Initialize( IDC_DIRECTORY, this );
	m_directory.SetWindowText( m_szDirectory );
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
