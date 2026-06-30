#ifndef __GFX_H_
#define __GFX_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GPixelFormat.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGfx
{
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EFS
{
	WINDOWED,
	FULL_SCREEN
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SVideoMode
{
	int nXSize, nYSize, nBpp, nRefreshRate;
	EFS fullScreen;
	SVideoMode() { nXSize = 800, nYSize = 600; nBpp = 32; nRefreshRate = 0; fullScreen = WINDOWED; }
	SVideoMode( int _nXSize, int _nYSize, int _nBpp, EFS _fullScreen, int _nRefreshRate = 0 )
		:nXSize(_nXSize), nYSize(_nYSize), nBpp(_nBpp), nRefreshRate(_nRefreshRate), fullScreen(_fullScreen) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRenderTargetsInfo
{
	unordered_map<int,int> targets; // resolution to number
	unordered_map<int,int> cubeTargets; // resolution to number
	int nRegisters;
	SRenderTargetsInfo() : nRegisters(0) {}

	void Clear() { targets.clear(); nRegisters = 0; }
	void Add(unordered_map<int,int> *pRes, int nResolution, int nTargets )
	{ 
		if ( pRes->find( nResolution ) == pRes->end() )
			(*pRes)[ nResolution ] = nTargets;
		else
			(*pRes)[ nResolution ] += nTargets;
	}
	void AddTex( int nResolution, int nTargets ) { Add( &targets, nResolution, nTargets ); }
	void AddCube( int nResolution, int nTargets ) { Add( &cubeTargets, nResolution, nTargets ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// general
bool Init3D( HWND hWnd );
void Done3D();
bool Is3DActive();
bool Is16BitTextures();   // @0x10ce10 -- selects 16-bit (SPixel1555) vs 32-bit (SPixel8888) dynamic 2D textures
void SetGamma( bool bGamma );
bool SetMode( const SVideoMode &m_, const SRenderTargetsInfo &_rtInfo );
void GetModesList( list<SVideoMode> *pRes, int nBpp = 32 );
CVec2 GetScreenRect();
void Flip();
void MakeScreenShot( CArray2D<SPixel8888> *pRes, bool bCorrectGamma );
void CheckBackBufferSize();
void CheckDeviceCaps();
//
struct SRenderStats
{
	int nVertices, nTris;
	SRenderStats(): nVertices(0), nTris(0) {}
	void Clear() { nVertices = 0; nTris = 0; }
};
externA5 SRenderStats renderStats;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif