#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////////
// CNewSectorDlg dialog
class CNewSectorDlg : public CDialog
{
	DECLARE_DYNAMIC(CNewSectorDlg)

public:
	CNewSectorDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CNewSectorDlg();

// Dialog Data
	enum { IDD = IDD_NEWSECTOR };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	int m_nSegments;
};
////////////////////////////////////////////////////////////////////////////////////////////////////