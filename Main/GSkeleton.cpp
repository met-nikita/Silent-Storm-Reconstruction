#include "StdAfx.h"
#include "GSkeleton.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAnimation
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// SBonePose
////////////////////////////////////////////////////////////////////////////////////////////////////
void SBonePose::UseParent( const SBonePose &pose )
{
	pos = pose.rot.Rotate(pos) + pose.pos;
	rot = pose.rot * rot;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SBonePose::MakeGlobal( const SSkeletonPose &pose )
{
	if ( nParent == -1 )
		return;
	SBonePose parent = pose[ nParent ];
	parent.MakeGlobal( pose );
	nParent = -1;
	pos = parent.rot.Rotate(pos) + parent.pos;
	rot = parent.rot * rot;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SBonePose::MakeLocal( const SSkeletonPose &pose, int nNewParent )
{
	if ( nParent == nNewParent )
		return;
	MakeGlobal( pose );
	SBonePose parent = pose[ nNewParent ];
	parent.MakeGlobal( pose );
	nParent = nNewParent;
	parent.rot.UnitInverse();
	pos = parent.rot.Rotate(pos - parent.pos);
	rot = parent.rot * rot;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SBonePose::Interpolate( const SSkeletonPose &pose, float val, const SBonePose &bone1, const SBonePose &bone2 )
{
	SBonePose b1 = bone1, b2 = bone2;
	if ( bone1.nParent == bone2.nParent )
		nParent = bone1.nParent;
	else
	{
		nParent = -1;
		b1.MakeGlobal( pose );
		b2.MakeGlobal( pose );
	}
	pos.Interpolate( b1.pos, b2.pos, val );
	rot.Interpolate( b1.rot, b2.rot, val );
	scale.Interpolate( b1.scale, b2.scale, val );
	MakeLocal( pose, bone1.nParent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
