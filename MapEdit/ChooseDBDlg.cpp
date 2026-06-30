// ChooseDBDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "ChooseDBDlg.h"


// CChooseDBDlg dialog

IMPLEMENT_DYNAMIC(CChooseDBDlg, CDialog)
CChooseDBDlg::CChooseDBDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CChooseDBDlg::IDD, pParent)
	, m_szDBServer(_T(""))
{
}

CChooseDBDlg::~CChooseDBDlg()
{
}

void CChooseDBDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_DBSERVER, m_szDBServer);
}


BEGIN_MESSAGE_MAP(CChooseDBDlg, CDialog)
END_MESSAGE_MAP()


// CChooseDBDlg message handlers
