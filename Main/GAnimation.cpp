#include "StdAfx.h"
#include "..\DBFormat\DataAnimation.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\Misc\RandomGen.h"
#include "..\Misc\BasicShare.h"
#include "..\MiscDll\LogStream.h"
#include "GSceneUtils.h"
#include "GAnimFormat.h"
#include "GAnimParticles.h"
#include "GAnimTerrain.h"
#include "GAnimPath.h"
#include "GAnimation.h"
#include "aiMap.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAnimation
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static CBasicShare<int, CFileAnimation> shareAnimations(106);
CBasicShare<int, CFileSkeleton> shareSkeletons(104);
CBasicShare<int, CFileLocators> shareLocators(123);
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* PrecacheAnimation( NDb::CAnimation *_pA )
{
	if ( !_pA	 )
		return 0;
	return new NGScene::CResourcePrecache<CFileAnimation>( shareAnimations.Get( _pA->GetRecordID() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFunctionLinear
////////////////////////////////////////////////////////////////////////////////////////////////////
float CAFunctionLinear::GetValue( STime t )
{
	if ( t < tStart || tStart == tEnd )
		return fStartValue;
	if ( t > tEnd )
		return fEndValue;
	return fStartValue + (fEndValue - fStartValue) * (t - tStart) / (tEnd - tStart);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAFunctionLinear::operator&( CStructureSaver &f )
{
	f.Add( 1, &tStart );
	f.Add( 2, &tEnd );
	f.Add( 3, &fEndValue );
	f.Add( 4, &fStartValue );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFunctionSinus
////////////////////////////////////////////////////////////////////////////////////////////////////
float CAFunctionSinus::GetValue( STime t )
{
	if ( t < tStart || tStart == tEnd )
		return 0.0f;
	if ( t > tEnd )
		return 1.0f;
	return 0.5f * ( 1.f - cos( static_cast<float>(t - tStart) / (tEnd - tStart) * FP_PI ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAFunctionSinus::operator&( CStructureSaver &f )
{
	f.Add( 1, &tStart );
	f.Add( 2, &tEnd );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFunctionConstant
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAFunctionConstant::operator&( CStructureSaver &f )
{
	f.Add( 1, &fValue );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAFunctionParabola
////////////////////////////////////////////////////////////////////////////////////////////////////
float CAFunctionParabola::GetValue( STime t )
{
	if ( t < tStart || tStart == tEnd )
		return 0.0f;
	if ( t > tEnd )
		return 1.0f;
	float f = 1.0f - static_cast<float>(t - tStart) / (tEnd - tStart);
	return 1.0f - sqr(f) * f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAFunctionParabola::operator&( CStructureSaver &f )
{
	f.Add( 1, &tStart );
	f.Add( 2, &tEnd );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAnimation
////////////////////////////////////////////////////////////////////////////////////////////////////
CAnimation::CAnimation( NDb::CAnimation *_pA, CPtrFuncBase<CFileSkeletonInfo> *_pSkeleton, STime _tStart )
	: tStart( _tStart ), pA(_pA)
{
	pAnim = shareAnimations.Get( pA->GetRecordID() );
	pAnim.Refresh();
	CFileAnimationInfo *pData = pAnim->GetValue();

	pSkeleton = _pSkeleton;
	pSkeleton.Refresh();

	int nBones = pSkeleton->GetValue()->bones.size(); // bones in skeleton
	int nAnimBones = pData->hdr.nRoots + pData->hdr.nBones + pData->hdr.nAddBones;

	int i;
	for ( i=0; i<nBones; ++i )
		nBoneIndices.push_back( -1 );
	for ( i=0; i<nAnimBones; ++i )
	{
		unsigned int nIdx = pData->hdr.indices[i];
//		ASSERT( nIdx < nBoneIndices.size() );
		if ( nIdx < nBoneIndices.size() )
			nBoneIndices[ nIdx ] = i;
	}

	tLength = pData->hdr.fLength * 1000 / pData->hdr.fFrameRate;
	tLength /= pA->fSpeed;
	if ( pA->bReverse )
		pFunc = new CAFunctionLinear( tStart, tStart + tLength, 1, 0 );
	else
		pFunc = new CAFunctionLinear( tStart, tStart + tLength );

	bCycle = true;
	bMoveCycle = false;
	bRotateCycle = false;

	shift = VNULL3;

	if ( pData->hdr.nRoots == 0 || pData->hdr.bScale )
		return;

	start = pData->keysRoots[0].pos;
	CQuat qStart = pData->keysRoots[0].rot;
	float fLength = pData->hdr.fLength;
	int nLength = (int)fLength;
	if ( fLength - nLength != 0 )
		++nLength;
	end = pData->keysRoots[ nLength * pData->hdr.nRoots ].pos;
	CQuat qEnd = pData->keysRoots[ nLength * pData->hdr.nRoots ].rot;

	move = end - start;
	move.y = 0;
	move.z = 0;
	fDistance = fabs( move );
	if ( tLength == 0 )
		fVelocity = 0;
	else
		fVelocity = fDistance * 1000 / tLength;
	start.y = 0;
	start.z = 0;
	end.z = 0;

	qEnd.UnitInverse();
	rotate = qStart * qEnd;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const STime CAnimation::GetOriginalTime()
{
	pAnim.Refresh();
	CFileAnimationInfo *pData = pAnim->GetValue();
	return (STime)(pData->hdr.fLength * 1000 / pData->hdr.fFrameRate);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const STime CAnimation::GetTimeLabel1()
{
	float f = (float)pA->nTime1 / GetOriginalTime();
	if ( pA->bReverse )
		f = 1.0f - f;
	return tStart + (STime)(f * tLength);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const STime CAnimation::GetStepLabel1()
{
	if ( pA->nStepTime1 == 0 )
		return 0;
	float f = (float)pA->nStepTime1 / GetOriginalTime();
	if ( pA->bReverse )
		f = 1.0f - f;
	return tStart + (STime)(f * tLength);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const STime CAnimation::GetStepLabel2()
{
	if ( pA->nStepTime2 == 0 )
		return 0;
	float f = (float)pA->nStepTime2 / GetOriginalTime();
	if ( pA->bReverse )
		f = 1.0f - f;
	return tStart + (STime)(f * tLength);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float CAnimation::GetAngle() const
{
	return ToRadian( (float)pA->nAngle );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimation::SetInterval( STime _tStart, STime _tEnd )
{
	tStart = _tStart;
	tLength = _tEnd - _tStart;
	if ( pA->bReverse )
		pFunc = new CAFunctionLinear( tStart, tStart + tLength, 1, 0 );
	else
		pFunc = new CAFunctionLinear( tStart, tStart + tLength );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimation::GetBoneFrame( STime t, SSkeletonPose *pPose, int nBone )
{
	pAnim.Refresh();
	CFileAnimationInfo *pData = pAnim->GetValue();

	int i = -1;
	if ( nBone < nBoneIndices.size() )
		i = nBoneIndices[ nBone ];	
	if ( i == -1 )
	{
		CAnimator::GetBoneFrame( t, pPose, nBone );
		return;
	}
	STime tGlobal = t;
	if ( t < tStart )
	{
//		if ( bCycle && tLength != 0 )
//			t = tStart + tLength - (tStart - t) % tLength;
//		else
			t = tStart;
	}
	else if ( t > tStart + tLength )
	{
		if ( bCycle && tLength != 0 )
			t = tStart + (t - tStart) % tLength;
		else
			t = tStart + tLength;
	}

	float fLength = pData->hdr.fLength;
	int nLength = (int)fLength;
	float fPart = pFunc->GetValue(t);
	float val = fPart * fLength;
	int nKey = (int)val;
	val = val - nKey;

	int j = i;
	int nGroupBones;
	if ( i >= pData->hdr.nRoots + pData->hdr.nBones )
	{
		j = i - pData->hdr.nRoots - pData->hdr.nBones;
		nGroupBones = pData->hdr.nAddBones;
	}
	else if ( pData->hdr.bScale )
		nGroupBones = pData->hdr.nRoots + pData->hdr.nBones;
	else if ( i < pData->hdr.nRoots )
		nGroupBones = pData->hdr.nRoots;
	else
	{
		j = i - pData->hdr.nRoots;
		nGroupBones = pData->hdr.nBones;
	}

	int nKey1 = nKey * nGroupBones + j;
	int nKey2 = nKey1 + nGroupBones;
	
	if ( nKey == nLength )
	{
		fLength = fLength - nLength;
		if ( fLength != 0 )
			val = val / fLength;
		else
			nKey2 = nKey1;
	}

	SBonePose key1, key2;
	key2.scale = key1.scale = CVec3(1,1,1);
	key2.nParent = key1.nParent = pSkeleton->GetValue()->bones[nBone].nParent;
	if ( i >= pData->hdr.nRoots + pData->hdr.nBones )
	{
		key1.nParent = pData->keysAddBones[nKey1].nParent;
		key1.pos = pData->keysAddBones[nKey1].pos;
		key1.rot = pData->keysAddBones[nKey1].rot;
		key2.nParent = pData->keysAddBones[nKey2].nParent;
		key2.pos = pData->keysAddBones[nKey2].pos;
		key2.rot = pData->keysAddBones[nKey2].rot;
	}
	else if ( pData->hdr.bScale )
	{
		key1.pos = pData->keysMSR[nKey1].pos;
		key1.rot = pData->keysMSR[nKey1].rot;
		key1.scale = pData->keysMSR[nKey1].scale;
		key2.pos = pData->keysMSR[nKey2].pos;
		key2.rot = pData->keysMSR[nKey2].rot;
		key2.scale = pData->keysMSR[nKey2].scale;
	}
	else if ( i < pData->hdr.nRoots )
	{
		key1.pos = pData->keysRoots[nKey1].pos;
		key1.rot = pData->keysRoots[nKey1].rot;
		key2.pos = pData->keysRoots[nKey2].pos;
		key2.rot = pData->keysRoots[nKey2].rot;
	}
	else
	{
		key2.pos = key1.pos = pSkeleton->GetValue()->bones[nBone].pos;
		key1.rot = pData->keysBones[nKey1].rot;
		key2.rot = pData->keysBones[nKey2].rot;
	}

	SBonePose &bone = (*pPose)[nBone];
	bone.Interpolate( *pPose, val, key1, key2 );

	if ( nBone == 0 ) // root should be shifted
	{
		bone.pos -= shift;
		if ( bMoveCycle )
		{
			bone.pos -= start;
			bone.pos -= fPart * move;
		}
		if ( bRotateCycle )
		{
			CQuat q;
			q.Interpolate( QNULL, rotate, fPart );
			bone.pos = q.Rotate( bone.pos );
			bone.rot = q * bone.rot;
		}
		if ( IsValid( pPos ) )
		{
			SSkeletonPose root;
			root.resize(1);
			pPos->GetFrame( tGlobal, &root );
			bone.UseParent( root[0] );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimation::SetStand( STime t, const CVec2 &pos, float fAngle )
{
	CVec3 posG( pos.x, pos.y, 0 );
	pPos = new CStandAnimator( t, posG, fAngle );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimation::SetStand( STime t, const CVec3 &pos, float fAngle )
{
	pPos = new CStandAnimator( t, pos, fAngle );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimation::SetTrajectory( STime t, float fVelocity, float fTStart, CPathInterpolator *pPath )
{
	pPos = new CTrajectoryAnimator( t, fVelocity, fTStart, pPath );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimation::SetMove( STime t, const CVec3 &pos, float fAStart, const CVec3 &vel, float fAVel )
{
	pPos = new CMoveAnimator( t, pos, fAStart, vel, fAVel );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimation::SetInventory( STime t, const CVec3 &pos, const CVec3 &angle )
{
	pPos = new CInventoryAnimator( t, pos, angle );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAInterpolator
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAInterpolator::GetFrame( STime t, SSkeletonPose *pPose )
{
	if ( !pFunc )
	{
		pAnim1->GetFrame( t, pPose );
		return;
	}
	float val = pFunc->GetValue(t);
	if ( val <= 0.0f )
	{
		pAnim1->GetFrame( t, pPose );
		return;
	}
	if ( val >= 1.0f )
	{
		pAnim2->GetFrame( t, pPose );
		return;
	}
	SSkeletonPose pose1, pose2;
	pose1.resize( pPose->size() );
	pose2.resize( pPose->size() );
	pAnim1->GetFrame( t, &pose1 );
	pAnim2->GetFrame( t, &pose2 );
	for ( int i = 0; i < pPose->size(); ++i )
		(*pPose)[i].Interpolate( *pPose, val, pose1[i], pose2[i] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAInterpolator::operator&( CStructureSaver &f )
{
	f.Add( 1, (CAnimator*)this );
	f.Add( 2, &pFunc );
	f.Add( 3, &pAnim1 );
	f.Add( 4, &pAnim2 );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAAdder
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAAdder::GetFrame( STime t, SSkeletonPose *pPose )
{
	if ( !pFunc )
	{
		pAnim1->GetFrame( t, pPose );
		return;
	}
	float val = pFunc->GetValue(t);
	if ( val <= 0.0f )
	{
		pAnim1->GetFrame( t, pPose );
		return;
	}
	SSkeletonPose pose1, pose2;
	pose1.resize( pPose->size() );
	pose2.resize( pPose->size() );
	pAnim1->GetFrame( t, &pose1 );
	pAnim2->GetFrame( t, &pose2 );

	for ( int i = 0; i < pPose->size(); ++i )
	{
		pose2[i].pos += pose1[i].pos;
		pose2[i].rot = pose1[i].rot * pose2[i].rot;
		//pose2.rot = pose2.rot * pose1.rot; // another order
		pose2[i].nParent = pose1[i].nParent;
		pose2[i].scale = pose1[i].scale;
		(*pPose)[i].Interpolate( *pPose, val, pose1[i], pose2[i] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAAdder::operator&( CStructureSaver &f )
{
	f.Add( 1, (CAnimator*)this );
	f.Add( 2, &pFunc );
	f.Add( 3, &pAnim1 );
	f.Add( 4, &pAnim2 );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CASubtractor
////////////////////////////////////////////////////////////////////////////////////////////////////
void CASubtractor::GetFrame( STime t, SSkeletonPose *pPose )
{
	SSkeletonPose pose1, pose2;
	pose1.resize( pPose->size() );
	pose2.resize( pPose->size() );
	pAnim1->GetFrame( t, &pose1 );
	pAnim2->GetFrame( t, &pose2 );
	for ( int i = 0; i < pPose->size(); ++i )
	{
		ASSERT( pose1[i].nParent == pose2[i].nParent );
		(*pPose)[i].nParent = pose1[i].nParent;
		(*pPose)[i].pos = pose2[i].pos - pose1[i].pos;
		(*pPose)[i].rot = pose2[i].rot / pose1[i].rot;
		/* // another order
		CQuat q1;
		q1.UnitInverse( pose1.rot );
		pPose->rot = pose2.rot * q1;
		*/
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CASubtractor::operator&( CStructureSaver &f )
{
	f.Add( 1, (CAnimator*)this );
	f.Add( 2, &pAnim1 );
	f.Add( 3, &pAnim2 );
	f.Add( 4, &t1 );
	f.Add( 5, &t2 );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CATurnBody
////////////////////////////////////////////////////////////////////////////////////////////////////
void CATurnBody::GetFrame( STime t, SSkeletonPose *pPose )
{
	pAnim->GetFrame( t, pPose );
	float val = pFunc->GetValue(t);
//		if ( val <= 0 )
//			return;
	CQuat turn = CQuat( val * fAngle / 2, CVec3(0,0,1) );
	int index[2];
	index[0] = nBoneSpine;
	index[1] = nBoneChest;
	for ( int j = 0; j < 2; ++j )
	{
		int i = index[j];
		int nParent = (*pPose)[i].nParent;
		(*pPose)[i].MakeGlobal( *pPose );
		(*pPose)[i].rot = turn * (*pPose)[i].rot;
		(*pPose)[i].MakeLocal( *pPose, nParent );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CATurnBody::operator&( CStructureSaver &f )
{
	f.Add( 1, (CAnimator*)this );
	f.Add( 2, &pAnim );
	f.Add( 3, &pFunc );
	f.Add( 4, &fAngle );
	f.Add( 5, &nBoneSpine );
	f.Add( 6, &nBoneChest );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIK2Chain
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIK2Chain::GetFrame( STime t, SSkeletonPose *pPose )
{
	pInput->GetFrame( t, pPose );

	SSkeletonPose &pose = *pPose, target;
	target.resize(1);
	pTarget->GetFrame( t, &target );
	SBonePose eff = target[0];

	eff.MakeLocal( pose, nBone1 );

	SBonePose &bone1 = pose[nBone1], &bone2 = pose[nBone2], &bone3 = pose[nBone3];

	CVec3 vec2 = bone2.rot.Rotate( bone3.pos );
	CVec3 vec1 = bone2.pos;

	float fLen1 = fabs(vec1);
	ASSERT( fLen1 > FP_EPSILON );
	float fLen2 = fabs(vec2);
	ASSERT( fLen2 > FP_EPSILON );
	float fDist = fabs(eff.pos);

	float fElbowAngle = FP_PI;
	if ( fDist < fLen1 + fLen2 )
	{
		if ( fDist < fLen1 - fLen2 )
			fElbowAngle = 0;
		else
			fElbowAngle = acos( (fLen1 * fLen1 + fLen2 * fLen2 - fDist * fDist) * 0.5f / fLen1 / fLen2 );
	}

	CVec3 n1, n2, axis;
	float fAngle = 0;

	n1 = -vec1;
	n2 = vec2;
	Normalize(&n1);
	Normalize(&n2);
	axis = n1 ^ n2;
	if ( fabs(axis) > 1e-6f )
	{
		fAngle = acos( Clamp( n1 * n2, -1.0f, 1.0f ) );
		CQuat q2( fElbowAngle - fAngle, axis, true );
		bone2.rot = q2 * bone2.rot;
		vec2 = bone2.rot.Rotate( bone3.pos );
	}
	float fDiffAngle = fAngle - fElbowAngle;
	if ( fDiffAngle < 0 )
		fDiffAngle = 0;

	n1 = vec1 + vec2;
	n2 = eff.pos;
	Normalize(&n1);
	Normalize(&n2);
	axis = n1 ^ n2;
	if ( fabs(axis) > 1e-6f )
	{
		fAngle = acos( Clamp( n1 * n2, -1.0f, 1.0f ) );
		CQuat q1( fAngle, axis, true );
		bone1.rot = bone1.rot * q1;
	}

	bone3.MakeGlobal( pose );
	bone3.rot = target[0].rot;
	bone3.MakeLocal( pose, nBone2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIK2Chain::operator&( CStructureSaver &f )
{
	f.Add( 1, (CAnimator*)this );
	f.Add( 2, &nBone1 );
	f.Add( 3, &nBone2 );
	f.Add( 4, &nBone3 );
	f.Add( 5, &pInput );
	f.Add( 6, &pTarget );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAPoseMemorizer
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAPoseMemorizer::GetFrame( STime t, SSkeletonPose *pPose )
{
	if ( t >= tMemory && memory.empty() )
	{
		memory.resize( pPose->size() );
		pInput->GetFrame( tMemory, &memory );
	}
	if ( t < tActive )
		pInput->GetFrame( t, pPose );
	else
	{
		ASSERT( memory.size() == pPose->size() );
		*pPose = memory;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAPoseMemorizer::operator&( CStructureSaver &f )
{
	f.Add( 1, &tMemory );
	f.Add( 2, &tActive );
	f.Add( 3, &pInput );
	f.Add( 4, &memory );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CASmartAimer
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CAnimation* CASmartAimer::GetIdleAnimation( const SSkeletonState &state )
{
	bool bCustomAnimationIdle = ( state.cIdleBannedFlags & E_CUSTOM_ANIMATION_IDLE ) > 0;
	if ( bCustomAnimationIdle && IsValid( state.pIdleAnimation ) )
	{
		return state.pIdleAnimation;
	} 
	else if ( state.szParams.empty() )
	{
		bool bBreathOnlyIdle = ( state.cIdleBannedFlags & E_BREATH_ONLY_IDLE ) > 0;
		return pSkeleton->GetAnimation( NDb::CAnimation::IDLE, 
			state.nAnimFlagsPoseWeapon, 0, state.nAnimFlagsClassSex, state.pSide, bBreathOnlyIdle );
	}
	else
	{
		return pSkeleton->GetAnimation( NDb::CAnimation::HEAL, state.nAnimFlagsPoseWeapon, 0, state.nAnimFlagsClassSex, state.pSide );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CASmartAimer::NeedUpdate( STime t )
{
	bool bTime = pNextPoseTime.Refresh();
	if ( pNextPose.Refresh() )
	{
		if ( IsValid(pState) )
			pState.Refresh();
		frost = FS_NONE;
		return true;
	}
	if ( IsValid(pIdleAnim) )
	{
		if ( IsValid(pState) )
			pState.Refresh();
		return true;
	}
	if ( IsValid( pState ) )
	{
		pState.Refresh();
		const SSkeletonState &state = pState->GetValue();
		if ( !state.IsIdleBanned() )
		{
			if ( GetIdleAnimation( state ) )
				return true;
		}
	}
	if ( frost == FS_FROZEN )
		return false;
	if ( bTime )
		frost = FS_FREEZE;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CASmartAimer::GetFrame( STime t, SSkeletonPose *pPose )
{
	NeedUpdate(t);
	//ASSERT( frost != FS_FROZEN );
	//pNextPoseTime.Refresh();
	//pNextPose.Refresh();

	STime tNext = pNextPoseTime->GetValue();
	const SSkeletonPose &aim = pNextPose->GetValue();
	if ( IsValid( pState ) )
	{
		//pState.Refresh();
		const SSkeletonState &state = pState->GetValue();
		if ( !state.IsIdleBanned() )
		{
			if ( t >= tIdleEnd )
			{
				//if ( !pIdleAnim.IsValid() )
				//	tIdleStart = t;
				STime tFrom = tIdleEnd;
				pIdleAnim = 0;
				CPtr<CAnimation> pAnim;
				pAnim = pAnimator->CreateAnimation( GetIdleAnimation( state ), tFrom );
				if ( pAnim && pAnim->GetTime() )
				{
					//ASSERT( frost == FS_NONE );
					/// CRAP
					if ( pAnim->GetTime() )
					{
						if ( tIdleEnd != 0 && t - tIdleEnd > pAnim->GetTime() )
							tFrom = t - random.Get( pAnim->GetTime() );
					}
					pAnim->SetInterval( tFrom, tFrom + pAnim->GetTime() );
					pAnim->SetStand( tFrom, state.pos, state.fAngle );
					tIdleEnd = tFrom + pAnim->GetTime();
					if ( !state.pTerrain )
						pIdleAnim = pAnim;
					else if ( state.bCrawl )
					{
						NAnimation::CATerrainCrawl *pT = new NAnimation::CATerrainCrawl;
						pT->pSkeleton = pAnimator->pSkeleton;
						pT->pTerrain = state.pTerrain;
						pT->pInput = pAnim;
						pT->Init();
						pIdleAnim = pT;
					}
					else
					{
						NAnimation::CATerrain *pT = new NAnimation::CATerrain;
						pT->pSkeleton = pAnimator->pSkeleton;
						pT->pTerrain = state.pTerrain;
						pT->pInput = pAnim;
						pIdleAnim = pT;
					}
				}
			}
			if ( IsValid( pIdleAnim ) )//&& t >= tIdleStart )
			{
				pIdleAnim->GetFrame( t, pPose );
				return;
			}
		}
		else
		{
			//if ( pIdleAnim.IsValid() )
			//tIdleStart = t;
			tIdleEnd = t;
			pIdleAnim = 0;
		}
	}
	
	ASSERT( pPose->size() == aim.size() );
	if ( frost == FS_FREEZE )
	{
		// source is in steady state & no idle is possible - freeze aimer
		frost = FS_FROZEN;
		tCurrent = t;
		current = aim;
		*pPose = aim;
		return;
	}
	if ( tCurrent == 0 )
	{
		tCurrent = t;
		current = aim;
	}

	if ( t > tNext || tCurrent == tNext )
	{
		*pPose = aim;
		return;
	}
	if ( t < tCurrent )
	{
		*pPose = current;
		return;
	}
	float val = float(t - tCurrent) / (tNext - tCurrent);
	for ( int i = 0; i < pPose->size(); ++i )
		(*pPose)[i].Interpolate( *pPose, val, current[i], aim[i] );
	current = *pPose;
	tCurrent = t;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAPoseAimer
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAPoseAimer::GetFrame( STime t, SSkeletonPose *pPose )
{
	pNextPoseTime.Refresh();
	pNextPose.Refresh();

	STime tNext = pNextPoseTime->GetValue();
	const SSkeletonPose &aim = pNextPose->GetValue();

	ASSERT( pPose->size() == aim.size() );
/*	
	{ // CRAP for testing
		*pPose = aim;
		return;
	}
*/
	if ( tCurrent == 0 )
	{
		tCurrent = t;
		current = aim;
	}

	if ( t > tNext || tCurrent == tNext )
	{
		*pPose = aim;
		return;
	}
	if ( t < tCurrent )
	{
		*pPose = current;
		return;
	}
	float val = float(t - tCurrent) / (tNext - tCurrent);
	for ( int i = 0; i < pPose->size(); ++i )
		(*pPose)[i].Interpolate( *pPose, val, current[i], aim[i] );
	current = *pPose;
	tCurrent = t;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAPoseAimer::operator&( CStructureSaver &f )
{
	f.Add( 1, &tCurrent );
	f.Add( 2, &current );
	f.Add( 3, &pNextPose );
	f.Add( 4, &pNextPoseTime );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CARandom
////////////////////////////////////////////////////////////////////////////////////////////////////
CARandom::CARandom( int nBones )
{
	rotations.resize( nBones );
	for ( int i = 0; i < nBones; ++i )
	{
		if ( i < 4 )
		{
			rotations[i] = QNULL;
			continue;
		}
		CVec3 axis;
		axis.x = random.GetFloat(-1,1);
		axis.y = random.GetFloat(-1,1);
		axis.z = random.GetFloat(-1,1);
		rotations[i] = CQuat( random.GetFloat(-0.5f, 0.5f), axis, true );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CARandom::GetFrame( STime t, SSkeletonPose *pPose )
{
	for ( int i = 0; i < rotations.size(); ++i )
	{
		(*pPose)[i].pos = VNULL3;
		(*pPose)[i].rot = rotations[i];
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CABoneFilter
////////////////////////////////////////////////////////////////////////////////////////////////////
void CABoneFilter::GetFrame( STime t, SSkeletonPose *pPose )
{
	pSource.Refresh();
	const SSkeletonPose &aim = pSource->GetValue();

	if ( nIndex >= aim.size() )
		return;
	SBonePose bone = aim[ nIndex ]; 
	bone.MakeGlobal( aim );
	(*pPose)[0] = bone;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CABoneFilter::operator&( CStructureSaver &f )
{
	f.Add( 1, &nIndex );
	f.Add( 2, &pSource );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSkeletonAnimator
////////////////////////////////////////////////////////////////////////////////////////////////////
CSkeletonAnimator::CSkeletonAnimator( NDb::CSkeleton *_pSkeleton )
{
	if ( !_pSkeleton )
	{
		pSeq = new CASequence;
		nBones = 1;
		value.resize(1);
		return;
	}

	pSkeleton = shareSkeletons.Get( _pSkeleton->GetRecordID() );
	pSkeleton.Refresh();

	pSeq = new CASequence;
	pSeq->pSkeleton = pSkeleton;
	nBones = pSkeleton->GetValue()->bones.size();
	value.resize( nBones );

	bServer = true;
	bItem = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSkeletonAnimator::NeedUpdate()
{
	if ( !pTime.Refresh() )
		return false;
	if ( pSkeleton )
		pSkeleton.Refresh();
	if ( value.size() != nBones )
		return true;
	STime time = pTime->GetValue();
	bool bUpdate = pSeq->NeedUpdate( time );
	/*
	if ( bUpdate )
	{
		char buf[128];
		sprintf( buf, "Updated animation: %d <%x> - %s\n", time, this, bServer ? "Server" : "Client" );
		OutputDebugString( buf );
	}
	*/
	return bUpdate;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonAnimator::Recalc()
{
	STime time = pTime->GetValue();
	if ( value.size() != nBones )
		value.resize( nBones );
	pSeq->GetFrame( time, &value );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSkeletonAnimator::IsFreezed() const
{
	STime time = pTime->GetValue();
	return !pSeq->NeedUpdate( time );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const int CSkeletonAnimator::GetBoneIndex( const char *pszName )
{
	pSkeleton.Refresh();
	return pSkeleton->GetValue()->GetBoneIndex( pszName );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonAnimator::GetCurrentBonePos( int nIndex, SBonePose *pBone )
{
	ASSERT( nIndex >= 0 );
	*pBone = GetValue()[nIndex];
	csSystem << CC_WHITE << " Usable bone parent = " << pBone->nParent << endl;
	pBone->MakeGlobal( GetValue() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonAnimator::GetDefaultBonePos( int nIndex, SBonePose *pBone )
{
	ASSERT( nIndex >= 0 );
	SBone &bone = pSkeleton->GetValue()->bones[nIndex];
	pBone->pos = bone.pos;
	pBone->rot = bone.rot;
	pBone->nParent = -1;
	int nParent = bone.nParent;
	while ( nParent >= 0 )
	{
		const SBone &parent = pSkeleton->GetValue()->bones[nParent];
		pBone->pos = parent.rot.Rotate(pBone->pos) + parent.pos;
		pBone->rot = parent.rot * pBone->rot;
		nParent = parent.nParent;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAnimation* CSkeletonAnimator::CreateAnimation( NDb::CAnimation *pA, STime tStart, bool bCycle )
{
	if ( !pA )
		return 0;
	pSkeleton.Refresh();
	CAnimation *pAnim = new CAnimation( pA, pSkeleton, tStart );
	pAnim->SetCycle( bCycle );
	return pAnim;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonAnimator::AddAnimator( STime t, CAnimator *pAnim )
{
	if ( pAnim )
		pSeq->Insert( t, pAnim );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonAnimator::AddMemorizer( STime tActive )
{
	CPtr<CAPoseMemorizer> pMemory = new CAPoseMemorizer;
	CPtr<CAnimator> pInput = pSeq->Apply( tActive, pMemory );
	pMemory->pInput = pInput;
	pMemory->tMemory = tActive;
	pMemory->tActive = tActive;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonAnimator::AddTransit( STime tFrom, STime tTo, CAnimator *pAnim, ETransitType trType )
{
	CAInterpolator *pTransit = new CAInterpolator;
	pTransit->pAnim1 = pSeq->Apply( tFrom, pTransit );
	pTransit->pAnim2 = pAnim;
	switch ( trType )
	{
		case LINEAR:
			pTransit->pFunc = new CAFunctionLinear( tFrom, tTo );
			break;
		case SINUS:
			pTransit->pFunc = new CAFunctionSinus( tFrom, tTo );
			break;
		case PARABOLA:
			pTransit->pFunc = new CAFunctionParabola( tFrom, tTo );
			break;
	}
	pSeq->Add( tTo, pAnim );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonAnimator::AddSimpleIK( STime tFrom, const char *pszBoneName, CAnimator *pEffector )
{
	CPtr<CAnimator> pTemp(pEffector);
	pSkeleton.Refresh();
	int nBone1, nBone2, nBone3;
	nBone3 = pSkeleton->GetValue()->GetBoneIndex( pszBoneName );
	if ( nBone3 == -1 )
		return;
	nBone2 = pSkeleton->GetValue()->bones[ nBone3 ].nParent;
	if ( nBone2 == -1 )
		return;
	nBone1 = pSkeleton->GetValue()->bones[ nBone2 ].nParent;
	if ( nBone1 == -1 )
		return;
	CAIK2Chain *pIK = new CAIK2Chain;
	pIK->nBone1 = nBone1;
	pIK->nBone2 = nBone2;
	pIK->nBone3 = nBone3;
	pIK->pInput = pSeq->Apply( tFrom, pIK );
	pIK->pTarget = pEffector;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonAnimator::AddDynamics( STime tFrom, STime tMaxAnim, const CVec3 &impact, 
	NAI::IAIMap *pMap, NGScene::CCInt *pFloor, CAnimator *pEffector )
{
	if ( !IsValid( pEffector ) )
	{
		CParticleSkeleton *pPart = new CParticleSkeleton( pFloor, pSkeleton );
		pPart->impact = impact;
		pPart->pInput = pSeq->Apply( tFrom, pPart );
		pPart->tActive = tFrom;
		pPart->tMax = tMaxAnim;
		pPart->pMap = pMap;
	}
	else
	{
		CAInterpolator *pTransit = new CAInterpolator;
		CParticleSkeleton *pPart = new CParticleSkeleton( pFloor, pSkeleton );
		pPart->impact = impact;
		pPart->pInput = pSeq->Apply( tFrom, pTransit );
		pPart->tActive = tFrom;
		pPart->tMax = tMaxAnim;
		pPart->pMap = pMap;
		pPart->pEffector = pEffector;
		pTransit->pAnim1 = pPart->pInput;
		pTransit->pAnim2 = pPart;
		pTransit->pFunc = new CAFunctionLinear( tFrom, tFrom + 500 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonAnimator::AddAimer( STime t, CFuncBase<SSkeletonPose> *pNextPose, CFuncBase<STime> *pNextPoseTime )
{
	ASSERT( IsValid( pNextPose ) );
	ASSERT( IsValid( pNextPoseTime ) );
	CAPoseAimer *pAimer = new CAPoseAimer;
	pAimer->pNextPose = pNextPose;
	pAimer->pNextPoseTime = pNextPoseTime;
	pSeq->Insert( t, pAimer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonAnimator::AddSmartAimer( STime t, CFuncBase<SSkeletonPose> *pNextPose,
	CFuncBase<SSkeletonState> *pState, CFuncBase<STime> *pNextPoseTime,
	CSkeletonAnimator *pAnimator, NDb::CSkeleton *pSkeleton )
{
	ASSERT( IsValid( pNextPose ) );
	ASSERT( IsValid( pNextPoseTime ) );
	CASmartAimer *pAimer = new CASmartAimer;
	pAimer->pNextPose = pNextPose;
	pAimer->pNextPoseTime = pNextPoseTime;
	pAimer->pState = pState;
	pAimer->pAnimator = pAnimator;
	pAimer->pSkeleton = pSkeleton;
	pSeq->Insert( t, pAimer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonAnimator::AddBoneFilter( STime tFrom, CFuncBase<SSkeletonPose> *pSource, int nAddBone )
{
	CABoneFilter *pFilter = new CABoneFilter;
	pFilter->pSource = pSource;
	pFilter->nIndex = nAddBone;
	pSeq->Insert( tFrom, pFilter );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonAnimator::AddRandom( STime tFrom, STime tTo )
{
	//CPtr<CAnimator> pAfter = pSeq->GetAnimator( tTo );
	NAnimation::CARandom *pRandom = new CARandom( nBones );
	CPtr<NAnimation::CAAdder> pAdder = new NAnimation::CAAdder;
	pAdder->pAnim1 = pSeq->Apply( tFrom, pAdder );
	pAdder->pAnim2 = pRandom;
	pAdder->pFunc = new NAnimation::CAFunctionLinear( tFrom, tTo, 1, 0 );
	//pSeq->Insert( tTo, pAfter );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CParticleSkeleton* CSkeletonAnimator::GetCorpseAnimator()
{
	STime time = pTime->GetValue();
	CAnimator *pMustBeCorpse = pSeq->GetAnimator( time );
	CDynamicCast<CParticleSkeleton> pCorpse( pMustBeCorpse );
	if ( !pCorpse )
	{
		CDynamicCast<CAPoseMemorizer> pMemorizer( pMustBeCorpse );
		if ( pMemorizer )
		{
			CDynamicCast<CASequence> pSeq2( pMemorizer->pInput );
			pCorpse = pSeq2->GetAnimator( time );
		}
	}
	return pCorpse;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSkeletonAnimator::IsInstableCorpse()
{
	CParticleSkeleton *pCorpse = GetCorpseAnimator();
	/*if ( pCorpse )
	{
		float fFreedom = pCorpse->CalcFreedomDegree();
		char buf[128];
		sprintf( buf, "FreedomDegree: %f\n", fFreedom );
		OutputDebugString( buf );
		return fFreedom > 0.05f;
	}
	else*/
		return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSkeletonAnimator::CalmCorpse()
{/*
	CParticleSkeleton *pCorpse = GetCorpseAnimator();
	if ( pCorpse )
	{
		pCorpse->Calm();
		return true;
	}
	else*/
		return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSkeletonState
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSkeletonState::NeedUpdate()
{
	return pTime.Refresh();
	//return (!value.bValid) | pTime.Refresh();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonState::Recalc()
{
	CDGPtr< CFuncBase<NAnimation::SSkeletonPose> > pAnim( pAnimator );
	pAnim.Refresh();
	value = state;
	value.cIdleBannedFlags = state.cIdleBannedFlags;
	if ( !state.IsIdleBanned() )
	{
		if ( pAnimator->IsFreezed() )
			value.cIdleBannedFlags &= ~( E_INTERNAL_IDLE_OFF | E_NO_IDLE_ON_ACTION | E_NO_IDLE_WHEN_STUNNED );
		else
			value.cIdleBannedFlags |= E_INTERNAL_IDLE_OFF;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAddBoneFilter
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAddBoneFilter::Recalc()
{
	pAnimation.Refresh();
	if ( nAddBone >= pAnimation->GetValue().size() )
		return;
	SBonePose bone = pAnimation->GetValue()[ nAddBone ]; 
	bone.MakeGlobal( pAnimation->GetValue() );
	MakeMatrix( &value.forward, bone.pos, bone.rot );
	value.backward.HomogeneousInverse( value.forward );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAddBoneLocators
////////////////////////////////////////////////////////////////////////////////////////////////////
CAddBoneLocators::CAddBoneLocators( int _nAddBone, NDb::CGeometry *pGeometry ) : nAddBone(_nAddBone)
{
	if ( pGeometry )
		pLocators = shareLocators.Get( pGeometry->GetRecordID() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAddBoneLocators::Recalc()
{
	vector< SBone > &bones = pLocators->GetValue()->bones;
	value.resize( bones.size() );
	if ( nAddBone >= pAnimation->GetValue().size() )
		return;
	SBonePose parent = pAnimation->GetValue()[ nAddBone ]; 
	parent.MakeGlobal( pAnimation->GetValue() );
	for ( int i=0; i<bones.size(); ++i )
	{
		value[i].nParent = -1;
		value[i].pos = parent.rot.Rotate( bones[i].pos ) + parent.pos;
		value[i].rot = parent.rot * bones[i].rot;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAddBoneLocators::operator&( CStructureSaver &f )
{
	f.Add( 1, &pLocators );
	f.Add( 2, &pAnimation );
	f.Add( 3, &nAddBone );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NAnimation;
REGISTER_SAVELOAD_CLASS( 0x01321150, CSkeletonAnimator )
REGISTER_SAVELOAD_CLASS( 0x01321151, CAFunctionLinear )
REGISTER_SAVELOAD_CLASS( 0x01321152, CAFunctionSinus )
REGISTER_SAVELOAD_CLASS( 0x01321153, CAFunctionConstant )
REGISTER_SAVELOAD_CLASS( 0x01321160, CAFunctionParabola )
REGISTER_SAVELOAD_CLASS( 0x11451161, CAFunctionCos )
REGISTER_SAVELOAD_CLASS( 0x01321155, CAnimation )
REGISTER_SAVELOAD_CLASS( 0x01321156, CAInterpolator )
REGISTER_SAVELOAD_CLASS( 0x12021130, CAIK2Chain )
REGISTER_SAVELOAD_CLASS( 0x11931180, CAAdder )
REGISTER_SAVELOAD_CLASS( 0x11931181, CASubtractor )
REGISTER_SAVELOAD_CLASS( 0x10441190, CAddBoneFilter )
REGISTER_SAVELOAD_CLASS( 0x11941151, CAPoseMemorizer )
REGISTER_SAVELOAD_CLASS( 0x11941152, CAPoseAimer )
REGISTER_SAVELOAD_CLASS( 0x11451160, CATurnBody )
REGISTER_SAVELOAD_CLASS( 0x13051130, CAddBoneLocators )
REGISTER_SAVELOAD_CLASS( 0x130A1190, CABoneFilter )
REGISTER_SAVELOAD_CLASS( 0x123c1191, CARandom )
REGISTER_SAVELOAD_CLASS( 0x10722130, CASmartAimer )
REGISTER_SAVELOAD_CLASS( 0x10722131, CSkeletonState )
using namespace NGScene;
REGISTER_SAVELOAD_TEMPL_CLASS( 0x028b2144, CResourcePrecache<CFileAnimation>, CResourcePrecache )
