#pragma once
#include "afxwin.h"
#include "afxcmn.h"
#include "resource.h"

class CMETreeView;
namespace NWysiwyg
{
	struct SDBCamera;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCameraAnimDlg dialog
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCameraAnimDlg : public CDialog
{
	DECLARE_DYNAMIC(CCameraAnimDlg)

public:
	CCameraAnimDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CCameraAnimDlg();

// Dialog Data
	enum { IDD = IDD_CAMERA_ANIMATION };
	vector<NWysiwyg::SDBCamera> transitions;

	virtual BOOL PreTranslateMessage(MSG* pMsg);

protected:
	CMETreeView *m_pTree;
	int nLastSelected;

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
	CStatic m_treePlace;
	CListCtrl m_list;
	afx_msg void OnLvnEndlabeleditKeyframes(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedOk();
	afx_msg void OnNMDblclkKeyframes(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnKeydownKeyframes(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedGenerateScript();
};
////////////////////////////////////////////////////////////////////////////////////////////////////