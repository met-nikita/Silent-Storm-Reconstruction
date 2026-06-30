#include "StdAfx.h"
#include "SectorCtrl.h"
#include "..\Main\ChapterInfo.h"

const float REGION_SELECTION_ACCURACY = 0.2f;
const float REGION_SELECTION_ACCURACY2 = sqr( REGION_SELECTION_ACCURACY );
////////////////////////////////////////////////////////////////////////////////////////////////////
CSectorCtrl::CSectorCtrl( int _nMaxX, int _nMaxY, int _nSegmentLen, const vector<CVec2> &_sector )
	: nMaxX(_nMaxX), nMaxY(_nMaxY), nSegmentLen(_nSegmentLen), sector(_sector) 
{
	fHitAccuracy = REGION_SELECTION_ACCURACY2;
	eSectorType = RANDOM;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSectorCtrl::CreateSector( const CVec2 &pt, float fRadius, int nSegments, bool bUseRadius )
{
	sector.clear();
	float len = FP_2PI * fRadius;
	int nSegs = len / nSegmentLen;
	if ( nSegments != -1 )
	{
		nSegs = nSegments;
		if ( !bUseRadius )
		{
			len = nSegments * nSegmentLen;
			fRadius = len / FP_2PI;
		}
	}
	const float fStep = FP_2PI / nSegs;
	int i;
	float fAng;
	for ( i = 0, fAng = 0; i < nSegs; fAng += fStep, ++i )
	{
		CVec2 vec = pt + CVec2( cos( fAng ) * fRadius, sin( fAng ) * fRadius );

		ClampPoint( &vec );
		sector.push_back( vec );
	}
	Rebuild();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CSectorCtrl::ClampPoint( CVec2 *pVec )
{
	pVec->x = Min( pVec->x, (float)nMaxX );
	pVec->x = Max( pVec->x, 0.0f );
	pVec->y = Min( pVec->y, (float)nMaxY );
	pVec->y = Max( pVec->y, 0.0f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSectorCtrl::Rebuild()
{
	if ( sector.empty() || eSectorType != RANDOM )
		return false;
	const int nLen = sector.size();
	vector<CVec2> newContour;

	CVec2 ptLast = sector.back();
	float fLen = 0;
	int   i;
	
	for ( i = 0; i < nLen; ++i )
	{
		CVec2 vec = sector[i] - ptLast;
		fLen += fabs( vec );
		if ( fLen > 0.02f * nSegmentLen )
		{
			/*
			if ( fLen > 2.0f * SEGMENT_LEN )
			{
				const int nSegs = fLen / SEGMENT_LEN;
				vec /= nSegs;
				for ( int j = 1; j < nSegs; ++j )
				{
					ptLast += vec;
					newContour.push_back( ptLast );
				}
			}
			*/
			newContour.push_back( sector[i] );
			fLen = 0;
		}
		ptLast = sector[i];
	}
	bool bRet = sector.size() != newContour.size();
	sector = newContour;
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSectorCtrl::RegionHitTest( const CVec2 &pt )
{
	int nSize = eSectorType == RANDOM ? sector.size() : Min( 1, (int)sector.size() );
	for	 ( int	j = 0; j < nSize; ++j )
		if ( fabs2( pt - sector[j] ) < fHitAccuracy )
		{
			return j;
		}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSectorCtrl::MovePoint( int nID, const CVec2 &pt )
{
	if ( nID < 0 || nID >= sector.size() )
	{
		ASSERT(0);
		return false;
	}
	CVec2 vec = pt;
	ClampPoint( &vec );
	sector[nID] = vec;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
