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

	virtual void SetMusic( NDb::CMusic *pMusic ) = 0;
	virtual void FadeOutMusic() = 0;

	virtual void Draw( CTransformStack *pTS ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
ISoundScene* CreateSoundScene( NDb::CMusic *pAmbient );
bool InitSound( HWND hWnd );
bool SetModeFromConfig();
void DoneSound();
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif