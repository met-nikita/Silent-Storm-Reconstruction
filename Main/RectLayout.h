#ifndef __RECTLAYOUT_H_
#define __RECTLAYOUT_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "GPixelFormat.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// CRectLayout -- retail (Game.pdb) is 12 bytes = { vector<SRect> rects } ONLY. The old dev
// CTPoint<float> scale member does NOT exist in retail: the quad size is baked into each SRect
// (fSizeX/fSizeY, PDB +0x08/+0x0c) by the 6-arg AddRect (retail @0x174620) at generate time, and
// the renderers consume the per-rect size directly (RenderRectLayout @0x1480b0). This is also the
// wire format: CRectLayout::operator& @0x16f0d0 = { 2 = rects (DoVector<SRect>) } -- the dev
// { 2 scale, 3 rects } table read retail's tag-2 vector chunk as 8 raw bytes of scale and left
// rects EMPTY (mission-UI button captions vanished after loading retail v1.2 saves).
// (The software renderer's NGScene::CSWRectLayout is a DIFFERENT class and DOES keep scale --
// PDB size 20 -- so SWRectLayout.h/GTerrainTexture/2DSceneSW stay as they are.)
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRectLayout
{
public:
	struct STextureCoord
	{
		ZDATA
		CTRect<float> rcTexRect;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&rcTexRect); return 0; }

		STextureCoord() {}
		STextureCoord( const CTRect<float> &_rcTexRect ): rcTexRect( _rcTexRect ) {}
		int GetWidth() const { return (int)rcTexRect.Width(); }
		int GetHeight() const { return (int)rcTexRect.Height(); }
	};
	// retail CRectLayout::SRect (PDB, 36 bytes): +0x00 fX, +0x04 fY, +0x08 fSizeX, +0x0c fSizeY,
	// +0x10 sTex (16B), +0x20 sColor. operator& @0x16f1b0: 2=fX, 3=fY, 4=fSizeX, 5=fSizeY, 6=sTex,
	// 7=sColor (slot-1 byte-walk proof: each vector element chunk = 50B {2:4 3:4 4:4 5:4 6:18 7:4}).
	struct SRect
	{
		ZDATA
		float fX, fY;
		float fSizeX, fSizeY;
		STextureCoord sTex;//, sMask;
		NGfx::SPixel8888 sColor;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&fX); f.Add(3,&fY); f.Add(4,&fSizeX); f.Add(5,&fSizeY); f.Add(6,&sTex); f.Add(7,&sColor); return 0; }

		SRect() {}
		SRect( float _fX, float _fY, float _fSizeX, float _fSizeY, const STextureCoord &_sTex, const NGfx::SPixel8888 &_sColor )
			:fX(_fX), fY(_fY), fSizeX(_fSizeX), fSizeY(_fSizeY), sTex(_sTex), sColor(_sColor) {}
	};
	ZDATA
	vector<SRect> rects;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&rects); return 0; }   // retail @0x16f0d0: { 2 = rects } only
	//
	CRectLayout() {}
	// retail @0x174620: the ONLY AddRect -- 6-arg, bakes the quad size into the rect.
	void AddRect( float fScreenX, float fScreenY, float fSizeX, float fSizeY, const STextureCoord &sTex, const NGfx::SPixel8888 &color = NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ) )
	{
		rects.push_back( SRect( fScreenX, fScreenY, fSizeX, fSizeY, sTex, color ) );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
