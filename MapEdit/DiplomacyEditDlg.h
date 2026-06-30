#pragma once
#include "afxwin.h"
#include "DiplomacyView.h"


// CDiplomacyEditDlg dialog
class CMETreeView;
class CDiplomacyEditDlg : public CDialog
{
	DECLARE_DYNAMIC(CDiplomacyEditDlg)

public:
	CDiplomacyEditDlg(int nPrevSelected = -1, CWnd* pParent = NULL);   // standard constructor
	virtual ~CDiplomacyEditDlg();

// Dialog Data
	enum { IDD = IDD_DIPLOMACY_EDIT };
	void SetItem( int _nItemID ) { nItemID = _nItemID; }
	int  GetSelectedID() { return nItemID; }

	BOOL PreTranslateMessage(MSG* pMsg);

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	int nItemID;
	int nPrevSelected;
	CMETreeView *m_pTree;
	CDiplomacyView m_view;
	DECLARE_MESSAGE_MAP()
	CStatic m_ctrlPlace;
	CStatic m_treeplace;
public:
	afx_msg void OnOK();
	afx_msg void OnRelEmpty();
};
