// TextEditor.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "TextEditor.h"
#include "Export.h"
#include "..\Script\Script.h"
#include <fstream>
#include "MEParams.h"
#include "MEUserSettings.h"

#pragma comment(linker, "/include:_ForceLuaLexer")

////////////////////////////////////////////////////////////////////////////////////////////////////
static string szErr;
static int ScriptLOG(lua_State* state)
{
	Script script(state);
	Script::Object obj = script.GetObject(script.GetTop());
	string sz = obj.GetString();
	for ( string::const_iterator i = sz.begin(); i != sz.end(); ++i )
		if ( *i != '\n' )
			szErr += *i;
		else
			szErr += "\r\n";
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextEditor::OnEnSelchangeEditText(NMHDR *pNMHDR, LRESULT *pResult)
{
	SELCHANGE *pSelChange = reinterpret_cast<SELCHANGE *>(pNMHDR);
	// TODO:  The control will not send this notification unless you override the
	// CDialog::OnInitDialog() function to send the EM_SETEVENTMASK message
	// to the control with the ENM_SELCHANGE flag ORed into the lParam mask.

	// TODO:  Add your control notification handler code here

	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTextEditor dialog
IMPLEMENT_DYNAMIC(CTextEditor, CDialog)
CTextEditor::CTextEditor( bool _bInitiallySelected, CWnd* pParent /*=NULL*/)
	: CDialog(CTextEditor::IDD, pParent), bInitiallySelected(_bInitiallySelected)
{
	bCheckSyntax = false;
	bFreezeUpdate = false;
	bModal = true;
}

CTextEditor::~CTextEditor()
{
	//m_fntDef.DeleteObject();
}

void CTextEditor::CheckSyntax( bool bCheck )
{
	bCheckSyntax = bCheck;
	if ( bCheck && ::IsWindow( m_hWnd ) )
		m_LuaEditor.SetLuaLexer();
}

void CTextEditor::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//DDX_Text(pDX, IDC_EDIT_TEXT, m_szText);
	DDX_Text(pDX, IDC_ERRLOG, m_szErrLog);
	DDX_Control(pDX, IDC_EDIT_TEXT, m_LuaEditor );
	DDX_Control(pDX, IDC_ERRLOG, m_ctrlErrLog);
	DDX_Control(pDX, IDC_LINEINFO, m_ctrlLineInfo);
}

void CTextEditor::SetText( const string &szText )
{
	if ( !::IsWindow( m_hWnd ) )
	{
		szInitialText = szText;
		return;
	}
	bFreezeUpdate = true;
	m_LuaEditor.ClearAll();
	m_LuaEditor.AddText( szText.c_str() );
	bFreezeUpdate = false;
}

string CTextEditor::GetText()
{
	if ( ::IsWindow( m_LuaEditor ) )
		return m_LuaEditor.GetText();
	return szLastText;
}

BOOL CTextEditor::OnInitDialog()
{
	if ( !CDialog::OnInitDialog() )
		return FALSE;
	m_LuaEditor.InitScintilla();
	m_LuaEditor.SetEditorMargins();
	// Create font
	/*
	LOGFONT lf;
	memset(&lf, 0, sizeof(LOGFONT));			// zero out structure
	lf.lfHeight = 15;							// request a ?-pixel-height font
	strcpy( lf.lfFaceName, "Courier New" );	// request a face name "Arial", "Courier", "MS Sans Serif"
	lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
	m_fntDef.CreateFontIndirect(&lf);			// create the fonts
	SetFont( &m_fntDef );
*/
	//
	try
	{
		string szDir = GetExportDstDir();
		ifstream fKeywords( (szDir + "ScriptKeywords.txt").c_str() );
		string szKeywords;
		vector<string> vszAutoComplete;

		while ( !fKeywords.bad() && !fKeywords.eof() )
		{
			char buf[512];
			fKeywords.getline( buf, sizeof(buf) );
			szKeywords += buf;
			szKeywords += ' ';
			vszAutoComplete.push_back( buf );
		}
		sort( vszAutoComplete.begin(), vszAutoComplete.end() );
		m_LuaEditor.AddFunctionNames( szKeywords.c_str() );
		m_LuaEditor.SetAutoComplete( vszAutoComplete );
	}
	catch (...) 
	{
	}

	SetText( szInitialText );
	CheckSyntax( bCheckSyntax );
	CheckSyntax();
	return true;
}

BEGIN_MESSAGE_MAP(CTextEditor, CDialog)
	//ON_EN_UPDATE(IDC_EDIT_TEXT, OnEnUpdateEditText)
	ON_WM_CHAR()
	ON_WM_KEYDOWN()
	ON_NOTIFY(EN_SELCHANGE, IDC_EDIT_TEXT, OnEnSelchangeEditText)
	ON_NOTIFY(SCN_CHARADDED, IDC_EDIT_TEXT, OnCnCharAdded)
	ON_EN_CHANGE(IDC_EDIT_TEXT, OnEnChangeEditText)
END_MESSAGE_MAP()


// CTextEditor message handlers


void CTextEditor::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{

	CDialog::OnChar(nChar, nRepCnt, nFlags);
}

void CTextEditor::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{

	CDialog::OnKeyDown(nChar, nRepCnt, nFlags);
}


void CTextEditor::OnEnChangeEditText()
{
	CheckSyntax();
	CWnd *pWnd = GetParent();
	if ( pWnd && !bFreezeUpdate )
		pWnd->PostMessage( WM_ME_TEXTCHANGED );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextEditor::CheckSyntax()
{
	if ( !bCheckSyntax || !::IsWindow( m_hWnd ) )
		return;
	if ( GetUserSettings().GetParam( ME_SCRIPT_SYNAXCOLORING ) )
	{
		CString str = GetText().c_str();

		//str.Replace( '\r', '\n' );
		Script script( true, ScriptLOG );
		szErr.clear();
//		lua_State *pState = script.GetState();
//		if ( pState->currentThread == 0 )
//			lua_setThread( pState, lua_newThread( pState ) );
		script.ParseBuffer( (LPCSTR)str, str.GetLength() );
		m_ctrlErrLog.SetWindowText( szErr.c_str() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextEditor::OnOK()
{
	szLastText = GetText();
	if ( bModal )
		CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextEditor::OnCancel()
{
	if ( bModal )
		CDialog::OnCancel();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextEditor::OnCnCharAdded(NMHDR *pNMHDR, LRESULT *pResult)
{
	SCNotification *pNotify = reinterpret_cast<SCNotification*>(pNMHDR);
	char ch = pNotify->ch;

	if  (ch  ==  '\r'  ||  ch  ==  '\n')
	{
		m_LuaEditor.NewLineIndent();
	}
	else if ( isalpha( ch ) )
	{
		m_LuaEditor.AutoComplete();
	}

	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////