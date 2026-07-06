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
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWorld;
class CTimedObject: public IDynamicObject, public IVisObj
{
	ZDATA
	int nSegmentsLeft;
	STime tEvent;
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
CTimedObject *CreateDParticles( CFuncBase<SFBTransform> *pPlace, NDb::CEffect *pEffect, int nFloor = -100 );
CTimedObject *CreateDParticles( const CVec3 &_pos, const CQuat &_q, NDb::CEffect *pEffect, int nFloor = -100 );
CTimedObject *Create3DSound( CFuncBase<CVec3> *pPos, NDb::CSound *pSound );
CTimedObject *Create3DSound( const CVec3 &_pos, NDb::CSound *pSound );
CTimedObject *CreateDGrassEvent( const CVec3 &_ptPlace );
CTimedObject *CreateDMesh( CObjectBase *pUnit, const CVec3 &pos, NDb::CModel *pModel, int nFloor );   // @0x3800f0
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
