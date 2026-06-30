// CameraInfoDlg.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "CameraInfoDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CCameraInfoDlg dialog


CCameraInfoDlg::CCameraInfoDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CCameraInfoDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CCameraInfoDlg)
	m_szInfo = _T("");
	//}}AFX_DATA_INIT
}


void CCameraInfoDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CCameraInfoDlg)
	DDX_Text(pDX, IDC_CAMERA_INFO, m_szInfo);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CCameraInfoDlg, CDialog)
	//{{AFX_MSG_MAP(CCameraInfoDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CCameraInfoDlg message handlers
