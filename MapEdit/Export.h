#if !defined(AFX_EXPORT_H__84B99444_24A9_4B8B_970F_B27E0BD68BAE__INCLUDED_)
#define AFX_EXPORT_H__84B99444_24A9_4B8B_970F_B27E0BD68BAE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Export.h : header file
//

////////////////////////////////////////////////////////////////////////////////////////////////////
const char REG_EXPORT_SRC[] = "ExportSrcDir";
const char REG_EXPORT_DST[] = "ExportDstDir";
const char REG_EXPORT_GUI[] = "GUIExport";
string GetExportSrcDir();
string GetExportDstDir();
void CopyLastVersion();
////////////////////////////////////////////////////////////////////////////////////////////////////
string GetResString( UINT nID );

////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportGeometry( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport );
void GeometryUpdateDBData( CItemsMgr *pItems, const vector<int> &nItemIDs );
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportAIGeometry( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport );
void AIGeometryUpdateDBData( CItemsMgr *pItems, const vector<int> &nItemIDs );
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportTextures( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport );
void TextureUpdateDBData( CItemsMgr *pItems, const vector<int> &nItemIDs );
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportFonts( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport );
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportSkeletons( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport );
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportAnimations( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport );
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportParticles( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport );
void ParticlesUpdateDBData( CItemsMgr *pItems, const vector<int> &nItemIDs );
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportSounds( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport );
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportHeads( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport );
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportHeadSeqs( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport );
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportAckInfos( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport );
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportLights( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport );
////////////////////////////////////////////////////////////////////////////////////////////////////
void DoorsUpdateDBData( CItemsMgr *pItems, const vector<int> &nItemIDs );
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExportResultDlg dialog
class CExportResultDlg : public CDialog
{
// Construction
public:
	CExportResultDlg( const string &szLog, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CExportResultDlg)
	enum { IDD = IDD_EXPORT_RESULTS };
	CString	m_Log;
	CString m_LogCopy;
	BOOL	m_bHideWarnings;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CExportResultDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CExportResultDlg)
	afx_msg void OnExportHidewarn();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EXPORT_H__84B99444_24A9_4B8B_970F_B27E0BD68BAE__INCLUDED_)
