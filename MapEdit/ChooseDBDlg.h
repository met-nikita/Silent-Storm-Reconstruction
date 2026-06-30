#pragma once

// CChooseDBDlg dialog

class CChooseDBDlg : public CDialog
{
	DECLARE_DYNAMIC(CChooseDBDlg)

public:
	CChooseDBDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CChooseDBDlg();

// Dialog Data
	enum { IDD = IDD_CHOOSEDATABASE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CString m_szDBServer;
};
