#ifndef __ROOMSLAYER_H_
#define __ROOMSLAYER_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "FloorLayer.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRoomsLayer : public CFloorsLayer
{
public:
	CRoomsLayer();
  ~CRoomsLayer();	

	virtual void BrowseBrush();
	virtual void Reset();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRoomListBox : public CListBox
{
	CPlacement *pPlacement;
public:
	CRoomListBox() : pPlacement( 0 ) {}
	
	void SetPlacement( CPlacement *pPl ) { pPlacement = pPl; };
	virtual void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct );
	virtual int CompareItem( LPCOMPAREITEMSTRUCT lpCompareItemStruct );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRoomDlg dialog

class CRoomDlg : public CDialog
{
	CPlacement *pPlacement;
// Construction
public:
	CRoomDlg( CPlacement *pPlacement, CWnd* pParent = NULL);   // standard constructor

	int nSelectedRoomID;
// Dialog Data
	//{{AFX_DATA(CRoomDlg)
	enum { IDD = IDD_ROOM };
	CRoomListBox	m_list;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRoomDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CRoomDlg)
	afx_msg void OnNewRoom();
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnDblclkRoomList();
	afx_msg void OnRoomColor();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __ROOMSLAYER_H_
