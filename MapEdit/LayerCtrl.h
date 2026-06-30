#if !defined(AFX_LAYERCTRL_H__9090D2DD_1911_4CB4_8FB0_344FC97F46A7__INCLUDED_)
#define AFX_LAYERCTRL_H__9090D2DD_1911_4CB4_8FB0_344FC97F46A7__INCLUDED_ 

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
/*! \file LayerCtrl.h
Описание контрола управления слоями CLayerList, а так же дополнительных компонентов, необходимых для его работы
*/

#include "TemplateView.h"
#include "..\Main\MELayers.h"

const UINT WM_USER_SELECT    = WM_USER + 1; //!< Сообщение, посылаемое когда пользователь выбирает новый слой. WPARAM - id слоя
const UINT WM_USER_LAYERUP   = WM_USER + 2; //!< Сообщение, посылаемое когда пользователь пытется переместить слой на уровень вверх. WPARAM - id слоя
const UINT WM_USER_LAYERDOWN = WM_USER + 3; //!< Сообщение, посылаемое когда пользователь пытется переместить слой на уровень вниз. WPARAM - id слоя
const UINT WM_USER_NOTIFY    = WM_USER + 4; //!< Сообщение, посылаемое когда произошли изменения в кофигурации слоев (очередность, видимость)
const UINT WM_USER_EXPORTTERR= WM_USER + 5; //!< необходимо проэкспортить все слои террейна
const UINT WM_USER_LINK      = WM_USER + 6;
const UINT WM_USER_FLOORLINK = WM_USER + 7;
const UINT LLN_ADD = 0x1; //!< Notify код, посылаемый через WPARAM, когда нажата кнопка Add
const UINT LLN_DEL = 0x2; //!< Notify код, посылаемый через WPARAM, когда нажата кнопка Del
const UINT LLN_TERR_CHANGED = 0x3; //!< Notify код, посылаемый через WPARAM, когда изменен ландшафт
const UINT LLN_SETUP = 0x4; //!< Notify код, посылаемый через WPARAM, когда слой необходимо заново инициализировать
const UINT LLN_PROPSRESET = 0x5; //!< необходимо сбросить список свойств
const UINT LLN_TERR_SERIALIZE = 0x6; //!<

HCURSOR CreateCursor( LPTSTR lpSysCursorName, UINT nExtraCursorID, float fAlpha = 0.0f );
class CPlacement;
////////////////////////////////////////////////////////////////////////////////////////////////////
inline COLORREF GetGrayedCr( COLORREF cr )
{
	DWORD mean = 0.333f * ( GetRValue( cr ) + GetGValue( cr ) + GetBValue( cr ) );
	return RGB( mean, mean, mean );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline COLORREF GetBrightCr( COLORREF cr, float fBrightness )
{
	return RGB( fBrightness * GetRValue( cr ), fBrightness * GetGValue( cr ), fBrightness * GetBValue( cr ) );
}
//! Кнопка выбора текущей активной кисти. Для кнопки можно задать ее цвет и имя
class CBrushButton : public CButton
{
public:
	CBrushButton();
	~CBrushButton();

	CString szBtnName;	//!< имя кнопки
	COLORREF crBtn;			//!< цвет кнопки
	CFont m_font;
	
	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBrushButton)
	public:
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	//}}AFX_VIRTUAL
	
	// Generated message map functions
	//{{AFX_MSG(CBrushButton)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()		
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLinkButton : public CButton
{
protected:
	CBitmap bmplink;
public:
	CLinkButton();
	~CLinkButton();

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLinkButton)
	public:
	//}}AFX_VIRTUAL
	
	// Generated message map functions
	//{{AFX_MSG(CLinkButton)
	afx_msg void OnPaint();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()		
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLayerCtrl dialog
class CLayerCtrl : public CDialog
{
	int  nLayerID;
	bool bActive;
	CFont m_font;
	CFont m_boldFont;
	CString szLayerName;
	bool bFirst; // определяет рисовать или нет каемку сверху\снизу
	bool bLast;
protected:
	void SetLayerID( int nID, int nLayerID );
	void SetLayerName( string szName ) { szLayerName = m_szName = szName.c_str(); }
	void Activate( bool bActivate = true );
	void Repaint() { if ( !::IsWindow(m_hWnd) ) return; CWnd *pPrnt = GetParent(); if ( pPrnt ) pPrnt->PostMessage( WM_USER_NOTIFY ); }
	void SendNotify( WPARAM wParam );
	void SetFirst() { bFirst = true; }
	void SetLast() { bLast = true; }
	void SetMiddle() { bFirst = false; bLast = false; }
	friend class CLayerList;
	// Construction
public:
	CLayerCtrl( int nLayerType, int nLayerInd, CString szName = "Layer name" );   // standard constructor
	virtual ~CLayerCtrl();

	int GetLayerID() const { return nLayerID; }
	int GetLayerType() const;
	void SetBrush( COLORREF cr, CString szName = "" );
	void SetVisible( bool bVisible = true );
	bool IsVisible() const;
	bool IsActive() const { return bActive; }
	string GetLayerName() const { return (LPCTSTR)szLayerName; }
	bool GetLink();
	void SetLink( bool bLink );

	virtual int  GetBrushID() const { return -1; }
	virtual void BrowseBrush() {}
	virtual void Reset() {}
	virtual bool CanDraw() const { return true; }
	virtual void Paint( ITemplateView *pView, float fBrightness = 1.0f, bool bGrayed = false ) {}
	virtual void Selection( const CRect &r, ITemplateView *pView ) {}
	virtual void SetPlacement( CPlacement *pPlacement ) {}
	virtual void ClearSelection() {}
	virtual void OnLButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView ) {}
	virtual void OnLButtonUp( UINT nFlags, CPoint pt, ITemplateView *pView ) {}
	virtual void OnLButtonDblClk( UINT nFlags, CPoint pt, ITemplateView *pView ) {}
	virtual void OnRButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView ) {}
	virtual void OnRButtonDblClk( UINT nFlags, CPoint pt, ITemplateView *pView ) {}
	virtual void OnMouseMove( UINT nFlags, CPoint pt, ITemplateView *pView ) {}
	virtual void OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, ITemplateView *pView ) {}
	virtual void OnTimer( ITemplateView *pView ) {}
	virtual bool OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message, ITemplateView *pView ) { return false; }
	virtual string GetExportDir() const { return ""; }
	virtual bool Export( const string &szExportDir, const string &szPrefix ) { return true; }
		
// Dialog Data
	//{{AFX_DATA(CLayerCtrl)
	enum { IDD = IDD_LAYER };
	CButton	m_linkCtrl;
	CStatic	m_Name;
	CBrushButton	m_btnBrush;
	CString	m_szName;
	BOOL	m_bVisible;
	BOOL	m_bLink;
	//}}AFX_DATA

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLayerCtrl)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLayerCtrl)
	afx_msg void OnBrushSelect();
	afx_msg void OnPaint();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	virtual BOOL OnInitDialog();
	virtual void OnCancel();
	virtual void OnOK();
	virtual afx_msg void OnVisible();
	afx_msg void OnLink();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LAYERCTRL_H__9090D2DD_1911_4CB4_8FB0_344FC97F46A7__INCLUDED_)
