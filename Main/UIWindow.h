#ifndef __A5_UI_WINDOW_H__
#define __A5_UI_WINDOW_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CSound;
}
namespace NScript
{
	class CScript;		// retail CWindow window-scripting members (eventsMap/pScript, op& tags 16/17)
}
#include "..\MiscDll\LogStream.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInterface;
class CToolTip;		// retail CWindow::pToolTip is a typed CObj<CToolTip> (UICommCtrls.h)
////////////////////////////////////////////////////////////////////////////////////////////////////
// Window styles
const int
	STYLE_MODAL						= 0x00000001,
	STYLE_VISIBLE					= 0x00000002,
	STYLE_ENABLED					= 0x00000004,
	STYLE_TOPMOST					= 0x00000008,
	STYLE_BOTTOMMOST			= 0x00000010,
	STYLE_TRANSPARENT			= 0x00000020,
	STYLE_NOACTIVATE			= 0x00000040;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Show window types
const int								
	SWTYPE_HIDE						= 0x00000001,
	SWTYPE_SHOW						= 0x00000002,
	SWTYPE_SHOWNA					= 0x00000004 | SWTYPE_SHOW;
////////////////////////////////////////////////////////////////////////////////////////////////////
// SWindowInfo
struct SWindowInfo
{
	int nStyle;
	string szID;
	SPoint sSize;
	SPoint sPosition;
	CPtr<CWindow> pParent;

	SWindowInfo(): nStyle( 0 ), sSize( 0, 0 ), sPosition( 0, 0 ) {}
	SWindowInfo( CWindow* _pParent, const SPoint &_sPosition, const SPoint &_sSize, const string &_szID = string("default"), int _nStyle = STYLE_VISIBLE | STYLE_ENABLED ):
		pParent( _pParent ), sPosition( _sPosition ), sSize( _sSize ), szID( _szID ), nStyle( _nStyle )
	{
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWindow
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWindow: public CObjectBase
{
	OBJECT_NOCOPY_METHODS(CWindow);
protected:
	ZDATA
	int nStyle;
	bool bActive;
	bool bRequireUpdate;		// retail tag 4 (@0xd4220); retail CWindow::Update @0x3273d0 gates the lazy
							// per-frame update on it -- the dev Update pipeline updates unconditionally,
							// so this stays latched true here (serialized for save-format parity)
	string szID;
	SPoint sSize;
	SPoint sPosition;
	SCursorInfo sInfo;
	CPtr<CWindow> pParent;
	CPtr<CWindow> pMouseFocus;	// retail member order/tags: 10=pMouseFocus, 11=pToolTip (dev had them swapped)
	CObj<CToolTip> pToolTip;
	CPtr<CInterface> pInterface;
	// retail listChildren is nstl::vector<CMObj<CWindow>> (AddChild/RemoveChild/~CWindow/Update all use
	// _M_start/_M_finish; s2_uiwindow.h:25). It was a std::list here: std::list::erase(end()) frees the list
	// SENTINEL node, so RemoveChild on a save-load-inconsistent parent freed a LIVE window's sentinel ->
	// teardown heap-use-after-free (ASan 2026-07-14). A vector has no sentinel -> that failure mode is gone.
	vector<CMObj<CWindow> > listChildren;
	// retail window-scripting/tooltip tail (@0xd4220 tags 14-17). sToolTipAnchor/eToolTipAnchorType are
	// copied from the originating NDb::CUIControl on EVENT_TEMPLATECREATE; eventsMap/pScript belong to
	// the Lua "onmessage" window-scripting layer (this tree's lua build lacks lua_tocallinfo, so the
	// maps stay empty at runtime -- serialized for save-format parity).
	SPoint sToolTipAnchor;
	NDb::EUIAnchor eToolTipAnchorType;
	unordered_map<CPtr<NScript::CScript>,int,SPtrHash> eventsMap;
	CObj<NScript::CScript> pScript;
public:
	ZEND int operator&( CStructureSaver &f );	// retail table @0xd4220 -- defined in UIWindow.cpp (needs CToolTip/NScript::CScript complete)

protected:
	void ActivateTest( int nX, int nY );
	void BringWindowToTop( CWindow *pWindow );
	void FormChildrenList( list<CPtr<CWindow> > *pList );
	
public:
	// retail SWindowInfo-ctor (@0x2112d0 family / oracle iMissionUI): bActive=false, bRequireUpdate=true,
	// sToolTipAnchor=(0,0), eToolTipAnchorType=UIA_NONE, eventsMap empty, pScript null. The default ctor
	// (serialization path) seeds the same deterministic state.
	CWindow(): nStyle( 0 ), bActive( false ), bRequireUpdate( true ), sSize( 0, 0 ), sPosition( 0, 0 ), sToolTipAnchor( 0, 0 ), eToolTipAnchorType( NDb::UIA_NONE ) {}
	CWindow( const SWindowInfo &sInfo );
	virtual ~CWindow();

	const string& GetWindowID() const;

	CInterface* GetInterface() const;
	void SetInterface( CInterface *pInterface );

	CWindow* GetParent() const;
	void SetParent( CWindow *pParent );

	void AddChild( CWindow *pWindow );
	void RemoveChild( CWindow *pWindow );
	CWindow* GetChildByID( const string &szID );
	void GetChildrenList( list<CPtr<CWindow> > *pList );

	bool GetStyle( int nStyle ) const;
	void SetStyle( int nStyle, bool bOn );

	bool IsActive() const;
	void ShowWindow( int nCmdShow );

	bool HitTest( int nX, int nY ) const;
	bool ClientToScreen( SPoint *pPosition, SRect *pWindow, bool bSelf = true ) const;
	void ScreenToClient( const SPoint &sScreenPos, SPoint *pPosition ) const;
	void VirtualToScreen( SPoint *pPosition, SRect *pRes );
	// retail @0x3275c0: full-window depth-clear quad (fZ=1 punches the 3D hole, fZ=0 restores)
	void CreateClearRect( NGScene::I2DGameView *pView, float fZ );

	CToolTip* GetToolTip() const;		// retail @0x327220: returns the typed CObj<CToolTip> member
	void SetToolTip( CToolTip *pWindow );

	const SCursorInfo& GetCursorInfo() const;
	void SetCursorInfo( const SCursorInfo &sInfo );

	virtual const SPoint& GetSize() const;
	virtual void SetSize( const SPoint &_sSize );

	virtual const SPoint& GetPosition() const;
	virtual void SetPosition( const SPoint &_sPosition );

	virtual void PlaySound( NDb::CSound *pSound );

	virtual bool SendMessage( CWindow *pTarget, const SEvent &sEvent );
	virtual bool ProcessMessage( const SEvent &sEvent );

	virtual void Update( const STime &sTime, NGScene::I2DGameView *pView );
	virtual void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// UI-Template functions
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE>
TYPE* GetUIWindow( CWindow *pContainer, const string &szID )
{
	CWindow* pChild = pContainer->GetChildByID( szID );
	if ( IsValid( pChild ) )
	{
		TYPE* pTarget = dynamic_cast<TYPE*>( pChild );
		if ( IsValid( pTarget ) )
			return pTarget;
	}

	csSystem << "UI-ERROR: UI Container not complete, control " << szID << " in container " << pContainer->GetWindowID() << " not found" << endl;
	return new TYPE( SWindowInfo( pContainer, SPoint( 0, 0 ), SPoint( 0, 0 ), szID, STYLE_ENABLED ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
