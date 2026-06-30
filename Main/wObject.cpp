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
CWindowDoor::CWindowDoor( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
	NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime, const vector<int> &vCreateFlags, bool bOpen )
	: CAnimObjectServerBase( pWorld, pos, bLightMap, pO, pRPG, _pTime, vCreateFlags )
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
	return trap.pGrenade != 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CRPGItem* CWindowDoor::DisarmMine()
{
	NDb::CRPGGrenade *pRes = trap.pGrenade;
	trap.pGrenade = 0;
	if ( pRes )
		return pRes->pItem;
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
int CWindowDoor::ProcessAttack( int nUserID, NRPG::CAttackPortion *pAttack, NDb::CRPGArmor *pArmor )
{
	int nRes = CAnimObjectServerBase::ProcessAttack( nUserID, pAttack, pArmor );
	// ���� ����������� ����������� �� �������� ��������� � ������
	if ( pAttack->atkType != NRPG::AT_CLICK_OF_DEATH && !IsBroken() )
	{
		CDynamicCast<NAI::CPathNetwork> pNetwork( pWorld->GetPathNetwork() );
		NAI::CPathNetwork::SFlipper *pFlipper = pNetwork->GetFlipper( this );
		bool bTmpOpened = pFlipper->bOpen;
		// ���� ����������� �������, �� ������� ������
		if ( GetChangeStateDirection( !bTmpOpened ) * pAttack->rTtrajectory.ptDir > 0.001f )
		{
			OpenClose( !bTmpOpened, true );
		}
	}
	if ( trap.pGrenade )
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
	if ( trap.pGrenade )
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
void CWindowDoor::GoBoom( CUnitServer *pWho )
{
	ASSERT( trap.pGrenade );
	// whoops, stuff is pucked up
	if ( IsValid(trap.pGrenade) )
	{
		pWorld->AddGrenadeExplosion( GetMinePos(), trap.pGrenade, 0, CastToObjectBase( this ) );   // door is its own igniter -> 10x self-damage
		trap.pGrenade = 0;
		pWorld->RemoveMine( this );
		NScript::luaCallFunction( "OnMineTriggered", "p", IsValid( pWho ) ? CastToObjectBase( pWho ) : 0 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWindowDoor::SetTrap( NDb::CRPGGrenade *pGrenade, int nDC )
{
	if ( trap.pGrenade )
	{
		GoBoom();
		return false;
	}
	trap.pGrenade = pGrenade;
	trap.nDC = nDC;
	pWorld->GetAIMap()->GetUnitHLPos( &trap.vPos, pAIHull, -1 );
	pWorld->AddMine( this );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCannon
////////////////////////////////////////////////////////////////////////////////////////////////////
CCannon::CCannon( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
	NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime, const vector<int> &vCreateFlags )
	: CAnimObjectServerBase( pWorld, pos, bLightMap, pO, pRPG, _pTime, vCreateFlags )
{
	ASSERT( IsValid( pO->pGun ) );
	pItem = NRPG::CreateWeaponItem( pO->pGun->pWeapon );
	ASSERT( pItem );
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
	CAnimPassageObject( CWorld *pWorld, const SObjectPlace &pos, 
		bool bLightMap,	NDb::CObject *pO, NRPG::IObject *pRPG, 
		CFuncBase<STime> *_pTime, int _nPassageZoneID,	int _nPassageObjectID, int _nAPRadius, const vector<int> &vCreateFlags );
	// IObject
	virtual bool IsTargetable() const { return true; }
	// IPassageObject
	virtual bool IsBroken() const;
	virtual void GetObjectApproaches( vector<NAI::SPathPlace> *pApproaches );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAnimPassageObject::CAnimPassageObject( CWorld *pWorld, const SObjectPlace &pos, 
	bool bLightMap, NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime, 
	int _nPassageZoneID, int _nPassageObjectID, int _nAPRadius, const vector<int> &vCreateFlags ):
		CAnimObjectServerBase( pWorld, pos, bLightMap, pO, pRPG, _pTime, vCreateFlags ), 
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
IPassageObject *CreateAnimPassageObject( CWorld *pWorld, const SObjectPlace &pos, 
	bool bLightMap, NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime, 
	int _nPassageZoneID, int _nPassageObjectID, int _nAPRadius, const vector<int> &vCreateFlags )
{
	return new CAnimPassageObject( pWorld, pos, bLightMap, pO, 
		pRPG, _pTime, _nPassageZoneID, _nPassageObjectID, _nAPRadius, vCreateFlags  );
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
	CPassageObject( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
		NDb::CObject *pO, NRPG::IObject *pRPG, int _nPassageZoneID, int _nPassageObjectID, 
		int _nAPRadius, const vector<int> &vCreateFlags );
	// IObject
	virtual bool IsTargetable() const { return true; }
	// IPassageObject
	virtual bool IsBroken() const;
	virtual void GetObjectApproaches( vector<NAI::SPathPlace> *pApproaches );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CPassageObject::CPassageObject( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
	NDb::CObject *pO, NRPG::IObject *pRPG, int _nPassageZoneID, int _nPassageObjectID, 
	int _nAPRadius, const vector<int> &vCreateFlags ):
		CObjectServerBase( pWorld, pos, bLightMap, pO, pRPG, vCreateFlags ), 
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
IPassageObject *CreatePassageObject( CWorld *pWorld, const SObjectPlace &pos,	
	bool bLightMap, NDb::CObject *pO, NRPG::IObject *pRPG, int _nPassageZoneID, 
	int _nPassageObjectID, int _nAPRadius, const vector<int> &vCreateFlags )
{
	return new CPassageObject( pWorld, pos, bLightMap, pO, pRPG, 
		_nPassageZoneID, _nPassageObjectID, _nAPRadius, vCreateFlags );
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