#include "StdAfx.h"
#include "wInterface.h" // for IWorld
#include "wInterfaceVisitors.h"
#include "Sync.h"
#include "RWSound.h"
#include "Sound.h"
#include "..\DBFormat\DataSound.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRender
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRenderSound: public IRenderSound, public COrdinarySyncDst<NWorld::IVisObj,CRenderSound>, 
public NWorld::ISoundVisitor
{
	typedef COrdinarySyncDst<NWorld::IVisObj,CRenderSound> TParent;
	OBJECT_BASIC_METHODS(CRenderSound);
	CPtr<NSound::ISoundScene> pScene;
	CTimeCounter timer;
public:
	CRenderSound() {}
	CRenderSound( CSyncSrc<NWorld::IVisObj> *pSrc, NSound::ISoundScene *pScene, NWorld::IWorld *pWorld );

	virtual void Add3DSound( STime tStart, NDb::CSound *pSound, CFuncBase<CVec3> *pPosition );
	virtual void Add2DSound( STime tStart, NDb::CSound *pSound );
	virtual void AddEffect( STime tStart, NDb::CSoundEffect *pEffect, CFuncBase<CVec3> *pPosition, const vector<int> &flags );
	virtual void Update( bool bAdvanceTime, CTransformStack *pTS, STime currentTime );
	virtual void ResetTiming() { timer.ResetTiming(); }
	// IRenderSound override forwarding to the sync base (retail @0x2d5a80 tail-call thunk)
	virtual void SetNewSource( CSyncSrc<NWorld::IVisObj> *pSrc ) { TParent::SetNewSource( pSrc ); }
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IRenderSound* CreateRenderSound( CSyncSrc<NWorld::IVisObj> *pSrc, NSound::ISoundScene *pSoundScene, NWorld::IWorld *pWorld )
{
	return new CRenderSound( pSrc, pSoundScene, pWorld );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRenderSound
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail ctor @0x2d5770: source comes from the caller (CRenderGame's GetActive/GetUnits pair) --
// the Jan03 inline union(GetActive,GetUnits) mixed sounds of units the viewing player cannot see.
CRenderSound::CRenderSound( CSyncSrc<NWorld::IVisObj> *pSrc, NSound::ISoundScene *_pScene, NWorld::IWorld *_pWorld )
: COrdinarySyncDst<NWorld::IVisObj,CRenderSound>( pSrc ),
	pScene(_pScene)
{
	// retail @0x2d581a: seed the render timer to world aim-time -- the SAME epoch that stamps every
	// C3DSound's tStart (CTimedObject::Attach @0x380330) -- so nDelay = now-tStart is the sound's TRUE
	// age. Without the seed 'now' starts at 0 while aim-time is already ~52s into the mission, pinning
	// nDelay to 0 so every re-sourced sound replayed from the top (the deafening on-sight burst).
	// The world is a CTOR-ONLY input (retail keeps no member either).
	timer.SetCurrent( _pWorld->GetAimTime()->GetValue() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderSound::Add3DSound( STime tStart, NDb::CSound *pSound, CFuncBase<CVec3> *pPosition )
{
	if ( !pSound )
		CPtr< CFuncBase<CVec3> > pHold( pPosition );
	else
	{
		// retail @0x2d5840: the scene receives delay = max(0, now - tStart - 50) -- how far INTO
		// the sample playback starts. Passing raw tStart (Jan03) made an OLD sound that (re)enters
		// the visible sync set -- e.g. a death grunt when its corpse is finally seen -- replay from
		// the top; with the delay it plays only its remaining tail, or nothing if already over.
		const int nDelay = Max( 0, (int)( timer.GetTime()->GetValue() - tStart ) - 50 );
		Register( pScene->Add3DSound( pSound, pPosition, (STime)nDelay ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderSound::AddEffect( STime tStart, NDb::CSoundEffect *pEffect, CFuncBase<CVec3> *pPosition, const vector<int> &flags )
{
	if ( !pEffect )
		CPtr< CFuncBase<CVec3> > pHold( pPosition );
	else
	{
		// v1.2 0x6d5f12 and Sentinels 0x821f52: effects take an elapsed offset too,
		// not an absolute begin time. CSoundInstance applies its per-instance start delay.
		const int nDelay = Max( 0, (int)( timer.GetTime()->GetValue() - tStart ) - 50 );
		Register( pScene->AddEffect( pEffect, (STime)nDelay, timer.GetTime(), pPosition, flags ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail v1.2 0x6d5eb0: use the same saved-event age as positional sounds.
void CRenderSound::Add2DSound( STime tStart, NDb::CSound *pSound )
{
	if ( pSound )
	{
		const int nDelay = Max( 0, (int)( timer.GetTime()->GetValue() - tStart ) - 50 );
		Register( pScene->Add2DSound( pSound, (STime)nDelay ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderSound::Update( bool bAdvanceTime, CTransformStack *pTS, STime currentTime )
{
	// ORIGINAL RETAIL BUG FIXED: v1.2 0x6d5d40 advances only once, losing all but
	// 150ms of a skipped turn. Sentinels 0x821d40..0x821d89 consumes the elapsed
	// GAME-clock delta in <=150ms steps before Sync, so old revealed/restored events
	// seek past their end rather than replay. CMissionBase supplies GetGameTime().
	// Reset/first-update still establishes a baseline; pause still freezes playback.
	// Bound the increment before adding, avoiding overflow in Sentinels' t + 150.
	STime t = timer.GetPrevTime();
	if ( t == 0 )
		t = currentTime;
	do
	{
		t = t < currentTime ? t + Min( (STime)150, currentTime - t ) : currentTime;
		timer.Advance( bAdvanceTime, t );
	} while ( t < currentTime );
	Sync();
	pScene->Draw( pTS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRenderSound::operator&( CStructureSaver &f )
{
	CPtr<CSyncSrc<NWorld::IVisObj> > pSrc = GetSource();
	f.Add( 1, &pSrc );
	f.Add( 2, &pScene );
	f.Add( 3, &timer );
	if ( f.IsReading() )
		SetNewSource( pSrc );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NRender;
REGISTER_SAVELOAD_CLASS( 0x03081142, CRenderSound );
