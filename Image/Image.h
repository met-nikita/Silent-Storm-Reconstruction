#ifndef __IMAGE_H__
#define __IMAGE_H__
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Misc\Geom.h"
#include "..\Main\GPixelFormat.h"
#include "..\Misc\2DArray.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NImage
{
typedef CArray2D<CVec4> CImage;
struct SMippedImage
{
	vector<CImage> levels;
};

inline void Convert( const vector<DWORD> &pixels, int nXSize, int nYSize, CImage *pRes )
{
	ASSERT( nXSize * nYSize == pixels.size() );
	pRes->SetSizes( nXSize, nYSize );
	for ( int y = 0; y < nYSize; ++y )
	{
		for ( int x = 0; x < nXSize; ++x )
		{
			NGfx::SPixel8888 p;
			p.color = pixels[y * nXSize + x];
			(*pRes)[y][x] = CVec4( p.r / 255.0f, p.g / 255.0f, p.b / 255.0f, p.a / 255.0f );
		}
	}
}
inline void FlipY( CImage *p )
{
	for ( int y = 0; y < p->GetYSize() / 2; ++y )
	{
		for ( int x = 0; x < p->GetXSize(); ++x )
			swap( (*p)[p->GetYSize() - 1 - y][x], (*p)[y][x] );
	}
}
inline DWORD RoundComponent( float f ) { return Clamp( Float2Int( f * 255 ), 0, 255 ); }
inline void Convert( const CImage &src, vector<DWORD> *pRes )
{
	int nXSize = src.GetXSize();
	pRes->resize( nXSize * src.GetYSize() );
	for ( int y = 0; y < src.GetYSize(); ++y )
	{
		for ( int x = 0; x < src.GetXSize(); ++x )
		{
			const CVec4 &s = src[y][x];
			DWORD dwR = RoundComponent( s.r ) << 16;
			DWORD dwG = RoundComponent( s.g ) << 8;
			DWORD dwB = RoundComponent( s.b );
			DWORD dwA = RoundComponent( s.a ) << 24;
			(*pRes)[y * nXSize + x ] = dwR | dwG | dwB | dwA;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// pixel format conversion class
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPixelConvertInfo
{
public:
	DWORD dwBitDepth;                     // bit depth of this format
	// Alpha channel info
	DWORD dwAMask;		                    // bit mask
	DWORD dwABits;		                    // # of bits in mask
	DWORD dwAShift;		                    // # of bits to shift down to canonical position
	DWORD dwAMul;
	// Red channel info
	DWORD dwRMask;		                    // bit mask
	DWORD dwRBits;		                    // # of bits in mask
	DWORD dwRShift;		                    // # of bits to shift down to canonical position
	DWORD dwRMul;
	// Green channel info
	DWORD dwGMask;		                    // bit mask
	DWORD dwGBits;		                    // # of bits in mask
	DWORD dwGShift;		                    // # of bits to shift down to canonical position
	DWORD dwGMul;
	// Blue channel Info
	DWORD dwBMask;		                    // bit mask
	DWORD dwBBits;		                    // # of bits in mask
	DWORD dwBShift;		                    // # of bits to shift down to canonical position
	DWORD dwBMul;
public:
	SPixelConvertInfo() {  }
	SPixelConvertInfo( DWORD dwABitMask, DWORD dwRBitMask, DWORD dwGBitMask, DWORD dwBBitMask ) { InitMaskInfo( dwABitMask, dwRBitMask, dwGBitMask, dwBBitMask ); }
	// initialization
	bool InitMaskInfo( DWORD dwABitMask, DWORD dwRBitMask, DWORD dwGBitMask, DWORD dwBBitMask );
	// color composition/decomposition (from ARGB, to ARGB)
	//DWORD ComposeColor( DWORD dwColor ) const;
	DWORD ComposeColorSlow( const CVec4 &color ) const;
	DWORD DecompColor( DWORD dwColor ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __IMAGE_H__