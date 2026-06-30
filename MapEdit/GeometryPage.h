#if !defined(AFX_GEOMETRYPAGE_H__F4B09979_9F6B_454E_94AE_C7208F1A9B20__INCLUDED_)
#define AFX_GEOMETRYPAGE_H__F4B09979_9F6B_454E_94AE_C7208F1A9B20__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GeometryPage.h : header file
//

////////////////////////////////////////////////////////////////////////////////////////////////////
// CGeometryPage dialog

class CGeometryPage : public CDialog
{
	DECLARE_DYNCREATE(CGeometryPage)

// Construction
public:
	CGeometryPage();
	~CGeometryPage();

// Dialog Data
	//{{AFX_DATA(CGeometryPage)
	enum { IDD = IDD_GEOMETRY_EDIT };
	CString	m_szGeometry;
	CString	m_szGeomRotation;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CGeometryPage)
	public:
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	int nConstructionPartID;
	// Generated message map functions
	//{{AFX_MSG(CGeometryPage)
	afx_msg void OnGeometryAppearance();
	afx_msg void OnCloseupGeometryRotation();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GEOMETRYPAGE_H__F4B09979_9F6B_454E_94AE_C7208F1A9B20__INCLUDED_)
