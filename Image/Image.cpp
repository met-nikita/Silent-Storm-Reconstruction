#include "StdAfx.h"
#include "Image.h"
namespace NImage
{
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SPixelConvertInfo::InitMaskInfo( DWORD dwABitMask, DWORD dwRBitMask, DWORD dwGBitMask, DWORD dwBBitMask )
{
	DWORD dwMask, dwBitShift, dwBitCount;
	//
	memset( this, 0, sizeof(*this) );
	// Get Alpha Mask info
	dwMask = dwABitMask;
	dwBitShift = dwBitCount = 0;
	if ( dwMask )
	{
		for ( ; (dwMask & 0x1) == 0; dwMask >>= 1 )
			dwBitShift++;
		for ( ; (dwMask & 0x1) == 1; dwMask >>= 1 )
			dwBitCount++;
	}
	dwAMask  = dwABitMask;
	dwABits  = dwBitCount;
	dwAShift = dwBitShift;
	dwAMul = (1 << dwABits) - 1;
	// Get Red Mask info
	dwMask = dwRBitMask;
	dwBitShift = dwBitCount = 0;
	if ( dwMask )
	{
		for ( ; (dwMask & 0x1) == 0; dwMask >>= 1 )
			dwBitShift++;
		for ( ; (dwMask & 0x1) == 1; dwMask >>= 1 )
			dwBitCount++;
	}
	dwRMask  = dwRBitMask;
	dwRBits  = dwBitCount;
	dwRShift = dwBitShift;
	// Get Green Mask info
	dwMask = dwGBitMask;
	dwBitShift = dwBitCount = 0;
	if ( dwMask )
	{
		for ( ; (dwMask & 0x1) == 0; dwMask >>= 1 )
			dwBitShift++;
		for ( ; (dwMask & 0x1) == 1; dwMask >>= 1 )
			dwBitCount++;
	}
	dwGMask  = dwGBitMask;
	dwGBits  = dwBitCount;
	dwGShift = dwBitShift;
	// Get Blue Mask info
	dwMask = dwBBitMask;
	dwBitShift = dwBitCount = 0;
	if ( dwMask )
	{
		for ( ; (dwMask & 0x1) == 0; dwMask >>= 1 )
			dwBitShift++;
		for ( ; (dwMask & 0x1) == 1; dwMask >>= 1 )
			dwBitCount++;
	}
	dwBMask  = dwBBitMask;
	dwBBits  = dwBitCount;
	dwBShift = dwBitShift;
	//
	DWORD dwMinBits = Min( dwRBits, Min( dwGBits, dwBBits ) );
	dwRMul = ((1 << dwMinBits) - 1) << ( dwRBits - dwMinBits );
	dwGMul = ((1 << dwMinBits) - 1) << ( dwGBits - dwMinBits );
	dwBMul = ((1 << dwMinBits) - 1) << ( dwBBits - dwMinBits );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// color composition/decomposition (from ARGB, to ARGB)
DWORD SPixelConvertInfo::ComposeColorSlow( const CVec4 &color ) const
{
	DWORD a = Float2Int( Clamp( color.a, 0.0f, 1.0f ) * dwAMul ) << dwAShift;
	DWORD r = Float2Int( Clamp( color.r, 0.0f, 1.0f ) * dwRMul ) << dwRShift;
	DWORD g = Float2Int( Clamp( color.g, 0.0f, 1.0f ) * dwGMul ) << dwGShift;
	DWORD b = Float2Int( Clamp( color.b, 0.0f, 1.0f ) * dwBMul ) << dwBShift;
	return (r | g | b | a);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD SPixelConvertInfo::DecompColor( DWORD dwColor ) const
{
	// Convert Alpha component
	DWORD a = ((dwColor & dwAMask) >> dwAShift);
	a <<= (8 - dwABits);
	a <<= 24;
	// Convert Red component
	DWORD r = ((dwColor & dwRMask) >> dwRShift);
	r <<= (8 - dwRBits);
	r <<= 16;
	// Convert Green component
	DWORD g = ((dwColor & dwGMask) >> dwGShift);
	g <<= (8 - dwGBits);
	g <<= 8;
	// Convert Blue component
	DWORD b = ((dwColor & dwBMask) >> dwBShift);
	b <<= (8 - dwBBits);
	// Return converted color
	return (r | g | b | a);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
