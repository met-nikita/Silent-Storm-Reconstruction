#if !defined(__PIECESINFOVIEW_H)
#define __PIECESINFOVIEW_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPiecesInfoView view

struct SPieceInfo
{
	bool juncs[6]; // x,y,z,-x,-y,-z
	bool bEditable;

	SPieceInfo(): bEditable(true)
	{
		memset( juncs, 0, sizeof( juncs ) );
	}
};

class CPiecesInfoView: public CWnd
{
	// Construction
public:
	CPiecesInfoView();

	// Attributes
public:
	string szAdditionalPices;
	string szOriginalPieces;

	// Operations
public:
	void SetAIGeometry( int nID );
	string GetAdditionalPieces( bool bOriginal = false );

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPiecesInfoView)
protected:
	//}}AFX_VIRTUAL

	// Implementation
public:
	virtual ~CPiecesInfoView();

private:
	int nAIGeometryID;
	CListCtrl	m_wndList;
	unordered_map<int, SPieceInfo> pieces;

	void EditRecord( NM_LISTVIEW *pNMListView );
	void AddRecord( int nHashID, const SPieceInfo &info );
	void UpdateItem( int iItem, int nHashID, const SPieceInfo &info );
	// Generated message map functions
protected:
	//{{AFX_MSG(CPiecesInfoView)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnClickParamlist(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblClickParamlist(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLvnKeydownList(NMHDR *pNMHDR, LRESULT *pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __PIECESINFOVIEW_H
