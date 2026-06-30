#ifndef __GDXINTERNAL_H_
#define __GDXINTERNAL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// full description of buffers & textures for internal use & some internal data access
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Misc\Win32Helper.h"
#include "GPixelFormat.h"
namespace NGfx
{
	class CGeometry;
	class CTexture;
	class CCubeTexture;
	enum EFace;
	externA5 NWin32Helper::com_ptr<IDirect3D9> pD3D;
	externA5 NWin32Helper::com_ptr<IDirect3DDevice9> pDevice;
	externA5 int nCurrentFrame;
	externA5 bool bHardwareVP, bHardwarePixelShaders, bHardwarePixelShaders14;
	externA5 bool bTnLDevice;
	externA5 bool bDoValidateDevice;
	externA5 bool bNVHackNP2, bUseAnisotropy, bBanNP2, bBan32BitIndices, bStaticNooverwrite;
	externA5 bool bNoCubeMapMipLevels;
	externA5 D3DCAPS9 devCaps;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Vertex info
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGeomFormatInfo
{
	int nFormatID;
	int nSize;
	D3DVERTEXELEMENT9 *pdwVSD;
	DWORD dwFVF;
};
externA5 SGeomFormatInfo geometryFormatInfo[6];
////////////////////////////////////////////////////////////////////////////////////////////////////
inline int GetGeomFormatSize( int nFormatID )
{
	ASSERT( nFormatID >= 0 && nFormatID < ARRAY_SIZE( geometryFormatInfo ) );
	ASSERT( nFormatID == geometryFormatInfo[nFormatID].nFormatID );
	return geometryFormatInfo[nFormatID].nSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline const D3DVERTEXELEMENT9* GetVertexLayout( int nFormatID )
{
	ASSERT( nFormatID >= 0 && nFormatID < ARRAY_SIZE( geometryFormatInfo ) );
	ASSERT( nFormatID == geometryFormatInfo[nFormatID].nFormatID );
	return geometryFormatInfo[nFormatID].pdwVSD;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline int D3DFormat2PixelID( D3DFORMAT format )
{
	switch ( format )
	{
  case D3DFMT_A8R8G8B8: return CF_A8R8G8B8;
  case D3DFMT_X8R8G8B8: return CF_A8R8G8B8;
  case D3DFMT_R5G6B5:   return CF_R5G6B5;
	case D3DFMT_X1R5G5B5: return CF_A1R5G5B5;
	case D3DFMT_A1R5G5B5: return CF_A1R5G5B5;
	case D3DFMT_A4R4G4B4: return CF_A4R4G4B4;
	case D3DFMT_X4R4G4B4: return CF_A4R4G4B4;
	case D3DFMT_DXT1: return CF_DXT1;
	case D3DFMT_DXT2: return CF_DXT2;
	case D3DFMT_DXT3: return CF_DXT3;
	case D3DFMT_DXT4: return CF_DXT4;
	case D3DFMT_DXT5: return CF_DXT5;
	}
	ASSERT(0);
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline int D3DFormat2PixelBitSize( D3DFORMAT format )
{
	switch ( format )
	{
  case D3DFMT_A8R8G8B8: return 4*8;
  case D3DFMT_X8R8G8B8: return 4*8;
  case D3DFMT_R5G6B5:   return 2*8;
	case D3DFMT_X1R5G5B5: return 2*8;
	case D3DFMT_A1R5G5B5: return 2*8;
	case D3DFMT_A4R4G4B4: return 2*8;
	case D3DFMT_X4R4G4B4: return 2*8;
	case D3DFMT_DXT1: return 4; // ???
	case D3DFMT_DXT2: return 8;
	case D3DFMT_DXT3: return 8;
	case D3DFMT_DXT4: return 8;
	case D3DFMT_DXT5: return 8;
	}
	ASSERT(0);
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline D3DFORMAT PixelID2D3DFormat( int nPixelID )
{
	switch ( nPixelID )
	{
  case CF_A8R8G8B8: return D3DFMT_A8R8G8B8;
  case CF_R5G6B5:   return D3DFMT_R5G6B5;
	case CF_A1R5G5B5: return D3DFMT_A1R5G5B5;
	case CF_A4R4G4B4: return D3DFMT_A4R4G4B4;
	case CF_DXT1: return D3DFMT_DXT1;
	case CF_DXT2: return D3DFMT_DXT2;
	case CF_DXT3: return D3DFMT_DXT3;
	case CF_DXT4: return D3DFMT_DXT4;
	case CF_DXT5: return D3DFMT_DXT5;
	}
	ASSERT( 0 );
	return D3DFMT_A8R8G8B8;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRenderStateKey
{
	int nStage;
	int state;
	
	SRenderStateKey() {}
	SRenderStateKey( int _nStage, D3DTEXTURESTAGESTATETYPE _state ): nStage(_nStage), state(_state) {}
	SRenderStateKey( int _nStage, D3DSAMPLERSTATETYPE _state ): nStage(_nStage), state(_state) {}
	SRenderStateKey( D3DRENDERSTATETYPE _state ): nStage(-1), state(_state) {}
	bool operator==( const SRenderStateKey &a ) const { return nStage == a.nStage && state == a.state; }
};
struct SRenderStateKeyHash
{
	int operator()( const SRenderStateKey &a ) const { return a.nStage ^ a.state; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGeometry : virtual public CObjectBase
{
public:
	virtual void DoTouch() = 0;
	virtual void SetVertexStream() = 0;
	virtual void* GetVertexStream() = 0;
	virtual int GetVBStart() const = 0;
	virtual int GetVBSize() const = 0;
	virtual int GetGeometryFormatID() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct S3DTriangle;
class CTriListWrapper;
class CTriList: virtual public CObjectBase
{
public:
	virtual const vector<S3DTriangle>& GetTris() const = 0;
	virtual int GetTrisNumber() const = 0;
	virtual void DrawPrimitive( int nVBStart, int nMinIndex, int nMaxIndex ) = 0;
	virtual CTriListWrapper* CreateWrapper( int nTris ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void InitBuffers();
void NextFrameBuffes( bool bOnThrashing = false );
void SetTexture( int nStage, CTexture *pTex );
void SetTexture( int nStage, CCubeTexture *pTex );
void DestroyLostableBuffers();
void DestroyManagedBuffers();
HRESULT InitRender();
bool InitZBuffer( D3DFORMAT format );
void DoneZBuffer();
void GetSurface( CTexture *pTexture, int nLevel, NWin32Helper::com_ptr<IDirect3DSurface9> *pRes );
void GetSurface( CCubeTexture *pTexture, EFace face, int nLevel, NWin32Helper::com_ptr<IDirect3DSurface9> *pRes );
bool IsWrapped( CTexture *pTex );
void DoneRender();
CTexture* MakeRenderTarget( int nXSize, int nYSize, int nPixelID );
void AddPrimitiveGeometry( CGeometry *pGeom, CTriList *pTriList, int nStartVertex, int nVertices );
void AddPrimitiveGeometry( CGeometry *pGeom, CTriList *pTriList );
void AddPrimitiveGeometry( CGeometry *pGeom, const STriangleList *pTris, int nCount, unsigned nMask );
void FlushPrimitive();
void DrawLineStrip( CGeometry *pGeom );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif