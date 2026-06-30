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
	CRenderSound( NWorld::IWorld *pWorld, NSound::ISoundScene *pScene );
	
	virtual void Add3DSound( STime tStart, NDb::CSound *pSound, CFuncBase<CVec3> *pPosition );
	virtual void AddEffect( STime tStart, NDb::CSoundEffect *pEffect, CFuncBase<CVec3> *pPosition, const vector<int> &flags );
	virtual void Update( CTransformStack *pTS, STime currentTime );
	virtual void ResetTiming() { timer.ResetTiming(); }
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IRenderSound* CreateRenderSound( NWorld::IWorld *pWorld, NSound::ISoundScene *pSoundScene )
{
	return new CRenderSound( pWorld, pSoundScene );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRenderSound
////////////////////////////////////////////////////////////////////////////////////////////////////
CRenderSound::CRenderSound( NWorld::IWorld *_pWorld, NSound::ISoundScene *_pScene )
: COrdinarySyncDst<NWorld::IVisObj,CRenderSound>( 
	new CBoolSyncSrc<NWorld::IVisObj, CUnionFunc>( _pWorld->GetActive(), _pWorld->GetUnits() ) ), 
	pScene(_pScene)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderSound::Add3DSound( STime tStart, NDb::CSound *pSound, CFuncBase<CVec3> *pPosition )
{
	if ( !pSound )
		CPtr< CFuncBase<CVec3> > pHold( pPosition );
	else
		Register( pScene->Add3DSound( pSound, pPosition, tStart ) );
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
