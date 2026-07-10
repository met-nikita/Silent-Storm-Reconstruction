#ifndef __wAnimation_H_
#define __wAnimation_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Time.h"
#include "aiPosition.h"
#include "GAnimation.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CGeometry;
	class CSkeleton;
	enum EWeaponType;
	enum ESlot;
	enum EItemPlace;
	enum EItemSubType;
	class CRPGPers;
	class CRPGClass;
	class CSide;
}
namespace NAnimation
{
	class CAnimator;
	class CAnimation;
	class CSkeletonAnimator;
	struct SBonePose;
}
namespace NAI
{
	class IAIMap;
	enum EBlowHeight;
}
namespace NRPG
{
	class IInventoryItem;
}
namespace NGScene
{
	class CCInt;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
class CUnitTerrain;
class CCannon;
class CUnit;
class CWorld;
class CUnitServer;   // Die's retail 4th arg (the dying server -> ragdoll pUnit)
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitAnimator
{
	ZDATA
	CObj<CUnitTerrain> pTerrainFunc;
	
	STime tEnd; // time when dynamic action will end
	int nAnimFlagsPoseWeapon;
	int nAnimFlagsClassSex;
	string szWeaponName;
	CVec3 hipShift;
	
	bool bIdle; 
	// movement vars
	bool bWalking;
	bool bStart;
	float fStartLeft;
	STime tAnim; // current animation time on tEnd
	CVec2 ptPrev;
	CVec2 ptPrevPrev; // :)
	// strafe vars
	bool bStrafing;
	bool bAimedStrafe;
	float fLookAngle;
	int nWalkAnim;
	// rotate vars
	float fRotateVel;
	// shoot vars
	bool bAimed;
	CVec3 aimDir;
	//
	bool bActiveItem;
	bool bInactivePose;
	bool bCorpse;
	bool bHealing;
	bool bLadder;
	
	// time labels
	STime tLabel1;
	vector<STime> tSteps;

	NDb::EWeaponType eAnimationWeaponType;
	NAI::EPose pose;
	CDBPtr<NDb::CSkeleton> pSkeleton;

	CObj<CFuncBase<STime> > pTime;
	CObj<NAnimation::CSkeletonAnimator> pAnimator;
	CObj<NAnimation::CSkeletonState> pAnimState;
	CPtr<CCannon> pCannon;
	CPtr<CUnit> pCarrier; // somebody who carries this unit
	CPtr<NAI::IAIMap> pAIMap;
	bool bHasPKSkeleton;
	CDBPtr<NDb::CRPGPers> pPers;
	CDBPtr<NDb::CSide> pSide;
	CDBPtr<NDb::CRPGClass> pUnitClass;
	CPtr<CWorld> pWorld;
	CObj<NGScene::CCInt> pFloor;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pTerrainFunc); f.Add(3,&tEnd); f.Add(4,&nAnimFlagsPoseWeapon); f.Add(5,&nAnimFlagsClassSex); f.Add(6,&szWeaponName); f.Add(7,&hipShift); f.Add(8,&bIdle); f.Add(9,&bWalking); f.Add(10,&bStart); f.Add(11,&fStartLeft); f.Add(12,&tAnim); f.Add(13,&ptPrev); f.Add(14,&ptPrevPrev); f.Add(15,&bStrafing); f.Add(16,&bAimedStrafe); f.Add(17,&fLookAngle); f.Add(18,&nWalkAnim); f.Add(19,&fRotateVel); f.Add(20,&bAimed); f.Add(21,&aimDir); f.Add(22,&bActiveItem); f.Add(23,&bInactivePose); f.Add(24,&bCorpse); f.Add(25,&bHealing); f.Add(26,&bLadder); f.Add(27,&tLabel1); f.Add(28,&tSteps); f.Add(29,&eAnimationWeaponType); f.Add(30,&pose); f.Add(31,&pSkeleton); f.Add(32,&pTime); f.Add(33,&pAnimator); f.Add(34,&pAnimState); f.Add(35,&pCannon); f.Add(36,&pCarrier); f.Add(37,&pAIMap); f.Add(38,&bHasPKSkeleton); f.Add(39,&pPers); f.Add(40,&pSide); f.Add(41,&pUnitClass); f.Add(42,&pWorld); f.Add(43,&pFloor); f.Add(44,&nSpecialPKFlags); f.Add(45,&bStandIfRecalcCommand); f.Add(46,&bIsCarried); f.Add(47,&fDeathFall); return 0; }
	int nSpecialPKFlags = 0;
	bool bStandIfRecalcCommand = false;
	bool bIsCarried = false;
	float fDeathFall = 0.0f;
private:
	//NAnimation::CAnimation* CreateAnimation( NDb::CAnimation::EType type, int nFlags, STime tStart );
	void PlayAnimation( const NAI::SUnitPosition &cmdPos, 
		NAnimation::CAnimation *pAnimation, bool bInstantly = false );
	void PlayAnimation( const NAI::SUnitPosition &cmdPos, 
		int nType, const char *pszParams = 0, bool bInstantly = false );
	void DefaultAction( const NAI::SUnitPosition &cmdPos );
	NAnimation::CAnimator* PutOnTerrain( NAnimation::CAnimator *pAnim );
	void Move( const NAI::SUnitPosition &prevPos, 
		const NAI::SUnitPosition &cmdPos, const NAI::SUnitPosition &nextPos, bool bEnd, bool bInterGrid );
	void StandStill( CVec2 &pos, float fAngle, bool bNeedStrafe = false );
	void Stand( const NAI::SUnitPosition &cmdPos, bool bNeedStrafe = false );
	void HealOn( const NAI::SUnitPosition &cmdPos, const char *pszParams );
	void IdleOn( const NAI::SUnitPosition &cmdPos );
	void IdleOff();
	void AttackCannon( const NAI::SUnitPosition &cmdPos, const CRay &ray, bool bInstant = false );
	void ReloadCannon( const NAI::SUnitPosition &cmdPos );
	void ReturnCannonToDefault();
	void AttachHandsToCannon();
	bool CanStandAimed( const NAI::SUnitPosition &cmdPos );
	bool PlayLadderAnimation( const NAI::SUnitPosition &prevPos, int nType );
public:
	CUnitAnimator() {}
	CUnitAnimator( CFuncBase<STime> *_pTime, const NAI::SUnitPosition &pos, NDb::CSkeleton *_pSkeleton, 
		NAI::IAIMap *pAIMap, NDb::CRPGPers *_pPers, CWorld *_pWorld );
	//
	NAnimation::CSkeletonAnimator* GetSkeletonAnimator() const { return pAnimator; }
	CFuncBase<NAnimation::SSkeletonState>* GetSkeletonState() const { return pAnimState; }
	//
	void PlaceUnit( const NAI::SUnitPosition &cmdPos );
	void Fly( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos ); // CExecFly mover (UnitFlyToWaypoint)
	void SetWeaponAnimation( NDb::EWeaponType eAWT ) { eAnimationWeaponType = eAWT; }
	void SetWeaponName( const char *pszName ) { szWeaponName = pszName;	}
	void SetActiveItem( bool _bActive ) { bActiveItem = _bActive;	}
	void SetPose( NAI::EPose _pose ) { pose = _pose; }
	// accessors
	void AlignTime( STime tDelay = 0 );
	STime GetTimeEnd() const { return tEnd; }
	STime GetTimeLabel1() const { return tLabel1; }
	bool GetHipPos( CVec3 *pRes );
	bool GetBarrelPos( NDb::CGeometry *pWeaponGeometry, NAnimation::SBonePose *pRes, bool bLeft );   // @0x33b720: bLeft picks L_Weapon vs R_Weapon on a PK skeleton
	bool IsAiming() const { return bAimed || bAimedStrafe; }
	bool IsStrafing() const { return bStrafing; }
	bool IsActiveItem() const { return bActiveItem; }
	bool IsCarryingCorpse() const { return bCorpse; }
	bool CanActivateItem() const { return !bCorpse && !bLadder && !bHealing && !bInactivePose; }
	CCannon* GetCannon() const { return pCannon; }
	CUnit* GetCorpseCarrier() const { return pCarrier; }
	// steps
	bool GetStepTime( STime *pT ) const;
	void DropStep();
	// commands
	void StartNewTurn( const NAI::SUnitPosition &cmdPos );
	void StartMove( bool bWalkStrafe );
	void EndMove( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos, bool bFreeze, bool bInterGrid );
	void Move( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos, const NAI::SUnitPosition &nextPos, bool bInterGrid );
	void Rotate( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos, bool bStart );
	void EndRotate( const NAI::SUnitPosition &cmdPos );
	void Attack( const NAI::SUnitPosition &cmdPos, const CRay &ray, bool bInstant = false, bool bNoShoot = false );
	void Snipe( const NAI::SUnitPosition &cmdPos, const CRay &ray );
	void CloseAttack( const NAI::SUnitPosition &cmdPos, NAI::EBlowHeight eHeight );
	void ChangePose( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos );
	void Climb( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos, bool bRealClimb );
	void Jump( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos, bool bRealJump );
	void Fall( const NAI::SUnitPosition &cmdPos, float fPrevHeight );
	void ForcedMove( const NAI::SUnitPosition &cmdPos );
	// retail @0x33bb90: bPlayDeath gates ONLY the death CLIP -- the ragdoll handoff (AddDynamics)
	// ALWAYS runs. bPlayDeath=false is the pure ragdoll-push entry (corpse push @0x350e20 /
	// blast impulse @0x3c0370 / SpillPK @0x3a62e0); true plays a directional death anim when idle.
	// pWhereImpact = the WORLD impact point that seeds CParticleSkeleton::Init's per-particle impulse
	// falloff (2 - |particle - impact|). NULL falls back to the body's ground position (cmdPos.GetCP());
	// the corpse-push passes the actual HIT-LOCATION bone so the ragdoll reacts to WHERE it was hit.
	void Die( const NAI::SUnitPosition &cmdPos, const CVec3 &ptDir, bool bPlayDeath = true,
		NWorld::CUnitServer *pServer = 0,     // retail 4th arg: the dying server (-> ragdoll pUnit)
		const CVec3 *pWhereImpact = 0 );
	void ActivateItem( const NAI::SUnitPosition &cmdPos,
		bool bHeavy, bool bBackpack, NDb::EItemPlace place,
		NDb::EWeaponType eAWT, bool bHide = false, bool bInstantly = false );   // retail @0x33e800 inserted bHide at slot 6
	void DeactivateItem( const NAI::SUnitPosition &cmdPos, bool bHeavy, 
		bool bBackpack, NDb::EItemPlace place, bool bInstantly = false );
	void ThrowGrenade( const NAI::SUnitPosition &cmdPos, const CVec3 &target, int nSide, int nGrenadeSize = 1 );
	void ThrowKnife( const NAI::SUnitPosition &cmdPos, const CVec3 &target );
	void OpenWindowDoor( const NAI::SUnitPosition &cmdPos );
	void Reload( const NAI::SUnitPosition &cmdPos );
	void StartHealing( const NAI::SUnitPosition &cmdPos, NAI::EBlowHeight eHeight );
	// @0x3401b0 -- power-armour REPAIR overload: bPanzerklein => fixed PK-heal clip params
	// ("PKHealGer" if a dedicated repair kit is active, else "PKHealEng"); else == the 2-arg form.
	void StartHealing( const NAI::SUnitPosition &cmdPos, NAI::EBlowHeight eHeight, bool bPanzerklein, bool bRepairKit );
	void FinishHealing( const NAI::SUnitPosition &cmdPos );
	// corpses
	void TakeCorpse( const NAI::SUnitPosition &cmdPos );
	void DropCorpse( const NAI::SUnitPosition &cmdPos );
	// retail @0x33bea0/@0x33b1b0 thread the CORPSE's own CUnitServer as the last arg -> AddDynamics
	// pServer -> CParticleSkeleton::pUnit, read by BeStopped -> WorldInformCorpseStop when the ragdoll
	// settles. Dropping this arg (hardcoding pServer=0) left pUnit null -> AV on the dropped-corpse settle.
	void BeTaken( CUnit *pCarrier, CUnitAnimator *pTaker, CUnitServer *pServer );
	void BeDropped( CUnitServer *pServer );
	// cannons
	void EnterCannon( const NAI::SUnitPosition &cmdPos, CCannon *pCannon );
	void LeaveCannon( const NAI::SUnitPosition &cmdPos );
	// ladders
	void EnterLadder( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos, bool bUp );
	void LeaveLadder( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos, bool bUp );
	void MoveLadder( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos );
	void JumpFromLadder( const NAI::SUnitPosition &prevPos, const NAI::SUnitPosition &cmdPos );
	//
	void Wound();
	void InitAsCorpseCarrier( const NAI::SUnitPosition &pos );
	void InitAsCorpse( const NAI::SUnitPosition &pos );
	//
	void IdleBan( char cBanSourceFlag, bool bBan ); 
	void ChangeSkeleton( NDb::CSkeleton *_pSkeleton, bool bBecomePanzerklein );	// For PK
	void PlayCustomAnimation( const NAI::SUnitPosition &cmdPos, int nDBAnimationID );
	bool IsInstableCorpse();
	bool CalmCorpse();
	void CalculateAnimFlags( bool bUseItemFlags = true );
	void SetBreathOnlyIdle( bool bOn = true );
	void SetCustomIdleAnimation( NDb::CAnimation *pIdleAnimation );
	int GetDynamicsFloor() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsTwoHeavy( NDb::EItemSubType subType, NDb::EItemSubType subTypeNext );
}
#endif
