#include "StdAfx.h"
#include "wInterface.h"
#include "Camera.h"
#include "wUICommands.h"
#include "iMission.h"
#include "iMissionUI.h"
#include "iMissionExec.h"
#include "iTeamMngMenu.h"
#include "scFlowChartItems.h"
#include "..\MiscDll\Commands.h"
#include "..\MiscDll\LogStream.h"     // csSystem ("[tutorcam]" camera-move evidence trace)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdExecContainer
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUICmdExecContainer::Add( CUICmdExec *pCmd )
{
	ASSERT( IsValid( pCmd ) );
	if ( !IsValid( pCmd ) )
		return;
	//
	commands.push_back( pCmd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUICmdExecContainer::Update( const STime &sTime )
{
	ASSERT( !commands.empty() );
	if ( commands.empty() )
		return true;
	//
	if ( commands.front()->Update( sTime ) )
	{
		commands.front()->Finished();
		commands.erase( commands.begin() );
	}
	//
	return commands.empty();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdLocatorExec  (iUIExec @0x24eea0 ctor / @0x24e690 GetPriority)
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdLocatorExec::CUICmdLocatorExec( NWorld::CUICmdCameraLocator *_pLocator ):
	CUICmdExec( _pLocator ), pLocator( _pLocator )
{
	// release stores the locator into BOTH refcounted slots: the base CUICmdExec::pCmd
	// (implicit CUICmdCameraLocator*->CUICmd* upcast) and the derived pLocator. The
	// CPtr ctors perform the two AddRefs the decomp open-codes.
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUICmdLocatorExec::GetPriority() const
{
	// decomp: return *(int*)((int)pLocator + 0x10)  == CUICmdCameraLocator::nPriority.
	return pLocator->GetPriority();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdMoveCameraExec (retail iUIExec.obj: ctor @0x24eee0, SetTarget @0x24e9f0, Update @0x24e840,
// Finished @0x24e6a0). W5: the retail multi-waypoint morph over a CUICmdScriptMoveCamera.
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdMoveCameraExec::CUICmdMoveCameraExec( NWorld::CUICmdScriptMoveCamera *_pCmd, IMission *_pMission ):
	CUICmdLocatorExec( _pCmd ), pMission( _pMission ), pCmd( _pCmd ),
	sMorphTime( 0 ), sTransitionTime( 0 ), iLastCamera( 0 )
{
	// retail @0x24eee0: adopt + normalize the command's waypoints, sample the live camera as the
	// first segment's start pose, then UNWRAP the sampled yaw to within +-pi of the FIRST waypoint
	// -- NormalizeAngle is fmod (range (-2pi,2pi)), so DB records with multi-turn yaws (e.g. camera
	// 3138 "StartCamera" yaw = -4pi) otherwise lerp a near-full-circle swirl on timed moves.
	SetTarget( _pCmd->positions, _pCmd->transitionTime );
	pMission->GetCamera()->GetPlacement( &sCameraPos );
	NormalizePos( &sCameraPos );
	if ( !vTargetPositions.empty() )
	{
		float fYawDiff = vTargetPositions[0].fYaw - sCameraPos.fYaw;
		sCameraPos.fYaw += floorf( fabsf( fYawDiff ) / ( 2 * PI ) + 0.5f ) * Sign( fYawDiff ) * 2 * PI;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUICmdMoveCameraExec::NormalizeAngle( float *pfAngle )
{
	*pfAngle = fmod( *pfAngle, 2 * PI );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUICmdMoveCameraExec::NormalizePos( ICamera::SCameraPos *pPos )
{
	NormalizeAngle( &pPos->fPitch );
	NormalizeAngle( &pPos->fYaw );
	NormalizeAngle( &pPos->fRoll );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x24e9f0: adopt the waypoints, store the PER-WAYPOINT transition, fmod-normalize the
// pitch/yaw/roll of every waypoint.
void CUICmdMoveCameraExec::SetTarget( const vector<ICamera::SCameraPos> &positions, STime _transitionTime )
{
	vTargetPositions = positions;
	sTransitionTime = _transitionTime;
	for ( int i = 0; i < (int)vTargetPositions.size(); ++i )
		NormalizePos( &vTargetPositions[i] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x24e840: segment index = elapsed / sTransitionTime; past the last waypoint -> land
// EXACTLY on it (sTransitionTime = 0 latches "done"). On a segment change the live camera is
// re-sampled as the new start pose; within a segment lerp start->waypoint by the elapsed fraction.
bool CUICmdMoveCameraExec::Update( const STime &sTime )
{
	if ( sMorphTime == 0 )
		sMorphTime = sTime;
	if ( vTargetPositions.empty() )
		return true;   // defensive: retail would index waypoint -1 here (no producer queues an empty list)

	int nIdx;
	if ( sTransitionTime == 0 )
		nIdx = 0;
	else
		nIdx = int( ( sTime - sMorphTime ) / sTransitionTime );
	if ( nIdx < 0 || nIdx >= (int)vTargetPositions.size() )
	{
		sTransitionTime = 0;
		nIdx = (int)vTargetPositions.size() - 1;
	}
	const ICamera::SCameraPos &sTargetPos = vTargetPositions[nIdx];
	if ( nIdx != iLastCamera )
		pMission->GetCamera()->GetPlacement( &sCameraPos );   // new segment: re-sample the start pose
	iLastCamera = nIdx;

	float fCoeff = 1.0f;
	if ( sTransitionTime != 0 )
		fCoeff = float( ( sTime - sMorphTime ) % sTransitionTime ) / float( sTransitionTime );

	ICamera::SCameraPos sNewCameraPos( sCameraPos );
	sNewCameraPos.fRod = sTargetPos.fRod * fCoeff + sCameraPos.fRod * ( 1 - fCoeff );
	sNewCameraPos.fYaw = sTargetPos.fYaw * fCoeff + sCameraPos.fYaw * ( 1 - fCoeff );
	sNewCameraPos.fPitch = sTargetPos.fPitch * fCoeff + sCameraPos.fPitch * ( 1 - fCoeff );
	sNewCameraPos.fRoll = sTargetPos.fRoll * fCoeff + sCameraPos.fRoll * ( 1 - fCoeff );
	sNewCameraPos.fFOV = sTargetPos.fFOV * fCoeff + sCameraPos.fFOV * ( 1 - fCoeff );
	sNewCameraPos.ptAnchor = sTargetPos.ptAnchor * fCoeff + sCameraPos.ptAnchor * ( 1 - fCoeff );
	pMission->GetCamera()->SetPlacement( sNewCameraPos );
	return sTransitionTime == 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUICmdMoveCameraExec::Finished()
{
	// release id-queue @0x24e6a0: post the finished command's id; CWorld::ExecuteCommand ->
	// pOwnScript->RemoveUIActionID unblocks the lua WaitForUI(id) that queued this camera move
	// (CameraMove/CameraSet/CameraSequence).
	pMission->DoEvent( new NWorld::CCmdInterfaceEvent( GetCmd()->nID ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdUnitCameraExec (release iUIExec.obj: ctor @0x24f090, Update @0x24eae0, Cancel @0x24ee60,
// Finished @0x24ee80). The arbitrated auto-focus sink: gate (priority-keyed), frame the shooter (+ target)
// via CCamera::ShowPlacesFromBestPoint, hold ~4 s, then finish. Scroll-locks the camera for the shot.
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdUnitCameraExec::CUICmdUnitCameraExec( NWorld::CUICmdUnitCamera *_pCmd, IMission *_pMission ):
	CUICmdLocatorExec( _pCmd ), pMission( _pMission ), pCmd( _pCmd ), bDone( false ), tStart( 0 )
{
	// release ctor @0x24f090 tail: grab the mission camera and scroll-lock it (Lock(1)) for the framing.
	pLockedCamera = pMission->GetCamera();
	if ( IsValid( pLockedCamera ) )
		pLockedCamera->SetLock( true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUICmdUnitCameraExec::Update( const STime &sTime )
{
	if ( !IsValid( pCmd ) || !IsValid( pMission ) )
		return true;
	NWorld::CUnit *pShooter = pCmd->pUnit.GetPtr();
	NWorld::CUnit *pTarget  = pCmd->pUnitTarget.GetPtr();
	if ( !IsValid( pShooter ) && !IsValid( pTarget ) )
		return true;
	NWorld::CUnit *pPrimary = IsValid( pTarget ) ? pTarget : pShooter;
	const int nPri = GetPriority();

	// release @0x24eae0 gating (the turn-based branch reuses the CUICmdFollowCameraExec-verified checks):
	if ( !pMission->IsRealTime() )
	{
		if ( !IsVisibleByActivePlayer( pShooter, pMission ) && !IsVisibleByActivePlayer( pPrimary, pMission ) )
			return true;                                    // frame only an action the active player can see
		if ( nPri < NWorld::PR_UNIT_IS_DEAD )
		{
			if ( !pMission->IsActionExecuted() || pMission->IsReady() )
				return true;
			if ( pMission->GetWorld()->GetCurrentPlayer() == pMission->GetActivePlayer()->GetPlayer() )
				return true;                                // only the OTHER player's action
			// release @0x24ebb8 (`mov esi,4` @0x24eb96): NEVER auto-show over a camera the user has
			// taken. The selector only returns the cinematic camera while the user is hands-off, so
			// GetCamera() != GetEnemyTurnCamera() IS "the user is driving". Deaths are exempt.
			if ( pMission->GetCamera() != pMission->GetEnemyTurnCamera() )
				return true;
			if ( nPri == NWorld::PR_UNIT_ACTION && ( !IsValid( pShooter ) || !pShooter->IsPerformingAction() ) )
				return true;
			// (release @0x24eae0 also gates PR_UNIT_ACTION on mission[vtbl+0x134]->GetPlayer() != active player
			//  -- an extra "not the shooter's own player" check; omitted pending that accessor's identification.)
		}
	}
	else
	{
		if ( !IsVisibleByActivePlayer( pShooter, pMission ) )
			return true;                                    // real-time: only require the shooter visible
	}

	if ( bDone )
		return 4000 < sTime - tStart;                       // ~4 s dwell then finish
	tStart = sTime;
	bDone = true;

	// framing: two-point best-point (CCamera::ShowPlacesFromBestPoint). dist = Max(unit bbox + 0.5, 12) -- for
	// any unit the 12 floor dominates (bbox radius << 12), so pass 12 (retail Max clamps identically here).
	CVec3 ptShooter( 0, 0, 0 ), ptFocus( 0, 0, 0 );
	( IsValid( pShooter ) ? pShooter : pPrimary )->GetRealPosition( &ptShooter );
	pPrimary->GetRealPosition( &ptFocus );
	int nFloor = ( nPri >= NWorld::PR_UNIT_IS_DEAD ) ? pMission->GetCutFloor()
	                                                 : pPrimary->GetPosition().pos.GetFloor();
	int nSloMo = ( nPri >= NWorld::PR_UNIT_IS_DEAD && pCmd->bUseSloMo ) ? 3 : 1;
	ICamera *pCamera = pMission->GetCamera();
	if ( IsValid( pCamera ) )
	{
		pCamera->ShowPlacesFromBestPoint( ptShooter, ptFocus, nFloor, 12.0f, nSloMo,
			pCmd->fSloMoIncrProbability, false, false );
		if ( nPri >= NWorld::PR_UNIT_IS_DEAD && IsValid( pShooter ) )
			pCamera->FollowUnit( pShooter );                // release: FollowUnit(shooter) for a death
	}
	return false;                                           // keep alive for the dwell
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUICmdUnitCameraExec::Cancel()
{
	if ( IsValid( pLockedCamera ) )
		pLockedCamera->SetLock( false );                    // release @0x24ee60: Lock(0)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUICmdUnitCameraExec::Finished()
{
	if ( IsValid( pLockedCamera ) )
		pLockedCamera->SetLock( false );                    // release @0x24ee80: Lock(0)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdExplosionCameraExec (retail iUIExec.obj: ctor @0x24f570, Update @0x24e6e0, Finished @0x24f550).
// The blast auto-focus: frame the explosion point (rod = Max(current rod, 12), the producer's -100
// nFloor sentinel resolves to the mission's current cut floor), optional slo-mo, 3 s dwell.
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdExplosionCameraExec::CUICmdExplosionCameraExec( NWorld::CUICmdPointCamera *_pCmd, IMission *_pMission ):
	CUICmdLocatorExec( _pCmd ), pMission( _pMission ), pCmd( _pCmd ), bDone( false ), tStart( 0 )
{
	// retail ctor @0x24f570 locks UNCONDITIONALLY (an original null-deref bug); guard like the
	// unit-camera exec does.
	pLockedCamera = pMission->GetCamera();
	if ( IsValid( pLockedCamera ) )
		pLockedCamera->SetLock( true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUICmdExplosionCameraExec::Update( const STime &sTime )
{
	if ( !IsValid( pCmd ) || !IsValid( pMission ) )
		return true;
	// release @0x24e6e7 (`mov ebx,2` @0x24e6ea): the same "don't fight the user" veto as the unit
	// exec, at PR_EXPLOSION. At exactly PR_EXPLOSION the cinematic camera is asked whether the user
	// has signalled intent to take it (@0x24e730, cam vtbl+0x94) -- if so, yield too.
	if ( GetPriority() < NWorld::PR_EXPLOSION && pMission->GetCamera() != pMission->GetEnemyTurnCamera() )
		return true;
	if ( GetPriority() == NWorld::PR_EXPLOSION && IsValid( pMission->GetEnemyTurnCamera() ) )
	{
		bool bJustWanted = false;
		if ( pMission->GetEnemyTurnCamera()->UserWantedItToUnlock( &bJustWanted ) )
			return true;
	}
	if ( bDone )
		return pCmd->nExecTime * 1000 < sTime - tStart;   // retail: finish after nExecTime seconds
	tStart = sTime;
	bDone = true;
	ICamera *pCamera = pMission->GetCamera();
	if ( IsValid( pCamera ) )
	{
		// retail @0x24e6e0 first tick: rod = Max(stored placement rod, 12); nFloor < -10 (the
		// producer's -100 sentinel) -> the controller's current cut floor; then the one-point
		// best-point framing with the slo-mo ratio (bUseSloMo ? 3 : 1).
		ICamera::SCameraPos sCameraPos;
		pCamera->GetPlacement( &sCameraPos );
		const float fRod = Max( sCameraPos.fRod, 12.0f );
		int nFloor = pCmd->nFloor;
		if ( nFloor < -10 )
			nFloor = pMission->GetCutFloor();
		pCamera->ShowPlacesFromBestPoint( pCmd->ptExplosion, pCmd->ptExplosion, nFloor, fRod,
			pCmd->bUseSloMo ? 3 : 1, pCmd->fSloMoIncrProbability, false, false );
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUICmdExplosionCameraExec::Cancel()
{
	if ( IsValid( pLockedCamera ) )
		pLockedCamera->SetLock( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUICmdExplosionCameraExec::Finished()
{
	if ( IsValid( pLockedCamera ) )
		pLockedCamera->SetLock( false );                    // retail @0x24f550
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CreateCameraExecutor @0x24f150 -- RTTI dispatch of a camera-locator command to its executor
// (retail order: UnitCamera, PointCamera, ScriptMoveCamera).
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdLocatorExec* CreateCameraExecutor( NWorld::CUICmdCameraLocator *pCmd, IMission *pMission )
{
	CDynamicCast<NWorld::CUICmdUnitCamera> pUnitCam( pCmd );
	if ( pUnitCam )
		return new CUICmdUnitCameraExec( pUnitCam, pMission );
	CDynamicCast<NWorld::CUICmdPointCamera> pPointCam( pCmd );
	if ( pPointCam )
		return new CUICmdExplosionCameraExec( pPointCam, pMission );
	CDynamicCast<NWorld::CUICmdScriptMoveCamera> pScriptMove( pCmd );
	if ( pScriptMove )
		return new CUICmdMoveCameraExec( pScriptMove, pMission );   // retail: the script camera move/sequence
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// W5 serialization-convergence: the NGame per-command UICmdExec WRAPPERS are REMOVED. Retail's
// iUIExec.obj registers EXACTLY three exec classes (0x50412162 CUICmdMoveCameraExec / 0x50412163
// CUICmdUnitCameraExec / 0x00183130 CUICmdExplosionCameraExec) and executes the wrapped commands
// INLINE in CMission::ExecWorldCommand @0x1fd8c0:
//   * CUICmdExecContinueChapter 0x50412164 -- was already dead (iMission.cpp posts CICRealEndMission
//     inline, exactly retail);
//   * CUICmdExecLoadTemplate 0x51312182 / CUICmdExecShowStore 0xB1122090 / CUICmdExecShowTeamMng
//     0xB1122091 -- now inline arms in CMission::ExecWorldCommands (CICBeginMission /
//     SetPanelState(PANEL_STORE|PANEL_INVENTORY) / CICTeamMngMenu with the retail 3rd nID arg);
//   * CUICmdExecPlayDialog 0x50412165 -- was registered but never created (dialogs are handled
//     inline via CMissionDlgUI, matching retail);
//   * CUICmdFollowCameraExec / CUICmdRestoreCameraExec (unregistered) -- died with the dev-only
//     CUICmdUnit arm; retail's follow behaviour is the arbitrated CUICmdUnitCamera ->
//     CUICmdUnitCameraExec path (already live).
////////////////////////////////////////////////////////////////////////////////////////////////////
// MustReplaceCameraExecutor / IsVisibleByActivePlayer  (iUIExec free fns)
////////////////////////////////////////////////////////////////////////////////////////////////////
bool MustReplaceCameraExecutor( int nCur, int nNew )
{
	// release @0x24e5b0 priority tie-break: a strictly higher priority always wins,
	// a strictly lower one never does; on equal priority replace UNLESS the newcomer's
	// priority is exactly 2.
	if ( nNew > nCur )
		return true;
	if ( nNew < nCur )
		return false;
	return nNew != 2;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsVisibleByActivePlayer( NWorld::CUnit *pUnit, IMission *pMission )
{
	// release @0x24ea80: null / being-deleted unit -> not visible.
	if ( !IsValid( pUnit ) )
		return false;
	// Visible if the active player directly sees the unit, OR the unit is on the
	// active player's own side (you always see your own team).
	if ( !pMission->GetActivePlayer()->IsUnitVisible( pUnit ) &&
		pUnit->GetPlayer() != pMission->GetActivePlayer()->GetPlayer() )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CreateExecutor
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdExec* CreateExecutor(NWorld::CUICmd* pCmd, IMission* pMission)
{
	// W5: every per-command wrapper arm is gone -- retail executes these commands INLINE in
	// CMission::ExecWorldCommand @0x1fd8c0, and camera-locator commands (incl. the script camera
	// move) ride the dedicated pExecLocator slot via CreateCameraExecutor. Nothing builds a
	// general-purpose executor any more; an unhandled command simply drains (retail shape).
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//
using namespace NGame;
//
// cast-only registration (W4.2 CMissionBase split): CMissionBase holds CObj<CUICmdLocatorExec>
// while the type is only forward-declared in most translation units (menus derive CMissionBase
// via CRenderBaseInterface); this provides the CastToObjectBase/CastToUserObject helpers for the
// incomplete-type path. The class stays UNREGISTERED for saveload (abstract, like retail).
BASIC_REGISTER_CLASS( CUICmdLocatorExec )
// W5 serialization-convergence: registrations now == retail's exact 3-exec set (iUIExec $E31/$E33/
// $E35). The five dev wrapper ids (0x50412164/0x50412165/0x51312182/0xB1122090/0xB1122091) are
// REMOVED with their classes -- all absent from retail (see the wrapper-removal banner above).
REGISTER_SAVELOAD_CLASS( 0x50412162, CUICmdMoveCameraExec )
REGISTER_SAVELOAD_CLASS( 0x50412163, CUICmdUnitCameraExec )
REGISTER_SAVELOAD_CLASS( 0x00183130, CUICmdExplosionCameraExec )