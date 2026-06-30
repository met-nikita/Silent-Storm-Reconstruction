#include "StdAfx.h"
#include "GAnimPath.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAnimation
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPathInterpolator
////////////////////////////////////////////////////////////////////////////////////////////////////
CPathInterpolator::CPathInterpolator( const vector<CVec3> &pts )
{
	points.resize( pts.size() );
	for ( int i = 0; i < points.size(); ++i )
		points[i].ptPos = pts[i];
	Smooth();
	CalcDirAndT();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPathInterpolator::CPathInterpolator( const vector<CVec2> &pts )
{
	points.resize( pts.size() );
	for ( int i = 0; i < points.size(); ++i )
		points[i].ptPos = CVec3( pts[i].x, pts[i].y, 0 );
	Smooth();
	CalcDirAndT();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPathInterpolator::CPathInterpolator( CVec3 &pt, float fAngle )
{
	points.resize( 1 );
	points[0].fT = 0;
	points[0].ptPos = pt;
	points[0].fAngle = fAngle;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPathInterpolator::CPathInterpolator( const CPathInterpolator &main, int nP1, int nP2 )
{
	ASSERT( (nP1 >= 0) | (nP1 < main.points.size()) | (nP2 >= 0) | (nP2 < main.points.size()) );
	float fT = main.points[nP1].fT;
	for ( int i=nP1; i<=nP2; ++i )
	{
		SPathPoint p = main.points[i];
		p.fT -= fT;
		points.push_back( p );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CPathInterpolator::SmoothIteration()
{
	vector<SPathPoint> newPoints;
	newPoints.reserve( points.size() * 2 + 1 );
	for ( int i = 0; i < points.size() - 1; ++i )
	{
		SPathPoint point;
		point.ptPos.x = (points[i].ptPos.x + points[i+1].ptPos.x) / 2;
		point.ptPos.y = (points[i].ptPos.y + points[i+1].ptPos.y) / 2;
		point.ptPos.z = (points[i].ptPos.z + points[i+1].ptPos.z) / 2;
		newPoints.push_back( points[i] );
		newPoints.push_back( point );
	}
	newPoints.push_back( points.back() );
	for ( int i = 1; i < points.size() - 1; ++i )
	{
		newPoints[i*2].ptPos.x = (4.0f * points[i].ptPos.x + points[i-1].ptPos.x + points[i+1].ptPos.x) / 6.0f;
		newPoints[i*2].ptPos.y = (4.0f * points[i].ptPos.y + points[i-1].ptPos.y + points[i+1].ptPos.y) / 6.0f;
		newPoints[i*2].ptPos.z = (4.0f * points[i].ptPos.z + points[i-1].ptPos.z + points[i+1].ptPos.z) / 6.0f;
	}
	points = newPoints;
	float fMax = 0;
	for ( int i = 0; i < points.size() - 1; ++i )
		fMax = Max( fMax, fabs( points[i+1].ptPos - points[i].ptPos ) );
	return fMax;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathInterpolator::CalcDirAndT()
{
	ASSERT( points.size() > 0 );
	if ( points.size() < 2 )
	{
		points[0].fT = 0;
		points[0].fAngle = 0;
		return;
	}
	float fT = 0;
	for ( int i = 0; i < points.size() - 1; ++i )
	{
		points[i].fT = fT;
		CVec3 ptDir = points[i+1].ptPos - points[i].ptPos;
		points[i].fAngle = atan2( ptDir.y, ptDir.x );
		fT += fabs(ptDir);
	}
	points.back().fT = fT;
	points.back().fAngle = points[ points.size() - 2 ].fAngle;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathInterpolator::Smooth()
{
	if ( points.size() < 2 )
		return;
	while ( SmoothIteration() > 0.05f ) ;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPathInterpolator::GetPoint( float fT, CPathInterpolator::SInterpolate *pRes )
{
	if ( fT < points[0].fT )
	{
		pRes->nIdx = 0;
		pRes->fAlpha = 0;
		return;
	}
	if ( fT >= points.back().fT )
	{
		pRes->nIdx = points.size() - 1;
		pRes->fAlpha = 0;
		return;
	}
	int i = 0;
	while ( i < points.size() - 1 && points[i + 1].fT < fT )
		++i;
	pRes->nIdx = i;
	pRes->fAlpha = ( fT - points[i].fT ) / ( points[i+1].fT - points[i].fT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CPathInterpolator::GetPosition( float fT )
{
	SInterpolate ip;
	GetPoint( fT, &ip );
	if ( ip.fAlpha != 0 )
		return points[ ip.nIdx ].ptPos * ( 1 - ip.fAlpha ) + points[ ip.nIdx + 1 ].ptPos * ip.fAlpha;
	return points[ ip.nIdx ].ptPos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CQuat CPathInterpolator::GetRotation( float fT )
{
	SInterpolate ip;
	GetPoint( fT, &ip );
	if ( ip.fAlpha != 0 )
	{
		CQuat q1( points[ ip.nIdx ].fAngle, CVec3(0,0,1) );
		CQuat q2( points[ ip.nIdx + 1 ].fAngle, CVec3(0,0,1) );
		CQuat q3;
		q3.Interpolate( q1, q2, ip.fAlpha );
		return q3;
	}
	return CQuat( points[ ip.nIdx ].fAngle, CVec3(0,0,1) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CPathInterpolator::GetDirection( float fT )
{
	CQuat q = GetRotation(fT);
	float fAngle;
	CVec3 axis;
	q.DecompAngleAxis( &fAngle, &axis );
	if ( axis.z < 0 )
		fAngle = -fAngle;
	return fAngle;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPathInterpolator::operator&( CStructureSaver &f )
{
	f.Add( 1, &points );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFunctionPathDirection
////////////////////////////////////////////////////////////////////////////////////////////////////
float CAFunctionPathDirection::GetValue( STime t )
{
	if ( t < tStart || tEnd == tStart )
		return pPath->GetDirection(0);
	float fDist = pPath->GetDistance();
	if ( t > tEnd )
		return pPath->GetDirection( fDist );
	float fT = fDist * (t - tStart) / (tEnd - tStart);
	return pPath->GetDirection( fT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAFunctionPathDirection::operator&( CStructureSaver &f )
{
	f.Add( 1, &tStart );
	f.Add( 2, &tEnd );
	f.Add( 3, &pPath );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFunctionSum
////////////////////////////////////////////////////////////////////////////////////////////////////
float CAFunctionSum::GetValue( STime t )
{
	float fV = pF1->GetValue(t) + pF2->GetValue(t);
	fV = SignumNormalizeAngleInRadian(fV);
	return fV;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStandAnimator
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStandAnimator::GetFrame( STime t, SSkeletonPose *pPose )
{
	(*pPose)[0].pos = pos;
	(*pPose)[0].rot = CQuat( fAngle, CVec3(0,0,1) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CStandAnimator::operator&( CStructureSaver &f )
{
	f.Add( 1, (CAnimator*)this );	
	f.Add( 2, &pos );
	f.Add( 3, &fAngle );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTrajectoryAnimator
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTrajectoryAnimator::GetFrame( STime t, SSkeletonPose *pPose )
{
	float fPlace = fTStart;
	if ( t > tStart )
		fPlace += fVelocity * (t - tStart) / 1000;
	(*pPose)[0].pos = pPath->GetPosition( fPlace );
	(*pPose)[0].rot = pPath->GetRotation( fPlace );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CTrajectoryAnimator::operator&( CStructureSaver &f )
{
	f.Add( 1, (CAnimator*)this );	
	f.Add( 2, &tStart );
	f.Add( 3, &fTStart );
	f.Add( 4, &fVelocity );
	f.Add( 5, &pPath );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMoveAnimator
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMoveAnimator::GetFrame( STime t, SSkeletonPose *pPose )
{
	STime tShift = t - tStart;
	float fTShift = tShift / 1000.f;
	(*pPose)[0].pos = start + vel * fTShift;
	(*pPose)[0].rot = CQuat( fAStart + fAVel * fTShift, CVec3(0,0,1) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMoveAnimator::operator&( CStructureSaver &f )
{
	f.Add( 1, (CAnimator*)this );	
	f.Add( 2, &tStart );
	f.Add( 3, &start );
	f.Add( 4, &vel );
	f.Add( 5, &fAStart );
	f.Add( 6, &fAVel );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInventoryAnimator
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInventoryAnimator::GetFrame( STime t, SSkeletonPose *pPose )
{
	(*pPose)[0].pos = pos;
	(*pPose)[0].rot = CQuat( angle.x, CVec3(1,0,0) ) * CQuat( angle.y, CVec3(0,1,0) ) * CQuat( angle.z, CVec3(0,0,1) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NAnimation;
REGISTER_SAVELOAD_CLASS( 0x01041130, CPathInterpolator );
REGISTER_SAVELOAD_CLASS( 0x12041181, CTrajectoryAnimator );
REGISTER_SAVELOAD_CLASS( 0x12041188, CMoveAnimator );
REGISTER_SAVELOAD_CLASS( 0x12041189, CStandAnimator );
REGISTER_SAVELOAD_CLASS( 0x11871141, CAFunctionSum );
REGISTER_SAVELOAD_CLASS( 0x11871142, CAFunctionPathDirection );
REGISTER_SAVELOAD_CLASS( 0x10661180, CInventoryAnimator );
