// TexSpotPage.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "TexSpotPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTexSpotPage dialog


CTexSpotPage::CTexSpotPage(CWnd* pParent /*=NULL*/)
	: CDialog(CTexSpotPage::IDD, pParent)
{
	//{{AFX_DATA_INIT(CTexSpotPage)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CTexSpotPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTexSpotPage)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTexSpotPage, CDialog)
	//{{AFX_MSG_MAP(CTexSpotPage)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTexSpotPage message handlers
