#pragma once

class IFindNextEvent: public CObjectBase
{
public:
	virtual void FindNext() {};
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class CLuaEditor;
class CFindNext: public IFindNextEvent
{
	OBJECT_NOCOPY_METHODS(CFindNext)
	CLuaEditor *pEditor;
public:
	CFindNext( CLuaEditor *_pEditor = 0 ): pEditor(_pEditor) {}
	virtual void FindNext();
};

// CFindTextDlg dialog
class CFindTextDlg : public CDialog
{
	DECLARE_DYNAMIC(CFindTextDlg)

public:
	CFindTextDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CFindTextDlg();

	void SetHandler( IFindNextEvent *_pHandler ) { pHandler = _pHandler; }
// Dialog Data
	enum { IDD = IDD_FINDTEXT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnCancel();

	DECLARE_MESSAGE_MAP()
public:
	CString m_szText;
	BOOL m_bWholeWord;
	BOOL m_bCase;
	CObj<IFindNextEvent> pHandler;
	afx_msg void OnBnClickedFindNext();
};
