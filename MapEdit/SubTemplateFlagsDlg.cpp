// SubTemplateFlagsDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "SubTemplateFlagsDlg.h"


// CSubTemplateFlagsDlg dialog

IMPLEMENT_DYNAMIC(CSubTemplateFlagsDlg, CDialog)
CSubTemplateFlagsDlg::CSubTemplateFlagsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CSubTemplateFlagsDlg::IDD, pParent)
{
}

CSubTemplateFlagsDlg::~CSubTemplateFlagsDlg()
{
}

void CSubTemplateFlagsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PARAMS_PLACE, m_ctrlPlace);
}


BEGIN_MESSAGE_MAP(CSubTemplateFlagsDlg, CDialog)
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CSubTemplateFlagsDlg::OnInitDialog()
{
	if ( !CDialog::OnInitDialog() )
		return FALSE;

	CRect r;
	m_ctrlPlace.GetWindowRect( &r );
	ScreenToClient( &r );
	if ( !m_paramsView.Create(NULL, "ParamsView", AFX_WS_DEFAULT_VIEW, r, this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create params view window\n");
		return -1;
	}
	m_paramsView.SetFlags( szFlags );

	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSubTemplateFlagsDlg message handlers
void CSubTemplateFlagsDlg::OnBnClickedOk()
{
	szFlags = m_paramsView.GetFlags();
	OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////