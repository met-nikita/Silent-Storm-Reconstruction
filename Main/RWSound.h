#ifndef __RWSOUND_H_
#define __RWSOUND_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class CSyncSrc;
namespace NWorld
{
	class IWorld;
	struct IVisObj;
//	class C3DSound;
//	class C2DSound;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NSound
{
	class ISoundScene;
	class CSound;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTransformStack;
namespace NRender
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class IRenderSound: public CObjectBase
{
public:
	virtual void Update( CTransformStack *pTS, STime currentTime ) = 0;
	virtual void ResetTiming() = 0;
	// retail IRenderSound vtbl+0x18: CRenderGame::UpdateVisible re-points the unit-sound mixer at the
	// same visibility-filtered source it hands rUnits (voice fog-of-war gate).
	virtual void SetNewSource( CSyncSrc<NWorld::IVisObj> *pSrc ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CreateRenderSound @0x2d5a10 / CRenderSound ctor @0x2d5770: the sync source is a CTOR PARAM
// built by the caller -- CRenderGame owns TWO mixers (GetActive-backed pSound for world/misc sounds,
// GetUnits-backed pUnitSounds for unit-emitted sounds) -- NOT the Jan03 inline
// union(GetActive,GetUnits), which mixed every unit sound regardless of player visibility.
IRenderSound* CreateRenderSound( CSyncSrc<NWorld::IVisObj> *pSrc, NSound::ISoundScene *pSoundScene );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif