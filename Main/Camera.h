#ifndef __CAMERA_H_
#define __CAMERA_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Time.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTransformStack;
class CObjectBase;
namespace NWorld { class IWorld; }
namespace NGScene { class IGameView; }
namespace NUI { class CInterface; }
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
// release keeps the tactical cut floor ON the camera (CBaseCamera +0x38, vtbl 0x44/0x48 GetCutFloor/
// SetCutFloor) -- the framing helpers (CanSeeOneRay ray floors, ShowTwoPlaces tail) read/write it. This dev
// camera relocated the cut floor to the render scene (IMission::Get/SetCutFloor -> CScene), so the mission
// installs this thin accessor over the single owner.
class ICameraCutFloor: public CObjectBase
{
public:
	virtual int  GetCutFloor() const = 0;
	virtual void SetCutFloor( int nFloor ) = 0;
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
	// retail ICamera::SCameraLimits -- the WIDENED 56-byte record (ctor @0xcb4d0; PDB layout, s2
	// oracle s2_camera.h:47). The old dev 32-byte struct carried sZoneLimit here; retail moved the
	// scroll-zone rect onto CBaseCamera itself (+0xC4, save tag 13, SetZoneLimits @0xcfff0) and
	// widened this struct with the movie flag + the camera-feel tuning fields. This is a SHARED,
	// SERIALIZED type (CBaseCamera tag 7 / CCamera sSoftLimits tag 6 / CMission cameraLimits tag 33
	// raw sizeof-blobs), so the widening IS the save-format change ("SCameraLimits 32->56 ripple").
	struct SCameraLimits
	{
		float fMinRod, fMaxRod, fMinPitch, fMaxPitch;	// +0x00..0x0C
		// +0x10: BeginSequence installs default limits with bMovie=TRUE (CMission @0x1fd8c0);
		// while set, CCamera::Update skips the whole terrain/approach tail so scripted poses HOLD.
		bool  bMovie;
		float fAttenuation;				// +0x14: Update ease dt scale (locked ease targets 0.5*this)
		float fMinHeight;				// +0x18: CorrectPlacement eye lift-off above terrain
		// The camera-feel tuning tail. Consumers: CCamera::ScrollAnchor @0xcd720 (scroll pair) and
		// CCamera::Update @0xcd930 (zoom/yaw/pitch); the *ZoomAcceleration fields feed the shared
		// zoom-out ramp GetAccelerationFactor @0xcbe70, which reads the SOFT rod limits.
		float fScrollZoomAcceleration;	// +0x1C
		float fScrollSpeed;				// +0x20
		float fZoomAcceleration;		// +0x24
		float fZoomSpeed;				// +0x28
		float fYawSpeed;				// +0x2C
		float fYawZoomAcceleration;		// +0x30
		float fPitchSpeed;				// +0x34

		// retail ctor @0xcb4d0 writes exactly these 13 values; bMovie is NOT initialized there
		// (fresh-heap zero in practice, and every live retail path stamps SetLimits from a
		// zero-.data default before any Update) -- pinned to false here to keep the observable
		// retail value deterministic.
		SCameraLimits(): fMinRod(-1000), fMaxRod(1000), fMinPitch( -FP_2PI ), fMaxPitch( FP_2PI ),
			bMovie( false ),
			fAttenuation( 1.0f ), fMinHeight( 4.0f ), fScrollZoomAcceleration( 1.2f ), fScrollSpeed( 1.0f ),
			fZoomAcceleration( 5.0f ), fZoomSpeed( 0.1f ), fYawSpeed( 1.0f ), fYawZoomAcceleration( 1.0f ),
			fPitchSpeed( 1.0f ) {}
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

	// retail ICamera vtbl+0x88 (CCamera @0xd0500): the cinematic slow-motion frame divider consumed by
	// CMission::Step's world-advance gate -- while > 1 the world advances only every Nth frame.
	// Non-cinematic cameras run at ratio 1.
	virtual int GetSloMoRatio() const { return 1; }

	// retail CBaseCamera::SetZoneLimits @0xcfff0 (ICamera virtual): the scroll-zone clamp rect --
	// it lives ON the camera (+0xC4, save tag 13) in the widened layout, NOT inside SCameraLimits.
	// The mission stamps it from the world's map safe zone. Default no-op so non-tactical ICamera
	// implementations need not override.
	virtual void SetZoneLimits( const CTRect<float> &sRect ) {}

	// retail CCamera::GetSoftLimits @0xd04a0 / SetSoftLimits @0xd04c0 (ICamera virtuals): the soft
	// limit set the ApplySoftDelta pullback uses (@0xcb930 -- that consumer is still the deferred
	// tuning leg); serialized on CCamera (tag 6) and stamped by the mission alongside the hard
	// limits (retail CMissionBase::CreateCamera @0x1a2390). Default no-op.
	virtual void GetSoftLimits( SCameraLimits *pLimits ) const {}
	virtual void SetSoftLimits( const SCameraLimits &sLimits ) {}

	// release ICamera vtbl[0x74] (CBaseCamera::Lock @0xcffa0): bump/unbump the scroll-lock COUNT.
	// While nLockCount >= 1, CCamera::Update (@0xcd930) mutes all PLAYER input (pan/rotate/zoom) and
	// CCamera::ScrollAnchor ignores scroll requests -- but scripted SetPlacement still works, which is
	// the CameraSet-while-CameraLock'ed case (the script CameraLock(b) -> CUICmdLockCamera dispatch).
	// Default no-op so non-tactical ICamera implementations need not override.
	virtual void SetLock( bool bLock ) {}

	// release ICamera vtbl[0x74] = CBaseCamera::FreezeCamera @0xcffc0 -- the SCRIPT CameraLock(b)
	// dispatch (vftable dump @0x4b529c: +0x70 = Lock @0xcffa0 scroll-count, +0x74 = FreezeCamera;
	// the round-4 SetLock routing was one slot off). A REFCOUNT (nLockNoUpdate, retail +0xd4): no
	// pose pin; freezing also clears the two unlock-intent flags and the body TAIL-CALLS vtbl+0x70
	// = Lock(bFreeze) (disasm @0x4cffec `jmp [edx+0x70]` with the bool re-stored to [esp+4]), so a
	// script CameraLock bumps BOTH counters. While frozen (a) CCamera::Update bails before the whole
	// movement/approach tail (SetPlacement slot 0x50 stays ungated, so scripted CameraSet still
	// lands -- the HQ per-room cameras), and (b) retail SetCutFloor @0xd0050 is a hard NO-OP
	// (`cmp [this+0xd4],0; jg ret`) -- THAT is the base one-floor lock: EBase's OnEnterZone calls
	// CameraLock() once and never unlocks, so the cut floor stays pinned; EFirst never calls it, so
	// its 2 floors switch freely.
	virtual void FreezeCamera( bool bFreeze ) {}
	virtual bool IsCameraFrozen() const { return false; }

	// retail ICamera vtbl+0x94 = CBaseCamera::UserWantedItToUnlock @0xd0020: the CONSUMER of the
	// serialized unlock-intent pair (CBaseCamera tags 15/16). One-shot: writes-and-clears
	// bUserJustWantedToUnlock into *pbJustWanted; returns bMustUnlockDueToUserDecision unless the
	// camera is script-frozen (nLockNoUpdate >= 1 -- FreezeCamera beats the user's intent). The one
	// caller is the CMissionBase::GetCamera selector @0x1a1ee0: it is what hands control back to the
	// PLAYER's camera when the user gives camera input during a locked/follow cinematic. Default
	// no-op so non-tactical ICamera implementations need not override.
	virtual bool UserWantedItToUnlock( bool *pbJustWanted ) { *pbJustWanted = false; return false; }

	// retail ICamera vtbl+0x3c/0x40 (CBaseCamera::SetCutFloorRange @0xcba10 / GetCutFloorRange
	// @0xcba40) and vtbl+0x44/0x48 (GetCutFloor @0xcff90 / SetCutFloor @0xd0050): the camera-owned
	// cut floor + its clamp range. The LIVE render floor stays on the scene in this dev tree (the
	// documented relocation above), but the camera-side slots are retail-serialized (CBaseCamera
	// tags 3/11/12) and retail stamps them per camera (CPlayerTracker ctor @0x287d70 deploy floor;
	// CMission::Initialize @0x200690 per-tracker variant range). Default no-ops.
	virtual void SetCutFloorRange( int nMin, int nMax ) {}
	virtual void GetCutFloorRange( int *pnMin, int *pnMax ) const { *pnMin = 0; *pnMax = 0; }
	virtual int  GetCutFloor() const { return 0; }
	virtual void SetCutFloor( int nFloor ) {}

	virtual void Update( const STime &sTime ) = 0;
	virtual void ProcessEvent( const NInput::SEvent &eEvent ) = 0;


	// release CCamera cinematic two-point framing: frame world points ptA (shooter) + ptB (target) from the
	// best-visible camera pose (ShowPlacesFromBestPoint @0xcf1c0, ICamera vtbl+0x5c), and latch a follow
	// target (FollowUnit @0xd0520). Default no-op so the menu / first-person / maya cameras need not override.
	virtual void ShowPlacesFromBestPoint( const CVec3 &ptA, const CVec3 &ptB, int nFloor, float fRodIn,
		int nSloMoRatio, float fDivisor, bool bKeepFollow, bool bForceRod ) {}
	virtual void FollowUnit( CObjectBase *pUnit ) {}
	// retail ICamera vtbl+0x7c (CBaseCamera::AddEarthQuake @0xce5d0): queue a grenade-blast screen
	// shake of the given amplitude (the point is accepted but unused by the base impl). Default no-op
	// so menu / first-person / maya cameras need not override.
	virtual void AddEarthQuake( const CVec3 &pt, float fAmplitude ) {}
	// the world handle (release CCamera +0xF4 -- the terrain raycast AND the height layers the terrain
	// leg samples) + the render cut-floor accessor, installed by the mission after world creation.
	// Default no-op for non-tactical cameras.
	virtual void SetWorld( NWorld::IWorld *pWorld ) {}
	virtual void SetCutFloorSource( ICameraCutFloor *pSource ) {}
	// the render-view handle (release CCamera pView +0xF8 / CFPCamera pView) and the UI handle
	// (release CFPCamera pUI): retail threads these through the CreateCamera factory ctor args
	// (@0xcf8e0 -- CreateCamera(CInterface*, IWorld*, IGameView*, ECameraType, float)); the dev
	// factory keeps its 2-arg signature (MapEdit/menu callers), so the mission installs them
	// post-creation like SetWorld. Both members are serialized (CCamera tag 3; CFPCamera tags 3/4).
	virtual void SetView( NGScene::IGameView *pView ) {}
	virtual void SetUI( NUI::CInterface *pUI ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
ICamera* CreateCamera( ECameraType eType = CAMERA_PC, float fCameraSpeed = 1.0f );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif