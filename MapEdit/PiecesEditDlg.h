#pragma once
#include "afxwin.h"
#include "PiecesInfoView.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// CPiecesEditDlg dialog
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPiecesEditDlg : public CDialog
{
	DECLARE_DYNAMIC(CPiecesEditDlg)

public:
	CPiecesEditDlg( int nAIGeomID, CWnd* pParent = NULL );   // standard constructor
	virtual ~CPiecesEditDlg();

	string szData;
// Dialog Data
	enum { IDD = IDD_GEOMETRYPIECES_EDIT };

protected:
	int nAIGeometryID;
	CPiecesInfoView m_infoView;
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
	CStatic m_ctrlInfoPlace;
	afx_msg void OnBnClickedOk();
	CString m_szOriginalPieces;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
