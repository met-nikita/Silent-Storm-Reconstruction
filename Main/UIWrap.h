#ifndef __A5_UI_WRAP_H__
#define __A5_UI_WRAP_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFBTransform;
class CTransformStack;
struct SFBTransform;
struct SHMatrix;
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
class IML;   // UIML.h -- the markup layout object CTextDraw owns (retail)
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTextDraw -- retail NUI::CTextDraw (UIWrap.obj, saveload id 0xB0814160). IML-BASED in retail: the
// text is laid out by one owned NUI::IML markup object (pML) instead of the old dev render nodes
// (CCWString/CCTPoint/CFuncBase<SText> -- dropped, convergence W4). nSize caches the layout width
// pushed into pML (0 = force a regenerate). operator& @0x32b300: 2=nSize, 3=sSize, 4=sRealSize,
// 5=sPosition, 6=wsText (string chunk), 7=pML.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTextDraw: public CObjectBase
{
	OBJECT_BASIC_METHODS(CTextDraw);
protected:
	ZDATA
	int nSize;
	SPoint sSize;
	SPoint sRealSize;
	SPoint sPosition;
	wstring wsText;
	CObj<IML> pML;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nSize); f.Add(3,&sSize); f.Add(4,&sRealSize); f.Add(5,&sPosition); f.Add(6,&wsText); f.Add(7,&pML); return 0; }

protected:
	void UpdateText( NGScene::I2DGameView *pView );   // retail @0x329680 (the old dev UpdateSize)

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
	CDBPtr<NDb::CUITexture> pUITexture;
	// retail CImageDraw::operator& @0x32b500 serializes ONLY {2 vScale, 3 sWindow, 4 sTextureRect,
	// 5 sColor, 6 pUITexture} -- and the retail NUI::CImageDraw UDT is 60 bytes with exactly these
	// five members. (The old dev retained-2D working members pRects/pWindow/pRectLayout, which retail
	// does not even have, were removed with the CRects/CPosNode path in the W5 convergence wave;
	// Draw renders from pUITexture directly.)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&vScale); f.Add(3,&sWindow); f.Add(4,&sTextureRect); f.Add(5,&sColor); f.Add(6,&pUITexture); return 0; }

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
// retail UIWrap.obj free helpers behind CModelDraw::Draw:
// MakeProjection @0x329710 -- the fixed UI-item projection (1024x768 virtual screen, FOV 60, z 0.1..300).
// MakeModelTransform @0x329830 -- folds camera+model matrices and the widget's on-screen placement into
// the single SFBTransform the created mesh follows (backward = identity).
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeProjection( CTransformStack *pTS );
void MakeModelTransform( SFBTransform *pRes, const SPoint &sPosition, const SPoint &sSize, const SHMatrix &sCameraTransform, const SHMatrix &sModelTransform );
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
	CObj<NGScene::IGameView> p3DView;
	CObj<CObjectBase> pRender;
	CObj<NGScene::CCFBTransform> pTransform;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bUpdated); f.Add(3,&bParentScene); f.Add(4,&sWindow); f.Add(5,&sModelTransform); f.Add(6,&sCameraTransform); f.Add(7,&sColor); f.Add(8,&pModel); f.Add(9,&pRender); f.Add(10,&p3DView); f.Add(11,&pTransform); return 0; }
	bool bUpdated = false;
	bool bParentScene = true;
	SHMatrix sModelTransform;
	SHMatrix sCameraTransform;

public:
	// retail @0x32a920 (both matrices start IDENTITY -- they are on the wire at tags 5/6)
	CModelDraw( const SRect &sWindow = SRect( 0, 0, 0, 0 ), NDb::CModel* pModel = 0 );

	const SRect& GetWindow() const;
	void SetWindow( const SRect &sWindow = SRect( 0, 0, 0, 0 ) );

	const NGfx::SPixel8888& GetColor() const;
	void SetColor( const NGfx::SPixel8888 &sColor );

	NDb::CModel* GetModel() const;
	void SetModel( NDb::CModel* pModel );

	// retail setters @0x32a390 / @0x3295b0 / @0x3295d0; the matrix setters raise bUpdated and
	// Draw folds both matrices into pTransform via MakeModelTransform.
	void SetScene( NGScene::IGameView *pView, bool bFast );
	void SetModelTransform( const SHMatrix &sMatrix );
	void SetCameraTransform( const SHMatrix &sMatrix );

	void Draw( CWindow *pWindow, const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif