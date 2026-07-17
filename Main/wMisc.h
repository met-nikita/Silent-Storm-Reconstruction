#ifndef __wMisc_H_
#define __wMisc_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "wDynObject.h"
#include "wInterfaceVisitors.h"
#include "Sync.h"
namespace NDb
{
	class CSound;
	class CParticleEffect;
}
namespace NAnimation
{
	class CSkeletonAnimator;
}
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWorld;
class CTimedObject: public IDynamicObject, public IVisObj
{
	ZDATA
	int nSegmentsLeft;
	STime tEvent;
protected:
	CSyncSrcBind<IVisObj> bindGlobal;
public:
		CPtr<CWorld> pWorld;   // release CTimedObject tag5 (CPtr<IWorld>); CWorld is the concrete, serializes by object-id identically
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nSegmentsLeft); f.Add(3,&tEvent); f.Add(4,&bindGlobal); f.Add(5,&pWorld); return 0; }
protected:
	STime GetEventTime() const { return tEvent; }
public:
	CTimedObject() {}
	CTimedObject( int _nSegmentsLeft );
	void Attach( CSyncSrc<NWorld::IVisObj> *pSrc, CWorld *pWorld );
	bool Segment() { return --nSegmentsLeft <= 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// C3DSound -- a positioned one-shot/looping sound in the world. Hoisted out of wMisc.cpp to a header
// surface (W3 serialization convergence): retail CExecShoot retains the long-burst C3DSound in its
// SLongBurstSnd slot (save tag 3, operator& @0x3b0ef0) and ends it via C3DSound::EndSound @0x37f7e0.
////////////////////////////////////////////////////////////////////////////////////////////////////
class C3DSound: public CTimedObject
{
	OBJECT_NOCOPY_METHODS(C3DSound);
	ZDATA_(CTimedObject)
	CObj<CFuncBase<CVec3> > pPosition;
	CDBPtr<NDb::CSound> pSound;
		bool bFinished = false;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CTimedObject*)this); f.Add(2,&pPosition); f.Add(3,&pSound); f.Add(4,&bFinished); return 0; }
public:
	C3DSound() {}
	C3DSound( CFuncBase<CVec3> *pPos, NDb::CSound *pSound );
	C3DSound( const CVec3 &_pos, NDb::CSound *pSound );

	// retail @0x37f7e0: a sound record WITH ending samples is flagged finished (the client plays the
	// ending) and the vis-binding is re-emitted so the render/sound side picks the change up.
	void EndSound();

	virtual void Visit( ISoundVisitor *p );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CTimedObject *CreateDParticles( CFuncBase<SFBTransform> *pPlace, NDb::CEffect *pEffect, int nFloor = -100 );
CTimedObject *CreateDParticles( const CVec3 &_pos, const CQuat &_q, NDb::CEffect *pEffect, int nFloor = -100 );
// retail @0x380130: the skeleton-glued variant -- the effect rides pAnimator's bones (CDParticles save tag 5).
CTimedObject *CreateDParticles( CFuncBase<SFBTransform> *pPlace, NAnimation::CSkeletonAnimator *pAnimator, NDb::CEffect *pEffect, int nFloor = -100 );
C3DSound *Create3DSound( CFuncBase<CVec3> *pPos, NDb::CSound *pSound );
C3DSound *Create3DSound( const CVec3 &_pos, NDb::CSound *pSound );
CTimedObject *CreateDGrassEvent( const CVec3 &_ptPlace );
CTimedObject *CreateDMesh( CObjectBase *pUnit, const CVec3 &pos, const CQuat &rot, NDb::CModel *pModel, int nFloor );   // @0x3800f0: (pUnit, pos, rot, pModel, nFloor)
// Classify a traced object as a heard-not-seen noise marker (CDMesh) and resolve its heard unit
// (weak back-ref, may return 0). Mirrors retail NWorld::Trace's AsAISound + sound-owner classify
// pair (@0x37d760) for the UI pick path; returns 0 for anything that is not a CDMesh.
CObjectBase* GetDMeshUnit( CObjectBase *p );
// The marker's world position (where the silhouette stands / the noise happened). Retail anchors
// both the shoot-the-noise tile command (CStateAttack::GetTargetCmd @0x1d9e20, IAISound::GetPosition)
// and the ear icon (CSoundIcon::Draw @0x210960) on the SOUND position, never on the live unit.
bool GetDMeshPos( CObjectBase *p, CVec3 *pPos );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
