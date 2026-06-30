#pragma once
#include "afxwin.h"
#include "afxcmn.h"


// CStoreItemsDlg dialog
class CMETreeView;
class CStoreItemsDlg : public CDialog
{
	DECLARE_DYNAMIC(CStoreItemsDlg)

public:
	CStoreItemsDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CStoreItemsDlg();

// Dialog Data
	enum { IDD = IDD_STORE_ITEMS };

protected:
	int nSide;
	SResTree rpgitems;
	CMETreeView *m_pTree;

	void SetSide( int nSide );
	void InsertItem( int nRecordID, int nItemID, int nRating, int nQuantity );
	void ChangeItemQuantity( int nDelta, int iItem = -1, bool bCanDelete = true );
	void DropTreeItem();
	void Sort();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CStatic m_treePlace;
	CListCtrl m_list;

	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	afx_msg void OnBnClickedAddButton();
	afx_msg void OnBnClickedRemoveButton();
	afx_msg void OnNMDblclkStoreList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedAllies();
	afx_msg void OnBnClickedAxis();
	afx_msg void OnLvnEndlabeleditStoreList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnKeydownStoreList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnItemchangedStoreList(NMHDR *pNMHDR, LRESULT *pResult);
};
