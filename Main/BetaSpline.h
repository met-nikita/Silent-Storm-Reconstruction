#ifndef __BETASPLINE_H_
#define __BETASPLINE_H_
//
template <class T> class CArray2D;
class CVec3;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBetaSpline - Barsky beta-spline surface-patch evaluation over a 4x4 control grid, used by the
// terrain for smoothed height sampling. Release class, reconstructed from the matched-release decode
// (oracle: decomp/src/s2_spline.h). Global-scope, data-only in the PDB, so the class is defined
// here with real methods.
//
// Init(b1,b2) stores PRE-SCALED basis coefficients: with delta = 2*b1^3 + 4*b1^2 + 4*b1 + b2 + 2,
//   fBeta1 = 2*b1/delta, fBeta1_2 = 2*b1^2/delta, fBeta1_3 = 2*b1^3/delta, fBeta2 = b2/delta,
//   invdelta = 1/delta. (b1=1, b2=0 reduces to the uniform cubic B-spline.) Control points are
//   indexed P[col + 4*row] with u choosing the column and v the row. fVolCoeffs is a 16-entry
//   patch-averaging stencil (sums to exactly 1 for any beta). The 5-float Ave + F01/F02/F12 evaluate
//   a fixed rational-integral form over a parameter rectangle (constants 1/676, 2/676, 4/676 -- baked,
//   independent of beta; transcribed literally).
// Semantics from the decode: b_1 @0xb7ff0, Value @0xb80c0, Derivative @0xb8630, VolumeCoeffs @0xb9170,
//   Ave @0xb8050/@0xb9390/@0xb9720, Value(grid) @0xb94c0, Init @0xb9c80, F01/F02/F12 @0xb9cf0/@0xb9d80/@0xb9e10.
// NB: the two grid-sampling overloads read the height field through the engine's real CArray2D<float>
// (private storage + operator[]/GetXSize/GetYSize), where the decomp mirror reached pData/nXSize
// directly; the access is layout-identical.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBetaSpline
{
public:
	float fBeta1;
	float fBeta2;
	float invdelta;
	float fBeta1_3;
	float fBeta1_2;
	float fVolCoeffs[16];

	void Init( float b1, float b2 );                                            // @0xb9c80
	float b_1( const float *p ) const;                                          // @0xb7ff0
	void CalcBasis( float t, float *b ) const;                                  // (factored from Value @0xb80c0)
	void CalcBasisDeriv( float t, float *db ) const;                            // (factored from Derivative @0xb8630)
	CVec3 Value( float u, float v, const CVec3 *pts ) const;                    // @0xb80c0
	void Derivative( CVec3 &dU, CVec3 &dV, float u, float v, const CVec3 *pts ) const; // @0xb8630
	void VolumeCoeffs( float b1, float b2 );                                    // @0xb9170
	float Ave( const float *p ) const;                                          // @0xb8050
	float Ave( const CArray2D<float> &arr, int x, int y ) const;                // @0xb9390
	float Value( const CArray2D<float> &arr, int x, int y ) const;              // @0xb94c0
	static float FacA( const float ( *p )[4] );
	static float FacB( const float ( *p )[4] );
	static float FacC( const float ( *p )[4] );
	float F01( const float ( *a )[4], const float ( *b )[4] ) const;            // @0xb9cf0
	float F02( const float ( *a )[4], const float ( *b )[4] ) const;            // @0xb9d80
	float F12( const float ( *a )[4], const float ( *b )[4] ) const;            // @0xb9e10
	float Ave( float u1, float u2, float v1, float v2, const float *p ) const;  // @0xb9720
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __BETASPLINE_H_
