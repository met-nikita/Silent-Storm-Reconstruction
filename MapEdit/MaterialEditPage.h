#if !defined(AFX_MATERIALEDITPAGE_H__18C09EDE_A87F_447F_8E12_40B0C9F75F00__INCLUDED_)
#define AFX_MATERIALEDITPAGE_H__18C09EDE_A87F_447F_8E12_40B0C9F75F00__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MaterialEditPage.h : header file
//
enum EMaterialSet;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMaterialEditPage dialog
class CMaterialEditPage : public CDialog
{
	DECLARE_DYNCREATE(CMaterialEditPage)

// Construction
public:
	CMaterialEditPage();
	~CMaterialEditPage();

	void SetNames();

// Dialog Data
	//{{AFX_DATA(CMaterialEditPage)
	enum { IDD = IDD_MATERIAL_EDIT };
	CButton	m_material;
	CButton	m_materialSet;
	int		m_nRotation;
	float	m_fScale;
	float	m_fShiftX;
	float	m_fShiftY;
	CString	m_szMaterial;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CMaterialEditPage)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	int nRPGArmorID;
	int nMaterialID;
	void GetValues();
	void SetCheck( int nControlID );
	void SetName( EMaterialSet set, CButton &btn );
	// Generated message map functions
	//{{AFX_MSG(CMaterialEditPage)
	afx_msg void OnMaterialAppearance();
	afx_msg void OnSetfocusMaterialArmor();
	afx_msg void OnMaterialSet();
	afx_msg void OnMaterial();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

public:
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedRadio1();
	afx_msg void OnBnClickedRadio2();
	afx_msg void OnBnClickedRadio3();
	afx_msg void OnBnClickedRadio4();
	afx_msg void OnBnClickedMaterialSet2();
	afx_msg void OnBnClickedMaterialSet3();
	CButton m_materialSet2;
	CButton m_materialSet3;
	afx_msg void OnBnClickedRadio5();
	afx_msg void OnBnClickedRadio6();
	afx_msg void OnBnClickedMaterialSet4();
	afx_msg void OnBnClickedMaterialSet5();
	CButton m_materialSet4;
	CButton m_materialSet5;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MATERIALEDITPAGE_H__18C09EDE_A87F_447F_8E12_40B0C9F75F00__INCLUDED_)
