#include "StdAfx.h"
#include "bmpfile.h"
#include "..\Misc\2Darray.h"
#include "..\FileIO\Streams.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// Reconstructed from release Game.exe (WriteBMP @ 0x004b9e90) via Ghidra decompilation against the
// matched PDB. Writes a standard uncompressed 32-bit BMP. NGfx::SPixel8888 is laid out b,g,r,a in
// memory, which is the native byte order of a 32bpp BI_RGB bitmap, so scanlines are written verbatim,
// bottom-to-top.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool WriteBMP( const CArray2D<NGfx::SPixel8888> &image, const char *pszFileName )
{
	const int nWidth = image.GetXSize();
	const int nHeight = image.GetYSize();

	CFileStream f;
	f.OpenWrite( pszFileName );

	BITMAPFILEHEADER fileHeader;
	fileHeader.bfType = 0x4D42; // "BM"
	fileHeader.bfSize = sizeof( BITMAPFILEHEADER ) + sizeof( BITMAPINFOHEADER ) + nWidth * nHeight * 4;
	fileHeader.bfReserved1 = 0;
	fileHeader.bfReserved2 = 0;
	fileHeader.bfOffBits = sizeof( BITMAPFILEHEADER ) + sizeof( BITMAPINFOHEADER );
	f.Write( &fileHeader, sizeof( fileHeader ) );

	BITMAPINFOHEADER infoHeader;
	infoHeader.biSize = sizeof( BITMAPINFOHEADER );
	infoHeader.biWidth = nWidth;
	infoHeader.biHeight = nHeight;
	infoHeader.biPlanes = 1;
	infoHeader.biBitCount = 32;
	infoHeader.biCompression = BI_RGB;
	infoHeader.biSizeImage = 0;
	infoHeader.biXPelsPerMeter = 1;
	infoHeader.biYPelsPerMeter = 1;
	infoHeader.biClrUsed = 0;
	infoHeader.biClrImportant = 0;
	f.Write( &infoHeader, sizeof( infoHeader ) );

	// .bmp scanlines are stored bottom-to-top
	for ( int y = nHeight - 1; y >= 0; --y )
		f.Write( &image[y][0], nWidth * sizeof( NGfx::SPixel8888 ) );

	return true;
}
