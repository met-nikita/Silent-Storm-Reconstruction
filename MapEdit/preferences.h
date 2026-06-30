#include "afxwin.h"
#if !defined(AFX_PREFERENCES_H__74961EE6_C4E2_4058_84CE_A8A719038CC0__INCLUDED_)
#define AFX_PREFERENCES_H__74961EE6_C4E2_4058_84CE_A8A719038CC0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// preferences.h : header file
//

const char REG_MAPBUILD_ARG[] = "MapbuildArgs";
const char REG_SHOW2DVIEW[] = "Show2DView";
const char REG_USEDXT[] = "UseDXT";
const char REG_INTERFACEPREVIEW[] = "iPreview";
const char REG_AUTOSELANIMMODEL[] = "AutoSelAnimModel";
const char REG_AUTOSELANIMITEMS[] = "AutoSelAnimItems";
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExportPrefsDlg dialog

class CExportPrefsDlg : public CPropertyPage
{
	DECLARE_DYNCREATE(CExportPrefsDlg)

// Construction
public:
	CExportPrefsDlg();
	~CExportPrefsDlg();

// Dialog Data
	//{{AFX_DATA(CExportPrefsDlg)
	enum { IDD = IDD_EXPORT_PREFS };
	CString	m_src;
	CString	m_dst;
	float	m_fCameraSpeed;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CExportPrefsDlg)
	public:
	virtual void OnOK();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	SECBrowseDirEdit m_srcCtrl;
	SECBrowseDirEdit m_dstCtrl;
	
	// Generated message map functions
	//{{AFX_MSG(CExportPrefsDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

public:
	BOOL m_bGUIMode;
	BOOL m_bUseDXT;
	BOOL m_bAutoSelAnimModel;
	BOOL m_bAutoSelAnimItems;
	CString m_szDBServer;
	BOOL m_bObjectsShowGround;
	BOOL m_bAnimShowFlags;
	BOOL m_bShowObjBrowser;
};

class CColorBtn : public CButton
{
public:
	COLORREF cr;
	void DrawItem(LPDRAWITEMSTRUCT);
protected:
	//{{AFX_MSG(CColorBtn)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLightPrefsDlg dialog

class CLightPrefsDlg : public CPropertyPage
{
	DECLARE_DYNCREATE(CLightPrefsDlg)

// Construction
public:
	CLightPrefsDlg();
	~CLightPrefsDlg();

// Dialog Data
	//{{AFX_DATA(CLightPrefsDlg)
	enum { IDD = IDD_LIGHT_PREFS };
	CColorBtn	m_vapourColor;
	CColorBtn	m_glossColor;
	CColorBtn	m_fogColor;
	CColorBtn	m_ambientColor;
	CColorBtn	m_lightColor;
	float	m_dirX;
	float	m_dirY;
	float	m_dirZ;
	float	m_orgX;
	float	m_orgY;
	float	m_orgZ;
	float	m_fFogDistance;
	float	m_fVapourDensity;
	float	m_fVapourHeight;
	float	m_fFogStart;
	float m_fVNP;
	float m_fVS;
	float m_fVST;
	//}}AFX_DATA
	COLORREF m_crAmbient;
	COLORREF m_crLight;
	COLORREF m_crFog;
	COLORREF m_crGloss;
	COLORREF m_crVapour;
	COLORREF m_crShadow;
	COLORREF m_crBack;
	COLORREF m_crGroundAmbient;
	int m_nSkyID;


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CLightPrefsDlg)
	public:
	virtual void OnOK();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CLightPrefsDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnAmbientColor();
	afx_msg void OnLightColor();
	afx_msg void OnFogColor();
	afx_msg void OnLightLoadpreset();
	afx_msg void OnLightSave();
	afx_msg void OnVapourColor();
	afx_msg void OnGlossColor();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

public:
	BOOL m_bSwitchSysColors;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTemplateViewPrefsDlg dialog

class CTemplateViewPrefsDlg : public CPropertyPage
{
	DECLARE_DYNCREATE(CTemplateViewPrefsDlg)

// Construction
public:
	CTemplateViewPrefsDlg();
	~CTemplateViewPrefsDlg();

// Dialog Data
	//{{AFX_DATA(CTemplateViewPrefsDlg)
	enum { IDD = IDD_TEMPLATEVIEW_PREFS };
	int		m_nDepth;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CTemplateViewPrefsDlg)
	public:
	virtual void OnOK();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CTemplateViewPrefsDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMapbuildPrefsDlg dialog

class CMapbuildPrefsDlg : public CPropertyPage
{
// Construction
public:
	CMapbuildPrefsDlg();   // standard constructor

// Dialog Data
	//{{AFX_DATA(CMapbuildPrefsDlg)
	enum { IDD = IDD_MAPBUILD_PREFS };
	CString	m_szArguments;
	int		m_nDepth;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMapbuildPrefsDlg)
	public:
	virtual void OnOK();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CMapbuildPrefsDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	BOOL m_bShow2DView;
	float m_fSubTemplateAlpha;
	BOOL m_bShowAISpheres;
	BOOL m_bInstantTerrain;
	int m_nGridInterval;
	CComboBox m_ctrlGridInterval;
	BOOL m_bGridSnap;
	float m_fKbdMovingSpeed;
	BOOL m_bShowHoles;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUILayersDlg dialog
class CUILayersDlg : public CPropertyPage
{
// Construction
public:
	CUILayersDlg();   // standard constructor

// Dialog Data
	//{{AFX_DATA(CUILayersDlg)
	enum { IDD = IDD_UILAYERS };
	BOOL	m_0;
	BOOL	m_1;
	BOOL	m_2;
	BOOL	m_3;
	BOOL	m_4;
	BOOL	m_5;
	BOOL	m_6;
	BOOL	m_7;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CUILayersDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CUILayersDlg)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	BOOL m_bShowPreview;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGItemPrefs dialog
class CRPGItemPrefs : public CPropertyPage
{
	DECLARE_DYNAMIC(CRPGItemPrefs)

public:
	CRPGItemPrefs(CWnd* pParent = NULL);   // standard constructor
	virtual ~CRPGItemPrefs();

// Dialog Data
	enum { IDD = IDD_RPGITEM_PREFS };

	virtual void OnOK();

protected:
	void SetCheck( int nControlID );
	void UpdateCustomFrame();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
	BOOL m_bShowPersItems;
	BOOL m_bSynaxColouring;
	BOOL m_bCustomFrame;
	int m_nFrameWidth;
	int m_nFrameHeight;
	afx_msg void OnBnClickedCustomframe();
	CEdit m_ctrlWidth;
	CEdit m_ctrlHeight;
	CStatic m_staticW;
	CStatic m_staticH;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PREFERENCES_H__74961EE6_C4E2_4058_84CE_A8A719038CC0__INCLUDED_)
