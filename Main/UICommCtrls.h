#ifndef __A5_UI_COMMON_CONTROLS_H__
#define __A5_UI_COMMON_CONTROLS_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "../DBFormat/DataSound.h"
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
	EVENT_LISTVIEW_ITEMSELECTED	=	0x00000100 | EVENT_FLAG_NOTIFY;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Common controls flags
const int
// ListView flags
	ELV_ITEMSSELECTED_TRUE		= 0x000000001,
	ELV_ITEMSSELECTED_FALSE		= 0x000000000;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEdit
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEdit: public CWindow
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
	ZDATA_(CWindow)
	int nSize;
	int nCursor;
	EMode eMode;
	bool bActiveState;
	bool bCursorVisible;
	SRect sTexRect;
	STime sFlashTime;
	wstring wsText;
	wstring wsFormat;
	////
	SCursorInfo sCursorInfo;
	CObj<NGScene::CCTPoint> pSize;
	CObj<NGScene::CCWString> pTextString;
	CDGPtr< CFuncBase<NGScene::SText> > pText;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&nSize); f.Add(3,&nCursor); f.Add(4,&eMode); f.Add(5,&bActiveState); f.Add(6,&bCursorVisible); f.Add(7,&sTexRect); f.Add(8,&sFlashTime); f.Add(9,&wsText); f.Add(10,&wsFormat); f.Add(11,&sCursorInfo); f.Add(12,&pSize); f.Add(13,&pTextString); f.Add(14,&pText); return 0; }
	
public:
	CEdit() {}
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
// CToolTip
////////////////////////////////////////////////////////////////////////////////////////////////////
class CToolTip: public CWindow
{
	OBJECT_NOCOPY_METHODS(CToolTip);
private:
	ZDATA_(CWindow)
	CObj<CMLText> pText;
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
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pText); f.Add(3,&pBackgroundUp); f.Add(4,&pBackgroundDown); f.Add(5,&pBackgroundLeft); f.Add(6,&pBackgroundRight); f.Add(7,&pBackgroundMiddle); f.Add(8,&pBackgroundUpLeft); f.Add(9,&pBackgroundUpRight); f.Add(10,&pBackgroundDownLeft); f.Add(11,&pBackgroundDownRight); return 0; }

protected:
	void DrawBackground( const STime &sTime, NGScene::I2DGameView *pView );

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
		CObj<CWindow> pWindow;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nID); f.Add(3,&pWindow); return 0; }

		SItem(): nID( -1 ) {}
		SItem( int _nID, CWindow *_pWindow ): nID( _nID ), pWindow( _pWindow ) {}
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
