#include "StdAfx.h"
#include <D3D9.h>
#include "..\Misc\HPTimer.h"
#include "..\Misc\2DArray.h"
#include "..\MiscDll\Commands.h"
#include "Gfx.h"
#include "GfxInternal.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGfx
{
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SVideoModeInfo
{
	D3DDISPLAYMODE mode;
	SVideoMode info;
	
	SVideoModeInfo() {}
	SVideoModeInfo( D3DDISPLAYMODE &_m, SVideoMode &_info ) : mode(_m), info(_info) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
NWin32Helper::com_ptr<IDirect3D9> pD3D;
NWin32Helper::com_ptr<IDirect3DDevice9> pDevice;
SRenderStats renderStats;
bool bHardwareVP, bHardwarePixelShaders, bHardwarePixelShaders14;
bool bTnLDevice = false;
// Render-config flag: when set, dynamic 2D textures (e.g. the Bink movie surface) use a
// 16-bit SPixel1555 surface instead of 32-bit SPixel8888.  This tree's render path is
// always 32-bit, so it defaults false (matches release Gfx.obj @0x10ce10).
bool b16BitTexturesNow = false;
bool Is16BitTextures() { return b16BitTexturesNow; }   // @0x10ce10: return b16BitTexturesNow;
static bool bForbidPS = false, bForceSWVP = false, bGammaIsSet = false;
bool bNVHackNP2Cfg = false, bNVHackNP2, bUseAnisotropy = false, bBanNP2 = false, bStaticNooverwrite = true;
bool bBan32BitIndices = true;
bool bNoCubeMapMipLevels = false;
int nVCacheSize = 10;
static SVideoMode videoMode;
static D3DPRESENT_PARAMETERS pp;
static unsigned char nGammaCorrection[256];
D3DCAPS9 devCaps;
SRenderTargetsInfo rtInfo;
static HWND hWnd;
static vector<SVideoModeInfo> videoModes;
HWND GetHWND() { return hWnd; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// forward declarations
static D3DFORMAT GetZBufferFormat( D3DFORMAT rTarget );
////////////////////////////////////////////////////////////////////////////////////////////////////
static void DestroyLostableDXObjects()
{
	DoneRender();
	DestroyLostableBuffers();
	DoneZBuffer();
}
static void DestroyManagedDXObjects()
{
	DestroyManagedBuffers();
}
static HRESULT InitDXObjects()
{
	// init itself
	InitZBuffer( GetZBufferFormat( pp.BackBufferFormat ) );
	InitBuffers();
	return InitRender();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool bDeviceCreated = false;
////////////////////////////////////////////////////////////////////////////////////////////////////
const _D3DDEVTYPE DEVICE_TYPE = D3DDEVTYPE_HAL;//D3DDEVTYPE_REF;//
static HRESULT ResetDevice()
{
	HRESULT hr;
	bNVHackNP2 = bNVHackNP2Cfg;
	if ( !bDeviceCreated )
	{
		int nForceTnLDevice = Float2Int( NGlobal::GetVar( "gfx_tnl_mode", -1 ).GetFloat() );
		bTnLDevice = true;

		// determine device class
		if ( nForceTnLDevice == 1 || ( (devCaps.VertexShaderVersion & 0xFFFF) == 0 && nForceTnLDevice != -1 ) )
			bTnLDevice = true;
		else
			bTnLDevice = false;
		// initialize device and determine if vertex processing is hardware
		hr = -1;
		if ( !bForceSWVP )
		{
			bHardwareVP = true;
			hr = pD3D->CreateDevice(
				D3DADAPTER_DEFAULT, 
				DEVICE_TYPE,
				hWnd,
#if defined(_DEBUG) && !defined(FAST_DEBUG)
				D3DCREATE_HARDWARE_VERTEXPROCESSING,
#else
				D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE,
#endif
				&pp,
				pDevice.GetAddr() );
			ASSERT( SUCCEEDED( hr ) );
		}
		if FAILED( hr )
		{
			bHardwareVP = false;
			hr = pD3D->CreateDevice( 
				D3DADAPTER_DEFAULT, 
				DEVICE_TYPE,
				hWnd, 
				D3DCREATE_SOFTWARE_VERTEXPROCESSING,
				&pp,
				pDevice.GetAddr() );
			ASSERT( SUCCEEDED( hr ) );
		}
		if ( !bHardwareVP )
			bNVHackNP2 = false;
		bBan32BitIndices = devCaps.MaxVertexIndex < 1000000 || !bHardwareVP; // ban streaming actually
	}
	else
	{
		DestroyLostableDXObjects();
		hr = pDevice->Reset( &pp );
	}
	{
		D3DDEVINFO_VCACHE vcache;
		Zero( vcache );
		IDirect3DQuery9 *pQ;
		HRESULT hr = pDevice->CreateQuery( D3DQUERYTYPE_VCACHE, &pQ );
		if ( SUCCEEDED(hr) )
		{
			pQ->Issue( D3DISSUE_BEGIN );
			while ( pQ->GetData( &vcache, sizeof(vcache), D3DGETDATA_FLUSH ) != S_OK )
				Sleep(0);
			pQ->Release();
			if ( vcache.Pattern == 0x48434143 )
				nVCacheSize = vcache.CacheSize;
		}
	}
	bGammaIsSet = false;
	SetWindowPos( 
		hWnd, 
		HWND_NOTOPMOST, 
		0, 0, pp.BackBufferWidth, pp.BackBufferHeight, 
		SWP_SHOWWINDOW );
	if ( bTnLDevice )
		rtInfo.Clear();
	if ( hr == D3D_OK )
	{
		hr = InitDXObjects();
		bDeviceCreated = true;
	}
	return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void DeviceFinalRelease()
{
	DestroyLostableDXObjects();
	DestroyManagedDXObjects();
	pD3D = 0;
	pDevice = 0;
	bDeviceCreated = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// Utility functions
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetBpp( D3DFORMAT format )
{
	switch ( format )
	{
		case D3DFMT_R8G8B8:   return 24;
    case D3DFMT_A8R8G8B8: return 32;
    case D3DFMT_X8R8G8B8: return 32;
    case D3DFMT_R5G6B5:   return 16;
    case D3DFMT_X1R5G5B5: return 16;
    case D3DFMT_A1R5G5B5: return 16;
    case D3DFMT_A4R4G4B4: return 16;
    case D3DFMT_R3G3B2:   return 8;
    case D3DFMT_A8:       return 8;
    case D3DFMT_A8R3G3B2: return 16;
    case D3DFMT_X4R4G4B4: return 16;

    case D3DFMT_A8P8:     return 16;
    case D3DFMT_P8:       return 8;

    case D3DFMT_L8:       return 8;
    case D3DFMT_A8L8:     return 16;
    case D3DFMT_A4L4:     return 8;

    case D3DFMT_V8U8:     return 16;
    case D3DFMT_L6V5U5:   return 16;
    case D3DFMT_X8L8V8U8: return 32;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetZBpp( D3DFORMAT format )
{
	switch ( format )
	{
		case D3DFMT_D16_LOCKABLE: return 16;
    case D3DFMT_D32:          return 32;
    case D3DFMT_D15S1:        return 16;
    case D3DFMT_D24S8:        return 32;
    case D3DFMT_D16:          return 16;
    case D3DFMT_D24X8:        return 32;
    case D3DFMT_D24X4S4:      return 32;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// DX initialisation/finalisation
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool TestZBufferFormat( D3DFORMAT screen, D3DFORMAT rTarget, D3DFORMAT zBuf )
{
	if ( D3D_OK != pD3D->CheckDeviceFormat( 
		D3DADAPTER_DEFAULT, 
		DEVICE_TYPE,
		screen, 
		D3DUSAGE_DEPTHSTENCIL,
		D3DRTYPE_SURFACE,
		zBuf ) )
		return false;
	return D3D_OK == pD3D->CheckDepthStencilMatch( 
		D3DADAPTER_DEFAULT, 
		DEVICE_TYPE,
		screen, 
		rTarget, 
		zBuf );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool TestRTargetFormat( D3DFORMAT screen, D3DFORMAT rTarget )
{
	if ( D3D_OK != pD3D->CheckDeviceFormat( 
		D3DADAPTER_DEFAULT, 
		DEVICE_TYPE,
		screen,
		D3DUSAGE_RENDERTARGET,
		D3DRTYPE_TEXTURE,
		rTarget ) )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static D3DFORMAT GetZBufferFormat( D3DFORMAT screen, D3DFORMAT rTarget )
{
	if ( GetBpp( rTarget ) > 16 )
	{
		if ( TestZBufferFormat( screen, rTarget, D3DFMT_D24S8 ) )
			return D3DFMT_D24S8;
		if ( TestZBufferFormat( screen, rTarget, D3DFMT_D24X4S4 ) )
			return D3DFMT_D24X4S4;
		if ( TestZBufferFormat( screen, rTarget, D3DFMT_D32 ) )
			return D3DFMT_D32;
		if ( TestZBufferFormat( screen, rTarget, D3DFMT_D24X8 ) )
			return D3DFMT_D24X8;
	}
	if ( TestZBufferFormat( screen, rTarget, D3DFMT_D16 ) )
		return D3DFMT_D16;
	ASSERT(0);
	return D3DFMT_UNKNOWN;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static D3DFORMAT GetZBufferFormat( D3DFORMAT rTarget )
{
	return GetZBufferFormat( pp.BackBufferFormat, rTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// to track rescaling
static void GetBackBufferSize()
{
  RECT windowPos;
  GetClientRect( pp.hDeviceWindow, &windowPos );
  pp.BackBufferWidth = windowPos.right;
  pp.BackBufferHeight = windowPos.bottom;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CheckBackBufferSize()
{
	RECT windowPos;
	GetClientRect( pp.hDeviceWindow, &windowPos );

	if ( !IsWindowVisible( pp.hDeviceWindow ) )
		return;
	if ( windowPos.bottom == 0 || windowPos.right == 0 )
		return;
	if ( pp.BackBufferHeight != windowPos.bottom || pp.BackBufferWidth != windowPos.right )
	{
		pp.BackBufferWidth = windowPos.right;
		pp.BackBufferHeight = windowPos.bottom;
		ResetDevice();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool FillPresent( const SVideoMode &m )
{
	HRESULT hr;
	D3DPRESENT_PARAMETERS ppOld = pp;
	memset( &pp, 0, sizeof(pp) );
	// Get the current desktop display mode of the current adapters
	D3DDISPLAYMODE desktop;
  hr = pD3D->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &desktop );
	if ( FAILED(hr) )
	{
		pp = ppOld;
		return false;
	}
	if ( FULL_SCREEN == m.fullScreen )
	{
		// search through modes to find fitting
		bool bFound = false;
		D3DDISPLAYMODE best;
		// search for suitable mode
		for ( int i = 0; i < videoModes.size(); ++i )
		{
			if ( videoModes[i].info.nXSize == m.nXSize && videoModes[i].info.nYSize == m.nYSize && videoModes[i].info.nBpp == m.nBpp )
			{
				bFound = true;
				best = videoModes[i].mode;
				break;
			}
		}
		if ( !bFound )
		{
			pp = ppOld;
			return false;
		}
		// fill structure
		pp.BackBufferWidth = m.nXSize;
		pp.BackBufferHeight = m.nYSize;
		pp.BackBufferFormat = best.Format;
		pp.BackBufferCount = 1;

		pp.MultiSampleType = D3DMULTISAMPLE_NONE;

		pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		pp.hDeviceWindow = hWnd;
		pp.Windowed = FALSE;
		pp.EnableAutoDepthStencil =	TRUE;
		pp.AutoDepthStencilFormat = GetZBufferFormat( pp.BackBufferFormat, pp.BackBufferFormat );
		//pp.EnableAutoDepthStencil =	FALSE;

		pp.FullScreen_RefreshRateInHz = best.RefreshRate;
		if ( devCaps.PresentationIntervals & D3DPRESENT_INTERVAL_IMMEDIATE )
			pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		else
			pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
		return true;
	}
	//
	// Windowed mode
	// ensure that our back buffer bit depth is the same as that of 
  // windowed display depth to work in windowed mode
  if ( GetBpp( desktop.Format ) != m.nBpp )
	{
		pp = ppOld;
		return false;
	}
	pp.BackBufferWidth = m.nXSize;
	pp.BackBufferHeight = m.nYSize;
	pp.BackBufferFormat = desktop.Format;//D3DFMT_UNKNOWN;//D3DFMT_X8R8G8B8;// D3DFMT_UNKNOWN;//D3DFMT_UNKNOWN_D24S8;
	pp.BackBufferCount = 1;

	pp.MultiSampleType = D3DMULTISAMPLE_NONE;

	pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp.hDeviceWindow = hWnd;
	pp.Windowed = TRUE;
	pp.EnableAutoDepthStencil =	TRUE;
	pp.AutoDepthStencilFormat = GetZBufferFormat( pp.BackBufferFormat, pp.BackBufferFormat );
	//pp.EnableAutoDepthStencil =	FALSE;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CheckDeviceCaps()
{
	HRESULT hr;
	pD3D->GetDeviceCaps( D3DADAPTER_DEFAULT, DEVICE_TYPE, &devCaps );
	bHardwarePixelShaders = false;
	bHardwarePixelShaders14 = false;
	{
		// CRAP detect nVidia & clear np2 workaround flag if not found
		D3DADAPTER_IDENTIFIER9 id;
		hr = pD3D->GetAdapterIdentifier( D3DADAPTER_DEFAULT, 0, &id );
		ASSERT( SUCCEEDED( hr ) );
		if ( id.VendorId == 0x10de )
		{
			if ( ( id.DeviceId & 0x100 ) == 0x100 )
				devCaps.VertexShaderVersion = 0; // ban hwvp on 4mxs
			if ( ( id.DeviceId & 0x200 ) != 0x200 )
				bNVHackNP2 = false;
		}
		else
		{
			bNVHackNP2 = false;
		}
	}
	if ( bForbidPS )
		devCaps.PixelShaderVersion = 0;
	if ( (devCaps.PixelShaderVersion & 0xFFFF ) >= 0x0101 )
		bHardwarePixelShaders = true;
	if ( (devCaps.PixelShaderVersion & 0xFFFF ) >= 0x0104 )
		bHardwarePixelShaders14 = true;
	if ( (devCaps.TextureCaps & D3DPTEXTURECAPS_MIPCUBEMAP ) == 0 )
		bNoCubeMapMipLevels = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void DetectModes( D3DFORMAT format, int nBpp )
{
	// Get the current desktop display mode of the current adapters
	D3DDISPLAYMODE desktop;
	HRESULT hr = pD3D->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &desktop );

	//bool bFound = false, bMatchScreenRefresh = false;
	for ( int i = 0; i < pD3D->GetAdapterModeCount( D3DADAPTER_DEFAULT, format ); i++ )
	{
		D3DDISPLAYMODE mode;
		pD3D->EnumAdapterModes( D3DADAPTER_DEFAULT, format, i, &mode );
		if ( mode.Height > desktop.Height || mode.RefreshRate != desktop.RefreshRate )
			mode.RefreshRate = D3DPRESENT_RATE_DEFAULT;
		SVideoMode info( mode.Width, mode.Height, nBpp, FULL_SCREEN, mode.RefreshRate );
		// replace with better mode
		bool bFound = false;
		for ( int k = 0; k < videoModes.size(); ++k )
		{
			SVideoModeInfo &v = videoModes[k];
			if ( v.mode.Width == mode.Width && v.mode.Height == mode.Height && v.mode.Format == mode.Format )
			{
				bFound = true;
				if ( mode.RefreshRate > v.mode.RefreshRate )
				{
					v.mode = mode;
					v.info = info;
				}
				break;
			}
		}
		if ( !bFound )
			videoModes.push_back( SVideoModeInfo( mode, info ) );
	}
}
static void DetectModes()
{
	videoModes.clear();
	DetectModes( D3DFMT_X8R8G8B8, 32 );
	DetectModes( D3DFMT_R5G6B5, 16 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool InitD3D()
{
	pD3D.Create( Direct3DCreate9( D3D_SDK_VERSION ) );
	if ( pD3D == 0 )
	{
		ASSERT( 0 );
		return false;
	}
	CheckDeviceCaps();
	DetectModes();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SetMode( const SVideoMode &m, const SRenderTargetsInfo &_rtInfo )
{
	if ( !FillPresent( m ) )
		return false;
	rtInfo = _rtInfo;
	videoMode = m;
	HRESULT hr = ResetDevice();
	return D3D_OK == hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetModesList( list<SVideoMode> *pRes, int nBpp )
{
	if ( videoModes.empty() )
		return;
	pRes->clear();
	for ( int k = 0; k < videoModes.size(); ++k )
	{
		if ( videoModes[k].info.nBpp == nBpp )
			pRes->push_back( videoModes[k].info );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec2 GetScreenRect()
{
	return CVec2( pp.BackBufferWidth, pp.BackBufferHeight );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeScreenShot( CArray2D<SPixel8888> *pRes, bool bCorrectGamma )
{
	HRESULT hr;
	NWin32Helper::com_ptr<IDirect3DSurface9> pSurface;
	if ( pp.BackBufferWidth == 0 || pp.BackBufferHeight == 0 )
	{
		pRes->SetSizes( 1, 1 );
		return;
	}
	D3DDISPLAYMODE desktop;
  hr = pD3D->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &desktop );
	ASSERT( D3D_OK == hr );
	hr = pDevice->CreateOffscreenPlainSurface( desktop.Width, desktop.Height, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, pSurface.GetAddr(), 0 );
	ASSERT( D3D_OK == hr );
	hr = pDevice->GetFrontBufferData( 0, pSurface );
	ASSERT( D3D_OK == hr );
	D3DLOCKED_RECT lr;
	hr = pSurface->LockRect( &lr, 0, D3DLOCK_READONLY );
	ASSERT( D3D_OK == hr );
	pRes->SetSizes( pp.BackBufferWidth, pp.BackBufferHeight );
	const char *pSrc = (const char*) lr.pBits;
	for ( int y = 0; y < pRes->GetYSize(); ++y )
	{
		memcpy( &((*pRes)[y][0]), pSrc, 4 * pRes->GetXSize() );
		pSrc += lr.Pitch;
	}
	if ( bCorrectGamma )
	{
		for ( int y = 0; y < pRes->GetYSize(); ++y )
		{
			for ( int x = 0; x < pRes->GetXSize(); ++x )
			{
				SPixel8888 &c = (*pRes)[y][x];
				c.r = nGammaCorrection[ c.r ];
				c.g = nGammaCorrection[ c.g ];
				c.b = nGammaCorrection[ c.b ];
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool bOutputFPS = false;
static float fTotalFrameTime = 0;
static int nTotalFrames = 0;
const int N_SLOW_FPS_TYPE = 31;//1023;//
void Flip()
{
	HRESULT hr = pDevice->EndScene();
	ASSERT( hr == D3D_OK );
	//
	pDevice->Present( 0, 0, hWnd, 0 );
	//
	if ( bOutputFPS )
	{
		static NHPTimer::STime timeFrameStart;
		static int nSlowOutCounter;
		double fFrameTime = NHPTimer::GetTimePassed( &timeFrameStart );
		fTotalFrameTime += fFrameTime;
		++nTotalFrames;
		if ( ((++nTotalFrames)&N_SLOW_FPS_TYPE) == 0 )
		{
			float fFPS = ( N_SLOW_FPS_TYPE + 1 ) / fTotalFrameTime;
			char szBuf[1024];
			sprintf( szBuf, "FPS = %f\n", fFPS );
			OutputDebugString( szBuf );
			nTotalFrames = 0;
			fTotalFrameTime = 0;
		}
	}
	//
	NextFrameBuffes();
  //
	hr = pDevice->BeginScene();
	ASSERT( D3D_OK == hr );
	renderStats.Clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// test cooperative level
////////////////////////////////////////////////////////////////////////////////////////////////////
static D3DGAMMARAMP keptGamma;
void SetGamma( bool bGamma )
{
	if ( !pDevice )
		return;
	if ( bGammaIsSet == bGamma )
		return;
	// set gamma
	D3DGAMMARAMP gamma;
	if ( bGamma )
	{
		pDevice->GetGammaRamp( 0, &keptGamma );
		for ( int k = 0; k < 256; ++k )
		{
			float f = k / 256.0f;
			//if ( f < 0.0031308f ) f = f * 12.92f; else 	f = 1.055f * exp( log( f ) / 2.4f ) - 0.055f;
			//if ( f < 0.026175f ) f = f * 4; else 	f = 1.1466f * exp( log( f ) / 2.4f ) - 0.1466f;
			float fGamma = NGlobal::GetVar( "gfx_gamma", 1 ).GetFloat();
			f = exp( log( f ) / fGamma );
			WORD wRes = Float2Int( f * 65535 );
			nGammaCorrection[k] = wRes >> 8;
			gamma.red[k] = wRes;
			gamma.green[k] = wRes;
			gamma.blue[k] = wRes;
		}
	}
	else
	{
		int nMax = 0;
		for ( int k = 0; k < 256; ++k )
		{
			nMax = Max( nMax, (int)keptGamma.red[k] );
			nMax = Max( nMax, (int)keptGamma.green[k] );
			nMax = Max( nMax, (int)keptGamma.blue[k] );
		}
		if ( nMax < 120 )
		{
			// suspicious gamma was returned better set to default
			for ( int k = 0; k < 256; ++k )
			{
				WORD wRes = k << 8;
				gamma.red[k] = wRes;
				gamma.green[k] = wRes;
				gamma.blue[k] = wRes;
			}
		}
		else
		{
			// for some reason gamma returned by GetGammaRamp is in different range then it is expected in SetGammaRamp
			int nShift = 0;
			while ( (nMax<<nShift) < 32768 )
				++nShift;
			for ( int k = 0; k < 256; ++k )
			{
				WORD wRes = k << 8;
				gamma.red[k] = keptGamma.red[k] << nShift;
				gamma.green[k] = keptGamma.green[k] << nShift;
				gamma.blue[k] = keptGamma.blue[k] << nShift;
			}
		}
	}
	pDevice->SetGammaRamp( 0, D3DSGR_NO_CALIBRATION, &gamma );
	bGammaIsSet = bGamma;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool Is3DActive()
{
	HRESULT hr = pDevice->TestCooperativeLevel();
	if ( hr == D3DERR_DEVICELOST )
		return false; 
	if ( hr == D3D_OK )
		return true;
	if ( pp.Windowed )
	{
		GetBackBufferSize();
		if ( !FillPresent( videoMode ) )
			return false;
	}
	ResetDevice();
	hr = pDevice->TestCooperativeLevel();
	if ( hr == D3DERR_DEVICELOST )
		return false;
	ASSERT( hr == D3D_OK );
	return hr == D3D_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool Init3D( HWND _hWnd )
{
	hWnd = _hWnd;
	if ( !InitD3D() )
	{
		ASSERT(0);
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void Done3D()
{
	DeviceFinalRelease();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Commands/Vars
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(Gfx)
	REGISTER_VAR( "gfx_tnl_mode", 0, -1, true )
	REGISTER_VAR( "gfx_gamma", 0, 1, true )
	////
	REGISTER_VAR_EX( "gfx_nopixelshaders", NGlobal::VarBoolHandler, &bForbidPS, 0, true )
	REGISTER_VAR_EX( "gfx_swvertexprocess", NGlobal::VarBoolHandler, &bForceSWVP, 0, true )
	REGISTER_VAR_EX( "gfx_validate", NGlobal::VarBoolHandler, &bDoValidateDevice, 0, true )
	REGISTER_VAR_EX( "gfx_anisotropic_filter", NGlobal::VarBoolHandler, &bUseAnisotropy, 0, true )
	REGISTER_VAR_EX( "gfx_fix_ban_np2", NGlobal::VarBoolHandler, &bBanNP2, 0, true )
	REGISTER_VAR_EX( "gfx_fix_nv_np2_hack", NGlobal::VarBoolHandler, &bNVHackNP2Cfg, 0, true )
	REGISTER_VAR_EX( "gfx_static_nooverwrite", NGlobal::VarBoolHandler, &bStaticNooverwrite, 1, true )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
}
