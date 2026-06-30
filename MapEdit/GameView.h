// GameView.h : interface of the CGameView class
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __CHILDVIEW_H__
#define __CHILDVIEW_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define LIGHT_SECTION		"Light"
#define CAMERA_SECTION	"Camera"

////////////////////////////////////////////////////////////////////////////////////////////////////
// CGameView window

class CGameView : public CWnd
{
// Construction
public:
	CGameView();

// Attributes
public:

// Operations
public:
  void SetRootPlacement( int nPlacementID );
  void SetModel( int nPlacementID, int nModelID );
	void SetRPGItem( int nRPGItemID );
	void SetRndModel( int nModelID );		
  void SetTexture( int nTextureID );
  void SetGeometry( int nGeometryID );
  void SetMaterial( int nMaterialID );
	void SetAnimation( int nGeometryID, int nAnimationID );
	void SetParticle( int nParticleID );
	void SetContainer( int nContainerID );
	void SetSoundEffect( int nID );
	void SetAIModel( int nAIModelID );
	void SetSound( int nSoundID );
	void SetInterface( int nUIContainerID );
	void SetHead( int nHeadID );
	void SetPers( int nPersID );
	void SetObject( int nID, int nVarID );
	void SetConstructionPart( int nPartVarID );
  int  GetCurTemplate() { return nLastPlacement;  }
	void Activate();
	void RunMap( int nMapID );
	
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGameView)
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL PreTranslateMessage(	MSG *pMsg );
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CGameView();

	// Generated message map functions
protected:
  int nLastPlacement;
	UINT nMenuTimer;
	UINT nTerrSpotTimer;
	CMenu wysiwygMenu;
	CPoint ptRBDown;
	bool bWysiwyg;
  bool SetGameWnd( CWnd *pWnd );

	//{{AFX_MSG(CGameView)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnDestroy();
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnNewTexspot();
	afx_msg void OnNewTerrspot();
	afx_msg void OnNewWaypoint();
	afx_msg void OnNewObject();
	afx_msg void OnNewLadder();
	afx_msg void OnUpdateViewWysiwyg(CCmdUI* pCmdUI);
	afx_msg void OnViewWysiwyg();
	afx_msg void OnSpotFragments();
	afx_msg void OnNewTemplateFromSel();
	afx_msg void OnSaveCamera();
	afx_msg void OnRestoreCamera();
	afx_msg void OnViewRoute();
	afx_msg void OnSetRoute();
	afx_msg void OnAnimateCamera();
	afx_msg void OnLockSelection();
	afx_msg void OnEditScript();
	afx_msg void OnUpdateLockSelection(CCmdUI* pCmdUI);
	afx_msg void OnUpdateEditScript(CCmdUI* pCmdUI);
	afx_msg void OnViewBrowser();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////
void SetupSysColors();
void RestoreSysColors();
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // __CHILDVIEW_H__
