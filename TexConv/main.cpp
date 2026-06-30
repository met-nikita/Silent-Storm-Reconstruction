#include "StdAfx.h"
#include "..\FileIO\Streams.h"
#include "..\Image\Image.h"
#include "..\Image\ImageMMP.h"
#include "..\Image\ImageOperation.h"
#include "..\Image\ImagePack.h"
#include "..\Misc\StrProc.h"
#include <ddraw.h>
#include "dds.h"

#include "..\Image\ImageTGA.h" // for debug

enum EImageType
{
	IT_PICTURE,
	IT_BUMP,
	IT_TRANSPARENT,
	IT_TRANSPARENT_ADD
};
static NImage::CImage picture;
static NGfx::EPixelFormat dstPixelFormat = NGfx::CF_A8R8G8B8;
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool LoadPicture( const char *pszInputFileName, NImage::CImage *pDst )
{
	try
	{
		CFileStream sStream;
		sStream.OpenRead( pszInputFileName );
		if ( !NImage::LoadImage( pDst, &sStream ) )
			return false;
	}
	catch(...)
	{
		printf( "Picture loading failed\n" );
		return false;
	}
	return true;
}
static float GetLinear( float f )
{
	if ( f <= 0.1047f )//0.04045f )
		return f / 4;//12.92f;
	else
		return exp( log( ( f + 0.1466f ) / 1.1466f ) * 2.4f );
}
static void ConvertToLinearRGB( NImage::CImage *p )
{
	for ( int y = 0; y < p->GetYSize(); ++y )
	{
		for ( int x = 0; x < p->GetXSize(); ++x )
		{
			CVec4 &color = (*p)[y][x];
			color.r = GetLinear( color.r );
			color.g = GetLinear( color.g );
			color.b = GetLinear( color.b );
			color.a = GetLinear( color.a );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SaveImage( const NImage::CImage &src, const char *pszName )
{
	CFileStream f;
	f.OpenWrite( pszName );
	NImage::SaveImageAsTGA( &f, src );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static bool SaveImageAsDDS( CDataStream *pStream, NImage::CImageMMP *pImage )
{
	DWORD dwMagic = 0x20534444;
	pStream->Write( &dwMagic, 4 );
	DDS_HEADER hdr;
	Zero( hdr );
	hdr.dwSize = sizeof(DDS_HEADER);
	hdr.dwHeaderFlags = DDSD_CAPS | DDSD_MIPMAPCOUNT | DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT;
	hdr.dwHeight = pImage->GetSizeY( 0 );
	hdr.dwWidth = pImage->GetSizeX( 0 );
//		DWORD dwPitchOrLinearSize;
//		DWORD dwDepth; // only if DDS_HEADER_FLAGS_VOLUME is set in dwHeaderFlags
	hdr.dwMipMapCount = pImage->GetNumMipLevels();
	switch ( pImage->GetFormat() )
	{
		case NGfx::CF_DXT1: 
			hdr.ddspf = DDSPF_DXT1; 
			hdr.dwPitchOrLinearSize = pImage->GetLinearSize( 0 );
			hdr.dwHeaderFlags |= DDSD_LINEARSIZE; 
			break;
		case NGfx::CF_DXT2: 
			hdr.ddspf = DDSPF_DXT2;
			hdr.dwPitchOrLinearSize = pImage->GetLinearSize( 0 );
			hdr.dwHeaderFlags |= DDSD_LINEARSIZE; 
			break;
		case NGfx::CF_DXT3: 
			hdr.ddspf = DDSPF_DXT3; 
			hdr.dwPitchOrLinearSize = pImage->GetLinearSize( 0 );
			hdr.dwHeaderFlags |= DDSD_LINEARSIZE; 
			break;
		case NGfx::CF_DXT4: 
			hdr.ddspf = DDSPF_DXT4; 
			hdr.dwPitchOrLinearSize = pImage->GetLinearSize( 0 );
			hdr.dwHeaderFlags |= DDSD_LINEARSIZE; 
			break;
		case NGfx::CF_DXT5: 
			hdr.ddspf = DDSPF_DXT5; 
			hdr.dwPitchOrLinearSize = pImage->GetLinearSize( 0 );
			hdr.dwHeaderFlags |= DDSD_LINEARSIZE; 
			break;
		case NGfx::CF_A8R8G8B8: 
			hdr.ddspf = DDSPF_A8R8G8B8; 
			hdr.dwPitchOrLinearSize = pImage->GetPitchInBytes( 0 );
			hdr.dwHeaderFlags |= DDSD_PITCH; 
			break;
		case NGfx::CF_A4R4G4B4: 
			hdr.ddspf = DDSPF_A4R4G4B4; 
			hdr.dwPitchOrLinearSize = pImage->GetPitchInBytes( 0 );
			hdr.dwHeaderFlags |= DDSD_PITCH; 
			break;
		case NGfx::CF_R5G6B5: 
			hdr.ddspf = DDSPF_R5G6B5; 
			hdr.dwPitchOrLinearSize = pImage->GetPitchInBytes( 0 );
			hdr.dwHeaderFlags |= DDSD_PITCH; 
			break;
		case NGfx::CF_A1R5G5B5:
			hdr.ddspf = DDSPF_A1R5G5B5; 
			hdr.dwPitchOrLinearSize = pImage->GetPitchInBytes( 0 );
			hdr.dwHeaderFlags |= DDSD_PITCH; 
			break;
		default: ASSERT(0); break;
	}
	//hdr.dwSurfaceFlags;
	//hdr.dwCubemapFlags;
	//DWORD dwReserved2[3];
	pStream->Write( &hdr, sizeof(hdr) );

	for ( int nTemp = 0; nTemp < pImage->GetNumMipLevels(); nTemp++ )
		pStream->Write( pImage->GetLFB( nTemp ), pImage->GetSizeX( nTemp ) * pImage->GetSizeY( nTemp ) * pImage->GetBPP() / 8 );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void Process( NImage::SMippedImage *pRes, int nMipMaps, bool bWrap, EImageType itype, float fMappingSize )
{
	NImage::SMippedImage &r = *pRes;
	if ( nMipMaps == 0 )
		nMipMaps = 100;
	r.levels.push_back( picture );
	NImage::CImage t( picture ), mip;//, tpoint(picture);
	//NImage::GenerateMipLevelPoint( &mip, t );
	//SaveImage( mip, "c:\\1\\tempSux.tga" );
	for ( int nMip = 0; nMip < nMipMaps; ++nMip )
	{
		bool bRes = NImage::GenerateMipLevel( &mip, t, bWrap );
		if ( !bRes )
			break;
		r.levels.push_back( mip );
		t = mip;
	}
	switch ( itype )
	{
		case IT_BUMP:
			for ( int i = 0; i < r.levels.size(); ++i )
				NImage::GenerateNormals( &r.levels[i], CVec4(1,0,0,0), fMappingSize, bWrap );
			break;
		case IT_TRANSPARENT:
			for ( int i = 0; i < r.levels.size(); ++i )
			{
				NImage::CImage &res = r.levels[i];
				for ( int y = 0; y < res.GetYSize(); ++y )
				{
					for ( int x = 0; x < res.GetXSize(); ++x )
					{
						CVec4 &v = res[y][x];
						v = CVec4( v.x * v.a, v.y * v.a, v.z * v.a, v.a );
					}
				}
			}
			break;
		case IT_TRANSPARENT_ADD:
			for ( int i = 0; i < r.levels.size(); ++i )
			{
				NImage::CImage &res = r.levels[i];
				for ( int y = 0; y < res.GetYSize(); ++y )
				{
					for ( int x = 0; x < res.GetXSize(); ++x )
					{
						CVec4 &v = res[y][x];
						v = CVec4( v.x * v.a, v.y * v.a, v.z * v.a, 0 );
					}
				}
			}
			break;
	}
//	SaveImage( r.levels[0], "c:\\1\\tempRulez.tga" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const char* GetFormatName( NGfx::EPixelFormat f )
{
	switch ( f )
	{
		case NGfx::CF_DXT1: return "DXT1"; 
		case NGfx::CF_DXT2: return "DXT2"; 
		case NGfx::CF_DXT3: return "DXT3"; 
		case NGfx::CF_DXT4: return "DXT4"; 
		case NGfx::CF_DXT5: return "DXT5"; 
		case NGfx::CF_A8R8G8B8: return "RGBA8888"; 
		case NGfx::CF_A4R4G4B4: return "RGBA4444"; 
		case NGfx::CF_R5G6B5:   return "RGB565"; 
		case NGfx::CF_A1R5G5B5: return "RGBA5551"; 
	}
	return "unknown";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void ImproveDXT1( NImage::SMippedImage *pRes )
{
	for ( int l = 0; l < pRes->levels.size(); ++l )
	{
		NImage::CImage &pic = pRes->levels[l];
		for ( int y = 0 ; y < pic.GetYSize(); ++y )
		for ( int x = 0 ; x < pic.GetXSize(); ++x )
		{
			if ( pic[y][x].a <= 0.8f )
				pic[y][x] = VNULL4;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SavePicture( const NImage::SMippedImage &res, const char *pszOutputFileName, bool bOutputDDS )
{
	CObj<NImage::CImageMMP> pImage = NImage::Pack( res, dstPixelFormat );
	try
	{
		CFileStream sStream;
		sStream.OpenWrite( pszOutputFileName );
		bool bRes = false;
		if ( bOutputDDS )
			bRes = SaveImageAsDDS( &sStream, pImage );
		else
			bRes = NImage::SaveImageAsMMP( &sStream, pImage );
		if ( !bRes )
			printf( "Picture saving failed\n" );
	}
	catch(...)
	{
		printf( "Picture saving failed\n" );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ShowUsage()
{
	printf( "Texture Convertor Utility\n" );
	printf( "(C) Nival Interactive, 2000\n" );
	printf( "Usage:\n" );
	printf( "  TexConv.exe -t<type> -f<format> -m<mips> -a<addr> -s<mapSize> <input file name> <output file name>\n" );
	printf( "   <type>    : Ordinary, Bump, Transparent, TransparentAdd, LinearPicture\n" );
	printf( "   <format>  : ARGB => 8888, 4444, 565, 1555; DXT# => DXT[1-5]\n" );
	printf( "   <mips>    : number of mip-map levels\n" );
	printf( "   <mapSize> : for bump, length on which texture is mapped if its max height is 1\n" );
	printf( "   <addr>    : w(wrap), c(clamp)\n" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int __cdecl main( int argc, char *argv[] )
{
/*	float f1, f2, a = 0.1047f;
	f1 = 0.25f * a;
	f2 = exp( log( 2.4 * a / ( 1 + 1.4 * a ) ) * 2.4f );*/
	if ( argc < 2 )
	{
		ShowUsage();
		return 0xDEAD;
	}
	//
	int nMipMaps = 0;
	EImageType itype = IT_PICTURE;
	bool bWrap = false;
	float fMappingSize = 2;
	bool bNeedGammaCorrect = true;
	std::string szInputName, szOutputName;
	bool bOutputDDS = false;
	for ( int i=1; i<argc; ++i )
	{
		if ( argv[i][0] == '-' )
		{
			if ( argv[i][1] == 'd' )
				bOutputDDS = true;
			else if ( argv[i][1] == 'a' )
			{
				if ( argv[i][2] == 'w' )
					bWrap = true;
				else
					bWrap = false;
			}
			else if ( argv[i][1] == 't' )
			{
				if ( strcmp( argv[i] + 2, "Bump" ) == 0 )
				{
					bNeedGammaCorrect = false;
					itype = IT_BUMP;
				}
				if ( strcmp( argv[i] + 2, "LinearPicture" ) == 0 )
					bNeedGammaCorrect = false;
				else if ( strcmp( argv[i] + 2, "Transparent" ) == 0 )
					itype = IT_TRANSPARENT;
				else if ( strcmp( argv[i] + 2, "TransparentAdd" ) == 0 )
					itype = IT_TRANSPARENT_ADD;
			}
			else if( argv[i][1] == 'm' )
				nMipMaps = atoi( argv[i] + 2 );
			else if( argv[i][1] == 's' )
				fMappingSize = atof( argv[i] + 2 );
			else if ( argv[i][1] == 'f' )
			{
				std::string szFormat = argv[i] + 2;
				NStr::ToLower( szFormat );
				// 8888, 4444, 0565, 1555, DXT[1-5], normals, s1, s2
				if ( szFormat == "dxt1" ) 
					dstPixelFormat = NGfx::CF_DXT1;
				else if ( szFormat == "dxt2" )
					dstPixelFormat = NGfx::CF_DXT2;
				else if ( szFormat == "dxt3" )
					dstPixelFormat = NGfx::CF_DXT3;
				else if ( szFormat == "dxt4" )
					dstPixelFormat = NGfx::CF_DXT4;
				else if ( szFormat == "dxt5" )
					dstPixelFormat = NGfx::CF_DXT5;
				else if ( szFormat == "8888" )
					dstPixelFormat = NGfx::CF_A8R8G8B8;
				else if ( szFormat == "4444" )
					dstPixelFormat = NGfx::CF_A4R4G4B4;
				else if ( szFormat == "565" )
					dstPixelFormat = NGfx::CF_R5G6B5;
				else if ( szFormat == "1555" )
					dstPixelFormat = NGfx::CF_A1R5G5B5;
				else
					printf( "ERROR: unknown format \"%s\"\n", szFormat.c_str() );
			}
		}
		else if ( szInputName.empty() )
			szInputName = argv[i];
		else if ( szOutputName.empty() )
			szOutputName = argv[i];
		else
			printf( "UNKNOWN parameter \"%s\"\n", argv[i] );
	}
	//
	if ( szInputName.empty() || szOutputName.empty() )
	{
		ShowUsage();
		return 0;
	}
	if ( !LoadPicture( szInputName.c_str(), &picture ) )
		printf( "failed to load picture %s\n", szInputName.c_str() );
	else
	{
		// gamma correct source picture
		//if ( bNeedGammaCorrect )
		//	ConvertToLinearRGB( &picture );
		// inform about current operation
		const char *pszOp;
		switch ( itype )
		{
			default: ASSERT(0); pszOp = "???"; break;
			case IT_PICTURE: pszOp = "Pic"; break;
			case IT_BUMP: pszOp = "Bump"; break;
			case IT_TRANSPARENT: pszOp = "Transparent"; break;
			case IT_TRANSPARENT_ADD: pszOp = "TransparentAdd"; break;
		}
		const char *pszName = szOutputName.c_str(), *pszTemp;
		pszTemp = strrchr( pszName, '\\' );
		if ( pszTemp )
			pszName = pszTemp + 1;
		printf( "%s %s, fmt=%s, mips=%d", pszOp, pszName, GetFormatName( dstPixelFormat ), nMipMaps );
		//
		NImage::SMippedImage res;
		Process( &res, nMipMaps, bWrap, itype, fMappingSize );
		if ( dstPixelFormat == NGfx::CF_DXT1 )
			ImproveDXT1( &res );
		SavePicture( res, szOutputName.c_str(), bOutputDDS );
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
