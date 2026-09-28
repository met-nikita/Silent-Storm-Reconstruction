#include "StdAfx.h"
#include "GfxBuffers.h"
#include "Gfx.h"
#include "GTexture.h"
#include "GPixelFormat.h"
#include "mmpFormat.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\DBFormat\DataFormat.h"
#include "SWTexture.h"
#include "..\Misc\HPTimer.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
bool bDXTModeOn = true;
static int GetRealTextureID( NDb::CTexture *pTex )
{
	int nID = pTex->GetRecordID();
	if ( bDXTModeOn )
		return nID;
	if ( pTex->bIsDXT )
		return nID | 0x01000000;
	return nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
extern int nTextureUseMip;
extern bool bLowRAM;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTextureLoader
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TPixel>
void LoadTextureData( NGfx::CTexture *pTexture, int _nX, int _nY, int _nSizeX, int _nSizeY, 
	int _nLevels, CDataStream *pFile, int nSkipMip = 0, const TPixel *p = 0 )
{
	CDynamicCast<NGfx::I2DBuffer> pTexBuffer( pTexture );
	int nLevels = Min( _nLevels, pTexBuffer->GetNumMipLevels() + nSkipMip );
	for ( int nLevel = 0; nLevel < nLevels; ++nLevel )
	{
		int nX = (_nX >> nLevel) / TPixel::XSize;
		int nY = (_nY >> nLevel) / TPixel::YSize;
		int nSizeX = (_nSizeX >> nLevel) / TPixel::XSize;
		int nSizeY = (_nSizeY >> nLevel) / TPixel::YSize;
		if ( nLevel < nSkipMip )
		{
			pFile->Seek( pFile->GetPosition() + nSizeX * nSizeY * sizeof(TPixel) );
			continue;
		}
		NGfx::CTextureLock<TPixel> lock( pTexture, nLevel - nSkipMip, NGfx::INPLACE );
		ASSERT( lock.GetXSize() >= nX + nSizeX );
		ASSERT( lock.GetYSize() >= nY + nSizeY );
		for ( int y = nY; y < nY + nSizeY; ++y )
			pFile->Read( &(lock[y][nX]), nSizeX * sizeof(TPixel) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static NGfx::EFace loadFace;
template <class TPixel>
void LoadTextureData( NGfx::CCubeTexture *pTexture, int _nX, int _nY, int _nSizeX, int _nSizeY, 
	int _nLevels, CDataStream *pFile, int nSkipMip = 0, const TPixel *p = 0 )
{
	CDynamicCast<NGfx::ICubeBuffer> pTexBuffer( pTexture );
	int nLevels = Min( _nLevels, pTexBuffer->GetNumMipLevels() );
	for ( int nLevel = 0; nLevel < nLevels; ++nLevel )
	{
		NGfx::CTextureLock<TPixel> lock( pTexture, loadFace, nLevel, NGfx::INPLACE );
		int nX = (_nX >> nLevel) / TPixel::XSize;
		int nY = (_nY >> nLevel) / TPixel::YSize;
		int nSizeX = (_nSizeX >> nLevel) / TPixel::XSize;
		int nSizeY = (_nSizeY >> nLevel) / TPixel::YSize;
		ASSERT( lock.GetXSize() >= nX + nSizeX );
		ASSERT( lock.GetYSize() >= nY + nSizeY );
		for ( int y = nY; y < nY + nSizeY; ++y )
			pFile->Read( &(lock[y][nX]), nSizeX * sizeof(TPixel) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TRes>
static bool RealLoadTexture( TRes *pTexture, CDataStream *pStream, const SMMPFileHeader &hdr,
	int nX, int nY, int nSizeX, int nSizeY, int nLevels, int nSkipMip = 0 )
{
	nLevels = Min( nLevels, hdr.nNumMipLevels );
	switch ( hdr.format )
	{
		case NGfx::CF_DXT1:			LoadTextureData<NGfx::SPixelDXT1>( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream, nSkipMip ); break;
		case NGfx::CF_DXT2:			LoadTextureData<NGfx::SPixelDXT2>( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream, nSkipMip ); break;
		case NGfx::CF_DXT3:			LoadTextureData<NGfx::SPixelDXT3>( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream, nSkipMip ); break;
		case NGfx::CF_DXT4:			LoadTextureData<NGfx::SPixelDXT4>( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream, nSkipMip ); break;
		case NGfx::CF_DXT5:			LoadTextureData<NGfx::SPixelDXT5>( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream, nSkipMip ); break;
		case NGfx::CF_R5G6B5:		LoadTextureData<NGfx::SPixel565> ( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream, nSkipMip ); break;
		case NGfx::CF_A1R5G5B5: LoadTextureData<NGfx::SPixel1555>( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream, nSkipMip ); break;
		case NGfx::CF_A4R4G4B4: LoadTextureData<NGfx::SPixel4444>( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream, nSkipMip ); break;
		case NGfx::CF_A8R8G8B8: LoadTextureData<NGfx::SPixel8888>( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream, nSkipMip ); break;
		default: return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFileTexture
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFileTexture::CreateChecker()
{
	NGfx::SPixel8888 colors[2];
	colors[0] = NGfx::SPixel8888(0,0,0,255);
	colors[1] = NGfx::SPixel8888(255,255,255,255);
	const int nSize = 128;
	pValue = NGfx::MakeTexture( nSize, nSize, 1, NGfx::SPixel8888::ID, NGfx::REGULAR, NGfx::CLAMP );
	NGfx::CTextureLock<NGfx::SPixel8888> lock( pValue, 0, NGfx::INPLACE );
	for ( int y = 0; y < nSize; ++y )
	{
		for ( int x = 0; x < nSize; ++x )
			lock[y][x] = colors[ ( (x&4) == 0 ) & ( (y&4) == 0 ) ];
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static NGfx::CTexture* MakeTexture( const SMMPFileHeader &hdr, NGfx::ETextureUsage eUsage, NGfx::EWrap eWrap, int nSkipMip = 0 )
{
	// Retail v1.2 0x57f760: only 8888 input belongs in the shared texture atlases.
	if ( ( eUsage == NGfx::TEXTURE_2D || eUsage == NGfx::TRANSPARENT_TEXTURE ) && hdr.format != NGfx::CF_A8R8G8B8 )
		eUsage = NGfx::REGULAR;
	return NGfx::MakeTexture( Max( 1, hdr.nSizeX >> nSkipMip ), Max( 1, hdr.nSizeY >> nSkipMip ), hdr.nNumMipLevels - nSkipMip,
		hdr.format, eUsage, eWrap );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail v1.2 0x57f8a0: opaque / binary alpha / continuous alpha select 565 / 1555 / 4444.
static int Select16BitFormat( const vector<NGfx::SPixel8888> &pixels, NGfx::ETextureUsage eUsage )
{
	if ( eUsage == NGfx::TEXTURE_2D || eUsage == NGfx::TRANSPARENT_TEXTURE )
		return NGfx::CF_A4R4G4B4;
	bool bOpaque = true, bBinary = true;
	for ( int i = 0; i < pixels.size(); ++i )
	{
		bOpaque &= pixels[i].a == 255;
		bBinary &= pixels[i].a == 0 || pixels[i].a == 255;
	}
	return bOpaque ? NGfx::CF_R5G6B5 : bBinary ? NGfx::CF_A1R5G5B5 : NGfx::CF_A4R4G4B4;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// The retail noise generator is local to texture conversion, not the game's RNG.
struct STextureDither
{
	DWORD values[3][10240];
	STextureDither()
	{
		DWORD nRnd = 0;
		for ( int i = 0; i < 10240; ++i )
		{
			DWORD c[4];
			for ( int k = 0; k < 4; ++k )
			{
				nRnd = ( ( nRnd << 16 ) ^ nRnd ) * 0xcd4891u + 0x476e4cebu;
				c[k] = ( nRnd >> 12 ) & 255;
			}
			values[0][i] = ( ( c[0] >> 4 ) << 24 ) | ( ( c[1] >> 4 ) << 16 ) | ( ( c[2] >> 4 ) << 8 ) | ( c[3] >> 4 );
			values[1][i] = ( ( c[1] >> 5 ) << 16 ) | ( ( c[2] >> 6 ) << 8 ) | ( c[3] >> 5 );
			values[2][i] = ( ( c[1] >> 5 ) << 16 ) | ( ( c[2] >> 5 ) << 8 ) | ( c[3] >> 5 );
		}
	}
};
static WORD Convert16BitPixel( NGfx::SPixel8888 pixel, DWORD noise, int nFormat )
{
	int b = Min( 255, (int)pixel.b + (int)( noise & 255 ) );
	int g = Min( 255, (int)pixel.g + (int)( ( noise >> 8 ) & 255 ) );
	int r = Min( 255, (int)pixel.r + (int)( ( noise >> 16 ) & 255 ) );
	int a = Min( 255, (int)pixel.a + (int)( noise >> 24 ) );
	if ( nFormat == NGfx::CF_R5G6B5 )
		return ( r >> 3 ) << 11 | ( g >> 2 ) << 5 | ( b >> 3 );
	if ( nFormat == NGfx::CF_A1R5G5B5 )
		return ( a >> 7 ) << 15 | ( r >> 3 ) << 10 | ( g >> 3 ) << 5 | ( b >> 3 );
	return ( a >> 4 ) << 12 | ( r >> 4 ) << 8 | ( g >> 4 ) << 4 | ( b >> 4 );
}
static NGfx::CTexture* LoadConvertTo16Bit( CDataStream *pStream, const SMMPFileHeader &hdr,
	NGfx::ETextureUsage eUsage, NGfx::EWrap eWrap, int nSkipMip )
{
	static const STextureDither dither;
	CObj<NGfx::CTexture> pTexture;
	int nFormat = NGfx::CF_A4R4G4B4;
	for ( int nLevel = 0; nLevel < hdr.nNumMipLevels; ++nLevel )
	{
		int nWidth = Max( 1, hdr.nSizeX >> nLevel ), nHeight = Max( 1, hdr.nSizeY >> nLevel );
		if ( nLevel < nSkipMip )
		{
			pStream->Seek( pStream->GetPosition() + nWidth * nHeight * sizeof(NGfx::SPixel8888) );
			continue;
		}
		if ( IsValid( pTexture ) )
		{
			CDynamicCast<NGfx::I2DBuffer> pBuffer( pTexture );
			if ( nLevel - nSkipMip >= pBuffer->GetNumMipLevels() )
				break;
		}
		vector<NGfx::SPixel8888> pixels( nWidth * nHeight );
		pStream->Read( &pixels[0], pixels.size() * sizeof(NGfx::SPixel8888) );
		if ( !IsValid( pTexture ) )
		{
			nFormat = Select16BitFormat( pixels, eUsage );
			pTexture = NGfx::MakeTexture( nWidth, nHeight, hdr.nNumMipLevels - nSkipMip, nFormat, eUsage, eWrap );
			if ( !IsValid( pTexture ) )
				return 0;
		}
		CDynamicCast<NGfx::I2DBuffer> pBuffer( pTexture );
		NGfx::I2DBufferLock *pLock = pBuffer->Lock( nLevel - nSkipMip, NGfx::INPLACE );
		int nDither = nFormat == NGfx::CF_R5G6B5 ? 1 : nFormat == NGfx::CF_A1R5G5B5 ? 2 : 0;
		for ( int y = 0; y < nHeight; ++y )
		{
			WORD *pRow = (WORD*)( (char*)pLock->GetBuffer() + y * pLock->GetStride() );
			for ( int x = 0; x < nWidth; ++x )
			{
				DWORD noise = nWidth <= 2048 && ( nWidth & 1 ) == 0 ? dither.values[nDither][( ( y * 0xff7 ) & 0x1ffe ) + x] : 0;
				pRow[x] = Convert16BitPixel( pixels[y * nWidth + x], noise, nFormat );
			}
		}
		delete pLock;
	}
	return pTexture.Extract();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFileTexture::Recalc()
{
	if ( IsValid(pRequest) && !pRequest->IsReady() && IsValid( pValue ) )
		return;

	NDb::CTexture *pTex = NDb::GetTexture( GetKey().nID );
	if ( !pTex )
	{
		CreateChecker();
		return;
	}
	NGfx::ETextureUsage eUsage;
	switch ( pTex->usage )
	{
		case NDb::CTexture::TEXTURE_USAGE_ORDINARY: eUsage = NGfx::REGULAR; break;
		case NDb::CTexture::TEXTURE_USAGE_2D: eUsage = NGfx::TEXTURE_2D; break;
		case NDb::CTexture::TEXTURE_USAGE_TRANSPARENT: eUsage = NGfx::REGULAR; break;
		default: ASSERT(0); eUsage = NGfx::REGULAR; break;
	}
	if ( GetKey().nFlags & STextureKey::TK_TRANSPARENT )
		eUsage = NGfx::TRANSPARENT_TEXTURE;
	NGfx::EWrap eWrap = ( ( GetKey().nFlags & STextureKey::TK_WRAP ) != 0 ) ? NGfx::WRAP : NGfx::CLAMP;
	if ( !IsValid(pRequest) )
	{
		pRequest = new CFileRequest( "Textures", GetRealTextureID( pTex ) );
		if ( pTex->usage == NDb::CTexture::TEXTURE_USAGE_2D || pTex->bInstantLoad )
			pRequest->Read();
		else
			AddFileRequest( pRequest );
	}

	if ( !pRequest->IsReady() )
	{
		bIsFakeTexture = true;
		bool bHasRead = false;
		// Retail skips previews in these modes; this is not an exceptional condition.
		if ( !bLowRAM && !NGfx::Is16BitTextures() )
		{
			try
			{
				SMMPFileHeader hdr;
				CResourceFileOpener file( "LRTextures", GetRealTextureID( pTex ) );
				file->Read( &hdr, sizeof(hdr) );
				pValue = MakeTexture( hdr, eUsage, eWrap );
				bHasRead = IsValid( pValue ) && RealLoadTexture( pValue.GetPtr(), file.GetStream(), hdr, 0, 0, hdr.nSizeX, hdr.nSizeY, hdr.nNumMipLevels );
			}
			catch(...)
			{
			}
		}
		if ( !bHasRead )
		{
			if ( NGfx::Is16BitTextures() )
			{
				pValue = NGfx::MakeTexture( 1, 1, 1, NGfx::SPixel4444::ID, eUsage, eWrap );
				NGfx::CTextureLock<NGfx::SPixel4444> lock( pValue, 0, NGfx::INPLACE );
				NGfx::SPixel8888 average; average.color = pTex->dwAverageColor;
				lock[0][0].color = Convert16BitPixel( average, 0, NGfx::CF_A4R4G4B4 );
			}
			else
			{
				pValue = NGfx::MakeTexture( 1, 1, 1, NGfx::SPixel8888::ID, eUsage, eWrap );
				NGfx::CTextureLock<NGfx::SPixel8888> lock( pValue, 0, NGfx::INPLACE );
				lock[0][0].color = pTex->dwAverageColor;
			}
		}
		return;
	}
	bIsFakeTexture = false;
	SMMPFileHeader hdr;
	CFileRequest &file = *pRequest;
	if ( file->GetSize() > 0 )
	{
		file->Seek(0);
		file->Read( &hdr, sizeof(hdr) );
		int nSkipMip = pTex->usage == NDb::CTexture::TEXTURE_USAGE_2D ? 0 : Clamp( nTextureUseMip, 0, Max( 0, hdr.nNumMipLevels - 1 ) );
		bool bLoaded;
		if ( hdr.format == NGfx::CF_A8R8G8B8 && NGfx::Is16BitTextures() )
		{
			pValue = LoadConvertTo16Bit( file.GetStream(), hdr, eUsage, eWrap, nSkipMip );
			bLoaded = IsValid( pValue );
		}
		else
		{
			pValue = MakeTexture( hdr, eUsage, eWrap, nSkipMip );
			bLoaded = IsValid( pValue ) && RealLoadTexture( pValue.GetPtr(), file.GetStream(), hdr, 0, 0, hdr.nSizeX, hdr.nSizeY, hdr.nNumMipLevels, nSkipMip );
		}
		if ( !bLoaded )
		{
			ASSERT(0);
			CreateChecker();
		}
	}
	else
	{
		ASSERT(0);
		CreateChecker();
	}
	pRequest = 0;
	ReleaseFileRequestHolder();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFileTexture::NeedUpdate()
{
	bool bRes = TParent::NeedUpdate();
	return bIsFakeTexture || bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFileCubeTexture
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetID( NDb::CTexture *p ) { if (p) return p->GetRecordID(); return 0; }
void CFileCubeTexture::Recalc()
{
	int nTextureIDs[6] = {0,0,0,0,0,0};
	NDb::CCubeTexture *pTex = NDb::GetCubeTexture( GetKey() );
	ASSERT( pTex );
	if ( pTex )
	{
		nTextureIDs[0] = GetID( pTex->pPositiveX );
		nTextureIDs[1] = GetID( pTex->pPositiveY );
		nTextureIDs[2] = GetID( pTex->pPositiveZ );
		nTextureIDs[3] = GetID( pTex->pNegativeX );
		nTextureIDs[4] = GetID( pTex->pNegativeY );
		nTextureIDs[5] = GetID( pTex->pNegativeZ );
	}
	ASSERT( NGfx::POSITIVE_X == 0 );
	CObj<CFileRequest> pRequest;
	try
	{
		SMMPFileHeader hdrMain;
		{
			pRequest = new CFileRequest( "Textures", GetRealTextureID( NDb::GetTexture( nTextureIDs[0] ) ) );
			pRequest->Read();
			pRequest->GetStream()->Read( &hdrMain, sizeof(hdrMain) );
		}
		//nSize = key.GetTextureSize();
		pValue = NGfx::MakeCubeTexture( hdrMain.nSizeX, hdrMain.nNumMipLevels, hdrMain.format, NGfx::REGULAR );
		for ( int i=0; i<6; ++i )
		{
			if ( !nTextureIDs[i] )
				continue;
			loadFace = (NGfx::EFace)i;
			SMMPFileHeader hdr;
			pRequest = new CFileRequest( "Textures", GetRealTextureID( NDb::GetTexture( nTextureIDs[i] ) ) );
			pRequest->Read();
			CFileRequest &file = *pRequest;
			file->Read( &hdr, sizeof(hdr) );

			if ( hdr.nSizeX != hdrMain.nSizeX || hdr.nSizeY != hdrMain.nSizeY ||
				hdr.nNumMipLevels != hdrMain.nNumMipLevels || hdr.format != hdrMain.format )
			{
				ASSERT(0);
				CreateChecker();
				return;
			}
			if ( !RealLoadTexture( pValue.GetPtr(), file.GetStream(), hdr, 0, 0, hdr.nSizeX, hdr.nSizeY, hdr.nNumMipLevels ) )
			{
				ASSERT(0);
				CreateChecker();
				return;
			}
		}
	}
	catch(...)
	{
		ASSERT(0);
		CreateChecker();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFileCubeTexture::CreateChecker()
{
	NGfx::SPixel8888 colors[2];
	colors[0] = NGfx::SPixel8888(0,0,0,255);
	colors[1] = NGfx::SPixel8888(255,255,255,255);
	const int nSize = 128;
	pValue = NGfx::MakeCubeTexture( nSize, 1, NGfx::SPixel8888::ID, NGfx::REGULAR );
	for ( int k = 0; k < 6; ++k )
	{
		NGfx::CTextureLock<NGfx::SPixel8888> lock( pValue, (NGfx::EFace)k, 0, NGfx::INPLACE );
		for ( int y = 0; y < nSize; ++y )
		{
			for ( int x = 0; x < nSize; ++x )
				lock[y][x] = colors[ ( (x&4) == 0 ) & ( (y&4) == 0 ) ];
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CColorTexture
////////////////////////////////////////////////////////////////////////////////////////////////////
void CColorTexture::Recalc()
{
	const int N_SIZE = 4;
	pValue = NGfx::MakeTexture( N_SIZE, N_SIZE, 1, NGfx::SPixel8888::ID, NGfx::REGULAR, NGfx::CLAMP );
	NGfx::CTextureLock<NGfx::SPixel8888> lock( pValue, 0, NGfx::INPLACE );
	for ( int y = 0; y < lock.GetYSize(); ++y )
	{
		for ( int x = 0; x < lock.GetXSize(); ++x )
			lock[y][x].color = NGfx::GetDWORDColor( vColor );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail GTextureInit registers THREE vars: usedxt + gfx_texture_mip (int @0x9c60a4, the global
// CTerrainTextureBlend::NeedUpdate consults -- lives in GTerrainTexture.cpp) + gfx_low_ram (unsaved)
extern int nTextureUseMip;   // GTerrainTexture.cpp, retail @0x9c60a4
bool bLowRAM = false; // gfx_low_ram, retail @0x9c60a8; shared with geometry caching
START_REGISTER(GTexture)
	REGISTER_VAR_EX( "gfx_texture_usedxt", NGlobal::VarBoolHandler, &bDXTModeOn, 1, true )
	REGISTER_VAR_EX( "gfx_texture_mip", NGlobal::VarIntHandler, &nTextureUseMip, 0, true )
	REGISTER_VAR_EX( "gfx_low_ram", NGlobal::VarBoolHandler, &bLowRAM, 0, false )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x00821150, CFileTexture )
//REGISTER_SAVELOAD_CLASS( 0x116A1130, CFileTextureComplex )
REGISTER_SAVELOAD_CLASS( 0x01412121, CFileCubeTexture )
REGISTER_SAVELOAD_CLASS( 0x00682200, CColorTexture )
