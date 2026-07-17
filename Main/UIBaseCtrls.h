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
// CText -- retail NUI::CText (UIBaseCtrls.obj, saveload id 0xB0241952). In retail this IS the
// evolved CMLText: it owns the markup source text + one NUI::IML layout object + the <value>
// substitution table, and no longer delegates to a CTextDraw. operator& @0x218720: 1=CWindow base,
// 2=nSize, 3=wsText (string chunk), 4=pText (CObj<IML>), 5=valuesMap. Base of retail CEdit et al.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CText: public CWindow
{
	OBJECT_NOCOPY_METHODS(CText);
private:
	ZDATA_(CWindow)
	int nSize;
	wstring wsText;
	CObj<IML> pText;
	unordered_map<wstring,wstring> valuesMap;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&nSize); f.Add(3,&wsText); f.Add(4,&pText); f.Add(5,&valuesMap); return 0; }

protected:
	void UpdateText( NGScene::I2DGameView *pView );   // retail @0x312480

public:
	CText() {}
	CText( const SWindowInfo &sInfo );                // retail @0x312b60

	const wstring& GetText() const;                   // retail @0x312390
	void SetText( const wstring &wsText, bool bProcessTAGs = true );   // retail @0x3128d0

	bool GetVal( const wstring &szID, wstring *pVal );   // retail @0x312910
	void SetVal( const wstring &szID, int nVal );        // retail @0x312c80
	void SetVal( const wstring &szID, float fVal );      // retail @0x312cf0
	void SetVal( const wstring &szID, const wstring &wsVal );   // retail @0x312d60

	IML* GetIML();                                    // retail @0x312470
	void SetUpdated();                                // retail @0x3123a0
	void GetRealSize( SPoint *pRes );                 // retail @0x312760

	bool ProcessMessage( const SEvent &sEvent );      // retail @0x312950
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );   // retail @0x312820
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

	// retail thin forwarders into the owned CModelDraw @0x312670/0x312680/0x312690
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
