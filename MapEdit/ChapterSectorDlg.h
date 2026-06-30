#ifndef __CHAPTERSECTORDLG_H_
#define __CHAPTERSECTORDLG_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CItemsMgr;
enum EChapterSectorType;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterSectorDlg dialog
class CChapterSectorDlg : public CDialog
{
// Construction
public:
	CChapterSectorDlg( bool bGlobalSector = false, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CMaterialSetEditDlg)
	enum { IDD = IDD_CHAPTERSECTOR_DIALOG };
	int m_nProbability;
	CButton	m_btnTemplate;
	CButton	m_btnDescr;
	CButton	m_btnImg;
	EChapterSectorType eSectorType;
	//}}AFX_DATA
	int m_nTemplteID;
	int m_nDescrID;
	int m_nImageID;
	int m_nX;
	int m_nY;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMaterialSetEditDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	//}}AFX_VIRTUAL

// Implementation
protected:
	bool bGlobalSector;
	CItemsMgr *pTemplates;
	CItemsMgr *pChapters;
	CItemsMgr *pStrings;
	CItemsMgr *pImages;
	CItemsMgr *pScenarioZones;

	// Generated message map functions
	//{{AFX_MSG(CChapterSectorDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	afx_msg void OnTemplateSelect();
	afx_msg void OnDescrSelect();
	afx_msg void OnImageSelect();
	void UpdateSectorType( bool bSave );
	DECLARE_MESSAGE_MAP()
public:
	CString m_szID;
	CEdit m_ctrlStrID;
	CEdit m_ctrlProbability;
	CButton m_ctrlRandom;
	CButton m_ctrlZone;
	CButton m_ctrlExitZone;
	CEdit m_ctrlX;
	CEdit m_ctrlY;
	CString m_szTemplateCaption;
	afx_msg void OnBnClickedRandomSector();
	afx_msg void OnBnClickedZone();
	afx_msg void OnBnClickedExitzone();
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // __CHAPTERSECTORDLG_H_
