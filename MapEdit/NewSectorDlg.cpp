// NewSectorDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "NewSectorDlg.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// CNewSectorDlg dialog
IMPLEMENT_DYNAMIC(CNewSectorDlg, CDialog)
CNewSectorDlg::CNewSectorDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CNewSectorDlg::IDD, pParent)
	, m_nSegments(6)
{
}

CNewSectorDlg::~CNewSectorDlg()
{
}

void CNewSectorDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_SEGMENTCOUNT, m_nSegments);
}


BEGIN_MESSAGE_MAP(CNewSectorDlg, CDialog)
END_MESSAGE_MAP()


// CNewSectorDlg message handlers
