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
	NRPG::CAttackPortion attack;
	CObj<CActionCounter> pAction;
	CObj<NRPG::IInventoryItem> pIItem;
	CVec3 curPos;
	CVec3 velocity;
	CPtr<CUnitServer> pIgnored;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&tFinish); f.Add(3,&pAnimator); f.Add(4,&pModel); f.Add(5,&pWorld); f.Add(6,&bindGlobal); f.Add(7,&attack); f.Add(8,&pAction); f.Add(9,&pIItem); f.Add(10,&curPos); f.Add(11,&velocity); f.Add(12,&pIgnored);return 0; }
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
: pWorld(_pWorld), pModel(_pModel), attack(_attack), pIItem(_pIItem), velocity(vSpeed), pIgnored(_pIgnored)
{
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
	for ( vector<NAI::SInterval>::iterator i = makeDamage.begin(); i != makeDamage.end(); ++i )
	{
		if ( i->enter.fT > 0 && i->enter.fT < fDist )
		{
			fCollDist = i->enter.fT;
			CObjectBase *pCollided = i->pSrc->pUserData;
			CUnitServer *pI = pIgnored;
			if ( pCollided == pI )
				continue;
			CDynamicCast<NRPG::IAttackable> pAttackCatcher( pCollided );
			if ( pAttackCatcher )
				res = pWorld->GetGame()->ProcessThrowingAttackPortion( &attack, pAttackCatcher, i->pSrc->pArmor, i->nUserID );
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
			pWorld->AddFrozenItem( ray.ptOrigin + ray.ptDir * fCollDist, nextRot, pIItem );
			return true;
		case NRPG::AR_BOUNCE:
			pWorld->AddDebris( pModel.GetPtr(), pWorld->GetAIMap(), ray.ptOrigin + ray.ptDir * (fCollDist - 0.1f),
				nextRot, velocity, pWorld->GetTime(), pIItem );
			return true;
		case NRPG::AR_BOUNCE_BODY:
			pWorld->AddDebris( pModel.GetPtr(), pWorld->GetAIMap(), ray.ptOrigin + ray.ptDir * (fCollDist - 0.1f),
				nextRot, VNULL3, pWorld->GetTime(), pIItem );
			return true;
	}
	if ( pWorld->GetTime()->GetValue() > tFinish )
	{
		pWorld->AddDebris( pModel.GetPtr(), pWorld->GetAIMap(), ray.ptOrigin + ray.ptDir * fCollDist,
			nextRot, velocity, pWorld->GetTime(), pIItem );
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
