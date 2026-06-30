#ifndef __RECTSLAYER_H_
#define __RECTSLAYER_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

enum EMapObjType;
class CPlacement;
class CMERectTracker;
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Слой в котором расставляются прямоугольники темплейтов
class CRectsLayer : public CLayerCtrl
{
  struct STemplTrack
  {
    CMERectTracker *pTrack;
		EMapObjType objType;
		float fDZ;
    int objID;
  };
  struct SActvObj
  {
		EMapObjType type;
    int id;			// id объекта
    int ind;		// индекс в track списке (для type == MO_TEMPLATE)
  };
  CPlacement *pPlacement;
	CPlacementCache *pPlacementCache;
	vector<STemplTrack> trackRects;
	vector<STemplTrack> internalRects; // подтемплейты
  vector<SActvObj>  actvObjs;
	COLORREF crFont;
	CMERectTracker *pCurTracker;
	CRect		curTrackRect;				// трэкаемый bound прямоугольник
	CPoint	curTrackDCenter;		// вектор между центрами трэкаемого прямоугольника и общим центром группы
  CMenu		m_popup;
	HCURSOR hLayerCursor;
	CBitmap bmpMenu;
	CVec2   ptLastRClick;
	int     nLastFloor;
	bool    bDirty;

	CPlacement* GetFirstPlacement( int nTemplateID );
  void DeleteRects();
	void SetupInternalRects( ITemplateView *pView, CPlacement *pPl, const CVec2 &ptCenter, int nRotation, int nDepth );
	bool SetActiveObj( EMapObjType type, int id );
  bool AddActiveObj( EMapObjType type, int id );	
	void DeactivateObjs();
	void DeactivateObj( int id );
	void Rotate( CPlacement *pPl, EMapObjType objType, int objID, int ang, ITemplateView *pView );
	void GetCurTrackRect( CRect *pRect );
	void SelectedItem( ITemplateView *pView, EMapObjType type, int nItemID );
	int  AddObject( int nTableID );
	void AddTracker( ITemplateView *pView, EMapObjType type, int nObjID, const CVec3 &ptCenter, const CVec2 &ptSize, int nRotation );

public:
	CRectsLayer( int nLayerType, const string &szName );
	~CRectsLayer();
	virtual void Paint( ITemplateView *pView, float fBrightness, bool bGrayed );
	virtual void OnLButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView );
	virtual void OnLButtonDblClk( UINT nFlags, CPoint pt, ITemplateView *pView );
	virtual void OnRButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView );
	virtual void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags, ITemplateView *pView );
	virtual void OnTimer( ITemplateView *pView );
	virtual bool OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message, ITemplateView *pView );
	virtual void ClearSelection();
	virtual void Reset();
	
	void SetPlacement( CPlacement *pPlacement, CPlacementCache *pCache, ITemplateView *pView );
	void MoveRects( const CPoint &pt );
  void GetNearestPos( CPoint *ppt, ITemplateView *pView );
	int  DropObject( EMapObjType type, int nObjID, ITemplateView *pView, CVec3 pt = CVec3( -1, -1, -1 ), int nRotation = 0 );
	bool ChangeObject( int nTreeID, int nObjectID, int nNewRelation, ITemplateView *pView );
	void SetupRects( ITemplateView *pView, bool bKeepActiveObjs = false );
	
protected:
	//{{AFX_MSG(CRectsLayer)
	afx_msg void OnDelObject();
	afx_msg void OnAddTemplate();
	afx_msg void OnAddExplosion();
	afx_msg void OnAddUnit();
	afx_msg void OnAddModel();
	afx_msg void OnAddContainer();
	//}}AFX_MSG
	virtual afx_msg void OnVisible();
	DECLARE_MESSAGE_MAP()		
};
////////////////////////////////////////////////////////////////////////////////////////////////////
EMapObjType TreeID2MapObjType( int nTreeID );
int MapObjType2TreeID( EMapObjType type );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __RECTSLAYER_H_
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExplosionDlg dialog

class CExplosionDlg : public CDialog
{
// Construction
public:
	CExplosionDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CExplosionDlg)
	enum { IDD = IDD_EXPLOSION };
	float	m_fPower;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CExplosionDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CExplosionDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
