#include "StdAfx.h"
#include "ImageTGA.h"

namespace NImage
{
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ETGAImageType
{
	TGAIT_NOIMAGEDATA				= 0,
	TGAIT_COLOR_MAPPED			= 1,
	TGAIT_TRUE_COLOR				= 2,
	TGAIT_BLACK_WHITE				= 3,
	TGAIT_RLE_COLOR_MAPPED	= 9,
	TGAIT_RLE_TRUE_COLOR		= 10,
	TGAIT_RLE_BLACK_WHITE		= 11
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma pack ( 1 )
struct SColor24 
{ 
	BYTE b;
	BYTE g;
	BYTE r; 
};
struct SColor
{
	DWORD b : 8;
	DWORD g : 8;
	DWORD r : 8;
	DWORD a : 8;

	SColor() {  }
	SColor( BYTE _r, BYTE _g, BYTE _b, BYTE _a ) : b( _b ), g( _g ), r( _r ), a( _a ) {  }
};
// describe the color map (if any) used for the image
struct SColorMapSpecification 
{
	WORD wFirstEntryIndex;								// Index of the first color map entry. Index refers to the starting entry in loading the color map.
	WORD wColorMapLength;									// Total number of color map entries included
	BYTE cColorMapEntrySize;							// Establishes the number of bits per entry. Typically 15, 16, 24 or 32-bit values are used

};
//
struct SImageDescriptor
{
	BYTE cAlphaChannelBits : 4;						// the number of attribute bits per pixel
	BYTE cLeftToRightOrder : 1;						// left-to-right ordering 
	BYTE cTopToBottomOrder : 1;						// top-to-bottom ordering 
	BYTE cUnused           : 2;						// Must be zero to insure future compatibility
};
// describe the image screen location, size and pixel depth
struct SImageSpecification
{
	WORD wXOrigin;												// absolute horizontal coordinate for the lower left corner of the image as it is positioned on a display device having an origin at the lower left of the screen 
	WORD wYOrigin;												// absolute vertical coordinate for the lower left corner of the image as it is positioned on a display device having an origin at the lower left of the screen
	WORD wImageWidth;											// width of the image in pixels
	WORD wImageHeight;										// height of the image in pixels
	BYTE cPixelDepth;											// number of bits per pixel. This number includes the Attribute or Alpha channel bits. Common values are 8, 16, 24 and 32 but other pixel depths could be used.
	SImageDescriptor descriptor;					//
};
struct STGAFileHeader
{
	BYTE cIDLength;												// identifies the number of bytes contained in Field 6, the Image ID Field
	BYTE cColorMapType;										// indicates the type of color map (if any) included with the image
	BYTE cImageType;											// TGA File Format can be used to store Pseudo-Color, True-Color and Direct-Color images of various pixel depths
	SColorMapSpecification colormap;      //
	SImageSpecification imagespec;				// 
};
struct STGAFileFooter
{
	DWORD dwExtensionAreaOffset;					// offset from the beginning of the file to the start of the Extension  Area
	DWORD dwDeveloperDirectoryOffset;			// offset from the beginning of the file to the start of the Developer Directory
	BYTE cSignature[16];									// "TRUEVISION-XFILE"
	BYTE cReservedCharacter;							// must be '.'
	BYTE cBinaryZeroStringTerminator;			// '\0'
};
#pragma pack()
////////////////////////////////////////////////////////////////////////////////////////////////////
bool RecognizeFormatTGA( CDataStream *pStream )
{
	// check for the new/original TGA file format
	pStream->Seek( pStream->GetSize() - 26 );
	STGAFileFooter footer;
	pStream->Read( &footer, sizeof(footer) );
	// check for the new
	char pszSignature[32];
	memcpy( pszSignature, footer.cSignature, 16 );
	pszSignature[16] = 0;
	bool bNewTGA = ( footer.cReservedCharacter == '.' ) && ( strcmp(pszSignature, "TRUEVISION-XFILE") == 0 );
	if ( bNewTGA )
		return true;
	// check for the original
	STGAFileHeader hdr;
	pStream->Seek( 0 );
	pStream->Read( &hdr, sizeof(hdr) );
	pStream->Seek( 0 );
	// image type <=> color map type
	bool bCheck = false;
	switch ( hdr.cImageType )
	{
		case TGAIT_NOIMAGEDATA:
			bCheck = false;
			break;
		case TGAIT_COLOR_MAPPED:
			bCheck = hdr.cColorMapType == 1;
			break;
		case TGAIT_TRUE_COLOR:
			bCheck = hdr.cColorMapType == 0;
			break;
		case TGAIT_BLACK_WHITE:
			bCheck = hdr.cColorMapType == 1;
			break;
		case TGAIT_RLE_COLOR_MAPPED:
			bCheck = hdr.cColorMapType == 1;
			break;
		case TGAIT_RLE_TRUE_COLOR:
			bCheck = hdr.cColorMapType == 0;
			break;
		case TGAIT_RLE_BLACK_WHITE:
			bCheck = hdr.cColorMapType == 0;
			break;
	}
	if ( !bCheck )
		return false;
	// some fields valid values
	bCheck = hdr.imagespec.descriptor.cUnused == 0;
	if ( !bCheck )
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool LoadTrueColorTGA( const STGAFileHeader &hdr, CImage *pImage, CDataStream *pStream );
bool LoadColorMappedTGA( const STGAFileHeader &hdr, CImage *pImage, CDataStream *pStream );
bool LoadRLETrueColorTGA( const STGAFileHeader &hdr, CImage *pImage, CDataStream *pStream );
bool LoadImageTGA( CDataStream *pStream, CImage *pImage )
{
	STGAFileHeader hdr;
	pStream->Seek( 0 );
	pStream->Read( &hdr, sizeof(hdr) );
	switch ( hdr.cImageType )
	{
		case TGAIT_TRUE_COLOR:
			if ( LoadTrueColorTGA( hdr, pImage, pStream ) )
				return true;
			break;
		case TGAIT_RLE_TRUE_COLOR:
			if ( LoadRLETrueColorTGA( hdr, pImage, pStream ) )
				return true;
			break;
		case TGAIT_COLOR_MAPPED:
			if ( LoadColorMappedTGA( hdr, pImage, pStream ) )
				return true;
			break;
		/*
		case TGAIT_COLOR_MAPPED:
		case TGAIT_BLACK_WHITE:
		case TGAIT_RLE_COLOR_MAPPED:
		case TGAIT_RLE_TRUE_COLOR:
		case TGAIT_RLE_BLACK_WHITE:
		*/
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool LoadTrueColorTGA( const STGAFileHeader &hdr, CImage *pImage, CDataStream *pStream )
{
	// skip image ID
	if ( hdr.cIDLength != 0 )
		pStream->Seek( hdr.cIDLength  + pStream->GetPosition() );
	pImage->SetSizes( hdr.imagespec.wImageWidth, hdr.imagespec.wImageHeight );
	// true color have no color map information
	// ...
	// image data
	int nBPP = hdr.imagespec.cPixelDepth;
	int nReadSizeBytes = hdr.imagespec.wImageWidth * hdr.imagespec.wImageHeight * nBPP / 8;
	//
	switch ( nBPP )
	{
		case 24:
			{
				std::vector<BYTE> buffer( nReadSizeBytes );
				pStream->Read( &( buffer[0] ), buffer.size() );
				CVec4 *pBuffer = &(*pImage)[0][0];
				for ( int i=0; i<buffer.size(); i += 3 ) 
				{
					pBuffer->b = buffer[i + 0] / 255.0f;
					pBuffer->g = buffer[i + 1] / 255.0f;
					pBuffer->r = buffer[i + 2] / 255.0f;
					pBuffer->a = 1;
					pBuffer++;
				}
			}
			break;
		case 32:
			{
				//pStream->Read( pImage->GetLFB(), nReadSizeBytes );
				std::vector<BYTE> buffer( nReadSizeBytes );
				pStream->Read( &( buffer[0] ), buffer.size() );
				CVec4 *pBuffer = &(*pImage)[0][0];
				for ( int i=0; i<buffer.size(); i += 4 ) 
				{
					pBuffer->b = buffer[i + 0] / 255.0f;
					pBuffer->g = buffer[i + 1] / 255.0f;
					pBuffer->r = buffer[i + 2] / 255.0f;
					pBuffer->a = buffer[i + 3] / 255.0f;
					pBuffer++;
				}
			}
			break;
		default:
			return false;
	}
	if ( hdr.imagespec.descriptor.cTopToBottomOrder != 0 )
		FlipY( pImage );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool LoadColorMappedTGA( const STGAFileHeader &hdr, CImage *pImage, CDataStream *pStream )
{
	// skip image ID
	if ( hdr.cIDLength != 0 )
		pStream->Seek( hdr.cIDLength + pStream->GetPosition() );
	pImage->SetSizes( hdr.imagespec.wImageWidth, hdr.imagespec.wImageHeight );
	// read color map information
	std::vector<CVec4> palette( hdr.colormap.wColorMapLength );
	pStream->Seek( hdr.colormap.wFirstEntryIndex * hdr.colormap.cColorMapEntrySize / 8 + pStream->GetPosition() );
	switch ( hdr.colormap.cColorMapEntrySize )
	{
		case 24:
			{
				std::vector<SColor24> palette1( hdr.colormap.wColorMapLength );
				pStream->Read( &( palette1[0] ), sizeof(SColor24) * palette1.size() );
				for ( int i=0; i<palette.size(); ++i )
					palette[i] = CVec4( palette1[i].r / 255.0f, palette1[i].g / 255.0f, palette1[i].b / 255.0f, 1 );
			}
			break;
		case 32:
			{
				std::vector<SColor> palette1( hdr.colormap.wColorMapLength );
				pStream->Read( &( palette1[0] ), sizeof(SColor) * palette1.size() );
				for ( int i=0; i<palette.size(); ++i )
					palette[i] = CVec4( palette1[i].r / 255.0f, palette1[i].g / 255.0f, palette1[i].b / 255.0f, palette1[i].a / 255.0f );
			}
			break;
	}
	//
	if ( hdr.imagespec.cPixelDepth == 8 )
	{
		std::vector<BYTE> colors( hdr.imagespec.wImageWidth * hdr.imagespec.wImageHeight );
		pStream->Read( &(colors[0]), colors.size() );

		CVec4 *pBuffer = &(*pImage)[0][0];
		for ( int i=0; i<colors.size(); ++i )
			pBuffer[i] = palette[ colors[i] ];
	}
	//
	if ( hdr.imagespec.descriptor.cTopToBottomOrder == 0 )
		FlipY( pImage );
	//
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// RLE encoding:
// 1 byte - the Repetition Count field
//          Run-length packet: bit7 = 1, other bits - run-length counter (up to 127 :)
//          Raw-data packet: bit7 = 0,  other bits - number of pixel values (up to 127 :)
// next bytes (depent on pixel format)
//          Run-lenght packet: single color value
//          Raw-data packet: 'umber of pixel values' color values

bool LoadRLETrueColorTGA( const STGAFileHeader &hdr, CImage *pImage, CDataStream *pStream )
{
	// skip image ID
	if ( hdr.cIDLength != 0 )
		pStream->Seek( hdr.cIDLength + pStream->GetPosition() );
	pImage->SetSizes( hdr.imagespec.wImageWidth, hdr.imagespec.wImageHeight );
	// true color have no color map information
	// ...
	// image data
	int nBPP = hdr.imagespec.cPixelDepth;
	int nReadSizeBytes = hdr.imagespec.wImageWidth * hdr.imagespec.wImageHeight * nBPP / 8;
	int nReadedBytes = 0;
	CVec4 *pBuffer = &(*pImage)[0][0];
	SColor pixelValue;
	BYTE cRepetitionCounter = 0;
	//
	int check = 0;
	switch ( nBPP )
	{
		case 24:
			{
				while ( nReadedBytes < nReadSizeBytes )
				{
					pStream->Read( &cRepetitionCounter, 1 );
					int nReps = cRepetitionCounter & 0x7f;
					if ( cRepetitionCounter & 0x80 )	// run-length packet
					{
						pStream->Read( &pixelValue, 3 );
						CVec4 color( pixelValue.r / 255.0f, pixelValue.g / 255.0f, pixelValue.b / 255.0f, 1 );
						for ( int k = 0; k < nReps; ++k )
							pBuffer[k] = color;
					}
					else															// raw-data packet
					{
						for ( int i=0; i<nReps; ++i )
						{
							pStream->Read( &pixelValue, 3 );
							CVec4 color( pixelValue.r / 255.0f, pixelValue.g / 255.0f, pixelValue.b / 255.0f, 1 );
							pBuffer[i] = color;
						}
					}
					pBuffer += nReps;
					nReadedBytes += nReps * 3;
				}
			}
			break;
		case 32:
			while ( nReadedBytes < nReadSizeBytes )
			{
				pStream->Read( &cRepetitionCounter, 1 );
				int nReps = cRepetitionCounter & 0x7f;
				if ( cRepetitionCounter & 0x80 )	// run-length packet
				{
					pStream->Read( &pixelValue, 4 );
					CVec4 color( pixelValue.r / 255.0f, pixelValue.g / 255.0f, pixelValue.b / 255.0f, pixelValue.a / 255.0f );
					for ( int k = 0; k < nReps; ++k )
						pBuffer[k] = color;
				}
				else															// raw-data packet
				{
					for ( int k = 0; k < nReps; ++k )
					{
						pStream->Read( &pixelValue, 4 );
						CVec4 color( pixelValue.r / 255.0f, pixelValue.g / 255.0f, pixelValue.b / 255.0f, pixelValue.a / 255.0f );
						pBuffer[k] = color;
					}
				}
				pBuffer += nReps;
				nReadedBytes += nReps * 4;
			}
			break;
		default:
			return false;
	}
	if ( check != nReadSizeBytes )
		return false;
	if ( hdr.imagespec.descriptor.cTopToBottomOrder == 0 )
		FlipY( pImage );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SaveImageAsTGA( CDataStream *pStream, const CImage &src )
{
	vector<DWORD> raw;
	Convert( src, &raw );
		// compose and write header
	STGAFileHeader hdr;
	Zero( hdr );
	hdr.cImageType = TGAIT_TRUE_COLOR;
	hdr.imagespec.cPixelDepth = 32;
	hdr.imagespec.wImageWidth = src.GetXSize();
	hdr.imagespec.wImageHeight = src.GetYSize();
	hdr.imagespec.descriptor.cTopToBottomOrder = 1;
	hdr.imagespec.descriptor.cAlphaChannelBits = 8;
	pStream->Write( &hdr, sizeof(hdr) );
	// write color data
	int nWriteSizeBytes = hdr.imagespec.wImageWidth * hdr.imagespec.wImageHeight * hdr.imagespec.cPixelDepth / 8;
	ASSERT( raw.size() * 4 == nWriteSizeBytes );
	pStream->Write( &raw[0], nWriteSizeBytes );
	// compose and write TGA file footer
	STGAFileFooter footer;
	Zero( footer );
	memcpy( footer.cSignature, "TRUEVISION-XFILE", 16 );
	footer.cReservedCharacter = '.';
	pStream->Write( &footer, sizeof(footer) );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}