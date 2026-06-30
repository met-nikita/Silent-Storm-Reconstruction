// TreeSeltemDlg.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "TreeSeltemDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTreeSeltemDlg dialog


CTreeSeltemDlg::CTreeSeltemDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CTreeSeltemDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CTreeSeltemDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CTreeSeltemDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTreeSeltemDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTreeSeltemDlg, CDialog)
	//{{AFX_MSG_MAP(CTreeSeltemDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTreeSeltemDlg message handlers
