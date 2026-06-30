#ifndef __GANIMBASE_H_
#define __GANIMBASE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "GSkeleton.h"
#include "Time.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAnimation
{
class CFileSkeletonInfo;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAFunction : public CObjectBase
{
public:
	virtual float GetValue( STime t ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAnimator : public CObjectBase
{
public:
	CDGPtr< CPtrFuncBase<CFileSkeletonInfo> > pSkeleton;

	virtual bool NeedUpdate( STime t );
	virtual void GetFrame( STime t, SSkeletonPose *pPose );
	virtual void GetBoneFrame( STime t, SSkeletonPose *pPose, int nBone );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SASegment
{
	STime t;
	CPtr< CAnimator > pAnim;

	SASegment( STime _t = 0, CAnimator *_pAnim = 0 ) : t( _t ), pAnim( _pAnim ) {}
	int operator&( CStructureSaver &f )
	{
		f.Add( 1, &t );
		f.Add( 2, &pAnim );
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CASequence : public CAnimator
{
	OBJECT_BASIC_METHODS(CASequence);
	CPtr< CAnimator > pLastAnim;
	list< SASegment > sequence;
public:

	CASequence() {}
	CASequence( STime t, CAnimator *pAnimator ) { sequence.push_back( SASegment( t, pAnimator ) ); }

	virtual bool NeedUpdate( STime t );
	virtual void GetFrame( STime t, SSkeletonPose *pPose );

	// simple - insert to back
	void Add( STime tStart, CAnimator *pAnimator );
	// return subsequence of segments which were in sequence before
	// insert this animator instead
	CASequence* Apply( STime tStart, CAnimator *pAnimator );
	// same but deletes the subsequence
	void Insert( STime tStart, CAnimator *pAnimator );
	CAnimator* GetAnimator( STime t );

	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class ITerrainFunction : public CObjectBase
{
public:
	virtual float GetHeight( float fX, float fY ) = 0;
	virtual CVec3 GetNormal( float fX, float fY ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif