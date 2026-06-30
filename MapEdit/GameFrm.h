// GameFrm.h : interface of the CChildFrame class
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __GAMEFRM_H__
#define __GAMEFRM_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "GameView.h"

class CGameFrame : public SECWorksheet
{
  DECLARE_DYNCREATE(CGameFrame)
public:
  CGameFrame();
  
  // Attributes
public:
  void SetCurPlacement( int nPlacementID );
	void DrawFrame();

  // Operations
public:
  
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CChildFrame)
public:
  virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
  virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	virtual BOOL PreTranslateMessage(	MSG *pMsg );
  //}}AFX_VIRTUAL
  
  // Implementation
public:
  // view for the client area of the frame.
  CGameView m_wndView;
  virtual ~CGameFrame();
#ifdef _DEBUG
  virtual void AssertValid() const;
  virtual void Dump(CDumpContext& dc) const;
#endif
  
  
  
  // Generated message map functions
protected:
  int nCurPlacementID;
  //{{AFX_MSG(CGameFrame)
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnMDIActivate(BOOL bActivate, CWnd* pActivateWnd, CWnd* pDeactivateWnd);
	afx_msg void OnClose();
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
};

#endif // __GAMEFRM_H__
