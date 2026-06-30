#ifndef __GfxRender_H_
#define __GfxRender_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
#include "GPixelFormat.h"
struct SPShader;
struct SVShader;
namespace NGfx
{
enum EHardwareLevel
{
	HL_TNL_DEVICE,
	HL_GFORCE,
	HL_GFORCE3,
	HL_RADEON2
};
EHardwareLevel GetHardwareLevel();
bool IsTnLDevice();
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EAlphaCombineMode
{
	COMBINE_NONE,
	COMBINE_ADD,
	COMBINE_MUL,
	COMBINE_MUL2,
	COMBINE_ALPHA,
	COMBINE_ALPHA_ADD,
	COMBINE_ZERO_ONE,
	COMBINE_SRC_ALPHA_MUL,
	COMBINE_SMART_ALPHA,
};
enum EStencilMode
{
	STENCIL_NONE,
	STENCIL_INCR,
	STENCIL_DECR,
	STENCIL_TESTINCR,
	STENCIL_TESTDECR,
	//STENCIL_TESTWRITE,
	STENCIL_TEST,
	STENCIL_TESTNE_WRITE,
	STENCIL_TEST_CLEAR, // will write anyway
	STENCIL_TESTNE_CLEAR, // will write anyway
	STENCIL_WRITE,
	//STENCIL_TEST,
	//STENCIL_INVERT,
	//STENCIL_NOTEQUAL,
	STENCIL_TEST_REPLACE
};
enum EDepthMode
{
	DEPTH_NONE,
	DEPTH_NORMAL,
	DEPTH_EQUAL,
	DEPTH_OVERWRITE,
	DEPTH_TESTONLY,
	DEPTH_INVERSETEST,
	DEPTH_GREATEEQTEST
};
enum ECullMode
{
	CULL_NONE,
	CULL_CW,
	CULL_CCW
};
enum EColorWriteMask
{
	COLORWRITE_NONE  = 0,
	COLORWRITE_RED   = 1,
	COLORWRITE_GREEN = 2,
	COLORWRITE_BLUE  = 4,
	COLORWRITE_ALPHA = 8,
	COLORWRITE_COLOR = 7,
	COLORWRITE_ALL   = 15
};
enum EWireframe
{
	WIREFRAME_OFF,
	WIREFRAME_ON
};
struct SStencilMode
{
	EStencilMode mode;
	int nVal, nMask;

	SStencilMode() {}
	SStencilMode( EStencilMode _mode, int _nVal = 0, int _nMask = 0xffffffff ): mode(_mode), nVal(_nVal), nMask(_nMask) {}
	bool operator==( const SStencilMode &m ) const { return mode == m.mode && nVal == m.nVal && nMask == m.nMask; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTexture;
class CCubeTexture;
class CGeometry;
class CTriList;
struct S3DTriangle;
enum EFace;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRenderContext
{
	enum ERenderTargetMode
	{
		RTM_SCREEN,
		RTM_TEXTURE,
		RTM_REGISTERS,
		RTM_CUBETEXTURE
	};
	SFBTransform transform;
	EAlphaCombineMode alpha;
	SStencilMode stencil;
	EColorWriteMask colorWrite;
	EDepthMode depth;
	ECullMode cull;
	ERenderTargetMode targetMode;
	CObj<CTexture> pTarget;
	CObj<CCubeTexture> pCubeTarget;
	int nMipLevel, nRegister;
	//CTRect<int> rViewport;
	const SPShader *pPixelShader;
	const SVShader *pVertexShader;
	void *pOutstandingStream; // 0 if no geometry is in fly

	void ApplyRenderTarget() const;
	void StartStream( CGeometry *pGeom );
	void CheckStream( CGeometry *pGeom );
public:
	CRenderContext();
	~CRenderContext();
	void SetTransform( const SFBTransform &_transform );
	void SetAlphaCombine( EAlphaCombineMode mode );
	void SetStencil( const SStencilMode &m );
	void SetStencil( EStencilMode m, int nVal = 0, int nMask = 0xffffffff ) { SetStencil( SStencilMode( m, nVal, nMask ) ); }
	void SetDepth( EDepthMode mode );
	void SetCulling( ECullMode mode );
	void SetColorWrite( EColorWriteMask mode );
	//
	void SetScreenRT();
	void SetTextureRT( CTexture *pTexture, int nMipLevel = 0 );
	void SetCubeTextureRT( CCubeTexture *pTexture, EFace nFace, int nMipLevel = 0 );
	//void SetVirtualRT( const CTRect<int> &size ); // registers available only in this mode
	void SetVirtualRT(); // screen size virtual RT
	void SetRegister( int nRegister );
	void ClearBuffers( DWORD dwColor = 0x808080 );
	void ClearTarget( DWORD dwColor = 0x808080 );
	void ClearZBuffer();
	//
	bool HasRegisters() const { return targetMode == RTM_REGISTERS; }
	// functions to be used in effect initialisation
	void SetPixelShader( const SPShader &pShader );
	void SetVertexShader( const SVShader &pShader );
	void SetVSConst( int nReg, const CVec4 *pData, int nSize );
	void SetVSConst( int nReg, const CVec3 &a );
	void SetVSConst( int nReg, const CVec4 &a ) { SetVSConst( nReg, &a, 1 ); }
	void SetPSConst( int nReg, const CVec4 *pData, int nSize );
	void SetPSConst( int nReg, const CVec3 &a );
	void SetPSConst( int nReg, const CVec4 &a ) { SetPSConst( nReg, &a, 1 ); }
	void SetAlphaRef( int nRef );
	void SetTexture( int nStage, CTexture *pTex, bool bPointFilter = false );
	void SetTexture( int nStage, CCubeTexture *pTex );
	void SetLightParams( const CVec3 &vAmbient, const CVec3 &vLight, const CVec3 &vDir );
	void Use() const;
	template<class T>
		void SetEffect( T *p ) { Use(); p->Use( this ); }
	//
	const SFBTransform& GetTransform() const { return transform; }
	//
	void DrawPrimitive( CGeometry *pGeom, CTriList *pTris, int nStartVertex, int nVertices );
	void DrawPrimitive( CGeometry *pGeom, CTriList *pTris ) { AddPrimitive( pGeom, pTris ); Flush(); }
	void DrawPrimitive( CGeometry *pGeom, const STriangleList &tris ) { AddPrimitive( pGeom, tris ); Flush(); }
	//void DrawPrimitive( CGeometry *pGeom, const vector<STriangle> &tris ) { AddPrimitive( pGeom, tris ); Flush(); }
	void AddPrimitive( CGeometry *pGeom, CTriList *pTris );
	//void AddPrimitive( CGeometry *pGeom, const vector<STriangle> &tris );
	void AddPrimitive( CGeometry *pGeom, const STriangleList &tris );
	void AddPrimitive( CGeometry *pGeom, const STriangleList *pTris, int nCount, unsigned nMask );
	void Flush();
	void DrawLineStrip( CGeometry *pGeom );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CTexture* GetRegisterTexture( int nRegister );
void SetWireframe( EWireframe wire );
void GetRegisterSize( CTRect<float> *pRes );
bool IsNVidiaNP2Bug();
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif