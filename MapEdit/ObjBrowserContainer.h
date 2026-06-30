#pragma once

#include "ObjBrowserDlg.h"
#include "ObjBrowserFrame.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjBrowserContainer dialog
class CObjBrowserContainer : public CDialog
{
	DECLARE_DYNAMIC(CObjBrowserContainer)

public:
	CObjBrowserContainer( vector< CPtr<IObjectBrowser> > browsers, CWnd* pParent = NULL);   // standard constructor
	virtual ~CObjBrowserContainer();

	void SetObject( int nObjectID, int nVarID );
// Dialog Data
	enum { IDD = IDD_OB_BROWSER_CONTAINER };

	virtual BOOL OnInitDialog();

protected:
	void Create();

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CObjBrowserFrame m_Frame;
	CSize ptIndent;

	vector<CObjBrowserDlg*> obDlgs;
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
