#ifndef __CAMERA_H_
#define __CAMERA_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Time.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTransformStack;
////////////////////////////////////////////////////////////////////////////////////////////////////
const float	F_FOV = 35;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NInput
{
	struct SEvent;
}
enum ECameraType
{
	CAMERA_PC,
	CAMERA_MAYA,
	CAMERA_FIRSTPERSON
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// release CCamera reaches the world/view terrain-height grids through its CPtr<IWorld>/CPtr<IGameView>
// members (@+0xF4/+0xF8) for the per-frame focus-height easing (Update @0xcd930) and the eye lift-off
// (CorrectPlacement @0xccd60). This dev camera is world-agnostic, so the mission installs this thin
// height-sampling source instead (implemented over NWorld terrain in iMission.cpp).
class ICameraHeightSource: public CObjectBase
{
public:
	// averaged terrain height (world z) over a window of half-extent nHalf grid cells centred on
	// world XY (release EstimateAverageHeight @0xcc970); false when the sample is unavailable.
	virtual bool EstimateAverageHeight( float fWorldX, float fWorldY, int nHalf, float *pfAvg ) = 0;
	// bilinear terrain height (world z) at world XY (release GetHeight @0xccc10); false off-grid.
	virtual bool GetHeight( float fWorldX, float fWorldY, float *pfHeight ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class ICamera: public CObjectBase
{
public:
  struct SCameraPos
  {
    float fRod;
    float fPitch, fYaw, fRoll;
		float fFOV;
    CVec3 ptAnchor;
		SCameraPos() { memset( this, 0, sizeof(*this) ); fFOV = F_FOV; }
		SCameraPos( CVec3 _ptAnchor, float _fRod, float _fPitch, float _fYaw, float _fRoll, float _fFOV = F_FOV ) :
			ptAnchor( _ptAnchor ), fRod( _fRod ), fPitch( _fPitch ), fYaw( _fYaw ), fRoll( _fRoll ), fFOV( _fFOV ) {}
  };
	struct SCameraLimits
	{
		float fMinRod, fMaxRod, fMinPitch, fMaxPitch;
		CTRect<float> sZoneLimit;

		SCameraLimits(): fMinRod(-1000), fMaxRod(1000), fMinPitch( -FP_2PI ), fMaxPitch( FP_2PI ), sZoneLimit( -1000, -1000, 1000, 1000 )  {}
		SCameraLimits( float _fMinRod, float _fMaxRod, float _fMinPitch, float _fMaxPitch, const CTRect<float> &_sZoneLimit )
			: fMinRod(_fMinRod), fMaxRod(_fMaxRod), fMinPitch(_fMinPitch), fMaxPitch(_fMaxPitch), sZoneLimit( _sZoneLimit ) {}
	};

public:
	virtual CVec3 GetForwardDir() const = 0;
	virtual CVec3 GetStrafeDir() const = 0;

	virtual CVec3 GetCP() const = 0;
	virtual SHMatrix GetPos() const = 0;
	virtual void GetTransform( CTransformStack *pTS, const CVec2 &vScreenSize ) const = 0;
	// target rect for correct center position, in [0,1] range
	virtual const CTRect<float>& GetScreenRect() const = 0;
	virtual void SetScreenRect( const CTRect<float> &sRect ) = 0;

	virtual float GetFOV() const = 0;
	virtual void SetFOV( float fFOV ) = 0;

	// near/far clip planes (release added these; the CameraSetClipping script binding -> CUICmdSetCameraClipDistance
	// drives them). Default no-op so non-CBaseCamera implementations need not override.
	virtual void SetClipDistance( float fMin, float fMax ) {}
	virtual void GetClipDistance( float *pMin, float *pMax ) const {}

  virtual void GetPlacement( SCameraPos *pPlacement ) const = 0;
	virtual void SetPlacement( const SCameraPos &sPlacement ) = 0;

	// release ICamera vtbl[0x54] (CBaseCamera @0xcbbc0, CCamera @0xcd720): pan the ground anchor by a
	// world-space delta. The smoothed (non-immediate) path moves only the DESIRED placement so the
	// per-frame Update eases the live camera into it; bImmediate shifts both 1:1. Default no-op so
	// non-camera ICamera implementations need not override.
	virtual void ScrollAnchor( const CVec3 &vDelta, bool bImmediate, bool bOnTerrain ) {}

	virtual void GetLimits( SCameraLimits *pLimits ) const {}
	virtual void SetLimits( const SCameraLimits &sLimits ) {}

	// release ICamera vtbl[0x74] (CBaseCamera::Lock @0xcffa0): bump/unbump the scroll-lock COUNT.
	// While nLockCount >= 1, CCamera::Update (@0xcd930) mutes all PLAYER input (pan/rotate/zoom) and
	// CCamera::ScrollAnchor ignores scroll requests -- but scripted SetPlacement still works, which is
	// the CameraSet-while-CameraLock'ed case (the script CameraLock(b) -> CUICmdLockCamera dispatch).
	// Default no-op so non-tactical ICamera implementations need not override.
	virtual void SetLock( bool bLock ) {}

	// release SCameraLimits.bMovie (+0x10 in the widened 56-byte limits; the dev SCameraLimits is the
	// frozen 32-byte save-format one, so the flag lives OUT of the struct as a runtime-only camera
	// member). CMission::ExecWorldCommand @0x1fd8c0: BeginSequence (nSequence 0->1) saves the limits and
	// installs DEFAULT limits with bMovie=TRUE; the matching EndSequence restores the saved (bMovie-less)
	// limits. While bMovie, CCamera::Update (@0xcd930) SKIPS the whole terrain/approach tail (averaged-
	// height easing, CorrectPlacement, Approach2DesiredPlacement and the soft rod/pitch clamps) -- the
	// live placement moves ONLY via SetPlacement, so scripted CameraSet/CameraMove poses HOLD exactly.
	// Default no-op so non-tactical ICamera implementations need not override.
	virtual void SetMovieMode( bool bMovie ) {}

	// release ICamera vtbl[0x74] = CBaseCamera::FreezeCamera @0xcffc0 -- the SCRIPT CameraLock(b)
	// dispatch (vftable dump @0x4b529c: +0x70 = Lock @0xcffa0 scroll-count, +0x74 = FreezeCamera;
	// the round-4 SetLock routing was one slot off). A bare REFCOUNT (nLockNoUpdate, retail +0xd4):
	// no pose pin. While frozen (a) CCamera::Update bails before the whole movement/approach tail
	// (SetPlacement slot 0x50 stays ungated, so scripted CameraSet still lands -- the HQ per-room
	// cameras), and (b) retail SetCutFloor @0xd0050 is a hard NO-OP (`cmp [this+0xd4],0; jg ret`) --
	// THAT is the base one-floor lock: EBase's OnEnterZone calls CameraLock() once and never
	// unlocks, so the cut floor stays pinned; EFirst never calls it, so its 2 floors switch freely.
	virtual void FreezeCamera( bool bFreeze ) {}
	virtual bool IsCameraFrozen() const { return false; }

	virtual void Update( const STime &sTime ) = 0;
	virtual void ProcessEvent( const NInput::SEvent &eEvent ) = 0;

	// install the terrain-height source for the release terrain legs (see ICameraHeightSource);
	// default no-op so non-tactical cameras need not override.
	virtual void SetHeightSource( ICameraHeightSource *pSource ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
ICamera* CreateCamera( ECameraType eType = CAMERA_PC, float fCameraSpeed = 1.0f );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif