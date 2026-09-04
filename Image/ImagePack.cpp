#include "StdAfx.h"
#include "ImagePack.h"
#include "ImageMMP.h"
#include <squish/squish.h>
namespace NImage
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddMipDXTN( CImageMMP *pDst, const CImage &src, NGfx::EPixelFormat format )
{
	float fWeights[] = { 0.309f, 0.609f, 0.082f, 0, 0, 0, 0, 0 };
	int squishFlags = 0; // = squish::kColourIterativeClusterFit;
	bool doAlphaPremult = false;
	bool doAlphaKey = false;
	// compose encoding type
	switch ( format )
	{
		case NGfx::CF_DXT1:
			squishFlags |= squish::kDxt1;
			break;
		case NGfx::CF_DXT2:
			doAlphaPremult = true;
		case NGfx::CF_DXT3:
			squishFlags |= squish::kDxt3;
			break;
		case NGfx::CF_DXT4:
			doAlphaPremult = true;
		case NGfx::CF_DXT5:
			squishFlags |= squish::kDxt5;
			break;
	}
	std::vector<unsigned char> rgba;
	rgba.reserve(src.GetXSize() * src.GetYSize() * 4);

	for (int y = 0; y < src.GetYSize(); ++y)
	{
		for (int x = 0; x < src.GetXSize(); ++x)
		{
			const CVec4& s = src[y][x];
			unsigned char ucR = RoundComponent(s.r) << 16;
			unsigned char ucG = RoundComponent(s.g) << 8;
			unsigned char ucB = RoundComponent(s.b);
			unsigned char ucA = RoundComponent(s.a) << 24;

			if (doAlphaKey)
			{
				ucA = (ucA == 0) ? 0 : 255;
			}
			else if(doAlphaPremult)
			{
				if (ucA < 255) {
					ucR = (BYTE)((ucR * ucA) / 255);
					ucG = (BYTE)((ucG * ucA) / 255);
					ucB = (BYTE)((ucB * ucA) / 255);
				}
			}

			rgba.push_back(ucR);
			rgba.push_back(ucG);
			rgba.push_back(ucB);
			rgba.push_back(ucA);
		}
	}

	int width = src.GetXSize();
	int height = src.GetYSize();

	int storageBytes = squish::GetStorageRequirements(width, height, squishFlags);

	std::vector<BYTE> outdata(storageBytes);

	squish::CompressImage(
		rgba.data(),
		width,
		height,
		outdata.data(),
		squishFlags,
		fWeights
	);

	// add mip-level (same as before)
	pDst->AddMipLevel(outdata.data(), outdata.size());
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddMipRGBA( CImageMMP *pDst, const CImage &src, NGfx::EPixelFormat format )
{
	SPixelConvertInfo pci;
	switch ( format )
	{
		case NGfx::CF_R5G6B5:
			pci.InitMaskInfo( 0x00000000, 0x0000F800, 0x000007E0, 0x0000001F );
			break;
		case NGfx::CF_A1R5G5B5:
			pci.InitMaskInfo( 0x00008000, 0x00007C00, 0x000003E0, 0x0000001F );
			break;
		case NGfx::CF_A4R4G4B4:
			pci.InitMaskInfo( 0x0000F000, 0x00000F00, 0x000000F0, 0x0000000F );
			break;
		case NGfx::CF_A8R8G8B8:
			pci.InitMaskInfo( 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF );
			break;
		default:
			ASSERT( 0 );
			return;
	}
	//
	int nSizeX = src.GetXSize();//pImage->GetSizeX();
	int nSizeY = src.GetYSize();//pImage->GetSizeY();
	int nBPP = NGfx::GetBPP( format );
	int nBytePerPixel = nBPP / 8;
	std::vector<BYTE> outdata( nSizeX * nSizeY * nBytePerPixel );
	{
		BYTE *pBuffer = reinterpret_cast<BYTE*>( &( outdata[0] ) );
		for ( int y = 0; y < src.GetYSize(); ++y )
		{
			for ( int x = 0; x < src.GetXSize(); ++x )
			{
				DWORD dwConverted = pci.ComposeColorSlow( src[y][x] );
				memcpy( pBuffer, &dwConverted, nBytePerPixel );
				pBuffer += nBytePerPixel;
			}
		}
	}
	pDst->AddMipLevel( &(outdata[0]), outdata.size() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddMip( CImageMMP *pDst, const CImage &src, NGfx::EPixelFormat format )
{
	if ( (format >= NGfx::CF_DXT1) && (format <= NGfx::CF_DXT5) )
	{
		if ( src.GetXSize() < 4 || src.GetYSize() < 4 )
			return;
		AddMipDXTN( pDst, src, format );
	}
	AddMipRGBA( pDst, src, format );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static DWORD CalcAverageColor( const CImage &src )
{
	CVec4 vAvrg(0,0,0,0);
	for ( int y = 0; y < src.GetYSize(); ++y )
	{
		for ( int x = 0; x < src.GetXSize(); ++x )
			vAvrg += src[y][x];
	}
	vAvrg /= src.GetXSize() * src.GetYSize();
	return NGfx::GetDWORDColor( vAvrg );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CImageMMP* Pack( const SMippedImage &src, NGfx::EPixelFormat format )
{
	CImageMMP *pRes = new CImageMMP( src.levels[0].GetXSize(), src.levels[0].GetYSize(), format, CalcAverageColor( src.levels[0] ) );
	for ( int nMip = 0; nMip < src.levels.size(); ++nMip )
		AddMip( pRes, src.levels[nMip], format );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
