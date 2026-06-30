#pragma once

#include "ObjectBrowser.h"
#include "ObjectBrowserSpin.h"
class COIDlg;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjBrowserDlg dialog
class CObjBrowserDlg : public CDialog
{
	DECLARE_DYNAMIC(CObjBrowserDlg)

public:
	CObjBrowserDlg( IObjectBrowser *pOB, CWnd* pParent = NULL );   // standard constructor
	virtual ~CObjBrowserDlg();

	void SetObject( int nObjID, int nVarID );
	CSize GetSize();
// Dialog Data
	enum { IDD = IDD_OBJECT_BROWSER };

	virtual BOOL OnInitDialog();

protected:
	typedef unordered_map< int, CObj<CSpin> > CSpinMap;
	CSize size;
	int nObjectID, nVariantID;
	CPtr<IObjectBrowser> pObjBrowser;
	vector<COIDlg*> oiCtrls;
	CSpinMap spins;
	vector<CPropMap> properties;

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	void Create();
	void ResizeColumns( int nSize );
	void SetObjectInternal();

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
