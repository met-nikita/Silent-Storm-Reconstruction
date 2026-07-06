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
	CRenderSound( CSyncSrc<NWorld::IVisObj> *pSrc, NSound::ISoundScene *pScene );

	virtual void Add3DSound( STime tStart, NDb::CSound *pSound, CFuncBase<CVec3> *pPosition );
	virtual void AddEffect( STime tStart, NDb::CSoundEffect *pEffect, CFuncBase<CVec3> *pPosition, const vector<int> &flags );
	virtual void Update( CTransformStack *pTS, STime currentTime );
	virtual void ResetTiming() { timer.ResetTiming(); }
	// IRenderSound override forwarding to the sync base (retail @0x2d5a80 tail-call thunk)
	virtual void SetNewSource( CSyncSrc<NWorld::IVisObj> *pSrc ) { TParent::SetNewSource( pSrc ); }
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IRenderSound* CreateRenderSound( CSyncSrc<NWorld::IVisObj> *pSrc, NSound::ISoundScene *pSoundScene )
{
	return new CRenderSound( pSrc, pSoundScene );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRenderSound
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail ctor @0x2d5770: source comes from the caller (CRenderGame's GetActive/GetUnits pair) --
// the Jan03 inline union(GetActive,GetUnits) mixed sounds of units the viewing player cannot see.
CRenderSound::CRenderSound( CSyncSrc<NWorld::IVisObj> *pSrc, NSound::ISoundScene *_pScene )
: COrdinarySyncDst<NWorld::IVisObj,CRenderSound>( pSrc ),
	pScene(_pScene)
{
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
		Register( pScene->AddEffect( pEffect, timer.GetTime()->GetValue(), timer.GetTime(), pPosition, flags ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderSound::Update( CTransformStack *pTS, STime currentTime )
{
	timer.Advance( true, currentTime );
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
