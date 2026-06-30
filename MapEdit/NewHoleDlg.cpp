// NewHoleDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "NewHoleDlg.h"


// CNewHoleDlg dialog

IMPLEMENT_DYNAMIC(CNewHoleDlg, CDialog)
CNewHoleDlg::CNewHoleDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CNewHoleDlg::IDD, pParent)
	, m_nSegments(4)
	, m_fHeight(-1)
	, m_fRadius( 5 )
{
}

CNewHoleDlg::~CNewHoleDlg()
{
}

void CNewHoleDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_SEGMENTCOUNT, m_nSegments);
	DDV_MinMaxInt(pDX, m_nSegments, 0, 1000);
	DDX_Text(pDX, IDC_TERRHOLE_HEIGHT, m_fHeight);
	DDX_Text(pDX, IDC_TERRHOLE_RADIUS, m_fRadius);
	DDV_MinMaxFloat(pDX, m_fRadius, 0.1f, 1000);
}


BEGIN_MESSAGE_MAP(CNewHoleDlg, CDialog)
END_MESSAGE_MAP()


// CNewHoleDlg message handlers
