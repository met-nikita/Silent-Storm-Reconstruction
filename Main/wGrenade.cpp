#include "StdAfx.h"
#include "wGrenade.h"
#include "wInterfaceVisitors.h"
#include "Sync.h"
#include "wDynObject.h"
#include "wMain.h"
#include "GAnimation.h"
#include "GAnimParticles.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\DBFormat\DataRPG.h"
#include "aiMap.h"
#include "RPGAttackMech.h"

namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGrenadeServer : public IDynamicObject, public IVisObj
{
	OBJECT_NOCOPY_METHODS(CGrenadeServer);
	ZDATA
	STime tExplode;
	CDGPtr<NAnimation::CSkeletonAnimator> pAnimator;
	CPtr<NDb::CModel> pModel;
	CPtr<CWorld> pWorld;
	CSyncSrcBind<IVisObj> bindGlobal;
	CDBPtr<NDb::CRPGGrenade> pRPGGrenade;
	CObj<CActionCounter> pAction;
	CPtr<CUnitServer> pUnitServer; //   
	CObj<NAnimation::CAnimator> pRealAnimator;
	bool bTimeDelayGrenade;
	STime tErase; //        
		CDBPtr<NDb::CRPGEngGrenade> pRPGEngGrenade;
	int nThrowerEngSkill = 0;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&tExplode); f.Add(3,&pAnimator); f.Add(4,&pModel); f.Add(5,&pWorld); f.Add(6,&bindGlobal); f.Add(7,&pRPGGrenade); f.Add(8,&pAction); f.Add(9,&pUnitServer); f.Add(10,&pRealAnimator); f.Add(11,&bTimeDelayGrenade); f.Add(12,&tErase); f.Add(13,&pRPGEngGrenade); f.Add(14,&nThrowerEngSkill); return 0; }

	bool IsTimeToExplode( STime tCur ) { return tCur >= tExplode; }
	const CVec3 GetPosition();
public:
	CGrenadeServer() {}
	CGrenadeServer( CWorld *pWorld, const CVec3 &vFrom, const CVec3 &vSpeed,
		STime tThrow, float fTFly, NDb::CModel *pModel, NDb::CRPGGrenade *_pRPGGrenade,
		CUnitServer *_pUnitServer = 0 );
	// retail @0x75cc50: the engineer-grenade flavour (pRPGGrenade stays null, thrower's ENG skill)
	CGrenadeServer( CWorld *pWorld, const CVec3 &vFrom, const CVec3 &vSpeed,
		STime tThrow, float fTFly, NDb::CModel *pModel, NDb::CRPGEngGrenade *_pRPGEngGrenade,
		CUnitServer *_pUnitServer, int _nThrowerEngSkill );
	//
	bool Segment();
	virtual void Visit( IRenderVisitor *p );
private:
	void InitGrenade( const CVec3 &vFrom, const CVec3 &vSpeed, STime tThrow, float fTDelay );   // retail @0x75c8f0 shared init
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CClickOfDeath : public IDynamicObject
{
	OBJECT_NOCOPY_METHODS(CClickOfDeath);
	ZDATA
	CObj<CActionCounter> pAction;
	CObj<CObjectBase> pTarget;
	int nUserID;
	CRay ray;
	ZEND 	CPtr<IWorld> pWorld;
	int operator&( CStructureSaver &f ) { f.Add(2,&pAction); f.Add(3,&pTarget); f.Add(4,&nUserID); f.Add(5,&ray); f.Add(6,&pWorld); return 0; }
public:
	CClickOfDeath() {}
	CClickOfDeath( CActionCounter *pC, CObjectBase *pTarget, int _nUserID, const CRay &ray );
	bool Segment();
	virtual void Visit( IRenderVisitor *p ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGrenadeServer
////////////////////////////////////////////////////////////////////////////////////////////////////
CGrenadeServer::CGrenadeServer( CWorld *_pWorld, const CVec3 &vFrom,
	const CVec3 &vSpeed, STime tThrow, float fTDelay, NDb::CModel *_pModel,
	NDb::CRPGGrenade *_pRPGGrenade, CUnitServer *_pUnitServer )
: pWorld(_pWorld), pModel(_pModel), pRPGGrenade(_pRPGGrenade), pUnitServer(_pUnitServer)//tExplode(_tExplode)//, pItem(_pItem)
{
	InitGrenade( vFrom, vSpeed, tThrow, fTDelay );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x75cc50: the engineer-grenade server -- no regular record, the eng record + the
// thrower's ENGINEERING skill (feeds the eng explosion), same shared InitGrenade.
CGrenadeServer::CGrenadeServer( CWorld *_pWorld, const CVec3 &vFrom,
	const CVec3 &vSpeed, STime tThrow, float fTDelay, NDb::CModel *_pModel,
	NDb::CRPGEngGrenade *_pRPGEngGrenade, CUnitServer *_pUnitServer, int _nThrowerEngSkill )
: pWorld(_pWorld), pModel(_pModel), pRPGEngGrenade(_pRPGEngGrenade), pUnitServer(_pUnitServer)
{
	nThrowerEngSkill = _nThrowerEngSkill;
	InitGrenade( vFrom, vSpeed, tThrow, fTDelay );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::InitGrenade @0x75c8f0 -- the shared launch body of both ctors.
void CGrenadeServer::InitGrenade( const CVec3 &vFrom, const CVec3 &vSpeed, STime tThrow, float fTDelay )
{
	// launch actual item
	vector<SMassSphere> spheres;
	CVec3 massCenter;
	NAI::GetSpheres( pModel, &spheres, &massCenter );

	NAnimation::CASphereSet *pSphereSet = new NAnimation::CASphereSet( -100 );
	pSphereSet->pTime = pWorld->GetTime();
	pSphereSet->pMap = pWorld->GetAIMap();
	pSphereSet->InitSpheres( spheres );
	pSphereSet->InitBound( pModel->pGeometry->boundCenter, pModel->pGeometry->boundSize );
	// Never start the ballistic arc in the FUTURE: when a throw clip's release label sits at/after
	// the clip's natural end (tLabel1 > tEnd), CUnitServer::Segment fires the throw via its
	// animation-end fallback while tThrow (= tLabel1) is still ahead of the world clock --
	// CASphereSet::GetFrame then pins the grenade at vFrom until the clock catches up (the
	// "grenade hovers at the hand for seconds, then flies" freeze). Clamping to `now` is a no-op
	// for well-formed clips (tLabel1 <= now at fire time) and only corrects the fallback case.
	// The fuse below stays keyed to the raw tThrow (release label), as retail computes it.
	STime tFlightStart = Min( tThrow, pWorld->GetTime()->GetValue() );
	pSphereSet->Init( tFlightStart, vFrom, QNULL, vSpeed, true );

	pRealAnimator = pSphereSet;

	pAnimator = new NAnimation::CSkeletonAnimator( 0 );
	pAnimator->pTime = pWorld->GetTime();
	pAnimator->AddAnimator( pWorld->GetTime()->GetValue(), pSphereSet );
	//sItem.pAnimItem->AddTransit( tThrow, tThrow + 200, pSphereSet );
	bindGlobal.Link( pWorld->GetActive(), this );
	// retail InitGrenade @0x75c8f0 fuse branch: an ENGINEER grenade (no regular record) is
	// contact-fused -- bTimeDelayGrenade=false, tExplode=tThrow (explodes where it lands).
	if ( !IsValid( pRPGGrenade ) )
	{
		bTimeDelayGrenade = false;
		tExplode = tThrow;
	}
	else
	{
		tExplode = tThrow + (STime)int( (float(pRPGGrenade->nMaxDelay) - fTDelay) * 1000 );
		bTimeDelayGrenade = ( pRPGGrenade->nMaxDelay != 0 );
	}
	tErase = tExplode + (STime)( 10 * 1000 ); //  10-
	pAction = pWorld->GetActiveCounter();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec3 CGrenadeServer::GetPosition()
{
/*	CDGPtr<	CFuncBase<NAnimation::SSkeletonPose> > pPosition( pItem->pPosition );
	pPosition.Refresh();
	NAnimation::SSkeletonPose pose = pPosition->GetValue();
	return pose[0].pos;*/
	pAnimator.Refresh();
	NAnimation::SSkeletonPose pos = pAnimator->GetValue();
	ASSERT( pos[0].pos.z > -100 && pos[0].pos.z < 100 );
	return pos[0].pos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrenadeServer::Visit( IRenderVisitor *p )
{
	p->AddItemMesh( pModel, pAnimator );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGrenadeServer::Segment()
{
	if ( pWorld->GetTime()->GetValue() > tErase )
		return true;
	//
	CDynamicCast<NAnimation::CASphereSet> pSphereSet( pRealAnimator );
	bool bCollisionExplode = !bTimeDelayGrenade && pSphereSet->DidCollide();
	bool bIsTimeToExplode = bTimeDelayGrenade && IsTimeToExplode( pWorld->GetTime()->GetValue() );
	if ( bIsTimeToExplode || bCollisionExplode )
	{
		// retail Segment @0x75c5e0 explosion dispatch: a live regular record -> world vtbl+0x124
		// (regular blast); else the engineer record + thrower's ENG skill -> vtbl+0x120.
		if ( IsValid( pRPGGrenade ) )
			pWorld->AddGrenadeExplosion( GetPosition(), pRPGGrenade, pUnitServer );
		else
			pWorld->AddGrenadeExplosion( GetPosition(), pRPGEngGrenade, nThrowerEngSkill, pUnitServer );
		return true; // = erase
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CClickOfDeath
////////////////////////////////////////////////////////////////////////////////////////////////////
CClickOfDeath::CClickOfDeath( CActionCounter *_pAction, CObjectBase *_pTarget, int _nUserID, const CRay &_ray )
: pAction(_pAction), pTarget(_pTarget), nUserID(_nUserID), ray(_ray)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CClickOfDeath::Segment()
{
	NRPG::CAttackPortion atk;
	atk.MakeClickOfDeath( ray );
	if ( IsValid( pTarget ) )
	{
		CDynamicCast<NRPG::IAttackable> pT(pTarget);
		if (pT)
			pT->ProcessAttack( nUserID, &atk, NDb::GetArmor( NDb::N_DEFAULT_ARMOR ) );
	}
	return true; // = erase
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
IDynamicObject *CreateGrenadeServer( CWorld *pWorld, const CVec3 &vFrom, const CVec3 &vSpeed,
		STime tThrow, float fTFly, NDb::CModel *pModel, NDb::CRPGGrenade *_pRPGGrenade,
		CUnitServer *_pUnitServer )
{
	return new CGrenadeServer( pWorld, vFrom, vSpeed, tThrow, fTFly, pModel, _pRPGGrenade, _pUnitServer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// engineer-grenade flavour (retail eng server ctor @0x75cc50)
IDynamicObject *CreateGrenadeServer( CWorld *pWorld, const CVec3 &vFrom, const CVec3 &vSpeed,
		STime tThrow, float fTFly, NDb::CModel *pModel, NDb::CRPGEngGrenade *_pRPGEngGrenade,
		CUnitServer *_pUnitServer, int _nThrowerEngSkill )
{
	return new CGrenadeServer( pWorld, vFrom, vSpeed, tThrow, fTFly, pModel, _pRPGEngGrenade, _pUnitServer, _nThrowerEngSkill );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IDynamicObject *CreateClickOfDeath( CActionCounter *pC, CObjectBase *pTarget, int _nUserID, const CRay &ray )
{
	return new CClickOfDeath( pC, pTarget, _nUserID, ray );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0x118B1160, CGrenadeServer )
REGISTER_SAVELOAD_CLASS( 0x118B1161, CClickOfDeath )
