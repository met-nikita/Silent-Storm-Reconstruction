#if !defined(AFX_PARAMSVIEW_H__2D06DC8C_9D04_44DF_A190_9026105A13AA__INCLUDED_)
#define AFX_PARAMSVIEW_H__2D06DC8C_9D04_44DF_A190_9026105A13AA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ParamsView.h : header file
//

////////////////////////////////////////////////////////////////////////////////////////////////////
// CParamsView view
class CAttributesList;

class CParamsView : public CWnd
{
	// Construction
public:
	CParamsView();
	
	// Attributes
public:
	
	// Operations
public:
	void SetActiveItem( int nTreeID, int nItemID );
	void SetFlags( const string &szFlags );
	string GetFlags();
	
	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CParamsView)
protected:
	//}}AFX_VIRTUAL
	
	// Implementation
public:
	virtual ~CParamsView();
	
private:
	CListCtrl	m_wndList;
	CAttributesList *const pAttrList;
	struct SActiveItem
	{
		int nTreeID;
		int nItemID;
		SActiveItem() : nTreeID(0), nItemID(0) {}
	};
	SActiveItem aItem;

	void UpdateAttributes();
	void AddVariantFlags( int nVarID, const unordered_map<int, bool> &flags ); // <AttributeID, value>
	void ToggleFlag( NM_LISTVIEW* pNMListView );
	
	// Generated message map functions
protected:
	//{{AFX_MSG(CParamsView)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnClickParamlist(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblClickParamlist(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PARAMSVIEW_H__2D06DC8C_9D04_44DF_A190_9026105A13AA__INCLUDED_)
