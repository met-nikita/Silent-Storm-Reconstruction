#if !defined(AFX_TERRAINLAYER_H__974130B5_2ACA_42E1_81D3_F21320E90BCD__INCLUDED_)
#define AFX_TERRAINLAYER_H__974130B5_2ACA_42E1_81D3_F21320E90BCD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TerrainLayer.h : header file
//

#include "2DEditor.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlacement;
struct STerrainHole;
class CTerrainBrushDlg;
class CTerrainHole;
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Слой рисования карты высот
class CHeightsLayer : public CLayerCtrl
{
	CMaskBrush activeBrush;
	float fCurrentH;
	bool  bEmboss;
	vector<CObj<CTerrainHole> > holes;
	CMenu m_popup;
	bool bDrag;
	int nRegInd;
	int nPtInd;
	CVec2 ptTempl;	// точка в темплейтных координатах, где кликнули правой кнопкой мыши
	CPlacement *pPlacement;
	
protected:
	C2DEditor editor;
	float fMinH;
	float fMaxH;
	string szExportDir;

	void  BrowseBrush( CTerrainBrushDlg *pDlg );
	COLORREF Height2Color( float height );
	float Color2Height( COLORREF cr );
	virtual void  SetCurrentH( float fH );
	float GetCurrentH() const { return fCurrentH; }
	virtual void  Draw( const CPoint &pt, ITemplateView *pView );
	const CMaskBrush& GetBrush() const { return activeBrush; }
	void  PutRegion( const CVec2 &pt, int nRadius );
	void  PaintRegion( CDC *pDC, ITemplateView *pView, const CTerrainHole *pHole );
	bool  RegionHitTest( ITemplateView *pView, const CPoint &pt, int *pRegInd, int *pPtInd );
	bool  RegionHitTest( const CVec2 &pt, int *pRegInd, int *pPtInd );

public:
	CHeightsLayer( int nLayerType, const string &szName );
	~CHeightsLayer();

	virtual void BrowseBrush();
	virtual void Paint( ITemplateView *pView, float fBrightness, bool bGrayed );
	virtual void OnLButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView );
	virtual void OnLButtonUp( UINT nFlags, CPoint pt, ITemplateView *pView );
	virtual void OnRButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView );
	virtual void OnMouseMove(UINT nFlags, CPoint pt, ITemplateView *pView );
	virtual string GetExportDir() const { return szExportDir; }
	virtual bool Export( const string &szExportDir, const string &szPrefix );


	void SetPlacement( CPlacement *pPlacement );
	void CreateImage( int nWidth, int nHeight, COLORREF cr );
	void SetImage( const CArray2D<WORD> *pHeights, float fMinH, float fMaxH );
	void SetHoles( const vector<STerrainHole> *pHoles );
	void GetTerrain( float *pMinH, float *pMaxH, CArray2D<WORD> *pHeights );
	void GetHoles( vector<STerrainHole> *pHoles );

protected:
	//{{AFX_MSG(CHeightsLayer)
	afx_msg void OnExport();
	afx_msg void OnImport();
	afx_msg void OnNewContour();
	afx_msg void OnDelContour();
	afx_msg void OnHoleProps();
	//}}AFX_MSG
	virtual afx_msg void OnVisible();
	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Слой рисования альфа значений
class CAlphaLayer : public CHeightsLayer
{
public:
	CAlphaLayer();
	virtual void BrowseBrush();

	void SetImage( const CArray2D<WORD> *pAlphas );
	void GetAlpha( CArray2D<WORD> *pAlpha );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Слой рисования травы
class CGrassLayer : public CHeightsLayer
{
private:
	int nGrassID;
	float fMaxDensity;
	int nGrassLayerInd;

	string GetGrassName();
protected:
	virtual void  SetCurrentH( float fH );
public:
	CGrassLayer( int nLayerCnt );
	virtual void BrowseBrush();

	void SetLayerInd( int nLayerInd );
	void SetImage( const CArray2D<BYTE> *pGrass, float fMaxDensity );
	void SetGrassID( int nGrassId );


	void GetGrass( CArray2D<BYTE> *pGrass );
	int  GetGrassID() const { return nGrassID; }
	float GetMaxDensity() const { return fMaxDensity; }
	int  GetGrassLayerInd() const { return nGrassLayerInd; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Слой рисования травы
class CColorLayer : public CHeightsLayer
{
	COLORREF color;
	CMaskBrush crBrush;
	unordered_map<int, int> brushID2Width;
	unordered_map<int, int> brushWidth2ID;
protected:
	virtual void Draw( const CPoint &pt, ITemplateView *pView );
	virtual afx_msg void OnVisible();
public:
	CColorLayer( COLORREF cr, float fScale, int nLayerID, int nLayerCnt, const CString &name = "" );
	virtual void BrowseBrush();

	void SetImage( const CArray2D<DWORD> *pColormap );

	void GetColormap( CArray2D<DWORD> *pColormap );
};
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTerrainBrushDlg dialog
class CTerrainBrushDlg : public CDialog
{
	float fMaxHeight;
// Construction
public:
	CTerrainBrushDlg( CWnd* pParent = NULL, int nLayerID = LID_HEIGHTS, float fMaxHeight = -1 );   // standard constructor

	bool bEmboss;
	int  nGrassID;
// Dialog Data
	//{{AFX_DATA(CTerrainBrushDlg)
	enum { IDD = IDD_HEIGHTS_LAYER };
	CEdit	m_minCtrl;
	CEdit	m_maxCtrl;
	CButton	m_heightGroup;
	CSliderCtrl	m_slider;
	float	m_fMax;
	float	m_fMin;
	float	m_fCurrent;
	int		m_nBrushRadius;
	CString	m_szCurrent;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTerrainBrushDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	int nLayerID;

	// Generated message map functions
	//{{AFX_MSG(CTerrainBrushDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnChangeCurrent();
	afx_msg void OnGrassType();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// CHolePropsDlg dialog

class CHolePropsDlg : public CDialog
{
// Construction
public:
	CHolePropsDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CHolePropsDlg)
	enum { IDD = IDD_HOLE_PROPS };
	float	m_fHeight;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CHolePropsDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CHolePropsDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TERRAINLAYER_H__974130B5_2ACA_42E1_81D3_F21320E90BCD__INCLUDED_)
