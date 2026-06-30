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

	ICamera::SCameraPos sCameraPos;
	pMission->GetCamera()->GetPlacement( &sCameraPos );
	pUnit->GetRealPosition( &sCameraPos.ptAnchor );
	sCameraPos.ptAnchor.z = 0;
	pMission->GetCamera()->SetPlacement( sCameraPos );
	pMission->SetCutFloor( pUnit->GetPosition().pos.GetFloor() );

	pMission->FreezeCamera( true );

	return false;
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
REGISTER_SAVELOAD_CLASS( 0x50412163, CUICmdExecPlayDialog )
REGISTER_SAVELOAD_CLASS( 0x50412164, CUICmdExecContinueChapter )
REGISTER_SAVELOAD_CLASS( 0x51312182, CUICmdExecLoadTemplate )
REGISTER_SAVELOAD_CLASS( 0xB1122090, CUICmdExecShowStore )
REGISTER_SAVELOAD_CLASS( 0xB1122091, CUICmdExecShowTeamMng )