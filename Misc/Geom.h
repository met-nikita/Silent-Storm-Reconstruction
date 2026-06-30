#ifndef __GEOM_H__
#define __GEOM_H__
////////////////////////////////////////////////////////////////////////////////////////////////////
#include <math.h>
#include "tools.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma pack( 4 )
////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// ** different vector classes: 2D, 3D and 4D vectors with all necessary functions
// ************************************************************************************************************************ //
////////////////////////////////////////////////////////////////////////////////////////////////////
// 2D vector
class CVec2
{
public:
  union
  {
    float m[2];
    struct
    {
      float x, y;
    };
    struct
    {
      float u, v;                       // for texture coord
    };
  };
public:
  CVec2() {  }
  CVec2( const float ax, const float ay ) : x(ax), y(ay) {  }
  // setup
  void Set( const float ax, const float ay ) { x = ax; y = ay; }
  // indexed access
  float& operator[]( int i ) { return m[i]; };
  const float& operator[]( int i ) const { return m[i]; }
  // comparison
	bool operator==( const CVec2 &v ) const { return ( (v.x == x) && (v.y == y) ); }
	bool operator!=( const CVec2 &v ) const { return ( (v.x != x) || (v.y != y) ); }
  // internal data non-math modification
  void Maximize( const CVec2 &v ) { x = Max( x, v.x ); y = Max( y, v.y ); }
  void Minimize( const CVec2 &v ) { x = Min( x, v.x ); y = Min( y, v.y ); }
  //
  void Negate( const CVec2 &v ) { x = -v.x; y = -v.y; } // this = -v
  void Negate() { x = -x; y = -y; }     // this = -this
  void Add( const CVec2 &v1, const CVec2 &v2 ) { x = v1.x + v2.x; y = v1.y + v2.y; } // this = v1 + v2
  void Sub( const CVec2 &v1, const CVec2 &v2 ) { x = v1.x - v2.x; y = v1.y - v2.y; } // this = v1 - v2
  void Displace( const CVec2 &v1, const CVec2 &v2, const float t ) { x = v1.x + t*v2.x; y = v1.y + t*v2.y; } //  this = v1 + t*v2
  void Displace( const CVec2 &v, const float t ) { x += t*v.x; y += t*v.y; } //  this += t*v; 
  void Interpolate( const CVec2 &v1, const CVec2 &v2, const float t ) { x = (1-t)*v1.x + t*v2.x; y = (1-t)*v1.y + t*v2.y; } //this = t*v1 + (1 - t)*v2
  // mathematical operations
  CVec2& operator+=( const CVec2 &v ) { x += v.x; y += v.y; return *this; }
  CVec2& operator-=( const CVec2 &v ) { x -= v.x; y -= v.y; return *this; }
  CVec2& operator*=( const float d ) { x *= d; y *= d; return *this; }
  CVec2& operator/=( const float d ) { float d1 = 1.0f / d; x *= d1; y *= d1; return *this; }
};
const CVec2 VNULL2 = CVec2( 0, 0 );
const CVec2 V2_AXIS_X = CVec2( 1, 0 );
const CVec2 V2_AXIS_Y = CVec2( 0, 1 );
////////////////////////////////////////////////////////////////////////////////////////////////////
inline const CVec2 operator-( const CVec2 &a) { return CVec2(-a.x, -a.y); }
inline const CVec2 operator+( const CVec2 &a, const CVec2 &b ) { return CVec2( a.x + b.x, a.y + b.y ); }
inline const CVec2 operator-( const CVec2 &a, const CVec2 &b ) { return CVec2( a.x - b.x, a.y - b.y ); }
inline float operator*( const CVec2 &a, const CVec2 &b ) { return ( a.x*b.x + a.y*b.y ); }
inline const CVec2 operator*( const CVec2 &a, const float b ) { return CVec2( a.x*b, a.y*b ); }
inline const CVec2 operator*( const float a, const CVec2 &b ) { return CVec2( b.x*a, b.y*a ); }
inline const CVec2 operator/( const CVec2 &a, const float b ) { float b1 = 1.0f/b; return CVec2( a.x*b1, a.y*b1 ); }
inline float fabs2( const CVec2 &a ) { return a.x*a.x + a.y*a.y; }
inline float fabs( const CVec2 &a ) { return sqrt( fabs2( a ) ); }
inline bool Normalize( CVec2 *pVec ) { float fLeng = fabs2(*pVec); if ( fLeng != 0 ) *pVec /= sqrt(fLeng); return fLeng != 0; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// 3D vector
class CVec3
{
public:
  union
  {
    float m[3];
    struct
    {
      float x, y, z;
    };
    struct
    {
      float r, g, b;                    // for color components
    };
    struct
    {
      float u, v, q;                    // for texture coord
    };
  };
public:
  CVec3() {  }
  CVec3( const float ax, const float ay, const float az ) : x(ax), y(ay), z(az) {  }
  CVec3( const CVec2 &a, float _z ) : x(a.x), y(a.y), z(_z) {  }
  // setup
  void Set( const float ax, const float ay, const float az ) { x = ax; y = ay; z = az; }
  // indexed access
  float& operator[]( int i ) { return m[i]; };
  const float& operator[]( int i ) const { return m[i]; }
  // comparison
	bool operator==( const CVec3 &v ) const { return ( (v.x == x) && (v.y == y) && (v.z == z) ); }
	bool operator!=( const CVec3 &v ) const { return ( (v.x != x) || (v.y != y) || (v.z != z) ); }
  // internal data non-math modification
  void Maximize( const CVec3 &v ) { x = Max( x, v.x ); y = Max( y, v.y ); z = Max( z, v.z ); }
  void Minimize( const CVec3 &v ) { x = Min( x, v.x ); y = Min( y, v.y ); z = Min( z, v.z ); }
  //
  void Negate( const CVec3 &v ) { x = -v.x; y = -v.y; z = -v.z; } // this = -v
  void Negate() { x = -x; y = -y; z = -z; }     // this = -this
  void Add( const CVec3 &v1, const CVec3 &v2 ) { x = v1.x + v2.x; y = v1.y + v2.y; z = v1.z + v2.z; } // this = v1 + v2
  void Sub( const CVec3 &v1, const CVec3 &v2 ) { x = v1.x - v2.x; y = v1.y - v2.y; z = v1.z - v2.z; } // this = v1 - v2
  void Displace( const CVec3 &v1, const CVec3 &v2, const float t ) { x = v1.x + t*v2.x; y = v1.y + t*v2.y; z = v1.z + t*v2.z; } //  this = v1 + t*v2
  void Displace( const CVec3 &v, const float t ) { x += t*v.x; y += t*v.y; z += t*v.z; } //  this += t*v; 
  void Interpolate( const CVec3 &v1, const CVec3 &v2, const float t ) { x = (1-t)*v1.x + t*v2.x; y = (1-t)*v1.y + t*v2.y; z = (1-t)*v1.z + t*v2.z; } //this = t*v1 + (1 - t)*v2
  // mathematical operations
  CVec3& operator+=( const CVec3 &v ) { x += v.x; y += v.y; z += v.z; return *this; }
  CVec3& operator-=( const CVec3 &v ) { x -= v.x; y -= v.y; z -= v.z; return *this; }
  CVec3& operator*=( const float d ) { x *= d; y *= d; z *= d; return *this; }
  CVec3& operator/=( const float d ) { float d1 = 1.0f / d; x *= d1; y *= d1; z *= d1; return *this; }
};
const CVec3 VNULL3 = CVec3( 0, 0, 0 );
const CVec3 V3_AXIS_X = CVec3( 1, 0, 0 );
const CVec3 V3_AXIS_Y = CVec3( 0, 1, 0 );
const CVec3 V3_AXIS_Z = CVec3( 0, 0, 1 );
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SVec3Hash
{
	int operator()( const CVec3 &a ) const { const int *p = (const int*)&a; return p[0] ^ p[1] ^ p[2]; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline const CVec3 operator-( const CVec3 &a) { return CVec3(-a.x, -a.y, -a.z); }
inline const CVec3 operator+( const CVec3 &a, const CVec3 &b ) { return CVec3( a.x + b.x, a.y + b.y, a.z + b.z ); }
inline const CVec3 operator-( const CVec3 &a, const CVec3 &b ) { return CVec3( a.x - b.x, a.y - b.y, a.z - b.z ); }
inline float operator*( const CVec3 &a, const CVec3 &b ) { return ( a.x*b.x + a.y*b.y + a.z*b.z ); }
inline const CVec3 operator*( const CVec3 &a, const float b ) { return CVec3( a.x*b, a.y*b, a.z*b ); }
inline const CVec3 operator*( const float a, const CVec3 &b ) { return CVec3( b.x*a, b.y*a, b.z*a ); }
inline const CVec3 operator/( const CVec3 &a, const float b ) { float b1 = 1.0f/b; return CVec3( a.x*b1, a.y*b1, a.z*b1 ); }
inline const CVec3 operator^( const CVec3 &a, const CVec3 &b ) { return CVec3( a.y*b.z - b.y*a.z, a.z*b.x - b.z*a.x, a.x*b.y - b.x*a.y ); }
inline float fabs2( const CVec3 &a ) { return a.x*a.x + a.y*a.y + a.z*a.z; }
inline float fabs( const CVec3 &a ) { return static_cast<float>( sqrt( fabs2(a) ) ); }
inline float fabsxy2( const CVec3 &a ) { return a.x*a.x + a.y*a.y; }
inline float fabsxy( const CVec3 &a ) { return static_cast<float>( sqrt( fabsxy2(a) ) ); }
inline bool Normalize( CVec3 *pVec ) { float fLeng = fabs2(*pVec); if ( fLeng != 0 ) *pVec /= sqrt(fLeng); return fLeng != 0; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// 4D vector
class CVec4
{
public:
  union
  {
    float m[4];
    struct
    {
      float x, y, z, w;
    };
    struct
    {
      float r, g, b, a;                 // for color components
    };
    struct
    {
      float u, v, q;                 // for texture coord
    };
  };
public:
  CVec4() {}
  CVec4( const float ax, const float ay, const float az, const float aw ) : x(ax), y(ay), z(az), w(aw) {}
	CVec4( const CVec3 &a, float _w ): x(a.x), y(a.y), z(a.z), w(_w) {}
  // cross-vector assignment as homogeneous vector
  CVec4& operator=( const CVec2 &v ) { x = v.x; y = v.y; z = 0; w = 1; return *this; }
  CVec4& operator=( const CVec3 &v ) { x = v.x; y = v.y; z = v.z; w = 1; return *this; }
  // setup
  void Set( const float ax, const float ay, const float az, const float aw ) { x = ax; y = ay; z = az; w = aw; }
  void Set( const CVec3 &a, float _w = 1 ) { x = a.x; y = a.y; z = a.z; w = _w; }
  // indexed access
  float& operator[]( int i ) { return m[i]; };
  const float& operator[]( int i ) const { return m[i]; }
  // comparison
	bool operator==( const CVec4 &v ) const { return ( (v.x == x) && (v.y == y) && (v.z == z) && (v.w == w) ); }
	bool operator!=( const CVec4 &v ) const { return ( (v.x != x) || (v.y != y) || (v.z != z) || (v.w != w) ); }
  // internal data non-math modification
  void Maximize( const CVec4 &v ) { x = Max( x, v.x ); y = Max( y, v.y ); z = Max( z, v.z ); w = Max( w, v.w ); }
  void Minimize( const CVec4 &v ) { x = Min( x, v.x ); y = Min( y, v.y ); z = Min( z, v.z ); w = Min( w, v.w ); }
  //
  void Negate( const CVec4 &v ) { x = -v.x; y = -v.y; z = -v.z; w = -v.w; } // this = -v
  void Negate() { x = -x; y = -y; z = -z; w = -w; }     // this = -this
  void Add( const CVec4 &v1, const CVec4 &v2 ) { x = v1.x + v2.x; y = v1.y + v2.y; z = v1.z + v2.z; w = v1.w + v2.w; } // this = v1 + v2
  void Sub( const CVec4 &v1, const CVec4 &v2 ) { x = v1.x - v2.x; y = v1.y - v2.y; z = v1.z - v2.z; w = v1.w - v2.w; } // this = v1 - v2
  void Displace( const CVec4 &v1, const CVec4 &v2, const float t ) { x = v1.x + t*v2.x; y = v1.y + t*v2.y; z = v1.z + t*v2.z; w = v1.w + t*v2.w; } //  this = v1 + t*v2
  void Displace( const CVec4 &v, const float t ) { x += t*v.x; y += t*v.y; z += t*v.z; w += t*v.w; } //  this += t*v; 
  void Interpolate( const CVec4 &v1, const CVec4 &v2, const float t ) { x = (1-t)*v1.x + t*v2.x; y = (1-t)*v1.y + t*v2.y; z = (1-t)*v1.z + t*v2.z; w = (1-t)*v1.w + t*v2.w; } //this = t*v1 + (1 - t)*v2
  // mathematical operations
  CVec4& operator+=( const CVec4 &v ) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
  CVec4& operator-=( const CVec4 &v ) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
  CVec4& operator*=( const float d ) { x *= d; y *= d; z *= d; w *= d; return *this; }
  CVec4& operator/=( const float d ) { float d1 = 1.0f / d; x *= d1; y *= d1; z *= d1; w *= d1; return *this; }
};
const CVec4 VNULL4 = CVec4( 0, 0, 0, 0 );
const CVec4 V4_AXIS_X = CVec4( 1, 0, 0, 0 );
const CVec4 V4_AXIS_Y = CVec4( 0, 1, 0, 0 );
const CVec4 V4_AXIS_Z = CVec4( 0, 0, 1, 0 );
const CVec4 V4_AXIS_W = CVec4( 0, 0, 0, 1 );
////////////////////////////////////////////////////////////////////////////////////////////////////
inline const CVec4 operator-( const CVec4 &a) { return CVec4(-a.x, -a.y, -a.z, -a.w); }
inline const CVec4 operator+( const CVec4 &a, const CVec4 &b ) { return CVec4( a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w ); }
inline const CVec4 operator-( const CVec4 &a, const CVec4 &b ) { return CVec4( a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w ); }
inline float operator*( const CVec4 &a, const CVec4 &b ) { return ( a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w ); }
inline const CVec4 operator*( const CVec4 &a, const float b ) { return CVec4( a.x*b, a.y*b, a.z*b, a.w*b ); }
inline const CVec4 operator*( const float a, const CVec4 &b ) { return CVec4( b.x*a, b.y*a, b.z*a, b.w*a ); }
inline const CVec4 operator/( const CVec4 &a, const float b ) { float b1 = 1.0f/b; return CVec4( a.x*b1, a.y*b1, a.z*b1, a.w*b1 ); }
inline float fabs2( const CVec4 &a ) { return a.x*a.x + a.y*a.y + a.z*a.z + a.w*a.w; }
inline float fabs( const CVec4 &a ) { return static_cast<float>( sqrt( fabs2(a) ) ); }
inline float fabsxyz2( const CVec4 &a ) { return a.x*a.x + a.y*a.y + a.z*a.z; }
inline float fabsxyz( const CVec4 &a ) { return static_cast<float>( sqrt( fabsxyz2(a) ) ); }
inline float fabsxy2( const CVec4 &a ) { return a.x*a.x + a.y*a.y; }
inline float fabsxy( const CVec4 &a ) { return static_cast<float>( sqrt( fabsxy2(a) ) ); }
inline bool Normalize( CVec4 *pVec ) { float fLeng = fabs2(*pVec); if ( fLeng != 0 ) *pVec /= sqrt(fLeng); return fLeng != 0; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// plane in 3D space
//      pt2
//     /
//  pt0
//     \
//      pt1
struct SPlane
{
public:
  union
  {
    struct 
    {
      CVec3 n;
      float d;
    };
    struct 
    {
      CVec4 vec4;
    };
  };
public:
  SPlane( const CVec3 &ptNormale, const float fDist ) : n( ptNormale ), d( fDist ) {  }
  SPlane( const CVec4 &pt ) : vec4( pt ) {  }
  SPlane() {  }
  // setup functions
  bool Set( const CVec3 &pt0, const CVec3 &pt1, const CVec3 &pt2 );
  bool Set( float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2 );
  void Set( const CVec3 &ptNormale, const float fDist ) { n = ptNormale; d = fDist; }
  // recalc 'd' coeff for a plane with the point 'pt'
  void RecalcDist( const CVec3 &pt ) { d = -( n * pt ); }
	// distance functions
	float GetDistanceToPoint( const CVec3 &pt ) const { return ( n*pt + d ); }
	bool IsPointOnPlane( const CVec3 &pt ) const { return n*pt == -d; }
	bool IsPointOverPlane( const CVec3 &pt ) const { return n*pt > -d; }
	bool IsPointUnderPlane( const CVec3 &pt ) const { return n*pt < -d; }
  // протестировать, не лежит ли точка под плоскостью. вернуть 0x80000000 если это так или 0 в противном случае
  DWORD CheckPointUnderPlane( const CVec3 &pt ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	|x'|	 |xx xy xz xw|	 |x|										 |x'|	  |11 12 13 14|	  |x|
//	|y'| = |yx yy yz yw| * |y|										 |y'| = |21 22 23 24| * |y|
//	|z'|	 |zx zy zz zw|	 |z|										 |z'|	  |31 32 33 34|	  |z|
//	|w'|	 |wx wy wz ww|	 |w|										 |w'|	  |41 42 43 44|	  |w|
//
// homogeneous matrix (4x4) for any type of transformation
struct SHMatrix
{
public :
	union
	{
		struct
		{
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
			float _41, _42, _43, _44;
		};
		struct
		{
			float xx, xy, xz, xw;
			float yx, yy, yz, yw;
			float zx, zy, zz, zw;
			float wx, wy, wz, ww;
		};
		struct  
		{
			CVec4 x, y, z, w;
		};
		struct
		{
			CVec3 x3; float xw3;
			CVec3 y3; float yw3;
			CVec3 z3; float zw3;
			CVec3 w3; float ww3;
		};
	};
public :
	// matrix-vector multiplication 
	SHMatrix() {};
	void RotateVector( CVec3 *pResult, const CVec3 &pt ) const;
	void RotateVectorTransposed( CVec3 *pResult, const CVec3 &pt ) const;
	void RotateHDirection( CVec4 *pResult, const CVec3 &pt ) const;
	void RotateHVector( CVec3 *pResult, const CVec3 &pt ) const;
	void RotateHVector( CVec4 *pResult, const CVec3 &pt ) const;
	void RotateHVector( CVec4 *pResult, const CVec4 &pt ) const;
	void RotateHVectorTransposed( CVec4 *pResult, const CVec4 &pt ) const;
	bool HomogeneousInverse( const SHMatrix &m );
	const CVec3 GetTranslation() const { return CVec3( _14, _24, _34 ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// прямое и обратное преобразование вместе
struct SFBTransform
{
	SHMatrix forward, backward;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// quaternion
class CQuat
{
private:
  union
  {
    struct
    {
      float i, j, k, s;
    };
    struct
    {
      float x, y, z, w;
    };
    struct
    {
      CVec3 n;
      float r;
    };
    struct
    {
      CVec4 vec4;
    };
  };
public:
  CQuat( float fX, float fY, float fZ, float fW ) : x( fX ), y( fY ), z( fZ ), w( fW ) {}
  CQuat( float fAngle, const CVec3 &ptAxis, const bool bNormalizeAxis = false );
  CQuat() {  }
  // composition
	void FromAngleAxis( float fAngle, const CVec3 &ptAxis, const bool bNormalizeAxis = false );
	//void FromAngleAxis( float fAngle, float fAxisX, float fAxisY, float fAxisZ, const bool bNormalizeAxis = false );
	void FromEulerMatrix( const SHMatrix &m );
  void FromEulerAngles( float yaw, float pitch, float roll );
	// decomposition
	void DecompAngleAxis( float *pfAngle, CVec3 *pptAxis ) const;
	void DecompAngleAxis( float *pfAngle, float *pfAxisX, float *pfAxisY, float *pfAxisZ ) const;
	void DecompEulerMatrix( SHMatrix *pMatrix ) const;
	void DecompReversedEulerMatrix( SHMatrix *pMatrix ) const;
  // internal data non-math modification
  void Maximize( const CQuat &v ) { x = Max( x, v.x ); y = Max( y, v.y ); z = Max( z, v.z ); w = Max( w, v.w ); }
  void Minimize( const CQuat &v ) { x = Min( x, v.x ); y = Min( y, v.y ); z = Min( z, v.z ); w = Min( w, v.w ); }
  //
  void Negate( const CQuat &q ) { x = -q.x; y = -q.y; z = -q.z; w = -q.w; } // this = -v
  void Negate() { x = -x; y = -y; z = -z; w = -w; }     // this = -this
	bool Inverse( const CQuat &q );
	bool Inverse();
	void UnitInverse( const CQuat &q ) { x = -q.x; y = -q.y; z = -q.z; w = q.w; }
	void UnitInverse() { x = -x; y = -y; z = -z; }
  void UnitInverseX() { x = -x; }
  void UnitInverseY() { y = -y; }
  void UnitInverseZ() { z = -z; }
  //
  void Deriv( const CQuat &q, const CVec3 &v );
  // some neccessary operators
  friend const CQuat operator*( const CQuat &a, const CQuat &b );
  friend const CQuat operator/( const CQuat &a, const CQuat &b );
  CQuat& operator*=( const CQuat &quat );
	CQuat& operator/=( const CQuat &quat );
	const CQuat operator*( const float c ) const;
	const CQuat operator+( const CQuat &q ) const { return CQuat( x + q.x, y + q.y, z + q.z, w + q.w ); }
	const CQuat operator-() const { return CQuat( -x, -y, -z, -w ); }        // unary minus
	float Dot( const CQuat &quat ) const { return x*quat.x + y*quat.y + z*quat.z + w*quat.w; }
	//
	void MinimizeRotationAngle() { if ( w < 0 ) { x = -x; y = -y; z = -z; w = -w; } }
	// mathematical functions
  const CQuat Exp() const;
  const CQuat Log() const;
	// interpolation
  // Spherical Linear intERPolation from 'p' to 'q' with coeff 'factor'
	void Slerp( const float factor, const CQuat &p, const CQuat &q );
  void Interpolate( const CQuat &p, const CQuat &q, const float t ) { Slerp(t, p, q); }
	// rotate vector via quaternion itself
  const CVec3 Rotate( const CVec3 &r ) const;
  void Rotate( CVec3 *pResult, const CVec3 &vec ) const;
	// rotate vectors 'X = (1, 0, 0)', 'Y = (0, 1, 0)' and 'Z = (0, 0, 1)' via quaternion
  const CVec3 GetXAxis() const;
  const CVec3 GetYAxis() const;
  const CVec3 GetZAxis() const;
  void GetXAxis( CVec3 *pResult ) const;
  void GetYAxis( CVec3 *pResult ) const;
  void GetZAxis( CVec3 *pResult ) const;
  //
  friend float fabs2( const CQuat &q ) { return fabs2( q.x, q.y, q.z, q.w ); }
  friend float fabs( const CQuat &q ) { return static_cast<float>( sqrt( fabs2(q) ) ); }
	friend bool Normalize( CQuat *pQ ) { return ::Normalize(pQ->x, pQ->y, pQ->z, pQ->w); }
};
const CQuat QNULL = CQuat( 0, CVec3(1, 0, 0) );
////////////////////////////////////////////////////////////////////////////////////////////////////
// template POINT
template <class TYPE>
class CTPoint
{
public:
	TYPE x, y;

	CTPoint() {}
	CTPoint( TYPE _x, TYPE _y ): x(_x), y(_y) {}
	
	bool operator==( const CTPoint<TYPE> &v ) const { return ( (v.x == x) && (v.y == y) ); }
	bool operator!=( const CTPoint<TYPE> &v ) const { return ( (v.x != x) || (v.y != y) ); }
  // mathematical operations
  CTPoint& operator+=( const CTPoint &v ) { x += v.x; y += v.y; return *this; }
  CTPoint& operator-=( const CTPoint &v ) { x -= v.x; y -= v.y; return *this; }
  CTPoint& operator*=( const TYPE d ) { x *= d; y *= d; return *this; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
inline float fabs( const CTPoint<T> &q ) { return static_cast<float>( sqrt( sqr(q.x) + sqr(q.y) ) ); }
template<class T>
inline const CTPoint<T> operator-( const CTPoint<T> &a) { return CTPoint<T>(-a.x, -a.y); }
template<class T>
inline const CTPoint<T> operator+( const CTPoint<T> &a, const CTPoint<T> &b ) { return CTPoint<T>( a.x + b.x, a.y + b.y ); }
template<class T>
inline const CTPoint<T> operator-( const CTPoint<T> &a, const CTPoint<T> &b ) { return CTPoint<T>( a.x - b.x, a.y - b.y ); }
template<class T>
inline const CTPoint<T> operator*( const CTPoint<T> &a, const float b ) { return CTPoint<T>( a.x*b, a.y*b ); }
template<class T>
inline const CTPoint<T> operator*( const T a, const CTPoint<T> &b ) { return CTPoint<T>( b.x*a, b.y*a ); }
template<class T>
inline const CTPoint<T> operator*( const CTPoint<T> &b, const T a ) { return CTPoint<T>( b.x*a, b.y*a ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
// template RECT
template <class TYPE>
class CTRect
{
public :
	union                                 // left (minimal) x
	{
		TYPE left;
		TYPE x1;
		TYPE minx;
	};
	union                                 // top (minimal) y
	{
		TYPE top;
		TYPE y1;
		TYPE miny;
	};
	union                                 // right (maximal) x
	{
		TYPE right;
		TYPE x2;
		TYPE maxx;
	};
	union                                 // bottom (maximal) y
	{
		TYPE bottom;
		TYPE y2;
		TYPE maxy;
	};
public :
	CTRect() {}
	CTRect( TYPE _minx, TYPE _miny, TYPE _maxx, TYPE _maxy ): minx(_minx), miny(_miny), maxx(_maxx), maxy(_maxy) {}
	TYPE Width() const { return ( maxx - minx ); }
	TYPE Height() const { return ( maxy - miny ); }
	const CTPoint<TYPE> TopLeft() const { return CTPoint<TYPE>( x1, y1 ); }
	const CTPoint<TYPE> BottomRight() const { return CTPoint<TYPE>( x2, y2 ); }

	void SetRect( TYPE _x1, TYPE _y1, TYPE _x2, TYPE _y2 ) { x1 = _x1; y1 = _y1; x2 = _x2; y2 = _y2; }
	void SetRectEmpty() { minx = miny = maxx = maxy = 0; }
	bool IsRectEmpty() const { return ( (minx == maxx) && (miny == maxy) ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct STriangle
{
	WORD i1, i2, i3;
	//
	STriangle() {}
	STriangle( WORD _i1, WORD _i2, WORD _i3 ): i1(_i1), i2(_i2), i3(_i3) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRay
{
public:
	CVec3 ptOrigin, ptDir;
	//
	CVec3 Get( float fT ) const { return ptOrigin + ptDir * fT; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSphere
{
	CVec3 ptCenter;
	float fRadius;

	SSphere() {}
	SSphere( const CVec3 &_ptCenter, float _fRadius ): ptCenter(_ptCenter), fRadius(_fRadius) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMassSphere
{
	CVec3 ptCenter;
	float fRadius;
	float fMass;

	SMassSphere() {}
	SMassSphere( const CVec3 &_ptCenter, float _fRadius, float _fMass ):
		ptCenter(_ptCenter), fRadius(_fRadius), fMass(_fMass) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SBound
{
	SSphere s;
	CVec3 ptHalfBox;
	
	void BoxInit( const CVec3 &ptMin, const CVec3 &ptMax )
	{
		ptHalfBox = ( ptMax - ptMin ) * 0.5f;
		s.ptCenter = ( ptMax + ptMin ) * 0.5f;
		s.fRadius = fabs( ptHalfBox );
	}
	void BoxExInit( const CVec3 &_ptCenter, const CVec3 &_ptHalfBox )
	{
		s.ptCenter = _ptCenter;
		s.fRadius = fabs( _ptHalfBox );
		ptHalfBox = _ptHalfBox;
	}
	void SphereInit( const CVec3 &_ptCenter, float fRadius )
	{
		s.ptCenter = _ptCenter;
		s.fRadius = fRadius;
		ptHalfBox = CVec3( fRadius, fRadius, fRadius );
	}
	void Extend( float fAxisHalfSize )
	{
		ptHalfBox.x += fAxisHalfSize; ptHalfBox.y += fAxisHalfSize; ptHalfBox.z += fAxisHalfSize;
		s.fRadius = fabs( ptHalfBox );
	}
	bool IsInside( const CVec3 &v )
	{
		CVec3 vTest( s.ptCenter - v );
		if ( fabs2( vTest ) > sqr( s.fRadius ) )
			return false;
		return fabs( vTest.x ) <= ptHalfBox.x && fabs( vTest.y ) < ptHalfBox.y && fabs( vTest.z ) < ptHalfBox.z;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool DoesIntersect( const SBound &a, const SBound &b )
{
	CVec3 ptDif = a.s.ptCenter - b.s.ptCenter;
	if ( fabs2(ptDif) > sqr( a.s.fRadius + b.s.fRadius ) )
		return false;
	if ( 
		fabs(ptDif.x) > a.ptHalfBox.x + b.ptHalfBox.x || 
		fabs(ptDif.y) > a.ptHalfBox.y + b.ptHalfBox.y || 
		fabs(ptDif.z) > a.ptHalfBox.z + b.ptHalfBox.z )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma pack()
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **                                  struct SPlane functions
// ************************************************************************************************************************ //
//      pt2
//     /
//  pt0
//     \
//      pt1
inline bool SPlane::Set( const CVec3 &pt0, const CVec3 &pt1, const CVec3 &pt2 )
{
  CVec3 v1( pt1.x - pt0.x, pt1.y - pt0.y, pt1.z - pt0.z ), v2( pt2.x - pt0.x, pt2.y - pt0.y, pt2.z - pt0.z );
	// calc normale
	n = v1 ^ v2;
  if ( !Normalize( &n ) )
		return false;
	// calc distance coeff
	d = -( pt0 * n );
	return true;
}
inline bool SPlane::Set( float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2 )
{
  CVec3 pt0( x0, y0, z0 ), v1( x1 - x0, y1 - y0, z1 - z0 ), v2( x2 - x0, y2 - y0, z2 - z0 );
  // calc normale
	n = v1 ^ v2;
  if ( !Normalize( &n ) )
		return false;
	// calc distance coeff
	d = -( pt0 * n );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// протестировать, не лежит ли точка под плоскостью. 
// вернуть 0x80000000 если это так или 0 в противном случае
inline DWORD SPlane::CheckPointUnderPlane( const CVec3 &pt ) const
{
  float fDist = n*pt + d;
  return ( bit_cast<DWORD>(fDist) & 0x80000000 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SHMatrix
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void Identity( SHMatrix *pRes )
{
	MemSetDWord( reinterpret_cast<DWORD*>(pRes), 0, 16 );
	pRes->_11 = pRes->_22 = pRes->_33 = pRes->_44 = 1.0f;
}
// allow &m == p
inline void Transpose( SHMatrix *p, const SHMatrix &m )
{
	float t;
	p->_11 = m._11; p->_22 = m._22; p->_33 = m._33; p->_44 = m._44;
	t = m._12; p->_12 = m._21; p->_21 = t; 
	t = m._13; p->_13 = m._31; p->_31 = t; 
	t = m._14; p->_14 = m._41; p->_41 = t; 
	t = m._23; p->_23 = m._32; p->_32 = t; 
	t = m._24; p->_24 = m._42; p->_42 = t; 
	t = m._34; p->_34 = m._43; p->_43 = t; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void Multiply( SHMatrix *p, const SHMatrix &a, const SHMatrix &b )
{
	p->_11 = a._11*b._11 + a._12*b._21 + a._13*b._31 + a._14*b._41;
	p->_12 = a._11*b._12 + a._12*b._22 + a._13*b._32 + a._14*b._42;
	p->_13 = a._11*b._13 + a._12*b._23 + a._13*b._33 + a._14*b._43;
	p->_14 = a._11*b._14 + a._12*b._24 + a._13*b._34 + a._14*b._44;

	p->_21 = a._21*b._11 + a._22*b._21 + a._23*b._31 + a._24*b._41;
	p->_22 = a._21*b._12 + a._22*b._22 + a._23*b._32 + a._24*b._42;
	p->_23 = a._21*b._13 + a._22*b._23 + a._23*b._33 + a._24*b._43;
	p->_24 = a._21*b._14 + a._22*b._24 + a._23*b._34 + a._24*b._44;

	p->_31 = a._31*b._11 + a._32*b._21 + a._33*b._31 + a._34*b._41;
	p->_32 = a._31*b._12 + a._32*b._22 + a._33*b._32 + a._34*b._42;
	p->_33 = a._31*b._13 + a._32*b._23 + a._33*b._33 + a._34*b._43;
	p->_34 = a._31*b._14 + a._32*b._24 + a._33*b._34 + a._34*b._44;

	p->_41 = a._41*b._11 + a._42*b._21 + a._43*b._31 + a._44*b._41;
	p->_42 = a._41*b._12 + a._42*b._22 + a._43*b._32 + a._44*b._42;
	p->_43 = a._41*b._13 + a._42*b._23 + a._43*b._33 + a._44*b._43;
	p->_44 = a._41*b._14 + a._42*b._24 + a._43*b._34 + a._44*b._44;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline const SHMatrix operator*( const SHMatrix &a, const SHMatrix &b )
{
  SHMatrix ret;
  Multiply( &ret, a, b );
  return ret;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline SFBTransform operator*( const SFBTransform &a, const SFBTransform &b ) 
{ 
	SFBTransform res;
	res.forward = a.forward * b.forward;
	res.backward = b.backward * a.backward;
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void SHMatrix::RotateVector( CVec3 *pResult, const CVec3 &pt ) const
{
	float x = pt.x, y = pt.y, z = pt.z;
	pResult->x = _11*x + _12*y + _13*z;
	pResult->y = _21*x + _22*y + _23*z;
	pResult->z = _31*x + _32*y + _33*z;
}
inline void SHMatrix::RotateVectorTransposed( CVec3 *pResult, const CVec3 &pt ) const
{
	float x = pt.x, y = pt.y, z = pt.z;
	pResult->x = _11*x + _21*y + _31*z;
	pResult->y = _12*x + _22*y + _32*z;
	pResult->z = _13*x + _23*y + _33*z;
}
inline void SHMatrix::RotateHDirection( CVec4 *pResult, const CVec3 &pt ) const
{
	float x = pt.x, y = pt.y, z = pt.z;
	pResult->x = _11*x + _12*y + _13*z;
	pResult->y = _21*x + _22*y + _23*z;
	pResult->z = _31*x + _32*y + _33*z;
	pResult->w = _41*x + _42*y + _43*z;
}
inline void SHMatrix::RotateHVector( CVec3 *pResult, const CVec3 &pt ) const
{
	float x = pt.x, y = pt.y, z = pt.z;
	pResult->x = _11*x + _12*y + _13*z + _14;
	pResult->y = _21*x + _22*y + _23*z + _24;
	pResult->z = _31*x + _32*y + _33*z + _34;
}
inline void SHMatrix::RotateHVector( CVec4 *pResult, const CVec3 &pt ) const
{
	float x = pt.x, y = pt.y, z = pt.z;
	pResult->x = _11*x + _12*y + _13*z + _14;
	pResult->y = _21*x + _22*y + _23*z + _24;
	pResult->z = _31*x + _32*y + _33*z + _34;
	pResult->w = _41*x + _42*y + _43*z + _44;
}
inline void SHMatrix::RotateHVector( CVec4 *pResult, const CVec4 &pt ) const
{
	float x = pt.x, y = pt.y, z = pt.z, w = pt.w;
	pResult->x = _11*x + _12*y + _13*z + _14*w;
	pResult->y = _21*x + _22*y + _23*z + _24*w;
	pResult->z = _31*x + _32*y + _33*z + _34*w;
	pResult->w = _41*x + _42*y + _43*z + _44*w;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void SHMatrix::RotateHVectorTransposed( CVec4 *pResult, const CVec4 &pt ) const
{
	float x = pt.x, y = pt.y, z = pt.z, w = pt.w;
	pResult->x = _11*x + _21*y + _31*z + _41*w;
	pResult->y = _12*x + _22*y + _32*z + _42*w;
	pResult->z = _13*x + _23*y + _33*z + _43*w;
	pResult->w = _14*x + _24*y + _34*z + _44*w;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool SHMatrix::HomogeneousInverse( const SHMatrix &m )
{
	float det =	m._11*(m._22*m._33 - m._23*m._32) +
		          m._21*(m._13*m._32 - m._12*m._33) +
							m._31*(m._12*m._23 - m._13*m._22);
	if ( det == 0 )
		return false;                       // singular matrix found !
	det = 1.0f/det;
	// calculate reverse rotation
	_11 = ( m._22*m._33 - m._23*m._32 ) * det;
	_12 = ( m._13*m._32 - m._12*m._33 ) * det;
	_13 = ( m._12*m._23 - m._13*m._22 ) * det;
	_14 = -( m._14*_11 + m._24*_12 + m._34*_13 );
	//
	_21 = ( m._23*m._31 - m._21*m._33 ) * det;
	_22 = ( m._11*m._33 - m._13*m._31 ) * det;
	_23 = ( m._13*m._21 - m._11*m._23 ) * det;
	_24 = -( m._14*_21 + m._24*_22 + m._34*_23 );
	//
	_31 = ( m._21*m._32 - m._22*m._31 ) * det;
	_32 = ( m._12*m._31 - m._11*m._32 ) * det;
	_33 = ( m._11*m._22 - m._12*m._21 ) * det;
	_34 = -( m._14*_31 + m._24*_32 + m._34*_33 );
	//
	_41 = _42 = _43 = 0.0f;
	_44 = 1.0f;
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CQuat
////////////////////////////////////////////////////////////////////////////////////////////////////
const float FP_QUAT_EPSILON = 1e-04f;  // cutoff for sin(angle) near zero
////////////////////////////////////////////////////////////////////////////////////////////////////
// create quaternion from rotation axis and angle
inline void CQuat::FromAngleAxis( float fAngle, const CVec3 &ptAxis, const bool bNormalizeAxis )
{
  fAngle *= 0.5f;
  const float fSinAlpha = bNormalizeAxis ? sin( fAngle ) / fabs( ptAxis ) : sin( fAngle );
  x = ptAxis.x * fSinAlpha;
  y = ptAxis.y * fSinAlpha;
  z = ptAxis.z * fSinAlpha;
  w = cos( fAngle );
}
/*inline void CQuat::FromAngleAxis( float fAngle, float fAxisX, float fAxisY, float fAxisZ, const bool bNormalizeAxis )
{
  fAngle *= 0.5f;
  const float fSinAlpha = bNormalizeAxis ? sin( fAngle ) / fabs( fAxisX, fAxisY, fAxisZ ) : sin( fAngle );
  x = fAxisX * fSinAlpha;
  y = fAxisY * fSinAlpha;
  z = fAxisZ * fSinAlpha;
  w = cos( fAngle );
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
// create quaternion from Euler matrix
inline void CQuat::FromEulerMatrix( const SHMatrix &m )
{
  float tmp;
  // first compute squared magnitudes of quaternion components - at least one
  // will be greater than 0 since quaternion is unit magnitude
  float qs2 = 0.25f * (m.xx + m.yy + m.zz + 1);
  float qx2 = qs2 - 0.5f * (m.yy + m.zz);
  float qy2 = qs2 - 0.5f * (m.zz + m.xx);
  float qz2 = qs2 - 0.5f * (m.xx + m.yy);
  // find maximum magnitude component
  int n = (qs2 > qx2 ) ? 
                        ((qs2 > qy2) ? ((qs2 > qz2) ? 0 : 3) : ((qy2 > qz2) ? 2 : 3)) :
                        ((qx2 > qy2) ? ((qx2 > qz2) ? 1 : 3) : ((qy2 > qz2) ? 2 : 3));
  // compute signed quaternion components using numerically stable method
  switch ( n ) 
  {
    case 0:
      w = static_cast<float>( sqrt( qs2 ) );
      tmp = 0.25f / w;
      x = ( m.zy - m.yz ) * tmp;
      y = ( m.xz - m.zx ) * tmp;
      z = ( m.yx - m.xy ) * tmp;
      break;
    case 1:
      x = static_cast<float>( sqrt( qx2 ) );
      tmp = 0.25f / x;
      w = ( m.zy - m.yz ) * tmp;
      y = ( m.xy + m.yx ) * tmp;
      z = ( m.xz + m.zx ) * tmp;
      break;
    case 2:
      y = static_cast<float>( sqrt( qy2 ) );
      tmp = 0.25f / y;
      w = ( m.xz - m.zx ) * tmp;
      z = ( m.yz + m.zy ) * tmp;
      x = ( m.yx + m.xy ) * tmp;
      break;
    case 3:
      z = static_cast<float>( sqrt( qz2 ) );
      tmp = 0.25f / z;
      w = ( m.yx - m.xy ) * tmp;
      x = ( m.zx + m.xz ) * tmp;
      y = ( m.zy + m.yz ) * tmp;
      break;
  }
  // for consistency, force positive scalar component [ (s; v) = (-s; -v) ]
  MinimizeRotationAngle();
  // normalize 
  Normalize( x, y, z, w );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//  converts 3 euler angles (in radians) to a quaternion
//  Assumes roll is rotation about X, pitch is rotation about Y, yaw is about Z.  
//  Assumes order of yaw, pitch, roll applied as follows:
//            p' = roll( pitch( yaw(p) ) )
inline void CQuat::FromEulerAngles( float yaw, float pitch, float roll )
{
  float fHalfYaw = yaw * 0.5f;
  float fHalfPitch = pitch * 0.5f;
  float fHalfRoll = roll * 0.5f;

  float fCosYaw = cos( fHalfYaw );
  float fSinYaw = sin( fHalfYaw );
  float fCosPitch = cos( fHalfPitch );
  float fSinPitch = sin( fHalfPitch );
  float fCosRoll = cos( fHalfRoll );
  float fSinRoll = sin( fHalfRoll );

  x = fSinRoll * fCosPitch * fCosYaw - fCosRoll * fSinPitch * fSinYaw;
  y = fCosRoll * fSinPitch * fCosYaw + fSinRoll * fCosPitch * fSinYaw;
  z = fCosRoll * fCosPitch * fSinYaw - fSinRoll * fSinPitch * fCosYaw;
  w = fCosRoll * fCosPitch * fCosYaw + fSinRoll * fSinPitch * fSinYaw;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*inline CQuat::CQuat( float fAngle, float fAxisX, float fAxisY, float fAxisZ, const bool bNormalizeAxis )
{
	FromAngleAxis( fAngle, fAxisX, fAxisY, fAxisZ, bNormalizeAxis );
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CQuat::CQuat( float fAngle, const CVec3 &ptAxis, const bool bNormalizeAxis )
{
	FromAngleAxis( fAngle, ptAxis, bNormalizeAxis );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CQuat::Deriv( const CQuat &q, const CVec3 &v )
{
  x = 0.5f * (  q.w*v.x - q.z*v.y + q.y*v.z );
  y = 0.5f * (  q.z*v.x + q.w*v.y - q.x*v.z );
  z = 0.5f * ( -q.y*v.x + q.x*v.y + q.w*v.z );
  w = 0.5f * ( -q.x*v.x - q.y*v.y - q.z*v.z );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// rotations
inline const CVec3 CQuat::GetXAxis() const
{
	return CVec3( w*w - (x*x + y*y + z*z) + 2.0f*x*x, (z*w + x*y)*2.0f, (-y*w + x*z)*2.0f );
}
inline const CVec3 CQuat::GetYAxis() const
{
	return CVec3( (-z*w + y*x)*2.0f, w*w - (x*x + y*y + z*z) + 2.0f*y*y, (x*w + y*z)*2.0f );
}
inline const CVec3 CQuat::GetZAxis() const
{
	return CVec3( (y*w + z*x)*2.0f, (-x*w + z*y)*2.0f, w*w - (x*x + y*y + z*z) + 2.0f*z*z );
}
inline void CQuat::GetXAxis( CVec3 *pRes ) const
{
	pRes->x = w*w - (x*x + y*y + z*z) + 2.0f*x*x;
	pRes->y = (z*w + x*y)*2.0f;
	pRes->z = (-y*w + x*z)*2.0f;
}
inline void CQuat::GetYAxis( CVec3 *pRes ) const
{
	pRes->x = (-z*w + y*x)*2.0f;
	pRes->y = w*w - (x*x + y*y + z*z) + 2.0f*y*y;
	pRes->z = (x*w + y*z)*2.0f;
}
inline void CQuat::GetZAxis( CVec3 *pRes ) const
{
	pRes->x = (y*w + z*x)*2.0f;
	pRes->y = (-x*w + z*y)*2.0f;
	pRes->z = w*w - (x*x + y*y + z*z) + 2.0f*z*z;
}
inline const CVec3 CQuat::Rotate( const CVec3 &r ) const
{
	const CVec3 L( x, y, z );
	return ( r*(w*w - L*L) + (2.0f*w)*(L^r) + (2.0f*(L*r))*L );
}
inline void CQuat::Rotate( CVec3 *pRes, const CVec3 &vec ) const
{
	*pRes = ( vec*(w*w - n*n) + (2.0f*w)*(n^vec) + (2.0f*(n*vec))*n );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline const CQuat operator*( const CQuat &a, const CQuat &b )
{
	return CQuat( a.w*b.x + b.w*a.x + (a.y*b.z - a.z*b.y),
  							a.w*b.y + b.w*a.y + (a.z*b.x - a.x*b.z),
  							a.w*b.z + b.w*a.z + (a.x*b.y - a.y*b.x), 
                a.w*b.w - (a.x*b.x + a.y*b.y + a.z*b.z) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// divide quaternion 'A' on quaternion 'B'
// stage 1: reverse axis for 'B' ( q1 = reversed 'B' )
// stage 2: return 'q1' * 'A'
inline const CQuat operator/( const CQuat &a, const CQuat &b )
{
	CQuat q1;
	q1.UnitInverse( b );
	return q1 * a;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// quaternion's multiplication with assignment (this = a*this)
inline CQuat& CQuat::operator*=( const CQuat &a )
{
  float xtmp = a.w*x + w*a.x + (a.y*z - a.z*y);
  float ytmp = a.w*y + w*a.y + (a.z*x - a.x*z);
  float ztmp = a.w*z + w*a.z + (a.x*y - a.y*x);
  float wtmp = a.w*w - ( a.x*x + a.y*y + a.z*z );
  x = xtmp; y = ytmp; z = ztmp; w = wtmp;  

  return *this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// divide 'this' quaternion on the 'q'
// stage 1: reverse axis for 'q' ( q1 = reversed 'q' )
// stage 2: 'this' = 'q1' * 'this'
inline CQuat& CQuat::operator/=( const CQuat &q )
{
	CQuat q1;
	q1.UnitInverse( q );
	(*this) *= q1;
  return *this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// quaternion's multiplication
// multiply quaternion by fp value (const function)
inline const CQuat CQuat::operator*( const float c ) const
{
  return CQuat( c*x, c*y, c*z, c*w );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// inverse quaternion
inline bool CQuat::Inverse( const CQuat &q )
{
  float norm = fabs2( q.x, q.y, q.z, q.w );
  if ( norm > FP_EPSILON2 )
  {
    norm = 1.0f / static_cast<float>( sqrt( norm ) );
    x = -q.x * norm;
    y = -q.y * norm;
    z = -q.z * norm;
    w =  q.w * norm;
    return true;
  }
  else
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// inverse quaternion
inline bool CQuat::Inverse()
{
  float norm = fabs2( x, y, z, w );
  if ( norm > FP_EPSILON2 )
  {
    norm = 1.0f / static_cast<float>( sqrt( norm ) );
    x *= -norm;
    y *= -norm;
    z *= -norm;
    w *=  norm;
    return true;
  }
  else
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// decomposing quaternion to axis and angle
inline void CQuat::DecompAngleAxis( float *pfAngle, CVec3 *pptAxis ) const
{
	// The quaternion representing the rotation is
	//   q = cos(A/2)+sin(A/2)*(x*i+y*j+z*k)
  float length2 = x*x + y*y + z*z;
  if ( length2 > 1e-8f )
  {
    *pfAngle = 2.0f * acos( w );
    float invlen = 1.0f / float( sqrt(length2) );
    pptAxis->x = x * invlen;
    pptAxis->y = y * invlen;
    pptAxis->z = z * invlen;
  }
  else
  {
    // angle is 0 (mod 2*pi), so any axis will do
    *pfAngle = 0.0f;
    pptAxis->x = 1.0f;
    pptAxis->y = 0.0f;
    pptAxis->z = 0.0f;
  }
}
inline void CQuat::DecompAngleAxis( float *pfAngle, float *pfAxisX, float *pfAxisY, float *pfAxisZ ) const
{
	// The quaternion representing the rotation is
	//   q = cos(A/2)+sin(A/2)*(x*i+y*j+z*k)
  float length2 = x*x + y*y + z*z;
  if ( length2 > 1e-8f )
  {
    *pfAngle = 2.0f * acos( w );
    float invlen = 1.0f / float( sqrt(length2) );
    *pfAxisX = x * invlen;
    *pfAxisY = y * invlen;
    *pfAxisZ = z * invlen;
  }
  else
  {
    // angle is 0 (mod 2*pi), so any axis will do
    *pfAngle = 0.0f;
    *pfAxisX = 1.0f;
    *pfAxisY = 0.0f;
    *pfAxisZ = 0.0f;
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// decompose quaternion to Euler matrix
inline void CQuat::DecompEulerMatrix( SHMatrix *pMatrix ) const
{
  float tx  = 2.0f*x;
  float ty  = 2.0f*y;
  float tz  = 2.0f*z;
  float twx = tx*w;
  float twy = ty*w;
  float twz = tz*w;
  float txx = tx*x;
  float txy = ty*x;
  float txz = tz*x;
  float tyy = ty*y;
  float tyz = tz*y;
  float tzz = tz*z;

	pMatrix->_11 = 1.0f - (tyy + tzz);
	pMatrix->_12 = txy - twz;
	pMatrix->_13 = txz + twy;

	pMatrix->_21 = txy + twz;
	pMatrix->_22 = 1.0f - (txx + tzz);
	pMatrix->_23 = tyz - twx;

	pMatrix->_31 = txz - twy;
	pMatrix->_32 = tyz + twx;
	pMatrix->_33 = 1.0f - (txx + tyy);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// decompose quaternion to reversed Euler matrix (reverse transform)
inline void CQuat::DecompReversedEulerMatrix( SHMatrix *pMatrix ) const
{
  float tx  = -2.0f*x;
  float ty  = -2.0f*y;
  float tz  = -2.0f*z;
  float twx =  tx*w;
  float twy =  ty*w;
  float twz =  tz*w;
  float txx = -tx*x;
  float txy = -ty*x;
  float txz = -tz*x;
  float tyy = -ty*y;
  float tyz = -tz*y;
  float tzz = -tz*z;

	pMatrix->_11 = 1.0f - (tyy + tzz);
	pMatrix->_12 = txy - twz;
	pMatrix->_13 = txz + twy;

	pMatrix->_21 = txy + twz;
	pMatrix->_22 = 1.0f - (txx + tzz);
	pMatrix->_23 = tyz - twx;

	pMatrix->_31 = txz - twy;
	pMatrix->_32 = tyz + twx;
	pMatrix->_33 = 1.0f - (txx + tyy);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// calculate exponent of the quaternion
inline const CQuat CQuat::Exp() const
{
  // If q = A*(x*i+y*j+z*k) where (x,y,z) is unit length, then
  // exp(q) = cos(A)+sin(A)*(x*i+y*j+z*k).  If sin(A) is near zero,
  // use exp(q) = cos(A)+A*(x*i+y*j+z*k) since A/sin(A) has limit 1.
  double angle = fabs( x, y, z );
  double sn = sin( angle );
  CQuat result;

  result.w = float( cos(angle) );

  if ( fabs(sn) >= FP_QUAT_EPSILON )
  {
    float coeff = float( sn / angle );
    result.x = coeff * x;
    result.y = coeff * y;
    result.z = coeff * z;
  }
  else
  {
    result.x = x;
    result.y = y;
    result.z = z;
  }

  return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// calculates natural logarithm of the quaternion
inline const CQuat CQuat::Log() const
{
  // If q = cos(A) + sin(A)*(x*i + y*j + z*k) where (x,y,z) is unit length, then
  // log(q) = A*(x*i + y*j + z*k).  If sin(A) is near zero, use log(q) =
  // sin(A)*(x*i + y*j + z*k) since sin(A)/A has limit 1.
  if ( fabs(w) < 1.0f )
  {
    double angle = acos( w );
    double sn = sin( angle );
    if ( fabs(sn) >= FP_QUAT_EPSILON )
    {
      float coeff = float( angle / sn );
      return CQuat( coeff * x, coeff * y, coeff * z, 0 );
    }
  }

  return CQuat( x, y, z, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Spherical Linear intERPolation between two quaternions (SLERP) (from 'p' to 'q' with coeff 'factor')
inline void CQuat::Slerp( const float factor, const CQuat &p, const CQuat &q )
{
	float scale0, scale1;
  CQuat q1( q );
	// use the dot product to get the cosine of the angle between the quaternions
	float cosom = p.x*q.x + p.y*q.y + p.z*q.z + p.w*q.w;
  // adjust signs (if necessary)
  if ( cosom < 0.0 )
	{
		cosom = -cosom;
		q1.Negate( q );
  }
  // calculate coefficients
  if ( (1.0 - cosom) > FP_QUAT_EPSILON )  // standard case (slerp)
	{
    float omega = acos( cosom );
    float sinom = static_cast<float>( 1.0/sin( omega ) );
    scale0 = float( sin((1.0 - factor) * omega) * sinom );
    scale1 = float( sin(factor * omega) * sinom );
  }
	else                                  // "p" and "q" quaternions are very close. so we can do a linear interpolation
	{
    scale0 = 1.0f - factor;
    scale1 = factor;
  }
  // calculate final values
  x = scale0*p.x + scale1*q1.x;
  y = scale0*p.y + scale1*q1.y;
  z = scale0*p.z + scale1*q1.z;
  w = scale0*p.w + scale1*q1.w;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void MakeMatrix( SHMatrix *pMatrix, const CVec3 &pos, const CQuat &rot )
{
	rot.DecompEulerMatrix( pMatrix );
	pMatrix->_14 = pos.x;
	pMatrix->_24 = pos.y;
	pMatrix->_34 = pos.z;
	pMatrix->_41 = pMatrix->_42 = pMatrix->_43 = 0.0f;
	pMatrix->_44 = 1.0f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void MakeMatrix( SHMatrix *pMatrix, const CVec3 &pos, const CQuat &rot, const CVec3 &scale )
{
	rot.DecompEulerMatrix( pMatrix );
	pMatrix->_11 *= scale.x;
	pMatrix->_21 *= scale.x;
	pMatrix->_31 *= scale.x;
	pMatrix->_12 *= scale.y;
	pMatrix->_22 *= scale.y;
	pMatrix->_32 *= scale.y;
	pMatrix->_13 *= scale.z;
	pMatrix->_23 *= scale.z;
	pMatrix->_33 *= scale.z;
	pMatrix->_14 = pos.x;
	pMatrix->_24 = pos.y;
	pMatrix->_34 = pos.z;
	pMatrix->_41 = pMatrix->_42 = pMatrix->_43 = 0.0f;
	pMatrix->_44 = 1.0f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
