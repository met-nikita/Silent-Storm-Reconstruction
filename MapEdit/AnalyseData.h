#if !defined(AFX_ANALYSEDATA_H__705EB94E_3B6C_44D4_B8B7_CA6E9FF74DFC__INCLUDED_)
#define AFX_ANALYSEDATA_H__705EB94E_3B6C_44D4_B8B7_CA6E9FF74DFC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// AnalyseData.h : header file
//

////////////////////////////////////////////////////////////////////////////////////////////////////
// CAnalyseDataDlg dialog

class CAnalyseDataDlg : public CDialog
{
	void SrcExistence( string szPrefix, const SResTree *pResTree, int nFolder = -1 );
// Construction
public:
	CAnalyseDataDlg(CWnd* pParent = NULL);   // standard constructor

	void GeometriesAndAIGeometriesTest();
	void AnimSrcFilesExistence();
	void GeometrySrcFilesExistence();
	void AIGeometrySrcFilesExistence();
	void TextureSrcFilesExistence();
	void EffectsSrcFilesExistence();
	void MaterialTexsValidity();
	void ParticleTexsValidity();
	void UITexsTexsValidity();
	void AckSoundSrcFilesExistence();
	void TextureGarbageCheck();
	void SoundGarbageCheck();
// Dialog Data
	//{{AFX_DATA(CAnalyseDataDlg)
	enum { IDD = IDD_ANALYSE_DATA };
	CListBox	m_list;
	CString	m_log;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAnalyseDataDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CAnalyseDataDlg)
	afx_msg void OnAnalyse();
	virtual BOOL OnInitDialog();
	afx_msg void OnDblclkAnalyseprocList();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ANALYSEDATA_H__705EB94E_3B6C_44D4_B8B7_CA6E9FF74DFC__INCLUDED_)
