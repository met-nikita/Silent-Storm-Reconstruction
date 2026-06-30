#include "StdAfx.h"
#include "wRocket.h"
#include "GAnimation.h"
#include "GSkeleton.h"
#include "aiMap.h"
#include "RPGGame.h"
#include "RPGItemInfo.h"
#include "..\DBFormat\DataRPG.h"
#include "wUnitServer.h"
#include "Transform.h"
#include "gSceneUtils.h"
#include "wMain.h"

namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRocketServer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRocketServer: public IDynamicObject, public IVisObj
{
	OBJECT_NOCOPY_METHODS(CRocketServer);
	ZDATA
	STime tFinish;
	CDGPtr<NAnimation::CSkeletonAnimator> pAnimator;
	CPtr<NDb::CModel> pModel;
	CPtr<CWorld> pWorld;
	CSyncSrcBind<IVisObj> bindGlobal;
	NRPG::CAttackPortion attack;
	CObj<CActionCounter> pAction;
	CObj<NRPG::IClipItem> pRocket;
	CVec3 curPos;
	CVec3 velocity;
	CPtr<CUnitServer> pIgnored;
	CDBPtr<NDb::CEffect> pEffect;
	CPtr< CFuncBase<SFBTransform> > pFlame;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&tFinish); f.Add(3,&pAnimator); f.Add(4,&pModel); f.Add(5,&pWorld); f.Add(6,&bindGlobal); f.Add(7,&attack); f.Add(8,&pAction); f.Add(9,&pRocket); f.Add(10,&curPos); f.Add(11,&velocity); f.Add(12,&pIgnored); f.Add(13,&pEffect); f.Add(14,&pFlame); return 0; }
public:
	CRocketServer() {}
	CRocketServer( CWorld *pWorld, const CVec3 &vFrom, const CVec3 &vSpeed,
		STime tThrow, float fDistance, NDb::CModel *pModel, NRPG::CAttackPortion &_attack, 
		NRPG::IClipItem *_pRocket, CUnitServer *_pIgnored, NDb::CEffect *_pEffect = 0 );
	//
	bool Segment();
	virtual void Visit( IRenderVisitor *p );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRocketAnimator
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRocketAnimator : public NAnimation::CAnimator
{
	OBJECT_BASIC_METHODS(CRocketAnimator);
	ZDATA_(CAnimator)
	STime tStart;
	CVec3 start;
	CVec3 vel;
	CQuat qRot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CAnimator*)this); f.Add(2,&tStart); f.Add(3,&start); f.Add(4,&vel); f.Add(5,&qRot); return 0; }
public:
	CRocketAnimator() {}
	CRocketAnimator( STime _tStart, const CVec3 &_start, const CVec3 &_vel );
	virtual void GetFrame( STime t, NAnimation::SSkeletonPose *pPose );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CRocketAnimator::CRocketAnimator( STime _tStart, const CVec3 &_start, const CVec3 &_vel ) :
	tStart(_tStart), start(_start), vel(_vel)
{
	SHMatrix sMatrix;
	CVec3 vDir( vel );
	Normalize( &vDir );
	MakeMatrix( &sMatrix, CVec3( 0, 0, 0 ), vDir );
	qRot.FromEulerMatrix( sMatrix );
	qRot = qRot * CQuat( ToRadian( 90.0f ), V3_AXIS_Z );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRocketAnimator::GetFrame( STime t, NAnimation::SSkeletonPose *pPose )
{
	STime tShift = t - tStart;
	float fTShift = tShift / 1000.f;
	(*pPose)[0].pos = start + vel * fTShift;
	(*pPose)[0].rot = qRot;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRocketFlameAnimator
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRocketFlameAnimator : public CFuncBase<SFBTransform>
{
protected:
	virtual bool NeedUpdate() { return pTime.Refresh(); }
	virtual void Recalc();
public:
	ZDATA
	STime tStart;
	CVec3 start;
	CVec3 vel;
	CDGPtr<CFuncBase<STime> > pTime;
	ZEND
	CRocketFlameAnimator() {}
	CRocketFlameAnimator( STime _tStart, const CVec3 &_start, const CVec3 &_vel ) :
		tStart(_tStart), start(_start), vel(_vel)	{}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRocketFlameAnimator::Recalc()
{
	STime tShift = pTime->GetValue() - tStart;
	float fTShift = tShift / 1000.f;
	CVec3 vDir( vel );
	Normalize( &vDir );
	CVec3 ptPos( start + fTShift * vel - vDir * 0.4f ); // crap - must use rocket size (where could I get it?)

	CQuat qRot;
	SHMatrix sMatrix;
	MakeMatrix( &sMatrix, CVec3( 0, 0, 0 ), -vDir );
	qRot.FromEulerMatrix( sMatrix );
	qRot = qRot * CQuat( ToRadian( 90.0f ), V3_AXIS_Z );

	CFBMatrixStack<4> m;
	m.Init();
	m.Push( ptPos, qRot );
	value = m.Get();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRocketServer
////////////////////////////////////////////////////////////////////////////////////////////////////
CRocketServer::CRocketServer( CWorld *_pWorld, const CVec3 &vFrom, const CVec3 &vSpeed,
	STime tThrow, float fDistance, NDb::CModel *_pModel, NRPG::CAttackPortion &_attack,
	NRPG::IClipItem *_pRocket, CUnitServer *_pUnitServer, NDb::CEffect *_pEffect )
: pWorld(_pWorld), pModel(_pModel), attack(_attack), pRocket(_pRocket), velocity(vSpeed), pIgnored(_pUnitServer), pEffect(_pEffect)
{
	CRocketAnimator *pRAnim = new CRocketAnimator( tThrow, vFrom, vSpeed );
	CRocketFlameAnimator *pFlameAn = new CRocketFlameAnimator( tThrow, vFrom, vSpeed );
	pFlameAn->pTime = pWorld->GetTime();
	pFlame = pFlameAn;
	pAnimator = new NAnimation::CSkeletonAnimator(0);
	pAnimator->pTime = pWorld->GetTime();
	pAnimator->AddAnimator( pWorld->GetTime()->GetValue(), pRAnim );
	bindGlobal.Link( pWorld->GetActive(), this );
	float fTFly = fDistance / fabs( vSpeed ) * 1000;
	tFinish = tThrow + (STime)Float2Int( fTFly );
	pAction = pWorld->GetActiveCounter();
	curPos = vFrom;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRocketServer::Visit( IRenderVisitor *p )
{
	p->AddItemMesh( pModel, pAnimator );
	if ( pEffect ) 
		p->AddParticleEffect( pWorld->GetAimTime()->GetValue(), pEffect, -100, pFlame );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRocketServer::Segment()
{
	vector<NAI::SInterval> intersect;
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
	pWorld->GetAIMap()->Trace( ray, &intersect, NWorld::TS_FRAGMENTED );
	//
	float fCollDist = 0;
	for ( vector<NAI::SInterval>::iterator i = intersect.begin(); i != intersect.end(); ++i )
	{
		if ( i->enter.fT > 0 && i->enter.fT < fDist )
		{
			fCollDist = i->enter.fT;
			CObjectBase *pCollided = i->pSrc->pUserData;
			CUnitServer *pI = pIgnored;
			if ( pCollided == pI )
				continue;
			// add grenade explosion
			pWorld->AddGrenadeExplosion( curPos, pRocket->GetDBAmmo()->pExplosiveBullet, pIgnored );
			return true;
		}
	}
	//
	if ( pWorld->GetTime()->GetValue() > tFinish )
	{
		// add фиктивную grenade
		pWorld->ThrowGrenade( curPos, velocity, pWorld->GetTime()->GetValue(), 
			0, pModel, pRocket->GetDBAmmo()->pExplosiveBullet, pIgnored );
		return true;
	}
	//
	curPos = nextPos;
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
IDynamicObject *CreateRocketServer( CWorld *pWorld, const CVec3 &vFrom, const CVec3 &vSpeed,
	STime tThrow, float fDistance, NDb::CModel *pModel, NRPG::CAttackPortion &_attack, 
	NRPG::IClipItem *_pRocket, CUnitServer *_pIgnored, NDb::CEffect *_pEffect )
{
	return new CRocketServer( pWorld, vFrom, vSpeed, tThrow, fDistance, pModel, _attack, _pRocket, _pIgnored, _pEffect );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0x73062151, CRocketAnimator );
REGISTER_SAVELOAD_CLASS( 0x73062152, CRocketServer )
