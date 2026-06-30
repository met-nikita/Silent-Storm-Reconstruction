#ifndef __RECTLAYOUT_H_
#define __RECTLAYOUT_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "GPixelFormat.h"

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
	struct SRect
	{
		ZDATA
		float fX, fY;
		STextureCoord sTex;//, sMask;
		NGfx::SPixel8888 sColor;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&fX); f.Add(3,&fY); f.Add(4,&fSizeX); f.Add(5,&fSizeY); f.Add(6,&sTex); f.Add(7,&sColor); return 0; }
	float fSizeX = 0.0f;
	float fSizeY = 0.0f;

		SRect() {}
		SRect( float _fX, float _fY, const STextureCoord &_sTex, const NGfx::SPixel8888 &_sColor )
			:fX(_fX), fY(_fY), sTex(_sTex), sColor(_sColor) {}
	};
	ZDATA
	CTPoint<float> scale;
	vector<SRect> rects;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&scale); f.Add(3,&rects); return 0; }
	//
	CRectLayout(): scale( 1, 1 ) {}
	void AddRect( float fScreenX, float fScreenY, const STextureCoord &sTex, const NGfx::SPixel8888 &color = NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ) )
	{
		rects.push_back( SRect( fScreenX, fScreenY, sTex, color ) );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif