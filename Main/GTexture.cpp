#include "StdAfx.h"
#include "GfxBuffers.h"
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
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTextureLoader
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TPixel>
void LoadTextureData( NGfx::CTexture *pTexture, int _nX, int _nY, int _nSizeX, int _nSizeY, 
	int _nLevels, CDataStream *pFile, const TPixel *p = 0 )
{
	CDynamicCast<NGfx::I2DBuffer> pTexBuffer( pTexture );
	int nLevels = Min( _nLevels, pTexBuffer->GetNumMipLevels() );
	for ( int nLevel = 0; nLevel < nLevels; ++nLevel )
	{
		NGfx::CTextureLock<TPixel> lock( pTexture, nLevel, NGfx::INPLACE );
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
static NGfx::EFace loadFace;
template <class TPixel>
void LoadTextureData( NGfx::CCubeTexture *pTexture, int _nX, int _nY, int _nSizeX, int _nSizeY, 
	int _nLevels, CDataStream *pFile, const TPixel *p = 0 )
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
	int nX, int nY, int nSizeX, int nSizeY, int nLevels )
{
	nLevels = Min( nLevels, hdr.nNumMipLevels );
	switch ( hdr.format )
	{
		case NGfx::CF_DXT1:			LoadTextureData<NGfx::SPixelDXT1>( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream ); break;
		case NGfx::CF_DXT2:			LoadTextureData<NGfx::SPixelDXT2>( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream ); break;
		case NGfx::CF_DXT3:			LoadTextureData<NGfx::SPixelDXT3>( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream ); break;
		case NGfx::CF_DXT4:			LoadTextureData<NGfx::SPixelDXT4>( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream ); break;
		case NGfx::CF_DXT5:			LoadTextureData<NGfx::SPixelDXT5>( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream ); break;
		case NGfx::CF_R5G6B5:		LoadTextureData<NGfx::SPixel565> ( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream ); break;
		case NGfx::CF_A1R5G5B5: LoadTextureData<NGfx::SPixel1555>( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream ); break;
		case NGfx::CF_A4R4G4B4: LoadTextureData<NGfx::SPixel4444>( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream ); break;
		case NGfx::CF_A8R8G8B8: LoadTextureData<NGfx::SPixel8888>( pTexture, nX, nY, nSizeX, nSizeY, nLevels, pStream ); break;
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
static NGfx::CTexture* MakeTexture( const SMMPFileHeader &hdr, NGfx::ETextureUsage eUsage, NGfx::EWrap eWrap )
{
	return NGfx::MakeTexture( hdr.nSizeX, hdr.nSizeY, hdr.nNumMipLevels, 
		hdr.format, eUsage, eWrap );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFileTexture::Recalc()
{
	if ( IsValid(pRequest) && !pRequest->IsReady() && IsValid( pValue ) )
		return;

	NDb::CTexture *pTex = NDb::GetTexture( GetKey().nID );
	NGfx::ETextureUsage eUsage;
	switch ( pTex->type )
	{
		case NDb::CTexture::REGULAR: eUsage = NGfx::REGULAR; break;
		case NDb::CTexture::TEXTURE_2D: eUsage = NGfx::TEXTURE_2D; break;
		case NDb::CTexture::TEXTURE_TRANSPARENT: eUsage = NGfx::REGULAR; break;
		default: ASSERT(0); eUsage = NGfx::REGULAR; break;
	}
	if ( GetKey().nFlags & STextureKey::TK_TRANSPARENT )
		eUsage = NGfx::TRANSPARENT_TEXTURE;
	NGfx::EWrap eWrap = ( ( GetKey().nFlags & STextureKey::TK_WRAP ) != 0 ) ? NGfx::WRAP : NGfx::CLAMP;
	if ( !IsValid(pRequest) )
	{
		pRequest = new CFileRequest( "Textures", GetRealTextureID( pTex ) );
		if ( pTex->type == NDb::CTexture::TEXTURE_2D || pTex->bInstantLoad )
			pRequest->Read();
		else
			AddFileRequest( pRequest );
	}

	if ( !pRequest->IsReady() )
	{
		bIsFakeTexture = true;
		bool bHasRead = false;
		try
		{
			SMMPFileHeader hdr;
			CResourceFileOpener file( "LRTextures", GetRealTextureID( pTex ) );
			file->Read( &hdr, sizeof(hdr) );
			pValue = MakeTexture( hdr, eUsage, eWrap );
			if ( !RealLoadTexture( pValue.GetPtr(), file.GetStream(), hdr, 0, 0, hdr.nSizeX, hdr.nSizeY, hdr.nNumMipLevels ) )
				throw int(0);
			bHasRead = true;
		}
		catch(...)
		{
		}
		if ( !bHasRead )
		{
			pValue = NGfx::MakeTexture( 1, 1, 1, 
				NGfx::SPixel8888::ID, eUsage, eWrap );
			NGfx::CTextureLock<NGfx::SPixel8888> lock( pValue, 0, NGfx::INPLACE );
			lock[0][0].color = pTex->dwAverageColor;
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
		pValue = MakeTexture( hdr, eUsage, eWrap );
		if ( !RealLoadTexture( pValue.GetPtr(), file.GetStream(), hdr, 0, 0, hdr.nSizeX, hdr.nSizeY, hdr.nNumMipLevels ) )
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
START_REGISTER(GTexture)
	REGISTER_VAR_EX( "gfx_texture_usedxt", NGlobal::VarBoolHandler, &bDXTModeOn, 1, true )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x00821150, CFileTexture )
//REGISTER_SAVELOAD_CLASS( 0x116A1130, CFileTextureComplex )
REGISTER_SAVELOAD_CLASS( 0x01412121, CFileCubeTexture )
REGISTER_SAVELOAD_CLASS( 0x00682200, CColorTexture )
