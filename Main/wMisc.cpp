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
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CTimedObject*)this); f.Add(2,&pEffect); f.Add(3,&pPosition); f.Add(4,&nFloor); return 0; }
public:
	CDParticles() {}
	CDParticles( CFuncBase<SFBTransform> *pPlace, NDb::CEffect *pEffect, int nFloor = -100 );
	CDParticles( const CVec3 &_pos, const CQuat &_q, NDb::CEffect *pEffect, int nFloor = -100 );
	
	virtual void Visit( IRenderVisitor *p );
};
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

	virtual void Visit( ISoundVisitor *p );
};
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
	p->AddParticleEffect( GetEventTime(), pEffect, nFloor, pPosition );
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
CTimedObject *Create3DSound( CFuncBase<CVec3> *pPos, NDb::CSound *pSound )
{
	return new C3DSound( pPos, pSound );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTimedObject *Create3DSound( const CVec3 &pos, NDb::CSound *pSound )
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
//REGISTER_SAVELOAD_CLASS( 0x01512130, CDFlash )
