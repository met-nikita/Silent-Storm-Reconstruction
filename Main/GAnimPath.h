#ifndef __GANIMPATH_H_
#define __GANIMPATH_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GAnimBase.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAnimation
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// path segment interpolation
class CPathInterpolator: public CObjectBase
{
	OBJECT_BASIC_METHODS(CPathInterpolator);
	struct SPathPoint
	{
		float fT;
		CVec3 ptPos;
		float fAngle; // direction angle
	};
	vector<SPathPoint> points;
	struct SInterpolate
	{
		int nIdx;
		float fAlpha;
	};

	void GetPoint( float fT, SInterpolate *pRes );
	float SmoothIteration();
	void CalcDirAndT();
	void Smooth();
public:
	CPathInterpolator() {}
	CPathInterpolator( const vector<CVec3> &pts );
	CPathInterpolator( const vector<CVec2> &pts );
	CPathInterpolator( CVec3 &pt, float fAngle );
	CPathInterpolator( const CPathInterpolator &main, int nP1, int nP2 );

	CVec3 GetPosition( float fT );
	CQuat GetRotation( float fT );
	float GetDirection( float fT );
	float GetDistance() { return points.back().fT; }
	int GetNumPoints() { return points.size(); }
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAFunctionPathDirection : public CAFunction
{
	OBJECT_BASIC_METHODS(CAFunctionPathDirection);
	STime tStart;
	STime tEnd;
	CPtr<CPathInterpolator> pPath;

public:
	CAFunctionPathDirection() : tStart(0), tEnd(0) {}
	CAFunctionPathDirection( STime t1, STime t2, CPathInterpolator *_pPath ) : tStart( t1 ), tEnd( t2 ), pPath(_pPath) {}
	virtual float GetValue( STime t );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAFunctionSum : public CAFunction
{
	OBJECT_BASIC_METHODS(CAFunctionSum);
	CPtr<CAFunction> pF1;
	CPtr<CAFunction> pF2;

public:
	CAFunctionSum() {}
	CAFunctionSum( CAFunction *_pF1, CAFunction *_pF2 ) : pF1( _pF1 ), pF2( _pF2 ) {}
	virtual float GetValue( STime t );
	int operator&( CStructureSaver &f ) { f.Add( 1, &pF1 ); f.Add( 2, &pF2 ); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStandAnimator : public CAnimator
{
	OBJECT_BASIC_METHODS(CStandAnimator);
	CVec3 pos;
	float fAngle;

public:
	CStandAnimator() {}
	CStandAnimator( STime _tStart, const CVec3 &_pos, float _fAngle ) : pos(_pos), fAngle(_fAngle) {}

	virtual bool NeedUpdate( STime t ) { return false; }
	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTrajectoryAnimator : public CAnimator
{
	OBJECT_BASIC_METHODS(CTrajectoryAnimator);
	STime tStart;
	float fTStart;
	float fVelocity;
	CPtr< CPathInterpolator > pPath;

public:
	CTrajectoryAnimator() {}
	CTrajectoryAnimator( STime _tStart, float _fVelocity, float _fTStart, CPathInterpolator *_pPath ) :
		tStart(_tStart), fVelocity(_fVelocity), fTStart(_fTStart), pPath(_pPath) {}

	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMoveAnimator : public CAnimator
{
	OBJECT_BASIC_METHODS(CMoveAnimator);
	STime tStart;
	CVec3 start;
	CVec3 vel;
	float fAStart;
	float fAVel;

public:
	CMoveAnimator() {}
	CMoveAnimator( STime _tStart, const CVec3 &_start, float _fAStart, const CVec3 &_vel, float _fAVel ) :
		tStart(_tStart), start(_start), vel(_vel), fAStart(_fAStart), fAVel(_fAVel) {}

	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInventoryAnimator : public CAnimator
{
	OBJECT_BASIC_METHODS(CInventoryAnimator);
	ZDATA_(CAnimator)
	CVec3 pos;
	CVec3 angle;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CAnimator*)this); f.Add(2,&pos); f.Add(3,&angle); return 0; }

public:
	CInventoryAnimator() {}
	CInventoryAnimator( STime _tStart, const CVec3 &_pos, const CVec3 &_angle ) : pos(_pos), angle(_angle) {}

	virtual bool NeedUpdate( STime t ) { return false; }
	virtual void GetFrame( STime t, SSkeletonPose *pPose );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif