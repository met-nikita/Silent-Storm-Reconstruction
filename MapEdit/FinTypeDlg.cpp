// FinTypeDlg.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "FinTypeDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// CFinTypeDlg dialog


CFinTypeDlg::CFinTypeDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CFinTypeDlg::IDD, pParent)
{
  isUnit = true;

	//{{AFX_DATA_INIT(CFinTypeDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CFinTypeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFinTypeDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CFinTypeDlg, CDialog)
	//{{AFX_MSG_MAP(CFinTypeDlg)
	ON_BN_CLICKED(IDC_RADIO_UNIT, OnRadioUnit)
	ON_BN_CLICKED(IDC_RADIO_FINELEM, OnRadioFinelem)
	ON_WM_SHOWWINDOW()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CFinTypeDlg message handlers

void CFinTypeDlg::OnRadioUnit() 
{
	isUnit = true;
}

void CFinTypeDlg::OnRadioFinelem() 
{
  isUnit = false;	
}

void CFinTypeDlg::OnShowWindow(BOOL bShow, UINT nStatus) 
{
	CDialog::OnShowWindow(bShow, nStatus);
	
	if ( bShow )
  {
    CButton *pBtn = (CButton*)GetDlgItem( IDC_RADIO_UNIT );
    
    if ( pBtn )
      pBtn->SetCheck( 1 );
  }
}
