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
#include "..\MiscDll\LogStream.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInterface;
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
	string szID;
	SPoint sSize;
	SPoint sPosition;
	SCursorInfo sInfo;
	CPtr<CWindow> pParent;
	CObj<CWindow> pToolTip;
	CPtr<CWindow> pMouseFocus;
	CPtr<CInterface> pInterface;
	list<CMObj<CWindow> > listChildren;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nStyle); f.Add(3,&bActive); f.Add(4,&szID); f.Add(5,&sSize); f.Add(6,&sPosition); f.Add(7,&sInfo); f.Add(8,&pParent); f.Add(9,&pToolTip); f.Add(10,&pMouseFocus); f.Add(11,&pInterface); f.Add(12,&listChildren); return 0; }

protected:
	void ActivateTest( int nX, int nY );
	void BringWindowToTop( CWindow *pWindow );
	void FormChildrenList( list<CPtr<CWindow> > *pList );
	
public:
	CWindow() {}
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

	CWindow* GetToolTip() const;
	void SetToolTip( CWindow *pWindow );

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
