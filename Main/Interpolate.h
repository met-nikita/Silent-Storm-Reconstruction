#ifndef __Interpolate_H_
#define __Interpolate_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// universal bilinear interpolation
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T, class TInterp> 
inline typename TInterp::RET GetBilinear( const CArray2D<T> &data, float x, float y, const TInterp &interp )
{
	x = Clamp( x, 0.0f, data.GetXSize() - 1.01f );
	y = Clamp( y, 0.0f, data.GetYSize() - 1.01f );
	int nX = x, nY = y;
	T a = interp( data[nY][nX], data[nY][nX+1], x - nX );
	T b = interp( data[nY+1][nX], data[nY+1][nX+1], x - nX );
	return interp( a, b, y - nY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct TLinearInterpolate
{
	typedef float RET;
	template<class T>
		float operator()( T a, T b, float f ) const { return (1-f) * a + f * b; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
