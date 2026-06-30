#include "StdAfx.h"
#include "DG.h"
#include "Gfx.h"
#include "GfxBuffers.h"
#include "2DSceneSW.h"
#include "SWTexture.h"
#include "GSceneUtils.h"
#include "ScreenShot.h"
#include "..\Misc\2DArray.h"
#include "..\DBFormat\DataFormat.h"
#include "..\Misc\BasicShare.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScreenshotTexture
///////////////////////////////////////////////////////////////////////////////////////////////////
CScreenshotTexture::CScreenshotTexture():
	eMode( COLOR ), vCoeff( 1, 1, 1, 1 )
{
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void CScreenshotTexture::Get( CArray2D<NGfx::SPixel8888> *pScreenShot )
{
	*pScreenShot = sScreenShot;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void CScreenshotTexture::Set( const CArray2D<NGfx::SPixel8888> &_sScreenShot )
{
	sScreenShot = _sScreenShot;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void CScreenshotTexture::Generate()
{
	NGfx::MakeScreenShot( &sScreenShot, false );
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void CScreenshotTexture::GetSize( CTPoint<int> *pSize )
{
	pSize->x = sScreenShot.GetXSize();
	pSize->y = sScreenShot.GetYSize();
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void CScreenshotTexture::SetMode( EMode _eMode, const CVec4 &_vCoeff )
{
	eMode = _eMode;
	vCoeff = _vCoeff;

	Updated();
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void CScreenshotTexture::Recalc()
{
	CTPoint<int> sSize( GetNextPow2( sScreenShot.GetXSize() ), GetNextPow2( sScreenShot.GetYSize() ) );
	pValue = NGfx::MakeTexture( sSize.x, sSize.y, 1, NGfx::SPixel8888::ID, NGfx::REGULAR, NGfx::CLAMP );

	if ( !IsValid( pValue ) )
		return;

	if ( eMode == COLOR )
	{
		NGfx::CTextureLock<NGfx::SPixel8888> sLock( pValue, 0, NGfx::INPLACE );

		for ( int nTempY = 0; nTempY < sScreenShot.GetYSize(); nTempY++ )
		{
			for ( int nTempX = 0; nTempX < sScreenShot.GetXSize(); nTempX++ )
			{
				NGfx::SPixel8888 &sDst = sLock[nTempY][nTempX];
				const NGfx::SPixel8888 &sSrc = sScreenShot[nTempY][nTempX];

				sDst.a = 0xFF * vCoeff.a;
				sDst.r = sSrc.r * vCoeff.r;
				sDst.g = sSrc.g * vCoeff.g;
				sDst.b = sSrc.b * vCoeff.b;
			}
		}
	}
	else if ( eMode == BLACKANDWHITE )
	{
		NGfx::CTextureLock<NGfx::SPixel8888> sLock( pValue, 0, NGfx::INPLACE );

		for ( int nTempY = 0; nTempY < sScreenShot.GetYSize(); nTempY++ )
		{
			for ( int nTempX = 0; nTempX < sScreenShot.GetXSize(); nTempX++ )
			{
				NGfx::SPixel8888 &sDst = sLock[nTempY][nTempX];
				const NGfx::SPixel8888 &sSrc = sScreenShot[nTempY][nTempX];

				float fVal = Float2Int( ( ( sSrc.r * 0.3086f ) + ( sSrc.g * 0.6094f ) + ( sSrc.b * 0.0820f ) ) / 2 );
				sDst.a = 0xFF * vCoeff.a;
				sDst.r = fVal * vCoeff.r;
				sDst.g = fVal * vCoeff.g;
				sDst.b = fVal * vCoeff.b;
			}
		}
	}

	sSize.x = sScreenShot.GetXSize();
	sSize.y = sScreenShot.GetYSize();

	Updated();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0xB2030101, CScreenshotTexture );
