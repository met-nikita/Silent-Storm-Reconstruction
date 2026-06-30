#ifndef __A5_CONSOLE_H__
#define __A5_CONSOLE_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Console modes
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EConsoleMode
{
	CON_NONE,
	CON_SLIDEUP,
	CON_SLIDEDOWN
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// CTextView
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTextView: public CWindow
{
	OBJECT_NOCOPY_METHODS(CTextView);
private:
	ZDATA_(CWindow)
	int nScroll;
	int nLineCount;
	vector< CPtr<CTextDraw> > textLines;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&nScroll); f.Add(3,&nLineCount); f.Add(4,&textLines); return 0; }

protected:
	void DrawLine( const STime &sTime, NGScene::I2DGameView *pView, int nLine, const wstring &wsText, int *pHeight, vector< CPtr<CTextDraw> > *pNewTextLines );

public:
	CTextView() {}
	CTextView( const SWindowInfo &sInfo );

	void Scroll( int nValue );
	void ScrollToEnd();

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Console class
class CConsole: public CWindow
{
	OBJECT_BASIC_METHODS(CConsole);
private:
	ZDATA_(CWindow)
	float fWeight;
	STime sLastTime;
	EConsoleMode eMode;
	CPtr<CEdit> pEdit;
	CPtr<CTextView> pTextView;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&fWeight); f.Add(3,&sLastTime); f.Add(4,&eMode); f.Add(5,&pEdit); f.Add(6,&pTextView); return 0; }

public:
	CConsole() {}
	CConsole( const SWindowInfo &sInfo );
	
	void SetConsoleState( bool bVisible );

	bool ProcessEvent( const NInput::SEvent &eEvent );
	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif