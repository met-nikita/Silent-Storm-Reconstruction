#ifndef __DIPLOMACYVIEW_H_
#define __DIPLOMACYVIEW_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//

class CItemsMgr;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDiplomacyView view
class CDiplomacyView : public CWnd
{
	// Construction
public:
	CDiplomacyView();

	// Attributes
public:

	// Operations
public:
	void SetActiveDiplomacy( int nItemID );

	// Implementation
public:
	virtual ~CDiplomacyView();

private:
	CListCtrl	m_wndList;
	int nActiveDiplomacyID;
	CItemsMgr *pItems;

	void ToggleFlag( NM_LISTVIEW* pNMListView );
	void SetPlayerDiplomacy( int nPlayer, DWORD nDiplomacy );
	void SavePlayerDiplomacy( int nPlayer );

	// Generated message map functions
protected:
	//{{AFX_MSG(CDiplomacyView)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnClickParamlist(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblClickParamlist(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DIPLOMACYVIEW_H_
