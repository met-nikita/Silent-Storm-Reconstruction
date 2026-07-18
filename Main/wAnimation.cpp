#include "StdAfx.h"
#include "wAnimation.h"
#include "wObject.h"
#include "..\Misc\2DArray.h"
#include "..\Misc\randomGen.h"
#include "..\DBFormat\DataAnimation.h"
#include "..\DBFormat\DataRPG.h"
#include "aiMap.h"
#include "aiHeight.h"
#include "aiGridSet.h"
#include "aiPosition.h"
#include "GSkeleton.h"
#include "GAnimation.h"
#include "GAnimFormat.h"
#include "GAnimTerrain.h"
#include "GAnimPath.h"
#include "RPGItemInfo.h"
#include "wTSFlags.h"
#include "wMain.h"
#include "GSceneUtils.h"

extern vector<SSphere> sphereParticles;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_HEIGHT_MAP_SIZE = 2.5f;
// retail CUnitAnimator::Rotate @0x33d0f0: 3*PI rad/s (~540 deg/s). Dev's old 700.0 was ~74x too fast.
const float F_DEFAULT_TURN_SPEED = 9.42477798f;
const float F_DEFAULT_MOVE_SPEED = 1.f;
const float F_DEFAULT_AIM_SPEED = 0.8f;
			STime N_DEFAULT_TRANSIT_TIME = 300;
const STime N_FORCED_MOVE_TIME = 1500;
const STime N_DEFAULT_ACTIVATE_TIME = 100;
const STime N_DEFAULT_STRAFE_START_TIME = 200;
const STime N_INSTANT_SHOOT_TIME = 1;
const STime N_INSTANT_TIME = 1;
/*
const float F_LOW_HEIGHT = 0.2f;
const float F_HIGH_1_HEIGHT = 0.6f;
const float F_HIGH_2_HEIGHT = 1.5f;
const float F_HIGH_3_HEIGHT = 2.5f;
*/
const float F_LOW_HEIGHT = 0.4f;
const float F_HIGH_1_HEIGHT = 1.35f;
const float F_HIGH_2_HEIGHT = 2.5f;
const float F_FALL_SPEED = 16.0f;

const float F_STRAFE_DIRECTION_ERROR = 0.05f;
const float FP_LITTLE_STEP = 0.05f;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHeightMapBlock: public CPtrFuncBase< NAI::CHeightMapBlockInfo >
{
	OBJECT_BASIC_METHODS(CHeightMapBlock);
	CObj<NAI::CHeightMapBlockInfo> pHeightsUnchanged;
	ZDATA
	CPtr<NAI::IAIMap> pMap;
	CVec3	center;
	list<CVec3> specialPoints;   // kept for Move()/CalcHeightMap behavior; release dropped it from serialize
	// release save-format @5/6/7 (replaces the serialized specialPoints list): a single special point + 2 floors.
	// Dead in this predecessor (Move drives the list); save-format member only, behavior deferred.
	CVec3 specialPoint = CVec3( 0.0f, 0.0f, 0.0f );
	int nFloor1 = 0;
	int nFloor2 = 0;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pMap); f.Add(3,&center); f.Add(5,&specialPoint); f.Add(6,&nFloor1); f.Add(7,&nFloor2); return 0; }
protected:
	virtual void Recalc();
public:
	CHeightMapBlock() {}
	CHeightMapBlock( const CVec3 &_center, NAI::IAIMap *_pMap ) : pMap(_pMap), center(_center) {}

	void Move( const CVec2 &newCenter, const NAI::SHeightCalcInfo &hNew );
	void Move( const CVec3 &newPoint, const list<CVec3> &specialPoints  );

	float GetHeight( float fX, float fY );
	CVec3 GetNormal( float fX, float fY );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
float CHeightMapBlock::GetHeight( float fX, float fY )
{
	return GetValue()->GetHeight( fX, fY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CHeightMapBlock::GetNormal( float fX, float fY )
{
	return GetValue()->GetNormal( fX, fY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightMapBlock::Move( const CVec3 &newCenter, const list<CVec3> &_specialPoints )
{
	center = newCenter;
	specialPoints = _specialPoints;
	if ( IsValid( pValue ) && IsValid( pHeightsUnchanged ) )
	{
		CTRect<int> r;
		NAI::CalcHeightMapSize( &r, center, F_HEIGHT_MAP_SIZE );
		pHeightsUnchanged->Move( r.left, r.top );
		NAI::CalcHeightMap( pMap, pHeightsUnchanged, newCenter.z, specialPoints );
		*pValue = *pHeightsUnchanged;
		NAI::CheckGradient( pValue );
		//pHeightsUnchanged->ShowSpheres();
	}
	else
	{
		pValue = 0;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeightMapBlock::Recalc()
{
	pValue = new NAI::CHeightMapBlockInfo;
	pHeightsUnchanged = new NAI::CHeightMapBlockInfo;
	CTRect<int> r;
	NAI::CalcHeightMapSize( &r, center, F_HEIGHT_MAP_SIZE );
	pHeightsUnchanged->Init( r );
	NAI::CalcHeightMap( pMap, pHeightsUnchanged, center.z, specialPoints );
	*pValue = *pHeightsUnchanged;
	NAI::CheckGradient( pValue );
	//pValue->ShowSpheres();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitTerrain
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitTerrain: public NAnimation::ITerrainFunction
{
	OBJECT_BASIC_METHODS(CUnitTerrain);
	CDGPtr< CHeightMapBlock > pBlock;
public:
	CUnitTerrain() {}
	CUnitTerrain( const CVec3 &center, NAI::IAIMap *pAIMap );
	void Move( const NAI::SUnitPosition &pos );
	void Move( const NAI::SUnitPosition &pos1, const NAI::SUnitPosition &pos2 );

	virtual float GetHeight( float fX, float fY );
	virtual CVec3 GetNormal( float fX, float fY );
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitTerrain::CUnitTerrain( const CVec3 &center, NAI::IAIMap *pAIMap )
{
	pBlock = new CHeightMapBlock( center, pAIMap );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTerrain::Move( const NAI::SUnitPosition &pos )
{
//	NAI::SHeightCalcInfo hInfo;
//	pos.pos.GetHInfo( &hInfo );
//	pBlock->Move( pos.GetCPNoHeight(), hInfo );
	list<CVec3> specialPoints;
	specialPoints.push_back( pos.GetCP() );
	pBlock->Move( pos.GetCP(), specialPoints ); 
//	CVec2 ps = pos.GetCPNoHeight();
//	NGScene::particleWaveTexture.Wave( Float2Int(ps.x / NGScene::WAVE_GRID_SIZE), Float2Int(ps.y / NGScene::WAVE_GRID_SIZE) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTerrain::Move( const NAI::SUnitPosition &pos1, const NAI::SUnitPosition &pos2 )
{
//	NAI::SHeightCalcInfo hInfo1, hInfo2;
//	pos1.pos.GetHInfo( &hInfo1 );
//	pos2.pos.GetHInfo( &hInfo2 );
//	hInfo1.Merge( hInfo2 );
	list<CVec3> specialPoints;
	specialPoints.push_back( pos1.GetCP() );
	specialPoints.push_back( pos2.GetCP() );
	pBlock->Move( pos2.GetCP(), specialPoints );
//	CVec2 ps = pos1.GetCPNoHeight();
//	NGScene::particleWaveTexture.Wave( Float2Int(ps.x / NGScene::WAVE_GRID_SIZE), Float2Int(ps.y / NGScene::WAVE_GRID_SIZE) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnitTerrain::GetHeight( float fX, float fY )
{
	if ( !IsValid(this) )
		return 0;
	pBlock.Refresh();
	return pBlock->GetHeight( fX, fY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CUnitTerrain::GetNormal( float fX, float fY )
{
	if ( !IsValid(this) )
		return CVec3(0,0,1);
	pBlock.Refresh();
	return pBlock->GetNormal( fX, fY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitTerrain::operator&( CStructureSaver &f )
{
	f.Add( 1, &pBlock );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitAnimator
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitAnimator::CUnitAnimator( CFuncBase<STime> *_pTime, const NAI::SUnitPosition &pos, 
	NDb::CSkeleton *_pSkeleton, NAI::IAIMap *_pAIMap, NDb::CRPGPers *_pPers, CWorld *_pWorld ) 
: pTime( _pTime ), pSkeleton(_pSkeleton), pAIMap(_pAIMap), pPers(_pPers), pWorld(_pWorld)
{
	pTerrainFunc = new CUnitTerrain( pos.GetCP(), pAIMap );
	ASSERT ( _pSkeleton );
	bStart = false;
	bStrafing = false;
	bAimedStrafe = false;
	bAimed = false;
	bWalking = false;
	bActiveItem = false;
	bInactivePose = false;
	bCorpse = false;
	bHealing = false;
	bLadder = false;
	pAnimator = new NAnimation::CSkeletonAnimator( _pSkeleton );
	pAnimator->pTime = _pTime;
	pAnimState = new NAnimation::CSkeletonState;
	pAnimState->pAnimator = pAnimator;
	pAnimState->pTime = _pTime;
	pAnimState->state.pTerrain = pTerrainFunc;
	pAnimState->state.cIdleBannedFlags = 0;
	eAnimationWeaponType = NDb::WT_DEFAULT;
	bHasPKSkeleton = false;
	pFloor = new NGScene::CCInt(-5);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::ChangeSkeleton( NDb::CSkeleton *_pSkeleton, bool bBecomePanzerklein, bool bBoss, bool bTerrorPK )
{
	char cIdleBannedFlags = pAnimState->state.cIdleBannedFlags;
	pSkeleton = _pSkeleton;
	pAnimator = new NAnimation::CSkeletonAnimator( _pSkeleton );
	pAnimator->pTime = pTime;
	pAnimState = new NAnimation::CSkeletonState;
	pAnimState->pAnimator = pAnimator;
	pAnimState->pTime = pTime;
	pAnimState->state.pTerrain = pTerrainFunc;
	pAnimState->state.cIdleBannedFlags = cIdleBannedFlags;
	bHasPKSkeleton = bBecomePanzerklein;
	// retail @0x33c8a0
	if ( bBecomePanzerklein && bBoss )
		nSpecialPKFlags |= NDb::CAnimation::BOSS;
	else
		nSpecialPKFlags &= ~NDb::CAnimation::BOSS;
	if ( bBecomePanzerklein && bTerrorPK )
		nSpecialPKFlags |= NDb::CAnimation::TERROR_PK;
	else
		nSpecialPKFlags &= ~NDb::CAnimation::TERROR_PK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitAnimator::GetHipPos( CVec3 *pRes )
{
	int nIndex = pAnimator->GetBoneIndex( "Hip" );
	if ( nIndex >= 0 )
	{
		CDGPtr<NAnimation::CSkeletonAnimator> pAnimatorDG( pAnimator );
		pAnimatorDG.Refresh();
		const NAnimation::SSkeletonPose &pose = pAnimatorDG->GetValue();
		NAnimation::SBonePose sBone = pose[nIndex];
		sBone.MakeGlobal( pose );

		float fZ = pTerrainFunc->GetHeight( sBone.pos.x, sBone.pos.y );

		pRes->x = sBone.pos.x;
		pRes->y = sBone.pos.y;
		pRes->z = fZ;
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitAnimator::GetBarrelPos( NDb::CGeometry *pWeaponGeometry, NAnimation::SBonePose *pRes, bool bLeft )
{
	*pRes = NAnimation::SBonePose();
	if ( IsValid( pCannon ) )
	{
		NAnimation::CSkeletonAnimator *pCAnimator = pCannon->GetSkeletonAnimator();
		int nIndex = pCAnimator->GetBoneIndex( "Barrel" );
		if ( nIndex >= 0 )
		{
			CDGPtr< CFuncBase<NAnimation::SSkeletonPose> > pCAnim = pCAnimator;
			pCAnim.Refresh();
			const NAnimation::SSkeletonPose &pose = pCAnim->GetValue();
			*pRes = pose[nIndex];
			pRes->MakeGlobal( pose );
			return true;
		}
	}
	else
	{
		const char *pszBoneName = 0;
		if ( bHasPKSkeleton )
			pszBoneName = bLeft ? "L_Weapon" : "R_Weapon";   // @0x33b720: dual-mount PK alternates barrels
		else
		{
			switch ( eAnimationWeaponType )
			{
				case NDb::WT_PISTOL:
					pszBoneName = "Item";
					break;
				case NDb::WT_RIFLE:
					pszBoneName = "Rifle";
					break;
				case NDb::WT_SUB_MACHINE_GUN:
					pszBoneName = "SubMachineGun";
					break;
				case NDb::WT_KNIFE:
					pszBoneName = "Knife";
					break;
				case NDb::WT_MACHINE_GUN:
					pszBoneName = "MachineGun";
					break;
				case NDb::WT_RLAUNCHER:
					pszBoneName = "RocketLauncher";
					break;
				default:
					ASSERT(0);
					return false;
			}
		}
		int nIndex = pAnimator->GetBoneIndex( pszBoneName );
		if ( nIndex >= 0 )
		{
			CPtr<NAnimation::CAddBoneLocators> pLocators =
				new NAnimation::CAddBoneLocators( nIndex, pWeaponGeometry );
			CDGPtr< CFuncBase<NAnimation::SSkeletonPose> > pLocatorsDG = pLocators;
			pLocators->pAnimation = pAnimator;
			pLocatorsDG.Refresh();
			nIndex = pLocators->pLocators->GetValue()->GetBoneIndex( "Barrel" );
			if ( nIndex >= 0 )
			{
				NAnimation::SBonePose &barrel = *pRes;
				barrel = pLocatorsDG->GetValue()[nIndex];
				return true;
			}
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::AlignTime( STime tDelay )
{
	tEnd = pTime->GetValue() + tDelay;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitAnimator::GetStepTime( STime *pT ) const
{
	if ( tSteps.empty() )
		return false;
	*pT = tSteps.front();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::DropStep()
{
	if ( !tSteps.empty() )
		tSteps.erase( tSteps.begin() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::StandStill( CVec2 &pos, float fAngle, bool bNeedStrafe )
{
	CPtr<NAnimation::CAnimation> pAnim;
	if ( bNeedStrafe )
	{
		pAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::POSE_STRAFE, nAnimFlagsPoseWeapon, "Aimed", nAnimFlagsClassSex, pSide ), tEnd );
	}
	else if ( bCorpse )
	{
		pAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::POSE_CORPSE, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ), tEnd );
	}
	if ( !pAnim )
	{
		pAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::POSE, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ), tEnd );
	}
	if ( !pAnim )
	{
		CalculateAnimFlags( false );
		pAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::POSE, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ), tEnd );
	}
	ASSERT(pAnim);
	// dev robustness: the pre-release game.db can lack a POSE animation matching this unit's pose/weapon/
	// class/sex/side flags, so pSkeleton->GetAnimation() returns null and CreateAnimation() yields null.
	// Retail relies on complete data and dereferences null here (it would crash identically -- see the
	// answer-key StandStill @0x33cac0, which falls through to SetStand(null)). Skip the stand-animator setup
	// rather than crash so the unit still gets placed (it just won't have this animation).
	if ( !IsValid( pAnim ) )
		return;
	pAnim->SetStand( tEnd, pos, fAngle );
	if ( bAimed || bWalking || bAimedStrafe != bNeedStrafe )
	{
		pAnimator->AddTransit( tEnd, tEnd + N_DEFAULT_TRANSIT_TIME, PutOnTerrain(pAnim) );
		tEnd += N_DEFAULT_TRANSIT_TIME;
	}
	else
		pAnimator->AddAnimator( tEnd, PutOnTerrain(pAnim) );
	bWalking = false;
	bAimed = false;
	bAimedStrafe = bNeedStrafe;
	hipShift = -pAnim->GetStart();
	pAnimator->AddMemorizer( tEnd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::Stand( const NAI::SUnitPosition &cmdPos, bool bNeedStrafe )
{
	pose = cmdPos.GetPose();
	CalculateAnimFlags();
	if ( bLadder )
	{
		CPtr<NAnimation::CAnimation> pAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::POSE, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ), tEnd );
		ASSERT(pAnim);
		pAnim->SetStand( tEnd, cmdPos.GetCP(), cmdPos.GetDirection() );
		pAnimator->AddTransit( tEnd, tEnd + N_DEFAULT_TRANSIT_TIME, pAnim );
		tEnd += N_DEFAULT_TRANSIT_TIME;
		pAnimator->AddMemorizer( tEnd );
	}
	else
	{
		StandStill( cmdPos.GetCPNoHeight(), cmdPos.GetDirection(), bNeedStrafe );
		pTerrainFunc->Move( cmdPos );
	}
	if ( bNeedStrafe || bCorpse || bInactivePose || bLadder )
		IdleOff();
	else
		IdleOn( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Fly the unit from prevPos to cmdPos in a single MOVE animation: one move clip translating
// (cmdCP - prevCP)/duration per second while turning at fRotateVel, then Stand at the destination.
// Retail CUnitAnimator::Fly @0x3407d0 (the scripted UnitFlyToWaypoint -> CCmdFly -> CExecFly mover).
void CUnitAnimator::Fly( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos )
{
	CPtr<NAnimation::CAnimation> pAnim = pAnimator->CreateAnimation(
		pSkeleton->GetAnimation( NDb::CAnimation::MOVE, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ), tEnd );
	if ( !pAnim )
	{
		pAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::IDLE, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ), tEnd );
		if ( !pAnim )
			return;
	}
	float fDir1 = prevPos.GetDirection();
	float fInvTime = 1000.0f / pAnim->GetTime();
	fRotateVel = SignumNormalizeAngleInRadian( cmdPos.GetDirection() - fDir1 ) * fInvTime;
	CVec3 startCP = prevPos.GetCP();
	CVec3 vel = ( cmdPos.GetCP() - startCP ) * fInvTime;
	pAnim->SetMove( tEnd, startCP, fDir1, vel, fRotateVel );
	pAnimator->AddAnimator( tEnd, pAnim );
	tEnd += pAnim->GetTime();
	pAnimator->AddMemorizer( tEnd );
	Stand( cmdPos, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::DefaultAction( const NAI::SUnitPosition &cmdPos )
{
	bWalking = true;
	tLabel1 = tEnd;
	Stand( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::CalculateAnimFlags( bool bUseItemFlags )
{
	nAnimFlagsPoseWeapon = nAnimFlagsClassSex = 0;
	switch ( pose )
	{
		case NAI::RUN:
		case NAI::WALK:
			nAnimFlagsPoseWeapon |= NDb::CAnimation::POSE_STAND;
			break;
		case NAI::CROUCH:
			nAnimFlagsPoseWeapon |= NDb::CAnimation::POSE_CROUCH;
			break;
		case NAI::CRAWL:
			nAnimFlagsPoseWeapon |= NDb::CAnimation::POSE_CRAWL;
			break;
		default:
			ASSERT( 0 );
	}
	nAnimFlagsPoseWeapon |= NDb::WeaponTypeToAnimFlags( eAnimationWeaponType, bActiveItem && bUseItemFlags, bHasPKSkeleton );
	pAnimState->state.nAnimFlagsPoseWeapon = nAnimFlagsPoseWeapon;

	nAnimFlagsClassSex = pPers->bIsFemale? NDb::CAnimation::SEX_FEMALE : NDb::CAnimation::SEX_MALE;
	if ( pPers && pPers->pClass )
	{
		// The class bit is SCOUT(0x10) shifted left by the class index (record id - 1). Retail
		// (CUnitAnimator::CalculateAnimFlags @0x33afb0) CLAMPS that index to [0,6] before shifting,
		// so any class record whose id >= 7 (Enemy=7, WeakEnemy=9, and the campaign-hero / PK class
		// records the shipped game.db assigns ids past 9) collapses onto the ENEMY bit (0x400)
		// instead of overflowing past 0x400 into a junk bit (e.g. 0x10<<10 = 0x4000) that no
		// animation carries -- which made nClassSexFlags reject EVERY candidate and GetAnimation
		// return null (invisible unit, crash on the first Stand/Move). Match retail exactly.
		int nClass = pPers->pClass->GetRecordID() - 1;
		if ( nClass > 6 )
			nClass = 6;
		else if ( nClass < 0 )
			nClass = 0;
		nAnimFlagsClassSex |= ( NDb::CAnimation::SCOUT << nClass );
	}
	nAnimFlagsClassSex |= pWorld->IsRealTime()? NDb::CAnimation::IN_REALTIME : NDb::CAnimation::IN_COMBAT;
	nAnimFlagsClassSex |= nSpecialPKFlags;
	// retail @0x33afb0: a TERROR_PK skeleton keys clips on exactly {TerrorPK}, with the shooter
	// pose bit remapped onto the TerrorShooter clips. NB the pAnimState poseWeapon mirror above
	// intentionally keeps the pre-remap value (retail does not re-store it).
	if ( nAnimFlagsClassSex & NDb::CAnimation::TERROR_PK )
	{
		nAnimFlagsClassSex = NDb::CAnimation::TERROR_PK;
		if ( nAnimFlagsPoseWeapon & NDb::CAnimation::PK_WEAPON_SHOOTER )
			nAnimFlagsPoseWeapon = ( nAnimFlagsPoseWeapon & ~NDb::CAnimation::PK_WEAPON_SHOOTER ) | NDb::CAnimation::PK_WEAPON_TERROR_SHOOTER;
	}

	pAnimState->state.nAnimFlagsClassSex = nAnimFlagsClassSex;
	pSide = pAnimState->state.pSide = pPers->pSide;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::HealOn( const NAI::SUnitPosition &cmdPos, const char *pszParams )
{
	IdleOn( cmdPos );
	pAnimState->state.szParams = pszParams;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::IdleBan( char cBanSourceFlag, bool bBan )
{ 
	if ( bBan )
		pAnimState->state.cIdleBannedFlags |= cBanSourceFlag;
	else
		pAnimState->state.cIdleBannedFlags &= ~cBanSourceFlag;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::IdleOn( const NAI::SUnitPosition &cmdPos )
{
	pAnimState->state.pos = CVec3( cmdPos.GetCPNoHeight(), 0 );
	pAnimState->state.fAngle = cmdPos.GetDirection();
	pAnimState->state.bCrawl = ( cmdPos.GetPose() == NAI::CRAWL );
	IdleBan( NAnimation::E_INTERNAL_IDLE_OFF, false );
	pAnimState->state.szParams.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::SetCustomIdleAnimation( NDb::CAnimation *pIdleAnimation )
{
	if ( IsValid( pIdleAnimation ) )
	{
		IdleBan( NAnimation::E_CUSTOM_ANIMATION_IDLE, true );
		pAnimState->state.pIdleAnimation = pIdleAnimation;
	}
	else
	{
		IdleBan( NAnimation::E_CUSTOM_ANIMATION_IDLE, false );
		pAnimState->state.pIdleAnimation = 0;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::IdleOff()
{
	IdleBan( NAnimation::E_INTERNAL_IDLE_OFF, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::SetBreathOnlyIdle( bool bOn )
{
 	IdleBan( NAnimation::E_BREATH_ONLY_IDLE, bOn );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NAnimation::CAnimator* CUnitAnimator::PutOnTerrain( NAnimation::CAnimator *pAnim )
{
	if ( pose == NAI::CRAWL )
	{
		NAnimation::CATerrainCrawl *pT = new NAnimation::CATerrainCrawl;
		pT->pSkeleton = pAnimator->pSkeleton;
		pT->pTerrain = pTerrainFunc;
		pT->pInput = pAnim;
		pT->Init();
		return pT;
	}
	else
	{
		NAnimation::CATerrain *pT = new NAnimation::CATerrain;
		pT->pSkeleton = pAnimator->pSkeleton;
		pT->pTerrain = pTerrainFunc;
		pT->pInput = pAnim;
		return pT;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::PlaceUnit( const NAI::SUnitPosition &cmdPos )
{
	// no transit needed
	bAimed = false;
	bWalking = false;
	bAimedStrafe = false;
	Stand( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::StartMove( bool bWalkStrafe )
{
	bStart = true;
	bStrafing = bWalkStrafe;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static NDb::CAnimation::EType GetStartStrafeAnimationType( int nWalkAnim, bool bAimedStrafe )
{
	NDb::CAnimation::EType type;
	switch ( nWalkAnim )
	{
		case 0:
			type = NDb::CAnimation::START_STRAFE_F;
			break;
		case 1:
			type = NDb::CAnimation::START_STRAFE_L;
			break;
		case 2:
			type = NDb::CAnimation::START_STRAFE_B;
			break;
		case 3:
			type = NDb::CAnimation::START_STRAFE_R;
			break;
	}
	return type;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static NDb::CAnimation::EType GetStrafeAnimationType( int nWalkAnim, bool bAimedStrafe )
{
	NDb::CAnimation::EType type;
	switch ( nWalkAnim )
	{
		case 0:
			type = NDb::CAnimation::STRAFE_F;
			break;
		case 1:
			type = NDb::CAnimation::STRAFE_L;
			break;
		case 2:
			type = NDb::CAnimation::STRAFE_B;
			break;
		case 3:
			type = NDb::CAnimation::STRAFE_R;
			break;
	}
	return type;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::Move( 
	const NAI::SUnitPosition &prevPos, 
	const NAI::SUnitPosition &cmdPos,
	const NAI::SUnitPosition &nextPos, 
	bool bEnd, bool bInterGrid )
{
	if ( fabs2(prevPos.GetCPNoHeight() - cmdPos.GetCPNoHeight()) < sqr(FP_LITTLE_STEP) )
		return;
	if ( fabs2(cmdPos.GetCPNoHeight() - nextPos.GetCPNoHeight()) < sqr(FP_LITTLE_STEP) )
		bEnd = true;

	if ( bStart )
		ptPrev = prevPos.GetCPNoHeight();
	vector<CVec2> path;
	// 1
	if ( !bStart )
		path.push_back( ptPrevPrev );
	// 2
	path.push_back( ptPrev );
	// 3
	path.push_back( cmdPos.GetCPNoHeight() );
	// 4
	if ( !bEnd )
		path.push_back( nextPos.GetCPNoHeight() );
	//
	ptPrevPrev = ptPrev;
	ptPrev = cmdPos.GetCPNoHeight();

	CVec2 direction = ptPrev - ptPrevPrev;

	CPtr<NAnimation::CPathInterpolator> pFull = new NAnimation::CPathInterpolator( path );
	int nPts = pFull->GetNumPoints();
	int nP1 = 0;
	int nP2 = nPts-1;
	if ( !bStart )
		nP1 += (nPts-1) / (path.size()-1);
	if ( !bEnd )
		nP2 -= (nPts-1) / (path.size()-1);
	CPtr<NAnimation::CPathInterpolator> pPathI = new NAnimation::CPathInterpolator( *pFull, nP1, nP2 );
	float fMoveDistance = pPathI->GetDistance();
	
	if ( bStrafing )
	{
		ASSERT( pose != NAI::RUN );
		ASSERT( pose != NAI::CRAWL );
		if ( pose == NAI::RUN || pose == NAI::CRAWL )
			bStrafing = false;
	}

	if ( bStart )
	{
		fLookAngle = prevPos.GetDirection();
		if ( bInterGrid && !bStrafing )
		{
			bWalking = true;
			StandStill( prevPos.GetCPNoHeight(), atan2( direction.y, direction.x ), false );
		}
		else
		{
			if ( bAimedStrafe && !CanStandAimed( prevPos ) )
				bAimedStrafe = false;
			Stand( prevPos, bAimedStrafe && bStrafing );
		}

		nWalkAnim = 0;
		if ( bStrafing )
		{
			float fNextMoveAngle = NormalizeAngleInRadian( atan2( direction.y, direction.x ) - fLookAngle );
			if ( fNextMoveAngle < FP_PI4 + F_STRAFE_DIRECTION_ERROR || fNextMoveAngle > FP_PI4 * 7 - F_STRAFE_DIRECTION_ERROR )
				nWalkAnim = 0;
			else if ( fNextMoveAngle < FP_PI4 * 3 + F_STRAFE_DIRECTION_ERROR )
				nWalkAnim = 1;
			else if ( fNextMoveAngle > FP_PI4 * 5 - F_STRAFE_DIRECTION_ERROR )
				nWalkAnim = 3;
			else
				nWalkAnim = 2;
		}
	}

	STime tBeforeAll = tEnd;
	if ( bStart && bStrafing )
		tEnd += N_DEFAULT_STRAFE_START_TIME;

	STime tBefore = tEnd;

	CPtr<NAnimation::CASequence> pSeq = new NAnimation::CASequence;
	pSeq->pSkeleton = pAnimator->pSkeleton;

	//tSteps.clear();
	for ( vector<STime>::iterator i = tSteps.begin(); i != tSteps.end(); ++i )
		if ( *i > tEnd )
		{
			tSteps.erase( i, tSteps.end() );
			break;
		}

	if ( bStart || fStartLeft > 0 )
	{
		NDb::CAnimation::EType type = NDb::CAnimation::START_MOVE;
		if ( bCorpse )
			type = NDb::CAnimation::START_MOVE_CORPSE;
		else if ( pose == NAI::RUN )
			type = NDb::CAnimation::START_RUN;
		else if ( bStrafing )
			type = GetStartStrafeAnimationType( nWalkAnim, bAimedStrafe );
		if ( bStart )
			tAnim = 0;

		CPtr<NAnimation::CAnimation> pStartAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( type, nAnimFlagsPoseWeapon, bAimedStrafe && bStrafing ? "Aimed" : 0, nAnimFlagsClassSex, pSide ),
			tEnd - tAnim );
		if ( !pStartAnim )
		{
			DefaultAction( cmdPos );
			bStart = false;
			return;
		}

		if ( bStart )
			fStartLeft = pStartAnim->GetDistance();

		if ( pStartAnim->GetDistance() > 0 )
		{
			CQuat q( nWalkAnim * FP_PI2, CVec3(0,0,-1) );
			CVec3 newHipShift;
			q.Rotate( &newHipShift, hipShift );
			pStartAnim->SetShift( newHipShift );
			pStartAnim->SetMoveCycle( true );

			pStartAnim->SetTrajectory( tEnd, pStartAnim->GetVelocity(), 0, pPathI );
			pSeq->Insert( tEnd, pStartAnim );
			
			STime tBegin = tEnd;
			if ( fStartLeft < fMoveDistance )
			{
				tEnd += fStartLeft * 1000 / pStartAnim->GetVelocity();
				tAnim = 0;
				fMoveDistance -= fStartLeft;
			}
			else
			{
				STime t = fMoveDistance * 1000 / pStartAnim->GetVelocity();
				tEnd += t;
				tAnim += t;
				fStartLeft -= fMoveDistance;
				fMoveDistance = 0;
			}
			STime tStep = pStartAnim->GetStepLabel1();
			if ( tStep && tStep >= tBegin && tStep <= tEnd )
				tSteps.push_back( tStep );
			//CRAP
			if ( type == NDb::CAnimation::START_RUN && bStart && bEnd )
				tSteps.push_back( tEnd );
			//
			tStep = pStartAnim->GetStepLabel2();
			if ( tStep && tStep >= tBegin && tStep <= tEnd )
				tSteps.push_back( tStep );
		}
	}

	float fVelocity = 0;
	if ( fMoveDistance > 0 )
	{
		NDb::CAnimation::EType type = NDb::CAnimation::MOVE;
		if ( bCorpse )
			type = NDb::CAnimation::MOVE_CORPSE;
		else if ( pose == NAI::RUN )
			type = NDb::CAnimation::RUN;
		else if ( bStrafing )
			type = GetStrafeAnimationType( nWalkAnim, bAimedStrafe );
		CPtr<NAnimation::CAnimation> pCycleAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( type, nAnimFlagsPoseWeapon, bAimedStrafe && bStrafing ? "Aimed" : 0, nAnimFlagsClassSex, pSide ), 
			tEnd - tAnim, true );
		if ( !pCycleAnim )
		{
			DefaultAction( cmdPos );
			bStart = false;
			sort( tSteps.begin(), tSteps.end() );
			return;
		}

		CQuat q( nWalkAnim * FP_PI2, CVec3(0,0,-1) );
		CVec3 newHipShift;
		q.Rotate( &newHipShift, hipShift );
		pCycleAnim->SetShift( newHipShift );
		pCycleAnim->SetMoveCycle( true );

		fVelocity = pCycleAnim->GetVelocity();
		pCycleAnim->SetTrajectory( tEnd, fVelocity, fStartLeft, pPathI );
		pSeq->Insert( tEnd, pCycleAnim );

		STime tBegin = tEnd;
		STime t = fMoveDistance * 1000 / fVelocity;
		tEnd += t;
		tAnim += t;
		fStartLeft = 0;

		STime tCycle = pCycleAnim->GetTime();
		STime tStep = pCycleAnim->GetStepLabel1();
		if ( tCycle < 1 )
		{
			ASSERT(0);
			tCycle = 100;
		}
		if ( tStep )
		{
			while ( tStep < tEnd )
			{
				if ( tStep >= tBegin )
					tSteps.push_back( tStep );
				tStep += tCycle;
			}
		}
		tStep = pCycleAnim->GetStepLabel2();
		if ( tStep )
		{
			while ( tStep < tEnd )
			{
				if ( tStep >= tBegin )
					tSteps.push_back( tStep );
				tStep += tCycle;
			}
		}
	}

	sort( tSteps.begin(), tSteps.end() );

	if ( !bStrafing )
		pAnimator->AddAnimator( tBefore, PutOnTerrain(pSeq) );
	else
	{
		float fConstAngle = SignumNormalizeAngleInRadian( nWalkAnim * FP_PI2 + fLookAngle );
		float fConstAngleNext = fConstAngle;
		CPtr<NAnimation::CAFunction> pFuncPath = new NAnimation::CAFunctionPathDirection( tBefore, tEnd, pPathI );
		CPtr<NAnimation::CAFunction> pFuncConst1 = new NAnimation::CAFunctionConstant( - fConstAngle );
		CPtr<NAnimation::CAFunction> pFuncSum1 = new NAnimation::CAFunctionSum( pFuncPath, pFuncConst1 );
		CPtr<NAnimation::CATurnBody> pTurn = new NAnimation::CATurnBody(-1);
		pTurn->pFunc = pFuncSum1;
		pTurn->pAnim = pSeq;
		pTurn->nBoneSpine = pAnimator->GetBoneIndex( "Spine" );
		pTurn->nBoneChest = pAnimator->GetBoneIndex( "Chest" );

		if ( bStart )
			pAnimator->AddTransit( tBeforeAll, tBefore, PutOnTerrain(pTurn) );
		else
			pAnimator->AddAnimator( tBefore, PutOnTerrain(pTurn) );

		bool bNeedChangeAnim = false;

		if ( !bEnd )
		{
			CVec2 nextDir = nextPos.GetCPNoHeight() - cmdPos.GetCPNoHeight();
			float fNextMoveAngle = SignumNormalizeAngleInRadian( atan2( nextDir.y, nextDir.x ) - fConstAngle );
			if ( fNextMoveAngle > FP_PI4 + F_STRAFE_DIRECTION_ERROR )
			{
				nWalkAnim = (nWalkAnim + 1) & 3;
				fConstAngleNext += FP_PI2;
				bNeedChangeAnim = true;
			}
			if ( fNextMoveAngle < - FP_PI4 - F_STRAFE_DIRECTION_ERROR )
			{
				nWalkAnim = (nWalkAnim + 3) & 3;
				fConstAngleNext -= FP_PI2;
				bNeedChangeAnim = true;
			}
		}
		if ( bAimedStrafe && !CanStandAimed( cmdPos ) )
		{
			bNeedChangeAnim = true;
			bAimedStrafe = false;
		}
		if ( bNeedChangeAnim )
		{
			NDb::CAnimation::EType type = GetStrafeAnimationType( nWalkAnim, bAimedStrafe );
			STime tMiddle = (tEnd + tBefore) / 2;
			CPtr<NAnimation::CAnimation> pStrafeAnim = pAnimator->CreateAnimation(
				pSkeleton->GetAnimation( type, nAnimFlagsPoseWeapon, bAimedStrafe && bStrafing ? "Aimed" : 0, nAnimFlagsClassSex, pSide ), 
				tMiddle, true );
			if ( !pStrafeAnim )
			{
				DefaultAction( cmdPos );
				bStart = false;
				return;
			}

			CQuat q( nWalkAnim * FP_PI2, CVec3(0,0,-1) );
			CVec3 newHipShift;
			q.Rotate( &newHipShift, hipShift );
			pStrafeAnim->SetShift( newHipShift );
			pStrafeAnim->SetMoveCycle( true );

			pStrafeAnim->SetTrajectory( tBefore, fVelocity, fStartLeft, pPathI );
			tAnim = tEnd - tMiddle;

			CPtr<NAnimation::CAFunction> pFuncConst2 = new NAnimation::CAFunctionConstant( - fConstAngleNext );
			CPtr<NAnimation::CAFunction> pFuncSum2 = new NAnimation::CAFunctionSum( pFuncPath, pFuncConst2 );
			CPtr<NAnimation::CATurnBody> pTurn = new NAnimation::CATurnBody(-1);
			pTurn->pFunc = pFuncSum2;
			pTurn->pAnim = pStrafeAnim;
			pTurn->nBoneSpine = pAnimator->GetBoneIndex( "Spine" );
			pTurn->nBoneChest = pAnimator->GetBoneIndex( "Chest" );

			pAnimator->AddTransit( tMiddle, tEnd, PutOnTerrain(pTurn) );

			fStartLeft = 0;
		}
	}

	bStart = false;

	pTerrainFunc->Move( prevPos, cmdPos );
	IdleOff();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x33a560 -- same layer/pose/direction and |dX|<=1 && |dY|<=1
static bool CheckIfOneStep( const NAI::SPathPlace &a, const NAI::SPathPlace &b )
{
	if ( a.GetLayer() != b.GetLayer() ) return false;
	if ( a.GetPose()  != b.GetPose()  ) return false;
	if ( a.GetDirection() != b.GetDirection() ) return false;
	int dx = (int)a.GetX() - (int)b.GetX();
	int dy = (int)a.GetY() - (int)b.GetY();
	return dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1;
}
// @0x33a5f0 -- orthogonal iff nX or nY matches
static bool CheckIfNotDiagonal( const NAI::SPathPlace &a, const NAI::SPathPlace &b )
{
	return a.GetX() == b.GetX() || a.GetY() == b.GetY();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::EndMove( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos, bool bFreeze, bool bInterGrid )
{
	bool bHandled = false;
	if ( bStart && !bStrafing && !bCorpse && !bHasPKSkeleton &&
		cmdPos.GetPose() != NAI::CRAWL && cmdPos.GetPose() != NAI::CROUCH &&
		CheckIfOneStep( prevPos.pos.p, cmdPos.pos.p ) )
	{
		if ( CheckIfNotDiagonal( prevPos.pos.p, cmdPos.pos.p ) )
			MoveOneStep( prevPos );
		else
		{
			NAI::EPose savedPose = pose;
			if ( pose == NAI::RUN ) pose = NAI::WALK;
			NAI::SUnitPosition prevWalk = prevPos, cmdWalk = cmdPos;
			prevWalk.SetPose( NAI::WALK );
			cmdWalk.SetPose( NAI::WALK );
			Move( prevWalk, cmdWalk, cmdWalk, true, bInterGrid );
			pose = savedPose;
		}
		bHandled = true;
	}
	if ( !bHandled )
		Move( prevPos, cmdPos, cmdPos, true, bInterGrid );
	bStrafing = false;
	bWalking = true;
	pAnimator->AddMemorizer( tEnd );
	IdleOff();
	if ( !bFreeze )
		Stand( cmdPos, bAimedStrafe );
	bStandIfRecalcCommand = false;   // retail tail; inert in dev (no producer)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::Move( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos, const NAI::SUnitPosition &nextPos, bool bInterGrid )
{
	Move( prevPos, cmdPos, nextPos, false, bInterGrid );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//static int nShootDir = 40;
void CUnitAnimator::Snipe( const NAI::SUnitPosition &cmdPos, const CRay &ray )
{
	Attack( cmdPos, ray, false, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::Attack( const NAI::SUnitPosition &cmdPos, const CRay &ray, bool bInstant, bool bNoShoot )
{
	if ( IsValid( pCannon ) )
	{
		ASSERT( !bNoShoot );
		if ( bNoShoot )
			return;
		AttackCannon( cmdPos, ray, bInstant );
		return;
	}
	CVec3 shootDir = ray.ptDir;
/*
	//
	shootDir.x = cos(cmdPos.GetDirection());
	shootDir.y = sin(cmdPos.GetDirection());
	shootDir.z = tan( FP_PI2 * nShootDir / 41 );
	Normalize(&shootDir);
	if ( --nShootDir == -40 )
		nShootDir = 40;
	sphereParticles.clear();
	for ( int i = 0; i < 20; ++i )
		sphereParticles.push_back( SSphere( ray.ptOrigin + shootDir * 0.3f * i, 0.05f ) );
	//
*/
	Normalize(&shootDir);
	float fNextAngle = atan2( shootDir.y, shootDir.x );
	float fCurAngle = cmdPos.GetDirection();

	STime tTransit = 0;
	if ( bAimed )
	{
		if ( bInstant )
			tTransit = N_INSTANT_SHOOT_TIME;
		else
		{
			CVec3 cross = aimDir ^ shootDir;
			tTransit = asin( fabs(cross) ) / F_DEFAULT_AIM_SPEED * 1000;
		}
	}
	else if ( bAimedStrafe )
	{
		CVec3 dir( cos(fCurAngle), sin(fCurAngle), 0 );
		CVec3 cross = dir ^ shootDir;
		tTransit = asin( fabs(cross) ) / F_DEFAULT_AIM_SPEED * 1000 + N_DEFAULT_TRANSIT_TIME;
	}
	else
	{
		CPtr<NAnimation::CAnimation> pStartAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::START_ATTACK, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ), tEnd );
		if ( pStartAnim )
		{
			pStartAnim->SetStand( tEnd, cmdPos.GetCPNoHeight(), cmdPos.GetDirection() );
			pAnimator->AddAnimator( tEnd, PutOnTerrain(pStartAnim) );
			STime t = pStartAnim->GetTime();
			tEnd += t / 2;
			tTransit = t - t / 2;
		}
		else
			tTransit = N_DEFAULT_TRANSIT_TIME;
	}

	// horizontal turn
	float fHorizAngle = SignumNormalizeAngleInRadian( fNextAngle - fCurAngle );
	// vertical turn
	float fVertAngle = asin( shootDir.z );

	CalculateAnimFlags();
	CPtr<NAnimation::CAnimation> pAtAnim = pAnimator->CreateAnimation(
		pSkeleton->GetAnimation( NDb::CAnimation::ATTACK, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ), tEnd + tTransit );
	if ( !pAtAnim )
	{
		CalculateAnimFlags( false );
		pAtAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::ATTACK, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ), tEnd + tTransit );
	}
	if ( !pAtAnim )
	{
		DefaultAction( cmdPos );
		return;
	}
	tLabel1 = pAtAnim->GetTimeLabel1();
	pAtAnim->SetStand( tEnd + tTransit, cmdPos.GetCPNoHeight(), cmdPos.GetDirection() );

	CPtr<NAnimation::CAnimator> pVertAnim = pAtAnim;
	CPtr<NAnimation::CAnimation> pUpAnim, pSuperUpAnim;
	if ( fVertAngle < 0 )
	{
		pUpAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::ATTACK_DOWN, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ), 
				tEnd + tTransit );
		pSuperUpAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::ATTACK_FLOOR, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ),
				tEnd + tTransit );
	}
	else
	{
		pUpAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::ATTACK_UP, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ), 
				tEnd + tTransit );
		pSuperUpAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::ATTACK_CEILING, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ),
				tEnd + tTransit );
	}

	float fUpAngle = 0, fSuperUpAngle = 0;
	if ( pUpAnim )
	{
		pUpAnim->SetStand( tEnd + tTransit, cmdPos.GetCPNoHeight(), cmdPos.GetDirection() );
		fUpAngle = pUpAnim->GetAngle();
	}
	if ( pSuperUpAnim )
	{
		pSuperUpAnim->SetStand( tEnd + tTransit, cmdPos.GetCPNoHeight(), cmdPos.GetDirection() );
		fSuperUpAngle = pSuperUpAnim->GetAngle();
	}
	if ( fUpAngle == 0 )
		fUpAngle = FP_PI4;
	if ( fSuperUpAngle == 0 || fSuperUpAngle < fUpAngle )
		fSuperUpAngle = FP_PI2;

	CPtr<NAnimation::CAInterpolator> pMiddle = new NAnimation::CAInterpolator;
	if ( !pUpAnim )
	{
		if ( pSuperUpAnim )
		{
			pMiddle->pAnim1 = pAtAnim;
			pMiddle->pAnim2 = pSuperUpAnim;
			pMiddle->pFunc = new NAnimation::CAFunctionConstant( Min( 1.0f, fabs(fVertAngle) / fSuperUpAngle ) );
			pVertAnim = pMiddle;
		}
	}
	else if ( pSuperUpAnim && fabs(fVertAngle) > fUpAngle )
	{
		pMiddle->pAnim1 = pUpAnim;
		pMiddle->pAnim2 = pSuperUpAnim;
		pMiddle->pFunc = new NAnimation::CAFunctionConstant( Min( 1.0f, (fabs(fVertAngle) - fUpAngle) / (fSuperUpAngle - fUpAngle) ) );
		pVertAnim = pMiddle;
	}
	else
	{
		pMiddle->pAnim1 = pAtAnim;
		pMiddle->pAnim2 = pUpAnim;
		pMiddle->pFunc = new NAnimation::CAFunctionConstant( Min( 1.0f, fabs(fVertAngle) / fUpAngle ) );
		pVertAnim = pMiddle;
	}

	CPtr<NAnimation::CATurnBody> pTurn = new NAnimation::CATurnBody( fHorizAngle );
	pTurn->pFunc = new NAnimation::CAFunctionConstant(1);
	pTurn->pAnim = pVertAnim;
	pTurn->nBoneSpine = pAnimator->GetBoneIndex( "Spine" );
	pTurn->nBoneChest = pAnimator->GetBoneIndex( "Chest" );
		
	aimDir = shootDir;
	bAimed = true;
	bAimedStrafe = true;

	pAnimator->AddTransit( tEnd, tEnd + tTransit, PutOnTerrain( pTurn ), NAnimation::PARABOLA );
	tEnd += tTransit;
	if ( !bNoShoot )
		tEnd += pAtAnim->GetTime();
	pAnimator->AddMemorizer( tEnd );
	IdleOff();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const char* GetAnimHeightParams( const NAI::SUnitPosition &cmdPos, NAI::EBlowHeight eBHeight )
{
	const char *pszParams = 0;
	NAI::EPose pose = cmdPos.GetPose();
	if ( pose == NAI::CRAWL )
		return 0;
	switch (eBHeight)
	{
		case NAI::BH_BOTTOM:
			pszParams = "Height1";
			break;
		case NAI::BH_MIDDLE:
			pszParams = "Height2";
			break;
		case NAI::BH_TOP:
			if ( pose == NAI::CROUCH )
				pszParams = "Height2";
			else
				pszParams = "Height3";
			break;
	}
	return pszParams;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::CloseAttack( const NAI::SUnitPosition &cmdPos, NAI::EBlowHeight eBHeight )
{
	if ( !bAimed )
	{
		CPtr<NAnimation::CAnimation> pStartAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::START_ATTACK, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ),
				tEnd );
		if ( pStartAnim )
		{
			pStartAnim->SetStand( tEnd, cmdPos.GetCPNoHeight(), cmdPos.GetDirection() );
			pAnimator->AddAnimator( tEnd, PutOnTerrain(pStartAnim) );
			tEnd += pStartAnim->GetTime();
		}
	}
	bAimed = true;
	bAimedStrafe = false;
	const char *pszParams = GetAnimHeightParams( cmdPos, eBHeight );
	if ( !pszParams )
	{
		ASSERT(0);
		DefaultAction( cmdPos );
		return;
	}
	CPtr<NAnimation::CAnimation> pAnim = pAnimator->CreateAnimation(
		pSkeleton->GetAnimation( NDb::CAnimation::ATTACK, nAnimFlagsPoseWeapon, pszParams, nAnimFlagsClassSex, pSide ),
			tEnd );
	if ( !pAnim )
	{
		DefaultAction( cmdPos );
		return;
	}
	tLabel1 = pAnim->GetTimeLabel1();
	pAnim->SetStand( tEnd, cmdPos.GetCPNoHeight(), cmdPos.GetDirection() );
	pAnimator->AddAnimator( tEnd, PutOnTerrain(pAnim) );
	tEnd += pAnim->GetTime();
	pAnimator->AddMemorizer( tEnd );
	IdleOff();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::Rotate( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos, bool bStart )
{
	if ( bStart )
	{
		Stand( prevPos );

		float fCurAngle = prevPos.GetDirection();
		float fNextAngle = cmdPos.GetDirection();

		float fDiffAngle = SignumNormalizeAngleInRadian( fNextAngle - fCurAngle );
		fRotateVel = F_DEFAULT_TURN_SPEED * Sign(fDiffAngle);

		CPtr<NAnimation::CAnimation> pAnim;
		if ( bCorpse )
		{
			pAnim = pAnimator->CreateAnimation(
				pSkeleton->GetAnimation( NDb::CAnimation::POSE_CORPSE, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ),
					tEnd, true );
		}
		else
		{
			/*
			if ( fRotateVel > 0 )
			{
				pAnim = pAnimator->CreateAnimation(
					pSkeleton->GetAnimation( NDb::CAnimation::TURN_LEFT, nAnimFlags ), tEnd, true );
			}
			else
			{
				pAnim = pAnimator->CreateAnimation(
					pSkeleton->GetAnimation( NDb::CAnimation::TURN_RIGHT, nAnimFlags ), tEnd, true );
			}
			if ( pAnim )
			{
				pAnim->SetRotateCycle( true );
				fRotateVel = Sign(fDiffAngle) * FP_PI2 * 1000 / pAnim->GetTime();
			}
			*/
		}
		if ( !pAnim )
		{
			pAnim = pAnimator->CreateAnimation(
				pSkeleton->GetAnimation( NDb::CAnimation::POSE, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ),
					tEnd, true );
		}
		ASSERT(pAnim);
		CVec3 posG( prevPos.GetCPNoHeight(), 0 );
		pAnim->SetMove( tEnd, posG, fCurAngle, VNULL3, fRotateVel );
		pAnimator->AddAnimator( tEnd, PutOnTerrain(pAnim) );
	}

	++tEnd;// += FP_PI4 * 1000 / fabs(fRotateVel);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::EndRotate( const NAI::SUnitPosition &cmdPos )
{
	pAnimator->AddMemorizer( tEnd );
	bWalking = true;
	Stand( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::ChangePose( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos )
{
	if ( prevPos.pos.p.GetPose() == cmdPos.pos.p.GetPose() )
		return;

	Stand( prevPos );

	const char *pszParams = 0;
	switch ( cmdPos.GetPose() )
	{
		case NAI::CRAWL:
			pszParams = "Crawl";
			break;
		case NAI::CROUCH:
			pszParams = "Crouch";
			break;
		case NAI::WALK:
		case NAI::RUN:
			pszParams = "Stand";
			break;
	}

	CPtr<NAnimation::CAnimation> pAnim = pAnimator->CreateAnimation(
		pSkeleton->GetAnimation( NDb::CAnimation::CHANGE_POSE, nAnimFlagsPoseWeapon, pszParams, nAnimFlagsClassSex, pSide ),
			tEnd );

	if ( !pAnim )
	{
		DefaultAction( cmdPos );
		return;
	}

	pose = cmdPos.GetPose();
	CalculateAnimFlags();

	pAnim->SetStand( tEnd, prevPos.GetCPNoHeight(), prevPos.GetDirection() );
	if ( prevPos.GetPose() == NAI::CRAWL || cmdPos.GetPose() == NAI::CRAWL )
	{
		NAnimation::CATerrain *pWalk = new NAnimation::CATerrain;
		pWalk->pSkeleton = pAnimator->pSkeleton;
		pWalk->pInput = pAnim;
		pWalk->pTerrain = pTerrainFunc;

		NAnimation::CATerrainCrawl *pCrawl = new NAnimation::CATerrainCrawl;
		pCrawl->pSkeleton = pAnimator->pSkeleton;
		pCrawl->pInput = pAnim;
		pCrawl->pTerrain = pTerrainFunc;
		pCrawl->Init();

		CPtr<NAnimation::CAInterpolator> pOut = new NAnimation::CAInterpolator;
		pOut->pFunc = new NAnimation::CAFunctionLinear( tEnd, tEnd + pAnim->GetTime() );
		if ( prevPos.GetPose() == NAI::CRAWL )
		{
			pOut->pAnim1 = pCrawl;
			pOut->pAnim2 = pWalk;
		}
		else
		{
			pOut->pAnim1 = pWalk;
			pOut->pAnim2 = pCrawl;
		}
		pAnimator->AddAnimator( tEnd, pOut );
	}
	else
		pAnimator->AddAnimator( tEnd, PutOnTerrain(pAnim) );
	tEnd += pAnim->GetTime();
	pAnimator->AddMemorizer( tEnd );
	IdleOn( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::Climb( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos, bool bRealClimb )
{
	bInactivePose = true;
	CPtr<NAnimation::CAnimator> pResult = 0;
	STime t = 0;
	Stand( prevPos );
	pose = cmdPos.GetPose();
	CalculateAnimFlags();

	if ( !bRealClimb )
	{
		pTerrainFunc->Move( prevPos, cmdPos );
		CPtr<NAnimation::CAnimation> pAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::JUMP_START, 0	), tEnd );
		if ( !pAnim )
		{
			DefaultAction( cmdPos );
			return;
		}
		pAnim->SetStand( tEnd, prevPos.GetCPNoHeight(), prevPos.GetDirection() );
		t = pAnim->GetTime();
		pResult = PutOnTerrain(pAnim);
	}
	else
	{
		pTerrainFunc->Move( cmdPos );
		float fHeightDiff = cmdPos.GetCP().z - prevPos.GetCP().z;
		float fFrom, fTo;
		const char *pszParams = 0;
		if ( fHeightDiff < F_HIGH_1_HEIGHT )
		{
			fFrom = F_LOW_HEIGHT;
			fTo = F_HIGH_1_HEIGHT;
			pszParams = "Height1";
		}
		else
		{
			fFrom = F_HIGH_1_HEIGHT;
			fTo = F_HIGH_2_HEIGHT;
			pszParams = "Height2";
		}
		CPtr<NAnimation::CAnimation> pFrom = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::CLIMB_LOW, 0, pszParams ), tEnd );
		CPtr<NAnimation::CAnimation> pTo = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::CLIMB_HIGH, 0, pszParams ), tEnd );
		if ( !pFrom || !pTo )
		{
			DefaultAction( cmdPos );
			return;
		}
		float fKoeff = (fHeightDiff - fFrom) / (fTo - fFrom);
		if ( fKoeff < 0 )
			fKoeff = 0;
		if ( fKoeff > 1 )
			fKoeff = 1;
		STime tFrom = pFrom->GetTime();
		STime tTo = pTo->GetTime();
		t = (STime)((1 - fKoeff) * (float)tFrom + fKoeff * (float)tTo);
		pFrom->SetInterval( tEnd, tEnd + t );
		pTo->SetInterval( tEnd, tEnd + t );
		pFrom->SetStand( tEnd, prevPos.GetCP(), prevPos.GetDirection() );
		pTo->SetStand( tEnd, prevPos.GetCP(), prevPos.GetDirection() );
		CPtr<NAnimation::CAInterpolator> pMiddle = new NAnimation::CAInterpolator;
		pMiddle->pAnim1 = pFrom;
		pMiddle->pAnim2 = pTo;
		pMiddle->pFunc = new NAnimation::CAFunctionConstant( fKoeff );
		pResult = pMiddle;
	}
	
	pAnimator->AddAnimator( tEnd, pResult );
	tEnd += t;
	pAnimator->AddMemorizer( tEnd );
	IdleOff();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::ForcedMove( const NAI::SUnitPosition &cmdPos )
{
	// crap - it works, but overriding constants is an awfully ugly thing to do
	STime backup = N_DEFAULT_TRANSIT_TIME;
	N_DEFAULT_TRANSIT_TIME = N_FORCED_MOVE_TIME;
	DefaultAction( cmdPos );
	N_DEFAULT_TRANSIT_TIME = backup;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::Fall( const NAI::SUnitPosition &cmdPos, float fPrevHeight )
{
	OutputDebugString("Forced fall began...\n");
	pTerrainFunc->Move( cmdPos );
	float fHeightDiff = fPrevHeight - cmdPos.GetCP().z;
	if ( fHeightDiff < 0 )
	{
		ForcedMove( cmdPos );
		return;
	}

	int nFlags = NDb::CAnimation::WEAPON_NONE;
	if ( cmdPos.GetPose() == NAI::CRAWL )
		nFlags |= NDb::CAnimation::POSE_CRAWL;
	else if ( cmdPos.GetPose() == NAI::CROUCH )
		nFlags |= NDb::CAnimation::POSE_CROUCH;
	else 
		nFlags |= NDb::CAnimation::POSE_STAND;

	tEnd = pTime->GetValue();
	NDb::CAnimation *pDbAnim = pSkeleton->GetAnimation( NDb::CAnimation::FALL, nFlags );
	CPtr<NAnimation::CAnimation> pBegin;
	if ( pDbAnim->fFallHeight < fHeightDiff )
		pBegin = pAnimator->CreateAnimation( pDbAnim, tEnd );
	if ( !pBegin )
	{
		ForcedMove( cmdPos );
		return;
	}

	OutputDebugString("Forced fall found appropriate animation...\n");
	
	STime tFall = pBegin->GetTimeLabel1();
	STime tFallInterval = tFall - tEnd;
	STime tFallLength = (STime)( (fHeightDiff - pDbAnim->fFallHeight) / F_FALL_SPEED * 1000 );

	float fPart = float( tFallInterval ) / pBegin->GetTime();
	CVec3 prevCP = cmdPos.GetCP();
	prevCP.z = fPrevHeight;
	pBegin->SetStand( tEnd, prevCP, cmdPos.GetDirection() );
	pAnimator->AddAnimator( tEnd, pBegin );

	CVec3 middleCP = prevCP;
	middleCP.z = fPrevHeight - pDbAnim->fFallHeight;
	CPtr<NAnimation::CAnimation> pMiddle = pAnimator->CreateAnimation( pDbAnim, tFall );
	pMiddle->SetTimeFunction( new NAnimation::CAFunctionConstant( fPart ) );
	pMiddle->SetMove( tFall, middleCP, cmdPos.GetDirection(), CVec3(0,0,-F_FALL_SPEED), 0 );
	pAnimator->AddAnimator( tFall, pMiddle );

	CPtr<NAnimation::CAnimation> pEnd = pAnimator->CreateAnimation( pDbAnim, tEnd + tFallLength );
	pEnd->SetStand( tEnd, cmdPos.GetCP() + CVec3( 0, 0, pDbAnim->fFallHeight ), cmdPos.GetDirection() );
	pAnimator->AddAnimator( tFall + tFallLength, pEnd );

	tEnd += pBegin->GetTime() + tFallLength;
	pAnimator->AddMemorizer( tEnd );
	
	pAnimator->Updated();
	IdleOn( cmdPos );

}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::Jump( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos, bool bRealJump, bool bJumpBack )
{
	bInactivePose = false;
	CPtr<NAnimation::CAnimator> pResult = 0;
	STime t = 0;
	pose = cmdPos.GetPose();
	if ( !bRealJump )
	{
		pTerrainFunc->Move( prevPos, cmdPos );
		CPtr<NAnimation::CAnimation> pAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::CLIMB_FINISH, 0 ), tEnd );
		if ( !pAnim )
		{
			DefaultAction( cmdPos );
			return;
		}
		pAnim->SetStand( tEnd, prevPos.GetCPNoHeight(), prevPos.GetDirection() );
		pResult = PutOnTerrain(pAnim);
		t = pAnim->GetTime();
	}
	else
	{
		pTerrainFunc->Move( cmdPos );
		// @0x73e068 -- bJumpBack picks the FACE-AWAY jump clips
		NDb::CAnimation::EType eHigh = bJumpBack ? NDb::CAnimation::JUMP_BACK_HIGH : NDb::CAnimation::JUMP_HIGH;
		NDb::CAnimation::EType eLow  = bJumpBack ? NDb::CAnimation::JUMP_BACK_LOW  : NDb::CAnimation::JUMP_LOW;
		float fHeightDiff = prevPos.GetCP().z - cmdPos.GetCP().z;
		if ( fHeightDiff > F_HIGH_2_HEIGHT ) // high jump
		{
			STime tFallLength = (STime)( (fHeightDiff - F_HIGH_2_HEIGHT) / F_FALL_SPEED * 1000 );

			NDb::CAnimation *pDbAnim = pSkeleton->GetAnimation( eHigh, 0, "Height2" );
			CPtr<NAnimation::CAnimation> pBegin = pAnimator->CreateAnimation( pDbAnim, tEnd );
			if ( !pBegin )
			{
				DefaultAction( cmdPos );
				return;
			}
			STime tFall = pBegin->GetTimeLabel1();
			float fPart = float(tFall - tEnd) / pBegin->GetTime();
			pBegin->SetStand( tEnd, prevPos.GetCP(), prevPos.GetDirection() );
			pAnimator->AddAnimator( tEnd, pBegin );

			CPtr<NAnimation::CAnimation> pMiddle = pAnimator->CreateAnimation( pDbAnim, tFall );
			pMiddle->SetTimeFunction( new NAnimation::CAFunctionConstant( fPart ) );
			pMiddle->SetMove( tFall, prevPos.GetCP(), prevPos.GetDirection(), CVec3(0,0,-F_FALL_SPEED), 0 );
			pAnimator->AddAnimator( tFall, pMiddle );

			CPtr<NAnimation::CAnimation> pEnd = pAnimator->CreateAnimation( pDbAnim, tEnd + tFallLength );
			pEnd->SetStand( tEnd, prevPos.GetCP() - CVec3( 0, 0, fHeightDiff - F_HIGH_2_HEIGHT ), prevPos.GetDirection() );
			pAnimator->AddAnimator( tFall + tFallLength, pEnd );

			tEnd += pBegin->GetTime() + tFallLength;
			pAnimator->AddMemorizer( tEnd );
			IdleOn( cmdPos );
			return;
		}

		float fFrom, fTo;
		const char *pszParams = 0;
		if ( fHeightDiff < F_HIGH_1_HEIGHT )
		{
			fFrom = F_LOW_HEIGHT;
			fTo = F_HIGH_1_HEIGHT;
			pszParams = "Height1";
		}
		else
		{
			fFrom = F_HIGH_1_HEIGHT;
			fTo = F_HIGH_2_HEIGHT;
			pszParams = "Height2";
		}
		CPtr<NAnimation::CAnimation> pFrom = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( eLow, 0, pszParams ), tEnd );
		CPtr<NAnimation::CAnimation> pTo = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( eHigh, 0, pszParams ), tEnd );
		if ( !pFrom || !pTo )
		{
			DefaultAction( cmdPos );
			return;
		}
		float fKoeff = (fHeightDiff - fFrom) / (fTo - fFrom);
		if ( fKoeff < 0 )
			fKoeff = 0;
		if ( fKoeff > 1 )
			fKoeff = 1;
		STime tFrom = pFrom->GetTime();
		STime tTo = pTo->GetTime();
		t = (STime)( (1 - fKoeff) * (float)tFrom + fKoeff * (float)tTo );
		pFrom->SetInterval( tEnd, tEnd + t );
		pTo->SetInterval( tEnd, tEnd + t );
		pFrom->SetStand( tEnd, prevPos.GetCP(), prevPos.GetDirection() );
		pTo->SetStand( tEnd, prevPos.GetCP(), prevPos.GetDirection() );
		CPtr<NAnimation::CAInterpolator> pMiddle = new NAnimation::CAInterpolator;
		pMiddle->pAnim1 = pFrom;
		pMiddle->pAnim2 = pTo;
		pMiddle->pFunc = new NAnimation::CAFunctionConstant( fKoeff );
		pResult = pMiddle;
	}

	pAnimator->AddAnimator( tEnd, pResult );
	tEnd += t;
	pAnimator->AddMemorizer( tEnd );
	IdleOn( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// !IsDead()
void CUnitAnimator::StartNewTurn( const NAI::SUnitPosition &cmdPos )
{
	if ( !bAimed && !bInactivePose && !IsValid( pCannon ) && !bHealing && !bLadder )
		Stand( cmdPos, bAimedStrafe );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::GetDeathType @0x33a660: rotate the killing-blow direction into the unit's facing
// frame and pick the directional death clip by quadrant (VNULL3 dir resolves to DEATH_LEFT -- retail
// FallAsIfDead @0x350770 passes VNULL3 here, the directional push arrives separately via the
// ProcessAttack corpse-push / AddImpulse bPlayDeath=false entries).
static NDb::CAnimation::EType GetDeathType( const NAI::SUnitPosition &cmdPos, const CVec3 &ptDir )
{
	float fFacing = cmdPos.GetDirection();
	float c = cos( fFacing ), s = sin( fFacing );
	float x = c * ptDir.x - s * ptDir.y;   // damage direction in the unit frame
	float y = c * ptDir.y + s * ptDir.x;
	if ( x >= y )
		return x + y > 0 ? NDb::CAnimation::DEATH_FRONT : NDb::CAnimation::DEATH_LEFT;
	return x + y > 0 ? NDb::CAnimation::DEATH_RIGHT : NDb::CAnimation::DEATH_BACK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x33bb90. The death CLIP is optional (bPlayDeath + idle gates + data), the ragdoll
// handoff (AddDynamics) is UNCONDITIONAL -- a death that skips it leaves the corpse frozen in
// its last animation pose with the render transform stuck at the death spot.
void CUnitAnimator::Die( const NAI::SUnitPosition &cmdPos, const CVec3 &ptDir, bool bPlayDeath,
	NWorld::CUnitServer *pServer, const CVec3 *pWhereImpact )
{
	bool bWasCannon = false;
	if ( IsValid( pCannon ) )
	{
		pCannon->SetCurrentUnit(0);
		pCannon = 0;
		bWasCannon = true;
	}
	STime tCur = pTime->GetValue();
	CPtr<NAnimation::CAnimation> pDeath;
	// retail clip gate adds !bInactivePose && bPlayDeath to the Jan03 idle tests
	if ( !bAimed && !bWalking && !bAimedStrafe && !bWasCannon && !bInactivePose && bPlayDeath )
	{
		// retail: directional death first (GetDeathType), plain DEATH when the directional
		// variant is absent from the skeleton's animation set (Ghidra mistyped the CPtr local;
		// disasm-confirmed the fallback condition is !IsValid(pDeath))
		pDeath = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( GetDeathType( cmdPos, ptDir ), nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ),
				tCur );
		if ( !IsValid( pDeath ) )
			pDeath = pAnimator->CreateAnimation(
				pSkeleton->GetAnimation( NDb::CAnimation::DEATH, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ),
					tCur );
		// retail enqueues the clip only on an INTEGRAL place (packed pos byte+2 bit0 == nIntegral --
		// a real tile, not a ladder step); otherwise the body goes straight to dynamics with ptDir.
		if ( pDeath && !cmdPos.pos.p.IsIntegral() )
			pDeath = 0;
	}
	STime tDeathTime = 0;
	if ( pDeath )
	{
		if ( fDeathFall <= 0 )
		{
			pDeath->SetStand( tCur, cmdPos.GetCPNoHeight(), cmdPos.GetDirection() );
			pAnimator->AddAnimator( tCur, PutOnTerrain(pDeath) );
		}
		else
		{
			// retail falling-death branch (@0x33bb90, disasm 0x73bd3f): the unit died while FALLING
			// (CUnitServer::ForcedMove @0x3c0b80 stored the pre-fall height into fDeathFall) -- the
			// death clip MOVES from (cp.x, cp.y, fDeathFall) straight down with velocity {0,0,-16}
			// (0xc1800000 disasm-verified) and zero angular velocity, and is added RAW (no
			// PutOnTerrain -- the corpse must not be snapped onto the terrain it is falling past).
			pDeath->SetMove( tCur, CVec3( cmdPos.GetCPNoHeight(), fDeathFall ), cmdPos.GetDirection(),
				CVec3( 0, 0, -16.0f ), 0 );
			pAnimator->AddAnimator( tCur, pDeath );
		}
		// retail does NOT advance to GetTimeLabel1() -- the dynamics window is the WHOLE clip
		// [tNow, tNow+len] so the ragdoll can take over mid-clip on collision; the old
		// label1-based window delayed the physics takeover past the clip end.
		tDeathTime = pDeath->GetTime();
	}

	// retail impact: VNULL3 while a clip is standing the body (it settles from the clip pose),
	// the raw impulse dir only on the clipless instant-ragdoll path.
	// ptWhereImpact = the WORLD impact point (the body's ground position). CParticleSkeleton::Init's
	// per-particle falloff is `2 - |last[i]_world - ptWhereImpact|`: particles NEAR the impact take the
	// strongest impulse, far ones taper off. The prior reconstruction reinterpret_cast'd the raw
	// SUnitPosition bytes (packed place + ptr) as the CVec3 -> a near-zero garbage point ~|worldPos|
	// away from the body, so the falloff went large-NEGATIVE and inverted the (always-upward) impulse
	// into a downward shove that drove the corpse straight through the floor on frame 1. That garbage
	// path was never exercised before (crouch/scripted deaths take the impact==VNULL3 branch, no
	// impulse). Feed the real impact point so the falloff behaves as designed (retail-observable: the
	// corpse ragdolls/launches instead of pancaking into the ground).
	pAnimator->AddDynamics( tCur, tCur + tDeathTime, pDeath ? VNULL3 : ptDir,
		pWhereImpact ? *pWhereImpact : cmdPos.GetCP(), pAIMap, pFloor, pServer, 0, fDeathFall > 0 );
	pAnimator->AddMemorizer( tCur + 10000 );
	IdleOff();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const char *pszActivateAnimParams[ NDb::N_ITEM_PLACES ] = 
{
	"BeltL1",
	"BeltR1",
	"BeltM1",
	"BeltMediumL1",
	"BeltMediumR1",
	"BeltMediumL2",
	"BeltMediumR2",
	"WaistBeltL1",
	"WaistBeltR1",
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::ActivateItem( const NAI::SUnitPosition &cmdPos,
	bool bHeavy, bool bBackpack, NDb::EItemPlace place,
	NDb::EWeaponType eAWT, bool bHide, bool bInstantly )
{
	Stand( cmdPos );
	// retail @0x33e800: bHide hides the just-activated item (bActiveItem=!bHide) and forces the unarmed
	// animation set (eAnimationWeaponType = bHide ? WT_DEFAULT : eAWT).
	bActiveItem = !bHide;
	if ( bHide )
		eAWT = NDb::WT_DEFAULT;
	bool bTransit = false;
	CPtr<NAnimation::CAnimation> pAnim;
	if ( bHeavy )
	{
		SetWeaponAnimation( eAWT );
		CalculateAnimFlags();
		if ( bBackpack )
			pAnim = pAnimator->CreateAnimation(
				pSkeleton->GetAnimation( NDb::CAnimation::GET_BACKPACK, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ),
					tEnd );
		else
			pAnim = pAnimator->CreateAnimation(
				pSkeleton->GetAnimation( NDb::CAnimation::ACTIVATE, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ),
					tEnd );
	}
	else
	{
		if ( bBackpack )
			pAnim = pAnimator->CreateAnimation(
				pSkeleton->GetAnimation( NDb::CAnimation::GET_BACKPACK, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ),
					tEnd );
		else
			pAnim = pAnimator->CreateAnimation(
				pSkeleton->GetAnimation( NDb::CAnimation::ACTIVATE, nAnimFlagsPoseWeapon, 
					pszActivateAnimParams[place], nAnimFlagsClassSex, pSide ), tEnd );
		SetWeaponAnimation( eAWT );
		CalculateAnimFlags();
		if ( eAnimationWeaponType != NDb::WT_DEFAULT )
			bTransit = true;
	}
	if ( !pAnim )
	{
		DefaultAction( cmdPos );
	}
	else
	{
		pAnim->SetStand( tEnd, cmdPos.GetCPNoHeight(), cmdPos.GetDirection() );
		pAnimator->AddAnimator( tEnd, PutOnTerrain(pAnim) );
		if ( bInstantly )
			pAnim->SetInterval( tEnd, tEnd + N_INSTANT_TIME );
		tEnd += pAnim->GetTime();
		tLabel1 = pAnim->GetTimeLabel1();
	}
	pAnimator->AddMemorizer( tEnd );
	if ( bTransit )
	{
		pAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::POSE, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ), 
			tEnd + N_DEFAULT_ACTIVATE_TIME );
		if ( pAnim )
		{
			pAnim->SetStand( tEnd, cmdPos.GetCPNoHeight(), cmdPos.GetDirection() );
			pAnimator->AddTransit( tEnd, tEnd + N_DEFAULT_ACTIVATE_TIME, PutOnTerrain(pAnim) );
			tEnd += N_DEFAULT_ACTIVATE_TIME;
			pAnimator->AddMemorizer( tEnd );
		}
	}
	IdleOn( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::DeactivateItem( const NAI::SUnitPosition &cmdPos, 
	bool bHeavy, bool bBackpack, NDb::EItemPlace place, bool bInstantly )
{
	Stand( cmdPos );
	bActiveItem = false;
	bool bTransit = false;
	STime tBegin = tEnd;
	CPtr<NAnimation::CAnimation> pAnim;
	if ( bHeavy )
	{
		if ( bBackpack )
			pAnim = pAnimator->CreateAnimation(
				pSkeleton->GetAnimation( NDb::CAnimation::PUT_BACKPACK, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ), tEnd );
		else
			pAnim = pAnimator->CreateAnimation(
				pSkeleton->GetAnimation( NDb::CAnimation::DEACTIVATE, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ), tEnd );
		SetWeaponAnimation( NDb::WT_DEFAULT );
		CalculateAnimFlags();
	}
	else
	{
		if ( eAnimationWeaponType != NDb::WT_DEFAULT )
		{
			bTransit = true;
			tEnd += N_DEFAULT_ACTIVATE_TIME;
		}
		SetWeaponAnimation( NDb::WT_DEFAULT );
		CalculateAnimFlags();
		if ( bBackpack )
			pAnim = pAnimator->CreateAnimation(
				pSkeleton->GetAnimation( NDb::CAnimation::PUT_BACKPACK, nAnimFlagsPoseWeapon, 0, nAnimFlagsClassSex, pSide ), tEnd );
		else
			pAnim = pAnimator->CreateAnimation(
				pSkeleton->GetAnimation( NDb::CAnimation::DEACTIVATE, nAnimFlagsPoseWeapon, 
					pszActivateAnimParams[place], nAnimFlagsClassSex, pSide ), tEnd );
	}
	if ( !pAnim )
	{
		DefaultAction( cmdPos );
	}
	else
	{
		pAnim->SetStand( tEnd, cmdPos.GetCPNoHeight(), cmdPos.GetDirection() );
		if ( bTransit )
			pAnimator->AddTransit( tBegin, tEnd, PutOnTerrain(pAnim) );
		else
			pAnimator->AddAnimator( tEnd, PutOnTerrain(pAnim) );
		if ( bInstantly )
			pAnim->SetInterval( tEnd, tEnd + N_INSTANT_TIME );
		tEnd += pAnim->GetTime();
		tLabel1 = pAnim->GetTimeLabel1();
	}
	pAnimator->AddMemorizer( tEnd );
	IdleOn( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::ThrowGrenade( const NAI::SUnitPosition &cmdPos, const CVec3 &target, int nSide, int nGrenadeSize )
{
	Stand( cmdPos );
	bActiveItem = false;
	CPtr<NAnimation::CAnimation> pAnim;

	const char *pszParams = 0;
	switch ( nSide )
	{
		case 0:
			pszParams = "F1";
			break;
		case 1:
			pszParams = "R1";
			break;
		case 2:
			pszParams = "L1";
			break;
	}

	pAnim = pAnimator->CreateAnimation(
		pSkeleton->GetAnimation( NDb::CAnimation::USE, nAnimFlagsPoseWeapon, pszParams, nAnimFlagsClassSex, pSide ), tEnd );
	if ( !pAnim )
	{
		DefaultAction( cmdPos );
		return;
	}
	pAnim->SetStand( tEnd, cmdPos.GetCPNoHeight(), cmdPos.GetDirection() );
	tLabel1 = pAnim->GetTimeLabel1();

	CVec3 shootDir = target - cmdPos.GetCP();
	Normalize(&shootDir);
	float fNextAngle = atan2( shootDir.y, shootDir.x );
	float fCurAngle = cmdPos.GetDirection();
	float fHorizAngle = SignumNormalizeAngleInRadian( fNextAngle - fCurAngle );

	CPtr<NAnimation::CATurnBody> pTurn = new NAnimation::CATurnBody( fHorizAngle );
	pTurn->pFunc = new NAnimation::CAFunctionLinear( tEnd, (tLabel1 + tEnd) / 2 );
	pTurn->pAnim = pAnim;
	pTurn->nBoneSpine = pAnimator->GetBoneIndex( "Spine" );
	pTurn->nBoneChest = pAnimator->GetBoneIndex( "Chest" );
	
	pAnimator->AddAnimator( tEnd, PutOnTerrain(pTurn) );
	tEnd += pAnim->GetTime();
	bWalking = true;
	Stand( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::ThrowKnife( const NAI::SUnitPosition &cmdPos, const CVec3 &target )
{
	Stand( cmdPos );
	bActiveItem = false;
	int nFlags = 0;
	switch ( pose )
	{
		case NAI::RUN:
		case NAI::WALK:
			nFlags |= NDb::CAnimation::POSE_STAND;
			break;
		case NAI::CROUCH:
			nFlags |= NDb::CAnimation::POSE_CROUCH;
			break;
		case NAI::CRAWL:
			nFlags |= NDb::CAnimation::POSE_CRAWL;
			break;
		default:
			ASSERT( 0 );
	}
	nFlags |= NDb::CAnimation::WEAPON_KNIFE;
	CPtr<NAnimation::CAnimation> pAnim = pAnimator->CreateAnimation(
		pSkeleton->GetAnimation( NDb::CAnimation::THROW_KNIFE, nFlags ), tEnd );
	if ( !pAnim )
	{
		DefaultAction( cmdPos );
		return;
	}
	pAnim->SetStand( tEnd, cmdPos.GetCPNoHeight(), cmdPos.GetDirection() );
	tLabel1 = pAnim->GetTimeLabel1();

	CVec3 shootDir = target - cmdPos.GetCP();
	Normalize(&shootDir);
	float fNextAngle = atan2( shootDir.y, shootDir.x );
	float fCurAngle = cmdPos.GetDirection();
	float fHorizAngle = SignumNormalizeAngleInRadian( fNextAngle - fCurAngle );

	CPtr<NAnimation::CATurnBody> pTurn = new NAnimation::CATurnBody( fHorizAngle );
	pTurn->pFunc = new NAnimation::CAFunctionLinear( tEnd, (tLabel1 + tEnd) / 2 );
	pTurn->pAnim = pAnim;
	pTurn->nBoneSpine = pAnimator->GetBoneIndex( "Spine" );
	pTurn->nBoneChest = pAnimator->GetBoneIndex( "Chest" );
	
	pAnimator->AddAnimator( tEnd, PutOnTerrain(pTurn) );
	tEnd += pAnim->GetTime();
	bWalking = true;
	Stand( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::Wound()
{
	if ( !pAnimator->IsFreezed() )
		return;
	STime t = pTime->GetValue();
	pAnimator->AddRandom( t, t + 500 );
	pAnimator->AddMemorizer( t + 500 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::PlayAnimation( const NAI::SUnitPosition &cmdPos, 
	NAnimation::CAnimation *pAnimation, bool bInstantly )
{
	Stand( cmdPos );
	if ( !IsValid( pAnimation ) )
	{
		DefaultAction( cmdPos );
		return;
	}
	//
	pAnimation->SetStand( tEnd, cmdPos.GetCPNoHeight(), cmdPos.GetDirection() );
	tLabel1 = pAnimation->GetTimeLabel1();
	pAnimator->AddAnimator( tEnd, PutOnTerrain( pAnimation ) );
	if ( bInstantly )
		pAnimation->SetInterval( tEnd, tEnd + 1 );
	tEnd += pAnimation->GetTime();
	pAnimator->AddMemorizer( tEnd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::PlayAnimation( const NAI::SUnitPosition &cmdPos, 
	int nType, const char *pszParams, bool bInstantly )
{
	CPtr<NAnimation::CAnimation> pAnim = pAnimator->CreateAnimation(
		pSkeleton->GetAnimation( (NDb::CAnimation::EType)nType, nAnimFlagsPoseWeapon, pszParams, 
			nAnimFlagsClassSex, pSide ), tEnd );
	PlayAnimation( cmdPos, pAnim, bInstantly );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x33f4c0: play the DB clip, then -- if bFreezeAfterLastFrame -- BAN the internal idle so
// the unit HOLDS the clip's last frame instead of returning to its (custom) idle when it ends. This
// is the flag scripted freeze poses ride (EFirst's wounded commander: 4438 fall-to-side, freeze=true).
// PlayAnimation -> Stand -> IdleOn CLEARS the bit, so the ban is set AFTER, matching retail order
// (@0x73f530 PlayAnimation, then @0x73f542 or byte[+0x4c],1). IdleOn on the next command lifts it.
void CUnitAnimator::PlayCustomAnimation( const NAI::SUnitPosition &cmdPos, int nDBAnimationID, bool bFreezeAfterLastFrame )
{
	CPtr<NAnimation::CAnimation> pAnim =
		pAnimator->CreateAnimation( NDb::GetDBAnimation( nDBAnimationID ), tEnd );
	if ( !IsValid( pAnim ) )
		return;
	PlayAnimation( cmdPos, pAnim );
	if ( bFreezeAfterLastFrame )
		IdleBan( NAnimation::E_INTERNAL_IDLE_OFF, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::OpenWindowDoor( const NAI::SUnitPosition &cmdPos )
{
	PlayAnimation( cmdPos, NDb::CAnimation::OPEN );
	IdleOn( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x3409d0 -- single-step clip, no trailing IdleOn/Stand
void CUnitAnimator::MoveOneStep( const NAI::SUnitPosition &cmdPos )
{
	PlayAnimation( cmdPos, NDb::CAnimation::MOVE_ONE_STEP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x33f5a0 -- kneel-and-place: MINE_TILE (ground mine) or MINE_OBJECT (object/door trap)
void CUnitAnimator::SetMine( const NAI::SUnitPosition &cmdPos, bool bMine )
{
	PlayAnimation( cmdPos, bMine ? NDb::CAnimation::MINE_TILE : NDb::CAnimation::MINE_OBJECT );
	IdleOn( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::Reload( const NAI::SUnitPosition &cmdPos )
{
	if ( IsValid( pCannon ) )
	{
		ReloadCannon( cmdPos );
		return;
	}
	PlayAnimation( cmdPos, NDb::CAnimation::RELOAD );
	IdleOn( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::TakeCorpse( const NAI::SUnitPosition &cmdPos )
{
	PlayAnimation( cmdPos, NDb::CAnimation::TAKE_CORPSE );
	bCorpse = true;
	IdleOff();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::DropCorpse( const NAI::SUnitPosition &cmdPos )
{
	PlayAnimation( cmdPos, NDb::CAnimation::DROP_CORPSE );
	bCorpse = false;
	IdleOn( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::BeTaken( CUnit *_pCarrier, CUnitAnimator *pTaker, CUnitServer *pServer )
{
	pCarrier = _pCarrier;
	bIsCarried = true;   // retail @0x33bea0 (read by the ProcessAttack corpse-push gate @0x350e20)
	STime t = pTime->GetValue();
	int nIndex;

	nIndex = pTaker->pAnimator->GetBoneIndex( "Corpse" );
	if ( nIndex < 0 )
		nIndex = 0;
	NAnimation::CABoneFilter *pChestAnim = new NAnimation::CABoneFilter;
	pChestAnim->pSource = pTaker->pAnimator;
	pChestAnim->nIndex = nIndex;
	// retail @0x33bea0: pServer (the corpse's own CUnitServer) -> CParticleSkeleton::pUnit.
	pAnimator->AddDynamics( t, t, VNULL3, VNULL3, pAIMap, pFloor, pServer, pChestAnim );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::BeDropped( CUnitServer *pServer )
{
	pCarrier = 0;
	bIsCarried = false;   // retail @0x33b1b0
	STime t = pTime->GetValue();
	// retail @0x33b1b0: pServer (the corpse's own CUnitServer) -> CParticleSkeleton::pUnit, so the
	// settling ragdoll's BeStopped -> WorldInformCorpseStop has a live unit instead of null.
	pAnimator->AddDynamics( t, t, VNULL3, VNULL3, pAIMap, pFloor, pServer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::ReturnCannonToDefault()
{
	if ( IsValid( pCannon ) )
	{
		CVec3 cannonPos = pCannon->GetPosition();
		float fCannonDir = pCannon->GetDirection();
		NDb::CSkeleton *pCSkeleton = pCannon->GetSkeleton();
		NAnimation::CSkeletonAnimator *pCAnimator = pCannon->GetSkeletonAnimator();

		CVec3 defaultDir = CVec3( cos( fCannonDir ), sin( fCannonDir ), 0 );
		CVec3 cross = aimDir ^ defaultDir;

		STime tTransit = asin( fabs(cross) ) / F_DEFAULT_AIM_SPEED * 1000;
		CPtr<NAnimation::CAnimation> pCannonAnim = pCAnimator->CreateAnimation(
			pCSkeleton->GetAnimation( NDb::CAnimation::POSE, 0 ), tEnd + tTransit );
		if ( pCannonAnim )
		{
			pCannonAnim->SetStand( tEnd + tTransit, cannonPos, fCannonDir );
			pCAnimator->AddTransit( tEnd, tEnd + tTransit, pCannonAnim, NAnimation::LINEAR );
			pCAnimator->AddMemorizer( tEnd + tTransit );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::AttachHandsToCannon()
{
	ASSERT( IsValid(pCannon) );
	NAnimation::CSkeletonAnimator *pCAnimator = pCannon->GetSkeletonAnimator();

	int nIndex = pCAnimator->GetBoneIndex( "L_Handle" ); //???
	if ( nIndex < 0 )
		nIndex = 0;
	NAnimation::CABoneFilter *pLEff = new NAnimation::CABoneFilter;
	pLEff->pSource = pCAnimator;
	pLEff->nIndex = nIndex;

	nIndex = pCAnimator->GetBoneIndex( "R_Handle" ); //???
	if ( nIndex < 0 )
		nIndex = 0;
	NAnimation::CABoneFilter *pREff = new NAnimation::CABoneFilter;
	pREff->pSource = pCAnimator;
	pREff->nIndex = nIndex;

	pAnimator->AddSimpleIK( tEnd, "L_Hand", pLEff );
	pAnimator->AddSimpleIK( tEnd, "R_Hand", pREff );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::EnterCannon( const NAI::SUnitPosition &cmdPos, CCannon *_pCannon )
{
	pCannon = _pCannon;
	ASSERT( IsValid(pCannon) );

	CVec3 cannonPos = pCannon->GetPosition();
	float fCannonDir = pCannon->GetDirection();
	NAnimation::CSkeletonAnimator *pCAnimator = pCannon->GetSkeletonAnimator();

	aimDir = CVec3( cos( fCannonDir ), sin( fCannonDir ), 0 );

	CPtr<NAnimation::CAnimation> pUnitAnim = pAnimator->CreateAnimation(
		pSkeleton->GetAnimation( NDb::CAnimation::START_ATTACK, nAnimFlagsPoseWeapon, 
			szWeaponName.c_str(), nAnimFlagsClassSex, pSide ), tEnd );
	if ( pUnitAnim )
	{
		CVec3 cannonPosHoNeight( cannonPos );
		cannonPosHoNeight.z = 0;
		pUnitAnim->SetStand( tEnd, cannonPosHoNeight, fCannonDir );
		pAnimator->AddAnimator( tEnd, PutOnTerrain(pUnitAnim) );
		tEnd += pUnitAnim->GetTime();
		AttachHandsToCannon();
	}
	pAnimator->AddMemorizer( tEnd );
	IdleOff();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::LeaveCannon( const NAI::SUnitPosition &cmdPos )
{
	ReturnCannonToDefault();
	pCannon = 0;
	bWalking = true;
	Stand( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::AttackCannon( const NAI::SUnitPosition &cmdPos, const CRay &ray, bool bInstant )
{
	ASSERT( IsValid( pCannon ) );

	CVec3 cannonPos = pCannon->GetPosition();
	float fCannonDir = pCannon->GetDirection();
	NDb::CSkeleton *pCSkeleton = pCannon->GetSkeleton();
	NAnimation::CSkeletonAnimator *pCAnimator = pCannon->GetSkeletonAnimator();

	CVec3 shootDir = ray.ptDir;
	Normalize(&shootDir);
	float fNextAngle = atan2( shootDir.y, shootDir.x );
	float fCurAngle = fCannonDir;

	STime tTransit = N_INSTANT_SHOOT_TIME;
	if ( !bInstant )
	{
		CVec3 cross = aimDir ^ shootDir;
		tTransit = asin( fabs(cross) ) / F_DEFAULT_AIM_SPEED * 1000;
	}

	aimDir = shootDir;

	// horizontal turn
	float fHorizAngle = SignumNormalizeAngleInRadian( fNextAngle - fCurAngle );
	float fHorizCoeff = Clamp( (fHorizAngle + FP_PI8) / FP_PI4, 0.0f, 1.0f );
	// vertical turn
	float fVertAngle = asin( shootDir.z );
	float fVertCoeff = Clamp( (fVertAngle + FP_PI8) / FP_PI4, 0.0f, 1.0f );

	CPtr<NAnimation::CAnimation> pLDAnim = pCAnimator->CreateAnimation(
		pCSkeleton->GetAnimation( NDb::CAnimation::ATTACK_LD, 0 ), tEnd + tTransit );
	CPtr<NAnimation::CAnimation> pLUAnim = pCAnimator->CreateAnimation(
		pCSkeleton->GetAnimation( NDb::CAnimation::ATTACK_LU, 0 ), tEnd + tTransit );
	CPtr<NAnimation::CAnimation> pRDAnim = pCAnimator->CreateAnimation(
		pCSkeleton->GetAnimation( NDb::CAnimation::ATTACK_RD, 0 ), tEnd + tTransit );
	CPtr<NAnimation::CAnimation> pRUAnim = pCAnimator->CreateAnimation(
		pCSkeleton->GetAnimation( NDb::CAnimation::ATTACK_RU, 0 ), tEnd + tTransit );
	if ( !pLDAnim || !pLUAnim || !pRDAnim || !pRUAnim )
	{
		DefaultAction( cmdPos );
		return;
	}
	tLabel1 = pLDAnim->GetTimeLabel1();
	pLDAnim->SetStand( tEnd + tTransit, cannonPos, fCannonDir );
	pLUAnim->SetStand( tEnd + tTransit, cannonPos, fCannonDir );
	pRDAnim->SetStand( tEnd + tTransit, cannonPos, fCannonDir );
	pRUAnim->SetStand( tEnd + tTransit, cannonPos, fCannonDir );

	CPtr<NAnimation::CAInterpolator> pLeft = new NAnimation::CAInterpolator;
	pLeft->pAnim1 = pLDAnim;
	pLeft->pAnim2 = pLUAnim;
	pLeft->pFunc = new NAnimation::CAFunctionConstant(fVertCoeff);
	CPtr<NAnimation::CAInterpolator> pRight = new NAnimation::CAInterpolator;
	pRight->pAnim1 = pRDAnim;
	pRight->pAnim2 = pRUAnim;
	pRight->pFunc = new NAnimation::CAFunctionConstant(fVertCoeff);
	CPtr<NAnimation::CAInterpolator> pResult = new NAnimation::CAInterpolator;
	pResult->pAnim1 = pRight;
	pResult->pAnim2 = pLeft;
	pResult->pFunc = new NAnimation::CAFunctionConstant(fHorizCoeff);

	pCAnimator->AddTransit( tEnd, tEnd + tTransit, pResult, NAnimation::PARABOLA );

	CPtr<NAnimation::CAnimation> pLDUnitAnim = pAnimator->CreateAnimation(
		pSkeleton->GetAnimation( NDb::CAnimation::ATTACK_LD, 0, szWeaponName.c_str() ), tEnd + tTransit );
	CPtr<NAnimation::CAnimation> pLUUnitAnim = pAnimator->CreateAnimation(
		pSkeleton->GetAnimation( NDb::CAnimation::ATTACK_LU, 0, szWeaponName.c_str() ), tEnd + tTransit );
	CPtr<NAnimation::CAnimation> pRDUnitAnim = pAnimator->CreateAnimation(
		pSkeleton->GetAnimation( NDb::CAnimation::ATTACK_RD, 0, szWeaponName.c_str() ), tEnd + tTransit );
	CPtr<NAnimation::CAnimation> pRUUnitAnim = pAnimator->CreateAnimation(
		pSkeleton->GetAnimation( NDb::CAnimation::ATTACK_RU, 0, szWeaponName.c_str() ), tEnd + tTransit );
	if ( pLDUnitAnim && pLUUnitAnim && pRDUnitAnim && pRUUnitAnim )
	{
		CVec3 cannonPosHoNeight( cannonPos );
		cannonPosHoNeight.z = 0;
		pLDUnitAnim->SetStand( tEnd + tTransit, cannonPosHoNeight, fCannonDir );
		pLUUnitAnim->SetStand( tEnd + tTransit, cannonPosHoNeight, fCannonDir );
		pRDUnitAnim->SetStand( tEnd + tTransit, cannonPosHoNeight, fCannonDir );
		pRUUnitAnim->SetStand( tEnd + tTransit, cannonPosHoNeight, fCannonDir );

		CPtr<NAnimation::CAInterpolator> pLeft = new NAnimation::CAInterpolator;
		pLeft->pAnim1 = pLDUnitAnim;
		pLeft->pAnim2 = pLUUnitAnim;
		pLeft->pFunc = new NAnimation::CAFunctionConstant(fVertCoeff);
		CPtr<NAnimation::CAInterpolator> pRight = new NAnimation::CAInterpolator;
		pRight->pAnim1 = pRDUnitAnim;
		pRight->pAnim2 = pRUUnitAnim;
		pRight->pFunc = new NAnimation::CAFunctionConstant(fVertCoeff);
		CPtr<NAnimation::CAInterpolator> pResult = new NAnimation::CAInterpolator;
		pResult->pAnim1 = pRight;
		pResult->pAnim2 = pLeft;
		pResult->pFunc = new NAnimation::CAFunctionConstant(fHorizCoeff);
		pAnimator->AddTransit( tEnd, tEnd + tTransit, PutOnTerrain(pResult) );

		AttachHandsToCannon();
	}

	tEnd += tTransit;
	tEnd += pLDAnim->GetTime();
	pCAnimator->AddMemorizer( tEnd );
	pAnimator->AddMemorizer( tEnd );
	IdleOff();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::ReloadCannon( const NAI::SUnitPosition &cmdPos )
{
	ASSERT( IsValid( pCannon ) );
	CVec3 cannonPos = pCannon->GetPosition();
	float fCannonDir = pCannon->GetDirection();
	NDb::CSkeleton *pCSkeleton = pCannon->GetSkeleton();
	NAnimation::CSkeletonAnimator *pCAnimator = pCannon->GetSkeletonAnimator();
	ReturnCannonToDefault();
	STime tTransit = N_DEFAULT_TRANSIT_TIME;
	CPtr<NAnimation::CAnimation> pAnim = pCAnimator->CreateAnimation(
		pCSkeleton->GetAnimation( NDb::CAnimation::RELOAD, 0 ), tEnd + tTransit );
	if ( pAnim )
	{
		pAnim->SetStand( tEnd + tTransit, cannonPos, fCannonDir );
		pCAnimator->AddAnimator( tEnd + tTransit, pAnim );
/*	
		CPtr<NAnimation::CAnimation> pCannonAnim = pCAnimator->CreateAnimation(
			pCSkeleton->GetAnimation( NDb::CAnimation::POSE, 0 ), tEnd + tTransit + pAnim->GetTime() );
		if ( pCannonAnim )
		{
			pCannonAnim->SetStand( tEnd + tTransit + pAnim->GetTime(), cannonPos, fCannonDir );
			pCAnimator->AddAnimator( tEnd + tTransit + pAnim->GetTime(), pCannonAnim );
		}
*/
	}
	CPtr<NAnimation::CAnimation> pUnitAnim = pAnimator->CreateAnimation(
		pSkeleton->GetAnimation( NDb::CAnimation::RELOAD, 0, szWeaponName.c_str() ), tEnd + tTransit );
	if ( pUnitAnim )
	{
		CVec3 cannonPosHoNeight( cannonPos );
		cannonPosHoNeight.z = 0;
		pUnitAnim->SetStand( tEnd + tTransit, cannonPosHoNeight, fCannonDir );
		pAnimator->AddTransit( tEnd, tEnd + tTransit, PutOnTerrain(pUnitAnim) );
		tEnd += tTransit;
		tEnd += pUnitAnim->GetTime();
		AttachHandsToCannon();
	}
	pCAnimator->AddMemorizer( tEnd );
	pAnimator->AddMemorizer( tEnd );
	IdleOff();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::StartHealing( const NAI::SUnitPosition &cmdPos, NAI::EBlowHeight eBHeight )
{
	bHealing = true;
	const char *pszParams = GetAnimHeightParams( cmdPos, eBHeight );
	if ( !pszParams )
	{
		ASSERT(0);
		DefaultAction( cmdPos );
		return;
	}
	PlayAnimation( cmdPos, NDb::CAnimation::START_HEAL, pszParams );
	HealOn( cmdPos, pszParams );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::StartHealing( const NAI::SUnitPosition &cmdPos, NAI::EBlowHeight eBHeight,
	bool bPanzerklein, bool bRepairKit )
{
	// @0x3401b0 -- power-armour REPAIR overload of StartHealing. When the heal target is a
	// Panzerklein (bPanzerklein) the clip params are the fixed PK-repair strings ("PKHealGer" if a
	// dedicated repair kit / bRepairKit is active, else "PKHealEng"); otherwise this is identical to
	// the 2-arg StartHealing above (height-derived params, DefaultAction on lookup failure).
	bHealing = true;
	const char *pszParams;
	if ( bPanzerklein )
		pszParams = bRepairKit ? "PKHealGer" : "PKHealEng";
	else
	{
		pszParams = GetAnimHeightParams( cmdPos, eBHeight );
		if ( !pszParams )
		{
			DefaultAction( cmdPos );   // bWalking=true; tLabel1=tEnd; Stand(cmdPos) -- matches decode inline
			return;
		}
	}
	PlayAnimation( cmdPos, NDb::CAnimation::START_HEAL, pszParams );
	HealOn( cmdPos, pszParams );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::FinishHealing( const NAI::SUnitPosition &cmdPos )
{
	bHealing = false;
	bWalking = true;
	Stand( cmdPos );
	IdleOn( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitAnimator::PlayLadderAnimation( const NAI::SUnitPosition &prevPos, int nType )
{
	CPtr<NAnimation::CAnimation> pAnim;
	pAnim = pAnimator->CreateAnimation( pSkeleton->GetAnimation( (NDb::CAnimation::EType)nType, 0 ), tEnd );
	if ( !pAnim )
		return false;
	pAnim->SetStand( tEnd, prevPos.GetCP(), prevPos.GetDirection() );
	tLabel1 = pAnim->GetTimeLabel1();
	pAnimator->AddAnimator( tEnd, pAnim );
	tEnd += pAnim->GetTime();
	pAnimator->AddMemorizer( tEnd );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::EnterLadder( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos, bool bUp )
{
	if ( !bUp )
		Stand( prevPos );
	bLadder = true;
	NDb::CAnimation::EType type = bUp ? NDb::CAnimation::ENTER_LADDER_UP : NDb::CAnimation::ENTER_LADDER_DOWN;
	if ( !PlayLadderAnimation( prevPos, type ) )
		DefaultAction( cmdPos );
	IdleOff();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::LeaveLadder( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos, bool bUp )
{
	bLadder = false;
	NDb::CAnimation::EType type = bUp ? NDb::CAnimation::LEAVE_LADDER_UP : NDb::CAnimation::LEAVE_LADDER_DOWN;
	pTerrainFunc->Move( cmdPos );
	if ( !PlayLadderAnimation( prevPos, type ) )
		DefaultAction( cmdPos );
	if ( bUp )
		IdleOff();
	else
	{
		pose = cmdPos.GetPose();
		CalculateAnimFlags();
		IdleOn( cmdPos );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::MoveLadder( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos )
{
	if ( cmdPos.GetCP().z > prevPos.GetCP().z )
	{
		// up
		if ( !PlayLadderAnimation( prevPos, NDb::CAnimation::MOVE_LADDER_UP ) )
			DefaultAction( cmdPos );
	}
	else
	{
		// down
		if ( !PlayLadderAnimation( cmdPos, NDb::CAnimation::MOVE_LADDER_DOWN ) )
			DefaultAction( cmdPos );
	}
	IdleOff();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::JumpFromLadder( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos )
{
	bLadder = false;
	float fHeightDiff = prevPos.GetCP().z - cmdPos.GetCP().z;
	if ( fHeightDiff < 0 )
		fHeightDiff = 0;

	NDb::CAnimation *pDbAnim = pSkeleton->GetAnimation( NDb::CAnimation::JUMP_LADDER, 0 );
	CPtr<NAnimation::CAnimation> pBegin = pAnimator->CreateAnimation( pDbAnim, tEnd );
	if ( !pBegin )
	{
		DefaultAction( cmdPos );
		return;
	}
	pBegin->SetStand( tEnd, prevPos.GetCP(), prevPos.GetDirection() );
	pAnimator->AddAnimator( tEnd, pBegin );

	if ( fHeightDiff > 0 )
	{
		STime tFallLength = (STime)( fHeightDiff / F_FALL_SPEED * 1000 );
		STime tFall = pBegin->GetTimeLabel1();
		float fPart = float(tFall - tEnd) / pBegin->GetTime();
		CPtr<NAnimation::CAnimation> pMiddle = pAnimator->CreateAnimation( pDbAnim, tFall );
		pMiddle->SetTimeFunction( new NAnimation::CAFunctionConstant( fPart ) );
		pMiddle->SetMove( tFall, prevPos.GetCP(), prevPos.GetDirection(), CVec3(0,0,-F_FALL_SPEED), 0 );
		pAnimator->AddAnimator( tFall, pMiddle );

		CPtr<NAnimation::CAnimation> pEnd = pAnimator->CreateAnimation( pDbAnim, tEnd + tFallLength );
		pEnd->SetStand( tEnd, prevPos.GetCP() - CVec3( 0, 0, fHeightDiff ), prevPos.GetDirection() );
		pAnimator->AddAnimator( tFall + tFallLength, pEnd );
		tEnd += tFallLength;
	}

	tEnd += pBegin->GetTime();
	pAnimator->AddMemorizer( tEnd );
	pose = cmdPos.GetPose();
	CalculateAnimFlags();
	IdleOn( cmdPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitAnimator::CanStandAimed( const NAI::SUnitPosition &cmdPos )
{
	NDb::CAnimWeaponType *pType = NDb::GetAnimWeaponType( nAnimFlagsPoseWeapon );
	if ( !pType || !IsValid(pAIMap) )
		return false;
	if ( !pType->bAimedStrafe )
		return false;
	CVec3 rel;
	switch ( cmdPos.GetPose() )
	{
		case NAI::CRAWL:
			rel = pType->crawl;
			break;
		case NAI::CROUCH:
			rel = pType->crouch;
			break;
		case NAI::WALK:
		case NAI::RUN:
			rel = pType->stand;
			break;
	}
	rel.y = 0; // CRAP - from unit center
	float fMinDistance = pType->fMinDistance;
	float fAngle = cmdPos.GetDirection();
	CQuat q( fAngle, CVec3(0,0,1) );
	CRay ray;
	ray.ptOrigin = cmdPos.GetCP() + q.Rotate(rel);
	ray.ptDir = CVec3( cos(fAngle), sin(fAngle), 0 );
	vector<NAI::SInterval> intersect;
	pAIMap->Trace( ray, &intersect, NWorld::TS_COVER );//OBJECTS );
	bool bObstacle = false;
	for ( vector<NAI::SInterval>::const_iterator i = intersect.begin(); i != intersect.end(); ++i )
	{
		if ( (i->pSrc->nTSFlags & NWorld::TS_WEAPON_BLOCKER)/* && i->pUserData != pIgnore*/ && i->enter.fT < fMinDistance )
		{
			bObstacle = true;
			break;
		}
	}
	return !bObstacle;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::InitAsCorpseCarrier( const NAI::SUnitPosition &pos )
{
	DeactivateItem( pos, false, false, NDb::BELT_M1, true );
	PlayAnimation( pos, NDb::CAnimation::TAKE_CORPSE, 0, true );
	bCorpse = true;
	IdleOff();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitAnimator::InitAsCorpse( const NAI::SUnitPosition &pos )
{
	IdleOff();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitAnimator::IsInstableCorpse()
{
	return pAnimator->IsInstableCorpse();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitAnimator::CalmCorpse()
{
	return pAnimator->CalmCorpse();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitAnimator::GetDynamicsFloor() const
{
	CDGPtr<CFuncBase<int> > pF( pFloor );
	pF.Refresh();
	return pF->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsTwoHeavy( NDb::EItemSubType subType, NDb::EItemSubType subTypeNext )
{
	if ( subTypeNext == NDb::SUBTYPE_HEAVY || subType == NDb::SUBTYPE_HEAVY )
		return true;
	if ( subTypeNext == NDb::SUBTYPE_MINE_DETECTOR || subType == NDb::SUBTYPE_MINE_DETECTOR )
		return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0x11461140, CUnitTerrain );
REGISTER_SAVELOAD_CLASS( 0x11461141, CHeightMapBlock );

