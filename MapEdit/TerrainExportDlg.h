#if !defined(AFX_TERRAINEXPORTDLG_H__CE5542DD_6115_465C_AEDF_8F4C2931F795__INCLUDED_)
#define AFX_TERRAINEXPORTDLG_H__CE5542DD_6115_465C_AEDF_8F4C2931F795__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TerrainExportDlg.h : header file
//

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTerrainExportDlg dialog

class CTerrainExportDlg : public CDialog
{
// Construction
public:
	CTerrainExportDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CTerrainExportDlg)
	enum { IDD = IDD_TERRAIN_EXPORT };
	CString	m_szDirectory;
	BOOL	m_bExportAll;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTerrainExportDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	SECBrowseDirEdit	m_directory;

	// Generated message map functions
	//{{AFX_MSG(CTerrainExportDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TERRAINEXPORTDLG_H__CE5542DD_6115_465C_AEDF_8F4C2931F795__INCLUDED_)
