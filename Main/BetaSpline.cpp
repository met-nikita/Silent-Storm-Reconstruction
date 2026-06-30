#include "StdAfx.h"
//
#include "..\Misc\Geom.h"        // CVec3
#include "..\Misc\2Darray.h"     // CArray2D<T>
//
#include "BetaSpline.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBetaSpline - release beta-spline patch evaluation bodies. Reconstructed from the matched-release
// decode (decomp/src/s2_spline.h). All constants and signs are transcribed literally from the
// disassembly; the grid-sampling overloads use the engine's real CArray2D<float> public API
// (operator[] / GetXSize / GetYSize), layout-identical to the mirror's direct pData/nXSize access.
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// @0xb9c80
void CBetaSpline::Init( float b1, float b2 )
{
	fBeta1 = b1;
	fBeta2 = b2;
	invdelta = 1.0f / ( ( ( b1 + b1 + 4.0f ) * b1 + 4.0f ) * b1 + b2 + 2.0f );
	VolumeCoeffs( b1, b2 );
	fBeta1_3 = 2.0f * invdelta * b1 * b1 * b1;
	fBeta1_2 = 2.0f * invdelta * b1 * b1;
	fBeta1 = 2.0f * invdelta * b1;
	fBeta2 = invdelta * b2;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xb7ff0: the b(-1) basis from a { t, t^2, t^3 } power row.
float CBetaSpline::b_1( const float *p ) const
{
	return ( ( p[2] - p[0] * 3.0f ) + 2.0f ) * fBeta1 + ( ( p[2] - p[1] * 3.0f ) + 2.0f ) * fBeta1_2 +
	       ( ( ( p[2] + p[2] ) - p[1] * 3.0f ) + 1.0f ) * fBeta2 +
	       ( ( p[1] - p[0] * 3.0f ) + 3.0f ) * fBeta1_3 * p[0];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// the four basis values at t (factored out of Value @0xb80c0)
void CBetaSpline::CalcBasis( float t, float *b ) const
{
	float p[3] = { t, t * t, t * t * t };
	b[0] = ( 1.0f - t ) * ( 1.0f - t ) * ( 1.0f - t ) * fBeta1_3;
	b[1] = b_1( p );
	float f = ( 1.0f - p[2] ) * invdelta;
	b[2] = f + f + ( 3.0f - p[1] ) * fBeta1 * t +
	       ( ( 3.0f - t ) * fBeta1_2 + ( 3.0f - ( t + t ) ) * fBeta2 ) * p[1];
	b[3] = 2.0f * p[2] * invdelta;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// d/dt of the four basis values (from Derivative @0xb8630)
void CBetaSpline::CalcBasisDeriv( float t, float *db ) const
{
	float t2 = t * t;
	db[0] = ( 1.0f - t ) * ( 1.0f - t ) * fBeta1_3 * -3.0f;
	float f = ( t - 1.0f ) * fBeta2;
	db[1] = ( ( t - 1.0f ) * ( t - 1.0f ) * fBeta1_3 + ( t2 - 1.0f ) * fBeta1 +
	          ( f + f + ( t - 2.0f ) * fBeta1_2 ) * t ) *
	        3.0f;
	float f2 = fBeta2 + fBeta2;
	db[2] = ( ( ( fBeta1_2 + fBeta1_2 + f2 ) * t + fBeta1 ) -
	          ( invdelta + invdelta + fBeta1_2 + f2 + fBeta1 ) * t2 ) *
	        3.0f;
	db[3] = t2 * invdelta * 6.0f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xb80c0: surface point at (u, v) over pts[16] (P[col + 4*row]).
CVec3 CBetaSpline::Value( float u, float v, const CVec3 *pts ) const
{
	float bu[4], bv[4];
	CalcBasis( u, bu );
	CalcBasis( v, bv );
	CVec3 res;
	res.x = res.y = res.z = 0.0f;
	for ( int c = 0; c < 4; ++c )
		for ( int r = 0; r < 4; ++r )
		{
			float w = bu[c] * bv[r];
			const CVec3 &p = pts[c + 4 * r];
			res.x += w * p.x;
			res.y += w * p.y;
			res.z += w * p.z;
		}
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xb8630: dU = d/du, dV = d/dv of the surface at (u, v).
void CBetaSpline::Derivative( CVec3 &dU, CVec3 &dV, float u, float v, const CVec3 *pts ) const
{
	float bu[4], bv[4], dbu[4], dbv[4];
	CalcBasis( u, bu );
	CalcBasis( v, bv );
	CalcBasisDeriv( u, dbu );
	CalcBasisDeriv( v, dbv );
	dU.x = dU.y = dU.z = 0.0f;
	dV.x = dV.y = dV.z = 0.0f;
	for ( int c = 0; c < 4; ++c )
		for ( int r = 0; r < 4; ++r )
		{
			const CVec3 &p = pts[c + 4 * r];
			float wu = dbu[c] * bv[r];
			float wv = bu[c] * dbv[r];
			dU.x += wu * p.x;
			dU.y += wu * p.y;
			dU.z += wu * p.z;
			dV.x += wv * p.x;
			dV.y += wv * p.y;
			dV.z += wv * p.z;
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xb9170: the 16-entry patch-averaging stencil (symmetric; sums to 1). Called with the RAW b1/b2
// (invdelta must be set already).
void CBetaSpline::VolumeCoeffs( float b1, float b2 )
{
	float f = invdelta * invdelta * 0.25f;
	float b2_ = b1 * b1, b3 = b2_ * b1, b4 = b3 * b1, b5 = b4 * b1, b6 = b5 * b1;
	fVolCoeffs[0] = b6 * f;
	fVolCoeffs[1] = fVolCoeffs[4] = ( b5 * 5.0f + ( b6 + b4 ) * 3.0f + b3 * b2 ) * f;
	fVolCoeffs[2] = fVolCoeffs[8] = ( b4 * 5.0f + b5 * 3.0f + b3 * 3.0f + b3 * b2 ) * f;
	fVolCoeffs[3] = fVolCoeffs[12] = b3 * f;
	fVolCoeffs[5] = ( b4 * 43.0f + b5 * 30.0f + ( b6 + b2_ ) * 9.0f +
	                  ( b2_ * 10.0f + ( b3 + b1 ) * 6.0f + b2 ) * b2 + b3 * 30.0f ) *
	                f;
	fVolCoeffs[6] = fVolCoeffs[9] =
	    ( b1 * 9.0f + b2_ * 30.0f + b3 * 43.0f + b4 * 30.0f + b5 * 9.0f +
	      ( ( b2_ + b1 ) * 8.0f + b3 * 3.0f + b2 + 3.0f ) * b2 ) *
	    f;
	fVolCoeffs[7] = fVolCoeffs[13] = ( b1 * 3.0f + b2_ * 5.0f + b3 * 3.0f + b2 ) * f;
	fVolCoeffs[10] = ( b1 * 30.0f + b2_ * 43.0f + ( b4 + 1.0f ) * 9.0f +
	                   ( b1 * 10.0f + ( b2_ + 1.0f ) * 6.0f + b2 ) * b2 + b3 * 30.0f ) *
	                 f;
	fVolCoeffs[11] = fVolCoeffs[14] = ( b1 * 5.0f + ( b2_ + 1.0f ) * 3.0f + b2 ) * f;
	fVolCoeffs[15] = f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xb8050: stencil-weighted average of a 4x4 patch.
float CBetaSpline::Ave( const float *p ) const
{
	float fSum = 0.0f;
	for ( int i = 0; i < 16; ++i )
		fSum += fVolCoeffs[i] * p[i];
	return fSum;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xb9390: gather the 4x4 window around (x, y) (rows y-1..y+2, cols x-1..x+2); out of range falls
// back to the raw sample.
float CBetaSpline::Ave( const CArray2D<float> &arr, int x, int y ) const
{
	if ( !( x > 0 && y > 0 && x + 2 < arr.GetXSize() && y + 2 < arr.GetYSize() ) )
		return arr[y][x];
	float p[16];
	for ( int r = 0; r < 4; ++r )
		for ( int c = 0; c < 4; ++c )
			p[r * 4 + c] = arr[y - 1 + r][x - 1 + c];
	return Ave( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xb94c0: spline-smoothed height at grid knot (x, y) -- evaluates the patch built from the
// surrounding window at (u, v) = (0, 0).
float CBetaSpline::Value( const CArray2D<float> &arr, int x, int y ) const
{
	if ( !( x > 0 && y > 0 && x + 2 < arr.GetXSize() && y + 2 < arr.GetYSize() ) )
		return arr[y][x];
	CVec3 pts[16];
	for ( int r = 0; r < 4; ++r )
		for ( int c = 0; c < 4; ++c )
		{
			CVec3 &p = pts[r * 4 + c];
			p.x = (float)( x - 1 + c );
			p.y = (float)( y - 1 + r );
			p.z = arr[y - 1 + r][x - 1 + c];
		}
	return Value( 0.0f, 0.0f, pts ).z;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// polynomial factors of the rectangle-integral form (a "pair" is { t1, t1^2, t1^3, t1^4 } followed
// by the same powers of t2)
float CBetaSpline::FacA( const float ( *p )[4] )
{
	return p[1][2] * 5.0f +
	       ( ( p[0][3] + p[0][3] + ( p[0][0] * 9.0f - p[0][2] * 5.0f ) ) - p[1][0] * 9.0f ) -
	       ( p[1][3] + p[1][3] );
}
float CBetaSpline::FacB( const float ( *p )[4] )
{
	return ( ( ( p[0][1] * 6.0f - p[0][0] * 4.0f ) - p[0][2] * 4.0f ) + p[0][3] ) -
	       ( ( ( p[1][0] * 6.0f - 4.0f ) - p[1][1] * 4.0f ) + p[1][2] ) * p[1][0];
}
float CBetaSpline::FacC( const float ( *p )[4] )
{
	return p[0][3] + p[0][3] +
	       ( ( ( ( p[1][1] + p[1][0] ) * 3.0f + 2.0f ) - ( p[1][2] + p[1][2] ) ) * p[1][0] -
	         ( p[0][0] + p[0][0] + ( p[0][2] + p[0][1] ) * 3.0f ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xb9cf0 / @0xb9d80 / @0xb9e10
float CBetaSpline::F01( const float ( *a )[4], const float ( *b )[4] ) const { return FacA( a ) * FacB( b ); }
float CBetaSpline::F02( const float ( *a )[4], const float ( *b )[4] ) const { return FacC( a ) * FacB( b ); }
float CBetaSpline::F12( const float ( *a )[4], const float ( *b )[4] ) const { return FacA( b ) * FacC( a ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0xb9720: the rectangle-integral form over [u2..u1] x [v2..v1] of a 4x4 patch (signs and
// 1/676-family constants transcribed exactly).
float CBetaSpline::Ave( float u1, float u2, float v1, float v2, const float *p ) const
{
	float u[2][4] = { { u1, u1 * u1, u1 * u1 * u1, u1 * u1 * u1 * u1 },
	                  { u2, u2 * u2, u2 * u2 * u2, u2 * u2 * u2 * u2 } };
	float v[2][4] = { { v1, v1 * v1, v1 * v1 * v1, v1 * v1 * v1 * v1 },
	                  { v2, v2 * v2, v2 * v2 * v2, v2 * v2 * v2 * v2 } };
	const float c1 = 1.0f / 676.0f;  // 0x3AC1E4BC
	const float c2 = 2.0f / 676.0f;  // 0x3B41E4BC
	const float c4 = 4.0f / 676.0f;  // 0x3BC1E4BC
	float fAu = FacA( u ), fAv = FacA( v );
	float fBu = FacB( u ), fBv = FacB( v );
	float fCu = FacC( u ), fCv = FacC( v );
	float fDU4 = u[0][3] - u[1][3];
	float fDV4 = v[0][3] - v[1][3];
	return c1 * fBu * fBv * p[0] - c2 * F01( u, v ) * p[1] + c2 * F02( u, v ) * p[2] -
	       c1 * fBv * fDU4 * p[3] - c2 * F01( v, u ) * p[4] + c4 * fAu * fAv * p[5] -
	       c4 * F12( u, v ) * p[6] + c2 * fAv * fDU4 * p[7] + c2 * F02( v, u ) * p[8] -
	       c4 * F12( v, u ) * p[9] + c4 * fCv * fCu * p[10] - c2 * fCv * fDU4 * p[11] -
	       c1 * fBu * fDV4 * p[12] + c2 * fAu * fDV4 * p[13] - c2 * fCu * fDV4 * p[14] +
	       c1 * fDU4 * fDV4 * p[15];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
