// FindDialog.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "FindDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CFindDialog dialog


CFindDialog::CFindDialog(bool _bHasVariants, CWnd* pParent /*=NULL*/)
	: CDialog(CFindDialog::IDD, pParent)
{
	bItem = false;
	bHasVariants = _bHasVariants;
	//{{AFX_DATA_INIT(CFindDialog)
	m_nItemID = 0;
	m_nVariantID = 0;
	//}}AFX_DATA_INIT
}


void CFindDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFindDialog)
	DDX_Control(pDX, IDC_VARIANT_ID, m_VariantCtrl);
	DDX_Control(pDX, IDC_ITEM_ID, m_ItemCtrl);
	DDX_Text(pDX, IDC_ITEM_ID, m_nItemID);
	DDX_Text(pDX, IDC_VARIANT_ID, m_nVariantID);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CFindDialog, CDialog)
	//{{AFX_MSG_MAP(CFindDialog)
	ON_BN_CLICKED(IDC_RADIO_ITEM_ID, OnRadioItemID)
	ON_BN_CLICKED(IDC_RADIO_VARIANT_ID, OnRadioVariantID)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CFindDialog::SetCheck()
{
	CButton* pItem = (CButton*)GetDlgItem( IDC_RADIO_ITEM_ID );
	CButton* pVar = (CButton*)GetDlgItem( IDC_RADIO_VARIANT_ID );
	if ( !pItem || !pVar )
		return;
	if ( pItem->GetCheck() > 0 )
	{
		m_ItemCtrl.EnableWindow( TRUE );
		m_VariantCtrl.EnableWindow( FALSE );
	}
	else
	{
		m_ItemCtrl.EnableWindow( FALSE );
		m_VariantCtrl.EnableWindow( TRUE );
	}
	//
	if ( !bHasVariants )
	{
		pVar->EnableWindow( FALSE );
		m_VariantCtrl.EnableWindow( FALSE );
	}

	UpdateData( FALSE );
}
/////////////////////////////////////////////////////////////////////////////
// CFindDialog message handlers

BOOL CFindDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	CButton* pItem = (CButton*)GetDlgItem( IDC_RADIO_ITEM_ID );
	if ( pItem )
		pItem->SetCheck( 1 );

	SetCheck();
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CFindDialog::OnRadioItemID() 
{
	SetCheck();
}

void CFindDialog::OnRadioVariantID() 
{
	SetCheck();
}

void CFindDialog::OnOK() 
{
	UpdateData();
	if ( m_ItemCtrl.IsWindowEnabled() )
		bItem = true;
	else
		bItem = false;
	CDialog::OnOK();
}
