// LadderDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "LadderDlg.h"
#include "..\Main\BuildingInfo.h"
#include "..\Input\Bind.h"


// CLadderDlg dialog

IMPLEMENT_DYNAMIC(CLadderDlg, CDialog)
CLadderDlg::CLadderDlg( NBuilding::SLadder *_pLadder, CWnd* pParent /*=NULL*/)
	: CDialog(CLadderDlg::IDD, pParent), m_nLadderSteps(0)
{
	pLadder = _pLadder;
}

CLadderDlg::~CLadderDlg()
{
}

void CLadderDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Text(pDX, IDC_LADDER_STEPS, m_nLadderSteps );
}


BEGIN_MESSAGE_MAP(CLadderDlg, CDialog)
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CLadderDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_nLadderSteps = pLadder->nHeight;
	UpdateData( FALSE );
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CLadderDlg::OnOK()
{
	UpdateData();

	if ( pLadder->nHeight != m_nLadderSteps )
	{
		pLadder->nHeight = m_nLadderSteps;
		NInput::PostEvent( "serialize_building" );
		NInput::PostEvent( "update_ladder" );
	}
	CDialog::OnOK();
}

// CLadderDlg message handlers
