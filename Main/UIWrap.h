#ifndef __A5_UI_WRAP_H__
#define __A5_UI_WRAP_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFBTransform;
namespace NGScene
{
	class CCTRect;
	class CCWString;
	class CCTPoint;
	class CCRectLayout;
	class ILight;
	class IGameView;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTextDraw
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTextDraw: public CObjectBase
{
	OBJECT_BASIC_METHODS(CTextDraw);
protected:
	ZDATA
	SPoint sSize;
	SPoint sRealSize;
	SPoint sPosition;
	wstring wsText;
	CObj<NGScene::CCWString> pTextString;
	CDGPtr<NGScene::CCTPoint> pSize;
	CDGPtr<CFuncBase<NGScene::SText> > pText;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sSize); f.Add(3,&sRealSize); f.Add(4,&sPosition); f.Add(5,&wsText); f.Add(6,&pTextString); f.Add(7,&pSize); f.Add(8,&pText); return 0; }

protected:
	void UpdateSize( NGScene::I2DGameView *pView );

public:
	CTextDraw( const SPoint &sPosition = SPoint( 0, 0 ), const SPoint &sSize = SPoint( -1, -1 ), const wstring &wsText = L"" );

	const SPoint& GetSize( NGScene::I2DGameView *pView = 0 );
	void SetSize( const SPoint &sSize );

	const SPoint& GetPosition() const;
	void SetPosition( const SPoint &sPosition );

	const wstring& GetText() const;
	void SetText( const wstring &wsText );

	void Draw( CWindow *pWindow, const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CImageDraw
////////////////////////////////////////////////////////////////////////////////////////////////////
class CImageDraw: public CObjectBase
{
	OBJECT_BASIC_METHODS(CImageDraw);
protected:
	ZDATA
	CVec2 vScale;
	SRect sWindow;
	SRect sTextureRect;
	NGfx::SPixel8888 sColor;
	CObj<NGScene::CRects> pRects;
	CObj<NGScene::CCTRect> pWindow;
	CDBPtr<NDb::CUITexture> pUITexture;
	CObj<NGScene::CCRectLayout> pRectLayout;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&vScale); f.Add(3,&sWindow); f.Add(4,&sTextureRect); f.Add(5,&sColor); f.Add(6,&pRects); f.Add(7,&pWindow); f.Add(8,&pUITexture); f.Add(9,&pRectLayout); return 0; }

public:
	CImageDraw( const SRect &sWindow = SRect( 0, 0, 0, 0 ), NDb::CUITexture* pTexture = 0, const SRect &sTexRect = SRect( 0, 0, 0, 0 ), const NGfx::SPixel8888 &sColor = NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ) );

	const SRect& GetWindow();
	void SetWindow( const SRect &sWindow = SRect( 0, 0, 0, 0 ) );

	void SetScale( const CVec2 &vScale );
	void SetColor( const NGfx::SPixel8888 &sColor );
	void SetImage( NDb::CUITexture* pTexture, const SRect &sTexRect = SRect( 0, 0, 0, 0 ) );

	void Draw( CWindow *pWindow, const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CModelDraw
////////////////////////////////////////////////////////////////////////////////////////////////////
class CModelDraw: public CObjectBase
{
	OBJECT_BASIC_METHODS(CModelDraw);
protected:
	ZDATA
	SRect sWindow;
	NGfx::SPixel8888 sColor;
	CPtr<NDb::CModel> pModel;
	CPtr<CFBTransform> pBaseTransform;
	CObj<NGScene::IGameView> p3DView;
	CObj<CObjectBase> pRender;
	CObj<NGScene::CCFBTransform> pTransform;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bUpdated); f.Add(3,&bParentScene); f.Add(4,&sWindow); f.Add(5,&sModelTransform); f.Add(6,&sCameraTransform); f.Add(7,&sColor); f.Add(8,&pModel); f.Add(9,&pRender); f.Add(10,&p3DView); f.Add(11,&pTransform); return 0; }
	bool bUpdated = false;
	bool bParentScene = true;
	SHMatrix sModelTransform;
	SHMatrix sCameraTransform;

public:
	CModelDraw( const SRect &sWindow = SRect( 0, 0, 0, 0 ), NDb::CModel* pModel = 0, CFBTransform *pBaseTransform = 0 );

	const SRect& GetWindow() const;
	void SetWindow( const SRect &sWindow = SRect( 0, 0, 0, 0 ) );

	const NGfx::SPixel8888& GetColor() const;
	void SetColor( const NGfx::SPixel8888 &sColor );

	NDb::CModel* GetModel() const;
	void SetModel( NDb::CModel* pModel );

	CFBTransform* GetTransform() const;
	void SetTransform( CFBTransform *pBaseTransform );

	// iSpecialView (release): setters over the existing fields ( sModelTransform,
	// sCameraTransform, p3DView, bParentScene ) -- additive; existing callers use
	// SetModel/SetTransform and are untouched.
	void SetScene( NGScene::IGameView *pView, bool bFast );
	void SetModelTransform( const SHMatrix &sMatrix );
	void SetCameraTransform( const SHMatrix &sMatrix );

	void Draw( CWindow *pWindow, const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif