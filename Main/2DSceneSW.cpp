#include "StdAfx.h"
#include "GfxBuffers.h"
#include "2DSceneSW.h"
#include "SWTexture.h"
#include "Render.h"
#include "..\Misc\BasicShare.h"
#include "..\DBFormat\DataFormat.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
static CBasicShare<int, CSWTexture> shareSWTextures(129);
CPtrFuncBase<CSWTextureData>* GetSWTex( NDb::CTexture *pTex ) 
{
	CPtrFuncBase<CSWTextureData> *pResult = shareSWTextures.Get( pTex->GetRecordID() );
	return pResult;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T, class TElement>
class CSomeRender: public CArrayRasterizer<T, TElement>
{
public:
	CSomeRender(): fScale(1) {}
	bool DoRenderBackface() const { return true; }
	void SetMaskMapping( const CVec4 &_ptDU, const CVec4 &_ptDV )
	{
		maskMapping.SetGradient( _ptDU, _ptDV );
	}
	void SetTexture( CSWTextureData &tex )
	{
		pTexture = &tex;
		pMaskTexture = 0;
		bDoMask = 0;
	}
	void SetMask( CSWTextureData &tex )
	{
		pMaskTexture = &tex;
	}
	void SetMask() { bDoMask = true; }
	void SetScale( float _fScale ) { fScale = _fScale; }
	float GetScale() const { return fScale; }
protected:
	STextureMapping maskMapping;
	CSWTextureData *pTexture, *pMaskTexture;
	bool bDoMask;
	float fScale;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
struct STextureIterator
{
	unsigned int nU, nDU, nV, nDV, nVShift;
	int nUMask, nVMask;
	unsigned int nMip;
	T *pMip;

	void Init( float fU, float fV, float fDU, float fDV, const vector< CArray2D<T> > &tex )
	{
		int nXSize = tex[0].GetXSize(), nYSize = tex[0].GetYSize();
		nMip = 0;
		while ( ( fabs(fDU) > 1.9 || fabs(fDV) > 1.9 ) && nMip < tex.size() - 1 )
		{
			fDU /= 2; fDV /= 2; fU /= 2; fV /= 2; nMip++; nXSize /= 2; nYSize /= 2;
		}
		nU = Float2Int( fU * 0x10000 );
		nV = Float2Int( fV * 0x10000 );
		nDU = Float2Int( fDU * 0x10000 );
		nDV = Float2Int( fDV * 0x10000 );
		int nXBits = GetMSB( nXSize );
		nVShift = 16 - nXBits;
		nUMask = nXSize - 1;
		nVMask = (nYSize - 1 ) << nXBits;
		pMip = &tex[nMip][0][0];
	}
	__forceinline const T& Fetch()
	{
		const T &r = pMip[((nV>>nVShift)&nVMask) + ((nU>>16)&nUMask) ];
		nU += nDU;
		nV += nDV;
		return r;
	}
	// slow variant for special rare case
	template<class TT>
	__forceinline const TT& Fetch( const vector< CArray2D<TT> > &tex )
	{
		TT *pData = &tex[nMip][0][0];
		const TT &r = pData[((nV>>nVShift)&nVMask) + ((nU>>16)&nUMask) ];
		nU += nDU;
		nV += nDV;
		return r;
	}
};
class CTrueColorRender;
class CTrueColorRender: public CSomeRender<CTrueColorRender, NGfx::SPixel8888>
{
public:
private:
	void RasterSpan( int nY, int nLeft, int nRight, float fZ, float fDZ, int nBackface )
	{
		STextureIterator<NGfx::SPixel8888> tex, mask;
		float fU = texMapping.ptDU.w + nLeft * texMapping.ptDU.x + nY * texMapping.ptDU.y;
		float fV = texMapping.ptDV.w + nLeft * texMapping.ptDV.x + nY * texMapping.ptDV.y;
		tex.Init( fU, fV, texMapping.ptDU.x, texMapping.ptDV.x, pTexture->mips );
		NGfx::SPixel8888 *pRow = &res[nY][0];
		NGfx::SPixel8888 *pDst = pRow + nLeft, *pFinish = pRow + nRight;
		if ( pMaskTexture )
		{
			if ( bDoMask )
			{
				mask.Init( fU, fV, texMapping.ptDU.x, texMapping.ptDV.x, pMaskTexture->mips );
				for ( ; pDst < pFinish; ++pDst )
				{
					const NGfx::SPixel8888 color = tex.Fetch();
					const NGfx::SPixel8888 maskColor = mask.Fetch();
					NGfx::SPixel8888 &dst = *pDst;
					int a = maskColor.a;
					dst.r = ( color.r * a + dst.r * ( 256 - a ) ) >> 8;
					dst.g = ( color.g * a + dst.g * ( 256 - a ) ) >> 8;
					dst.b = ( color.b * a + dst.b * ( 256 - a ) ) >> 8;
				}
			}
			else
			{
				float fMaskU = maskMapping.ptDU.w + nLeft * maskMapping.ptDU.x + nY * maskMapping.ptDU.y;
				float fMaskV = maskMapping.ptDV.w + nLeft * maskMapping.ptDV.x + nY * maskMapping.ptDV.y;
				mask.Init( fMaskU, fMaskV, maskMapping.ptDU.x, maskMapping.ptDV.x, pMaskTexture->mips );
				for ( ; pDst < pFinish; ++pDst )
				{
					const NGfx::SPixel8888 &m = mask.Fetch();
					const NGfx::SPixel8888 &color = tex.Fetch();
					NGfx::SPixel8888 &dst = *pDst;
					//m.r = m.g = m.b = m.a;
					dst.r = ( color.r * m.r + dst.r * ( 256 - m.r ) ) >> 8;
					dst.g = ( color.g * m.g + dst.g * ( 256 - m.g ) ) >> 8;
					dst.b = ( color.b * m.b + dst.b * ( 256 - m.b ) ) >> 8;
				}
			}
		}
		else
		{
			if ( bDoMask )
			{
				_asm
				{
					pxor mm2, mm2
				}
				for ( ; pDst < pFinish; ++pDst )
				{
					DWORD color = tex.Fetch().color;
					__asm
					{
						mov esi, pDst
						movd mm0, color
						movd mm1, [esi]
						movq mm3, mm0
						punpcklbw mm3, mm3
						punpcklbw mm0, mm2
						punpckhwd mm3, mm3
						punpcklbw mm1, mm2
						punpckhdq mm3, mm3
						paddw mm0, mm1
						psllw mm1, 1
						psrlw mm3, 1
						pmulhw mm1, mm3
						psubw mm0, mm1
						packuswb mm0, mm0
						movd [esi], mm0
					}
					//const NGfx::SPixel8888 &color = tex.Fetch();
					//NGfx::SPixel8888 &dst = *pDst;
					//int a = color.a;
					//dst.r = color.r + ( ( dst.r * ( 256 - a ) ) >> 8 );
					//dst.g = color.g + ( ( dst.g * ( 256 - a ) ) >> 8 );
					//dst.b = color.b + ( ( dst.b * ( 256 - a ) ) >> 8 );
					//dst.a = color.a + ( ( dst.a * ( 256 - a ) ) >> 8 );
				}
				__asm emms
			}
			else
			{
				for ( ; pDst < pFinish; ++pDst )
					*pDst = tex.Fetch();
			}
		}
	}
	friend class CRasterizer<CTrueColorRender>;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBumpRender;
class CBumpRender : public CSomeRender<CBumpRender, SBumpPixel>
{
private:
	void RasterSpan( int nY, int nLeft, int nRight, float fZ, float fDZ, int nBackface )
	{
		pTexture->PrepareBump();
		const float fDiv = 1.0f / 256;
		STextureIterator<SBumpPixel> tex;
		STextureIterator<NGfx::SPixel8888> mask;
		float fU = texMapping.ptDU.w + nLeft * texMapping.ptDU.x + nY * texMapping.ptDU.y;
		float fV = texMapping.ptDV.w + nLeft * texMapping.ptDV.x + nY * texMapping.ptDV.y;
		tex.Init( fU, fV, texMapping.ptDU.x, texMapping.ptDV.x, pTexture->bumpMips );
		if ( pMaskTexture )
		{
			if ( bDoMask )
			{
				mask.Init( fU, fV, texMapping.ptDU.x, texMapping.ptDV.x, pMaskTexture->mips );
				for ( int x = nLeft; x < nRight; ++x )
				{
					const SBumpPixel &color = tex.Fetch();
					const NGfx::SPixel8888 &maskColor = mask.Fetch();
					SBumpPixel &dst = res[nY][x];
					int a = maskColor.a;
					dst.fDU = ( color.fDU * a + dst.fDU * ( 256 - a ) ) * fDiv;
					dst.fDV = ( color.fDV * a + dst.fDV * ( 256 - a ) ) * fDiv;
				}
			}
			else
			{
				float fMaskU = maskMapping.ptDU.w + nLeft * maskMapping.ptDU.x + nY * maskMapping.ptDU.y;
				float fMaskV = maskMapping.ptDV.w + nLeft * maskMapping.ptDV.x + nY * maskMapping.ptDV.y;
				mask.Init( fMaskU, fMaskV, maskMapping.ptDU.x, maskMapping.ptDV.x, pMaskTexture->mips );
				for ( int x = nLeft; x < nRight; ++x )
				{
					const SBumpPixel &color = tex.Fetch();
					const NGfx::SPixel8888 &m = mask.Fetch();
					SBumpPixel &dst = res[nY][x];
					int a = m.r;
					dst.fDU = ( color.fDU * a + dst.fDU * ( 256 - a ) ) * fDiv;
					dst.fDV = ( color.fDV * a + dst.fDV * ( 256 - a ) ) * fDiv;
				}
			}
		}
		else
		{
			if ( bDoMask )
			{
				for ( int x = nLeft; x < nRight; ++x )
				{
					const NGfx::SPixel8888 &fc = tex.Fetch( pTexture->mips );
					const SBumpPixel &color = tex.Fetch();
					SBumpPixel &dst = res[nY][x];
					int a = fc.a;
					dst.fDU = ( color.fDU * a + dst.fDU * ( 256 - a ) ) * fDiv;
					dst.fDV = ( color.fDV * a + dst.fDV * ( 256 - a ) ) * fDiv;
				}
			}
			else
			{
				for ( int x = nLeft; x < nRight; ++x )
				{
					const SBumpPixel &color = tex.Fetch();
					res[nY][x] = color;
				}
			}
		}
	}
	friend class CRasterizer<CBumpRender>;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFogRender;
class CFogRender: public CSomeRender<CFogRender, int>
{
private:
	void RasterSpan( int nY, int nLeft, int nRight, float fZ, float fDZ, int nBackface )
	{
		const float fDiv = 1.0f / 255;
		STextureIterator<NGfx::SPixel8888> tex;
		float fU = texMapping.ptDU.w + nLeft * texMapping.ptDU.x + nY * texMapping.ptDU.y;
		float fV = texMapping.ptDV.w + nLeft * texMapping.ptDV.x + nY * texMapping.ptDV.y;
		tex.Init( fU, fV, texMapping.ptDU.x, texMapping.ptDV.x, pTexture->mips );

		for ( int x = nLeft; x < nRight; ++x )
		{
			const NGfx::SPixel8888 &color = tex.Fetch();
			res[nY][x] = Min( res[nY][x] , (int)( 0xFF - color.r ) );// ( ( res[nY][x] * color.r ) >> 8 ); 
		}
	}
	friend class CRasterizer<CFogRender>;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// SW Rects
////////////////////////////////////////////////////////////////////////////////////////////////////
class ISWRects: public CObjectBase
{
public:
	virtual void Draw( CTrueColorRender *pRes ) = 0;
	virtual void Draw( CBumpRender *pRes ) = 0;
	virtual void Draw( CFogRender *pRes ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSWRects
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSWRects: public ISWRects
{
	OBJECT_BASIC_METHODS(CSWRects);
	template<class T, class TElement>
		void DrawRect( CSomeRender<T, TElement> *pRes )
	{
		pLayout.Refresh();
		const CSWRectLayout &sRects = pLayout->GetValue();
//		ASSERT( sRects.scale.x == 1 );
//		ASSERT( sRects.scale.y == 1 );
		CSWTextureData *pMaskData = 0, *pTextureData;
		pTexture.Refresh();
		pTextureData = pTexture->GetValue();
		if ( IsValid( pMask ) )
		{
			pMask.Refresh();
			pMaskData = pMask->GetValue();
		}
		for ( size_t nTemp = 0; nTemp < sRects.rects.size(); nTemp++ )
		{
			const CSWRectLayout::SRect &r = sRects.rects[nTemp];
			CVec3 vGeom[4], vMaskRect[4], vTextureRect[4];
			ApplyRectOrient( r.sTex.eOrient, r.sTex.rcTexRect, vTextureRect );
			float fWidth = r.sTex.GetWidth() * sRects.scale.x;
			float fHeight = r.sTex.GetHeight() * sRects.scale.y;
			vGeom[0] = CVec3( r.nX, r.nY, 0 );
			vGeom[1] = CVec3( r.nX, r.nY + fHeight, 0 );
			vGeom[2] = CVec3( r.nX + fWidth, r.nY + fHeight, 0 );
			vGeom[3] = CVec3( r.nX + fWidth, r.nY, 0 );
			for ( int i = 0; i < 4; ++i )
				vGeom[i] *= pRes->GetScale();
			SGradientMatrix gm;
			PrepareGradientMatrix( &gm, vGeom[0], vGeom[1], vGeom[2] );
			CVec4 ptDU, ptDV;
			CalcGradient( gm, &ptDU, vTextureRect[0].x, vTextureRect[1].x, vTextureRect[2].x );
			CalcGradient( gm, &ptDV, vTextureRect[0].y, vTextureRect[1].y, vTextureRect[2].y );
			pRes->SetTextureMapping( ptDU, ptDV );
			pRes->SetTexture( *pTextureData );
			if ( pMaskData )
			{
				CVec4 ptMaskDU, ptMaskDV;
				ApplyRectOrient( r.sMask.eOrient, r.sMask.rcTexRect, vMaskRect );
				CalcGradient( gm, &ptMaskDU, vMaskRect[0].x, vMaskRect[1].x, vMaskRect[2].x );
				CalcGradient( gm, &ptMaskDV, vMaskRect[0].y, vMaskRect[1].y, vMaskRect[2].y );
				pRes->SetMaskMapping( ptMaskDU, ptMaskDV );
				pRes->SetMask( *pMaskData );
			}
			pRes->RasterNoClip( vGeom[0], vGeom[1], vGeom[2] );
			pRes->RasterNoClip( vGeom[0], vGeom[2], vGeom[3] );
		}
	}
public:
	ZDATA
	CDGPtr<CPtrFuncBase<CSWTextureData> > pMask, pTexture;
	CDGPtr< CFuncBase<CSWRectLayout> > pLayout;
	//float fGain;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pMask); f.Add(3,&pTexture); f.Add(4,&pLayout); return 0; }
	CSWRects() {}
	CSWRects( CFuncBase<CSWRectLayout> *_pLayout, CPtrFuncBase<CSWTextureData> *_pTexture, CPtrFuncBase<CSWTextureData> *_pMask = 0 )
		: pLayout(_pLayout), pTexture(_pTexture), pMask(_pMask) {}
	void Draw( CTrueColorRender *pRes ) { DrawRect( pRes ); }
	void Draw( CBumpRender *pRes ) { DrawRect( pRes ); }
	void Draw( CFogRender *pRes ) { DrawRect( pRes ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSWSpot
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSWSpot: public ISWRects
{
	OBJECT_BASIC_METHODS(CSWSpot);
	template<class T, class TElement>
		void DrawSpot( CSomeRender<T, TElement> *pRes )
	{
		CSWTextureData *pTextureData;
		pTexture.Refresh();
		pTextureData = pTexture->GetValue();

		CVec3 vGeom[4];
		float fCos = cos( fAngle ), fSin = sin( fAngle );
		CVec3 ptRight(  ptSize.x * fCos, ptSize.x * fSin, 0 );
		CVec3 ptUp   ( -ptSize.y * fSin, ptSize.y * fCos, 0 );
		CVec3 ptCenter( ptPos, 0 );
		vGeom[0] = ptCenter;
		vGeom[1] = ptCenter + ptUp;
		vGeom[2] = ptCenter + ptUp + ptRight;
		vGeom[3] = ptCenter + ptRight;
		for ( int i = 0; i < 4; ++i )
			vGeom[i] *= pRes->GetScale();
		SGradientMatrix gm;
		PrepareGradientMatrix( &gm, vGeom[0], vGeom[1], vGeom[2] );
		CVec4 ptDU, ptDV;
		float fTexXSize = (float)pTextureData->GetXSize();
		float fTexYSize = (float)pTextureData->GetYSize();
		CalcGradient( gm, &ptDU, 0, 0, fTexXSize );
		CalcGradient( gm, &ptDV, 0, fTexYSize, fTexYSize );
		pRes->SetTextureMapping( ptDU, ptDV );
		pRes->SetTexture( *pTextureData );
		pRes->SetMask();
		if ( pMask )
		{
			pMask.Refresh();
			pRes->SetMask( *pMask->GetValue() );
		}
		pRes->RasterNoClip( vGeom[0], vGeom[1], vGeom[2] );
		pRes->RasterNoClip( vGeom[0], vGeom[2], vGeom[3] );
	}
public:
	ZDATA
	CDGPtr<CPtrFuncBase<CSWTextureData> > pTexture, pMask;
	float fAngle;
	CVec2 ptPos, ptSize;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTexture); f.Add(3,&pMask); f.Add(4,&fAngle); f.Add(5,&ptPos); f.Add(6,&ptSize); return 0; }
	CSWSpot() {}
	CSWSpot( const CVec2 &_ptPos, const CVec2 &_ptSize, float _fAngle,
		CPtrFuncBase<CSWTextureData> *_pTexture, CPtrFuncBase<CSWTextureData> *_pMask )
		: ptPos(_ptPos), ptSize(_ptSize), fAngle(_fAngle), pTexture(_pTexture), pMask(_pMask) {}
	void Draw( CTrueColorRender *pRes ) { DrawSpot( pRes ); }
	void Draw( CBumpRender *pRes ) { DrawSpot( pRes ); }
	void Draw( CFogRender *pRes ) { DrawSpot( pRes ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Software 2D Scene
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSW2DScene: public ISW2DScene
{
	OBJECT_BASIC_METHODS(CSW2DScene);
private:
	ZDATA
	list< CPtr<ISWRects> > rects;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&rects); return 0; }

	template<class T>
	void DrawScene( T *pRes )
	{
		for ( list< CPtr<ISWRects> >::iterator iTemp = rects.begin(); iTemp != rects.end(); )
		{
			ISWRects *pRect = *iTemp;
			if ( IsValid( pRect ) )
			{
				pRect->Draw( pRes );
				++iTemp;
			}
			else
				iTemp = rects.erase( iTemp );
		}
	}
	template<class T>
		void DrawScene( T *pRes, NGfx::CTexture *pTarget, const CTPoint<int> &vViewport )
	{
		CDynamicCast<NGfx::I2DBuffer> p2DBuffer( pTarget );
		float fScale = 1;
		int nXSize = vViewport.x, nYSize = vViewport.y;
		while ( nXSize > p2DBuffer->GetXSize() )
		{
			nXSize /= 2; nYSize /= 2; fScale *= 0.5f;
		}
		CTRect<int> dstRegion( 0, 0, nXSize, nYSize );
		pRes->SetRegion( dstRegion );
		pRes->SetScale( fScale );
		DrawScene( pRes );
	}
public:
	ISWRects* CreateRects( CPtrFuncBase<CSWTextureData> *pTexture, CFuncBase<CSWRectLayout> *pLayout );
	ISWRects* CreateRects( CPtrFuncBase<CSWTextureData> *pTexture, CPtrFuncBase<CSWTextureData> *pMask, CFuncBase<CSWRectLayout> *pLayout );
	ISWRects* CreateSpot( CPtrFuncBase<CSWTextureData> *pTexture, CPtrFuncBase<CSWTextureData> *pMask, const CVec2 &_ptPos, const CVec2 &_ptSize, float _fAngle );
	void Draw( NGfx::CTexture *pTarget, const CTPoint<int> &vViewport );
	void DrawFog( CArray2D<int> *pFogMap, const CTPoint<int> &vViewport );
	void DrawBump( NGfx::CTexture *pTarget, const CTPoint<int> &vViewport, float fScale );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Create scene
////////////////////////////////////////////////////////////////////////////////////////////////////
ISW2DScene* Make2DSWScene()
{
	return new CSW2DScene;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSW2DScene
////////////////////////////////////////////////////////////////////////////////////////////////////
ISWRects* CSW2DScene::CreateRects( CPtrFuncBase<CSWTextureData> *pTexture, CFuncBase<CSWRectLayout> *pLayout )
{
	CPtr<CFuncBase<CSWRectLayout> > pHold(pLayout);
	if ( !pTexture )
		return 0;
	CSWRects *pRes = new CSWRects( pLayout, pTexture );
	rects.push_back( pRes );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
ISWRects* CSW2DScene::CreateRects( CPtrFuncBase<CSWTextureData> *pTexture, CPtrFuncBase<CSWTextureData> *pMask, CFuncBase<CSWRectLayout> *pLayout )
{
	CPtr<CFuncBase<CSWRectLayout> > pHold(pLayout);
	if ( !pTexture )
		return 0;
	CSWRects *pRes;
	pRes = new CSWRects( 
		pLayout, 
		pTexture,
		pMask );
	rects.push_back( pRes );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
ISWRects* CSW2DScene::CreateSpot( CPtrFuncBase<CSWTextureData> *pTexture, CPtrFuncBase<CSWTextureData> *pMask,
	const CVec2 &_ptPos, const CVec2 &_ptSize, float _fAngle )
{
	if ( !pTexture )
		return 0;
	CSWSpot *pSpot = new CSWSpot( _ptPos, _ptSize, _fAngle, pTexture, pMask );
	rects.push_back( pSpot );
	return pSpot;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CalcMip( CArray2D<NGfx::SPixel8888> *pRes )
{
	CArray2D<NGfx::SPixel8888> src = *pRes;
	int nXSize = pRes->GetXSize() / 2;
	int nYSize = pRes->GetYSize() / 2;
	pRes->SetSizes( nXSize, nYSize );
	for ( int y = 0; y < nYSize; ++y )
	{
		for ( int x = 0; x < nXSize; ++x )
		{
			const NGfx::SPixel8888 &s = src[y*2][x*2];
			const NGfx::SPixel8888 &s1 = src[y*2+1][x*2];
			const NGfx::SPixel8888 &s2 = src[y*2][x*2+1];
			const NGfx::SPixel8888 &s3 = src[y*2+1][x*2+1];
			(*pRes)[y][x] = NGfx::SPixel8888( 
				(unsigned char)((int(s.r) + s1.r + s2.r + s3.r)>>2),
				(unsigned char)((int(s.g) + s1.g + s2.g + s3.g)>>2),
				(unsigned char)((int(s.b) + s1.b + s2.b + s3.b)>>2), 0 );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSW2DScene::Draw( NGfx::CTexture *pTarget, const CTPoint<int> &vViewport )
{
	CTrueColorRender r;
	DrawScene( &r, pTarget, vViewport );
	//
	CDynamicCast<NGfx::I2DBuffer> p2DBuffer( pTarget );
	for ( int nMip = 0; nMip < p2DBuffer->GetNumMipLevels(); ++nMip )
	{
		NGfx::CTextureLock<NGfx::SPixel8888> lock( pTarget, nMip, NGfx::INPLACE );
		for ( int y = 0; y < lock.GetYSize(); ++y )
		{
			for ( int x = 0; x < lock.GetXSize(); ++x )
				lock[y][x] = r.res[y][x];
		}
		CalcMip( &r.res );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSW2DScene::DrawFog( CArray2D<int> *pFogMap, const CTPoint<int> &vViewport )
{
	CFogRender r;

	float fScale = 1;
	CTRect<int> dstRegion( 0, 0, pFogMap->GetXSize(), pFogMap->GetYSize() );
	r.SetRegion( dstRegion );
	r.SetScale( fScale );
	r.res = (*pFogMap);
	DrawScene( &r );

	for ( int y = 0; y < pFogMap->GetYSize(); ++y )
	{
		for ( int x = 0; x < pFogMap->GetXSize(); ++x )
			(*pFogMap)[y][x] = r.res[y][x];
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CalcMip( CArray2D<SBumpPixel> *pRes )
{
	CArray2D<SBumpPixel> src = *pRes;
	int nXSize = pRes->GetXSize() / 2;
	int nYSize = pRes->GetYSize() / 2;
	pRes->SetSizes( nXSize, nYSize );
	for ( int y = 0; y < nYSize; ++y )
	{
		for ( int x = 0; x < nXSize; ++x )
		{
			(*pRes)[y][x].fDU = 0.25f * ( src[y*2][x*2].fDU + src[y*2+1][x*2].fDU + src[y*2][x*2+1].fDU + src[y*2+1][x*2+1].fDU );
			(*pRes)[y][x].fDV = 0.25f * ( src[y*2][x*2].fDV + src[y*2+1][x*2].fDV + src[y*2][x*2+1].fDV + src[y*2+1][x*2+1].fDV );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CalcNormals( const CArray2D<SBumpPixel> &m, NGfx::CTexture *pTarget, int nMip, float fScale )
{
	NGfx::CTextureLock<NGfx::SPixel8888> lock( pTarget, nMip, NGfx::INPLACE );
	ASSERT( m.GetXSize() == lock.GetXSize() && m.GetYSize() == lock.GetYSize() );
	for ( int y = 0; y < m.GetYSize(); ++y )
	{
		for ( int x = 0; x < m.GetXSize(); ++x )
		{
			CVec3 vNormal( m[y][x].fDU, m[y][x].fDV, 1 );
			Normalize( &vNormal );
			lock[y][x] = NGfx::SPixel8888( 
				Float2Int( vNormal.x * 127 + 128 ), 
				Float2Int( vNormal.y * 127 + 128 ), 
				Float2Int( vNormal.z * 127 + 128 ), 255 );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSW2DScene::DrawBump( NGfx::CTexture *pTarget, const CTPoint<int> &vViewport, float _fBumpScale )
{
	float fBumpScale = _fBumpScale;
	CBumpRender r;
	DrawScene( &r, pTarget, vViewport );
	fBumpScale *= r.GetScale();
	//
	CDynamicCast<NGfx::I2DBuffer> p2DBuffer( pTarget );
	for ( int nMip = 0; nMip < p2DBuffer->GetNumMipLevels(); ++nMip )
	{
		CalcNormals( r.res, pTarget, nMip, fBumpScale );
		CalcMip( &r.res );
		fBumpScale /= 2;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGScene;
BASIC_REGISTER_CLASS( ISWRects );
REGISTER_SAVELOAD_CLASS( 0xF3005171, CSWRects );
REGISTER_SAVELOAD_CLASS( 0xF3005172, CSW2DScene );
REGISTER_SAVELOAD_CLASS( 0xF3005173, CSWSpot );
