#include "StdAfx.h"

#include "wObject.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\DBFormat\DataAnimation.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataSound.h"
#include "..\DBFormat\DataRPG.h"
#include "RPGObject.h"
#include "RPGItem.h"
#include "Transform.h"
#include "GAnimation.h"
#include "GAnimBase.h"
#include "wMain.h"
#include "wMainPath.h"
#include "aiGrid.h"
#include "wUnitMove.h"
#include "wOSBase.h"
#include "wUnitServer.h"
#include "aiNearestPosition.h"
#include "scriptCallLUA.h"
#include "aiMap.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWindowDoor
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail ctor @0x3823f0: eTimeOfDay sits between vCreateFlags and bOpen
CWindowDoor::CWindowDoor( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
	NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay, bool bOpen )
	: CAnimObjectServerBase( pWorld, pos, bLightMap, pO, pRPG, _pTime, vCreateFlags, eTimeOfDay )
{
	bIsOpen = bOpen;
	bIsLocked = false;
	nKeyID = 0;
	nLockHardness = 0;
	if ( bOpen )
	{
		STime t = pTime->GetValue();
		CPtr<NAnimation::CAnimation> pAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::DEACTIVATE, 0 ), t );
		if ( pAnim )
		{
			pAnim->SetStand( t, position.ptPos, position.fAngle );
			pAnimator->AddAnimator( t, pAnim );
			pAnimator->AddMemorizer( t );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CWindowDoor::GetMineDC()
{
	return trap.nDC;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CWindowDoor::GetMinePos()
{
	return trap.vPos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWindowDoor::IsMineSet()
{
	// retail: EITHER grenade slot arms the trap (this+0x10 pGrenade OR this+0x30 pEngGrenade); Jan03 only knew pGrenade
	return trap.pGrenade != 0 || trap.pEngGrenade != 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CRPGItem* CWindowDoor::DisarmMine()
{
	// retail @0x381870: take whichever slot is armed -- the plain grenade preferred, the eng grenade as
	// fallback -- clear it and hand back the contained item record.
	NDb::CRPGGrenade *pRes = trap.pGrenade;
	trap.pGrenade = 0;
	if ( pRes )
		return pRes->pItem;
	NDb::CRPGEngGrenade *pEngRes = trap.pEngGrenade;
	trap.pEngGrenade = 0;
	if ( pEngRes )
		return pEngRes->pItem;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWindowDoor::IsBroken() const
{
	NDb::CContainerModel *pCont = pDbObject->pModels[nDestroyStage];
	if ( pCont )
	{
		NDb::CModel *pModel = pCont->pModel;
		if ( pModel && pModel->pSkeleton == pSkeleton )
			return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CWindowDoor::GetChangeStateDirection( bool bOpen ) const
{
	CVec3 ptDir = VNULL3;
	CDynamicCast<NAI::CPathNetwork> pNetwork( pWorld->GetPathNetwork() );
	NAI::CPathNetwork::SFlipper *pFlipper = 0;
	pFlipper = pNetwork->GetFlipper( this );
	if ( pFlipper != 0 && !pFlipper->locksOpen.empty() && !pFlipper->locksClosed.empty() )
	{
		//CRAP{
		bool bBigGates = pFlipper->locksOpen.size() > 100 || pFlipper->locksClosed.size() > 100;
		//CRAP}
		if ( !bBigGates )
		{
			CVec3 ptOpened, ptClosed;
			ptOpened = pNetwork->GetCP( pFlipper->locksOpen.begin()->first );
			ptClosed = pNetwork->GetCP( pFlipper->locksClosed.begin()->first );
			if ( bOpen )
				ptDir = ptOpened - ptClosed;
			else	
				ptDir = ptClosed - ptOpened;
		}
	}
	return ptDir;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x382050 order (disasm-decoded 2026-07-11): (1) the shove-open branch runs FIRST -- before the
// base damage -- so it reads the PRE-damage IsBroken state, and its gate is dot(normalized attack dir,
// GetChangeStateDirection(!bOpen)) > 0.707 (0x3f34fdf4; the old Jan03 0.001 threshold flipped doors on
// near-perpendicular hits); a shove detonates the trap inside OpenClose. (2) base damage/destroy stages.
// (3) IsMineSet() -> GoBoom(NULL) UNCONDITIONALLY (any hit on an armed door blows the trap -- EITHER slot).
// retail @0x382050 resolves the path network off this->pWorld, NOT the argument (disasm 0x782082
// is a member load) -- hence _pWorld/_vDir, so neither shadows the member nor the local vDir.
int CWindowDoor::ProcessAttack( NWorld::IWorld *_pWorld, int nUserID, NRPG::CAttackPortion *pAttack,
	const CVec3 &_vDir, NDb::CRPGArmor *pArmor )
{
	if ( pAttack->atkType != NRPG::AT_CLICK_OF_DEATH && !IsBroken() )
	{
		CDynamicCast<NAI::CPathNetwork> pNetwork( pWorld->GetPathNetwork() );
		NAI::CPathNetwork::SFlipper *pFlipper = pNetwork->GetFlipper( this );
		bool bTmpOpened = pFlipper->bOpen;
		// LEFT AS DEV'S: retail reads _vDir here; its CAttackPortion has no rTtrajectory. Rerouting
		// needs that member removed tree-wide -- separate leg, see dossier.
		CVec3 vDir = pAttack->rTtrajectory.ptDir;
		Normalize( &vDir );
		// if the direction is good, move the door
		if ( GetChangeStateDirection( !bTmpOpened ) * vDir > 0.707f )
		{
			OpenClose( !bTmpOpened, true );
		}
	}
	int nRes = CAnimObjectServerBase::ProcessAttack( _pWorld, nUserID, pAttack, _vDir, pArmor );
	if ( IsMineSet() )
		GoBoom();
	//
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindowDoor::OpenClose( bool bOpen, bool bAbruptly, CUnitServer *pWho )
{
	pUser = pWho;
	const STime tAbruptly = 100;
	//
	if ( IsLockedDoor() )
	{
		ASSERT(0);
		return;
	}
	if ( IsBroken() )
		return;
	if ( bOpen == bIsOpen )
		return;
	if ( IsMineSet() )   // EITHER slot: opening a TNT-trapped door detonates it (plain-slot-only left eng traps inert)
		GoBoom( pWho );
	bIsOpen = bOpen;
	tEnd = pTime->GetValue();
	CPtr<NAnimation::CAnimation> pAnim;
	static SRand rnd;
	if ( bOpen )
	{
		if ( !bAbruptly && pDbObject->pDoor->pOpenSound )
		{
			NDb::CSoundVariant *p = pDbObject->pDoor->pOpenSound->GetSound( &rnd, GetCreateFlags() );
			if ( p )
				pWorld->MakeSound( position.ptPos, p->pSound );
		}
		pAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::ACTIVATE, 0 ), tEnd );
		//pCurrentWorld->Add3DSound( NDb::GetSound( 13 ), new NGScene::CCFBTransform( position ), 0 );
	}
	else
	{
		if ( !bAbruptly && pDbObject->pDoor->pCloseSound )
		{
			NDb::CSoundVariant *p = pDbObject->pDoor->pCloseSound->GetSound( &rnd, GetCreateFlags() );
			if ( p )
				pWorld->MakeSound( position.ptPos, p->pSound );
		}
		pAnim = pAnimator->CreateAnimation(
			pSkeleton->GetAnimation( NDb::CAnimation::DEACTIVATE, 0 ), tEnd );
		//pCurrentWorld->Add3DSound( NDb::GetSound( 14 ), new NGScene::CCFBTransform( position ), 0 );
	}
	if ( pAnim )
	{
		pAnim->SetStand( tEnd, position.ptPos, position.fAngle );
		pAnimator->AddAnimator( tEnd, pAnim );
		if ( bAbruptly )
			pAnim->SetInterval( tEnd, tEnd + tAbruptly );
		tEnd += pAnim->GetTime();
		pAction = pWorld->GetActiveCounter( 10 );
		pAnimator->AddMemorizer( tEnd );
	}
	pWorld->GetPathNetwork()->FlipperOpenClose( this, bOpen );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3813b0: lock/unlock the door and, WHEN LOCKING, record the required-key id and the
// lockpick difficulty passed from the map script (disasm: bIsLocked=bLock; LockUnlockFlipper; then
// `if(bLock){ nKeyID@220=param2; nLockHardness@224=param3; }`). The dev previously took only the bool
// and silently dropped both ints.
void CWindowDoor::LockDoor( bool bLock, int _nKeyID, int _nLockHardness )
{
	bIsLocked = bLock;
	pWorld->GetPathNetwork()->LockUnlockFlipper( this, bLock );
	if ( bLock )
	{
		nKeyID = _nKeyID;
		nLockHardness = _nLockHardness;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWindowDoor::Segment()
{
	STime tCur = pTime->GetValue();
	if ( tCur > tEnd && IsValid( pAction ) )
	{
		pAction = 0;
		pWorld->UpdateVisible();
		//
		if ( bIsOpen )
			NScript::luaCallFunction( "OnOpenObject", "pp", pUser.GetBarePtr(), CastToObjectBase(this) );
		else
			NScript::luaCallFunction( "OnCloseObject", "pp", pUser.GetBarePtr(), CastToObjectBase(this) );
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindowDoor::Visit( IAIVisitor *p )
{
	nDestroyStage = pRPG->GetDestroyStage();
	SFBTransform rv;
	CreateTransform( &rv );

	NDb::CContainerModel *pCont = pDbObject->pModels[nDestroyStage];
	if ( pCont )
	{
		NDb::CModel *pModel = pCont->pModel;
		if ( pModel && pModel->pGeometry && pModel->pGeometry->pAIGeometry )
		{
			int nMask = GetMask( pModel->pGeometry->pAIGeometry, pModel->pRPGArmor ) | TS_PICK;
			// retail CWindowDoor_Visit (s2_wobject.h:521-524): a locked door/window marks its hull
			// so the stability catcher query never treats it as wreckage support (Jan03 only OR'd TS_PICK)
			if ( bIsLocked )
				nMask |= TS_LOCKED_EXTRA;
			if ( pModel->pSkeleton == pSkeleton )
			{
				NAnimation::CAnimation *pAnim1, *pAnim2;
				// this is a crap animator made to get only one frame #0 ( i.e. when door is closed ) 
				CPtr<NAnimation::CSkeletonAnimator> pAn1 = new NAnimation::CSkeletonAnimator( pSkeleton );
				STime current = pTime->GetValue();
				pAn1->pTime = new CCTime( current );
				pAnim1 = pAn1->CreateAnimation(
					pSkeleton->GetAnimation( NDb::CAnimation::ACTIVATE, 0 ), current );
				if ( pAnim1 )
				{
					pAnim1->SetStand( current, CVec3(0,0,0), 0 );
					pAn1->AddAnimator( current, pAnim1 );
				}
				// this is a crap animator made to get only one frame #0 ( i.e. when door is open )
				CPtr<NAnimation::CSkeletonAnimator> pAn2 = new NAnimation::CSkeletonAnimator( pSkeleton );
				pAn2->pTime = new CCTime( current );
				pAnim2 = pAn2->CreateAnimation(
					pSkeleton->GetAnimation( NDb::CAnimation::DEACTIVATE, 0 ), current );
				if ( pAnim2 )
				{
					pAnim2->SetStand( current, CVec3(0,0,0), 0 );
					pAn2->AddAnimator( current, pAnim2 );
				}
				pAIHull = p->AddFlippingHull( pModel->pGeometry->pAIGeometry, pSkeleton, rv, pAn1, pAn2, pModel->pRPGArmor, 
					position.nFloor, nMask, bIsOpen, pDbObject->pDoor->GetRecordID(), nDestroyStage );
			}
			else
				pAIHull = p->AddHull( pModel->pGeometry->pAIGeometry, rv, pModel->pRPGArmor, position.nFloor, nMask );
		}
		CObjectServerBase::AddEffects( p, pCont, rv );
	}
	else
		pAIHull = 0;
	PrecacheAIGeom( p );
	
	if ( IsValid( pDbObject->pChild ) )
		AddObject( p, pDbObject->pChild, rv );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x381d60: the detonation gate is IsMineSet() (EITHER grenade slot live); a plain grenade goes out
// through the regular AddGrenadeExplosion (world vtbl+0x124), an engineer grenade through the eng overload
// (vtbl+0x120) with the stored placer eng skill; afterwards BOTH slots are released and the perk mine-modifiers
// reset to the {1,1,false} defaults.
void CWindowDoor::GoBoom( CUnitServer *pWho )
{
	ASSERT( IsMineSet() );
	// whoops, stuff is pucked up
	if ( IsValid(trap.pGrenade) || IsValid(trap.pEngGrenade) )
	{
		if ( IsValid(trap.pGrenade) )
			pWorld->AddGrenadeExplosion( GetMinePos(), trap.pGrenade, 0, CastToObjectBase( this ), &trap.sMineModifiers );   // door is its own igniter -> 10x self-damage; carry the placer's perk mods (retail @0x381d60)
		else
			pWorld->AddGrenadeExplosion( GetMinePos(), trap.pEngGrenade, trap.nEngSkill, 0, CastToObjectBase( this ), &trap.sMineModifiers );   // retail @0x381d60: eng blast via world vtbl+0x120 with trap.nEngSkill, thrower=0
		trap.pGrenade = 0;
		trap.pEngGrenade = 0;
		trap.sMineModifiers = SPerkMineModifiers();   // retail resets the carried perk mods to {1,1,false}
		pWorld->RemoveMine( this );
		NScript::luaCallFunction( "OnMineTriggered", "p", IsValid( pWho ) ? CastToObjectBase( pWho ) : 0 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWindowDoor::SetTrap( NDb::CRPGGrenade *pGrenade, int nDC, const SPerkMineModifiers *pMods )
{
	if ( IsMineSet() )
	{
		GoBoom();
		return false;
	}
	trap.pGrenade = pGrenade;
	trap.nDC = nDC;
	if ( pMods )
		trap.sMineModifiers = *pMods;   // retail @0x381ee0: the trapped door carries the placer's explosive-perk mods (map traps have none -> keep the {1,1,false} default)
	// retail @0x381ee0 resolves the door's hull through the user-hulls tracker (pAIHull is a
	// creation-path cache that a LOADED door doesn't have); GetHull scans by src.pUserData.
	pWorld->GetAIMap()->GetUnitHLPos( &trap.vPos, pWorld->GetAIMap()->GetHull( CastToObjectBase( this ) ), -1 );
	pWorld->AddMine( this );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x381f90: the engineer-grenade overload -- identical to @0x381ee0 except the record slot and that
// the placer's eng skill is recorded (it scales the blast at detonation: waves/radius/damage/fragments).
bool CWindowDoor::SetTrap( NDb::CRPGEngGrenade *pEngGrenade, int nDC, const SPerkMineModifiers *pMods, int nEngSkill )
{
	if ( IsMineSet() )
	{
		GoBoom();
		return false;
	}
	trap.pEngGrenade = pEngGrenade;
	trap.nDC = nDC;
	if ( pMods )
		trap.sMineModifiers = *pMods;
	trap.nEngSkill = nEngSkill;
	// retail @0x381ee0 resolves the door's hull through the user-hulls tracker (pAIHull is a
	// creation-path cache that a LOADED door doesn't have); GetHull scans by src.pUserData.
	pWorld->GetAIMap()->GetUnitHLPos( &trap.vPos, pWorld->GetAIMap()->GetHull( CastToObjectBase( this ) ), -1 );
	pWorld->AddMine( this );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCannon
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail ctor @0x382610: eTimeOfDay appended after vCreateFlags
CCannon::CCannon( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
	NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay )
	: CAnimObjectServerBase( pWorld, pos, bLightMap, pO, pRPG, _pTime, vCreateFlags, eTimeOfDay )
{
	ASSERT( IsValid( pO->pGun ) );
	pItem = NRPG::CreateWeaponItem( pO->pGun->pWeapon );
	ASSERT( pItem );
	// retail ctor @0x382610 tail: seed the clear distance and the barrel muzzle offset from the
	// DB gun record (NDb::CGun +0x18 = fMinClearDist, +0x1c..+0x24 = ptCannonAttackOrig).
	fMinClearDistance = pO->pGun->fMinClearDist;
	ptCannonAttackOrig = pO->pGun->ptCannonAttackOrig;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CCannon::GetPosition()
{
	return position.ptPos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CCannon::GetDirection()
{
	return position.fAngle;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCannon::IsBroken() const
{
	return nDestroyStage > 0;//bBroken;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPassageObjectBase
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPassageObjectBase: public IPassageObject
{
	ZDATA
	int nPassageZoneID;
	int nPassageObjectID;
	int nAPRadius;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nPassageZoneID); f.Add(3,&nPassageObjectID); f.Add(4,&nAPRadius); return 0; }
	//
	CPassageObjectBase() {}
	CPassageObjectBase( int _nPassageZoneID,	int _nPassageObjectID, int _nAPRadius );
	// IPassageObject
	virtual bool CanPass( CUnitServer *pUS );
	virtual int GetAPRadius() const { return nAPRadius; }
	virtual int GetPassageZoneID() const { return nPassageZoneID; }
	virtual int GetPassageObjectID() const { return nPassageObjectID; }
	virtual bool UsePassageObject( CUnitServer *pUS );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CPassageObjectBase::CPassageObjectBase( int _nPassageZoneID,
	int _nPassageObjectID, int _nAPRadius ):
	nPassageZoneID( _nPassageZoneID ), nPassageObjectID( _nPassageObjectID ), nAPRadius( _nAPRadius )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPassageObjectBase::CanPass( CUnitServer *pUS )
{
	vector<NAI::SPathPlace> approaches;
	GetObjectApproaches( &approaches );
	CPtr<NAI::CPath> pPath = FindPath( pUS->GetWorld()->GetPathNetwork(), pUS,
		pUS->GetPosition().pos.p, approaches, pUS, false, NAI::PF_DEFAULT, false, false, true );
	if ( IsValid( pPath ) )
	{
		CPtr<CCommandExecute> pExec = CreateSimpleMoveExecutor( pUS, pPath, NAI::PF_DEFAULT );
		if ( IsValid( pExec ) )
			return pExec->GetActionAP() <= nAPRadius;
	}
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPassageObjectBase::UsePassageObject( CUnitServer *pUS )
{
	ASSERT( IsValid( pUS ) );
	if ( !IsValid( pUS ) )
		return false;
	//
	if ( !IsBroken() )
	{
		bool bRes = pUS->GetWorld()->UsePassageObject( pUS, nPassageZoneID );
		if ( !bRes )
			OutputDebugString( "[PASSAGE] Can't use passage\n" );
		else
			OutputDebugString( "[PASSAGE] Passage activated\n" );
		return bRes;
	}
	else
	{
		OutputDebugString( "[PASSAGE] Passage broken\n" );
		return false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAnimPassageObject
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAnimPassageObject: public CAnimObjectServerBase, public CPassageObjectBase
{
	OBJECT_NOCOPY_METHODS( CAnimPassageObject );
	ZDATA
	ZPARENT( CAnimObjectServerBase );
	ZPARENT( CPassageObjectBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAnimObjectServerBase *)this); f.Add(3,(CPassageObjectBase *)this); return 0; }
	//
public:
	CAnimPassageObject() {}
	// retail ctor @0x3827d0: eTimeOfDay appended after vCreateFlags
	CAnimPassageObject( CWorld *pWorld, const SObjectPlace &pos,
		bool bLightMap,	NDb::CObject *pO, NRPG::IObject *pRPG,
		CFuncBase<STime> *_pTime, int _nPassageZoneID,	int _nPassageObjectID, int _nAPRadius, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay );
	// IObject
	virtual bool IsTargetable() const { return true; }
	// IPassageObject
	virtual bool IsBroken() const;
	virtual void GetObjectApproaches( vector<NAI::SPathPlace> *pApproaches );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAnimPassageObject::CAnimPassageObject( CWorld *pWorld, const SObjectPlace &pos,
	bool bLightMap, NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime,
	int _nPassageZoneID, int _nPassageObjectID, int _nAPRadius, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay ):
		CAnimObjectServerBase( pWorld, pos, bLightMap, pO, pRPG, _pTime, vCreateFlags, eTimeOfDay ),
		CPassageObjectBase( _nPassageZoneID, _nPassageObjectID, _nAPRadius )
{
	ASSERT( IsValid( pWorld ) );
	ASSERT( IsValid( pO ) );
	ASSERT( IsValid( pO->pPassage ) );
	ASSERT( IsValid( pRPG ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimPassageObject::GetObjectApproaches( vector<NAI::SPathPlace> *pApproaches )
{
	GetApproaches( pApproaches, GetWorld()->GetPathNetwork() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAnimPassageObject::IsBroken() const
{
	return nDestroyStage > 0;//bBroken;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x382930: threads the element's eTimeOfDay through to the ctor
IPassageObject *CreateAnimPassageObject( CWorld *pWorld, const SObjectPlace &pos,
	bool bLightMap, NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime,
	int _nPassageZoneID, int _nPassageObjectID, int _nAPRadius, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay )
{
	return new CAnimPassageObject( pWorld, pos, bLightMap, pO,
		pRPG, _pTime, _nPassageZoneID, _nPassageObjectID, _nAPRadius, vCreateFlags, eTimeOfDay );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPassageObject
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPassageObject: public CObjectServerBase, public CPassageObjectBase
{
	OBJECT_NOCOPY_METHODS( CPassageObject );
	ZDATA
	ZPARENT( CObjectServerBase );
	ZPARENT( CPassageObjectBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CObjectServerBase *)this); f.Add(3,(CPassageObjectBase *)this); return 0; }
	//
public:
	CPassageObject() {}
	// retail ctor @0x3829e0: eTimeOfDay appended after vCreateFlags
	CPassageObject( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
		NDb::CObject *pO, NRPG::IObject *pRPG, int _nPassageZoneID, int _nPassageObjectID,
		int _nAPRadius, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay );
	// IObject
	virtual bool IsTargetable() const { return true; }
	// IPassageObject
	virtual bool IsBroken() const;
	virtual void GetObjectApproaches( vector<NAI::SPathPlace> *pApproaches );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CPassageObject::CPassageObject( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
	NDb::CObject *pO, NRPG::IObject *pRPG, int _nPassageZoneID, int _nPassageObjectID,
	int _nAPRadius, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay ):
		CObjectServerBase( pWorld, pos, bLightMap, pO, pRPG, vCreateFlags, eTimeOfDay ),
		CPassageObjectBase( _nPassageZoneID, _nPassageObjectID, _nAPRadius )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPassageObject::IsBroken() const
{
	return nDestroyStage > 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPassageObject::GetObjectApproaches( vector<NAI::SPathPlace> *pApproaches )
{
	ASSERT( pApproaches != 0 );
	if ( pApproaches == 0 )
		return;
	//
	NAI::SPosition pos = NAI::GetNearestPosition( position.ptPos, pWorld->GetPathNetwork() );
	pos.p.SetPose( NAI::CM_STAND );
	pApproaches->push_back( pos.p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x382b00: threads the element's eTimeOfDay through to the ctor
IPassageObject *CreatePassageObject( CWorld *pWorld, const SObjectPlace &pos,
	bool bLightMap, NDb::CObject *pO, NRPG::IObject *pRPG, int _nPassageZoneID,
	int _nPassageObjectID, int _nAPRadius, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay )
{
	return new CPassageObject( pWorld, pos, bLightMap, pO, pRPG,
		_nPassageZoneID, _nPassageObjectID, _nAPRadius, vCreateFlags, eTimeOfDay );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NWorld;
//
REGISTER_SAVELOAD_CLASS( 0x11291180, CObjectServer )
REGISTER_SAVELOAD_CLASS( 0x12512140, CWindowDoor )
REGISTER_SAVELOAD_CLASS( 0x12512141, CCannon )
REGISTER_SAVELOAD_CLASS( 0x11122120, CAnimObjectServer )
REGISTER_SAVELOAD_CLASS( 0x51892141, CAnimPassageObject )
REGISTER_SAVELOAD_CLASS( 0x51412110, CPassageObject )