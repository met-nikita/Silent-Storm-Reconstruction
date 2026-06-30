#pragma once
#include "afxcmn.h"
#include "SyntaxColorizer.h"
#include "LuaEditor.h"

const int WM_ME_TEXTCHANGED = WM_USER + 5;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTextEditor dialog
class CTextEditor : public CDialog
{
	DECLARE_DYNAMIC(CTextEditor)

public:
	CTextEditor( bool bInitiallySelected = false, CWnd* pParent = NULL);   // standard constructor
	virtual ~CTextEditor();

	void CheckSyntax( bool bCheck );
// Dialog Data
	enum { IDD = IDD_TEXT_EDIT };
	virtual BOOL OnInitDialog();

	void SetText( const string &szText );
	string GetText();
	void SetModal( bool _bModal ) { bModal = _bModal; }
	//CString m_szText;
	CString m_szErrLog;

protected:
	bool bInitiallySelected;
	BOOL bCheckSyntax;
	CFont m_fntDef;
	string szInitialText;
	string szLastText;
	bool bFreezeUpdate;
	bool bModal;

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();
	virtual void OnCancel();
	void CheckSyntax();

	DECLARE_MESSAGE_MAP()
public:
	CLuaEditor m_LuaEditor;
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	CEdit m_ctrlErrLog;
	CEdit m_ctrlLineInfo;
	afx_msg void OnEnSelchangeEditText(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnEnChangeEditText();
	afx_msg void OnCnCharAdded(NMHDR *pNMHDR, LRESULT *pResult);
};
////////////////////////////////////////////////////////////////////////////////////////////////////
