#include "StdAfx.h"
#include "wDynObject.h"
#include "wInterfaceVisitors.h"
#include "Sync.h"
#include "RPGAttackMech.h"
#include "wKnife.h"
#include "wMain.h"
#include "GAnimation.h"
#include "GSkeleton.h"
#include "aiMap.h"
#include "RPGGame.h"
#include "RPGBullet.h"   // NRPG::SAttackRayInfo -- CKnifeServer::rayInfo (retail save tag 11)
#include "wUnitServer.h"

namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CKnifeServer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CKnifeServer: public IDynamicObject, public IVisObj
{
	OBJECT_NOCOPY_METHODS(CKnifeServer);
	ZDATA
	STime tFinish;
	CDGPtr<NAnimation::CSkeletonAnimator> pAnimator;
	CPtr<NDb::CModel> pModel;
	CPtr<CWorld> pWorld;
	CSyncSrcBind<IVisObj> bindGlobal;
	CObj<CActionCounter> pAction;
	CObj<NRPG::IInventoryItem> pIItem;
	CVec3 curPos;
	CVec3 velocity;
	// retail folds the attack + from/dir + the shooter's in-flight ignore object into the
	// SAttackRayInfo carrier (PDB CKnifeServer+0x4c, save tag 11); the Jan03 scalar pIgnored
	// became the `ignore` vector (retail +0xe0, save tag 12, DoVector<CPtr<CObjectBase>>).
	NRPG::SAttackRayInfo rayInfo;
	vector<CPtr<CObjectBase> > ignore;
	// retail operator& @0x360ea0: 2=tFinish 3=pAnimator 4=pModel 5=pWorld 6=bindGlobal 7=pAction
	// 8=pIItem 9=curPos 10=velocity 11=rayInfo 12=ignore (dev tags 7-12 were permuted -- W3 fix)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&tFinish); f.Add(3,&pAnimator); f.Add(4,&pModel); f.Add(5,&pWorld); f.Add(6,&bindGlobal); f.Add(7,&pAction); f.Add(8,&pIItem); f.Add(9,&curPos); f.Add(10,&velocity); f.Add(11,&rayInfo); f.Add(12,&ignore); return 0; }
public:
	CKnifeServer() {}
	CKnifeServer( CWorld *pWorld, const CVec3 &vFrom, const CVec3 &vSpeed,
		STime tThrow, float fDistance, NDb::CModel *pModel, NRPG::CAttackPortion &_attack, 
		NRPG::IInventoryItem *_pIItem, CUnitServer *_pIgnored );
	//
	bool Segment();
	virtual void Visit( IRenderVisitor *p );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CKnifeAnimator
////////////////////////////////////////////////////////////////////////////////////////////////////
class CKnifeAnimator : public NAnimation::CAnimator
{
	OBJECT_BASIC_METHODS(CKnifeAnimator);
	ZDATA_(CAnimator)
	STime tStart;
	CVec3 start;
	CVec3 vel;
	CQuat qRot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CAnimator*)this); f.Add(2,&tStart); f.Add(3,&start); f.Add(4,&vel); f.Add(5,&qRot); return 0; }
public:
	CKnifeAnimator() {}
	CKnifeAnimator( STime _tStart, const CVec3 &_start, const CVec3 &_vel );
	virtual void GetFrame( STime t, NAnimation::SSkeletonPose *pPose );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CKnifeAnimator::CKnifeAnimator( STime _tStart, const CVec3 &_start, const CVec3 &_vel ) :
	tStart(_tStart), start(_start), vel(_vel)
{
	qRot = QNULL;
	float fAngle = acos( vel.z / fabs(vel) );
	CVec3 axis( -vel.y, vel.x, 0 );
	if ( fabs2(axis) > 1e-6f )
		qRot.FromAngleAxis( fAngle, axis, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CKnifeAnimator::GetFrame( STime t, NAnimation::SSkeletonPose *pPose )
{
	// for knife throwing
	STime tShift = t - tStart;
	float fTShift = tShift / 1000.f;
	(*pPose)[0].pos = start + vel * fTShift;
	(*pPose)[0].rot = qRot;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CKnifeServer
////////////////////////////////////////////////////////////////////////////////////////////////////
CKnifeServer::CKnifeServer( CWorld *_pWorld, const CVec3 &vFrom, const CVec3 &vSpeed,
	STime tThrow, float fDistance, NDb::CModel *_pModel, NRPG::CAttackPortion &_attack, NRPG::IInventoryItem *_pIItem, CUnitServer *_pIgnored )
: pWorld(_pWorld), pModel(_pModel), pIItem(_pIItem), velocity(vSpeed)
{
	// retail ctor @0x360590 receives a pre-built SAttackRayInfo; the dev throw pipeline still hands
	// the pieces separately, so the carrier is assembled here (same fields the retail copy carries).
	rayInfo.atk = _attack;
	rayInfo.vOrigin = vFrom;
	rayInfo.vDir = vSpeed;
	Normalize( &rayInfo.vDir );
	rayInfo.pUS = _pIgnored;        // the thrower
	rayInfo.pIgnore = _pIgnored;    // the thrower's hull is ignored in flight (retail Segment reads rayInfo.pIgnore)
	// `ignore` starts empty, exactly like the retail ctor.
	CKnifeAnimator *pLinear = new CKnifeAnimator( tThrow, vFrom, vSpeed );
	pAnimator = new NAnimation::CSkeletonAnimator(0);
	pAnimator->pTime = pWorld->GetTime();
	pAnimator->AddAnimator( pWorld->GetTime()->GetValue(), pLinear );
	bindGlobal.Link( pWorld->GetActive(), this );
	float fTFly = fDistance / fabs( vSpeed ) * 1000;
	tFinish = tThrow + (STime)Float2Int( fTFly );
	pAction = pWorld->GetActiveCounter();
	curPos = vFrom;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CKnifeServer::Visit( IRenderVisitor *p )
{
	p->AddItemMesh( pModel, pAnimator );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CKnifeServer::Segment()
{
	pAnimator.Refresh();
	NAnimation::SSkeletonPose pos = pAnimator->GetValue();
	CVec3 nextPos = pos[0].pos;
	CQuat nextRot = pos[0].rot;
	CRay ray;
	ray.ptOrigin = curPos;
	ray.ptDir = nextPos - curPos;
	float fDist = fabs(ray.ptDir);
	if ( fDist < FP_EPSILON )
		return false;
	Normalize( &ray.ptDir );
	vector<NAI::SInterval> makeDamage, passBlockers;
	pWorld->GetAIMap()->Trace( ray, &makeDamage, NWorld::TS_FRAGMENTED | NWorld::TS_ITEM_BLOCKER );

	curPos = nextPos;

	NRPG::EAttackResult res = NRPG::AR_IGNORE;
	float fCollDist = 0;
	int nCollFloor = 0;   // retail @0x76002c: 0 until a hull is inspected; the landed knife's CDItem floor
	for ( vector<NAI::SInterval>::iterator i = makeDamage.begin(); i != makeDamage.end(); ++i )
	{
		if ( i->enter.fT > 0 && i->enter.fT < fDist )
		{
			fCollDist = i->enter.fT;
			CObjectBase *pCollided = i->pSrc->pUserData;
			if ( pCollided == rayInfo.pIgnore.GetPtr() )   // retail @0x35fe50: the carrier's ignore object
				continue;
			nCollFloor = i->pSrc->nFloor;   // retail @0x7600a8: the hit hull's floor (after the ignore test)
			CDynamicCast<NRPG::IAttackable> pAttackCatcher( pCollided );
			if ( pAttackCatcher )
				res = pWorld->GetGame()->ProcessThrowingAttackPortion( &rayInfo.atk, pAttackCatcher, i->pSrc->pArmor, i->nUserID );
			else 
			{
				if ( ( i->pSrc->nTSFlags & NWorld::TS_ITEM_BLOCKER ) != 0 )
					res = NRPG::AR_STUCK;
			}
			if ( res != NRPG::AR_IGNORE )
				break;
		}
	}
	switch ( res )
	{
		case NRPG::AR_STUCK:
			pWorld->AddFrozenItem( pWorld->GetAIMap(), ray.ptOrigin + ray.ptDir * fCollDist, nextRot, pIItem );
			return true;
		case NRPG::AR_BOUNCE:
			// retail @0x35fe50: the bounce drop-back constant is 0.2f (Jan03 used 0.1f); all three
			// knife AddDebris calls pass bFallFromBody=false, no visibility parent, the hit floor
			// (@0x760414 push nCollFloor / @0x760415 push pIItem / @0x76041a-0x76041f push 0,0)
			pWorld->AddDebris( pModel.GetPtr(), pWorld->GetAIMap(), ray.ptOrigin + ray.ptDir * (fCollDist - 0.2f),
				nextRot, velocity, pWorld->GetTime(), false, 0, pIItem, nCollFloor );
			return true;
		case NRPG::AR_BOUNCE_BODY:
			pWorld->AddDebris( pModel.GetPtr(), pWorld->GetAIMap(), ray.ptOrigin + ray.ptDir * (fCollDist - 0.2f),
				nextRot, VNULL3, pWorld->GetTime(), false, 0, pIItem, nCollFloor );   // retail @0x760314
			return true;
	}
	if ( pWorld->GetTime()->GetValue() > tFinish )
	{
		pWorld->AddDebris( pModel.GetPtr(), pWorld->GetAIMap(), ray.ptOrigin + ray.ptDir * fCollDist,
			nextRot, velocity, pWorld->GetTime(), false, 0, pIItem, nCollFloor );   // retail @0x760231
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
IDynamicObject *CreateKnifeServer( CWorld *pWorld, const CVec3 &vFrom, const CVec3 &vSpeed,
		STime tThrow, float fDistance, NDb::CModel *pModel, NRPG::CAttackPortion &_attack, 
		NRPG::IInventoryItem *_pIItem, CUnitServer *_pIgnored )
{
	return new CKnifeServer( pWorld, vFrom, vSpeed, tThrow, fDistance, pModel, _attack, _pIItem, _pIgnored );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0x10661181, CKnifeAnimator );
REGISTER_SAVELOAD_CLASS( 0x10662182, CKnifeServer )
