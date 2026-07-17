#include "StdAfx.h"
#include "Camera.h"
#include "..\Misc\Geom.h"
#include "Transform.h"
#include "..\Input\Bind.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"      // NGlobal::GetVar / RegisterCmd / CValue (BUG 4 camera sensitivity)
#include "..\FileIO\BasicChunk1.h"    // START_REGISTER / FINISH_REGISTER
#include "wInterface.h"               // NWorld::IWorld::GetAIMap (the framing raycast world handle)
#include "wHeightLayers.h"            // NWorld::IHeightLayers / SHLayer (the per-floor height fields the terrain leg samples)
#include "Interpolate.h"              // GetBilinear<float,TLinearInterpolate> -- the layer sampler (release @0xcfb40)
#include "Grid.h"                     // FP_INV_GRID_STEP -- world metres -> terrain-cell coords ([0x952958] = 1.6)
#include "GView.h"                    // NGScene::IGameView (CCamera/CFPCamera pView -- serialized CPtr member)
#include "Interface.h"                // NUI::CInterface via the UI umbrella (CFPCamera pUI -- serialized CPtr member)
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
public:
	// release CBaseCamera::SEarthQuake (PDB size 8): start time (0 = not yet stamped by
	// UpdateEarthQuakes) + peak amplitude.
	struct SEarthQuake { STime sTime; float fAmplitude; };
protected:
	NInput::CBind fwd, strafe, zoom, pitch, rotate;
	// retail CBaseCamera serialized state, PDB layout (size 220) -- full save-format parity with
	// operator& @0xd0100 (tags 2..16). The old "save format frozen" hedge is REVERSED: the widened
	// 56-byte SCameraLimits (+ the sZoneLimit relocation onto the camera) IS the retail v1.2 format.
	ZDATA
	// release +0x34: the scroll-lock COUNT (CBaseCamera::Lock @0xcffa0; script CameraLock). <= 0 = free.
	int nLockCount;
	// release +0x38: the retail tactical cut floor (GetCutFloor @0xcff90 / SetCutFloor @0xd0050).
	// The dev relocated the LIVE cut floor to the render scene (ICameraCutFloor accessor); this
	// member is the retail save slot, mirrored by CCamera's CutFloorSet writes (see note there).
	int nCutFloor;
	float fMinClipDistance;			// release +0x3C
	float fMaxClipDistance;			// release +0x40
	SCameraPos sPlacement;			// release +0x44
	SCameraLimits sLimits;			// release +0x64 (the widened 56-byte record)
	CTRect<float> sScreenRect;		// release +0x9C
	CVec3 vEarthQuakeDelta;			// release +0xAC (.z written by UpdateEarthQuakes)
	list<SEarthQuake> earthQuakesList;	// release +0xB8
	// release +0xBC/+0xC0: the [min,max] cut-floor clamp range (SetCutFloorRange @0xcba10). No dev
	// writer (the dev floor clamp lives with the scene owner) -- retail ctor defaults, serialized.
	int nMinCutFloor;
	int nMaxCutFloor;
	// release +0xC4: the scroll-zone clamp rect -- MOVED OUT of SCameraLimits by the retail widening
	// (SetZoneLimits @0xcfff0; the mission stamps it from the world's map safe zone).
	CTRect<float> sZoneLimit;
	// release +0xD4 (FreezeCamera @0xcffc0 = script CameraLock, vtbl+0x74): refcount; while >= 1 the
	// Update movement tail bails and retail SetCutFloor @0xd0050 no-ops.
	int nLockNoUpdate;
	// release +0xD8/+0xD9: the "user wants to unlock" intent pair -- latched by ProcessEvent
	// (@0xcbd00) / the locked CCamera::ScrollAnchor (@0xcd720), cleared by FreezeCamera(true).
	bool bMustUnlockDueToUserDecision;
	bool bUserJustWantedToUnlock;
public:
	// retail CBaseCamera::operator& @0xd0100 -- the full table, retail tag order.
	ZEND int operator&( CStructureSaver &f )
	{
		f.Add(2,&nLockCount);
		f.Add(3,&nCutFloor);
		f.Add(4,&fMinClipDistance);
		f.Add(5,&fMaxClipDistance);
		f.Add(6,&sPlacement);
		f.Add(7,&sLimits);
		f.Add(8,&sScreenRect);
		f.Add(9,&vEarthQuakeDelta);
		f.Add(10,&earthQuakesList);
		f.Add(11,&nMinCutFloor);
		f.Add(12,&nMaxCutFloor);
		f.Add(13,&sZoneLimit);
		f.Add(14,&nLockNoUpdate);
		f.Add(15,&bMustUnlockDueToUserDecision);
		f.Add(16,&bUserJustWantedToUnlock);
		return 0;
	}

public:
	CBaseCamera();

	// retail @0xce5d0: admit only a strictly-positive amplitude (NaN/0/negative dropped); sTime 0 --
	// UpdateEarthQuakes stamps the real start on first touch. The point is unused by the base impl.
	void AddEarthQuake( const CVec3 &pt, float fAmplitude )
	{
		if ( fAmplitude > 0 )
		{
			SEarthQuake quake;
			quake.sTime = 0;
			quake.fAmplitude = fAmplitude;
			earthQuakesList.push_back( quake );
		}
	}
	// retail CBaseCamera::Update @0xccc80 (the quake integrator; CCamera::Update calls it FIRST --
	// disasm @0x4cd93e): per quake, stamp the start time, then while younger than 1501 ms accumulate
	// amplitude * cos(now * 2pi/300) * e^(-0.006 * age_ms); expired quakes are erased. The sum lands
	// in vEarthQuakeDelta.z (an empty list leaves the last delta in place -- faithful).
	void UpdateEarthQuakes( const STime &sTime );

	// release @0xcffa0: faithful unclamped ++/-- (unpaired unlocks go negative, as in the release).
	void SetLock( bool bLock ) { if ( bLock ) ++nLockCount; else --nLockCount; }

	// see ICamera::FreezeCamera (Camera.h) -- release @0xcffc0: freezing bumps the refcount and
	// clears the unlock-intent pair; unfreezing decrements. Either way the body TAIL-CALLS
	// vtbl+0x70 = Lock(bFreeze) (disasm @0x4cffec forwards the same bool), so the scroll-lock
	// count moves in step -- ported as the virtual SetLock call.
	void FreezeCamera( bool bFreeze )
	{
		if ( bFreeze )
		{
			++nLockNoUpdate;
			bMustUnlockDueToUserDecision = false;
			bUserJustWantedToUnlock = false;
		}
		else
			--nLockNoUpdate;
		SetLock( bFreeze );
	}
	bool IsCameraFrozen() const { return nLockNoUpdate > 0; }

	// retail CBaseCamera::UserWantedItToUnlock @0xd0020 (ICamera vtbl+0x94): one-shot read of the
	// unlock-intent pair (see Camera.h). bUserJustWantedToUnlock is CLEARED on every read; the
	// return is bMustUnlockDueToUserDecision gated on NOT script-frozen (nLockNoUpdate < 1).
	bool UserWantedItToUnlock( bool *pbJustWanted )
	{
		*pbJustWanted = bUserJustWantedToUnlock;
		bUserJustWantedToUnlock = false;
		return bMustUnlockDueToUserDecision && nLockNoUpdate < 1;
	}

	// retail CBaseCamera::SetCutFloorRange @0xcba10: store the clamp range, then re-clamp the
	// current floor through the SetCutFloor(GetCutFloor()) virtual pair (exactly the retail body).
	void SetCutFloorRange( int nMin, int nMax )
	{
		nMinCutFloor = nMin;
		nMaxCutFloor = nMax;
		SetCutFloor( GetCutFloor() );
	}
	// retail CBaseCamera::GetCutFloorRange @0xcba40
	void GetCutFloorRange( int *pnMin, int *pnMax ) const { *pnMin = nMinCutFloor; *pnMax = nMaxCutFloor; }
	// retail CBaseCamera::GetCutFloor @0xcff90
	int GetCutFloor() const { return nCutFloor; }
	// retail CBaseCamera::SetCutFloor @0xd0050: hard NO-OP while script-frozen (nLockNoUpdate >= 1
	// -- the base's one-floor lock), else clamp to [nMinCutFloor, nMaxCutFloor] and store.
	void SetCutFloor( int nFloor )
	{
		if ( nLockNoUpdate < 1 )
		{
			if ( nFloor > nMaxCutFloor )
				nFloor = nMaxCutFloor;
			if ( nFloor < nMinCutFloor )
				nFloor = nMinCutFloor;
			nCutFloor = nFloor;
		}
	}

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

	// release @0xcfff0: plain 4-float copy of the scroll-zone rect (+0xC4).
	void SetZoneLimits( const CTRect<float> &sRect ) { sZoneLimit = sRect; }

	void ProcessEvent( const NInput::SEvent &eEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CBaseCamera::CBaseCamera():
	sScreenRect( 0, 0, 1, 1 ),
	sZoneLimit( -1000, -1000, 1000, 1000 ),		// release ctor @0xcd380 (the rect that used to default inside SCameraLimits)
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
	nCutFloor = 4;				// release ctor @0xcd380 defaults for the cut-floor trio
	nMinCutFloor = -3;
	nMaxCutFloor = 4;
	nLockNoUpdate = 0;
	bMustUnlockDueToUserDecision = false;
	bUserJustWantedToUnlock = false;
	vEarthQuakeDelta = VNULL3;
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
	// release @0xcbd00: all five binds always run (ProcessEvent has side effects); if ANY of them
	// consumed the event and the camera has not already been flagged, latch the "user wants to
	// unlock" intent once (FreezeCamera(true) clears it).
	bool bFwd = fwd.ProcessEvent( eEvent );
	bool bStrafe = strafe.ProcessEvent( eEvent );
	bool bZoom = zoom.ProcessEvent( eEvent );
	bool bPitch = pitch.ProcessEvent( eEvent );
	bool bRotate = rotate.ProcessEvent( eEvent );
	if ( ( bRotate || bFwd || bStrafe || bZoom || bPitch ) && !bMustUnlockDueToUserDecision )
	{
		bUserJustWantedToUnlock = true;
		bMustUnlockDueToUserDecision = true;
	}
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
// release GetHeight @0xccc10: bilinear height over one height layer at GRID coords. The bounds test is
// on the ROUNDED cell (Float2Int = fld/fistp @0x4ccc1a, round-to-nearest -- a C cast would truncate),
// while the sample itself is bilinear at the raw grid position.
// Layer floats are already METRES: no FP_TERRAIN_H_SCALE here (that decode belongs to the raw heightmap,
// and applying it to a layer would scale metres a second time).
static bool GetHeight( const CVec3 &vGrid, const NWorld::SHLayer *pLayer, float *pfHeight )
{
	const int nX = Float2Int( vGrid.x );
	const int nY = Float2Int( vGrid.y );
	if ( nX < 0 || nY < 0 )                                                        // @0x4ccc35 / @0x4ccc3d
		return false;
	if ( nX >= pLayer->heights.GetXSize() || nY >= pLayer->heights.GetYSize() )    // @0x4ccc41 / @0x4ccc46
		return false;
	*pfHeight = GetBilinear( pLayer->heights, vGrid.x, vGrid.y, TLinearInterpolate() );   // @0x4ccc56
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release EstimateAverageHeight @0xcc970: mean AND max height over the cell window of half-extent
// (nHalfX,nHalfY) around the rounded grid cell, clipped to the layer; false when the window is empty.
// Decoded from raw disasm -- Ghidra aliases the sum and the count into one local and is unusable. The
// three accumulators are distinct stack slots, all seeded 0 @0x4cc9f1: [esp+0x18] fSum, [esp+0x10]
// nCount, [esp+0x1c] fMax. fMax is a RUNNING max seeded 0 (@0x4cca38 Max(fMax,cell)) -- NOT a per-cell
// clamp: the sum adds the RAW cell (@0x4cca4a).
static bool EstimateAverageHeight( float *pfAvg, float *pfMax, const NWorld::SHLayer *pLayer,
                                   const CVec3 &vGrid, int nHalfX, int nHalfY )
{
	const int nX = Float2Int( vGrid.x ), nY = Float2Int( vGrid.y );
	const int x1 = Max( nX - nHalfX, 0 );                                          // @0x4cc9a1
	const int x2 = Min( nX + nHalfX, pLayer->heights.GetXSize() );                 // @0x4cc9b2
	const int y1 = Max( nY - nHalfY, 0 );                                          // @0x4cc9cd
	const int y2 = Min( nY + nHalfY, pLayer->heights.GetYSize() );                 // @0x4cc9e1
	float fSum = 0, fMax = 0;
	int nCount = 0;
	for ( int x = x1; x < x2; ++x )
		for ( int y = y1; y < y2; ++y )
		{
			const float fCell = pLayer->heights[y][x];                             // @0x4cca2a: pData[y][x]
			fMax = Max( fMax, fCell );
			fSum += fCell;
			++nCount;
		}
	if ( nCount == 0 )                                                             // @0x4cca67
		return false;
	*pfMax = fMax;                                                                 // @0x4cca91
	*pfAvg = fSum / nCount;                                                        // @0x4cca8a
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCamera: public CBaseCamera
{
	OBJECT_BASIC_METHODS(CCamera);
private:
	// release CCamera slow-motion block @+0x180 (0xc bytes) and "FOV effect" transition block
	// @+0x18C (0x38 bytes, PDB layout -- the FOV-spring members tOn/tOnFOVSpring/tMaxLen/
	// sDesiredPlacement drive Update's separate effect, still unported; carried + serialized so the
	// save chunk is the retail 0x38). Ctor defaults = the release ctor stores (nSloMo 1, FOV 35).
	struct SCameraSloMo     { int nSloMo; int tOn; STime tMaxLen; SCameraSloMo(): nSloMo(1), tOn(0), tMaxLen(0) {} };
	struct SCameraFOVEffect
	{
		int nSloMo;						// +0x00 (0x18C)
		int tOn;						// +0x04
		int tOnFOVSpring;				// +0x08
		STime tMaxLen;					// +0x0C
		float fFOV;						// +0x10 (0x19C)
		SCameraPos sDesiredPlacement;	// +0x14 (0x1A0)
		float fRoll;					// +0x34 (0x1C0)
		SCameraFOVEffect(): nSloMo(1), tOn(0), tOnFOVSpring(0), tMaxLen(0), fFOV(35.0f), fRoll(0) {}
	};
	// retail CCamera serialized state -- full parity with operator& @0xd0c30 (tags 2..13).
	ZDATA_(CBaseCamera)
	CPtr<NWorld::IWorld>     pWorld;					// tag 2, release +0x0F4 (the raycast + terrain world handle)
	CPtr<NGScene::IGameView> pView;						// tag 3, release +0x0F8 (the view height-grid handle; consumer = the deferred 2nd CorrectPlacement pass)
	SCameraPos               sDesiredPlacement;			// tag 4, release +0x0FC (all user input drives it; Update eases the live pose toward it)
	STime                    sLastTime;					// tag 5, release +0x11C (0xffffffff = "no frame yet" sentinel, ctor @0xce8e0)
	SCameraLimits            sSoftLimits;				// tag 6, release +0x120 (Get/SetSoftLimits @0xd04a0/@0xd04c0; ApplySoftDelta consumer deferred)
	float                    fFloorSliderValue;			// tag 7 (the retail floor-slider input accumulator; no dev floor-slider bind -- inert, serialized)
	float                    fAttenuation;				// tag 8, release +0x15C: the WORKING attenuation -- while script-LOCKED, Update eases with dt * this, ramping toward sLimits.fAttenuation*0.5; reset by the SlowCameraAcceleration settle (@0xd03c0)
	SCameraPos               sPlacementToAccelerateTo;	// tag 9, release +0x160 (SlowCameraAcceleration compares/copies)
	SCameraSloMo             sloMo;						// tag 10, release +0x180
	SCameraFOVEffect         fov;						// tag 11, release +0x18C
	CPtr<CObjectBase>        pFollowUnit;				// tag 12, release +0x1C4
	STime                    sMaxFollowUnitTime;		// tag 13, release +0x1C8 (= sLastTime + 10000)
public:
	// retail CCamera::operator& @0xd0c30 -- the full table, retail tag order.
	ZEND int operator&( CStructureSaver &f )
	{
		f.Add(1,(CBaseCamera*)this);
		f.Add(2,&pWorld);
		f.Add(3,&pView);
		f.Add(4,&sDesiredPlacement);
		f.Add(5,&sLastTime);
		f.Add(6,&sSoftLimits);
		f.Add(7,&fFloorSliderValue);
		f.Add(8,&fAttenuation);
		f.Add(9,&sPlacementToAccelerateTo);
		f.Add(10,&sloMo);
		f.Add(11,&fov);
		f.Add(12,&pFollowUnit);
		f.Add(13,&sMaxFollowUnitTime);
		return 0;
	}
private:
	// the render cut-floor accessor. Runtime-only (re-installed on every mission init / snapshot
	// restore). The dev terrain-height stand-in that used to sit beside it is gone: the terrain legs
	// now sample pWorld->GetHeightLayers() the way retail does.
	CPtr<ICameraCutFloor>  pCutFloorSource;             // the render cut-floor accessor (see ICameraCutFloor)
	//
	int  CutFloorGet() const { return IsValid( pCutFloorSource ) ? pCutFloorSource->GetCutFloor() : 0; }
	// the LIVE owner stays the render scene (dev relocation); nCutFloor is the retail save slot
	// (CBaseCamera tag 3), mirrored here so a save carries the floor the camera framing chose.
	void CutFloorSet( int nF ) { nCutFloor = nF; if ( IsValid( pCutFloorSource ) ) pCutFloorSource->SetCutFloor( nF ); }
	bool CanSeeOneRay( const CVec3 &target ) const;                             // release @0xceb20
	bool CanSeeNow( const CVec3 &target );                                      // release @0xcecd0
	int  TestCurrentDesiredPosition( const CVec3 &p1, const CVec3 &p2 );        // release @0xcee10
	int  TryShowPlaces( const CVec3 &ptA, const CVec3 &ptB, int nFloor, float fRod, float fYaw );  // @0xcee90
	int  ShowTwoPlaces( const CVec3 &ptA, const CVec3 &ptB, int nFloor, float fRodIn );            // @0xceff0
	void SlowCameraAcceleration();                                              // release @0xd03c0

	// release @0xccd60 -- __thiscall (SCameraPos*, SHLayer*), ret 8: the eye lift-off against ONE layer.
	void CorrectPlacement( SCameraPos &pos, const NWorld::SHLayer *pLayer ) const;

public:
	CCamera();

	CVec3 GetForwardDir() const;
	CVec3 GetStrafeDir() const;

	CVec3 GetCP() const;
	SHMatrix GetPos() const;

	void SetPlacement( const SCameraPos &sPlacement );
	void ScrollAnchor( const CVec3 &vDelta, bool bImmediate, bool bOnTerrain );

	// release CCamera::GetSoftLimits @0xd04a0 / SetSoftLimits @0xd04c0: plain copies of the
	// soft-limits record at +0x120. (The retail vtbl has NO CCamera Lock override -- the old dev
	// unlock-time fAttenuation reset was a stand-in for the then-unported SlowCameraAcceleration
	// settle @0xd03c0, which now does the retail reset.)
	void GetSoftLimits( SCameraLimits *pLimits ) const { ASSERT( pLimits ); *pLimits = sSoftLimits; }
	void SetSoftLimits( const SCameraLimits &_sLimits ) { sSoftLimits = _sLimits; }

	float GetAccelerationFactor( float fAcceleration ) const;   // release @0xcbe70

	void Update( const STime &sTime );
	virtual void SetWorld( NWorld::IWorld *_pWorld ) { pWorld = _pWorld; }
	virtual void SetView( NGScene::IGameView *_pView ) { pView = _pView; }
	virtual void SetCutFloorSource( ICameraCutFloor *pSource ) { pCutFloorSource = pSource; }
	virtual void ShowPlacesFromBestPoint( const CVec3 &ptA, const CVec3 &ptB, int nFloor, float fRodIn,
		int nSloMoRatio, float fDivisor, bool bKeepFollow, bool bForceRod );   // release @0xcf1c0
	virtual void FollowUnit( CObjectBase *pUnit );                             // release @0xd0520
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CCamera::CCamera():
	sLastTime( 0xFFFFFFFF ), fFloorSliderValue( 0 ), fAttenuation( 0 ), sMaxFollowUnitTime( 0 )
{
	// release ctor @0xce6d0/@0xce8e0: the desired placement starts equal to the live default pose (the
	// sloMo/fov/pFollowUnit framing state defaults via their own ctors -- nSloMo=1, fFOV=35, follow=null;
	// sSoftLimits defaults to the SCameraLimits ctor set, matching the release literal stores).
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
	NWorld::IHeightLayers *pLayers = IsValid( pWorld ) ? pWorld->GetHeightLayers() : 0;
	if ( pLayers )
	{
		float fAvg = 0, fMax = 0;
		// the framing samples the anchor directly (no Accelerate easing -- this is a candidate pose,
		// not a frame step), window half-extent 2 cells like the Update leg.
		if ( EstimateAverageHeight( &fAvg, &fMax, pLayers->GetTerrainLayer(),
		                            sDesiredPlacement.ptAnchor * FP_INV_GRID_STEP, 2, 2 ) )
			sDesiredPlacement.ptAnchor.z = fAvg;
		CorrectPlacement( sDesiredPlacement, pLayers->GetTerrainLayer() );
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
	if ( fAttenuation < 0.5f * sLimits.fAttenuation )   // release: 0.5*sLimits.fAttenuation <= working fAttenuation
		return;
	if ( sDesiredPlacement.ptAnchor.x != sPlacementToAccelerateTo.ptAnchor.x ) return;
	if ( sDesiredPlacement.ptAnchor.y != sPlacementToAccelerateTo.ptAnchor.y ) return;
	if ( sDesiredPlacement.ptAnchor.z != sPlacementToAccelerateTo.ptAnchor.z ) return;
	if ( sDesiredPlacement.fYaw       != sPlacementToAccelerateTo.fYaw )       return;
	fAttenuation = 0;
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
	sMaxFollowUnitTime = sLastTime + 10000;
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
	// retail GetCP adds the earthquake shake delta (only .z is ever nonzero)
	return sPlacement.ptAnchor - GetForwardDir() * sPlacement.fRod + vEarthQuakeDelta;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SHMatrix CCamera::GetPos() const
{
	SHMatrix res;
	MakeMatrix( &res, sPlacement.fPitch, sPlacement.fYaw, sPlacement.fRoll, GetCP() );
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release CCamera::CorrectPlacement @0xccd60: push the placement back / tilt it up so the camera EYE
// never sinks below fMinHeight above the sampled layer. The anchor and yaw stay untouched; rod + pitch
// are recomputed so the eye lifts to layer+minHeight at the same XY. The layer is a PARAMETER (ret 8):
// Update calls it twice, once against the terrain layer and once against the rendered floor's.
void CCamera::CorrectPlacement( SCameraPos &pos, const NWorld::SHLayer *pLayer ) const
{
	if ( !pLayer )
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

	// release @0xccd60: the EYE goes to grid coords before the sample (fmul [0x952958] = FP_INV_GRID_STEP).
	float fTerrain = 0;
	if ( !GetHeight( CVec3( fEyeX, fEyeY, 0 ) * FP_INV_GRID_STEP, pLayer, &fTerrain ) )
		return;                                        // off-grid: leave the placement alone
	if ( !( fTerrain > fEyeZ - sLimits.fMinHeight ) )  // release reads sLimits.fMinHeight ([edi+0x7c], ctor default 4.0)
		return;                                        // eye already clears the layer

	const float fNewEyeZ = fTerrain + sLimits.fMinHeight;
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
// retail CBaseCamera::Update @0xccc80 -- the grenade-blast screen-shake integrator. Per quake:
// stamp the start time on first touch; younger than 1501 ms -> accumulate
// amplitude * cos(now * 2pi/300) * e^(-0.006 * age_ms) (a 300 ms oscillation under an exponential
// decay); else erase. The sum lands in vEarthQuakeDelta.z; an empty list leaves the delta as-is.
void CBaseCamera::UpdateEarthQuakes( const STime &sTime )
{
	if ( earthQuakesList.empty() )
		return;
	float fSum = 0;
	for ( list<SEarthQuake>::iterator it = earthQuakesList.begin(); it != earthQuakesList.end(); )
	{
		if ( it->sTime == 0 )
			it->sTime = sTime;
		const STime sAge = sTime - it->sTime;
		if ( sAge < 1501 )
		{
			fSum += it->fAmplitude * (float)( cos( (double)(int)sTime * 0.02094395086169243 /*2pi/300*/ )
				* exp( -0.006 * (double)(int)sAge ) );
			++it;
		}
		else
			it = earthQuakesList.erase( it );
	}
	vEarthQuakeDelta.z = fSum;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release ApplySoftDelta @0xcb930: the camera's SOFT-limit spring, applied to the desired pitch and
// rod. Inside [soft, soft] the delta passes through untouched. Push past a soft bound and the delta
// is scaled by how far the value still is from the HARD bound, so it decays to zero as it approaches
// it -- you can overshoot the soft limit, but ever more slowly. Release the input (delta == 0) and
// the value eases back to the nearest soft bound. That easing is the rubber-band; a plain Clamp to
// the hard bounds has neither half.
static void ApplySoftDelta( float *pVal, float fSoftMin, float fSoftMax, float fHardMin, float fHardMax,
                            float fDelta, float fDT )
{
	if ( *pVal < fSoftMin && fDelta < 0 )								// @0x4cb930
		fDelta = ( ( *pVal - fHardMin ) / ( fSoftMin - fHardMin ) ) * fDelta;
	if ( *pVal > fSoftMax && fDelta > 0 )								// @0x4cb960
		fDelta = ( ( *pVal - fHardMax ) / ( fSoftMax - fHardMax ) ) * fDelta;

	*pVal += fDelta;													// @0x4cb98c
	if ( *pVal < fHardMin )
		*pVal = fHardMin;
	if ( *pVal > fHardMax )
		*pVal = fHardMax;

	// @0x4cb9be: the spring-back runs ONLY on zero input -- while the user is still driving, the
	// attenuated delta above is the whole behaviour.
	if ( fDelta == 0 )
	{
		float fTarget;
		if ( *pVal < fSoftMin )
			fTarget = fSoftMin;
		else if ( *pVal > fSoftMax )
			fTarget = fSoftMax;
		else
			return;
		*pVal = Accelerate( fTarget, *pVal, 0.1f, fDT, 0.02f );			// @0x4cb9f1
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release CCamera::GetAccelerationFactor @0xcbe70: the zoom-out input ramp shared by the scroll and
// yaw scales -- 1.0 at or below the SOFT min rod, rising linearly to 1+fAcceleration at the soft max.
// The rod bounds are clamped to [0,1000] and the LIVE rod drives it, not the desired one.
float CCamera::GetAccelerationFactor( float fAcceleration ) const
{
	const float fLo = Max( 0.0f, sSoftLimits.fMinRod );
	const float fHi = Min( 1000.0f, sSoftLimits.fMaxRod );
	if ( sPlacement.fRod > fLo )
		return 1.0f + ( ( sPlacement.fRod - fLo ) * fAcceleration ) / ( fHi - fLo );
	return 1.0f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCamera::ScrollAnchor( const CVec3 &vDelta, bool bImmediate, bool bOnTerrain )
{
	// release @0xcd720: an immediate move shifts BOTH the live and desired anchors 1:1; a smoothed
	// move shifts only the DESIRED anchor so Update eases the camera into it. While script-LOCKED
	// (nLockCount >= 1) the release ignores the scroll and only latches the "user wants to unlock"
	// intent on a non-trivial request (|delta|^2 > 0.001) -- serialized at CBaseCamera tags 15/16.
	// Still-omitted release bit: the SetOnTerrain re-clamp (needs the world height grids).
	if ( nLockCount > 0 )
	{
		if ( fabs2( vDelta ) > 0.001 && !bMustUnlockDueToUserDecision )
		{
			bUserJustWantedToUnlock = true;
			bMustUnlockDueToUserDecision = true;
		}
		return;
	}
	if ( bImmediate )
	{
		sPlacement.ptAnchor += vDelta;
		sDesiredPlacement.ptAnchor += vDelta;
	}
	else
	{
		// release @0xcd720: the smoothed scroll rides the zoom-out ramp (@0xcbe70) * fScrollSpeed --
		// a zoomed-out camera scrolls proportionally faster. The immediate branch is NOT scaled.
		sDesiredPlacement.ptAnchor += vDelta
		    * ( GetAccelerationFactor( sLimits.fScrollZoomAcceleration ) * sLimits.fScrollSpeed );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCamera::Update( const STime &sTime )
{
	// release CCamera::Update @0xcd930 OPENS with the base quake integrator (call 0x4ccc80 disasm
	// @0x4cd93e) -- the earthquake delta advances on every tactical-camera frame.
	UpdateEarthQuakes( sTime );
	// release @0xcd930: user input mutates the DESIRED placement; the tail then eases the live
	// placement toward it (Approach2DesiredPlacement @0xcb710). The release terrain legs (averaged-
	// height anchor easing + CorrectPlacement) need the world/view height grids the camera cannot
	// reach here -- the Jan03 stand-on-ground fix below keeps standing in for them.
	float fPitchDelta = pitch.GetDelta();
	float fYawDelta = rotate.GetDelta();
	float fFwd = fwd.GetDelta() * 10.0f;
	float fStrafe = strafe.GetDelta() * 10.0f;
	// release @0xcd930: the zoom bind is read RAW here (no input scale) -- the rod step is scaled
	// below, once, from the LIVE rod. |delta| > 100 is treated as a device glitch and dropped.
	float fZoom = zoom.GetDelta();
	if ( fabs( fZoom ) > 100.0f )
	{
		DebugTrace( "ZOOM value = %e\n", fZoom );
		fZoom = 0;
	}

	// release @0xcd930: frame dt clamped to [0,100] ms (0xffffffff sentinel -> first tick applies nothing)
	STime sDelta = 0;
	if ( sLastTime != 0xFFFFFFFF )
		sDelta = Min( sTime - sLastTime, (STime)100 );
	sLastTime = sTime;

	if ( sDelta > 0 )
	{
		// release FreezeCamera @0xcffc0 (nLockNoUpdate, the SCRIPT CameraLock -- vtbl+0x74): while
		// frozen the ENTIRE movement/approach tail is skipped -- no input, no terrain easing, no
		// Approach2DesiredPlacement. SetPlacement stays ungated, so scripted CameraSet still lands
		// and HOLDS exactly (the HQ per-room static cameras).
		if ( nLockNoUpdate > 0 )
		{
			sLastTime = sTime;
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
		if ( fabs( fYawDelta ) > fabs( fPitchDelta ) * 10.0f )
			fPitchDelta = 0;
		if ( fabs( fPitchDelta ) > fabs( fYawDelta ) * 10.0f )
			fYawDelta = 0;
		// release @0xcd930: turn input scale -- yaw rides the zoom-out ramp (@0xcbe70), pitch does not.
		fYawDelta = GetAccelerationFactor( sLimits.fYawZoomAcceleration ) * sLimits.fYawSpeed * fYawDelta;
		fPitchDelta = fPitchDelta * sLimits.fPitchSpeed;
		}

		// pitch/yaw mutate the DESIRED placement. release @0xcd930: in movie mode the raw delta is
		// applied (@0x4cdb60); while locked pitch is left alone entirely (@0x4cdb1c); otherwise it
		// goes through the soft-limit spring (@0x4cdb55). Yaw is unbounded -- no limits either way.
		if ( sLimits.bMovie )
			sDesiredPlacement.fPitch += fPitchDelta;
		else if ( nLockCount <= 0 )
			ApplySoftDelta( &sDesiredPlacement.fPitch, sSoftLimits.fMinPitch, sSoftLimits.fMaxPitch,
			                sLimits.fMinPitch, sLimits.fMaxPitch, fPitchDelta, (float)sDelta );
		sDesiredPlacement.fYaw -= fYawDelta;

		// release @0xcd930: the rod step -- clamp the raw bind to +/-5, then scale by fZoomSpeed and
		// an acceleration curve on the LIVE rod, so a zoomed-out camera zooms proportionally faster.
		// Read sPlacement.fRod HERE: the terrain fallback below mutates it.
		fZoom = Max( -5.0f, Min( fZoom, 5.0f ) ) * sLimits.fZoomSpeed
		      * ( fabs( sPlacement.fRod ) * sLimits.fZoomAcceleration + 1.0f );

		// forward: the keyboard pan goes through the ScrollAnchor virtual (@0x4cdc32, cam vtbl+0x54 --
		// slot-walked), NON-immediate, so it picks up the scroll input scale. Writing ptAnchor direct
		// bypassed that scale entirely and made panning ~3x too slow at range.
		CVec3 ptDir( GetForwardDir() );
		ptDir.z = 0;
		if ( fabs2( ptDir ) > 0 )
		{
			Normalize( &ptDir );
			ScrollAnchor( ptDir * fFwd, false, false );
		}
		// strafe (@0x4cdcb8)
		ptDir = GetStrafeDir();
		ptDir.z = 0;
		if ( fabs2( ptDir ) > 0 )
		{
			Normalize( &ptDir );
			ScrollAnchor( ptDir * fStrafe, false, false );
		}
		// zoom -> DESIRED rod. release @0xcd930: movie mode or locked applies the raw step
		// (@0x4cdd05); otherwise the rod rides the soft-limit spring (@0x4cdcfe) -- push past the
		// soft bound and the step decays toward the hard bound, release and it eases back.
		if ( sLimits.bMovie || nLockCount > 0 )
			sDesiredPlacement.fRod += fZoom;
		else
			ApplySoftDelta( &sDesiredPlacement.fRod, sSoftLimits.fMinRod, sSoftLimits.fMaxRod,
			                sLimits.fMinRod, sLimits.fMaxRod, fZoom, (float)sDelta );

		// release @0xcd930 terrain+approach leg (@0x4cdd15..0x4ce0aa) -- ONE three-way gate on the
		// height layers, with the approach living INSIDE it. It runs AFTER the scroll/strafe/zoom
		// above (retail order: @0x4cdc32 pan, @0x4cdcb8 strafe, @0x4cdcf8 zoom, THEN @0x4cdd15), so
		// CorrectPlacement sees this frame's rod.
		NWorld::IHeightLayers *pLayers = IsValid( pWorld ) ? pWorld->GetHeightLayers() : 0;   // world vtbl+0x90 @0x376f00
		if ( sLimits.bMovie )
		{
			// DELIBERATE DIVERGENCE, unchanged by this leg: retail SNAPS here (sPlacement =
			// sDesiredPlacement, rep movsd @0x4ce091). Dev holds the live placement instead, so a
			// scripted CameraSet sticks -- which is what the visually-verified menu-camera fix
			// (cd15bc6) rests on. Porting the snap is its own leg with its own play-test.
		}
		else if ( !pLayers )
		{
			// @0x4cdffe: no layers (world not ready) -- snap, then stand the LIVE placement on the
			// ground plane. Retail snaps FIRST (@0x4ce03d) and fixes sPlacement ONLY.
			CVec3 ptFwd( GetForwardDir() );                                    // @0x4cdffe (cam vtbl+0x10)
			sPlacement = sDesiredPlacement;                                    // @0x4ce03d
			if ( fabs( ptFwd.z ) > 0.1f )
			{
				const float fFix = sPlacement.ptAnchor.z / ptFwd.z;
				sPlacement.fRod -= fFix;
				sPlacement.ptAnchor -= ptFwd * fFix;
			}
		}
		else
		{
			// @0x4cdd58: the grid pos comes from the LIVE anchor ([ebx+0x58]) while Accelerate writes
			// the DESIRED anchor's z ([ebx+0x118]). That asymmetry is retail's -- reproduced as-is.
			const CVec3 vGrid = sPlacement.ptAnchor * FP_INV_GRID_STEP;
			float fAvg = 0, fMax = 0;
			if ( EstimateAverageHeight( &fAvg, &fMax, pLayers->GetTerrainLayer(), vGrid, 2, 2 ) )   // @0x4cddad
				sDesiredPlacement.ptAnchor.z = Accelerate( fAvg, sDesiredPlacement.ptAnchor.z, 0.01f, (float)sDelta, 0.05f );   // @0x4cdde2

			// PASS 1 (@0x4cde03): the STORED desired placement, terrain layer only.
			CorrectPlacement( sDesiredPlacement, pLayers->GetTerrainLayer() );
			// PASS 2 (@0x4cde2f): a STACK COPY against the RENDERED floor's layer -- and it is that
			// copy, not sDesiredPlacement, that BOTH approach paths chase (edi = [B+0x48] in both).
			SCameraPos sCopy = sDesiredPlacement;                              // @0x4cde11
			CorrectPlacement( sCopy, pLayers->GetLayer( pView->GetCutFloor() ) );   // view vtbl+0x74 @0x586220

			if ( nLockCount <= 0 )
				Approach2DesiredPlacement( &sPlacement, sCopy, (float)sDelta * sLimits.fAttenuation, 0.03f );   // @0x4cde60
			else
			{
				// @0x4cdef5 (locked): ramp the WORKING attenuation (+0x15C) toward fAttenuation*0.5 by
				// (dt/250)*target -- g_fGameCameraAccelerateTime = 250 -- then ease with the wider 0.2
				// epsilon. The slow cinematic glide a scripted CameraSet gets while CameraLock'ed.
				const float fTarget = sLimits.fAttenuation * 0.5f;
				if ( fTarget > fAttenuation )
					fAttenuation += ( (float)sDelta / 250.0f ) * fTarget;
				if ( fAttenuation > fTarget )
					fAttenuation = fTarget;
				Approach2DesiredPlacement( &sPlacement, sCopy, (float)sDelta * fAttenuation, 0.2f );
			}
		}
	}

	// release @0xcd930 tail: final zone clamps, desired then live (ClampPlacement @0xccbd0 x2, over
	// the camera's own sZoneLimit -- the rect the retail widening moved OUT of SCameraLimits) --
	// FREE camera only (the release skips them while locked, so a scripted off-limits pose sticks)
	if ( nLockCount <= 0 )
	{
		sDesiredPlacement.ptAnchor.x = Clamp( sDesiredPlacement.ptAnchor.x, sZoneLimit.x1, sZoneLimit.x2 );
		sDesiredPlacement.ptAnchor.y = Clamp( sDesiredPlacement.ptAnchor.y, sZoneLimit.y1, sZoneLimit.y2 );
		sPlacement.ptAnchor.x = Clamp( sPlacement.ptAnchor.x, sZoneLimit.x1, sZoneLimit.x2 );
		sPlacement.ptAnchor.y = Clamp( sPlacement.ptAnchor.y, sZoneLimit.y1, sZoneLimit.y2 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// FirstPerson camera
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFPCamera: public CBaseCamera
{
	OBJECT_BASIC_METHODS(CFPCamera);
private:
	// retail CFPCamera serialized state -- parity with operator& @0xd0dd0 (tags 2..4). Retail seeds
	// these in the gameplay ctor from the CreateCamera factory args (@0xcf7d0); the dev factory
	// keeps its 2-arg signature, so the mission installs them post-creation (SetWorld/SetUI/SetView).
	ZDATA_(CBaseCamera)
	CPtr<NWorld::IWorld>     pWorld;	// tag 2
	CPtr<NUI::CInterface>    pUI;		// tag 3
	CPtr<NGScene::IGameView> pView;		// tag 4
public:
	// retail CFPCamera::operator& @0xd0dd0.
	ZEND int operator&( CStructureSaver &f )
	{
		f.Add(1,(CBaseCamera*)this);
		f.Add(2,&pWorld);
		f.Add(3,&pUI);
		f.Add(4,&pView);
		return 0;
	}

public:
	CFPCamera() {}

	CVec3 GetForwardDir() const;
	CVec3 GetStrafeDir() const;

	CVec3 GetCP() const;
	SHMatrix GetPos() const;

	virtual void SetWorld( NWorld::IWorld *_pWorld ) { pWorld = _pWorld; }
	virtual void SetUI( NUI::CInterface *_pUI ) { pUI = _pUI; }
	virtual void SetView( NGScene::IGameView *_pView ) { pView = _pView; }

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
public:
	// retail CMayaCamera::operator& @0xd0bc0 (base@1 + fSpeed@2 -- already retail-matching).
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
