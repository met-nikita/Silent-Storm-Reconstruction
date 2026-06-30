#pragma once
#include "afxwin.h"
#include "afxcmn.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SResTree;
class CMETreeView;
// CItemsListDlg dialog
////////////////////////////////////////////////////////////////////////////////////////////////////
class CItemsListDlg : public CDialog
{
	DECLARE_DYNAMIC(CItemsListDlg)

public:
	CItemsListDlg() : m_pTree(0) {}
	CItemsListDlg( int nRPGPersID, int nItemsTree, const string &szItems4PersTbl, CWnd* pParent = NULL);
	virtual ~CItemsListDlg();

// Dialog Data
	enum { IDD = IDD_RPGPERS_ITEMS };

	static BOOL RegisterControlClass();

	void SetRPGPersID( int nPersID );

protected:
	int nRPGPersID;
	const SResTree *pItems;
	const string szItems4PersTbl;
	CMETreeView *m_pTree;
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	virtual void PostNcDestroy();
	virtual void OnCancel();
	static LRESULT CALLBACK EXPORT WndProcHook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
public:
	CStatic m_ctrlItemTreePlace;
	CListCtrl m_ctrlItems;
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedAddItem();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnLvnBeginlabeleditItemsList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnEndlabeleditItemsList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnDeleteitemItemsList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnKeydownItemsList(NMHDR *pNMHDR, LRESULT *pResult);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGItemsListPropPage dialog
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGItemsListPropPage : public CPropertyPage
{
	DECLARE_DYNAMIC(CRPGItemsListPropPage)

public:
	CRPGItemsListPropPage( UINT nIDCaption, int nRPGPersID, int nItemsTree, const string &szItems4PersTbl );
	virtual ~CRPGItemsListPropPage();

	void SetRPGPersID( int nPersID ) { m_list.SetRPGPersID( nPersID ); }
// Dialog Data
	enum { IDD = IDD_PROPPAGE_PERSITEMS };

	virtual BOOL OnInitDialog();

protected:
	CStatic m_place;
	CItemsListDlg m_list;
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
