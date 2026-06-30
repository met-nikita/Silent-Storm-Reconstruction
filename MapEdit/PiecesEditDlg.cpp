// PiecesEditDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "PiecesEditDlg.h"
#include "..\DBFormat\DataGeometry.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// CPiecesEditDlg dialog

IMPLEMENT_DYNAMIC(CPiecesEditDlg, CDialog)
CPiecesEditDlg::CPiecesEditDlg( int nAIGeomID, CWnd* pParent /*=NULL*/)
	: CDialog(CPiecesEditDlg::IDD, pParent), nAIGeometryID(nAIGeomID)
	, m_szOriginalPieces(_T(""))
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPiecesEditDlg::~CPiecesEditDlg()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPiecesEditDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_INFO_PLACE, m_ctrlInfoPlace);
	DDX_Text(pDX, IDC_ORIGINAL_PIECES, m_szOriginalPieces);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CPiecesEditDlg::OnInitDialog()
{
	if ( !CDialog::OnInitDialog() )
		return FALSE;

	CRect r;
	m_ctrlInfoPlace.GetWindowRect( &r );
	ScreenToClient( &r );
	m_infoView.SetAIGeometry( nAIGeometryID );
	m_infoView.szAdditionalPices = szData;
	if ( !m_infoView.Create(NULL, "InfoView", AFX_WS_DEFAULT_VIEW, r, this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create info view window\n");
		return -1;
	}
	m_szOriginalPieces = m_infoView.szOriginalPieces.c_str();
	UpdateData(FALSE);

	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CPiecesEditDlg, CDialog)
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CPiecesEditDlg message handlers

void CPiecesEditDlg::OnBnClickedOk()
{
	szData = m_infoView.GetAdditionalPieces();
	OnOK();
}
