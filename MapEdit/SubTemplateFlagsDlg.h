#pragma once
#include "afxwin.h"
#include "ParamsView.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// CSubTemplateFlagsDlg dialog
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSubTemplateFlagsDlg : public CDialog
{
	DECLARE_DYNAMIC(CSubTemplateFlagsDlg)

public:
	CSubTemplateFlagsDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSubTemplateFlagsDlg();

// Dialog Data
	enum { IDD = IDD_SUBTEMPLATE_FLAGS };

	string szFlags;

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
private:
	CStatic m_ctrlPlace;
	CParamsView m_paramsView;
public:
	afx_msg void OnBnClickedOk();
};
////////////////////////////////////////////////////////////////////////////////////////////////////