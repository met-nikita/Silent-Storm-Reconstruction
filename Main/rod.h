#ifndef __ROD_H_
#define __ROD_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBuilding
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CJunction;
class CBuildingSchema;
class CJunctionID
{
	int nID;
public:
	CJunctionID( int n = -1 ): nID(n) {}

	int operator=(int op) { nID = op; return nID;}
	operator int() const { return nID; }
};
typedef int CRodID;
typedef vector<CJunctionID> CJuncList;
enum EDirection;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct CIVec3
{
	int x, y, z;

  CIVec3() {  }
  CIVec3( const float ax, const float ay, const float az ) : x(Float2Int(ax)), y(Float2Int(ay)), z(Float2Int(az)) {}

	bool operator==( const CIVec3 &v ) const { return ( (v.x == x) && (v.y == y) && (v.z == z) ); }
	bool operator!=( const CIVec3 &v ) const { return ( (v.x != x) || (v.y != y) || (v.z != z) ); }
  CIVec3& operator+=( const CIVec3 &v ) { x += v.x; y += v.y; z += v.z; return *this; }
  CIVec3& operator-=( const CIVec3 &v ) { x -= v.x; y -= v.y; z -= v.z; return *this; }
  CIVec3& operator*=( const float d ) { x *= d; y *= d; z *= d; return *this; }
};
inline const CIVec3 operator-( const CIVec3 &a, const CIVec3 &b ) { return CIVec3( a.x - b.x, a.y - b.y, a.z - b.z ); }
inline const CIVec3 operator+( const CIVec3 &a, const CIVec3 &b ) { return CIVec3( a.x + b.x, a.y + b.y, a.z + b.z ); }
inline float fabs2( const CIVec3 &a ) { return a.x*a.x + a.y*a.y + a.z*a.z; }
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_RODWEIGHT = 1.0f;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ERodSide
{
	RS_LEFT = 0,
	RS_RIGHT = 1,
};
struct SPath;
class CRod;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Если стержень вертикальный, то левый край должен быть ниже правого
class CRod
{
	CBuildingSchema *const pSchema;
	union
	{
		int juncs[2]; // CJunctionID
		struct 
		{
			CJunctionID nLJunction;
			CJunctionID nRJunction;
		};
	};
	enum { NLOCKS = 2 };
	BYTE bLock;
	bool bVert;						// вертикальный стержень 
	bool bDestroy;

public:
	CRod( CBuildingSchema *pSchema, CJunctionID nLJ, CJunctionID nRJ );

	bool IsVert() const { return bVert; }
	CIVec3 GetLeftPt() const;
	CIVec3 GetRightPt() const;
	CJunctionID GetJunction( ERodSide side ) const;
	CJunctionID GetOppositeJunction( ERodSide side ) const;
	void AddWeight( float fWeight );

	bool Lock() { if ( bLock >= NLOCKS ) return false; return ++bLock; }
	void Unlock() { --bLock; ASSERT( bLock >= 0 ); }

	//EStability GetStability() const { return stability; }
	//EStability ComputeStability();
	//void SetStability( EStability s );
	bool Shoot( ERodSide from, EDirection dir );
	bool AdvanceWavefront( ERodSide side, const int nWaveID, EDirection from, int nStep, CJuncList *pWaveList );
	void Destroy();
	bool IsDestroyed() const { return bDestroy; }
	void Reset()
	{
		bLock = 0;
	}
	void operator=( const CRod &op )
	{
		memcpy( this, &op, sizeof( *this ) );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// возвращает узел со стороны side
inline CJunctionID CRod::GetJunction( ERodSide side ) const
{
	return juncs[side];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// возвращает узел противоположный стороне side
inline CJunctionID CRod::GetOppositeJunction( ERodSide side ) const
{
	return juncs[side^1];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __ROD_H__
