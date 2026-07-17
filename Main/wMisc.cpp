#include "StdAfx.h"
#include "wMisc.h"
#include "Transform.h"
#include "GSceneUtils.h"
#include "wMain.h"
#include "..\DBFormat\DataSound.h"
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// class definitions: CDParticles, C3DSound, CDGrassEvent
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDParticles: public CTimedObject
{
	OBJECT_NOCOPY_METHODS(CDParticles);
	ZDATA_(CTimedObject)
	CDBPtr<NDb::CEffect> pEffect;
	CObj<CFuncBase<SFBTransform> > pPosition;
	int nFloor;
	CDGPtr<NAnimation::CSkeletonAnimator> pAnimator;   // retail tag 5 (@0x380f00): the skeleton the effect is glued to (retail CreateDParticles @0x380130 writer)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CTimedObject*)this); f.Add(2,&pEffect); f.Add(3,&pPosition); f.Add(4,&nFloor); f.Add(5,&pAnimator); return 0; }
public:
	CDParticles() {}
	CDParticles( CFuncBase<SFBTransform> *pPlace, NDb::CEffect *pEffect, int nFloor = -100 );
	CDParticles( CFuncBase<SFBTransform> *pPlace, NAnimation::CSkeletonAnimator *pAnimator, NDb::CEffect *pEffect, int nFloor = -100 );
	CDParticles( const CVec3 &_pos, const CQuat &_q, NDb::CEffect *pEffect, int nFloor = -100 );

	virtual void Visit( IRenderVisitor *p );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// (C3DSound class definition moved to wMisc.h -- W3: CExecShoot's SLongBurstSnd retention slot,
//  retail save tag 3 @0x3b0ef0, needs the public surface + EndSound @0x37f7e0.)
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDGrassEvent: public CTimedObject
{
	OBJECT_NOCOPY_METHODS(CDGrassEvent);
	ZDATA_(CTimedObject)
	CVec3 vPlace;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CTimedObject*)this); f.Add(2,&vPlace); return 0; }
public:
	CDGrassEvent() {}
	CDGrassEvent( const CVec3 &_ptPlace );

	virtual void Visit( IRenderVisitor *p );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDMesh -- retail @0x37f8d0 (2004-only, absent from Jan03): a renderable + TARGETABLE noise marker placed at a
// heard-not-seen unit's last position (CreateSoundStuff). Unlike the Jan03 particle it draws a static DB mesh
// (model N_SOUND_MARKER_MODEL_ID) AND submits a pickable AI hull (TS_PICK), so the player can click/attack the
// heard position; pUnit is a weak back-reference to the heard unit for target association (may go null).
class CDMesh: public CTimedObject
{
	OBJECT_NOCOPY_METHODS(CDMesh);
	ZDATA_(CTimedObject)
	int nFloor;                     // retail +0x28, save tag 2
	CVec3 pos;                      // retail +0x2c, save tag 3
	CQuat rot;                      // retail +0x38, save tag 4: marker rotation (identity from the only creator, CreateSoundStuff @0x369110)
	CPtr<CObjectBase> pUnit;        // retail +0x48 (CPtr<CUnit>), save tag 5: weak ref to the heard unit (targetability association)
	CPtr<NDb::CModel> pModel;       // retail +0x4c, save tag 6: the static marker mesh (DB model 3899)
	// retail wire 1:1 (operator& @0x381050; byte-walk x58 across the retail slots: 1:32 2:4 3:12 4:16 5:4 6:4)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CTimedObject*)this); f.Add(2,&nFloor); f.Add(3,&pos); f.Add(4,&rot); f.Add(5,&pUnit); f.Add(6,&pModel); return 0; }
public:
	// retail default ctor @0x380580 nulls pUnit/pModel and zeroes tEvent/bind only; nFloor/pos/rot are
	// left unwritten (the loader fills tags 2-4).
	CDMesh() {}
	// retail ctor @0x37f8d0: (pUnit, pos, rot, pModel, nFloor), nSegmentsLeft = 1000
	CDMesh( CObjectBase *_pUnit, const CVec3 &_pos, const CQuat &_rot, NDb::CModel *_pModel, int _nFloor )
		: CTimedObject( 1000 ), nFloor(_nFloor), pos(_pos), rot(_rot), pUnit(_pUnit), pModel(_pModel) {}

	virtual void Visit( IRenderVisitor *p );
	virtual void Visit( IAIVisitor *p );
	CObjectBase* GetUnit() const { return pUnit; }
	const CVec3& GetPos() const { return pos; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
/*class CDFlash: public CTimedObject
{
	OBJECT_NOCOPY_METHODS(CDFlash);
	ZDATA_(CTimedObject)
	CVec3 vPlace;
	CVec3 vColor;
	float fRadius;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CTimedObject*)this); f.Add(2,&vPlace); f.Add(3,&vColor); f.Add(4,&fRadius); return 0; }
public:
	CDFlash() {}
	CDFlash( const CVec3 &_vPlace, CVec3 &_vColor, float _fRadius, int nTime );

	virtual void Visit( IRenderVisitor *p );
};*/
////////////////////////////////////////////////////////////////////////////////////////////////////
CTimedObject::CTimedObject( int _nSegmentsLeft )
: nSegmentsLeft(_nSegmentsLeft) 
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTimedObject::Attach( CSyncSrc<NWorld::IVisObj> *pSrc, CWorld *pWorld )
{
	tEvent = pWorld->GetAimTime()->GetValue();//_tEvent;
	bindGlobal.Link( pSrc, this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDParticles
////////////////////////////////////////////////////////////////////////////////////////////////////
CDParticles::CDParticles( CFuncBase<SFBTransform> *_pPlace, NDb::CEffect *_pEffect, int _nFloor )
: CTimedObject( 1000 ), pPosition(_pPlace), pEffect(_pEffect), nFloor(_nFloor)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x380130 (skeleton-glued): same as above + the bone-riding animator (save tag 5)
CDParticles::CDParticles( CFuncBase<SFBTransform> *_pPlace, NAnimation::CSkeletonAnimator *_pAnimator, NDb::CEffect *_pEffect, int _nFloor )
: CTimedObject( 1000 ), pPosition(_pPlace), pEffect(_pEffect), nFloor(_nFloor), pAnimator(_pAnimator)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CDParticles::CDParticles( const CVec3 &_pos, const CQuat &_q, NDb::CEffect *_pEffect, int _nFloor )
: CTimedObject( 1000 ), pEffect(_pEffect), nFloor(_nFloor)
{
  CFBMatrixStack<4> m;
  m.Init();
	m.Push( _pos, _q );
	//m.PushScale( ptScale.x, ptScale.y, ptScale.z );
	pPosition = new NGScene::CCFBTransform( m.Get() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDParticles::Visit( IRenderVisitor *p )
{
	// retail @0x37f580: the skeleton animator rides along so a glued effect follows the bones (null for free effects)
	p->AddParticleEffect( GetEventTime(), pEffect, nFloor, pPosition, pAnimator.GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// C3DSound
////////////////////////////////////////////////////////////////////////////////////////////////////
C3DSound::C3DSound( CFuncBase<CVec3> *_pPos, NDb::CSound *_pSound )
: CTimedObject( 1000 ), pPosition(_pPos), pSound(_pSound) // 1000 - CRAP, should take exact sound length from pSound
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
C3DSound::C3DSound( const CVec3 &_pos, NDb::CSound *_pSound )
: CTimedObject( 1000 ), pPosition( new NGScene::CCVec3( _pos ) ), pSound(_pSound) // 1000 - CRAP, should take exact sound length from pSound
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void C3DSound::Visit( ISoundVisitor *p )
{
	p->Add3DSound( GetEventTime(), pSound, pPosition );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x37f7e0: only a record with ending samples reacts -- flag finished + re-emit the vis-binding
void C3DSound::EndSound()
{
	if ( IsValid( pSound ) && pSound->nEndingSamples != 0 )
	{
		bFinished = true;
		bindGlobal.Update();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDGrassEvent
////////////////////////////////////////////////////////////////////////////////////////////////////
CDGrassEvent::CDGrassEvent( const CVec3 &_vPlace )
: CTimedObject( 100 ), vPlace(_vPlace) 
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDGrassEvent::Visit( IRenderVisitor *p )
{
	p->AddGrassEvent( vPlace );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDMesh
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDMesh::Visit( IRenderVisitor *p )
{
	// retail @0x37f630: transform = fresh matrix stack Init() + Push(pos, rot); NO model guard --
	// retail hands pModel straight to the visitor (the wire always carries a valid DB model).
	CFBMatrixStack<4> m;
	m.Init();
	m.Push( pos, rot );
	p->AddMesh( pModel, m.Get(), 0, nFloor, -1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDMesh::Visit( IAIVisitor *p )
{
	// retail @0x37f700: submit a pickable hull so the heard marker is clickable/targetable (mask TS_PICK,
	// armor 0). Guard is IsValid(pModel->pGeometry) ONLY -- retail dereferences pModel unchecked and hands
	// pGeometry->pAIGeometry to AddHull unchecked.
	NDb::CGeometry *pGeometry = pModel->pGeometry;
	if ( !IsValid( pGeometry ) )
		return;
	CFBMatrixStack<4> m;
	m.Init();
	m.Push( pos, rot );
	p->AddHull( pGeometry->pAIGeometry, m.Get(), 0, nFloor, TS_PICK );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDFlash
////////////////////////////////////////////////////////////////////////////////////////////////////
/*CDFlash::CDFlash( const CVec3 &_vPlace, CVec3 &_vColor, float _fRadius, int nTime )
: CTimedObject( nTime ), vPlace(_vPlace), vColor(_vColor), fRadius(_fRadius) 
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDFlash::Visit( IRenderVisitor *p )
{
	p->AddPointLight( vColor, vPlace, fRadius, false );
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
CTimedObject *CreateDParticles( CFuncBase<SFBTransform> *pPlace, NDb::CEffect *pEffect, int nFloor )
{
	return new CDParticles( pPlace, pEffect, nFloor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTimedObject *CreateDParticles( const CVec3 &pos, const CQuat &q, NDb::CEffect *pEffect, int nFloor )
{
	return new CDParticles( pos, q, pEffect, nFloor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x380130 -- the skeleton-glued variant (fills CDParticles::pAnimator, save tag 5)
CTimedObject *CreateDParticles( CFuncBase<SFBTransform> *pPlace, NAnimation::CSkeletonAnimator *pAnimator, NDb::CEffect *pEffect, int nFloor )
{
	return new CDParticles( pPlace, pAnimator, pEffect, nFloor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
C3DSound *Create3DSound( CFuncBase<CVec3> *pPos, NDb::CSound *pSound )
{
	return new C3DSound( pPos, pSound );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTimedObject *CreateDMesh( CObjectBase *pUnit, const CVec3 &pos, const CQuat &rot, NDb::CModel *pModel, int nFloor )
{
	return new CDMesh( pUnit, pos, rot, pModel, nFloor );   // @0x3800f0
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* GetDMeshUnit( CObjectBase *p )
{
	CDMesh *pMesh = dynamic_cast<CDMesh*>( p );
	if ( pMesh == 0 )
		return 0;
	return pMesh->GetUnit();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool GetDMeshPos( CObjectBase *p, CVec3 *pPos )
{
	CDMesh *pMesh = dynamic_cast<CDMesh*>( p );
	if ( pMesh == 0 )
		return false;
	*pPos = pMesh->GetPos();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
C3DSound *Create3DSound( const CVec3 &pos, NDb::CSound *pSound )
{
	return new C3DSound( pos, pSound );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTimedObject *CreateDGrassEvent( const CVec3 &ptPlace )
{
	return new CDGrassEvent(ptPlace);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NWorld;
BASIC_REGISTER_CLASS( CTimedObject )
REGISTER_SAVELOAD_CLASS( 0x01512120, CDParticles )
REGISTER_SAVELOAD_CLASS( 0x01512121, C3DSound )
REGISTER_SAVELOAD_CLASS( 0x01512122, CDGrassEvent )
REGISTER_SAVELOAD_CLASS( 0xB3130170, CDMesh )   // @0x37f8d0 retail saveload id
//REGISTER_SAVELOAD_CLASS( 0x01512130, CDFlash )
