#ifndef __SOUND_H_
#define __SOUND_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "Time.h"

namespace NDb
{
	class CSound;
	class CMusic;
	class CSoundEffect;
	enum EMusicType : int;	// defined in DBFormat\DataSound.h (fixed underlying type for this opaque declaration)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTransformStack;
namespace NSound
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSound;
class CSoundEffect;
class ISound2D : public CObjectBase
{
public:
	virtual bool IsPlaying() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class ISoundScene: public CObjectBase
{
public:
	virtual CSound* Add3DSound( NDb::CSound *pSample, CFuncBase<CVec3> *pPos, STime tStart ) = 0;
	virtual ISound2D* Add2DSound( NDb::CSound *pSample ) = 0;
	virtual CSoundEffect* AddEffect( NDb::CSoundEffect *pEff, STime stBeginTime, CFuncBase<STime> *pTime, CFuncBase<CVec3> *pPos, const vector<int> &flags ) = 0;

	// retail ISoundScene::SetMusic(EMusicType) @0x304db0 (vtbl+0x1c): the edge-triggered music-type
	// switch CMission::UpdateSound drives every frame (retail @0x1feb50 passes MT_AMBIENT/MT_COMBAT).
	virtual void SetMusic( NDb::EMusicType eType ) = 0;
	// dev-facing adapter: adopts the record into the scene's ambient/combat slot (by its eType)
	// and drives the type machine above.
	virtual void SetMusic( NDb::CMusic *pMusic ) = 0;
	// maps onto the retail SetMusic(MT_AMBIENT) edge (retail FadeOutMusic @0x304c20 itself is the
	// internal data-driven fade the machine calls through vtbl+0x20).
	virtual void FadeOutMusic() = 0;

	virtual void Draw( CTransformStack *pTS ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NSound::CreateSoundScene @0x305ad0 takes BOTH slots (ambient, combat) -- the ctor
// (@0x3058c0) stores them and the type machine launches from them; without the combat slot a
// fresh scene could never start combat music (retail passes GetTMusic(3)-derived combat here).
ISoundScene* CreateSoundScene( NDb::CMusic *pAmbient, NDb::CMusic *pCombat = 0 );
bool InitSound( HWND hWnd );
bool SetModeFromConfig();
void DoneSound();
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif