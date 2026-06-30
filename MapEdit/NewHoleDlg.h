#pragma once


// CNewHoleDlg dialog

class CNewHoleDlg : public CDialog
{
	DECLARE_DYNAMIC(CNewHoleDlg)

public:
	CNewHoleDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CNewHoleDlg();

// Dialog Data
	enum { IDD = IDD_NEW_REGION };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	int m_nSegments;
	float m_fHeight;
	float m_fRadius;
};
