#ifndef __A5_UI_COMMON_CONTROLS_H__
#define __A5_UI_COMMON_CONTROLS_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "../DBFormat/DataSound.h"
#include "UIBaseCtrls.h"   // NUI::CText -- retail CEdit's base class
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	class CCTPoint;
	class CCWString;
	class CScreenshotTexture;
	class IVideoPlayer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMLLayout;
class CImageDraw;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Common controls styles
const int
	SLDRSTYLE_HORZ				= 0x00000100,
	SLDRSTYLE_VERT				= 0x00000200;
// Scroll
const int
	SCRLSTYLE_HORZ				= 0x00000100,
	SCRLSTYLE_VERT				= 0x00000200;
// ListView
const int
	LVSTYLE_SHOWSELALWAYS = 0x00000100;
// ProgressBar
const int
	PBARSTYLE_HORZ				= 0x00000100,
	PBARSTYLE_VERT				= 0x00000200,
	PBARSTYLE_SCALE				= 0x00000400,
	PBARSTYLE_CENTERED		= 0x00000800;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Common controls events
const int
	EVENT_LISTVIEW_ITEMSELECTED	=	0x00000101 | EVENT_FLAG_NOTIFY;	// retail 0x1000101 (senders @0x1ba890/@0x316890, receiver @0x3169e0)
////////////////////////////////////////////////////////////////////////////////////////////////////
// Common controls flags
const int
// ListView flags
	ELV_ITEMSSELECTED_TRUE		= 0x000000001,
	ELV_ITEMSSELECTED_FALSE		= 0x000000000;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEdit -- retail NUI::CEdit (UICommCtrls.obj, saveload id 0xB0241960, PDB sizeof 228): derives
// from NUI::CText (NOT CWindow -- the Jan03 CWindow base with bActiveState/sTexRect was dropped in
// retail). operator& @0x31b2d0: 1=CText base, 2=nSize, 3=nCursor, 4=eMode, 5=bCursorVisible (1B),
// 6=sFlashTime, 7=wsText (string chunk), 8=wsFormat (string chunk), 9=sCursorInfo. The dev render
// nodes (pSize/pTextString/pText) do not exist in retail and stay OFF-WIRE (rebuildable draw
// caches; retail renders through the CText base IML instead).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEdit: public CText
{
	OBJECT_NOCOPY_METHODS(CEdit);
public:
	enum EMode
	{
		NORMAL,
		NUMERIC,
		FILENAME
	};

private:
	ZDATA_(CText)
	int nSize;
	int nCursor;
	EMode eMode;
	bool bCursorVisible;
	STime sFlashTime;
	wstring wsText;
	wstring wsFormat;
	////
	SCursorInfo sCursorInfo;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CText*)this); f.Add(2,&nSize); f.Add(3,&nCursor); f.Add(4,&eMode); f.Add(5,&bCursorVisible); f.Add(6,&sFlashTime); f.Add(7,&wsText); f.Add(8,&wsFormat); f.Add(9,&sCursorInfo); return 0; }

private:
	// Off-wire draw caches (dev render infra; absent from retail CEdit and from retail saves --
	// both ctors create pSize/pTextString, Draw re-syncs them and lazily creates pText).
	CObj<NGScene::CCTPoint> pSize;
	CObj<NGScene::CCWString> pTextString;
	CDGPtr< CFuncBase<NGScene::SText> > pText;

public:
	CEdit();
	CEdit( const SWindowInfo &sInfo );
	
	EMode GetMode() const;
	void SetMode( EMode eMode );

	const wstring& GetText() const;
	void SetText( const wstring &wsText );

	void SetEditSize( int nSize );
	void SetTextFormat( const wstring &wsFormat );
	void SetCursorPosition( int nPos );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CButton
////////////////////////////////////////////////////////////////////////////////////////////////////
class CButton: public CWindow
{
	OBJECT_NOCOPY_METHODS(CButton);
private:
	ZDATA_(CWindow)
	int nState;
	bool bPushed;
	bool bMouseEnter;
	string szID;
	CPtr<CImage> pGlow;
	CPtr<CImage> pMountUp;
	CPtr<CImage> pMountDown;
	CPtr<CImage> pMountDisabled;
	NGfx::SPixel8888 sColor;
	CObj<CObjectBase> pMouseCaptture;
	CDBPtr<NDb::CSound> pClickSound;
	unordered_map<int,CObj<CWindow> > statesMap;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&nState); f.Add(3,&bPushed); f.Add(4,&bMouseEnter); f.Add(5,&szID); f.Add(6,&pGlow); f.Add(7,&pMountUp); f.Add(8,&pMountDown); f.Add(9,&pMountDisabled); f.Add(10,&sColor); f.Add(11,&pMouseCaptture); f.Add(12,&pClickSound); f.Add(13,&statesMap); return 0; }

protected:
	virtual void OnAction();

public:
	CButton() {}
	CButton( const SWindowInfo &sInfo );

	void SetNotifyID( const string &szID );

	const NGfx::SPixel8888& GetColor() const;
	void SetColor( const NGfx::SPixel8888 &sColor );

	CWindow* AddState( int nID );
	void RemoveState( int nID );
	CWindow* GetState( int nID ) const;

	CWindow* AddTextState( int nID, const wstring &wsText );
	CWindow* AddImageState( int nID, NDb::CUITexture *pTexture, const NGfx::SPixel8888 &sColor = NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ) );

	int GetActiveState();
	void SetActiveState( int nID );

	bool IsPushed() const;
	bool IsMouseCover() const;

	bool ProcessMessage( const SEvent &sEvent );
	void Update( const STime &sTime, NGScene::I2DGameView *pView );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPushButton
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPushButton: public CButton
{
	OBJECT_NOCOPY_METHODS(CPushButton);
private:
	ZDATA_(CButton)
	wstring wsText;
	CObj<CTextDraw> pText;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CButton*)this); f.Add(2,&wsText); f.Add(3,&pText); return 0; }
	
public:
	CPushButton() {}
	CPushButton( const SWindowInfo &sInfo );

	const wstring& GetText() const;
	void SetText( const wstring &wsText );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCheckButton
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCheckButton: public CPushButton
{
	OBJECT_NOCOPY_METHODS(CCheckButton);
private:
	ZDATA_(CPushButton)
	bool bChecked;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CPushButton*)this); f.Add(2,&bChecked); return 0; }
	
protected:
	virtual void OnAction();

public:
	CCheckButton() {}
	CCheckButton( const SWindowInfo &sInfo );

	bool IsChecked() const;
	void SetChecked( bool bState );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// (CToolTip is defined below, AFTER CFrame -- retail NUI::CToolTip derives from CFrame.)
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFrame -- retail NUI::CFrame (saveload id 0xB024196D; ctors @0x1e5ea0/@0x316160, operator&
// @0x1e66f0, Draw @0x3135f0): the nine-slice background frame widget, base of CTextFrame /
// CFrameText (iGlobalMapUI) / CAckView in retail. The SWindowInfo ctor fills the nine slices with
// the engine's standard frame skin (the same UI-texture ids 654..662 the dev CToolTip used).
// operator&: 1=CWindow base, 2=pBackgroundUp .. 10=pBackgroundDownRight.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFrame: public CWindow
{
	OBJECT_NOCOPY_METHODS(CFrame);
protected:
	ZDATA_(CWindow)
	CObj<CImageDraw> pBackgroundUp;
	CObj<CImageDraw> pBackgroundDown;
	CObj<CImageDraw> pBackgroundLeft;
	CObj<CImageDraw> pBackgroundRight;
	CObj<CImageDraw> pBackgroundMiddle;
	CObj<CImageDraw> pBackgroundUpLeft;
	CObj<CImageDraw> pBackgroundUpRight;
	CObj<CImageDraw> pBackgroundDownLeft;
	CObj<CImageDraw> pBackgroundDownRight;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pBackgroundUp); f.Add(3,&pBackgroundDown); f.Add(4,&pBackgroundLeft); f.Add(5,&pBackgroundRight); f.Add(6,&pBackgroundMiddle); f.Add(7,&pBackgroundUpLeft); f.Add(8,&pBackgroundUpRight); f.Add(9,&pBackgroundDownLeft); f.Add(10,&pBackgroundDownRight); return 0; }

public:
	CFrame() {}                          // retail @0x1e5ea0: nine null slices
	CFrame( const SWindowInfo &sInfo );  // retail @0x316160: fills the nine slices with the standard skin

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );   // retail @0x3135f0
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTextFrame -- retail NUI::CTextFrame (module UICommCtrls, saveload id 0xb024196e): a self-sizing
// bordered panel = a CFrame (which owns the nine border slices) plus one CText markup label.
// operator& @0x31b420: 1=CFrame base, 2=pText -- the border slices serialize through the base
// (convergence W4: dev previously built it on CWindow with its own 9 slices at tags 3-11).
// Differs from CToolTip in the retail-exact resize/clamp (SetText @0x3143c0 re-fits + re-clamps
// INSIDE THE PARENT, not the fixed 1024x768 screen). Used by the tactical-state unit tooltip
// (MakeUnitStateToolTip).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTextFrame: public CFrame
{
	OBJECT_NOCOPY_METHODS(CTextFrame);
private:
	ZDATA_(CFrame)
	CObj<CText> pText;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CFrame*)this); f.Add(2,&pText); return 0; }

protected:
	void UpdateSize();          // retail @0x313950: fit the frame around the label
	void UpdatePosition();      // retail @0x314160: clamp the frame inside its parent

public:
	CTextFrame() {}
	CTextFrame( const SWindowInfo &sInfo );   // retail @0x3163c0

	void SetText( const wstring &wsText );   // retail @0x3143c0

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );   // retail @0x3143f0: re-fit + re-clamp, then CFrame::Draw
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CToolTip -- retail NUI::CToolTip (saveload id 0xB0241964; ctors @0x319f70/@0x316510). Like
// CTextFrame it is a CFrame (which owns the nine border slices) plus one CText markup label; the
// SWindowInfo ctor chains CFrame(sInfo) to fill the slices, so the border round-trips through the
// CFrame base. operator& @ (inlined): 1=CFrame base, 2=pText -- NOT the dev-legacy CWindow base with
// its own 9 slices at tags 3-11, which mis-reads a retail save (the slices are nested inside the
// CFrame base chunk at tag 1) -> null pBackground* -> CToolTip::Draw AV in CImageDraw::GetWindow.
// Differs from CTextFrame in the clamp target: the tooltip re-fits + clamps to the 1024x768 SCREEN.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CToolTip: public CFrame
{
	OBJECT_NOCOPY_METHODS(CToolTip);
private:
	ZDATA_(CFrame)
	CObj<CText> pText;   // retail's tooltip text is a CText
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CFrame*)this); f.Add(2,&pText); return 0; }

public:
	CToolTip() {}
	CToolTip( const SWindowInfo &sInfo );

	bool GetVal( const wstring &szID, wstring *pVal );
	void SetVal( const wstring &szID, int nVal );
	void SetVal( const wstring &szID, float fVal );
	void SetVal( const wstring &szID, const wstring &wsVal );

	void SetText( const wstring &szText );

	void SetPosition( const SPoint &_sPosition );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSlider
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSlider: public CWindow
{
	OBJECT_NOCOPY_METHODS(CSlider);
private:
	ZDATA_(CWindow)
	int nValue;
	int nMaxValue;
	int nPageStep;
	bool bSlide;
	CPtr<CWindow> pSlider;
	CObj<CObjectBase> pMouseCapture;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&nValue); f.Add(3,&nMaxValue); f.Add(4,&nPageStep); f.Add(5,&bSlide); f.Add(6,&pSlider); f.Add(7,&pMouseCapture); return 0; }

protected:
	void Slide( int nX, int nY );
	void PageSlide( int nX, int nY );
	virtual void OnAction();

public:
	CSlider() {}
	CSlider( const SWindowInfo &sInfo );

	int GetValue();
	void SetValue( int nVal );

	int GetMaxValue() const;
	void SetMaxValue( int nMaxValue );

	void SetPageStep( int nPageStep );

	bool ProcessMessage( const SEvent &sEvent );
	void Update( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScroll
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScroll: public CWindow
{
	OBJECT_NOCOPY_METHODS(CScroll);
private:
	ZDATA_(CWindow)
	CPtr<CWindow> pNotify;
	CPtr<CSlider> pSlider;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pNotify); f.Add(3,&pSlider); return 0; }

protected:
	virtual void OnAction();

public:
	CScroll() {}
	CScroll( const SWindowInfo &sInfo );

	int GetValue();
	void SetValue( int nVal );

	int GetMaxValue() const;
	void SetMaxValue( int nMaxValue );

	void SetPageStep( int nPageStep );

	void SetNotifyWindow( CWindow *pWindow );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CListView
////////////////////////////////////////////////////////////////////////////////////////////////////
class CListView: public CWindow
{
	OBJECT_NOCOPY_METHODS(CListView);
private:
	struct SItem
	{
		ZDATA
		int nID;
		bool bHidden;   // retail SItem +0xc: hidden rows are selectable but never shown (operator& @0x31b0a0 tags 2/3/4)
		CObj<CWindow> pWindow;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nID); f.Add(3,&bHidden); f.Add(4,&pWindow); return 0; }

		SItem(): nID( -1 ), bHidden( false ) {}
		SItem( int _nID, CWindow *_pWindow, bool _bHidden = false ): nID( _nID ), bHidden( _bHidden ), pWindow( _pWindow ) {}
	};
	ZDATA_(CWindow)
	int nSelectedID;
	list<SItem> itemsList;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&nSelectedID); f.Add(3,&itemsList); return 0; }

protected:
	bool FindItemByID( int nID, SItem *pItem ) const;
	bool FindItemByWindow( CWindow *pWindow, SItem *pItem ) const;
	virtual void OnAction();

public:
	CListView() {}
	CListView( const SWindowInfo &sInfo );
	
	void AddItem( int nID, CWindow *pItem );
	void AddHiddenItem( int nID, CWindow *pItem );   // retail @0x3179d0
	void RemoveItem( int nID );
	void RemoveAllItems();
	int GetItemsCount() const;
	CWindow* GetItem( int nID ) const;
	// GetItemsList @0x317170 -- snapshot every row window into *pList (push each item's CObj<CWindow> as
	// a CPtr<CWindow>). Added for the iCustomGameMenu mods view (CCustomGameView::GetModsList enumerates
	// the left list by window, which list ids cannot do after MoveItem re-keys rows).
	void GetItemsList( list<CPtr<CWindow> > *pList ) const;

	int GetSelectedItem() const;
	void SetSelectedItem( int nID );

	bool ProcessMessage( const SEvent &sEvent );
	void Update( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CComboBox
////////////////////////////////////////////////////////////////////////////////////////////////////
class CComboBox: public CWindow
{
	OBJECT_NOCOPY_METHODS(CComboBox)
public:
	enum EState
	{
		STATE_NORMAL		= 0,
		STATE_HILIGHTED,
		STATE_SELECTED,
		STATE_DISABLED,
		STATE_LAST
	};
	struct SInfo
	{
		ZDATA
		wstring wsText;
		NGfx::SPixel8888 sColor;
		CDBPtr<NDb::CUITexture> pImage;

		SInfo(): sColor( 0, 0, 0, 0 ) {}
		SInfo( const wstring &_wsText, NDb::CUITexture *_pImage = 0, const NGfx::SPixel8888 &_sColor = NGfx::SPixel8888( 0, 0, 0, 0 ) ): wsText( _wsText ), pImage( _pImage ), sColor( _sColor ) {}
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&wsText); f.Add(3,&sColor); f.Add(4,&pImage); return 0; }
	};

private:
	ZDATA_(CWindow)
	CObj<CWindow> pSelected;
	CPtr<CWindow> pSelectedView;
	CObj<CListView> pList;
	CObj<CObjectBase> pMouseCapture;
	vector<SInfo> statesSet;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pSelected); f.Add(3,&pSelectedView); f.Add(4,&pList); f.Add(5,&pMouseCapture); f.Add(6,&statesSet); return 0; }

public:
	CComboBox() {}
	CComboBox( const SWindowInfo &sInfo );

	void AddItem( int nID, const SInfo &sItem, int nTemplate = -1 );
	void AddHiddenItem( int nID, const SInfo &sItem, int nTemplate = -1 );   // retail @0x318fa0: selectable but not listed
	void RemoveAllItems();
	bool GetItem( int nID, SInfo *pInfo );
	void SetStateInfo( EState eState, const SInfo &sInfo );

	int GetSelectedItem() const;
	void SetSelectedItem( int nID );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CProgressBar
////////////////////////////////////////////////////////////////////////////////////////////////////
class CProgressBar: public CWindow
{
	OBJECT_NOCOPY_METHODS(CProgressBar);
private:
	ZDATA_(CWindow)
	float fValue;
	int nImageWidth;
	CObj<CImageDraw> pImage;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&fValue); f.Add(3,&nImageWidth); f.Add(4,&pImage); return 0; }

public:
	CProgressBar() {}
	CProgressBar( const SWindowInfo &sInfo );
	
	float GetValue();
	void SetValue( float fValue );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScreenShot
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScreenShot: public CWindow
{
	OBJECT_NOCOPY_METHODS(CScreenShot);
public:
	enum EMode
	{
		COLOR,
		BLACKANDWHITE
	};

private:
	ZDATA_(CWindow)
	EMode eMode;
	CVec4 vCoeff;
	CDGPtr<NGScene::CScreenshotTexture> pTexture;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&eMode); f.Add(3,&vCoeff); f.Add(4,&pTexture); return 0; }

public:
	CScreenShot() {}
	CScreenShot( const SWindowInfo &sInfo );
	
	void Set( const CArray2D<NGfx::SPixel8888> &sScreenShot );
	void Generate();

	NGScene::CScreenshotTexture* GetTexture() const;
	void SetTexture( NGScene::CScreenshotTexture* pTexture );

	void SetMode( EMode eMode, const CVec4 &vCoeff = CVec4( 1, 1, 1, 1 ) );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVideoPlayer  --  a CWindow that owns one NGScene::IVideoPlayer (the concrete Bink
// movie player, built via NGScene::CreateVideoPlayer) and plays a movie into it.
// Reconstructed from the release module UICommCtrls.obj (absent here).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CVideoPlayer: public CWindow
{
	OBJECT_NOCOPY_METHODS(CVideoPlayer);
public:
	// Public control flags; Set() remaps each to the player's internal play-flags
	// by a one-bit LEFT shift of bits 0..3.
	enum EPlayFlags
	{
		PLAY_LOOPED         = 1,   // -> 0x02
		PLAY_FROM_MEMORY    = 2,   // -> 0x04
		PLAY_WITH_SOUND     = 4,   // -> 0x08
		PLAY_NO_TIME_UPDATE = 8    // -> 0x10
	};

private:
	ZDATA_(CWindow)
	CDGPtr<NGScene::IVideoPlayer> pTexture;   // +0x80 .pNode (IVideoPlayer*) / +0x84 .nVersion
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pTexture); return 0; }

public:
	CVideoPlayer() {}
	CVideoPlayer( const SWindowInfo &sInfo );

	void Set( const string &szName, int nFlags );   // build + adopt a fresh player

	void Play( bool bRestart );
	void Stop();
	bool IsPlaying();

	void SetFrame( int nFrame );
	int  GetLength();
	int  GetFrameCount();

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
