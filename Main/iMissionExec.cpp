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
static bool bCameraFollow = false;
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
// CUICmdCameraExec
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdMoveCameraExec::CUICmdMoveCameraExec( NWorld::CUICmd *pCmd, 
	IMission *_pMission, const ICamera::SCameraPos &_sTargetPos, STime _transitionTime ): 
	CUICmdExec( pCmd ), pMission( _pMission ),
	transitionTime( _transitionTime ), sMorphTime( 0 )
{
	SetTarget( _sTargetPos );
	pMission->GetCamera()->GetPlacement( &sCameraPos );
	NormalizePos( &sCameraPos );
	// retail exec ctor @0x24eee0: UNWRAP the sampled start yaw to within +-pi of the target --
	// NormalizeAngle is fmod (range (-2pi,2pi)), so DB records with multi-turn yaws (e.g. camera
	// 3138 "StartCamera" yaw = -4pi) otherwise lerp a near-full-circle swirl on timed moves; the
	// final pose was never affected (t=0 sets fCoeff=1 immediately).
	float fYawDiff = sTargetPos.fYaw - sCameraPos.fYaw;
	sCameraPos.fYaw += floorf( fabsf( fYawDiff ) / ( 2 * PI ) + 0.5f ) * Sign( fYawDiff ) * 2 * PI;
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
void CUICmdMoveCameraExec::SetTarget( const ICamera::SCameraPos &_sTargetPos )
{
	sTargetPos = _sTargetPos;
	NormalizePos( &sTargetPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUICmdMoveCameraExec::Update( const STime &sTime )
{
	if ( sMorphTime == 0 )
		sMorphTime = sTime;

	float fCoeff;
	if ( transitionTime <= 0 )
		fCoeff = 1;
	else
		fCoeff = Min( float( sTime - sMorphTime ) / transitionTime, 1.f );

	ICamera::SCameraPos sNewCameraPos( sCameraPos );
	sNewCameraPos.fRod = sTargetPos.fRod * fCoeff + sCameraPos.fRod * ( 1 - fCoeff );
	sNewCameraPos.fYaw = sTargetPos.fYaw * fCoeff + sCameraPos.fYaw * ( 1 - fCoeff );
	sNewCameraPos.fPitch = sTargetPos.fPitch * fCoeff + sCameraPos.fPitch * ( 1 - fCoeff );
	sNewCameraPos.fRoll = sTargetPos.fRoll * fCoeff + sCameraPos.fRoll * ( 1 - fCoeff );
	sNewCameraPos.fFOV = sTargetPos.fFOV * fCoeff + sCameraPos.fFOV * ( 1 - fCoeff );
	sNewCameraPos.ptAnchor = sTargetPos.ptAnchor * fCoeff + sCameraPos.ptAnchor * ( 1 - fCoeff );
	pMission->GetCamera()->SetPlacement( sNewCameraPos );
	return fCoeff == 1.0f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUICmdMoveCameraExec::Cancel()
{
	pMission->GetCamera()->SetPlacement( sTargetPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUICmdMoveCameraExec::Finished()
{
	// release id-queue: post the finished command's id; CWorld::ExecuteCommand -> pOwnScript->RemoveUIActionID
	// unblocks the lua WaitForUI(id) that queued this camera move (CameraMove/CameraSet).
	pMission->Command( new NWorld::CCmdInterfaceEvent( GetCmd()->nID ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdCameraExec
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdFollowCameraExec::CUICmdFollowCameraExec( NWorld::CUICmd *pCmd, IMission *_pMission, NWorld::CUnit *_pUnit ):
	CUICmdExec( pCmd ), pMission( _pMission ), pUnit( _pUnit )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUICmdFollowCameraExec::Update( const STime &sTime )
{
	if ( !pUnit->IsPerformingAction() )
		return true;
	if ( !pMission->GetActivePlayer()->IsUnitVisible( pUnit ) )
		return true;
	if ( !pMission->IsActionExecuted() || pMission->IsReady() )
		return true;
	if ( pMission->GetWorld()->GetCurrentPlayer() == pMission->GetActivePlayer()->GetPlayer() )
		return true;

	// GLUE FIX: the Jan03 body hard-SET the camera onto the unit's LIVE position AND FreezeCamera(true)
	// EVERY frame while returning false, so the camera stayed glued to (and player control locked on) a
	// moving unit for the whole action. Retail replaced it with a self-terminating, eased, non-freezing
	// fly-to (CUICmdUnitCameraExec @0x24eae0). Reproduce that shape: nudge the camera toward the unit
	// via the DESIRED-placement path (ScrollAnchor smoothed -> CCamera::Update eases the live camera in,
	// player keeps control) ONCE and finish. No FreezeCamera, no per-frame re-pin.
	ICamera::SCameraPos sCameraPos;
	pMission->GetCamera()->GetPlacement( &sCameraPos );
	CVec3 ptTarget;
	pUnit->GetRealPosition( &ptTarget );
	ptTarget.z = 0;
	pMission->GetCamera()->ScrollAnchor( ptTarget - sCameraPos.ptAnchor, false, false );
	pMission->SetCutFloor( pUnit->GetPosition().pos.GetFloor() );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUICmdFollowCameraExec::Cancel()
{
	pMission->FreezeCamera( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUICmdFollowCameraExec::Finished()
{
	pMission->FreezeCamera( false );
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
// CreateCameraExecutor @0x24f150 -- RTTI dispatch of a camera-locator command to its executor. The dev has
// only the unit-camera family (the explosion CUICmdPointCamera and the script-move exec route elsewhere).
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdLocatorExec* CreateCameraExecutor( NWorld::CUICmdCameraLocator *pCmd, IMission *pMission )
{
	CDynamicCast<NWorld::CUICmdUnitCamera> pUnitCam( pCmd );
	if ( pUnitCam )
		return new CUICmdUnitCameraExec( pUnitCam, pMission );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdRestoreCameraExec
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdRestoreCameraExec::CUICmdRestoreCameraExec( NWorld::CUICmd *pCmd, IMission *pMission ):
	CUICmdMoveCameraExec( pCmd, pMission, ICamera::SCameraPos(), 0 )
{
	ICamera::SCameraPos sCameraPos;
	pMission->GetCamera()->GetPlacement( &sCameraPos );
	SetTarget( sCameraPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdExecPlayDialog
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdExecPlayDialog: public CUICmdExec
{
	OBJECT_BASIC_METHODS( CUICmdExecPlayDialog )
private:
	ZDATA
	ZPARENT( CUICmdExec )
	CPtr<IMission> pMission;
	vector< CPtr<NWorld::CAckEvent> > phrases;
	vector< CPtr<NWorld::CUnit> > units;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmdExec *)this); f.Add(3,&pMission); f.Add(4,&phrases); f.Add(5,&units); return 0; }
	//
public:
	CUICmdExecPlayDialog() {}
	CUICmdExecPlayDialog( NWorld::CUICmd *pCmd, IMission *_pMission, 	
		const vector< CPtr<NWorld::CUnit> > &_units, const vector< CPtr<NWorld::CAckEvent> > &_phrases );
	//
	bool Update( const STime &sTime );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdExecPlayDialog::CUICmdExecPlayDialog( NWorld::CUICmd *pCmd, IMission *_pMission, 	
	const vector< CPtr<NWorld::CUnit> > &_units, const vector< CPtr<NWorld::CAckEvent> > &_phrases ):
	units( _units ), phrases( _phrases ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUICmdExecPlayDialog::Update( const STime &sTime )
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdExecContinueChapter
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdExecContinueChapter: public CUICmdExec
{
	OBJECT_BASIC_METHODS( CUICmdExecContinueChapter )
private:
	ZDATA
	ZPARENT( CUICmdExec )
	CPtr<IMission> pMission;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmdExec *)this); f.Add(3,&pMission); return 0; }
	//
public:
	CUICmdExecContinueChapter() {}
	CUICmdExecContinueChapter( NWorld::CUICmd *pCmd, IMission *_pMission );
	//
	bool Update( const STime &sTime );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdExecContinueChapter::CUICmdExecContinueChapter( NWorld::CUICmd *pCmd, IMission *_pMission ):
		CUICmdExec( pCmd ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUICmdExecContinueChapter::Update( const STime &sTime )
{
	// Dead path under the script-driven exit (CMission::ExecWorldCommands now handles CUICmdContinueChapter
	// directly, pre-empting this executor). Kept defensive: post the teardown command, NOT CICEndMission --
	// CICEndMission now only re-fires OnRealExit, which would loop here.
	NMainLoop::Command( new CICRealEndMission( pMission ) );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdExecLoadTemplate
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdExecLoadTemplate: public CUICmdExec
{
	OBJECT_BASIC_METHODS( CUICmdExecLoadTemplate )
private:
	ZDATA
	ZPARENT( CUICmdExec )
	CPtr<IMission> pMission;
	CPtr<NScenario::CScenarioZone> pZone;
	int nTemplateID;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUICmdExec *)this); f.Add(3,&pMission); f.Add(4,&pZone); f.Add(5,&nTemplateID); return 0; }
	//
public:
	CUICmdExecLoadTemplate() {}
	CUICmdExecLoadTemplate( NWorld::CUICmd *pCmd, 
		IMission *_pMission, NScenario::CScenarioZone *_pZone, int _nTemplateID );
	//
	bool Update( const STime &sTime );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdExecLoadTemplate::CUICmdExecLoadTemplate( NWorld::CUICmd *pCmd, 
	IMission *_pMission, NScenario::CScenarioZone *_pZone, int _nTemplateID ):
		CUICmdExec( pCmd ), pMission( _pMission ), nTemplateID( _nTemplateID ), pZone( _pZone )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUICmdExecLoadTemplate::Update( const STime &sTime )
{
	NMainLoop::Command( new CICBeginMission( pZone, 
		nTemplateID, vector<string>(), pMission->GetRPGGame() ) );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdExecShowStore
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdExecShowStore: public CUICmdExec
{
	OBJECT_BASIC_METHODS( CUICmdExecShowStore )
private:
	ZDATA_(CUICmdExec)
	CPtr<IMission> pMission;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmdExec*)this); f.Add(2,&pMission); return 0; }

public:
	CUICmdExecShowStore() {}
	CUICmdExecShowStore( NWorld::CUICmd *pCmd, IMission *_pMission );

	bool Update( const STime &sTime );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdExecShowStore::CUICmdExecShowStore( NWorld::CUICmd *pCmd, IMission *_pMission ):
	CUICmdExec( pCmd ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUICmdExecShowStore::Update( const STime &sTime )
{
	pMission->SetPanelState( PANEL_STORE | PANEL_INVENTORY, true );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUICmdExecShowTeamMng
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUICmdExecShowTeamMng: public CUICmdExec
{
	OBJECT_BASIC_METHODS( CUICmdExecShowTeamMng )
private:
	ZDATA_(CUICmdExec)
	CPtr<IMission> pMission;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUICmdExec*)this); f.Add(2,&pMission); return 0; }

public:
	CUICmdExecShowTeamMng() {}
	CUICmdExecShowTeamMng( NWorld::CUICmd *pCmd, IMission *_pMission );

	bool Update( const STime &sTime );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUICmdExecShowTeamMng::CUICmdExecShowTeamMng( NWorld::CUICmd *pCmd, IMission *_pMission ):
	CUICmdExec( pCmd ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUICmdExecShowTeamMng::Update( const STime &sTime )
{
	NMainLoop::Command( new CICTeamMngMenu( pMission->GetActivePlayer()->GetGlobalPlayer(), pMission ) );
	return true;
}
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
	CDynamicCast<NWorld::CUICmdTurn> pTurn(pCmd);
	if (!pTurn)
	{
		CDynamicCast<NWorld::CUICmdUnit> pUnit(pCmd);
		if (pUnit)
		{
			NWorld::CUnit* pWUnit = pUnit->pUnit;
			if (!bCameraFollow)
				return 0;
			if (!pWUnit->IsPerformingAction())
				return 0;
			if (!pMission->GetActivePlayer()->IsUnitVisible(pWUnit))
				return 0;
			if (!pMission->IsActionExecuted() || pMission->IsReady())
				return 0;
			if (pMission->GetWorld()->GetCurrentPlayer() == pMission->GetActivePlayer()->GetPlayer())
				return 0;

			CUICmdExecContainer* pContainer = new CUICmdExecContainer(pUnit);
			pContainer->Add(new CUICmdFollowCameraExec(pUnit, pMission, pUnit->pUnit));
			pContainer->Add(new CUICmdRestoreCameraExec(pUnit, pMission));
			return pContainer;
		}
		else {
			CDynamicCast<NWorld::CUICmdMoveCamera> pCamera(pCmd);
			if (pCamera)
				return new CUICmdMoveCameraExec(pCmd, pMission, pCamera->pos, pCamera->transitionTime);
			else {
				CDynamicCast<NWorld::CUICmdContinueChapter> pContinueChapter(pCmd);
				if (pContinueChapter)
					return new CUICmdExecContinueChapter(pCmd, pMission);
				else {
					CDynamicCast<NWorld::CUICmdLoadTemplate> pLoadTemplate(pCmd);
					if (pLoadTemplate)
						return new CUICmdExecLoadTemplate(pCmd, pMission, pLoadTemplate->pZone, pLoadTemplate->nTemplateID);
					else {
						CDynamicCast<NWorld::CUICmdShowStore> pShowStore(pCmd);
						if (pShowStore)
							return new CUICmdExecShowStore(pCmd, pMission);
						else {
							CDynamicCast<NWorld::CUICmdShowTeamMng> pShowTeamMng(pCmd);
							if (pShowTeamMng)
								return new CUICmdExecShowTeamMng(pCmd, pMission);
						}
					}
				}
			}
		}
	}
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(iMissionExec)
	REGISTER_VAR_EX( "ui_followcamera", NGlobal::VarBoolHandler, &bCameraFollow, 0, true )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//
using namespace NGame;
//
REGISTER_SAVELOAD_CLASS( 0x50412162, CUICmdMoveCameraExec )
REGISTER_SAVELOAD_CLASS( 0x50412165, CUICmdUnitCameraExec )	// fresh id: retail 0x50412163 = kept CUICmdExecPlayDialog
REGISTER_SAVELOAD_CLASS( 0x50412163, CUICmdExecPlayDialog )
REGISTER_SAVELOAD_CLASS( 0x50412164, CUICmdExecContinueChapter )
REGISTER_SAVELOAD_CLASS( 0x51312182, CUICmdExecLoadTemplate )
REGISTER_SAVELOAD_CLASS( 0xB1122090, CUICmdExecShowStore )
REGISTER_SAVELOAD_CLASS( 0xB1122091, CUICmdExecShowTeamMng )