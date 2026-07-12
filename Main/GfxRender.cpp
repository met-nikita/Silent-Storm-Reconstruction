#include "StdAfx.h"
#include <D3D9.h>
#include "GfxRender.h"
#include "GfxInternal.h"
#include "GfxBuffers.h"
#include "Gfx.h"
#include "GfxShaders.h"
#include "GfxShadersDescr.h"

namespace NGfx
{
externA5 SRenderTargetsInfo rtInfo;
static bool operator==( const SFBTransform &a, const SFBTransform &b )
{
	return memcmp( &a, &b, sizeof(SFBTransform) ) == 0;
}
template<class T>
void Apply( const T &a );
template<typename T>
struct SRenderParam
{
	T value;
	SRenderParam( const T &_a ): value(_a) {}
	void Set( const T &_a ) { if ( _a == value ) return; value = _a; Apply( _a ); }
	void DoApply() { Apply( value ); }
};
typedef unordered_map<int, NWin32Helper::com_ptr<IDirect3DSurface9> > CDepthHash;

const int N_MAX_REGISTERS =	5;
static int nScreenRegisters;
bool bDoValidateDevice = false;
static SRenderParam<EWireframe> wireframeMode( WIREFRAME_OFF );
static SRenderParam<EAlphaCombineMode> alphaMode( COMBINE_NONE );
static SRenderParam<SStencilMode> stencilMode( SStencilMode( STENCIL_NONE, 0, 0xffffffff ) );
static SRenderParam<EColorWriteMask> colorMode( COLORWRITE_ALL );
static SRenderParam<EDepthMode> depthMode( DEPTH_NORMAL );
static SRenderParam<ECullMode> cullMode( CULL_CW );
static SRenderParam<SFBTransform> transformMode = SRenderParam<SFBTransform>( SFBTransform() );
static NWin32Helper::com_ptr<IDirect3DSurface9> pScreenColor, pScreenDepth, pRegisterDepth;
static CDepthHash sharedZBuffers;
static CMObj<CTexture> pRegisters[N_MAX_REGISTERS];
static CTPoint<int> ptRegisterBufferSize, ptScreenSize;
//static bool bUseSeparateZBuffer;
static bool bLastUsedAddressMode[8], bPointAddress[8];
static NWin32Helper::com_ptr<IDirect3DPixelShader9> pixelShaders[200];
static NWin32Helper::com_ptr<IDirect3DVertexShader9> vertexShaders[200];
static NWin32Helper::com_ptr<IDirect3DVertexDeclaration9> vertexDeclarations[200];
//static unordered_map<int, CVertexShader> vxShaders;
//static unordered_map<int, CVertexDeclaration> vxDeclarations;
static int nLastUsedVShader, nLastUsedVDeclaration;
static const SPShader *pCurrentPixelShader;
typedef unordered_map<SRenderStateKey, DWORD, SRenderStateKeyHash> CRenderStatesHash;
static CRenderStatesHash renderStates, samplerStates;
////////////////////////////////////////////////////////////////////////////////////////////////////
EHardwareLevel GetHardwareLevel()
{
	if ( bTnLDevice )
		return HL_TNL_DEVICE;
	if ( bHardwarePixelShaders14 )
		return HL_RADEON2;
	if ( bHardwarePixelShaders )
		return HL_GFORCE3;
	return HL_GFORCE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsTnLDevice()
{
	return bTnLDevice;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#define ZERO_ARRAY(a) { for ( int k = 0; k < ARRAY_SIZE(a); ++k ) a[k] = 0; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// Vertex formats description
////////////////////////////////////////////////////////////////////////////////////////////////////
//{ 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
//{ 0, 16, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
//{ 0, 20, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
static D3DVERTEXELEMENT9 dwVecTC[] =
{
	{0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	{0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	{0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	D3DDECL_END()
};
/*static DWORD dwVecTC[] = {
	D3DVSD_STREAM(0),
	D3DVSD_REG(0, D3DVSDT_FLOAT3),
	D3DVSD_REG(2, D3DVSDT_D3DCOLOR),
	D3DVSD_REG(3, D3DVSDT_FLOAT2),
	D3DVSD_END()
};*/
static D3DVERTEXELEMENT9 dwVecNT[] =
{
	{0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	{0, 12, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
	{0, 24, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	D3DDECL_END()
};
/*static DWORD dwVecNT[] = {
	D3DVSD_STREAM(0),
	D3DVSD_REG(0, D3DVSDT_FLOAT3),
	D3DVSD_REG(1, D3DVSDT_FLOAT3),
	D3DVSD_REG(3, D3DVSDT_FLOAT2),
	D3DVSD_END()
};*/
static D3DVERTEXELEMENT9 dwVecFull[] =
{
	{0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	{0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
	{0, 16, D3DDECLTYPE_SHORT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	{0, 20, D3DDECLTYPE_SHORT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
	{0, 24, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0 },
	{0, 28, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 1 },
	D3DDECL_END()
};
/*static DWORD dwVecFull[] = {
	D3DVSD_STREAM(0),
	D3DVSD_REG(0, D3DVSDT_FLOAT3),
	D3DVSD_REG(1, D3DVSDT_D3DCOLOR),
	D3DVSD_REG(3, D3DVSDT_SHORT2),
	D3DVSD_REG(6, D3DVSDT_SHORT2),
	D3DVSD_REG(4, D3DVSDT_D3DCOLOR),
	D3DVSD_REG(5, D3DVSDT_D3DCOLOR),
	D3DVSD_END()
};*/
SGeomFormatInfo geometryFormatInfo[6] =
{
	{ 0, 0, dwVecFull, 0 },//SGeomVec::ID, sizeof(SGeomVec), dwVec },
	{ 0, 0, dwVecFull, 0 },//{ SGeomVecT1::ID, sizeof(SGeomVecT1), dwVecT },
	{ SGeomVecT1C1::ID, sizeof(SGeomVecT1C1), dwVecTC, D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1 },
	{ 0, 0, dwVecFull, 0 },//{ SGeomVecT2C1::ID, sizeof(SGeomVecT2C1), dwVecT2C },
	{ SGeomVecNT1::ID, sizeof(SGeomVecNT1), dwVecNT,  D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1 },
	{ SGeomVecFull::ID, sizeof(SGeomVecFull), dwVecFull, 0 }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static void ApplyRenderState( D3DRENDERSTATETYPE state, DWORD dwVal )
{
	SRenderStateKey key( state );
	CRenderStatesHash::iterator i = renderStates.find( key );
	if ( i != renderStates.end() )
	{
		if ( i->second != dwVal )
			i->second = dwVal;
		else
			return;
	}
	else
		renderStates[key] = dwVal;
	pDevice->SetRenderState( state, dwVal );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void ApplyRenderState( int nStage, D3DTEXTURESTAGESTATETYPE state, DWORD dwVal )
{
	SRenderStateKey key( nStage, state );
	CRenderStatesHash::iterator i = renderStates.find( key );
	if ( i != renderStates.end() )
	{
		if ( i->second != dwVal )
			i->second = dwVal;
		else
			return;
	}
	else
		renderStates[key] = dwVal;
	pDevice->SetTextureStageState( nStage, state, dwVal );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void ApplySamplerState( int nStage, D3DSAMPLERSTATETYPE state, DWORD dwVal )
{
	SRenderStateKey key( nStage, state );
	CRenderStatesHash::iterator i = samplerStates.find( key );
	if ( i != samplerStates.end() )
	{
		if ( i->second != dwVal )
			i->second = dwVal;
		else
			return;
	}
	else
		samplerStates[key] = dwVal;
	pDevice->SetSamplerState( nStage, state, dwVal );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void ApplyRenderStates( const SRenderState *pRS )
{
	for ( const SRenderState *p = pRS; p->state != 0; ++p )
		ApplyRenderState( p->state, p->dwVal );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void ApplyRenderStates( STextureStageState *pRS )
{
	for ( const STextureStageState *p = pRS; p->nStage != -1; ++p )
		ApplyRenderState( p->nStage, p->state, p->dwVal );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetVSConst( int nReg, const CVec4 *pData, int nSize )
{
	pDevice->SetVertexShaderConstantF( nReg, (const float*)pData, nSize );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetPSConst( int nReg, const CVec4 *pData, int nSize )
{
	if ( bHardwarePixelShaders )
		pDevice->SetPixelShaderConstantF( nReg, (const float*)pData, nSize );
	else
	{
		ASSERT( nReg == 0 && nSize == 1 );
		DWORD dwColor = GetDWORDColor( pData[0] );
		ApplyRenderState( D3DRS_TEXTUREFACTOR, dwColor );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetPixelShader( const SPShader &s )
{
	HRESULT hr;
	if ( pCurrentPixelShader == &s )
		return;
	pCurrentPixelShader = &s;
	ApplyRenderStates( pCurrentPixelShader->pStateRS );
	ApplyRenderStates( pCurrentPixelShader->pStateTSS );
	if ( bHardwarePixelShaders )
	{
		hr = pDevice->SetPixelShader( pixelShaders[ s.nID - 1 ] );
		ASSERT( D3D_OK == hr );
	}
	else
	{
		ApplyRenderStates( pCurrentPixelShader->pShaRS );
		ApplyRenderStates( pCurrentPixelShader->pShaTSS );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetVertexShader( CGeometry *pVB, const SVShader &shader )
{
	int nFormatID = pVB->GetGeometryFormatID();
	if ( bTnLDevice )
	{
		if ( nFormatID == nLastUsedVDeclaration )
			return;

		nLastUsedVDeclaration = nFormatID;
		DWORD dwFVF = geometryFormatInfo[nFormatID].dwFVF;
		pDevice->SetFVF( dwFVF );
		pDevice->SetRenderState( D3DRS_LIGHTING, (dwFVF & D3DFVF_DIFFUSE) ? FALSE : TRUE );
		return;
	}

	if ( nFormatID != nLastUsedVDeclaration )
	{
		nLastUsedVDeclaration = nFormatID;
		HRESULT hRes = pDevice->SetVertexDeclaration( vertexDeclarations[ nFormatID ] );
		ASSERT( hRes == D3D_OK );
	}
	if ( nLastUsedVShader != shader.nID )
	{
		nLastUsedVShader = shader.nID;
		HRESULT hRes = pDevice->SetVertexShader( vertexShaders[ shader.nID - 1 ] );
		ASSERT( hRes == D3D_OK );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// render modes application
////////////////////////////////////////////////////////////////////////////////////////////////////
template<>
static void Apply( const SFBTransform &trans )
{
	if ( bTnLDevice )
	{
		SHMatrix m;
		Transpose( &m, trans.forward );
		pDevice->SetTransform( D3DTS_PROJECTION, (const D3DMATRIX*)&m );
		return;
	}
	SetVSConst( 10, (CVec4*)&trans.forward, 4 );
	// calculate the world-space homogeneous eye point and store it to c9 (retail @0x11b070):
	// inverse-transform the clip-space +Z direction (0,0,1,0). For a perspective camera this
	// yields (camPos*l, l) with l != 0; for the orthogonal tactical camera it yields the view
	// direction with w == 0. The shaders compute eyeDir = c9.xyz - c9.w*pos (CalcCameraDir),
	// which handles both. The old unproject-of-(0,0,0,1) gave a bogus near-plane point for the
	// ortho camera and corrupted every reflection/specular that needs the eye vector.
	CVec4 ptRes;
	trans.backward.RotateHVector( &ptRes, CVec4(0,0,1,0) );
	if ( ptRes.w <= 0 )
	{
		// orient the homogeneous eye toward the viewer (retail negates all four components)
		ptRes.x = -ptRes.x;
		ptRes.y = -ptRes.y;
		ptRes.z = -ptRes.z;
		ptRes.w = -ptRes.w;
	}
	SetVSConst( 9, &ptRes, 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<>
static void Apply( const EWireframe &wireFrame )
{
	if ( wireFrame )
		ApplyRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );
	else
		ApplyRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<>
static void Apply( const EAlphaCombineMode &alphaMode )
{
	switch ( alphaMode )
	{
		case COMBINE_NONE:
			ApplyRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
			break;
		case COMBINE_ADD:
			ApplyRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
			ApplyRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
			ApplyRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
			break;
		case COMBINE_MUL:
			ApplyRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
			ApplyRenderState( D3DRS_SRCBLEND, D3DBLEND_ZERO );
			ApplyRenderState( D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR );
			break;
		case COMBINE_MUL2:
			ApplyRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
			ApplyRenderState( D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR );
			ApplyRenderState( D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR );
			break;
		case COMBINE_SMART_ALPHA:
			ApplyRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
			ApplyRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
			ApplyRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			break;
		case COMBINE_ALPHA:
			ApplyRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
			ApplyRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			ApplyRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			break;
		case COMBINE_ALPHA_ADD:
			ApplyRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
			ApplyRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			ApplyRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
			break;
		case COMBINE_ZERO_ONE:
			ApplyRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
			ApplyRenderState( D3DRS_SRCBLEND, D3DBLEND_ZERO );
			ApplyRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
			break;
		case COMBINE_SRC_ALPHA_MUL:
			ApplyRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
			ApplyRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
			ApplyRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<>
static void Apply( const SStencilMode &m )
{
	switch ( m.mode )
	{
		case STENCIL_NONE:
			ApplyRenderState( D3DRS_STENCILENABLE, FALSE );
			break;
		case STENCIL_INCR:
			ApplyRenderState( D3DRS_STENCILENABLE, TRUE );
			ApplyRenderState( D3DRS_STENCILFUNC, D3DCMP_ALWAYS );
			ApplyRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_INCR );
			ApplyRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
			ApplyRenderState( D3DRS_STENCILREF, m.nVal );
			ApplyRenderState( D3DRS_STENCILMASK, m.nMask );
			ApplyRenderState( D3DRS_STENCILWRITEMASK, m.nMask );
			break;
		case STENCIL_DECR:
			ApplyRenderState( D3DRS_STENCILENABLE, TRUE );
			ApplyRenderState( D3DRS_STENCILFUNC, D3DCMP_ALWAYS );
			ApplyRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_DECR );
			ApplyRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
			ApplyRenderState( D3DRS_STENCILREF, m.nVal );
			ApplyRenderState( D3DRS_STENCILMASK, m.nMask );
			ApplyRenderState( D3DRS_STENCILWRITEMASK, m.nMask );
			break;
		case STENCIL_TESTINCR:
			ApplyRenderState( D3DRS_STENCILENABLE, TRUE );
			ApplyRenderState( D3DRS_STENCILFUNC, D3DCMP_EQUAL );
			ApplyRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_INCR );
			ApplyRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
			ApplyRenderState( D3DRS_STENCILREF, m.nVal );
			ApplyRenderState( D3DRS_STENCILMASK, m.nMask );
			ApplyRenderState( D3DRS_STENCILWRITEMASK, m.nMask );
			break;
		case STENCIL_TESTDECR:
			ApplyRenderState( D3DRS_STENCILENABLE, TRUE );
			ApplyRenderState( D3DRS_STENCILFUNC, D3DCMP_EQUAL );
			ApplyRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_DECR );
			ApplyRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
			ApplyRenderState( D3DRS_STENCILREF, m.nVal );
			ApplyRenderState( D3DRS_STENCILMASK, m.nMask );
			ApplyRenderState( D3DRS_STENCILWRITEMASK, m.nMask );
			break;
		case STENCIL_TEST:
			ApplyRenderState( D3DRS_STENCILENABLE, TRUE );
			ApplyRenderState( D3DRS_STENCILFUNC, D3DCMP_EQUAL );
			ApplyRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_KEEP );
			ApplyRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
			ApplyRenderState( D3DRS_STENCILREF, m.nVal );
			ApplyRenderState( D3DRS_STENCILMASK, m.nMask );
			break;
		/*case STENCIL_TESTWRITE:
			ApplyRenderState( D3DRS_STENCILENABLE, TRUE );
			ApplyRenderState( D3DRS_STENCILFUNC, D3DCMP_EQUAL );
			ApplyRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE );
			ApplyRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
			ApplyRenderState( D3DRS_STENCILREF, m.nVal );
			ApplyRenderState( D3DRS_STENCILMASK, m.nMask );
			ApplyRenderState( D3DRS_STENCILWRITEMASK, m.nMask );
			break;*/
		case STENCIL_TESTNE_WRITE:
			ApplyRenderState( D3DRS_STENCILENABLE, TRUE );
			ApplyRenderState( D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL );
			ApplyRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE );
			ApplyRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
			ApplyRenderState( D3DRS_STENCILREF, m.nVal );
			ApplyRenderState( D3DRS_STENCILMASK, m.nMask );
			ApplyRenderState( D3DRS_STENCILWRITEMASK, m.nMask );
			break;
		case STENCIL_TEST_CLEAR:
			ApplyRenderState( D3DRS_STENCILENABLE, TRUE );
			ApplyRenderState( D3DRS_STENCILFUNC, D3DCMP_EQUAL );
			ApplyRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE );
			ApplyRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_REPLACE );
			ApplyRenderState( D3DRS_STENCILREF, m.nVal );
			ApplyRenderState( D3DRS_STENCILMASK, m.nMask );
			ApplyRenderState( D3DRS_STENCILWRITEMASK, m.nMask );
			break;
		case STENCIL_TESTNE_CLEAR:
			ApplyRenderState( D3DRS_STENCILENABLE, TRUE );
			ApplyRenderState( D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL );
			ApplyRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE );
			ApplyRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_REPLACE );
			ApplyRenderState( D3DRS_STENCILREF, m.nVal );
			ApplyRenderState( D3DRS_STENCILMASK, m.nMask );
			ApplyRenderState( D3DRS_STENCILWRITEMASK, 0xffffffff );
			break;
		case STENCIL_WRITE:
			ApplyRenderState( D3DRS_STENCILENABLE, TRUE );
			ApplyRenderState( D3DRS_STENCILFUNC, D3DCMP_ALWAYS );
			ApplyRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE );
			ApplyRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
			ApplyRenderState( D3DRS_STENCILREF, m.nVal );
			ApplyRenderState( D3DRS_STENCILWRITEMASK, m.nMask );
			break;
/*		case STENCIL_TEST:
			ApplyRenderState( D3DRS_STENCILENABLE, TRUE );
			ApplyRenderState( D3DRS_STENCILFUNC, D3DCMP_EQUAL );
			ApplyRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_KEEP );
			ApplyRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
			ApplyRenderState( D3DRS_STENCILREF, m.nVal );
			ApplyRenderState( D3DRS_STENCILMASK, m.nMask );
			ApplyRenderState( D3DRS_STENCILWRITEMASK, m.nMask );
			break;
		case STENCIL_INVERT:
			ApplyRenderState( D3DRS_STENCILENABLE, TRUE );
			ApplyRenderState( D3DRS_STENCILFUNC, D3DCMP_ALWAYS );
			ApplyRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_INVERT );
			ApplyRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
			ApplyRenderState( D3DRS_STENCILREF, m.nVal );
			ApplyRenderState( D3DRS_STENCILMASK, m.nMask );
			ApplyRenderState( D3DRS_STENCILWRITEMASK, m.nMask );
			break;
		case STENCIL_NOTEQUAL:
			ApplyRenderState( D3DRS_STENCILENABLE, TRUE );
			ApplyRenderState( D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL );
			ApplyRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_KEEP );
			ApplyRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
			ApplyRenderState( D3DRS_STENCILREF, m.nVal );
			ApplyRenderState( D3DRS_STENCILMASK, m.nMask );
			ApplyRenderState( D3DRS_STENCILWRITEMASK, m.nMask );
			break;*/
		case STENCIL_TEST_REPLACE:
			ApplyRenderState( D3DRS_STENCILENABLE, TRUE );
			ApplyRenderState( D3DRS_STENCILFUNC, D3DCMP_EQUAL );
			ApplyRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE );//INVERT );
			ApplyRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
			ApplyRenderState( D3DRS_STENCILREF, m.nVal );
			ApplyRenderState( D3DRS_STENCILMASK, m.nMask );
			ApplyRenderState( D3DRS_STENCILWRITEMASK, 0xffffffff );//0x80 );
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<>
static void Apply( const EDepthMode &depth )
{
	switch ( depth )
	{
		case DEPTH_NONE:
			ApplyRenderState( D3DRS_ZENABLE, FALSE );
			ApplyRenderState( D3DRS_ZWRITEENABLE, FALSE );
			ApplyRenderState( D3DRS_ZFUNC, D3DCMP_ALWAYS );
			break;
		case DEPTH_NORMAL:
			ApplyRenderState( D3DRS_ZENABLE, TRUE );
			ApplyRenderState( D3DRS_ZWRITEENABLE, TRUE );
			ApplyRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
			break;
		case DEPTH_INVERSETEST:
			ApplyRenderState( D3DRS_ZENABLE, TRUE );
			ApplyRenderState( D3DRS_ZWRITEENABLE, FALSE );
			ApplyRenderState( D3DRS_ZFUNC, D3DCMP_GREATER );
			break;
		case DEPTH_EQUAL:
			ApplyRenderState( D3DRS_ZENABLE, TRUE );
			ApplyRenderState( D3DRS_ZWRITEENABLE, FALSE );
			ApplyRenderState( D3DRS_ZFUNC, D3DCMP_EQUAL );
			break;
		case DEPTH_OVERWRITE:
			ApplyRenderState( D3DRS_ZENABLE, TRUE );
			ApplyRenderState( D3DRS_ZWRITEENABLE, TRUE );
			ApplyRenderState( D3DRS_ZFUNC, D3DCMP_ALWAYS );
			break;
		case DEPTH_TESTONLY:
			ApplyRenderState( D3DRS_ZENABLE, TRUE );
			ApplyRenderState( D3DRS_ZWRITEENABLE, FALSE );
			ApplyRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
			break;
		case DEPTH_GREATEEQTEST:
			ApplyRenderState( D3DRS_ZENABLE, TRUE );
			ApplyRenderState( D3DRS_ZWRITEENABLE, FALSE );
			ApplyRenderState( D3DRS_ZFUNC, D3DCMP_GREATEREQUAL );
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<>
static void Apply( const ECullMode &cull )
{
  switch ( cull )
  {
    case CULL_CW:
      ApplyRenderState( D3DRS_CULLMODE, D3DCULL_CW );
      break;
    case CULL_CCW:
      ApplyRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
      break;
    case CULL_NONE:
      ApplyRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
      break;
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<>
static void Apply( const EColorWriteMask &colorMode )
{
	DWORD dwFlags = 
		(( colorMode & COLORWRITE_RED ) ? D3DCOLORWRITEENABLE_RED : 0) |
		(( colorMode & COLORWRITE_GREEN ) ? D3DCOLORWRITEENABLE_GREEN : 0) |
		(( colorMode & COLORWRITE_BLUE ) ? D3DCOLORWRITEENABLE_BLUE : 0) |
		(( colorMode & COLORWRITE_ALPHA ) ? D3DCOLORWRITEENABLE_ALPHA : 0);
	ApplyRenderState( D3DRS_COLORWRITEENABLE, dwFlags );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetTextureWrap( int nStage, bool bWrap )
{
	if ( bLastUsedAddressMode[nStage] == bWrap )
		return;
	bLastUsedAddressMode[nStage] = bWrap;
	ApplySamplerState( nStage, D3DSAMP_ADDRESSU, bWrap ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP );
	ApplySamplerState( nStage, D3DSAMP_ADDRESSV, bWrap ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetTexturePointFilter( int n, bool bPoint )
{
	if ( bPointAddress[n] == bPoint )
		return;
	bPointAddress[n] = bPoint;
	if ( bPoint )
	{
		pDevice->SetSamplerState( n, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
		pDevice->SetSamplerState( n, D3DSAMP_MINFILTER, D3DTEXF_POINT );
		pDevice->SetSamplerState( n, D3DSAMP_MIPFILTER, D3DTEXF_POINT );//D3DTEXF_POINT );
	}
	else
	{
		if ( bUseAnisotropy )
		{
			pDevice->SetSamplerState( n, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );//D3DTEXF_LINEAR );
			pDevice->SetSamplerState( n, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );// D3DTEXF_LINEAR );
			pDevice->SetSamplerState( n, D3DSAMP_MIPFILTER, D3DTEXF_NONE );//D3DTEXF_LINEAR );//D3DTEXF_POINT );
			pDevice->SetSamplerState( n, D3DSAMP_MAXANISOTROPY, 2 );
		}
		else
		{
			pDevice->SetSamplerState( n, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
			pDevice->SetSamplerState( n, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
			pDevice->SetSamplerState( n, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );//D3DTEXF_POINT );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static NWin32Helper::com_ptr<IDirect3DSurface9> pCurrentRTTB, pCurrentRTZB;
static void SetRT( IDirect3DSurface9 *pTB, IDirect3DSurface9 *pZB )
{
	ASSERT( pTB && pZB );
	// short cut
	if ( pCurrentRTTB == pTB && pCurrentRTZB == pZB )
		return;
	HRESULT hr;
	// actually this was required only for textures that are rendertargets
	for ( int k = 0; k < 8; ++k )
		pDevice->SetTexture( k, 0 ); 
	//hr = pDevice->EndScene(); ASSERT( hr == D3D_OK );
	if ( pCurrentRTTB != pTB )
	{
		pCurrentRTTB = pTB;
		hr = pDevice->SetRenderTarget( 0, pTB );
		ASSERT( D3D_OK == hr );
	}
	if ( pCurrentRTZB != pZB )
	{
		pCurrentRTZB = pZB;
		hr = pDevice->SetDepthStencilSurface( pZB );
		ASSERT( D3D_OK == hr );
	}
	//hr = pDevice->BeginScene(); ASSERT( hr == D3D_OK );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRenderContext
////////////////////////////////////////////////////////////////////////////////////////////////////
static const CRenderContext *pCurrentRenderContext;
CRenderContext::CRenderContext()
	: alpha(COMBINE_NONE), stencil( SStencilMode(STENCIL_NONE) ), depth(DEPTH_NORMAL), 
	cull(CULL_CW), colorWrite( COLORWRITE_ALL ), targetMode( RTM_SCREEN ),
	pTarget(0), nMipLevel(0), pPixelShader(&psDiffuse), pVertexShader(&vsPureGeometry),
	nRegister(0), pOutstandingStream(0)
{
	Identity( &transform.forward );
	Identity( &transform.backward );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRenderContext::~CRenderContext()
{
	ASSERT( pOutstandingStream == 0 );
	if ( this == pCurrentRenderContext )
		pCurrentRenderContext = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetTransform( const SFBTransform &_transform )
{
	ASSERT( pOutstandingStream == 0 );
	transform = _transform;
	if ( pCurrentRenderContext == this )
		transformMode.Set( transform );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetAlphaCombine( EAlphaCombineMode mode )
{
	ASSERT( pOutstandingStream == 0 );
	alpha = mode;
	if ( pCurrentRenderContext == this )
		alphaMode.Set( alpha );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetStencil( const SStencilMode &m )
{
	ASSERT( pOutstandingStream == 0 );
	stencil = m;
	if ( pCurrentRenderContext == this )
		stencilMode.Set( stencil );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetDepth( EDepthMode mode )
{
	ASSERT( pOutstandingStream == 0 );
	depth = mode;
	if ( pCurrentRenderContext == this )
		depthMode.Set( depth );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetCulling( ECullMode mode )
{
	ASSERT( pOutstandingStream == 0 );
	cull = mode;
	if ( pCurrentRenderContext == this )
		cullMode.Set( cull );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetColorWrite( EColorWriteMask mode )
{
	ASSERT( pOutstandingStream == 0 );
	colorWrite = mode;
	if ( pCurrentRenderContext == this )
		colorMode.Set( mode );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetScreenRT()
{
	ASSERT( pOutstandingStream == 0 );
	targetMode = RTM_SCREEN;
	pTarget = 0;
	pCubeTarget = 0;
	if ( pCurrentRenderContext == this )
		ApplyRenderTarget();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetTextureRT( CTexture *pTexture, int _nMipLevel )
{
	ASSERT( pOutstandingStream == 0 );
	targetMode = RTM_TEXTURE;
	pTarget = pTexture;
	pCubeTarget = 0;
	nMipLevel = _nMipLevel;
	if ( pCurrentRenderContext == this )
		ApplyRenderTarget();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetCubeTextureRT( CCubeTexture *pTexture, EFace nFace, int _nMipLevel )
{
	ASSERT( pOutstandingStream == 0 );
	targetMode = RTM_CUBETEXTURE;
	pTarget = 0;
	pCubeTarget = pTexture;
	nMipLevel = _nMipLevel;
	nRegister = nFace;
	if ( pCurrentRenderContext == this )
		ApplyRenderTarget();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetVirtualRT()
{
	ASSERT( pOutstandingStream == 0 );
	targetMode = RTM_REGISTERS;
	pTarget = 0;
	pCubeTarget = 0;
	nRegister = 0;
	if ( pCurrentRenderContext == this )
		ApplyRenderTarget();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetRegister( int _nRegister )
{
	ASSERT( pOutstandingStream == 0 );
	ASSERT( targetMode == RTM_REGISTERS );
	if ( _nRegister >= 0 && _nRegister < nScreenRegisters )
		nRegister = _nRegister;
	else
		ASSERT(0);
	if ( pCurrentRenderContext == this )
		ApplyRenderTarget();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetRT( IDirect3DSurface9 *pTB, int nSize )
{
	CDepthHash::iterator i = sharedZBuffers.find( nSize );
	if ( i == sharedZBuffers.end() )
	{
		int nMax = 1000000;
		for ( CDepthHash::iterator k = sharedZBuffers.begin(); k != sharedZBuffers.end(); ++k )
		{
			if ( k->first > nSize && k->first < nMax )
			{
				i = k;
				nMax = k->first;
			}
		}
		if ( i == sharedZBuffers.end() )
		{
			ASSERT( 0 && "no suitable zbuffer found!" );
			return;
		}
	}
	SetRT( pTB, i->second );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::ApplyRenderTarget() const
{
	switch ( targetMode )
	{
	case RTM_SCREEN:
		SetRT( pScreenColor, pScreenDepth );
		break;
	case RTM_TEXTURE:
		if ( IsValid( pTarget ) )
		{
			NWin32Helper::com_ptr<IDirect3DSurface9> pTB;
			GetSurface( pTarget, nMipLevel, &pTB );
			//D3DSURFACE_DESC ddsd;
			//pTB->GetDesc( &ddsd );
			CDynamicCast<I2DBuffer> pTargetBuf( pTarget );
			SetRT( pTB, pTargetBuf->GetXSize() );
		}
		else
			ASSERT(0);
		break;
	case RTM_REGISTERS:
		{
			NWin32Helper::com_ptr<IDirect3DSurface9> pTB;
			GetSurface( pRegisters[nRegister], 0, &pTB );
			SetRT( pTB, pRegisterDepth );
		}
		break;
	case RTM_CUBETEXTURE:
		if ( IsValid( pCubeTarget ) )
		{
			NWin32Helper::com_ptr<IDirect3DSurface9> pTB;
			GetSurface( pCubeTarget, (EFace)nRegister, nMipLevel, &pTB );
			//D3DSURFACE_DESC ddsd;
			//pTB->GetDesc( &ddsd );
			CDynamicCast<ICubeBuffer> pTargetBuf( pCubeTarget );
			SetRT( pTB, pTargetBuf->GetSize() );
		}
		else
			ASSERT(0);
		break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*void CRenderContext::ClearTarget( DWORD dwColor, int nFlags )
{
	ASSERT( pOutstandingStream == 0 );
	HRESULT hr;
	switch ( targetMode )
	{
	case RTM_SCREEN:
		Use();
		hr = pDevice->Clear( 0, 0, D3DCLEAR_TARGET|nFlags, dwColor, 1, 0 );
		ASSERT( D3D_OK == hr );
		break;
	case RTM_TEXTURE:
		Use();
		hr = pDevice->Clear( 0, 0, D3DCLEAR_TARGET|nFlags, dwColor, 1, 0 );
		ASSERT( D3D_OK == hr );
		break;
	case RTM_REGISTERS:
		{
			//D3DRECT r;
			//r.x1 = 0; r.x2 = rViewport.Width();
			//r.y1 = 0; r.y2 = rViewport.Height();
			int nReg = nRegister;
			for ( int k = 0; k < 2nScreenRegisters; ++ k )
			{
				DWORD dwFlags = k == 0 ? D3DCLEAR_TARGET|nFlags : D3DCLEAR_TARGET;//0;
				//if ( !dwFlags )
				//	continue;
				DWORD dwClearColor = k == 0 ? dwColor : 0;
				nRegister = k;
				ApplyRenderTarget();
				//hr = pDevice->Clear( 1, &r, dwFlags, dwColor, 1, 0 ); // nVidia does not recommend this
				hr = pDevice->Clear( 0, 0, dwFlags, dwClearColor, 1, 0 );
				ASSERT( D3D_OK == hr );
			}
			nRegister = nReg;
		}
		break;
	case RTM_CUBETEXTURE:
		Use();
		hr = pDevice->Clear( 0, 0, D3DCLEAR_TARGET|nFlags, dwColor, 1, 0 );
		ASSERT( D3D_OK == hr );
		break;
	}
	pCurrentRenderContext = 0; // to set correct render target when rc is used
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::ClearTarget( DWORD dwColor )
{
	ASSERT( pOutstandingStream == 0 );
	HRESULT hr;
	Use();
	hr = pDevice->Clear( 0, 0, D3DCLEAR_TARGET, dwColor, 1, 0 );
	ASSERT( D3D_OK == hr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::ClearBuffers( DWORD dwColor )
{
	ASSERT( pOutstandingStream == 0 );
	HRESULT hr;
	Use();
	hr = pDevice->Clear( 0, 0, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, dwColor, 1, 0 );
	ASSERT( D3D_OK == hr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::ClearZBuffer()
{
	ASSERT( pOutstandingStream == 0 );
	HRESULT hr;
	Use();
	hr = pDevice->Clear( 0, 0, D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 0, 1, 0 );
	ASSERT( D3D_OK == hr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetPixelShader( const SPShader &s )
{
	ASSERT( pOutstandingStream == 0 );
	pPixelShader = &s;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetVertexShader( const SVShader &s )
{
	ASSERT( !bTnLDevice );
	ASSERT( pOutstandingStream == 0 );
	ASSERT( !IsTnLDevice() );
	pVertexShader = &s;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetVSConst( int nReg, const CVec4 *pData, int nSize )
{
	ASSERT( pOutstandingStream == 0 );
	ASSERT( pCurrentRenderContext == this );
	NGfx::SetVSConst( nReg, pData, nSize );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetVSConst( int nReg, const CVec3 &_param )
{
	ASSERT( pOutstandingStream == 0 );
	CVec4 a( _param, 0 );
	NGfx::SetVSConst( nReg, &a, 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetPSConst( int nReg, const CVec4 *pData, int nSize )
{
	ASSERT( pOutstandingStream == 0 );
	ASSERT( pCurrentRenderContext == this );
	NGfx::SetPSConst( nReg, pData, nSize );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetPSConst( int nReg, const CVec3 &_param )
{
	ASSERT( pOutstandingStream == 0 );
	CVec4 a( _param, 0 );
	NGfx::SetPSConst( nReg, &a, 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetAlphaRef( int nRef )
{
	ASSERT( pOutstandingStream == 0 );
	ASSERT( pCurrentRenderContext == this );
	ApplyRenderState( D3DRS_ALPHAREF, nRef );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetTexture( int nStage, CTexture *pTex, bool bPointFilter )
{
	ASSERT( pOutstandingStream == 0 );
	ASSERT( nStage < 8 );
	ASSERT( pCurrentRenderContext == this );
	NGfx::SetTexture( nStage, pTex );
	if ( !pTex )
		return;
	SetTextureWrap( nStage, IsWrapped( pTex ) );
	SetTexturePointFilter( nStage, bPointFilter );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetTexture( int nStage, CCubeTexture *pTex )
{
	ASSERT( pOutstandingStream == 0 );
	ASSERT( pCurrentRenderContext == this );
	NGfx::SetTexture( nStage, pTex );
	SetTextureWrap( nStage, true );
	SetTexturePointFilter( nStage, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::SetLightParams( const CVec3 &vAmbient, const CVec3 &vLight, const CVec3 &vDir )
{
	ASSERT( pOutstandingStream == 0 );
	ASSERT( pCurrentRenderContext == this );
	ASSERT( IsTnLDevice() );
	D3DLIGHT9 l;
	Zero( l );
	l.Type = D3DLIGHT_DIRECTIONAL;
	l.Ambient = *(const D3DCOLORVALUE*)&vAmbient;
	l.Diffuse = *(const D3DCOLORVALUE*)&vLight;
	l.Direction.x = -vDir.x; l.Direction.y = -vDir.y; l.Direction.z = -vDir.z;
	HRESULT hr = pDevice->SetLight( 0, &l );
	ASSERT( hr == D3D_OK );
	hr = pDevice->LightEnable( 0, TRUE );
	ASSERT( hr == D3D_OK );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::Use() const
{
	if ( pCurrentRenderContext == this )
		return;
	pCurrentRenderContext = this;
	transformMode.Set( transform );
	alphaMode.Set( alpha );
	stencilMode.Set( stencil );
	depthMode.Set( depth );
	cullMode.Set( cull );
	colorMode.Set( colorWrite );
	ApplyRenderTarget();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void DoValidateDevice( int nPID, int nVID )
{
	HRESULT hr;
	DWORD dwPasses;
	hr = pDevice->ValidateDevice( &dwPasses );
	char szBuf[1024];
	sprintf( szBuf, "D3D validate device failed ps = %d,  vs = %d", nPID, nVID );
	if ( D3D_OK != hr )
		MessageBox( 0, szBuf, 0, MB_OK );
	ASSERT( D3D_OK == hr && dwPasses == 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::StartStream( CGeometry *pGeom )
{
	ASSERT( pOutstandingStream == 0 );
	ASSERT( pCurrentRenderContext == this );
	ASSERT( pPixelShader );
	ASSERT( pVertexShader );
	NGfx::SetPixelShader( *pPixelShader );
	pGeom->SetVertexStream();
	pOutstandingStream = pGeom->GetVertexStream();
	NGfx::SetVertexShader( pGeom, *pVertexShader );
	if ( bDoValidateDevice )
		DoValidateDevice( pPixelShader->nID, pVertexShader->nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::CheckStream( CGeometry *pGeom )
{
	if ( pOutstandingStream )
	{
		if ( pGeom->GetVertexStream() != pOutstandingStream ) 
		{
			Flush();
			StartStream( pGeom );
		}
	}
	else
		StartStream( pGeom );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::DrawPrimitive( CGeometry *pGeom, CTriList *pTris, int nStartVertex, int nVertices )
{
	if ( !pGeom || !pTris )
		return;
	CheckStream( pGeom );
	AddPrimitiveGeometry( pGeom, pTris, nStartVertex, nVertices );
	Flush();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::AddPrimitive( CGeometry *pGeom, CTriList *pTris )
{
	if ( !pGeom || !pTris )
		return;
	CheckStream( pGeom );
	AddPrimitiveGeometry( pGeom, pTris );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::AddPrimitive( CGeometry *pGeom, const STriangleList *pTris, int nCount, unsigned nMask )
{
	if ( !pGeom )
		return;
	CheckStream( pGeom );
	AddPrimitiveGeometry( pGeom, pTris, nCount, nMask );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::AddPrimitive( CGeometry *pGeom, const STriangleList &tris )
{
	AddPrimitive( pGeom, &tris, 1, 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*void CRenderContext::AddPrimitive( CGeometry *pGeom, const vector<STriangle> &tris )
{
	AddPrimitive( pGeom, STriangleList( &tris[0], tris.size() ) );
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::Flush()
{
	FlushPrimitive();
	pOutstandingStream = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderContext::DrawLineStrip( CGeometry *pGeom )
{
	if ( !pGeom )
		return;
	ASSERT( pCurrentRenderContext == this );
	ASSERT( pGeom );
	ASSERT( pPixelShader );
	ASSERT( pVertexShader );
	NGfx::SetPixelShader( *pPixelShader );
	pGeom->SetVertexStream();
	NGfx::SetVertexShader( pGeom, *pVertexShader );
	if ( bDoValidateDevice )
		DoValidateDevice( pPixelShader->nID, pVertexShader->nID );
	NGfx::DrawLineStrip( pGeom );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// GLOBAL FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetWireframe( EWireframe wire )
{
	wireframeMode.Set( wire );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetRegisterSize( CTRect<float> *pRes )
{
	pRes->x1 = 0; pRes->x2 = ptRegisterBufferSize.x;
	pRes->y1 = 0; pRes->y2 = ptRegisterBufferSize.y;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTexture* GetRegisterTexture( int nRegister )
{
	ASSERT( nRegister >= 0 && nRegister < nScreenRegisters );
	return pRegisters[nRegister];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsNVidiaNP2Bug() 
{ 
	return bNVHackNP2; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// INITIALISATION / FINALISATION CODE
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool AddZBuffer( D3DFORMAT format, int nSize )
{
	CDepthHash::iterator i = sharedZBuffers.find( nSize );
	if ( i == sharedZBuffers.end() )
	{
		HRESULT hr;
		hr = pDevice->CreateDepthStencilSurface( nSize, nSize, format, D3DMULTISAMPLE_NONE, 0,
			TRUE, sharedZBuffers[ nSize ].GetAddr(), 0 );
		if FAILED(hr)
			return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool InitZBuffer( D3DFORMAT format )
{
	HRESULT hr;
	nScreenRegisters = rtInfo.nRegisters;
	if ( nScreenRegisters > 0 )
	{
		CVec2 ptSize = GetScreenRect();
		int nXRSize = Float2Int( ptSize.x ), nXSize;
		int nYRSize = Float2Int( ptSize.y ), nYSize;
		ptScreenSize = CTPoint<int>( nXRSize, nYRSize );
		nXSize = 1024;
		nYSize = 512;
		if ( !bBanNP2 && (devCaps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) != 0 || (devCaps.TextureCaps & D3DPTEXTURECAPS_POW2) == 0 )
		{
#ifdef _MAPEDIT
			//bUseSeparateZBuffer = false;
			nXSize = nXRSize;
			nYSize = nYRSize;
#else
			nXSize = nXRSize;
			nYSize = Float2Int( nYRSize * (564.0f / 768.0f ) );
#endif
		}
		ptRegisterBufferSize = CTPoint<int>( nXSize, nYSize );
		hr = pDevice->CreateDepthStencilSurface( nXSize, nYSize, format, D3DMULTISAMPLE_NONE, 0,
			FALSE, pRegisterDepth.GetAddr(), 0 );
		if FAILED(hr)
			return false;
		ASSERT( nScreenRegisters <= N_MAX_REGISTERS );
		for ( int k = 0; k < nScreenRegisters; ++k )
			pRegisters[k] = MakeRenderTarget( nXSize, nYSize, SPixel8888::ID );
	}
	for (unordered_map<int,int>::iterator i = rtInfo.targets.begin(); i != rtInfo.targets.end(); ++i )
	{
		if ( !AddZBuffer( format, i->first ) )
			return false;
	}
	for (unordered_map<int,int>::iterator i = rtInfo.cubeTargets.begin(); i != rtInfo.cubeTargets.end(); ++i )
	{
		if ( !AddZBuffer( format, i->first ) )
			return false;
	}
	
	hr = pDevice->GetRenderTarget( 0, pScreenColor.GetAddr() );
	ASSERT( D3D_OK == hr );
	hr = pDevice->GetDepthStencilSurface( pScreenDepth.GetAddr() );
	ASSERT( D3D_OK == hr );
	// ritual nVidia doesn't work without
	for ( int k = 0; k < nScreenRegisters; ++ k )
	{
		NWin32Helper::com_ptr<IDirect3DSurface9> pTB;
		GetSurface( pRegisters[k], 0, &pTB );
		SetRT( pTB, pRegisterDepth );
		hr = pDevice->Clear( 0, 0, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER, 0, 1, 0 );
		ASSERT( D3D_OK == hr );
	}
	SetRT( pScreenColor, pScreenDepth );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void DoneZBuffer()
{
	pCurrentRTTB = 0;
	pCurrentRTZB = 0;
	pCurrentRenderContext = 0;
	pScreenDepth = 0;
	sharedZBuffers.clear();
	for ( int i = 0; i < nScreenRegisters; ++i )
		pRegisters[i] = 0;
	pRegisterDepth = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void InitTextureStage( int n )
{
	if ( bUseAnisotropy )
	{
		pDevice->SetSamplerState( n, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );//D3DTEXF_LINEAR );
		pDevice->SetSamplerState( n, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );// D3DTEXF_LINEAR );
		pDevice->SetSamplerState( n, D3DSAMP_MIPFILTER, D3DTEXF_NONE );//D3DTEXF_LINEAR );//D3DTEXF_POINT );
		pDevice->SetSamplerState( n, D3DSAMP_MAXANISOTROPY, 2 );
	}
	else
	{
		pDevice->SetSamplerState( n, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
		pDevice->SetSamplerState( n, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
		pDevice->SetSamplerState( n, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );//D3DTEXF_POINT );
	}
	float fMipBias = 0;//-1;
	pDevice->SetSamplerState( n, D3DSAMP_MIPMAPLODBIAS, *(DWORD*)&fMipBias );
	bLastUsedAddressMode[n] = false;
	bPointAddress[n] = false;
	pDevice->SetSamplerState( n, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
	pDevice->SetSamplerState( n, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void InitStateBlocks()
{
	ASSERT( ARRAY_SIZE(pixelShaders) >= ARRAY_SIZE(psAllShaders) );
	ASSERT( ARRAY_SIZE(vertexShaders) >= ARRAY_SIZE(vsAllShaders) );
	ASSERT( ARRAY_SIZE(vertexDeclarations) >= ARRAY_SIZE(geometryFormatInfo) );
	ZERO_ARRAY( pixelShaders );
	ZERO_ARRAY( vertexShaders );
	ZERO_ARRAY( vertexDeclarations );
	if ( bHardwarePixelShaders )
	{
		for ( int k = 0; k < ARRAY_SIZE(psAllShaders); ++k )
		{
			DWORD *pShader = bHardwarePixelShaders14 ? psAllShaders[k]->pShader14 : psAllShaders[k]->pShader;
			if ( pShader )
			{
				HRESULT hr;
				hr = pDevice->CreatePixelShader( 
					pShader,
					pixelShaders[k].GetAddr() );
				ASSERT( D3D_OK == hr );
			}
		}
	}
	if ( !bTnLDevice )
	{
		for ( int k = 0; k < ARRAY_SIZE(vsAllShaders); ++k )
		{
			DWORD *pShader = vsAllShaders[k]->pShader;
			if ( pShader )
			{
				HRESULT hr;
				hr = pDevice->CreateVertexShader( 
					pShader,
					vertexShaders[k].GetAddr() );
				ASSERT( D3D_OK == hr );
			}
		}

		for ( int k = 0; k < ARRAY_SIZE(geometryFormatInfo); ++k )
		{
			const D3DVERTEXELEMENT9 *p = geometryFormatInfo[k].pdwVSD;
			if ( p )
			{
				HRESULT hr;
				hr = pDevice->CreateVertexDeclaration( 
					p,
					vertexDeclarations[k].GetAddr() );
				ASSERT( D3D_OK == hr );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsPow2( int n ) { return GetNextPow2(n) == n; }
inline bool IsPow2( const CTPoint<int> &t ) { return IsPow2(t.x) && IsPow2(t.y); }
static CVec4 GetRegisterMapScale( const CTPoint<int> &vRegSize )
{
	//const CTRect<int> &vp = ptRegisterBufferSize; //rViewport;//ptRegisterSize;
	//float fXHalf = vp.Width() * 0.5f, fYHalf = vp.Height() * 0.5f;
	CTPoint<int> buf;
	if ( !IsPow2(vRegSize) && bNVHackNP2 )
		buf = CTPoint<int>(1,1);
	else
		buf = vRegSize; // true register size
	float fXHalf = vRegSize.x * 0.5f, fYHalf = vRegSize.y * 0.5f;
	return CVec4( fXHalf/buf.x, -fYHalf/buf.y, fXHalf/buf.x + 0.5/buf.x, fYHalf/buf.y + 0.5/buf.y );// + (1-600.0/1024) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static CVec4 GetRegisterMapScale()
{
	//ASSERT( targetMode == RTM_REGISTERS );
	return GetRegisterMapScale( ptRegisterBufferSize );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void InitEffects()
{
	// some universal initialisation
	if ( !bHardwareVP )
		pDevice->SetSoftwareVertexProcessing( TRUE );
	//pDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
	pDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
	pDevice->SetRenderState( D3DRS_DITHERENABLE, TRUE );
	renderStates.clear();
	samplerStates.clear();
	for ( int nTemp = 0; nTemp < 8; nTemp++ )
		InitTextureStage( nTemp );
	
	InitStateBlocks();
	nLastUsedVShader = -1;
	nLastUsedVDeclaration = -1;
	pCurrentPixelShader = 0;
	//pCurrentVertexShader = &vsDiffuse;
	if ( IsTnLDevice() )
	{
		D3DMATERIAL9 mat;
		Zero( mat );
		D3DCOLORVALUE white;
		white.r = white.g = white.b = white.a = 1;
		mat.Ambient = white;
		mat.Diffuse = white;
		pDevice->SetMaterial( &mat );
	}
	else
	{
		CVec4 c[9];
		c[0] = CVec4(0,0,0,0);
		c[1] = CVec4(1,1,1,1);
		c[2] = CVec4(0.5f, 1, 2, 4);
		c[3] = CVec4( 2.0f * 255 / 254, -256.0f / 254, 0, 0 );
		const float F_POINT_FALLOFF = 6;
		c[4] = CVec4(0.25, 1/16.0, F_POINT_FALLOFF, 4096 );
		c[5] = CVec4( 0.5f * F_POINT_FALLOFF * F_POINT_FALLOFF, 0.5f, 0, 0 );
		c[6] = CVec4( 1.0f / N_VEC_FULL_TEX_SIZE, 1.0f/65536, 0.5f, 0 );
		c[7] = GetRegisterMapScale();
		SetVSConst( 0, &c[0], 8 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT InitRender()
{
	InitEffects();
	HRESULT hr;
	wireframeMode.DoApply();
	transformMode.DoApply();
	alphaMode.DoApply();
	stencilMode.DoApply();
	depthMode.DoApply();
	cullMode.DoApply();
	colorMode.DoApply();
	//
	hr = pDevice->BeginScene();
	ASSERT( D3D_OK == hr );
	return D3D_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void DoneEffects()
{
	ZERO_ARRAY( pixelShaders );
	ZERO_ARRAY( vertexShaders );
	ZERO_ARRAY( vertexDeclarations );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void DoneRender()
{
	HRESULT hr;
	hr = pDevice->EndScene();
	ASSERT( D3D_OK == hr );
	pScreenColor = 0;
	pScreenDepth = 0;
	DoneEffects();
	pDevice->SetVertexShader( 0 );
	pDevice->SetPixelShader( 0 );
	pDevice->SetVertexDeclaration( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
