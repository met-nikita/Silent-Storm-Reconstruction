#ifndef __A5_UI_INTERFACE_H__
#define __A5_UI_INTERFACE_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CString;
	class CSound;
}
namespace NSound
{
	class ISoundScene;
}
#include "..\Input\Bind.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWindow;
class CConsole;
class CTextDraw;
class CMouseCaptureHandler;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////////////////////////
wstring GetDBString( int nID );
wstring GetDBString( NDb::CString *pString );
void LoadTemplate( CWindow *pWindow, NDb::CUIContainer *pTemplate );
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLoader
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLoader: public CObjectBase
{
	OBJECT_BASIC_METHODS(CLoader)
private:
	struct SWindow
	{
		SWindowInfo sInfo;
		CPtr<NDb::CUIControl> pControl;
	};
	typedef pair<CPtr<CWindow>,SWindow> TTemplateWindow;

	CPtr<CWindow> pParent;
	vector<TTemplateWindow> windowsSet;

public:
	CLoader() {}

	void Load( CWindow* _pParent, NDb::CUIContainer *pTemplate );

	const SWindowInfo& GetControl( const string &szID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInterface: public CWindow
{
	OBJECT_BASIC_METHODS(CInterface);
private:
	NInput::CBind cmdConsole, cmdLButtonDown, cmdLButtonUp, cmdRButtonDown, cmdRButtonUp, bindScroll, cmdFPSShow;

	ZDATA_(CWindow)
	STime sLastLButtonDownTime, sLastRButtonDownTime, sDoubleClickTime;
	STime sLastTime;		// transient UI ms clock (retail CInterface sLastTime @+0xb8): set each Step(), read by NScript::luaGetUITime; NOT serialized (deliberately omitted from operator& below)
	SPoint sCursorPoint;
	SCursorInfo sCursor;
	SCursorInfo sDefaultCursor;
	CTimeCounter sTimer;
	CDGPtr<CCTime> pTimer;
	CPtr<ICursor> pCursor;
	CPtr<CConsole> pConsole;
	CPtr<NSound::ISoundScene> pSound;
	CObj<NGScene::I2DGameView> pView;
	CMObj<CMouseCaptureHandler> pMouseCapture;
	////
	CObj<CWindow> pToolTip;
	CPtr<CWindow> pToolTipOwner;
	////
	bool bShowFPSStats;
	CObj<CTextDraw> pFPSText;
	///CRAP
	CObj<CTextDraw> pNonPublicDemo;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&sLastLButtonDownTime); f.Add(3,&sLastRButtonDownTime); f.Add(4,&sDoubleClickTime); f.Add(5,&sCursorPoint); f.Add(6,&sCursor); f.Add(7,&sDefaultCursor); f.Add(8,&sTimer); f.Add(9,&pTimer); f.Add(10,&pCursor); f.Add(11,&pConsole); f.Add(12,&pSound); f.Add(13,&pView); f.Add(14,&pMouseCapture); f.Add(15,&pToolTip); f.Add(16,&pToolTipOwner); f.Add(17,&bShowFPSStats); f.Add(18,&pFPSText); f.Add(19,&pNonPublicDemo); return 0; }

protected:
	void UpdateFPSText();

public:
	CInterface();
	CInterface( ICursor* pCursor, NSound::ISoundScene *pSound = 0 );

	const SPoint& GetCursorPos() const;
	STime GetLastTime() const { return sLastTime; }		// UI ms clock, read by NScript::luaGetUITime

	const SCursorInfo& GetCursorInfo() const;
	const SCursorInfo& GetDefaultCursorInfo() const;
	void SetCursorInfo( const SCursorInfo &sInfo );

	void SetToolTipOwner( CWindow *pOwner );
	CObjectBase* CreateMouseCapture( CWindow *pWindow );

	NSound::ISoundScene* GetSound();
	NGScene::I2DGameView* GetView();

	bool IsActive() const { return true; };
	bool IsMouseCover() const { return true; };

	bool ProcessEvent( const NInput::SEvent &eEvent );
	bool ProcessMessage( const SEvent &sEvent );
	void UpdateCursor();
	void Step( const STime &sTime );
	void Draw( const STime &sTime );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
