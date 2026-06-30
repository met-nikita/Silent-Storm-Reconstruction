#include "StdAfx.h"
#include "..\FileIO\Streams.h"
#include "..\Image\Image.h"
#include "..\Image\ImageMMP.h"
#include "..\Misc\StrProc.h"

static CObj<NImage::CImageMMP> pImage;
////////////////////////////////////////////////////////////////////////////////////////////////////
static NImage::CImageMMP* LoadPicture( const char *pszInputFileName )
{
	try
	{
		CFileStream sStream;
		sStream.OpenRead( pszInputFileName );
		if ( !NImage::RecognizeFormatMMP( &sStream) )
			return false;
		return NImage::LoadImageMMP( &sStream );
	}
	catch(...)
	{
	}
	return 0;
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
static void SavePicture( NImage::CImageMMP *pImage, const char *pszOutputFileName )
{
	try
	{
		CFileStream sStream;
		sStream.OpenWrite( pszOutputFileName );
		bool bRes = false;
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
	printf( "Texture mip level stripper\n" );
	printf( "Usage:\n" );
	printf( "  TexMipStrip.exe [-m<maxMipSize>] <input directory> <output directory>\n" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void ConvertFile( const string &szSrc, const string &szDst, int nMaxMip )
{
	CObj<NImage::CImageMMP> pSrc = LoadPicture( szSrc.c_str() );
	if ( !IsValid(pSrc) )
		printf( "failed to load picture %s\n", szSrc.c_str() );
	else
	{
		//printf( "picture format %s, %dx%d\n", GetFormatName( pSrc->GetFormat() ), pSrc->GetSizeX(0), pSrc->GetSizeY(0) );
		while ( pSrc->GetMipsNumber() > 1 && Max( pSrc->GetSizeX(0), pSrc->GetSizeY(0) ) > nMaxMip )
			pSrc->DropLargestMip();
		SavePicture( pSrc, szDst.c_str() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int __cdecl main( int argc, char *argv[] )
{
	if ( argc < 2 )
	{
		ShowUsage();
		return 0xDEAD;
	}
	//
	int nMaxMip = 16;
	std::string szInputName, szOutputName;
	for ( int i=1; i<argc; ++i )
	{
		if ( argv[i][0] == '-' )
		{
			if( argv[i][1] == 'm' )
				nMaxMip = atoi( argv[i] + 2 );
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

	string szMask = szInputName + "\\*.*";
	WIN32_FIND_DATA ff;
	HANDLE hf = FindFirstFile( szMask.c_str(), &ff );
	string szPath = szMask.substr( 0, szMask.length() - 3 );
	if ( hf != INVALID_HANDLE_VALUE )
	{
		for ( BOOL bCont = TRUE; bCont; bCont = FindNextFile( hf, &ff ) )
		{
			if ( ff.cFileName[0] == '.' || (ff.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)) != 0 )
				continue;
			if ( ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
				continue;
			ConvertFile( szPath + ff.cFileName, szOutputName + "\\" + ff.cFileName, nMaxMip );
		}
		FindClose( hf );
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
