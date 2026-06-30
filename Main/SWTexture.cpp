#include "StdAfx.h"
#include "GPixelFormat.h"
#include "mmpFormat.h"
#include "SWTexture.h"
#include "..\Misc\StrProc.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFileTexture
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TPixel>
void LoadTextureData( CSWTextureData *pTexture, int _nMips, int _nSizeX, int _nSizeY, CDataStream *pFile )
{
	int nXSize = _nSizeX;
	int nYSize = _nSizeY;
	
	ASSERT( _nMips > 0 );
	_nMips = Max( _nMips, 1 );
	pTexture->mips.resize( _nMips );
	for ( int nMip = 0; nMip < _nMips; ++nMip )
	{
		ASSERT( nXSize > 0 && nYSize > 0 );
		pTexture->mips[nMip].SetSizes( nXSize, nYSize );
		for ( int y = 0; y < nYSize; ++y )
			pFile->Read( &pTexture->mips[nMip][y][0], nXSize * sizeof(TPixel) );
		nXSize >>= 1;
		nYSize >>= 1;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CreateChecker( CSWTextureData *pTexture )
{
	NGfx::SPixel8888 colors[2];
	colors[0] = NGfx::SPixel8888(0,0,0,255);
	colors[1] = NGfx::SPixel8888(255,255,255,255);
	const int nSize = 128;

	pTexture->mips.resize( 1 );
	pTexture->mips[0].SetSizes( nSize, nSize );

	for ( int y = 0; y < nSize; ++y )
		for ( int x = 0; x < nSize; ++x )
			pTexture->mips[0][y][x] = colors[ ( ( x & 4) == 0 ) & ( ( y & 4) == 0 ) ];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSWTexture::IsReady()
{ 
	Touch();
	if ( bIsReady )
		return true;
	if ( IsValid(pRequest) )
	{
		if ( !pRequest->IsReady() )
			return false;
		bIsReady = true;
		LoadTexture();
		pRequest = 0;
		ReleaseFileRequestHolder();
		return true;
	}
	pRequest = new CFileRequest( "Textures", GetKey() );
	AddFileRequest( pRequest );
	return false; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSWTexture::LoadTexture()
{
	pValue = new CSWTextureData;
	try
	{
		CFileRequest &file = *pRequest;
		SMMPFileHeader hdr;
		file->Read( &hdr, sizeof(hdr) );

		// fill with data
		switch ( hdr.format )
		{
			/*
			case NGfx::CF_DXT1:			LoadTextureData<NGfx::SPixelDXT1>( pValue, hdr.nSizeX, hdr.nSizeY, hdr.nNumMipLevels, file.GetStream() ); break;
			case NGfx::CF_DXT2:			LoadTextureData<NGfx::SPixelDXT2>( pValue, hdr.nSizeX, hdr.nSizeY, hdr.nNumMipLevels, file.GetStream() ); break;
			case NGfx::CF_DXT3:			LoadTextureData<NGfx::SPixelDXT3>( pValue, hdr.nSizeX, hdr.nSizeY, hdr.nNumMipLevels, file.GetStream() ); break;
			case NGfx::CF_DXT4:			LoadTextureData<NGfx::SPixelDXT4>( pValue, hdr.nSizeX, hdr.nSizeY, hdr.nNumMipLevels, file.GetStream() ); break;
			case NGfx::CF_DXT5:			LoadTextureData<NGfx::SPixelDXT5>( pValue, hdr.nSizeX, hdr.nSizeY, hdr.nNumMipLevels, file.GetStream() ); break;
			case NGfx::CF_R5G6B5:		LoadTextureData<NGfx::SPixel565>( pValue, hdr.nSizeX, hdr.nSizeY, hdr.nNumMipLevels, file.GetStream() ); break;
			case NGfx::CF_A1R5G5B5: LoadTextureData<NGfx::SPixel1555>( pValue, hdr.nSizeX, hdr.nSizeY, hdr.nNumMipLevels, file.GetStream() ); break;
			case NGfx::CF_A4R4G4B4: LoadTextureData<NGfx::SPixel4444>( pValue, hdr.nSizeX, hdr.nSizeY, hdr.nNumMipLevels, file.GetStream() ); break;
			case NGfx::CF_NORMALES: LoadTextureData<NGfx::SPixel8888>( pValue, hdr.nSizeX, hdr.nSizeY, hdr.nNumMipLevels, file.GetStream() ); break;
			*/
		case NGfx::CF_A8R8G8B8: LoadTextureData<NGfx::SPixel8888>( pValue, hdr.nNumMipLevels, hdr.nSizeX, hdr.nSizeY, file.GetStream() ); break;
		default: ASSERT( 0 ); CreateChecker( pValue ); break;
		}
	}
	catch(...)
	{
		CreateChecker( pValue );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSWTexture::Recalc()
{
	ASSERT(0);
	while ( !IsReady() )
		Sleep(0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSWTextureData
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSWTextureData::PrepareBump()
{
	if ( bumpMips.size() == mips.size() )
		return;
	bumpMips.resize( mips.size() );
	for ( int k = 0; k < mips.size(); ++k )
	{
		bumpMips[k].SetSizes( mips[k].GetXSize(), mips[k].GetYSize() );
		for ( int y = 0; y < mips[k].GetYSize(); ++y )
		{
			for ( int x = 0; x < mips[k].GetXSize(); ++x )
			{
				NGfx::SPixel8888 src = mips[k][y][x];
				SBumpPixel &res = bumpMips[k][y][x];
				if ( src.b == 128 )
				{
					res.fDU = res.fDV = 0;
				}
				else
				{
					float fInv = 1.0f / ( src.b - 128 );
					res.fDU = ( ((int)src.r) - 128 ) * fInv;
					res.fDV = ( ((int)src.g) - 128 ) * fInv;
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBilinearTexture
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBilinearTexture::Recalc()
{
	pValue = new CSWTextureData;
	pValue->mips.resize( 1 );
	pValue->mips[0].SetSizes( nXSize, nYSize );
	ASSERT( pic.GetXSize() > 1 && pic.GetYSize() > 1 );
	float fdU = 0, fdV = 0;
	if ( nXSize > 1 )
		fdU = ( (float)pic.GetXSize() - 1.01f ) / ( nXSize - 1 );
	if ( nYSize > 1 )
		fdV = ( (float)pic.GetYSize() - 1.01f ) / ( nYSize - 1 );
	int nUPos, nDU = Float2Int( fdU * 0x8000 ), nVPos, nDV = Float2Int( fdV * 0x8000 );
	int nNextY = pic.GetXSize() * 4;
	int64 shift = 0x10001000100010;
	nVPos = 0;
	for ( int y = 0; y < nYSize; ++y )
	{
		int nYMul = ( nVPos & 0x7fff ), nYMul1 = 0x7fff - nYMul;
		nUPos = 0;
		NGfx::SPixel8888 *pPictureSrc = &pic[ nVPos >> 15 ][0];
		NGfx::SPixel8888 *pDst = &pValue->mips[0][y][0];
		__asm
		{
			movd mm4, nYMul
			movd mm5, nYMul1
			punpcklwd mm4, mm4
			punpcklwd mm5, mm5
			punpckldq mm4, mm4
			punpckldq mm5, mm5
		}
		for ( int x = 0; x < nXSize; ++x )
		{
			int nXMul = ( nUPos & 0x7fff );
			NGfx::SPixel8888 *pSrc = &pPictureSrc[nUPos>>15];
			__asm
			{
				mov esi, pSrc
				mov edi, nNextY
				movd mm6, nXMul
				punpcklwd mm6, mm6
				pxor mm0, mm0
				punpckldq mm6, mm6
				pxor mm1, mm1
				punpcklbw mm0, [esi]
				pxor mm2, mm2
				punpcklbw mm1, [esi + 4]
				pxor mm3, mm3
				punpcklbw mm2, [esi + edi]
				punpcklbw mm3, [esi + edi + 4]
				psrlw mm0, 1
				mov esi, pDst
				psrlw mm1, 1
				psrlw mm2, 1
				psrlw mm3, 1
				// mm0 = mm0 * (1-mm6) + mm1 * mm6
				// mm2 = mm2 * (1-mm6) + mm3 * mm6
				psubw mm1, mm0
				psubw mm3, mm2
				psrlw mm0, 1
				psrlw mm2, 1
				pmulhw mm1, mm6
				pmulhw mm3, mm6
				paddw mm0, mm1
				paddw mm2, mm3
				// mm0 = mm0 * mm5 + mm2 * mm4
				pmulhw mm0, mm5
				pmulhw mm2, mm4
				paddw mm0, mm2
				paddw mm0, shift
				psrlw mm0, 5
				packuswb mm0, mm0
				movd [esi], mm0
			}
			pDst++;
			nUPos += nDU;
		}
		nVPos += nDV;
	}
	_asm emms
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0xF0821150, CSWTexture )
REGISTER_SAVELOAD_CLASS( 0x007c1140, CBilinearTexture )
