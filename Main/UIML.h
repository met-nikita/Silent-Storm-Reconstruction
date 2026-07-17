#ifndef __A5_UI_ML_H__
#define __A5_UI_ML_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GLocale.h"
#include "GPixelFormat.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class IML;
class IMLObject;
class IMLLayout;
class CMLStream;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ECommand
{
	CMD_NULL,
	CMD_TAB,
	CMD_SPACE,
	CMD_BREAKLINE
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	FONT_SIZE_MASK			= 0x00FFFFFF,
	FONT_SIZE_PIXELS		= 0x10000000,
	FONT_SIZE_POINTS		= 0x20000000;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SState
{
	enum EHORAlign
	{
		HORALIGN_DEFAULT,
		HORALIGN_LEFT,
		HORALIGN_RIGHT,
		HORALIGN_CENTER,
		HORALIGN_NOWRAP,		// retail value 4 (PDB) -- serialized (SState tag 2 / CMLImageObject tag 5), so the order must match retail
		HORALIGN_JUSTIFY,
		HORALIGN_WRAP_LEFT,
		HORALIGN_WRAP_RIGHT
	};
	enum EVERTAlign
	{
		VERTALIGN_TOP,
		VERTALIGN_BOTTOM,
		VERTALIGN_MIDDLE
	};

	ZDATA
	//// reflow
	EHORAlign eHAlign;
	EVERTAlign eVAlign;
	//// font
	NGScene::SFont sFont;
	NGfx::SPixel8888 sColor;
	//// outline font
	int nOutlineBorder;
	NGfx::SPixel8888 sOutlineColor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&eHAlign); f.Add(3,&eVAlign); f.Add(4,&sFont); f.Add(5,&sColor); f.Add(6,&nOutlineBorder); f.Add(7,&sOutlineColor); f.Add(8,&bForceFontSize); f.Add(9,&nMinFontSize); return 0; }
	bool bForceFontSize = false;
	int nMinFontSize = 0;

	SState(): eHAlign( HORALIGN_DEFAULT ), eVAlign( VERTALIGN_MIDDLE ), sFont( 16, "System" ), sColor( 0xFF, 0xFF, 0xFF, 0xFF ), nOutlineBorder( 0 ), sOutlineColor( 0xFF, 0xFF, 0xFF, 0xFF ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMLStream -- retail NUI::CMLStream (UIML.obj, saveload id 0xB100323D): the shared text stream the
// CML tokenizer parses and the CMLTextObject leaves slice. nPos is the parse caret (handlers insert
// substituted text AT the caret so the tokenizer picks it up inline -- see CVALUEHandler), wsText is
// the full stream text, nSize the cached element count. operator& @0x323360: 2=nPos, 3=wsText
// (string chunk), 4=nSize. Ctors @0x3232f0/@0x324250; GetString @0x320c00; InsertString @0x320c80.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMLStream: public CObjectBase
{
	OBJECT_BASIC_METHODS(CMLStream);
public:
	ZDATA
	int nPos;
	wstring wsText;
	int nSize;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nPos); f.Add(3,&wsText); f.Add(4,&nSize); return 0; }

	CMLStream(): nPos( 0 ), nSize( 0 ) {}

	// retail @0x320c00: copy wsText[nStart, nStart+nCount) into *pRes -- ONLY when the slice is in
	// range under an UNSIGNED bounds test (a negative start/count reads as huge and silently no-ops).
	void GetString( int nStart, int nCount, wstring *pRes ) const
	{
		if ( (unsigned int)( nStart + nCount ) > (unsigned int)wsText.size() )
			return;
		pRes->assign( wsText, nStart, nCount );
	}

	// retail @0x320c80: splice wsInsert into wsText at the caret nPos, refresh the cached count.
	// The caret itself is NOT advanced (faithful to the decomp).
	void InsertString( const wstring &wsInsert )
	{
		wsText = wsText.substr( 0, nPos ) + wsInsert + wsText.substr( nPos );
		nSize = wsText.size();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// SRange / SReflowInfo -- the reflow cursor CMLLayout::Generate threads through the passes. Retail
// NUI::SRange (PDB, 8B) = { float fValue, float fHeight }; retail NUI::SReflowInfo (PDB, 48B) is
// FLOAT-based throughout (fY/fMaxX/fLineWidth/fLineHeight/fLastLineHeight + sLeft/sRight + the
// three object lists) -- the reflow never rounds; only CML::GetSize (@0x320880) converts to pixels.
// Exposed to the objects via IMLObject::DynamicGenerate. (Stack-only in retail -- never serialized.)
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRange
{
	ZDATA
	float fValue;
	float fHeight;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&fValue); f.Add(3,&fHeight); return 0; }

	SRange() {}
	SRange( float _fValue, float _fHeight ): fValue( _fValue ), fHeight( _fHeight ) {}
};
struct SReflowInfo
{
	ZDATA
	float fY;
	float fMaxX;
	float fLineWidth, fLineHeight, fLastLineHeight;
	SRange sLeft, sRight;
	list<CPtr<IMLObject> > line, leftWraped, rightWraped;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&fY); f.Add(3,&fMaxX); f.Add(4,&fLineWidth); f.Add(5,&fLineHeight); f.Add(6,&fLastLineHeight); f.Add(7,&sLeft); f.Add(8,&sRight); f.Add(9,&line); f.Add(10,&leftWraped); f.Add(11,&rightWraped); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IMLObject
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMLObject: public CObjectBase
{
public:
	virtual void Generate( NGScene::I2DGameView *pView ) = 0;

	// retail IMLObject vtbl+0x14: per-reflow sizing against the LIVE reflow cursor -- called by
	// CMLLayout::Generate pass 2 for every object before its size is read. Only CMLTabObject
	// implements it (the pen-relative tab-stop gap); everything else is a no-op.
	virtual void DynamicGenerate( NGScene::I2DGameView *pView, const SReflowInfo &sInfo ) {}

	// retail ML geometry is float end-to-end (CMLTextObject::sSize/sPosition etc. are
	// CTPoint<float> per Game.pdb); GetSize/GetPosition/SetPosition carry the float points.
	virtual const CTPoint<float>& GetSize() const = 0;

	virtual const SState& GetState() const = 0;
	virtual void SetState( const SState &sState ) = 0;

	virtual const CTPoint<float>& GetPosition() const = 0;
	virtual void SetPosition( const CTPoint<float> &sPosition ) = 0;

	// NOTE: retail's Render signatures are float too (list<CTRect<float>>, CTPoint<float>,
	// CTRect<float> -- PDB @0x3207c0/@0x320ac0), because retail's whole 2D pipeline down through
	// I2DGameView::CreateDynamicRects (@0xd8b20) is float. Dev's I2DGameView chain is still
	// int-based, so the int SPoint/SRect boundary is kept HERE; the implementations do the float
	// math internally and narrow only at the CreateDynamicRects / pRender->push_back boundary.
	virtual void Render( list<SRect> *pRender, const SPoint &sGlobalPosition, const SRect &sWindow ) = 0;
	virtual void Render( NGScene::I2DGameView *pView, const SPoint &sPosition, const SRect &sWindow ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// (dev-legacy CreateIMLTextObject(wstring) REMOVED: retail CMLTextObject is stream-based -- text
// objects are built by CML::Generate from CMLStream slices; handlers insert text via the stream.)
IMLObject* CreateIMLImageObject( NDb::CUITexture *pUITexture, SState::EHORAlign eAlign, int nBorder, int nWidth, int nHeight );
IMLObject* CreateIMLTabObject();   // retail NUI::CreateIMLTabObject @0x320ba0 -- the <tab> reflow object (CTABHandler)
////////////////////////////////////////////////////////////////////////////////////////////////////
// IMLLayout
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMLLayout: public CObjectBase
{
public:
	virtual void AddObject( IMLObject *pObject ) = 0;
	virtual void AddCommand( ECommand eCommand, IMLObject *pObject = 0 ) = 0;

	// retail CMLLayout::sSize is CTPoint<float> (PDB +0x38); the int conversion happens only in
	// CML::GetSize (@0x320880), the single float->int boundary.
	virtual const CTPoint<float>& GetSize() const = 0;

	virtual const SState& GetState() = 0;
	virtual void SetState( const SState &sState ) = 0;

	// retail CMLLayout::Generate @0x321fe0 takes a FLOAT width (mangled ...UAEXPAVI2DGameView_
	// NGScene__M_Z); IML::Generate keeps the int width and CML::Generate converts (@0x322320).
	virtual void Generate( NGScene::I2DGameView *pView, float fWidth ) = 0;

	// (int boundary kept, see IMLObject::Render note)
	virtual void Render( list<SRect> *pRender, const SPoint &sPosition, const SRect &sWindow ) = 0;
	virtual void Render( NGScene::I2DGameView *pView, const SPoint &sPosition, const SRect &sWindow ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IMLHandler
////////////////////////////////////////////////////////////////////////////////////////////////////
class IMLHandler: public CObjectBase
{
public:
	// retail Exec carries a leading IML* (the dispatching CML passes itself) so handlers can reach
	// the parse stream (pML->GetStream()->InsertString at the caret) -- e.g. CVALUEHandler.
	virtual void Exec( IML *pML, IMLLayout *pLayout, const vector<wstring> &paramsSet ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IML
////////////////////////////////////////////////////////////////////////////////////////////////////
class IML: public CObjectBase
{
public:
	virtual void SetText( const wstring &wsText, int nFlags ) = 0;
	virtual void SetHandler( const wstring &wsTAG, IMLHandler *pHandler ) = 0;

	// retail IML vtbl+0x18: the parse stream (CML returns its pStream; live only during/after
	// Generate). Handlers use it to inject substituted text at the parse caret.
	virtual CMLStream* GetStream() = 0;

	virtual const SPoint& GetSize() const = 0;

	virtual void Generate( NGScene::I2DGameView *pView, int nWidth ) = 0;

	virtual void Render( list<SRect> *pRender, const SPoint &sPosition, const SRect &sWindow ) = 0;
	virtual void Render( NGScene::I2DGameView *pView, const SPoint &sPosition, const SRect &sWindow ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IML* CreateML();
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif