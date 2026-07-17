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
class CToolTip;
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
	SPoint sLastLButtonClickPoint;	// last L-click in UI space; transient like the times (retail @0x31c090 dblclk gate)
	STime sLastTime;		// transient UI ms clock (retail CInterface sLastTime @+0xb8): set each Step(), read by NScript::luaGetUITime; NOT serialized (retail op& @0x31fea0 skips it)
	SPoint sCursorPoint;
	SCursorInfo sCursor;
	SCursorInfo sDefaultCursor;
	CTimeCounter sTimer;		// dev frame timer -- retail dropped it from the format (tag-8 hole @0x31fea0); kept as a transient member
	CDGPtr<CCTime> pTimer;		// dev frame-time source -- retail tag-9 hole; transient
	CPtr<ICursor> pCursor;
	CPtr<CConsole> pConsole;
	CPtr<NSound::ISoundScene> pSound;
	CObj<NGScene::I2DGameView> pView;
	CMObj<CMouseCaptureHandler> pMouseCapture;
	////
	CPtr<CWindow> pToolTipOwner;	// retail tags: 15=pToolTipOwner, 16=pToolTip (dev had the pair swapped)
	CObj<CToolTip> pToolTip;		// retail type: typed CObj<NUI::CToolTip>
	////
	bool bShowFPSStats;
	CObj<CTextDraw> pFPSText;
	// retail tail members @0x31fea0 tags 20-23:
	bool bToolTipSet;			// sticky "a window claimed the tooltip this frame" flag (retail Step/SetToolTipOwner)
	float fMouseWheelDelta;		// mouse-wheel accumulator (retail ProcessEvent @0x31c090 step 11)
	CTimeCounter sCounter;		// retail sound-listener counter (advanced in Draw when bOwnSoundScene)
	bool bOwnSoundScene;		// this interface created its own sound scene (retail ctor @0x31dbd0)
	///CRAP
	CObj<CTextDraw> pNonPublicDemo;	// dev-only overlay -- retail tag-19 hole @0x31fea0; kept as a transient member
	// retail NUI::CInterface::operator& @0x31fea0: 1=CWindow base, 5=sCursorPoint, 6=sCursor,
	// 7=sDefaultCursor, 10=pCursor, 11=pConsole, 12=pSound, 13=pView, 14=pMouseCapture,
	// 15=pToolTipOwner, 16=pToolTip, 17=bShowFPSStats, 18=pFPSText, 20=bToolTipSet,
	// 21=fMouseWheelDelta, 22=sCounter, 23=bOwnSoundScene. Tags 2,3,4,8,9,19 are format HOLES
	// (retail stopped serializing the button/double-click times, the frame timer pair and the
	// non-public-demo overlay -- the members above stay transient).
	ZEND int operator&( CStructureSaver &f );	// defined in UIInterface.cpp (needs CToolTip complete)

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
	void ResetMouseCapture();		// retail @0x31c060

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
