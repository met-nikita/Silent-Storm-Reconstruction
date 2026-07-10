#include "StdAfx.h"
#include "Camera.h"
#include "..\Misc\Geom.h"
#include "Transform.h"
#include "..\Input\Bind.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"      // NGlobal::GetVar / RegisterCmd / CValue (BUG 4 camera sensitivity)
#include "..\FileIO\BasicChunk1.h"    // START_REGISTER / FINISH_REGISTER
#include "wInterface.h"               // NWorld::IWorld::GetAIMap (the framing raycast world handle)
#include "aiMap.h"                    // NAI::IAIMap::Trace / CFloorsSet / SInterval (occlusion ray -- CanSeeOneRay)
#include "wTSFlags.h"                 // NWorld::TS_VISION (the vision trace-set flag)
#include "..\Misc\RandomGen.h"        // SRand (the framing's rod fan-sweep roll)
////////////////////////////////////////////////////////////////////////////////////////////////////
// BUG 4 -- camera sensitivity / invert config consumer. Retail UpdateCameraFromConfig @0xcd180 pushes the
// game_camerasensivity / game_scrollsensivity floats and the four invert flags into the per-command input
// coeffs via NInput::SetCommandCoeff @0x3d0880. The a5dll registered these options (iOptionsMenu) but with a
// NULL handler and never consumed them -- so sensitivity + invert did NOTHING. This restores the consumer,
// fired by the `camera_update` command (already invoked from the Options apply/reset paths) and once per
// camera at construction so it applies from game start.
// ORIGINAL BUG (retail @0xcd180): the two scroll axes are cross-wired -- camera_strafe (horizontal pan) is
// inverted by game_invertscrollY, camera_forward (vertical pan) by game_invertscrollX. Ported verbatim.
////////////////////////////////////////////////////////////////////////////////////////////////////
static void UpdateCameraFromConfig()
{
	float fCam = NGlobal::GetVar( "game_camerasensivity", NGlobal::CValue( 1.0f ) ).GetFloat();
	float fScr = NGlobal::GetVar( "game_scrollsensivity", NGlobal::CValue( 1.0f ) ).GetFloat();
	float sTurnX   = NGlobal::GetVar( "game_invertturnx"   ).GetFloat() != 0.f ? -1.0f : 1.0f;
	float sTurnY   = NGlobal::GetVar( "game_invertturny"   ).GetFloat() != 0.f ? -1.0f : 1.0f;
	float sScrollX = NGlobal::GetVar( "game_invertscrollx" ).GetFloat() != 0.f ? -1.0f : 1.0f;
	float sScrollY = NGlobal::GetVar( "game_invertscrolly" ).GetFloat() != 0.f ? -1.0f : 1.0f;
	NInput::SetCommandCoeff( "camera_zoom",    fCam );
	NInput::SetCommandCoeff( "camera_pitch",   fCam * sTurnY );
	NInput::SetCommandCoeff( "camera_rotate",  fCam * sTurnX );
	NInput::SetCommandCoeff( "camera_strafe",  fScr * sScrollY );   // ORIGINAL BUG @0xcd180: strafe <- invertscrollY
	NInput::SetCommandCoeff( "camera_forward", fScr * sScrollX );   // ORIGINAL BUG @0xcd180: forward <- invertscrollX
}
static void CommandCameraUpdate( const string &, const vector<wstring> &, void * ) { UpdateCameraFromConfig(); }
START_REGISTER(Camera)
	REGISTER_CMD( "camera_update", CommandCameraUpdate )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBaseCamera
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBaseCamera: public ICamera
{
protected:
	NInput::CBind fwd, strafe, zoom, pitch, rotate;
	ZDATA
  SCameraPos sPlacement;
	SCameraLimits sLimits;
	CTRect<float> sScreenRect;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sPlacement); f.Add(3,&sLimits); f.Add(4,&sScreenRect); return 0; }
	// NOTE: full CBaseCamera save-format parity (release adds nLockCount/nCutFloor/clip + a widened
	// SCameraLimits 32->56) is the separate, still-BLOCKED SCameraLimits ripple leg. These clip members are
	// runtime-only here (the menu camera is never saved); they are intentionally NOT in operator&.
	float fMinClipDistance;
	float fMaxClipDistance;
	// release CBaseCamera +0x34: the scroll-lock COUNT (CBaseCamera::Lock @0xcffa0; script CameraLock).
	// <= 0 = free. Runtime-only (save format frozen per the note above) -- a save made mid-CameraLock
	// reloads UNLOCKED; noted divergence (release serializes it), the scripts re-lock via their own flow.
	int nLockCount;
	// release SCameraLimits.bMovie (+0x10 of the widened 56-byte limits -- ctor @0xcb4d0 leaves it
	// UNinitialized; only CMission's BeginSequence limits set it TRUE). Kept OUT of the frozen 32-byte
	// dev SCameraLimits (save-format ripple leg still blocked), so it is a runtime-only member here,
	// driven by ICamera::SetMovieMode from the mission's Begin/EndSequence dispatch (@0x1fd8c0).
	bool bMovieMode;
	// release CBaseCamera +0xd4 nLockNoUpdate (FreezeCamera @0xcffc0 = script CameraLock, vtbl+0x74):
	// refcount; while >= 1 the Update movement tail bails and retail SetCutFloor @0xd0050 no-ops.
	// Runtime-only (save-format leg blocked); a save mid-CameraLock reloads unfrozen -- the zone
	// scripts re-lock in their own flow.
	int nFreezeCount;

public:
	CBaseCamera();

	// release @0xcffa0: faithful unclamped ++/-- (unpaired unlocks go negative, as in the release).
	void SetLock( bool bLock ) { if ( bLock ) ++nLockCount; else --nLockCount; }

	// see ICamera::SetMovieMode (Camera.h) -- the release carries this in SCameraLimits.bMovie.
	void SetMovieMode( bool bMovie ) { bMovieMode = bMovie; }

	// see ICamera::FreezeCamera (Camera.h) -- release @0xcffc0, unclamped like Lock.
	void FreezeCamera( bool bFreeze ) { if ( bFreeze ) ++nFreezeCount; else --nFreezeCount; }
	bool IsCameraFrozen() const { return nFreezeCount > 0; }

	void GetTransform( CTransformStack *pTS, const CVec2 &vScreenSize ) const;

	const CTRect<float>& GetScreenRect() const;
	void SetScreenRect( const CTRect<float> &sRect );

	float GetFOV() const;
	void SetFOV( float fFOV );

	void SetClipDistance( float fMin, float fMax ) { fMinClipDistance = fMin; fMaxClipDistance = fMax; }
	void GetClipDistance( float *pMin, float *pMax ) const { *pMin = fMinClipDistance; *pMax = fMaxClipDistance; }

  void GetPlacement( SCameraPos *pPlacement ) const;
	void SetPlacement( const SCameraPos &sPlacement );

	// release @0xcbbc0: the BASE implementation shifts the live anchor 1:1 (both flags ignored here;
	// only the CCamera override honours them).
	void ScrollAnchor( const CVec3 &vDelta, bool bImmediate, bool bOnTerrain );

	void GetLimits( SCameraLimits *pLimits ) const;
	void SetLimits( const SCameraLimits &sLimits );

	void ProcessEvent( const NInput::SEvent &eEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CBaseCamera::CBaseCamera():
	sScreenRect( 0, 0, 1, 1 ),
	fwd( "camera_forward" ), strafe( "camera_strafe" ), zoom( "camera_zoom" ), pitch ( "camera_pitch" ), rotate( "camera_rotate" )
{
	sPlacement.ptAnchor = VNULL3;
	sPlacement.fRod = 30;
	sPlacement.fPitch = ToRadian( -70.0f ); 
	sPlacement.fYaw = 0;
	sPlacement.fRoll = 0;
	sPlacement.fFOV = F_FOV;
	fMinClipDistance = 0.1f;		// release CBaseCamera ctor defaults (matches the old hardcoded MakeProjective values)
	fMaxClipDistance = 100;
	nLockCount = 0;
	bMovieMode = false;
	nFreezeCount = 0;
	// BUG 4: apply the sensitivity/invert config to this camera's command coeffs at construction (the
	// CBind members above have just registered camera_forward/strafe/zoom/pitch/rotate in the global
	// commands map), so the options take effect from game start -- not only after the Options screen.
	UpdateCameraFromConfig();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::GetTransform( CTransformStack *pTS, const CVec2 &vScreenSize ) const
{
	float fShiftX = ( sScreenRect.x1 + sScreenRect.x2 ) - 1;
	float fShiftY = ( sScreenRect.y1 + sScreenRect.y2 ) - 1;
	pTS->MakeProjective( vScreenSize, sPlacement.fFOV, fMinClipDistance, fMaxClipDistance, CVec2( fShiftX, -fShiftY ) );
	pTS->SetCamera( GetPos() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CTRect<float>& CBaseCamera::GetScreenRect() const
{
	return sScreenRect;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::SetScreenRect( const CTRect<float> &sRect )
{
	sScreenRect = sRect;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CBaseCamera::GetFOV() const
{
	return sPlacement.fFOV;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::SetFOV( float _fFOV )
{
	sPlacement.fFOV = _fFOV;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::GetPlacement( SCameraPos *pPlacement ) const
{
	ASSERT( pPlacement );
	*pPlacement = sPlacement;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::SetPlacement( const SCameraPos &_sPlacement )
{
	sPlacement = _sPlacement;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::ScrollAnchor( const CVec3 &vDelta, bool /*bImmediate*/, bool /*bOnTerrain*/ )
{
	// release @0xcbbc0: unconditional live-anchor shift; no desired-placement move, no terrain snap.
	sPlacement.ptAnchor += vDelta;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::GetLimits( SCameraLimits *pLimits ) const
{
	ASSERT( pLimits );
	*pLimits = sLimits;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::SetLimits( const SCameraLimits &_sLimits )
{
	sLimits = _sLimits;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseCamera::ProcessEvent( const NInput::SEvent &eEvent )
{
	fwd.ProcessEvent( eEvent );
	strafe.ProcessEvent( eEvent );
	zoom.ProcessEvent( eEvent );
	pitch.ProcessEvent( eEvent );
	rotate.ProcessEvent( eEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCamera
////////////////////////////////////////////////////////////////////////////////////////////////////
// release Accelerate @0xcb6b0: exponential approach of fCurrent toward fDesired -- the remaining
// offset decays by factor fBase every second (fBase^(0.001*dtMs)); within fEps of the target it
// settles (returns fCurrent unchanged).
static float Accelerate( float fDesired, float fCurrent, float fBase, float fDtMs, float fEps )
{
	if ( fabs( fDesired - fCurrent ) < fEps )
		return fCurrent;
	return fDesired + ( fCurrent - fDesired ) * (float)pow( fBase, 0.001 * fDtMs );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release Approach2DesiredPlacement @0xcb710: blend the live placement toward the desired one --
// the release camera's "start fast, decelerate in" chase. Rod decays with base 1/300 (deadzone
// 0.02); pitch/yaw/roll with base 1/9000 (deadzone 0.01, yaw first unwrapped into the desired's
// +-pi range); the anchor decays with base 1/9000 (0.00011111111f in the binary, caller's eps);
// FOV is not eased.
static void Approach2DesiredPlacement( ICamera::SCameraPos *pCur, const ICamera::SCameraPos &sDesired, float fDtMs, float fEps )
{
	if ( !( fabs( sDesired.fRod - pCur->fRod ) < 0.02f ) )
		pCur->fRod = sDesired.fRod + ( pCur->fRod - sDesired.fRod ) * (float)pow( 1.0 / 300.0, 0.001 * fDtMs );
	const float fAngleBlend = (float)pow( 1.0 / 9000.0, 0.001 * fDtMs );
	if ( !( fabs( sDesired.fPitch - pCur->fPitch ) < 0.01f ) )
		pCur->fPitch = sDesired.fPitch + ( pCur->fPitch - sDesired.fPitch ) * fAngleBlend;
	while ( pCur->fYaw - sDesired.fYaw > FP_PI )
		pCur->fYaw -= FP_2PI;
	while ( pCur->fYaw - sDesired.fYaw < -FP_PI )
		pCur->fYaw += FP_2PI;
	if ( !( fabs( sDesired.fYaw - pCur->fYaw ) < 0.01f ) )
		pCur->fYaw = sDesired.fYaw + ( pCur->fYaw - sDesired.fYaw ) * fAngleBlend;
	if ( !( fabs( sDesired.fRoll - pCur->fRoll ) < 0.01f ) )
		pCur->fRoll = sDesired.fRoll + ( pCur->fRoll - sDesired.fRoll ) * fAngleBlend;
	pCur->ptAnchor.x = Accelerate( sDesired.ptAnchor.x, pCur->ptAnchor.x, 1.0f / 9000.0f, fDtMs, fEps );
	pCur->ptAnchor.y = Accelerate( sDesired.ptAnchor.y, pCur->ptAnchor.y, 1.0f / 9000.0f, fDtMs, fEps );
	pCur->ptAnchor.z = Accelerate( sDesired.ptAnchor.z, pCur->ptAnchor.z, 1.0f / 9000.0f, fDtMs, fEps );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCamera: public CBaseCamera
{
	OBJECT_BASIC_METHODS(CCamera);
private:
	ZDATA_(CBaseCamera)
	ZEND int operator&( CStructureSaver &f )
	{
		f.Add(1,(CBaseCamera*)this);
		// release serializes its desired placement too, but that is the frozen CBaseCamera
		// save-format parity leg -- re-seed the transient from the restored live placement instead.
		if ( f.IsReading() )
		{
			sDesiredPlacement = sPlacement;
			sLastUpdateTime = 0xFFFFFFFF;
		}
		return 0;
	}
	// release CCamera +0xFC: the DESIRED placement all user input drives; the live sPlacement eases
	// toward it every Update (@0xcd930). Runtime-only here (NOT in operator& -- save format frozen,
	// see the CBaseCamera clip-member note above).
	SCameraPos sDesiredPlacement;
	STime sLastUpdateTime;		// release CCamera +0x11C sLastTime (0xffffffff = "no frame yet" sentinel, ctor @0xce8e0)
	// release CCamera +0x15C fAttenuation (the WORKING attenuation): while script-LOCKED, Update eases
	// the live placement with dt * this ramping factor (toward sLimits.fAttenuation*0.5, i.e. 0.5 at the
	// frozen dev limits) so a scripted CameraSet settles in smoothly. Runtime-only. The release resets it
	// via the SlowCameraAcceleration settle (@0xd03c0, absent here); the dev resets it on full unlock so
	// each lock episode ramps in afresh.
	float fLockAttenuation;
	// release CCamera +0xF4 CPtr<IWorld> (the terrain-leg world handle); the mission installs the
	// dev height source after world creation. Runtime-only (re-installed on every mission init).
	CPtr<ICameraHeightSource> pHeightSource;

	// release CCamera cinematic two-point framing state (ShowPlacesFromBestPoint @0xcf1c0 chain). The FOV
	// effect struct is reduced to the fields the framing touches (release +0x18C is wider -- the FOV-spring
	// members drive Update's separate effect, not ported here). Runtime-only (not serialized).
	struct SCameraSloMo     { int nSloMo; int tOn; STime tMaxLen; SCameraSloMo(): nSloMo(1), tOn(0), tMaxLen(0) {} };
	struct SCameraFOVEffect { int nSloMo; float fFOV; float fRoll; SCameraFOVEffect(): nSloMo(1), fFOV(35.0f), fRoll(0) {} };
	SCameraPos             sPlacementToAccelerateTo;   // release +0x160 (SlowCameraAcceleration compares/copies)
	SCameraSloMo           sloMo;                       // release +0x180
	SCameraFOVEffect       fov;                          // release +0x18C
	CPtr<CObjectBase>      pFollowUnit;                 // release +0x1C4
	STime                  sMaxFollowUnitTime;          // release +0x1C8 (= sLastTime + 10000)
	CPtr<NWorld::IWorld>   pWorld;                       // release +0x0F4 (the raycast + terrain world handle)
	CPtr<ICameraCutFloor>  pCutFloorSource;             // the render cut-floor accessor (see ICameraCutFloor)
	//
	int  CutFloorGet() const { return IsValid( pCutFloorSource ) ? pCutFloorSource->GetCutFloor() : 0; }
	void CutFloorSet( int nF ) { if ( IsValid( pCutFloorSource ) ) pCutFloorSource->SetCutFloor( nF ); }
	bool CanSeeOneRay( const CVec3 &target ) const;                             // release @0xceb20
	bool CanSeeNow( const CVec3 &target );                                      // release @0xcecd0
	int  TestCurrentDesiredPosition( const CVec3 &p1, const CVec3 &p2 );        // release @0xcee10
	int  TryShowPlaces( const CVec3 &ptA, const CVec3 &ptB, int nFloor, float fRod, float fYaw );  // @0xcee90
	int  ShowTwoPlaces( const CVec3 &ptA, const CVec3 &ptB, int nFloor, float fRodIn );            // @0xceff0
	void SlowCameraAcceleration();                                              // release @0xd03c0

	void CorrectPlacement( SCameraPos &pos ) const;   // release @0xccd60 (world-heightmap grid pass)

public:
	CCamera();

	CVec3 GetForwardDir() const;
	CVec3 GetStrafeDir() const;

	CVec3 GetCP() const;
	SHMatrix GetPos() const;

	void SetPlacement( const SCameraPos &sPlacement );
	void ScrollAnchor( const CVec3 &vDelta, bool bImmediate, bool bOnTerrain );

	// on a full unlock, reset the working locked-ease attenuation so the next lock ramps in afresh
	void SetLock( bool bLock ) { CBaseCamera::SetLock( bLock ); if ( nLockCount <= 0 ) fLockAttenuation = 0; }

	void Update( const STime &sTime );
	virtual void SetHeightSource( ICameraHeightSource *pSource ) { pHeightSource = pSource; }
	virtual void SetWorld( NWorld::IWorld *_pWorld ) { pWorld = _pWorld; }
	virtual void SetCutFloorSource( ICameraCutFloor *pSource ) { pCutFloorSource = pSource; }
	virtual void ShowPlacesFromBestPoint( const CVec3 &ptA, const CVec3 &ptB, int nFloor, float fRodIn,
		int nSloMoRatio, float fDivisor, bool bKeepFollow, bool bForceRod );   // release @0xcf1c0
	virtual void FollowUnit( CObjectBase *pUnit );                             // release @0xd0520
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CCamera::CCamera():
	sLastUpdateTime( 0xFFFFFFFF ), fLockAttenuation( 0 ), sMaxFollowUnitTime( 0 )
{
	// release ctor @0xce6d0/@0xce8e0: the desired placement starts equal to the live default pose (the
	// sloMo/fov/pFollowUnit framing state defaults via their own ctors -- nSloMo=1, fFOV=35, follow=null).
	sDesiredPlacement = sPlacement;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// The cinematic two-point framing chain (release Camera.obj: ShowPlacesFromBestPoint @0xcf1c0 ->
// ShowTwoPlaces @0xceff0 -> TryShowPlaces @0xcee90 -> TestCurrentDesiredPosition @0xcee10 -> CanSeeNow
// @0xcecd0 -> CanSeeOneRay @0xceb20). Constants read from Game.exe .rdata (see the port dossier). The
// frustum test reuses CBaseCamera::GetTransform + CTransformStack::IsIn; the occlusion ray reuses the world
// AIMap TS_VISION trace; the terrain lift reuses CorrectPlacement + the height source -- all already present.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace {
// release AngleDiff @0xcbee0: smallest unsigned angular distance in [0, PI].
float AngleDiff( float a, float b )
{
	float d = fabs( a - b );
	while ( d > FP_2PI ) d -= FP_2PI;
	if ( d > FP_PI ) d = FP_2PI - d;
	return d;
}
// release CBaseCamera::MakeCameraPosition @0xcbbf0: midpoint-anchor placement, roll 0, FOV 35.
void MakeCameraPosition( const CVec3 &p1, const CVec3 &p2, float fRod, float fPitch, float fYaw,
	ICamera::SCameraPos &out )
{
	out.fRoll = 0;
	out.fFOV = 35.0f;
	out.ptAnchor = ( p1 + p2 ) * 0.5f;
	out.fRod = fRod;
	out.fYaw = fYaw;
	out.fPitch = fPitch;
}
// release CalcPitch @0xccba0: pitch (radians) from rod -- Clamp(rod*-2.125, -85, -20) degrees.
float CalcPitch( float fRod )
{
	return Clamp( fRod * -2.125f, -85.0f, -20.0f ) * 0.017453292519943295f;
}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CanSeeOneRay @0xceb20: LOS ray from the camera eye to `target`, blocked iff a vision hit sits before the
// 0.5-unit slack at the target, over floors [floor-1, floor].
bool CCamera::CanSeeOneRay( const CVec3 &target ) const
{
	if ( !IsValid( pWorld ) )
		return true;
	CVec3 cp = GetCP();
	CVec3 d = target - cp;
	float fLen = fabs( d );
	if ( fLen <= 0 )
		return true;
	CRay ray;
	ray.ptOrigin = cp;
	ray.ptDir = d * ( 1.0f / fLen );          // NORMALIZED direction
	const int nF = CutFloorGet();
	vector<int> vf;
	vf.push_back( nF );
	vf.push_back( nF - 1 );                    // release CFloorsSet(floor, floor-1)
	NAI::CFloorsSet floors( vf );
	vector<NAI::SInterval> hits;
	pWorld->GetAIMap()->Trace( ray, &hits, NWorld::TS_VISION, floors );   // release IWorld[+0x20]->[+0x18], flag 0x20
	for ( int k = 0; k < (int)hits.size(); ++k )
	{
		const float fT = hits[k].enter.fT;
		if ( fT >= 0.0f && ( fLen - 0.5f ) >= fT )
			return false;                       // blocked before the target
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CanSeeNow @0xcecd0: can the camera, AT ITS DESIRED PLACEMENT, see `target`? (frustum-in && LOS, with a
// 1.7-unit eye-raise retry). Snaps live=desired for the transform/eye query, then restores.
bool CCamera::CanSeeNow( const CVec3 &target )
{
	if ( sDesiredPlacement.fRod < 8.0f )        // zoom gate
		return false;
	SCameraPos saved = sPlacement;
	sPlacement = sDesiredPlacement;
	CTransformStack ts;
	GetTransform( &ts, CVec2( 4.0f, 3.0f ) );   // 4:3 aspect
	bool bResult = false;
	if ( ts.IsIn( SSphere( target, -1.5f ) ) )  // well-inside frustum (negative radius)
	{
		if ( CanSeeOneRay( target ) )
			bResult = true;
		else
		{
			CVec3 raised = target;
			raised.z += 1.7f;                   // eye height retry
			bResult = CanSeeOneRay( raised );
		}
	}
	sPlacement = saved;
	return bResult;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// TestCurrentDesiredPosition @0xcee10: 1 = single point visible, 2 = both visible, 0 = none.
int CCamera::TestCurrentDesiredPosition( const CVec3 &p1, const CVec3 &p2 )
{
	if ( p1.x == p2.x && p1.y == p2.y && p1.z == p2.z )
		return CanSeeNow( p1 ) ? 1 : 0;
	if ( CanSeeNow( p1 ) && CanSeeNow( p2 ) )
		return 2;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// TryShowPlaces @0xcee90: build one desired placement at (fRod, fYaw), terrain-correct the anchor+eye, test.
int CCamera::TryShowPlaces( const CVec3 &ptA, const CVec3 &ptB, int /*nFloor*/, float fRod, float fYaw )
{
	MakeCameraPosition( ptA, ptB, fRod, CalcPitch( fRod ), fYaw, sDesiredPlacement );
	if ( IsValid( pHeightSource ) )
	{
		float fAvg = 0;
		if ( pHeightSource->EstimateAverageHeight( sDesiredPlacement.ptAnchor.x,
		                                           sDesiredPlacement.ptAnchor.y, 2, &fAvg ) )   // window half-extent 2
			sDesiredPlacement.ptAnchor.z = fAvg;
		CorrectPlacement( sDesiredPlacement );   // world-heightmap eye lift-off (the 2nd building-grid pass has no dev reach)
	}
	int nRes = TestCurrentDesiredPosition( ptA, ptB );
	if ( nRes > 0 )
		SlowCameraAcceleration();
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ShowTwoPlaces @0xceff0: frame BOTH points -- fast path, then a two-yaw x rod-candidate sweep.
int CCamera::ShowTwoPlaces( const CVec3 &ptA, const CVec3 &ptB, int nFloor, float fRodIn )
{
	if ( CanSeeNow( ptA ) && CanSeeNow( ptB ) && sDesiredPlacement.fRod >= F_FOV )
	{
		if ( nFloor != CutFloorGet() ) CutFloorSet( nFloor );
		return 1;
	}
	float fBase = atan2f( ptB.y - ptA.y, ptB.x - ptA.x ) + 0.5235988f;   // + PI/6
	float fOpp  = fBase + FP_PI;
	float fPrimary = fBase, fSecondary = fOpp;
	if ( AngleDiff( sDesiredPlacement.fYaw, fBase ) >= AngleDiff( sDesiredPlacement.fYaw, fOpp ) )
	{
		fPrimary = fOpp;
		fSecondary = fBase;
	}
	const float rods[3] = { 15.0f, fRodIn, ( fRodIn + 40.0f ) * 0.5f };
	int r;
	for ( int i = 0; i < 3; i++ ) { r = TryShowPlaces( ptA, ptB, nFloor, rods[i], fPrimary );   if ( r ) return r; }
	for ( int i = 0; i < 3; i++ ) { r = TryShowPlaces( ptA, ptB, nFloor, rods[i], fOpp );        if ( r ) return r; }
	r = TryShowPlaces( ptA, ptB, nFloor, 40.0f, fPrimary ); if ( r ) return r;
	for ( int i = 0; i < 3; i++ ) { r = TryShowPlaces( ptA, ptB, nFloor, rods[i], fSecondary );  if ( r ) return r; }
	return TryShowPlaces( ptA, ptB, nFloor, 40.0f, fOpp );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SlowCameraAcceleration @0xd03c0: one-shot "settled" reset -- fires only once the live ease has caught the
// desired anchor+yaw (fully ramped attenuation and exact-equal anchor/yaw).
void CCamera::SlowCameraAcceleration()
{
	if ( fLockAttenuation < 0.5f )   // release: 0.5*fAttenuation(=1.0) <= fLockAttenuation
		return;
	if ( sDesiredPlacement.ptAnchor.x != sPlacementToAccelerateTo.ptAnchor.x ) return;
	if ( sDesiredPlacement.ptAnchor.y != sPlacementToAccelerateTo.ptAnchor.y ) return;
	if ( sDesiredPlacement.ptAnchor.z != sPlacementToAccelerateTo.ptAnchor.z ) return;
	if ( sDesiredPlacement.fYaw       != sPlacementToAccelerateTo.fYaw )       return;
	fLockAttenuation = 0;
	sloMo.nSloMo = 1;
	pFollowUnit = 0;
	fov.fRoll = 0;
	fov.nSloMo = 1;
	fov.fFOV = 35.0f;
	sPlacementToAccelerateTo = sDesiredPlacement;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// FollowUnit @0xd0520: latch a follow target with a 10 s lease.
void CCamera::FollowUnit( CObjectBase *pUnit )
{
	pFollowUnit = pUnit;
	sMaxFollowUnitTime = sLastUpdateTime + 10000;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ShowPlacesFromBestPoint @0xcf1c0: reset the cinematic state, try to frame both points, else fan-sweep
// around ptA for a single visible pose. (The random cinematic slo-mo -- nSloMoRatio>1 && bCheatSloMo -- is
// gated by the slo-mo cheat, which this dev build has no camera support for, so that branch stays inert; the
// framing search below is independent of it.)
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCamera::ShowPlacesFromBestPoint( const CVec3 &ptA, const CVec3 &ptB, int nFloor, float fRodIn,
	int nSloMoRatio, float fDivisor, bool bKeepFollow, bool bForceRod )
{
	sloMo.nSloMo = 1; fov.nSloMo = 1; fov.fFOV = 35.0f; fov.fRoll = 0.0f;   // (1) reset
	if ( !bKeepFollow )
		pFollowUnit = 0;                                                     // (2) release follow

	SCameraPos backup = sDesiredPlacement;                                  // (5) backup
	bool bSame = ( ptB.x == ptA.x && ptB.y == ptA.y && ptB.z == ptA.z );
	if ( bSame || ShowTwoPlaces( ptA, ptB, nFloor, fRodIn ) == 0 )          // (6) frame both, else fan-sweep ptA
	{
		int nSloMo = sloMo.nSloMo;
		sDesiredPlacement = backup;
		if ( nSloMo == 1 && sDesiredPlacement.fRod < 30.0f && CanSeeNow( ptA ) )
		{
			if ( nFloor != CutFloorGet() ) CutFloorSet( nFloor );
			return;                                                          // already framed
		}
		const float fBaseYaw = sDesiredPlacement.fYaw;
		SRand rnd;
		const float fRod = ( !bForceRod && rnd.Get( 3 ) != 1 ) ? 12.0f : fRodIn;
		const float kPi4 = 0.7853982f, kPi3 = 1.0471976f, kPi2 = 1.5707964f;
		bool bAllZero =
			TryShowPlaces( ptA, ptA, nFloor, fRod,   fBaseYaw )        == 0 &&
			TryShowPlaces( ptA, ptA, nFloor, fRod,   fBaseYaw + kPi4 ) == 0 &&
			TryShowPlaces( ptA, ptA, nFloor, fRod,   fBaseYaw - kPi4 ) == 0 &&
			TryShowPlaces( ptA, ptA, nFloor, fRodIn, fBaseYaw )        == 0 &&
			TryShowPlaces( ptA, ptA, nFloor, fRodIn, fBaseYaw + kPi3 ) == 0 &&
			TryShowPlaces( ptA, ptA, nFloor, fRodIn, fBaseYaw - kPi3 ) == 0 &&
			TryShowPlaces( ptA, ptA, nFloor, 40.0f,  fBaseYaw )        == 0 &&
			TryShowPlaces( ptA, ptA, nFloor, 40.0f,  fBaseYaw + kPi3 ) == 0 &&
			TryShowPlaces( ptA, ptA, nFloor, 40.0f,  fBaseYaw - kPi3 ) == 0 &&
			TryShowPlaces( ptA, ptA, nFloor, fRod,   fBaseYaw + kPi2 ) == 0 &&
			TryShowPlaces( ptA, ptA, nFloor, fRod,   fBaseYaw - kPi2 ) == 0;
		if ( bAllZero )
		{
			sDesiredPlacement = backup;
			sloMo.nSloMo = 1;
			return;
		}
		if ( nFloor != CutFloorGet() ) CutFloorSet( nFloor );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CCamera::GetForwardDir() const
{
	CVec3 ptRes;
	CQuat q = CQuat( sPlacement.fYaw, V3_AXIS_Z ) * CQuat( sPlacement.fPitch, V3_AXIS_X );
	q.GetYAxis( &ptRes );
	return ptRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CCamera::GetStrafeDir() const
{
	CVec3 ptRes;
	CQuat q = CQuat( sPlacement.fYaw, V3_AXIS_Z ) * CQuat( sPlacement.fPitch, V3_AXIS_X ) * CQuat( sPlacement.fRoll, V3_AXIS_Y );
	q.GetXAxis( &ptRes );
	return ptRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CCamera::GetCP() const
{
	return sPlacement.ptAnchor - GetForwardDir() * sPlacement.fRod;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SHMatrix CCamera::GetPos() const
{
	SHMatrix res;
	MakeMatrix( &res, sPlacement.fPitch, sPlacement.fYaw, sPlacement.fRoll, GetCP() );
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release sLimits.fMinHeight (widened-SCameraLimits ctor default 4.0, s2 oracle s2_camera.h:65).
// The widened limits are the deferred tuning leg, so the release default is a constant here.
static const float F_CAMERA_MIN_HEIGHT = 4.0f;
// release CCamera::CorrectPlacement @0xccd60 (world-heightmap grid pass): push the placement back /
// tilt it up so the camera EYE never sinks below fMinHeight above the terrain. The anchor and yaw
// stay untouched; rod + pitch are recomputed so the eye lifts to terrain+minHeight at the same XY.
// (Release runs a second pass against the VIEW grid -- the building-aware height grid the dev
// camera has no reach to; the world-heightmap pass covers the reported terrain cases.)
void CCamera::CorrectPlacement( SCameraPos &pos ) const
{
	if ( !IsValid( pHeightSource ) )
		return;
	CVec3 fwd;
	CQuat q = CQuat( pos.fYaw, V3_AXIS_Z ) * CQuat( pos.fPitch, V3_AXIS_X );
	q.GetYAxis( &fwd );
	const float fRod = pos.fRod;
	const float fDX = fwd.x * fRod;                    // anchor - eye offsets
	const float fDY = fwd.y * fRod;
	const float fEyeX = pos.ptAnchor.x - fDX;
	const float fEyeY = pos.ptAnchor.y - fDY;
	const float fEyeZ = pos.ptAnchor.z - fwd.z * fRod;

	float fTerrain = 0;
	if ( !pHeightSource->GetHeight( fEyeX, fEyeY, &fTerrain ) )
		return;                                        // off-grid: leave the placement alone
	if ( !( fTerrain > fEyeZ - F_CAMERA_MIN_HEIGHT ) )
		return;                                        // eye already clears the terrain

	const float fNewEyeZ = fTerrain + F_CAMERA_MIN_HEIGHT;
	const float fDZ = pos.ptAnchor.z - fNewEyeZ;
	const float fNewRod = sqrt( fDX * fDX + fDY * fDY + fDZ * fDZ );
	pos.fRod = fNewRod;
	if ( fabs( fNewRod ) > FP_EPSILON )
	{
		const float fHoriz = sqrt( fDX * fDX + fDY * fDY );
		const float fPitch = acos( fHoriz / fNewRod );
		pos.fPitch = ( fDZ < 0 ) ? -fPitch : fPitch;   // Sign(dz) * acos(horiz/rod)
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCamera::SetPlacement( const SCameraPos &_sPlacement )
{
	// release @0xcd6e0 (the CCamera vtbl[0x50] override): write the DESIRED placement, then snap the
	// live placement onto it -- explicit placements BYPASS the easing (scripted camera moves,
	// focus-on-unit, mission start, player switch, freeze restore). The release's optional
	// SetOnTerrain(bForce) snap needs the world height grids the camera has no access to here.
	sDesiredPlacement = _sPlacement;
	CBaseCamera::SetPlacement( sDesiredPlacement );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCamera::ScrollAnchor( const CVec3 &vDelta, bool bImmediate, bool bOnTerrain )
{
	// release @0xcd720: an immediate move shifts BOTH the live and desired anchors 1:1; a smoothed
	// move shifts only the DESIRED anchor so Update eases the camera into it. While script-LOCKED
	// (nLockCount >= 1) the release ignores the scroll entirely (it only latches a "user wants to
	// unlock" intent -- no dev consumer, omitted). Other omitted release bits: the SetOnTerrain
	// re-clamp (world height grids) and the smoothed-branch GetAccelerationFactor(
	// fScrollZoomAcceleration) * fScrollSpeed input scale (@0xcbe70 -- the deferred SCameraLimits
	// 32->56 tuning leg; the factor is 1.0 at the release ctor defaults).
	if ( nLockCount > 0 )
		return;
	if ( bImmediate )
	{
		sPlacement.ptAnchor += vDelta;
		sDesiredPlacement.ptAnchor += vDelta;
	}
	else
		sDesiredPlacement.ptAnchor += vDelta;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCamera::Update( const STime &sTime )
{
	// release @0xcd930: user input mutates the DESIRED placement; the tail then eases the live
	// placement toward it (Approach2DesiredPlacement @0xcb710). The release terrain legs (averaged-
	// height anchor easing + CorrectPlacement) need the world/view height grids the camera cannot
	// reach here -- the Jan03 stand-on-ground fix below keeps standing in for them.
	float fPitchDelta = pitch.GetDelta();
	float fYawDelta = rotate.GetDelta();
	float fFwd = fwd.GetDelta() * 10.0f;
	float fStrafe = strafe.GetDelta() * 10.0f;
	float fZoom = zoom.GetDelta() * 10.0f;	// dev input scale kept; release scales the rod step by fZoomSpeed*(|rod|*fZoomAcceleration+1) -- deferred SCameraLimits tuning leg
	//ASSERT( fabs(fZoom) < 5 );
	if ( fZoom < -5 )
		fZoom = -5;
	if ( fZoom > 5 )
		fZoom = 5;

	// release @0xcd930: frame dt clamped to [0,100] ms (0xffffffff sentinel -> first tick applies nothing)
	STime sDelta = 0;
	if ( sLastUpdateTime != 0xFFFFFFFF )
		sDelta = Min( sTime - sLastUpdateTime, (STime)100 );
	sLastUpdateTime = sTime;

	if ( sDelta > 0 )
	{
		// release FreezeCamera @0xcffc0 (nLockNoUpdate, the SCRIPT CameraLock -- vtbl+0x74): while
		// frozen the ENTIRE movement/approach tail is skipped -- no input, no terrain easing, no
		// Approach2DesiredPlacement. SetPlacement stays ungated, so scripted CameraSet still lands
		// and HOLDS exactly (the HQ per-room static cameras).
		if ( nFreezeCount > 0 )
		{
			sLastUpdateTime = sTime;
			return;
		}
		if ( nLockCount > 0 )
		{
			// release @0xcd930 (s2 oracle CCamera_Update step 6): while scroll-LOCKED (vtbl+0x70) all
			// five input deltas are zeroed -- the PLAYER cannot mutate the desired placement -- but
			// scripted SetPlacement (which snaps desired+live) still works, and the terrain easing +
			// approach below keep running so a scripted pose settles smoothly.
			fPitchDelta = 0;
			fYawDelta = 0;
			fFwd = 0;
			fStrafe = 0;
			fZoom = 0;
		}
		else
		{
		// release @0xcd930: dominant-axis filter -- a strong rotate mutes pitch and vice versa.
		// (The release also scales yaw by GetAccelerationFactor(fYawZoomAcceleration)*fYawSpeed and
		// pitch by fPitchSpeed -- deferred SCameraLimits tuning leg, both 1.0 at ctor defaults.)
		if ( fabs( fYawDelta ) > fabs( fPitchDelta ) * 10.0f )
			fPitchDelta = 0;
		if ( fabs( fPitchDelta ) > fabs( fYawDelta ) * 10.0f )
			fYawDelta = 0;
		}

		// pitch/yaw mutate the DESIRED placement (release: ApplySoftDelta @0xcb930 over the pitch
		// range -- soft limits are the deferred tuning leg; the hard clamp below matches its bounds)
		sDesiredPlacement.fPitch += fPitchDelta;
		sDesiredPlacement.fYaw -= fYawDelta;

		// release @0xcd930 terrain leg: with the height grids available, ease the DESIRED anchor's z
		// toward the AVERAGED terrain height under the focus XY (EstimateAverageHeight @0xcc970,
		// window half-extent 2 grid cells; Accelerate base 0.01 / eps 0.05), then lift the eye off
		// the terrain (CorrectPlacement @0xccd60). The camera focus thus tracks roofs/basements
		// instead of the fixed z=0 plane. Without a height source, the Jan03 stand-on-ground fix
		// below stands in (the release runs the same fallback while the world is not ready).
		// MOVIE MODE (release SCameraLimits.bMovie, installed by BeginSequence @0x1fd8c0): the release
		// skips this ENTIRE terrain/approach tail (@0xcd930 step 9) -- without the skip the terrain
		// easing + CorrectPlacement kept mutating the desired pose every frame and the approach ramp
		// dragged the live camera off every scripted CameraSet ("reverts at any opportunity").
		CVec3 ptDir( GetForwardDir() );
		if ( bMovieMode )
			; // release bMovie: desired placement stays exactly what the script set
		else if ( IsValid( pHeightSource ) )
		{
			float fAvg = 0;
			if ( pHeightSource->EstimateAverageHeight( sDesiredPlacement.ptAnchor.x,
			                                           sDesiredPlacement.ptAnchor.y, 2, &fAvg ) )
				sDesiredPlacement.ptAnchor.z = Accelerate( fAvg, sDesiredPlacement.ptAnchor.z, 0.01f, (float)sDelta, 0.05f );
			CorrectPlacement( sDesiredPlacement );
		}
		else if ( fabs( ptDir.z ) > 0.1f )
		{
			float fFix = sDesiredPlacement.ptAnchor.z / ptDir.z;
			sDesiredPlacement.fRod -= fFix;
			sDesiredPlacement.ptAnchor -= ptDir * fFix;
			fFix = sPlacement.ptAnchor.z / ptDir.z;
			sPlacement.fRod -= fFix;
			sPlacement.ptAnchor -= ptDir * fFix;
		}
		// forward: keyboard pan moves the DESIRED anchor (release routes it through the
		// ScrollAnchor virtual, non-immediate)
		ptDir.z = 0;
		if ( fabs2( ptDir ) > 0 )
		{
			Normalize( &ptDir );
			sDesiredPlacement.ptAnchor += ptDir * fFwd;
		}
		// strafe
		ptDir = GetStrafeDir();
		ptDir.z = 0;
		if ( fabs2( ptDir ) > 0 )
		{
			Normalize( &ptDir );
			sDesiredPlacement.ptAnchor += ptDir * fStrafe;
		}
		// zoom -> DESIRED rod: the wheel re-targets the zoom, the ease below closes the gap
		sDesiredPlacement.fRod += fZoom;

		// hard limits on the DESIRED placement (release: ApplySoftDelta hard bounds + the zone
		// clamp ClampPlacement @0xccbd0). Release skips ApplySoftDelta entirely while locked OR in
		// movie mode (bMovie applies the RAW pitch/rod deltas), so a scripted pose is NOT clamped back.
		if ( !bMovieMode && nLockCount <= 0 )
		{
			sDesiredPlacement.fRod = Clamp( sDesiredPlacement.fRod, sLimits.fMinRod, sLimits.fMaxRod );
			sDesiredPlacement.fPitch = Clamp( sDesiredPlacement.fPitch, sLimits.fMinPitch, sLimits.fMaxPitch );
		}

		if ( bMovieMode )
		{
			// release @0xcd930 movie mode: NO Approach2DesiredPlacement at all -- the live placement
			// moves only via SetPlacement (the CameraSet/CameraMove executor path), so it HOLDS.
		}
		else if ( nLockCount <= 0 )
		{
			// release @0xcd930 (free camera): ease the live placement toward the desired one. dt is
			// scaled by sLimits.fAttenuation (widened-SCameraLimits field, deferred leg) -- ctor default
			// 1.0, and retail's autoexec.cfg never overrides it; 0.03 is the free-camera epsilon.
			Approach2DesiredPlacement( &sPlacement, sDesiredPlacement, (float)sDelta, 0.03f );
		}
		else
		{
			// release @0xcd930 (locked): ramp the WORKING attenuation toward sLimits.fAttenuation*0.5
			// (= 0.5 at the frozen dev limits) by (dt/250)*target -- g_fGameCameraAccelerateTime=250 --
			// then ease with dt*working and the wider locked epsilon 0.2. This is the slow cinematic
			// glide a scripted CameraSet gets while CameraLock'ed.
			const float fTarget = 0.5f;
			if ( fTarget > fLockAttenuation )
				fLockAttenuation += ( (float)sDelta / 250.0f ) * fTarget;
			if ( fLockAttenuation > fTarget )
				fLockAttenuation = fTarget;
			Approach2DesiredPlacement( &sPlacement, sDesiredPlacement, (float)sDelta * fLockAttenuation, 0.2f );
		}
	}

	// release @0xcd930 tail: final zone clamps, desired then live (ClampPlacement @0xccbd0 x2) --
	// FREE camera only (the release skips them while locked, so a scripted off-limits pose sticks)
	if ( nLockCount <= 0 )
	{
		sDesiredPlacement.ptAnchor.x = Clamp( sDesiredPlacement.ptAnchor.x, sLimits.sZoneLimit.x1, sLimits.sZoneLimit.x2 );
		sDesiredPlacement.ptAnchor.y = Clamp( sDesiredPlacement.ptAnchor.y, sLimits.sZoneLimit.y1, sLimits.sZoneLimit.y2 );
		sPlacement.ptAnchor.x = Clamp( sPlacement.ptAnchor.x, sLimits.sZoneLimit.x1, sLimits.sZoneLimit.x2 );
		sPlacement.ptAnchor.y = Clamp( sPlacement.ptAnchor.y, sLimits.sZoneLimit.y1, sLimits.sZoneLimit.y2 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// FirstPerson camera
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFPCamera: public CBaseCamera
{
	OBJECT_BASIC_METHODS(CFPCamera);
private:
	ZDATA_(CBaseCamera)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CBaseCamera*)this); return 0; }

public:
	CFPCamera() {}

	CVec3 GetForwardDir() const;
	CVec3 GetStrafeDir() const;

	CVec3 GetCP() const;
	SHMatrix GetPos() const;

	void Update( const STime &sTime );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CFPCamera::GetForwardDir() const
{
	CVec3 ptRes;
	CQuat q = CQuat( sPlacement.fYaw, V3_AXIS_Z ) * CQuat( sPlacement.fPitch, V3_AXIS_X );
	q.GetYAxis( &ptRes );
	return ptRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CFPCamera::GetStrafeDir() const
{
	CVec3 ptRes;
	CQuat q = CQuat( sPlacement.fYaw, V3_AXIS_Z ) * CQuat( sPlacement.fPitch, V3_AXIS_X );
	q.GetXAxis( &ptRes );
	return ptRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CFPCamera::GetCP() const
{
	return sPlacement.ptAnchor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SHMatrix CFPCamera::GetPos() const
{
	SHMatrix res;
	MakeMatrix( &res, sPlacement.ptAnchor, GetForwardDir() );
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float CAMERA_SPEED = 8.0f;
const float ROTATE_SPEED = 5.0f;
void CFPCamera::Update( const STime &sTime )
{
	sPlacement.fPitch += CAMERA_SPEED * pitch.GetDelta();
	sPlacement.fYaw -= CAMERA_SPEED * rotate.GetDelta();
	float fFwd = fwd.GetDelta() * CAMERA_SPEED;
	float fStrafe = strafe.GetDelta() * CAMERA_SPEED;
	float fZoom = zoom.GetDelta() * CAMERA_SPEED;
	//
	CVec3 ptDir( GetForwardDir() );
	Normalize( &ptDir );
	sPlacement.ptAnchor += ptDir * fFwd;

	ptDir = GetStrafeDir();
	Normalize( &ptDir );
	sPlacement.ptAnchor += ptDir * fStrafe;

	sPlacement.fRod += fZoom;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Maya 
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMayaCamera: public CBaseCamera
{
	OBJECT_BASIC_METHODS(CMayaCamera);
private:
	NInput::CBind roll;
	ZDATA_(CBaseCamera)
	float fSpeed;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CBaseCamera*)this); f.Add(2,&fSpeed); return 0; }

public:
	CMayaCamera( float fSpeed = 1 );

	CVec3 GetForwardDir() const;
	CVec3 GetStrafeDir() const;

	CVec3 GetCP() const;
	SHMatrix GetPos() const;

	void ProcessEvent( const NInput::SEvent &eEvent );
	void Update( const STime &sTime );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//																						MAYA camera
////////////////////////////////////////////////////////////////////////////////////////////////////
CMayaCamera::CMayaCamera( float _fSpeed ):
	fSpeed(_fSpeed), roll( "camera_roll" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CMayaCamera::GetForwardDir() const
{
	CVec3 ptRes;
	CQuat q = CQuat( sPlacement.fYaw, V3_AXIS_Z ) * CQuat( sPlacement.fPitch, V3_AXIS_X );
	q.GetYAxis( &ptRes );
	return ptRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CMayaCamera::GetStrafeDir() const
{
	CVec3 ptRes;
	CQuat q = CQuat( sPlacement.fYaw, V3_AXIS_Z ) * CQuat( sPlacement.fPitch, V3_AXIS_X ) * CQuat( sPlacement.fRoll, V3_AXIS_Y );
	q.GetXAxis( &ptRes );
	return ptRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CMayaCamera::GetCP() const
{
	return sPlacement.ptAnchor - GetForwardDir() * sPlacement.fRod;
	return sPlacement.ptAnchor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SHMatrix CMayaCamera::GetPos() const
{
	SHMatrix res;
	MakeMatrix( &res, sPlacement.fPitch, sPlacement.fYaw, sPlacement.fRoll, GetCP() );
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMayaCamera::ProcessEvent( const NInput::SEvent &eEvent )
{
	roll.ProcessEvent( eEvent );
	CBaseCamera::ProcessEvent( eEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float PITCH_SPEED = 15;
const float YAW_SPEED = 10;
const float ROLL_SPEED = 10;
void CMayaCamera::Update( const STime &sTime )
{
	sPlacement.fPitch += fSpeed * PITCH_SPEED * pitch.GetDelta();
	sPlacement.fYaw -= fSpeed * YAW_SPEED * rotate.GetDelta();
	sPlacement.fRoll -= fSpeed * ROLL_SPEED * roll.GetDelta();
	float fUp = fSpeed * fwd.GetDelta() * 2.0f;
	float fStrafe = fSpeed * strafe.GetDelta() * 2.0f;
	float fZoom = fSpeed * zoom.GetDelta() * (fabs(sPlacement.fRod) + 1) * 7.0f;
	//
	CVec3 ptMove( fStrafe, 0, fUp );
	if ( fabs2( ptMove ) > FP_EPSILON )
	{
		ptMove *= Max( fabs( sPlacement.fRod ), 2.0f );
		SHMatrix m;
		MakeMatrix( &m, sPlacement.fPitch, sPlacement.fYaw, sPlacement.fRoll, GetCP() );
		m.RotateHVector( &ptMove, ptMove );
		CVec3 ptFw = GetForwardDir();
		sPlacement.ptAnchor = ptMove + sPlacement.fRod * ptFw;
		sPlacement.fRod = Sign( sPlacement.fRod ) * fabs(sPlacement.ptAnchor - ptMove) / fabs( ptFw );
	}
	// zoom
	sPlacement.fRod += fZoom;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CreateCamera
////////////////////////////////////////////////////////////////////////////////////////////////////
ICamera* CreateCamera( ECameraType eType, float fCameraSpeed )
{
	switch( eType )
	{
		case CAMERA_PC:
			return new CCamera;
		case CAMERA_FIRSTPERSON:
			return new CFPCamera;
		case CAMERA_MAYA:
			return new CMayaCamera( fCameraSpeed );
	}

	ASSERT(0);
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
REGISTER_SAVELOAD_CLASS( 0xF2221130, CCamera );
REGISTER_SAVELOAD_CLASS( 0xF2221131, CFPCamera );
REGISTER_SAVELOAD_CLASS( 0xA2912170, CMayaCamera );
