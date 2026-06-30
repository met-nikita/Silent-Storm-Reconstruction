#pragma once
#include "afxwin.h"
#include "TextEditor.h"

class CMETreeView;
// CScriptEditDlg dialog

class CScriptEditDlg : public CDialog
{
	DECLARE_DYNAMIC(CScriptEditDlg)

public:
	CScriptEditDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CScriptEditDlg();

// Dialog Data
	enum { IDD = IDD_SCRIPT_EDIT };

	void SetScriptID( int nID );

	BOOL PreTranslateMessage(MSG* pMsg);

protected:
	CMETreeView *m_pTree;
	int nItemID;
	int nPrevSelected;

	void SetMapScriptID( int nID );
	void ShowScriptText( int nID );
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
	CStatic m_treePlace;
	CStatic m_ScriptPlace;

	CTextEditor mEdit;
	afx_msg void OnRelEmpty();
	afx_msg void OnOK();
	afx_msg void OnCancel();
};
