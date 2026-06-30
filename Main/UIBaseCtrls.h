#ifndef __A5_UI_BASE_CONTROLS_H__
#define __A5_UI_BASE_CONTROLS_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFBTransform;
struct SHMatrix;
namespace NGScene
{
	class IGameView;
}
namespace NDb
{
	class CModel;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class IML;
class CTextDraw;
class CImageDraw;
class CModelDraw;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CText
////////////////////////////////////////////////////////////////////////////////////////////////////
class CText: public CWindow
{
	OBJECT_NOCOPY_METHODS(CText);
private:
	ZDATA_(CWindow)
	CObj<CTextDraw> pText;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pText); return 0; }

public:
	CText() {}
	CText( const SWindowInfo &sInfo );

	const wstring& GetText() const;
	void SetText( const wstring &wsText );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMLText
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMLText: public CWindow
{
	OBJECT_NOCOPY_METHODS(CMLText);
private:
	ZDATA_(CWindow)
	int nSize;
	wstring wsText;
	CObj<IML> pText;
	unordered_map<wstring,wstring> valuesMap;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&nSize); f.Add(3,&wsText); f.Add(4,&pText); f.Add(5,&valuesMap); return 0; }

protected:
	void UpdateText( NGScene::I2DGameView *pView );

public:
	CMLText() {}
	CMLText( const SWindowInfo &sInfo );

	const wstring& GetText() const;
	void SetText( const wstring &wsText, bool bProcessTAGs = true );

	bool GetVal( const wstring &szID, wstring *pVal );
	void SetVal( const wstring &szID, int nVal );
	void SetVal( const wstring &szID, float fVal );
	void SetVal( const wstring &szID, const wstring &wsVal );

	IML* GetIML();
	void SetUpdated();
	void GetRealSize( SPoint *pRes );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CImage
////////////////////////////////////////////////////////////////////////////////////////////////////
class CImage: public CWindow
{
	OBJECT_NOCOPY_METHODS(CImage);
private:
	ZDATA_(CWindow)
	CObj<CImageDraw> pImage;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pImage); return 0; }

public:
	CImage() {}
	CImage( const SWindowInfo &sInfo );

	void SetScale( const CVec2 &vScale );
	void SetColor( const NGfx::SPixel8888 &sColor );
	void SetImage( NDb::CUITexture* pTexture, const SRect &sTexRect = SRect( 0, 0, 0, 0 ) );
	void SetSizeFromImage( NDb::CUITexture* pTexture );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CModel
////////////////////////////////////////////////////////////////////////////////////////////////////
class CModel: public CWindow
{
	OBJECT_NOCOPY_METHODS(CModel);
private:
	ZDATA_(CWindow)
	CObj<CModelDraw> pModel;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pModel); return 0; }

public:
	CModel() {}
	CModel( const SWindowInfo &sInfo );

	const NGfx::SPixel8888& GetColor() const;
	void SetColor( const NGfx::SPixel8888 &sColor );

	NDb::CModel* GetModel() const;
	void SetModel( NDb::CModel *pModel );

	CFBTransform* GetTransform() const;
	void SetTransform( CFBTransform *pBaseTransform );

	// iSpecialView (release): thin forwarders into the owned CModelDraw over its
	// already-present sModelTransform/sCameraTransform/p3DView/bParentScene fields.
	void SetScene( NGScene::IGameView *pView, bool bFast );
	void SetModelTransform( const SHMatrix &sMatrix );
	void SetCameraTransform( const SHMatrix &sMatrix );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
