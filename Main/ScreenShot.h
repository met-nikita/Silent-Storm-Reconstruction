#ifndef __SCREENSHOT_H__
#define __SCREENSHOT_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "GSceneUtils.h"
#include "GPixelFormat.h"
#include "..\Misc\2DArray.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGfx
{
	class CTexture;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScreenshotTexture: public CPtrFuncBase<NGfx::CTexture>
{
	OBJECT_BASIC_METHODS(CScreenshotTexture);
public:
	enum EMode
	{
		COLOR,
		BLACKANDWHITE
	};

private:
	ZDATA
	EMode eMode;
	CVec4 vCoeff;
	CArray2D<NGfx::SPixel8888> sScreenShot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&eMode); f.Add(3,&vCoeff); f.Add(4,&sScreenShot); return 0; }
	
protected:
	void Recalc();

public:
	CScreenshotTexture();

	void Get( CArray2D<NGfx::SPixel8888> *pScreenShot );
	void Set( const CArray2D<NGfx::SPixel8888> &sScreenShot );
	void Generate();
	void GetSize( CTPoint<int> *pSize );
	void SetMode( EMode eMode, const CVec4 &vColor );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif