#include "StdAfx.h"
#include "GAnimFormat.h"
#include "GAnimBase.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAnimation
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAnimator
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAnimator::NeedUpdate( STime t )
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimator::GetBoneFrame( STime t, SSkeletonPose *pPose, int nBone )
{
	ASSERT( pSkeleton );
	if ( !pSkeleton )
		return;
	if ( nBone >= pSkeleton->GetValue()->bones.size() )
		return;
	SBone &bone = pSkeleton->GetValue()->bones[nBone];
	(*pPose)[nBone].nParent = bone.nParent;
	(*pPose)[nBone].pos = bone.pos;
	(*pPose)[nBone].rot = bone.rot;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimator::GetFrame( STime t, SSkeletonPose *pPose )
{
	for ( int i = 0; i < pPose->size(); ++i )
		GetBoneFrame( t, pPose, i );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAnimator::operator&( CStructureSaver &f )
{
	f.Add( 1, &pSkeleton );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CASequence
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CASequence::NeedUpdate( STime t )
{
	// we assume that STimes are always increasing
	list< SASegment >::iterator it;
	for ( it = sequence.begin(); it != sequence.end(); ++it )
		if ( it->t > t )
			break;
	if ( it != sequence.begin() )
		sequence.erase( sequence.begin(), --it );
	if ( sequence.empty() )
	{
		if ( pLastAnim != 0 )
			return true;
		return false;
	}
	else
	{
		if ( pLastAnim == sequence.front().pAnim )
			return pLastAnim->NeedUpdate(t);
		return true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CASequence::GetFrame( STime t, SSkeletonPose *pPose )
{
	// we assume that STimes are always increasing
	list< SASegment >::iterator it;
	bool bCleared = false;
	while ( !bCleared )
	{
		bCleared = true;
		for ( it = sequence.begin(); it != sequence.end(); ++it )
		{
			if ( !IsValid( it->pAnim ) )
			{
				sequence.erase( it );
				bCleared = false;
				break;
			}
		}
	}
	for ( it = sequence.begin(); it != sequence.end(); ++it )
		if ( it->t > t )
			break;
	if ( it != sequence.begin() )
		sequence.erase( sequence.begin(), --it );
	if ( sequence.empty() )
	{
		pLastAnim = 0;
		CAnimator::GetFrame( t, pPose );
	}
	else
	{
		pLastAnim = sequence.front().pAnim;
		ASSERT( IsValid( pLastAnim ) );
		if ( !IsValid( pLastAnim ) )
		{
			pLastAnim = 0;
			CAnimator::GetFrame( t, pPose );
			return;
		}
		pLastAnim->GetFrame( t, pPose );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CASequence::Add( STime tStart, CAnimator *pAnimator )
{
	sequence.push_back( SASegment( tStart, pAnimator ) );
	pAnimator->pSkeleton = pSkeleton;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CASequence::Insert( STime tStart, CAnimator *pAnimator )
{
	list< SASegment >::iterator it;
	for ( it = sequence.begin(); it != sequence.end(); ++it )
		if ( it->t >= tStart )
		{
			sequence.erase( it, sequence.end() );
			break;
		}
	Add( tStart, pAnimator );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CASequence* CASequence::Apply( STime tStart, CAnimator *pAnimator )
{
	CASequence *pSeq = new CASequence();
	pSeq->sequence = sequence;
	Insert( tStart, pAnimator );
	return pSeq;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAnimator* CASequence::GetAnimator( STime t )
{
	if ( sequence.empty() )
		return 0;
	list< SASegment >::iterator it;
	for ( it = sequence.begin(); it != sequence.end(); ++it )
		if ( it->t > t )
			break;
	if ( it != sequence.begin() )
		--it;
	return it->pAnim;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CASequence::operator&( CStructureSaver &f )
{
	f.Add( 1, (CAnimator*)this );
	f.Add( 2, &sequence );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NAnimation;
REGISTER_SAVELOAD_CLASS( 0x01321154, CASequence );
