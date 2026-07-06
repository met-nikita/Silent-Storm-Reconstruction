#include "StdAfx.h"
#include "Gfx.h"
#include "wInterface.h"
#include "wMisc.h"			// NWorld::GetDMeshUnit/GetDMeshPos -- heard-noise-marker attack target
#include "GView.h"
#include "Sound.h"
#include "RWGame.h"
#include "RWSound.h"
#include "GSceneUtils.h"
#include "Transform.h"
#include "DiscretePos.h"
#include "RPGGame.h"
#include "RPGGlobal.h"
#include "RPGItemInfo.h"
#include "RPGUnitInfo.h"
#include "..\Input\Bind.h"
#include "Interface.h"
#include "iMission.h"
#include "iChapterMap.h"
#include "iOptionsMenu.h"
#include "iGameStates.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\MiscDll\LogStream.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataSound.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "iMissionUI.h"
#include "UICommCtrls.h"		// NUI::CTextFrame -- the retail unit-hover tooltip window ("enemyToolTip")
#include "UIInterface.h"		// NUI::CInterface / CWindow ScreenToClient
#include "RPGUnitInfo.h"		// NRPG::SUnitInfo (feeds SEnemyInfo)
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSelectionWindow
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSelectionWindow: public CImage
{
	OBJECT_BASIC_METHODS(CSelectionWindow);
private:
	ZDATA_(CImage)
	CPtr<NGame::CStateSelection> pSelection;
	////
	CObj<CObjectBase> pMouseCapture;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CImage*)this); f.Add(2,&pSelection); f.Add(3,&pMouseCapture); return 0; }
public:
	CSelectionWindow() {}
	CSelectionWindow( const SWindowInfo &sInfo, NGame::CStateSelection *pSelection );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CSelectionWindow::CSelectionWindow( const SWindowInfo &sInfo, NGame::CStateSelection *_pSelection ):
	CImage( sInfo ), pSelection( _pSelection )
{
	SetColor( NGfx::SPixel8888( 0, 0, 0, 0x7F ) );
	pMouseCapture = GetInterface()->CreateMouseCapture( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSelectionWindow::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
			return true;
	case EVENT_LBUTTONUP:
		{
			pSelection->Handle();
			return true;
		}
	case EVENT_MOUSECAPTURELOSE:
		{
			pSelection->Cancel();
			break;
		}
	}

	return CImage::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const float
	F_MIN_SELECTION_DIST	= 20;
const CVec4
	V_SELECTIONCOLOR_TEAM					= CVec4( 0.1f, 0.1f, 1, 1 ),
	V_SELECTIONCOLOR_TEAM_HILIGHT	= CVec4( 1, 1, 1, 0.5 ),
	V_SELECTIONCOLOR_ENEMY				= CVec4( 1, 0.1f, 0.1f, 0.5f ),
	V_SELECTIONCOLOR_CORPSE				= CVec4( 0.1f, 1, 0.1f, 0.5f ),
	V_SELECTIONCOLOR_OBJECT				= CVec4( 0.1f, 1, 0.1f, 0.5f ),
	V_SELECTIONCOLOR_NEUTRAL			= CVec4( 0.1f, 1, 1, 0.5f );
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NGame::SayAckForAll @0x1d96d0: an ORDER handler pairs its ShowError/success with a
// per-selected-unit NWorld::CCmdPlayAck dispatched on the ONE-ARG mission Command channel
// (mission vtbl+0x2c -> world commander queue -> CWorld::ExecuteCommand -> CGlobalAck). It must
// NOT use Command(unit, cmd): that wraps the ack in CCmdSetCommand -> CUnitServer::Do, which
// CANCELS the unit's running executor (the "orders die after one step" regression).
// Retail success barks (IA_CONFIRMATION) exist ONLY for orders that move the unit somewhere:
// move/rotate/use/set-trap/set-mine/first-aid. Attack/pick-item/untrap confirm NOTHING.
static void SayAckForAll( IMission *pMission, NWorld::EInterfaceAcks eAck )
{
	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );
	for ( vector< CPtr<NGame::IUnitTracker> >::iterator i = unitsSet.begin(); i != unitsSet.end(); ++i )
		pMission->Command( new NWorld::CCmdPlayAck( (*i)->GetUnit(), eAck ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// EUnitCommandResult -> Wide String
////////////////////////////////////////////////////////////////////////////////////////////////////
void ShowError( IMission *pMission, NWorld::EUnitCommandResult eResult )
{
	switch( eResult )
	{
	case NWorld::UCR_GENERAL_FAILURE:
		csGame << L"<color=red>(debug)General failure!" << endl;
		break;
	case NWorld::UCR_INVALID_COMMAND:
		csGame << L"<color=red>(debug)This action imposible in this state!" << endl;
		break;

	case NWorld::UCR_NO_TARGET:
		csGame << L"<color=beige>Not a valid target" << endl;
		break;
	case NWorld::UCR_NOT_ENOUGH_AP:
		csGame << L"<color=beige>Not enough AP" << endl;
		break;
	case NWorld::UCR_PATH_NOT_FOUND:
		csGame << L"<color=beige>Path not found" << endl;
		break;

	case NWorld::UCR_NEED_RELOAD:
		csGame << L"<color=beige>Need reload!" << endl;
		break;
	case NWorld::UCR_NO_EQUIPMENT:
		csGame << L"<color=beige>No equipment!" << endl;
		break;
	case NWorld::UCR_WEAPON_JAMMED:
		csGame << L"<color=beige>Weapon jamed!" << endl;
		break;
	case NWorld::UCR_CRITICALS_BAN:
		csGame << L"<color=beige>Action blocked by critical" << endl;
		break;
	case NWorld::UCR_TARGET_OUT_OF_RANGE:
		csGame << L"<color=beige>Can't reach target" << endl;
		break;
	case NWorld::UCR_CANT_HEAL:
		// @0x1d5fc0 (retail ShowError case 0x10): heal target's CanHeal() failed -> feedback message + error sound.
		csGame << L"<color=beige>Can't heal this unit" << endl;
		break;

	case NWorld::UCR_INVENTORY_NO_PLACE:
		csGame << L"<color=beige>No place in inventory" << endl;
		break;
	case NWorld::UCR_NEED_HIGHER_SKILL:
		csGame << L"<color=beige>Skill too low" << endl;
		break;
	case NWorld::UCR_NOT_ALL_UNITS_NEAR_PASSAGE:
		csGame << L"<color=beige>Not all units are near the passage" << endl;
		break;
	case NWorld::UCR_PK_BAN:
		// @0x1d5fc0 (retail ShowError case 0x13 -> caseD_5): crouch+look ban is a FULLY silent
		// no-op -- no message AND no error sound. Return before the unconditional Add2DSound below.
		return;
	// UCR_NOT_HERO: deliberately no message -- retail treats it as a silent no-op
	}

	//// Error sound
	pMission->GetSoundScene()->Add2DSound( NDb::GetSound( N_SOUND_ERROR ) );

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateBase
////////////////////////////////////////////////////////////////////////////////////////////////////
CStateBase::CStateBase( bool bNeedMouseInstantly ):
	bLButtonDown( bNeedMouseInstantly )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateBase::Initialize( IMission *_pMission )
{
	pMission = _pMission;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateBase::Terminate()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateBase::ProcessEvent( const NInput::SEvent &sEvent )
{
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateBase::ProcessMessage( const NUI::SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case NUI::EVENT_LBUTTONUP:
		{
			if ( !bLButtonDown )
				return false;

			bLButtonDown = false;
			return OnLButtonUp( sEvent.nX, sEvent.nY );
		}
	case NUI::EVENT_LBUTTONDOWN:
		{
			bLButtonDown = true;
			return OnLButtonDown( sEvent.nX, sEvent.nY );
		}
	case NUI::EVENT_LBUTTONDBLCLK:
		{
			bLButtonDown = false;
			return OnLButtonDblClk( sEvent.nX, sEvent.nY );
		}
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateBase::Step()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IMission* CStateBase::GetMission() const
{
	return pMission;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// UPDATED
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateWait
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateWait::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	if ( GetMission()->IsRealTime() || !GetMission()->IsActionExecuted() )
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::SCursorInfo CStateWait::GetCursorInfo() const
{
	return NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_BUSY ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateTeam
////////////////////////////////////////////////////////////////////////////////////////////////////
CStateTeam::CStateTeam(): 
	bindModifier( "modifier" ), bModifier( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateTeam::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	CObjectBase* pObject = GetMission()->GetStateTarget();
	if ( !IsValid( pObject ) )
		return false;
	CDynamicCast<NWorld::CUnit> pUnit(pObject);
	if (pUnit)
	{
		if ( pUnit->GetPlayer() != GetMission()->GetActivePlayer()->GetPlayer() )
			return false;

		vector<CPtr<IUnitTracker> > unitsSet;
		GetMission()->GetUnits( &unitsSet );
		for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
		{
			if ( unitsSet[nTemp]->GetUnit() != pUnit )
				continue;

			pUnitTracker = unitsSet[nTemp];
			pUnitTracker->SetHilighted( true );
			return true;
		}
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateTeam::Terminate()
{
	CStateBase::Terminate();
	pUnitTracker->SetHilighted( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateTeam::ProcessEvent( const NInput::SEvent &sEvent )
{
	bindModifier.ProcessEvent( sEvent );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateTeam::Step()
{
	CStateBase::Step();
	bModifier = bindModifier.IsActive();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateTeam::OnLButtonUp( int nX, int nY )
{
	CObjectBase* pObject = GetMission()->GetStateTarget();
	if ( !IsValid( pObject ) )
		return false;
	CDynamicCast<NWorld::CUnit> pUnit(pObject);
	if (pUnit)
		GetMission()->Select( pUnit, bModifier );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateFriend
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NGame::MakeUnitStateToolTip @0x1d7b10: fill + position the hover tooltip for a unit. Full
// parity with the shipped binary: the text is the FOG-OF-WAR view (SEnemyInfo) -- the unit's name
// (format string 19329), and, per available block, either the exact HP "%d/%d" (when a roster perk /
// own-unit reveals it) or a localized health-CONDITION label (19202..19207 unit, 19208..19213 PK). The
// frame is positioned ABOVE the unit's projected screen position (raised 3.6 world units, centred), and
// HIDDEN when the unit is dead/off-screen. Used by every tactical state that hovers a unit.
static void MakeUnitStateToolTip( IMission *pMission, NWorld::CUnit *pUnit, NUI::CTextFrame *pFrame )
{
	if ( !IsValid( pFrame ) )
		return;
	if ( !IsValid( pUnit ) )
	{
		pFrame->SetStyle( NUI::STYLE_VISIBLE, false );
		return;
	}

	// --- screen projection: raise the anchor 3.6 units above the unit, project to screen; off-screen -> hide
	CVec3 vWorld = pUnit->GetPosition().GetCP();
	vWorld.z += 3.6f;   // retail constant 0x40666666 (raise above the unit)
	CVec2 vScreenRect = pMission->GetScene()->GetScreenRect();
	CTransformStack sTS = pMission->GetCameraTransform();
	CVec2 vScreen;
	if ( !TestRayInFrustrum( vWorld, &sTS, vScreenRect, &vScreen ) )
	{
		pFrame->SetStyle( NUI::STYLE_VISIBLE, false );
		return;
	}
	vScreen.x = vScreen.x * 1024 / vScreenRect.x;
	vScreen.y = vScreen.y * 768 / vScreenRect.y;

	// --- text: what the active player may learn about this unit
	NWorld::SEnemyInfo info;
	pMission->GetActivePlayer()->GetPlayer()->GetEnemyUnitInfo( pUnit, &info );

	wstring wsText = NUI::GetDBString( 19329 );   // "EnemyTooltip Name Format" header
	wsText += info.wsName;

	if ( info.bUnitInfo )                     // Character (unit) VP row
	{
		wsText += L"<br>";
		wsText += NUI::GetDBString( 19199 );       // "Character VP"
		if ( !info.bCanSeeUnitHP )
			wsText += NUI::GetDBString( 19202 + (int)info.eUnitCondition );   // 0..5 -> Healthy..Unconscious
		else
		{
			WCHAR wsHP[64];
			swprintf( wsHP, L"%d/%d", info.nUnitHP, info.nMaxUnitHP );
			wsText += wsHP;
		}
	}
	if ( info.bPKInfo )                        // Panzerklein (PK) VP row
	{
		wsText += L"<br>";
		wsText += NUI::GetDBString( 19200 );        // "PK VP"
		if ( !info.bCanSeePKHP )
			wsText += NUI::GetDBString( 19208 + (int)info.ePKCondition );     // 0..5 -> Intact..Destroyed
		else
		{
			WCHAR wsHP[64];
			swprintf( wsHP, L"%d/%d", info.nPKHP, info.nMaxPKHP );
			wsText += wsHP;
		}
	}

	pFrame->SetText( wsText );
	pFrame->SetStyle( NUI::STYLE_VISIBLE, true );

	// --- position: centre the frame horizontally on the projected unit point (client-space)
	NUI::SPoint sPos;
	pFrame->GetParent()->ScreenToClient( NUI::SPoint( vScreen.x, vScreen.y ), &sPos );
	sPos.x -= pFrame->GetSize().x / 2;
	pFrame->SetPosition( sPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CStateFriend::UpdateToolTipInfo @0x1d8290: refill the cached tooltip from the traced unit.
void CStateFriend::UpdateToolTipInfo()
{
	CDynamicCast<NWorld::CUnit> pUnit( GetMission()->GetTraceObject() );
	if ( IsValid( pUnit ) && IsValid( pUnitToolTip ) )
		MakeUnitStateToolTip( GetMission(), pUnit, pUnitToolTip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateFriend::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	CObjectBase* pObject = GetMission()->GetStateTarget();
	if ( !IsValid( pObject ) )
		return false;

	CDynamicCast<NWorld::CUnit> pUnit( pObject );
	if ( !IsValid( pUnit ) )
		return false;

	// retail CStateFriend::Initialize @0x1d90c0 (world vtbl+0xbc = (IPlayer,IPlayer)): the ACTIVE
	// player's stance toward the target's player -- not the target's stance toward me.
	NDb::EDiplomacyState eState = GetMission()->GetWorld()->GetDiplomacyState( GetMission()->GetActivePlayer()->GetPlayer(), pUnit->GetPlayer() );
	if ( ( eState != NDb::DS_ALLY ) && ( eState != NDb::DS_NEUTRAL ) )
		return false;

	if ( eState == NDb::DS_ALLY )
		pTraceSelection = GetMission()->GetRenderGame()->Select( pObject, V_SELECTIONCOLOR_TEAM );
	else
		pTraceSelection = GetMission()->GetRenderGame()->Select( pObject, V_SELECTIONCOLOR_NEUTRAL );

	if ( !pUnit->CanTalk() )
		sCursorInfo = NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_NORMAL ) );
	else
		sCursorInfo = NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_TALK ) );	// retail @0x1d90c0: UICursors row 20 "talk" (Talk.cur), not open/close

	// retail CStateFriend::Initialize @0x1d90c0: the "enemyToolTip" CTextFrame, parented to the
	// mission desktop's client window (NOT the interface/cursor), then filled by UpdateToolTipInfo.
	pUnitToolTip = new NUI::CTextFrame( NUI::SWindowInfo( GetMission()->GetDesktop()->GetClientWindow(),
		NUI::SPoint( 0, 0 ), NUI::SPoint( 0, 0 ), "enemyToolTip", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_TRANSPARENT | NUI::STYLE_TOPMOST ) );
	UpdateToolTipInfo();

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateFriend::Step()
{
	CStateBase::Step();
	// retail: refresh the tooltip each frame (MakeUnitStateToolTip re-projects + re-positions it above
	// the unit; it also hides itself when the unit dies or leaves the screen).
	UpdateToolTipInfo();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateFriend::Terminate()
{
	CStateBase::Terminate();
	pTraceSelection = 0;
	pUnitToolTip = 0;   // release the tooltip window (unparents from the interface)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateFriend::OnLButtonUp( int nX, int nY )
{
	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetSelectedUnits( &unitsSet );
	if ( unitsSet.size() != 1 )
		return false;

	CObjectBase* pObject = GetMission()->GetStateTarget();
	if ( !IsValid( pObject ) )
		return false;

	CDynamicCast<NWorld::CUnit> pUnit( pObject );
	if ( IsValid( pUnit ) && pUnit->CanTalk() )
		GetMission()->Command( unitsSet[0]->GetUnit(), new NWorld::CCmdTalk( pUnit ) );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::SCursorInfo CStateFriend::GetCursorInfo() const
{
	return sCursorInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateMove
////////////////////////////////////////////////////////////////////////////////////////////////////
CStateMove::CStateMove( bool _bForced ):
	bForced( _bForced )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateMove::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	bAnchorSet = false;

	SActionInfo sGeneralInfo;
	pMission->GetActionInfo( UA_MOVE, &sGeneralInfo );
	if ( ( GetType() == FORCED ) && ( !sGeneralInfo.bOk || !sGeneralInfo.bEnoughAP ) )
	{
		ShowError( GetMission(), sGeneralInfo.eResult );
		return false;
	}

	UpdateCursor();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateMove::ProcessEvent( const NInput::SEvent &sEvent )
{
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IState::EType CStateMove::GetType() const
{
	if ( bForced )
		return FORCED;

	return UPDATED;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateMove::OnLButtonUp( int nX, int nY )
{
	DoMove( GetType() == FORCED );

	if ( GetType() == FORCED )
		GetMission()->ResetState();

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateMove::OnLButtonDown( int nX, int nY )
{
	vAnchor = GetMission()->GetCursor()->GetPos();
	bAnchorSet = true;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateMove::OnLButtonDblClk( int nX, int nY )
{
	DoMove( true );

	if ( GetType() == FORCED )
		GetMission()->ResetState();

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::SCursorInfo CStateMove::GetCursorInfo() const
{
	return sCursorInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateMove::DoMove( bool bInstant )
{
	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetSelectedUnits( &unitsSet );

	NAI::SPosition pos;
	if ( !GetMission()->GetTracePosition( &pos ) )
	{
		ShowError( GetMission(), NWorld::UCR_PATH_NOT_FOUND );
		SayAckForAll( GetMission(), NWorld::IA_IMPOSSIBLE_TO_PERFORM );	// retail CStateMove::DoMove @0x1dbb10
		return;
	}

	NAI::SPathPlace p( pos.p );
	p.SetPose( NAI::CM_CROUCH );
	if ( !GetMission()->GetWorld()->GetPathNetwork()->IsNativePassable( p ) )
	{
		ShowError( GetMission(), NWorld::UCR_PATH_NOT_FOUND );
		SayAckForAll( GetMission(), NWorld::IA_IMPOSSIBLE_TO_PERFORM );
		return;
	}

	vector< NAI::SPosition > unitPlaces;
	for ( vector< CPtr<NGame::IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
	{
		NAI::SPosition posUnit = (*iTemp)->GetUnit()->GetPosition().pos;
		unitPlaces.push_back( posUnit );
	}

	GetMission()->GetWorld()->GetPathNetwork()->FormationMoveTo( &unitPlaces, pos  );

	for ( int nUnit = 0; nUnit < unitsSet.size(); ++nUnit )
		unitsSet[nUnit]->SetTargetPosition( unitPlaces[nUnit], bInstant );

	// retail barks only when the per-unit SetTargetPosition results aggregate to success; the
	// unconditional bark is an accepted approximation (dev SetTargetPosition returns void)
	SayAckForAll( GetMission(), NWorld::IA_CONFIRMATION );	// retail: "order acknowledged" on a move order
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateMove::UpdateCursor()
{
	NAI::SPosition pos;
	if ( !GetMission()->GetTracePosition( &pos ) )
	{
		sCursorInfo = NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_BLOCK ) );
		return;
	}

	NAI::SPathPlace p( pos.p );
	p.SetPose( NAI::CM_CROUCH );
	if ( GetMission()->GetWorld()->GetPathNetwork()->IsNativePassable( p ) )
		sCursorInfo = NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_MOVE ) );
	else
		sCursorInfo = NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_BLOCK ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateMove::Step()
{
	CStateBase::Step();

	if ( bAnchorSet && ( fabs( GetMission()->GetCursor()->GetPos() - vAnchor ) > F_MIN_SELECTION_DIST ) )
		GetMission()->CommandState( new CStateSelection( vAnchor ) );

	UpdateCursor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateAttack
////////////////////////////////////////////////////////////////////////////////////////////////////
CStateAttack::CStateAttack(): 
	eHitLocation( NAI::HL_ANY ),
	bindHitLocationHead( "hitlocation_head" ), bindHitLocationBody( "hitlocation_body" ), 
	bindHitLocationLArm( "hitlocation_larm" ), bindHitLocationRArm( "hitlocation_rarm" ), bindHitLocationLLeg( "hitlocation_lleg" ), bindHitLocationRLeg( "hitlocation_rleg" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CStateAttack::CStateAttack( bool _bForced ):
	bForced( _bForced ), eHitLocation( NAI::HL_ANY ),
	bindHitLocationHead( "hitlocation_head" ), bindHitLocationBody( "hitlocation_body" ), 
	bindHitLocationLArm( "hitlocation_larm" ), bindHitLocationRArm( "hitlocation_rarm" ), bindHitLocationLLeg( "hitlocation_lleg" ), bindHitLocationRLeg( "hitlocation_rleg" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateAttack::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	SActionInfo sGeneralInfo;
	pMission->GetActionInfo( UA_ATTACK, &sGeneralInfo );
	if ( ( GetType() == FORCED ) && ( !sGeneralInfo.bOk || !sGeneralInfo.bEnoughAP ) )
	{
		ShowError( GetMission(), sGeneralInfo.eResult );
		return false;
	}

	if ( GetType() != FORCED )
	{
		CObjectBase* pObject = GetMission()->GetStateTarget();
		if ( !IsValid( pObject ) )
			return false;

		// retail CStateAttack::Initialize @0x1dd240 accepts CUnit OR IObject OR IAISound targets;
		// the heard-noise marker (dev CDMesh) is the IAISound case. The own-unit check applies to
		// real units only (the marker skips it, exactly retail).
		NWorld::CUnit* pUnit = dynamic_cast<NWorld::CUnit*>( pObject );
		bool bHeardMarker = ( NWorld::GetDMeshUnit( pObject ) != 0 );
		if ( !IsValid( pUnit ) && !bHeardMarker )
			return false;

		if ( IsValid( pUnit ) && pUnit->GetPlayer() == GetMission()->GetActivePlayer()->GetPlayer() )
			return false;
	}

	UpdateTraceSelection();
	UpdateBlockedState();
	UpdateCursorInfo();
	UpdateCursor();

	// retail CStateAttack::Initialize @0x1dd240: the "enemyToolTip" CTextFrame over the aimed enemy
	// (only the non-FORCED aim-at-unit state has a real unit target).
	if ( GetType() != FORCED )
	{
		pUnitToolTip = new NUI::CTextFrame( NUI::SWindowInfo( GetMission()->GetDesktop()->GetClientWindow(),
			NUI::SPoint( 0, 0 ), NUI::SPoint( 0, 0 ), "enemyToolTip", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_TRANSPARENT | NUI::STYLE_TOPMOST ) );
		UpdateToolTipInfo();
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CStateAttack::UpdateToolTipInfo @0x1d8540: refill from the aimed (state-target) unit.
void CStateAttack::UpdateToolTipInfo()
{
	CDynamicCast<NWorld::CUnit> pUnit( GetMission()->GetStateTarget() );
	if ( IsValid( pUnit ) && IsValid( pUnitToolTip ) )
		MakeUnitStateToolTip( GetMission(), pUnit, pUnitToolTip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateAttack::Terminate()
{
	CStateBase::Terminate();
	pTraceSelection = 0;
	pUnitToolTip = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IState::EType CStateAttack::GetType() const
{
	if ( bForced )
		return FORCED;

	return UPDATED;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateAttack::OnLButtonUp( int nX, int nY )
{
	SActionInfo sInfo;
	CObj<NWorld::CCmd> pCmd = GetTargetCmd();
	if ( !IsValid( pCmd ) )
		return true;

	GetMission()->CanDoCommand( pCmd, false, &sInfo );
	if ( !sInfo.bAvailable || !sInfo.bOk )
	{
		ShowError( GetMission(), sInfo.eResult );
		// retail CStateAttack::OnLButtonUp @0x1dbec0: the out-of-ammo result (retail raw code 10)
		// barks "weapon empty", any other failure "impossible to perform". Dev's enum diverged
		// from the retail ordinals -- the semantic equivalent is UCR_NEED_RELOAD.
		SayAckForAll( GetMission(), sInfo.eResult == NWorld::UCR_NEED_RELOAD ?
			NWorld::IA_WEAPON_EMPTY : NWorld::IA_IMPOSSIBLE_TO_PERFORM );
		return false;
	}

	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetSelectedUnits( &unitsSet );
	for ( vector< CPtr<NGame::IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
		GetMission()->Command( (*iTemp)->GetUnit(), pCmd );

	// retail plays NO success confirmation for an attack order ("order acknowledged" is
	// reserved for orders that move the unit somewhere)

	if ( GetType() == FORCED )
		GetMission()->ResetState();

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::SCursorInfo CStateAttack::GetCursorInfo() const
{
	return sCursorInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CCmd* CStateAttack::GetTargetCmd()
{
	CObjectBase* pTargetObject = GetMission()->GetStateTarget();
	if ( IsValid( pTargetObject ) )
	{
		// retail @0x1d9e20: a heard-noise marker is attacked as a TILE at the noise position
		// (z + 1.0) -- never as an object and never as the (hidden) unit.
		CVec3 posMarker;
		if ( NWorld::GetDMeshPos( pTargetObject, &posMarker ) )
		{
			posMarker.z += 1.0f;
			return new NWorld::CCmdShootTile( posMarker );
		}
		return new NWorld::CCmdShootObject( pTargetObject, 0, eHitLocation );
	}

	CVec3 pos;
	if ( !GetMission()->GetTracePosition( &pos ) )
		return 0;

	return new NWorld::CCmdShootTile( pos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateAttack::UpdateCursor()
{
	NAI::EHitLocation eNewHitLocation = NAI::HL_ANY;

	if ( bindHitLocationHead.IsActive() )
		eNewHitLocation = NAI::HL_HEAD;
	else if ( bindHitLocationBody.IsActive() )
		eNewHitLocation = NAI::HL_BODY;
	else if ( bindHitLocationLArm.IsActive() )
		eNewHitLocation = NAI::HL_LHAND;
	else if ( bindHitLocationRArm.IsActive() )
		eNewHitLocation = NAI::HL_RHAND;
	else if ( bindHitLocationLLeg.IsActive() )
		eNewHitLocation = NAI::HL_LLEG;
	else if ( bindHitLocationRLeg.IsActive() )
		eNewHitLocation = NAI::HL_RLEG;

	if ( eNewHitLocation != eHitLocation )
	{
		eHitLocation = eNewHitLocation;
		UpdateCursorInfo();
	}

	if ( !bActionUnavailable )
	{
		switch ( eHitLocation )
		{
		case NAI::HL_ANY:
			{
				int nDefault = N_CURSOR_ATTACK;
				vector< CPtr<NGame::IUnitTracker> > unitsSet;
				GetMission()->GetSelectedUnits( &unitsSet );
				if ( unitsSet.size() == 1 )
				{
					NWorld::CUnit::EState eState = unitsSet[0]->GetUnit()->GetState();
					switch( eState )
					{
						case NWorld::CUnit::ST_NORMAL_MELEE:
							nDefault = N_CURSOR_ATTACK_MELEE;
							break;
						case NWorld::CUnit::ST_NORMAL_GRENADE:
							nDefault = N_CURSOR_ATTACK_GRENADE;
							break;
						case NWorld::CUnit::ST_NORMAL_PISTOL:
							nDefault = N_CURSOR_ATTACK_PISTOL;
							break;
						case NWorld::CUnit::ST_NORMAL_RIFLE:
							nDefault = N_CURSOR_ATTACK_RIFLE;
							break;
						case NWorld::CUnit::ST_NORMAL_SUB_MACHINE_GUN:
						case NWorld::CUnit::ST_NORMAL_HAND_MACHINE_GUN: // ??????????????????????
						case NWorld::CUnit::ST_NORMAL_RLAUNCHER: // ??????????????????????
							nDefault = N_CURSOR_ATTACK_MACHINEGUN;
							break;
					}
				}

				sCursorInfo.pTexture = NDb::GetUITexture( nDefault );
			}
			break;
		case NAI::HL_HEAD:
			sCursorInfo.pTexture = NDb::GetUITexture( N_CURSOR_ATTACK_HEAD );
			break;
		case NAI::HL_BODY:
			sCursorInfo.pTexture = NDb::GetUITexture( N_CURSOR_ATTACK_BODY );
			break;
		case NAI::HL_LHAND:
			sCursorInfo.pTexture = NDb::GetUITexture( N_CURSOR_ATTACK_LARM );
			break;
		case NAI::HL_RHAND:
			sCursorInfo.pTexture = NDb::GetUITexture( N_CURSOR_ATTACK_RARM );
			break;
		case NAI::HL_LLEG:
			sCursorInfo.pTexture = NDb::GetUITexture( N_CURSOR_ATTACK_LLEG );
			break;
		case NAI::HL_RLEG:
			sCursorInfo.pTexture = NDb::GetUITexture( N_CURSOR_ATTACK_RLEG );
			break;
		default:
			ASSERT( 0 );
		}
	}
	else
		sCursorInfo = NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_BLOCK ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateAttack::UpdateCursorInfo()
{
	sCursorInfo.wsText = L"";

	int nSelectedCount = GetMission()->CountSelected();
	if ( nSelectedCount )
	{
		CPtr<CObjectBase> pTraceObject = GetMission()->GetStateTarget();

		int nTotalShootAP = 0;
		int nMin = 0x7fffffff, nMax = -0x7fffffff; // numeric_limits<int>::max(), numeric_limits<int>::min();
		vector< CPtr<NGame::IUnitTracker> > unitsSet;
		GetMission()->GetSelectedUnits( &unitsSet );
		for ( vector< CPtr<NGame::IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
		{
			CPtr<NWorld::IWorld> pWorld = GetMission()->GetWorld();

			int nToHit = 0;

			NAI::SPosition pos;
			bool bTraceOk = GetMission()->GetTracePosition( &pos );
			CDynamicCast<NWorld::CUnit> pUnit(pTraceObject);
			if (pUnit)
			{
				CDynamicCast<NRPG::IWeaponItemInfo> pWeapon((*iTemp)->GetUnit()->GetRPG()->GetInventoryInfo()->GetActive());
				if (pWeapon)
				{
					if ( !pWeapon->GetDBWeapon()->bBazookaLogic )
						nToHit = pWorld->GetGame()->GetCompositeToHit( (*iTemp)->GetUnit(), pUnit, eHitLocation, pWorld->IsFirstTurn() );
					else
					{
						if ( bTraceOk )
							nToHit = pWorld->GetGame()->GetBazookaToHit( (*iTemp)->GetUnit(), pos.GetCP(),	NAI::THL_MIDDLE, pWorld->IsFirstTurn() );
					}
				}
				else {
					CDynamicCast<NRPG::IGrenadeItemInfo> pGrenade((*iTemp)->GetUnit()->GetRPG()->GetInventoryInfo()->GetActive());
					if (pGrenade)
					{
						if (bTraceOk)
							nToHit = pWorld->GetGame()->GetGrenadeCompositeToHit((*iTemp)->GetUnit(), pos.GetCP(), pWorld->IsFirstTurn(), pGrenade->GetDBGrenade());
					}
					else {
						CDynamicCast<NRPG::IMeleeWeaponItem> pMelee((*iTemp)->GetUnit()->GetRPG()->GetInventoryInfo()->GetActive());
						if (pMelee)
						{
							//if ( pMelee->GetDBMeleeWeapon()->bThrowing )
							nToHit = pWorld->GetGame()->GetCompositeToHit((*iTemp)->GetUnit(), pUnit, eHitLocation, pWorld->IsFirstTurn());
						}
					}
				}
			}
			else if ( bTraceOk )
			{
				CDynamicCast<NRPG::IGrenadeItemInfo> pGrenade((*iTemp)->GetUnit()->GetRPG()->GetInventoryInfo()->GetActive());
				if (pGrenade)
					nToHit = pWorld->GetGame()->GetGrenadeCompositeToHit( (*iTemp)->GetUnit(), pos.GetCP(), pWorld->IsFirstTurn(), pGrenade->GetDBGrenade() );
				else {
					CDynamicCast<NRPG::IWeaponItemInfo> pWeapon((*iTemp)->GetUnit()->GetRPG()->GetInventoryInfo()->GetActive());
					if (pWeapon)
					{
						if (!pWeapon->GetDBWeapon()->bBazookaLogic)
							nToHit = pWorld->GetGame()->GetTileCompositeToHit((*iTemp)->GetUnit(), pos.GetCP(), NAI::THL_MIDDLE, pWorld->IsFirstTurn());
						else
							nToHit = pWorld->GetGame()->GetBazookaToHit((*iTemp)->GetUnit(), pos.GetCP(),
								NAI::THL_MIDDLE, pWorld->IsFirstTurn());
					}
					else {
						CDynamicCast<NRPG::IMeleeWeaponItem> pMelee((*iTemp)->GetUnit()->GetRPG()->GetInventoryInfo()->GetActive());
						if (pMelee)
						{
							// retail UpdateCursorInfo @0x1da200 routes EVERY weapon at a tile target
							// through the same NRPG::GetToHit tile overload (via the CCmdShootTile
							// target command) -- the throwing-vs-swinging dispatch happens inside
							// RPGUnitGetTileToHit @0x2b4df0 (TH_THROWING -> knife calcer, TH_MELEE ->
							// 100 when the cover walk connects). The old bThrowing-only gate left
							// nToHit at 0 for a swung melee weapon aimed at ground/walls/objects.
							nToHit = pWorld->GetGame()->GetTileCompositeToHit((*iTemp)->GetUnit(), pos.GetCP(), NAI::THL_MIDDLE, pWorld->IsFirstTurn());
						}
					}
				}
			}

			// retail UpdateCursorInfo @0x1da200: the composite to-hit returns the -1 sentinel for an
			// impossible attack (melee swing out of arm's reach, blocked direction); such units are
			// EXCLUDED from the min/max fold, and when EVERY selected unit reports -1 no percentage
			// is shown at all (`if (iVar6 != -1)` around the fold, `if (iVar9 != -1)` around the print).
			if ( nToHit == -1 )
				continue;

			nMin = min( nMin, nToHit );
			nMax = max( nMax, nToHit );

			WCHAR wsString[256];
			if ( unitsSet.size() == 1 )
			{
				if ( !GetMission()->IsRealTime() )
					swprintf( wsString, L"<normal>%2d%%<br>AP: %d", nMin, nActionAP );
				else
					swprintf( wsString, L"<normal>%2d%%", nMin );
			}
			else
				swprintf( wsString, L"<normal>%2d-%2d%%", nMin, nMax );

			sCursorInfo.wsText = wsString;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateAttack::UpdateBlockedState()
{
	nActionAP = 0;
	bActionUnavailable = true;

	SActionInfo sInfo;
	CObj<NWorld::CCmd> pCmd = GetTargetCmd();
	if ( !IsValid( pCmd ) )
		return;

	GetMission()->CanDoCommand( pCmd, false, &sInfo );

	nActionAP = sInfo.nActionAP;
	bActionUnavailable = !sInfo.bAvailable || !sInfo.bOk;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateAttack::UpdateTraceSelection()
{
	pTraceSelection = 0;

	CObjectBase* pObject = GetMission()->GetStateTarget();
	if ( !IsValid( pObject ) )
		return;

	if ( pObject )
		pTraceSelection = GetMission()->GetRenderGame()->Select( pObject, V_SELECTIONCOLOR_ENEMY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateAttack::Step()
{
	CStateBase::Step();

	if ( GetType() == FORCED )
	{
		UpdateBlockedState();
		UpdateCursorInfo();
	}

	UpdateCursor();

	// retail: refresh the aimed enemy's tooltip each frame (MakeUnitStateToolTip re-projects/positions
	// it above the unit and hides it when the target dies or leaves the screen)
	UpdateToolTipInfo();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateUse
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateUse::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	SActionInfo sGeneralInfo;
	pMission->GetActionInfo( UA_USE, &sGeneralInfo );
	if ( ( GetType() == FORCED ) && ( !sGeneralInfo.bOk || !sGeneralInfo.bEnoughAP ) )
	{
		ShowError( GetMission(), sGeneralInfo.eResult );
		return false;
	}

	CObjectBase* pTargetObject = GetMission()->GetStateTarget();
	if ( !IsValid( pTargetObject ) )
		return false;

	CObjectBase *pObject = pTargetObject;
	if ( !IsValid( pObject ) )
		return false;

	bool bRet = false;
	// retail CStateUse::Initialize @0x1d85a0 cursor pick, DISASM-verified (Ghidra drops the ECX
	// literal ids -- read straight off the binary):
	//   @0x5d8a01  mov ecx,0x15 -> GetUICursor(21) "use"          = Use.cur     -- the DEFAULT: cannon
	//              MOUNT, carry-body, and any non-door/non-passage use target show the Use hand
	//   @0x5d8988  mov ecx,0x16 -> GetUICursor(22) "usetool"      = UseTool.cur -- ONLY for a
	//              window/door whose IWindowDoor::IsLockedDoor() (vtbl+0x14, @0x5d897c) is true
	//              (locked door = the lockpick/tool cursor)
	//   @0x5d89ab/@0x5d89de  mov ecx,0x17 -> GetUICursor(23) "use_openclose"    -- unlocked
	//              window/door and IPassageObject (hatch/ladder)
	// The dev tree instead showed UseTool.cur when manning a cannon/mounted gun and never showed
	// the lockpick cursor on locked doors.
	int nCursorID = N_CURSOR_USE;
	CVec4 vHilightColor( V_SELECTIONCOLOR_OBJECT );
	CDynamicCast<NWorld::CUnit> pDeadUnit(pObject);
	if (pDeadUnit)
	{
		bRet = pDeadUnit->IsDead() || pDeadUnit->IsUnconscious();
		vHilightColor = V_SELECTIONCOLOR_CORPSE;
		nCursorID = N_CURSOR_USE;			// carry body -> Use.cur (hand): retail default branch, id 21
	}
	else {
		CDynamicCast<NWorld::IObject> pTempObject(pObject);
		if (pTempObject)
		{
			vHilightColor = V_SELECTIONCOLOR_OBJECT;
			CDynamicCast<NWorld::ICannon> pCannon(pTempObject.GetPtr());
			if (pCannon)
			{
				bRet = !pCannon->IsBroken();
				nCursorID = N_CURSOR_USE;		// mount a cannon/turret -> Use.cur (hand): retail id 21 (@0x5d8a01)
			}
			else {
				CDynamicCast<NWorld::IWindowDoor> pWindowDoor(pTempObject.GetPtr());
				if (pWindowDoor)
				{
					bRet = !pWindowDoor->IsBroken();
					// retail @0x5d897c: IsLockedDoor() -> UseTool.cur (lockpick, id 22) else UseOpen&Close.cur (id 23)
					nCursorID = pWindowDoor->IsLockedDoor() ? N_CURSOR_USE_TOOL : N_CURSOR_OPEN_CLOSE;
				}
				else {
					CDynamicCast<NWorld::IPassageObject> pPassage(pTempObject.GetPtr());
					if (pPassage)
					{
						bRet = !pPassage->IsBroken();
						nCursorID = N_CURSOR_OPEN_CLOSE;	// hatch/ladder passage -> UseOpen&Close.cur: retail id 23 (@0x5d89de)
					}
				}
			}
		}
	}

	if ( !bRet )
		return false;

	pTraceSelection = GetMission()->GetRenderGame()->Select( pObject, vHilightColor );

	SActionInfo sInfo;
	CObj<NWorld::CCmd> pCmd = GetTargetCmd();
	if ( IsValid( pCmd ) )
		GetMission()->CanDoCommand( GetTargetCmd(), false, &sInfo );

	if ( sInfo.bOk )
	{
		WCHAR wsBuffer[1024] = L"";
		if ( !GetMission()->IsRealTime() )
			swprintf( wsBuffer, L"AP: %d", sInfo.nActionAP );
		sCursorInfo = NUI::SCursorInfo( NDb::GetUITexture( nCursorID ), wsBuffer );
	}
	else
		sCursorInfo = NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_BLOCK ) );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateUse::Terminate()
{
	CStateBase::Terminate();
	pTraceSelection = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateUse::OnLButtonUp( int nX, int nY )
{
	SActionInfo sInfo;
	CObj<NWorld::CCmd> pCmd = GetTargetCmd();
	if ( !IsValid( pCmd ) )
		return true;

	GetMission()->CanDoCommand( pCmd, false, &sInfo );
	if ( !sInfo.bAvailable || !sInfo.bOk )
	{
		ShowError( GetMission(), sInfo.eResult );
		// retail CStateUse::OnLButtonUp barks NOTHING on failure
		return false;
	}

	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() < 1 )
		return false;

	GetMission()->Command( unitsSet.front()->GetUnit(), pCmd );
	SayAckForAll( GetMission(), NWorld::IA_CONFIRMATION );	// retail: use IS a go-there order
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::SCursorInfo CStateUse::GetCursorInfo() const
{
	return sCursorInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CCmd* CStateUse::GetTargetCmd()
{
	CObjectBase* pObject = GetMission()->GetStateTarget();
	if ( !IsValid( pObject ) )
		return 0;
	CDynamicCast<NWorld::CUnit> pDeadUnit(pObject);
	if (pDeadUnit)
	{
		if ( pDeadUnit->IsDead() || pDeadUnit->IsUnconscious() )
		{
			if ( !pDeadUnit->GetCorpseCarrier() )
				return new NWorld::CCmdTakeCorpse( pDeadUnit );
			else
				return new NWorld::CCmdDropCorpse( pDeadUnit );
		}
	}
	else {
		CDynamicCast<NWorld::IObject> pTempObject(pObject);
		if (pTempObject)
		{
			CDynamicCast<NWorld::ICannon> pCannon(pTempObject.GetPtr());
			if (pCannon)
			{
				if (!pCannon->IsBroken())
				{
					if (!pCannon->IsOccupied())
						return new NWorld::CCmdCannon(pTempObject);
					else
						return new NWorld::CCmdExitCannon;
				}
			}
			else {
				CDynamicCast<NWorld::IWindowDoor> pWindowDoor(pTempObject.GetPtr());
				if (pWindowDoor)
				{
					if (!pWindowDoor->IsBroken())
						return new NWorld::CCmdOpenClose(pTempObject, !pWindowDoor->IsOpen());
				}
				else {
					CDynamicCast<NWorld::IPassageObject> pPassage(pTempObject.GetPtr());
					if (pPassage)
					{
						if (!pPassage->IsBroken())
							return new NWorld::CCmdUsePassage(pPassage);
					}
				}
			}
		}
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStatePickItem
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStatePickItem::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	CObjectBase* pTargetObject = GetMission()->GetStateTarget();
	if ( !IsValid( pTargetObject ) )
		return false;

	CObjectBase *pObject = pTargetObject;
	if ( !IsValid( pObject ) )
		return false;
	CDynamicCast<NWorld::IItem> pItem(pObject);
	if (pItem)
	{
		if ( !IsValid( pItem->GetInvItem() ) )
			return false;

		pTraceSelection = GetMission()->GetRenderGame()->Select( pObject, V_SELECTIONCOLOR_OBJECT );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStatePickItem::Terminate()
{
	CStateBase::Terminate();
	pTraceSelection = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStatePickItem::OnLButtonUp( int nX, int nY )
{
	SActionInfo sInfo;
	CObj<NWorld::CCmd> pCmd = GetTargetCmd();
	if ( !IsValid( pCmd ) )
		return true;

	GetMission()->CanDoCommand( pCmd, false, &sInfo );
	if ( !sInfo.bAvailable || !sInfo.bOk )
	{
		ShowError( GetMission(), sInfo.eResult );
		// retail CStatePickItem @0x1dc7b0 barks nothing here (its gated call passes the raw
		// eResult, which falls outside PlayAck's 0..3 switch -- effectively silent; retail quirk)
		return false;
	}

	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() != 1 )
		return false;

	GetMission()->Command( unitsSet[0]->GetUnit(), pCmd );
	// retail plays NO success confirmation for pick-item
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::SCursorInfo CStatePickItem::GetCursorInfo() const
{
	return NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_PICKITEM ) );	// retail @0x1dc570: UICursors row 18 "pickitem"
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CCmd* CStatePickItem::GetTargetCmd()
{
	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() != 1 )
		return 0;

	CPtr<CObjectBase> pObject = GetMission()->GetStateTarget();
	CDynamicCast<NWorld::IItem> pTempItem(pObject);
	if (pTempItem)
	{
		NWorld::SItem sSource;
		sSource.eType = NWorld::SItem::GROUND;
		sSource.pItem = pTempItem->GetInvItem();
		sSource.pWorldItem = pTempItem;
		sSource.pUnit = 0;
		sSource.nSlot = -1;

		if ( GetMission()->GetPanelState( PANEL_INVENTORY ) == 0 )
		{
			NWorld::SItem sTarget;
			sTarget.eType = NWorld::SItem::BACKPACK;
			sTarget.pUnit = unitsSet[0]->GetUnit();
			sTarget.sPosition = CTPoint<int>( -1, -1 );
			return new NWorld::CCmdMoveInventoryItem( sSource, sTarget );
		}

		return new NWorld::CCmdMoveInventoryItem( sSource, NWorld::SItem( unitsSet[0]->GetUnit(), NWorld::SItem::HAND ) );
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateDragItem
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateDragItem::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	if ( !GetMission()->IsRealTime() && ( GetMission()->GetActivePlayer()->GetPlayer() != GetMission()->GetWorld()->GetCurrentPlayer() ) )
		return false;

	NWorld::IPlayer::SItemInfo sInfo;
	if ( !GetMission()->GetActivePlayer()->GetPlayer()->GetInHandItem( &sInfo ) )
		return false;

	NUI::SPoint sCellSize( 36, 36 );

	const NUI::SPoint &sInventoryItemSize = sInfo.pItem->GetSize();
	NUI::SPoint sItemSize( sCellSize.x * sInventoryItemSize.x, sCellSize.y * sInventoryItemSize.y );

	CPtr<NDb::CRPGItem> pRPGItem( sInfo.pItem->GetDBItem() );
	pModel = new NUI::CModel( NUI::SWindowInfo( pMission->GetInterface(), NUI::SPoint( 0, 0 ), sItemSize, "icon", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_TOPMOST | NUI::STYLE_TRANSPARENT ) );
	if ( pRPGItem->pModel )
	{
		const NDb::SCameraParams &sCamera = pRPGItem->sCameras[NDb::CAMERA_NORMAL];

		SRand sRnd;
		CVec3 vForwardDir;
		CQuat q = CQuat( sCamera.fYaw, V3_AXIS_Z ) * CQuat( sCamera.fPitch, V3_AXIS_X );
		q.GetYAxis( &vForwardDir );

		CVec3 vCP( sCamera.vAnchor - vForwardDir * sCamera.fDistance );
		SFBTransform res;
		MakeMatrix( &res, sCamera.fPitch, sCamera.fYaw, sCamera.fRoll, vCP );

		pModel->SetModel( pRPGItem->pModel->CreateModel( &sRnd ) );
		pModel->SetTransform( new CFBTransform( res ) );
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateDragItem::Terminate()
{
	pModel = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateDragItem::Step()
{
	CVec2 vScreenRect = GetMission()->GetScene()->GetScreenRect();
	const NUI::SPoint &sSize = pModel->GetSize();

	CVec2 vCursorPos = GetMission()->GetCursor()->GetPos();
	vCursorPos.x = vCursorPos.x * 1024 / vScreenRect.x;
	vCursorPos.y = vCursorPos.y * 768 / vScreenRect.y;

	NUI::SPoint sPosition( vCursorPos.x - sSize.x / 2, vCursorPos.y - sSize.y / 2 );
	pModel->SetPosition( sPosition );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateDragItem::OnLButtonUp( int nX, int nY )
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateDragItem::OnLButtonDown( int nX, int nY )
{
	NWorld::IPlayer::SItemInfo sInfo;
	if ( !GetMission()->GetActivePlayer()->GetPlayer()->GetInHandItem( &sInfo ) )
		return false;

	CObjectBase* pTargetObject = GetMission()->GetStateTarget();
	CDynamicCast<NWorld::CUnit> pUnit(pTargetObject);
	if (pUnit)
	{
		if ( pUnit->GetPlayer() != GetMission()->GetActivePlayer()->GetPlayer() )
			return false;

		NWorld::SItem sTarget;
		sTarget.eType = NWorld::SItem::UNIT_ANYPLACE;
		sTarget.pUnit = pUnit;
		GetMission()->Command( pUnit, new NWorld::CCmdMoveInventoryItem( NWorld::SItem( sInfo.pUnit, NWorld::SItem::HAND ), sTarget ) );
	}
	else
	{
		NWorld::SItem sTarget;
		sTarget.eType = NWorld::SItem::GROUND;
		sTarget.pUnit = sInfo.pUnit;
		GetMission()->Command( sInfo.pUnit, new NWorld::CCmdMoveInventoryItem( NWorld::SItem( sInfo.pUnit, NWorld::SItem::HAND ), sTarget ) );
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateUntrap
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateUntrap::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetSelectedUnits( &unitsSet );
	if ( unitsSet.size() != 1 )
		return false;

	if ( unitsSet[0]->GetUnit()->GetState() != NWorld::CUnit::ST_NORMAL_TOOL )
		return false;

	SActionInfo sGeneralInfo;
	pMission->GetActionInfo( UA_USE, &sGeneralInfo );
	if ( ( GetType() == FORCED ) && ( !sGeneralInfo.bOk || !sGeneralInfo.bEnoughAP ) )
	{
		ShowError( GetMission(), sGeneralInfo.eResult );
		return false;
	}

	SActionInfo sInfo;
	CPtr<NWorld::CCmd> pCmd = GetTargetCmd();
	if ( !IsValid( pCmd ) )
		return false;

	GetMission()->CanDoCommand( pCmd, false, &sInfo );
	if ( !sInfo.bAvailable )
		return false;

	WCHAR wsBuffer[1024] = L"";
	if ( !GetMission()->IsRealTime() )
		swprintf( wsBuffer, L"AP: %d", sInfo.nActionAP );
	sCursorInfo = NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_HEAL ), wsBuffer );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateUntrap::OnLButtonUp( int nX, int nY )
{
	SActionInfo sInfo;
	CObj<NWorld::CCmd> pCmd = GetTargetCmd();
	if ( !IsValid( pCmd ) )
		return true;

	GetMission()->CanDoCommand( pCmd, false, &sInfo );
	if ( !sInfo.bAvailable || !sInfo.bOk )
	{
		ShowError( GetMission(), sInfo.eResult );
		// retail CStateUntrap barks nothing, success or failure
		return false;
	}

	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetSelectedUnits( &unitsSet );

	for ( vector< CPtr<NGame::IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
		GetMission()->Command( (*iTemp)->GetUnit(), pCmd );

	if ( GetType() == FORCED )
		GetMission()->ResetState();

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::SCursorInfo CStateUntrap::GetCursorInfo() const
{
	return sCursorInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CCmd* CStateUntrap::GetTargetCmd()
{
	CObjectBase* pObject = GetMission()->GetStateTarget();
	if ( !IsValid( pObject ) )
		return 0;

	list< CPtr<CObjectBase> > trappedObjects;
	GetMission()->GetActivePlayer()->GetPlayer()->GetTrappedObjectsList( &trappedObjects );
	if ( !IsInSet( trappedObjects, pObject ) )
		return 0;

	return new NWorld::CCmdUntrapObject( pObject );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// TEMPORARY STATES
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateSelection
////////////////////////////////////////////////////////////////////////////////////////////////////
CStateSelection::CStateSelection( const CVec2 &_vAnchor ): 
	CStateBase( true ), vAnchor( _vAnchor )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateSelection::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	GetMission()->FreezeCamera( true );
	CVec2 vScreenRect = GetMission()->GetScene()->GetScreenRect();
	NUI::SPoint sPoint( vAnchor.x * 1024 / vScreenRect.x, vAnchor.y * 768 / vScreenRect.y );
	pSelection = new NUI::CSelectionWindow( NUI::SWindowInfo( GetMission()->GetDesktop()->GetClientWindow(), sPoint, NUI::SPoint( 0, 0 ), "selection", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_TRANSPARENT | NUI::STYLE_BOTTOMMOST ), this );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateSelection::Terminate()
{
	GetMission()->FreezeCamera( false );
	pSelection = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateSelection::Step()
{
	CStateBase::Step();

	CVec2 vCursorPos = GetMission()->GetCursor()->GetPos();
	CVec2 vScreenRect = GetMission()->GetScene()->GetScreenRect();

	NUI::SRect sRect( vAnchor.x * 1024 / vScreenRect.x, vAnchor.y * 768 / vScreenRect.y, vCursorPos.x * 1024 / vScreenRect.x, vCursorPos.y * 768 / vScreenRect.y );
	pSelection->SetSize( NUI::SPoint( abs( sRect.Width() ), abs( sRect.Height() ) ) );
	pSelection->SetPosition( NUI::SPoint( Min( sRect.x1, sRect.x2 ), Min( sRect.y1, sRect.y2 ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateSelection::Cancel()
{
	GetMission()->ResetState();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateSelection::Handle()
{
	bool bKeepSelection = false;
	CVec2 vCursorPos = GetMission()->GetCursor()->GetPos();
	CVec2 vScreenRect = GetMission()->GetScene()->GetScreenRect();
	CTransformStack sTS = GetMission()->GetCameraTransform();

	NUI::SRect sRect( Min( vAnchor.x, vCursorPos.x ), Min( vAnchor.y, vCursorPos.y ), Max( vAnchor.x, vCursorPos.x ), Max( vAnchor.y, vCursorPos.y ) );

	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetUnits( &unitsSet );
	for ( vector< CPtr<NGame::IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
	{
		CVec2 vScreenPos;
		if ( !TestRayInFrustrum( (*iTemp)->GetUnit()->GetPosition().GetCP(), &sTS, vScreenRect, &vScreenPos ) )
			continue;
		if ( ( vScreenPos.x < sRect.x1 ) || ( vScreenPos.x > sRect.x2 ) || ( vScreenPos.y < sRect.y1 ) || ( vScreenPos.y > sRect.y2 ) )
			continue;

		GetMission()->Select( (*iTemp)->GetUnit(), bKeepSelection );
		bKeepSelection = true;
	}

	GetMission()->ResetState();
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateUnloadItem
////////////////////////////////////////////////////////////////////////////////////////////////////
CStateUnloadItem::CStateUnloadItem():
	bindCancel( "cancel" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateUnloadItem::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateUnloadItem::ProcessEvent( const NInput::SEvent &sEvent )
{
	if ( bindCancel.ProcessEvent( sEvent ) )
	{
		GetMission()->ResetState();
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateUnloadItem::OnLButtonUp( int nX, int nY )
{
	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() != 1 )
		return true;

	CPtr<NWorld::CCmd> pCmd = GetTargetCmd();
	if ( !IsValid( pCmd ) )
		return true;

	GetMission()->Command( unitsSet.front()->GetUnit(), pCmd );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CCmd* CStateUnloadItem::GetTargetCmd()
{
	CObjectBase* pObject = GetMission()->GetStateTarget();
	if ( !IsValid( pObject ) )
		return 0;
	CDynamicCast<NRPG::IWeaponItemInfo> pItem(pObject);
	if (pItem)
		return new NWorld::CCmdUnloadWeapon( pItem );

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::SCursorInfo CStateUnloadItem::GetCursorInfo() const
{
	return NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_UNLOAD ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// FORCED STATES
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateRotate
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateRotate::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	SActionInfo sGeneralInfo;
	pMission->GetActionInfo( UA_LOOK, &sGeneralInfo );
	if ( ( GetType() == FORCED ) && ( !sGeneralInfo.bOk || !sGeneralInfo.bEnoughAP ) )
	{
		ShowError( GetMission(), sGeneralInfo.eResult );
		return false;
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateRotate::OnLButtonUp( int nX, int nY )
{
	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetSelectedUnits( &unitsSet );

	NAI::SPosition pos;
	if ( !GetMission()->GetTracePosition( &pos ) )
		return true;

	for ( vector< CPtr<NGame::IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
	{
		NAI::EDirection eDir = GetMission()->GetWorld()->GetPathNetwork()->GetClosestDir( (*iTemp)->GetUnit()->GetPosition().pos.p, pos.p );
		NAI::SPosition sPos = (*iTemp)->GetUnit()->GetPosition().pos;
		sPos.p.SetDirection( eDir );
		GetMission()->Command( (*iTemp)->GetUnit(), new NWorld::CCmdLook( sPos ) );
	}

	SayAckForAll( GetMission(), NWorld::IA_CONFIRMATION );	// retail CStateRotate::OnLButtonUp @0x1daf70

	GetMission()->ResetState();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::SCursorInfo CStateRotate::GetCursorInfo() const
{
	return NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_ROTATE ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateSetTrap
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateSetTrap::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	SActionInfo sGeneralInfo;
	pMission->GetActionInfo( UA_ATTACK, &sGeneralInfo );
	if ( ( GetType() == FORCED ) && ( !sGeneralInfo.bOk || !sGeneralInfo.bEnoughAP ) )
	{
		ShowError( GetMission(), sGeneralInfo.eResult );
		return false;
	}

	SActionInfo sInfo;
	CPtr<NWorld::CCmd> pCmd = GetTargetCmd();
	if ( IsValid( pCmd ) )
		GetMission()->CanDoCommand( pCmd, false, &sInfo );


	if ( sInfo.bAvailable )
	{
		WCHAR wsBuffer[1024] = L"";
		if ( !GetMission()->IsRealTime() )
			swprintf( wsBuffer, L"AP: %d", sInfo.nActionAP );
		sCursorInfo = NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_HEAL ), wsBuffer );
	}
	else
		sCursorInfo = NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_BLOCK ) );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateSetTrap::OnLButtonUp( int nX, int nY )
{
	SActionInfo sInfo;
	CObj<NWorld::CCmd> pCmd = GetTargetCmd();
	if ( !IsValid( pCmd ) )
		return true;

	GetMission()->CanDoCommand( pCmd, false, &sInfo );
	if ( !sInfo.bAvailable || !sInfo.bOk )
	{
		ShowError( GetMission(), sInfo.eResult );
		// retail CStateSetTrap barks nothing on failure
		return false;
	}

	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetSelectedUnits( &unitsSet );

	for ( vector< CPtr<NGame::IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
		GetMission()->Command( (*iTemp)->GetUnit(), pCmd );

	SayAckForAll( GetMission(), NWorld::IA_CONFIRMATION );	// retail: set-trap is a go-there order

	if ( GetType() == FORCED )
		GetMission()->ResetState();

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::SCursorInfo CStateSetTrap::GetCursorInfo() const
{
	return sCursorInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CCmd* CStateSetTrap::GetTargetCmd()
{
	CObjectBase* pObject = GetMission()->GetStateTarget();
	if ( !IsValid( pObject ) )
		return 0;

	return new NWorld::CCmdSetGrenadeOnObject( pObject );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateSetMine
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateSetMine::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	SActionInfo sGeneralInfo;
	pMission->GetActionInfo( UA_MINE, &sGeneralInfo );
	if ( ( GetType() == FORCED ) && ( !sGeneralInfo.bOk || !sGeneralInfo.bEnoughAP ) )
	{
		ShowError( GetMission(), sGeneralInfo.eResult );
		return false;
	}

	UpdateCursor();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateSetMine::OnLButtonUp( int nX, int nY )
{
	SActionInfo sInfo;
	CObj<NWorld::CCmd> pCmd = GetTargetCmd();
	if ( !IsValid( pCmd ) )
		return true;

	GetMission()->CanDoCommand( pCmd, false, &sInfo );
	if ( !sInfo.bAvailable || !sInfo.bOk )
	{
		ShowError( GetMission(), sInfo.eResult );
		// retail CStateSetMine @0x1db3a0 barks nothing on failure
		return false;
	}

	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetSelectedUnits( &unitsSet );

	for ( vector< CPtr<NGame::IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
		GetMission()->Command( (*iTemp)->GetUnit(), pCmd );

	SayAckForAll( GetMission(), NWorld::IA_CONFIRMATION );	// retail: set-mine is a go-there order

	if ( GetType() == FORCED )
		GetMission()->ResetState();

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::SCursorInfo CStateSetMine::GetCursorInfo() const
{
	return sCursorInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CCmd* CStateSetMine::GetTargetCmd()
{
	NAI::SPosition pos;
	if ( !GetMission()->GetTracePosition( &pos ) )
		return 0;

	return new NWorld::CCmdSetMineOnTile( pos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateSetMine::UpdateCursor()
{
	SActionInfo sInfo;
	NAI::SPosition pos;
	if ( GetMission()->GetTracePosition( &pos ) )
	{
		if ( pos == sLastPosition )
			return;

		sLastPosition = pos;
		CPtr<NWorld::CCmd> pCmd = GetTargetCmd();
		if ( IsValid( pCmd ) )
			GetMission()->CanDoCommand( pCmd, false, &sInfo );
	}

	if ( sInfo.bAvailable )
	{
		WCHAR wsBuffer[1024] = L"";
		if ( !GetMission()->IsRealTime() )
			swprintf( wsBuffer, L"AP: %d", sInfo.nActionAP );
		sCursorInfo = NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_HEAL ), wsBuffer );
	}
	else
		sCursorInfo = NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_BLOCK ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateSetMine::Step()
{
	CStateBase::Step();
	UpdateCursor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateFirstAid
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateFirstAid::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	SActionInfo sGeneralInfo;
	pMission->GetActionInfo( UA_HEAL, &sGeneralInfo );
	if ( ( GetType() == FORCED ) && ( !sGeneralInfo.bOk || !sGeneralInfo.bEnoughAP ) )
	{
		ShowError( GetMission(), sGeneralInfo.eResult );
		return false;
	}

	SActionInfo sInfo;
	CPtr<NWorld::CCmd> pCmd = GetTargetCmd();
	if ( IsValid( pCmd ) )
		GetMission()->CanDoCommand( pCmd, false, &sInfo );


	if ( sInfo.bAvailable )
	{
		WCHAR wsBuffer[1024] = L"";
		if ( !GetMission()->IsRealTime() )
			swprintf( wsBuffer, L"AP: %d", sInfo.nActionAP );
		sCursorInfo = NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_HEAL ), wsBuffer );
	}
	else
		sCursorInfo = NUI::SCursorInfo( NDb::GetUITexture( N_CURSOR_BLOCK ) );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateFirstAid::OnLButtonUp( int nX, int nY )
{
	SActionInfo sInfo;
	CObj<NWorld::CCmd> pCmd = GetTargetCmd();
	if ( !IsValid( pCmd ) )
		return true;

	GetMission()->CanDoCommand( pCmd, false, &sInfo );
	if ( !sInfo.bAvailable || !sInfo.bOk )
	{
		ShowError( GetMission(), sInfo.eResult );
		// retail CStateFirstAid @0x1db550 barks nothing on failure
		return false;
	}

	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetSelectedUnits( &unitsSet );

	for ( vector< CPtr<NGame::IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
		GetMission()->Command( (*iTemp)->GetUnit(), pCmd );

	SayAckForAll( GetMission(), NWorld::IA_CONFIRMATION );	// retail: first-aid is a go-there order

	if ( GetType() == FORCED )
		GetMission()->ResetState();

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::SCursorInfo CStateFirstAid::GetCursorInfo() const
{
	return sCursorInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CCmd* CStateFirstAid::GetTargetCmd()
{
	CObjectBase* pObject = GetMission()->GetStateTarget();
	if ( !IsValid( pObject ) )
		return 0;

	NWorld::CUnit* pUnit = dynamic_cast<NWorld::CUnit*>( pObject );
	if ( !IsValid( pUnit ) )
		return 0;

	if ( pUnit->GetPlayer() != GetMission()->GetActivePlayer()->GetPlayer() )
		return 0;

	return new NWorld::CCmdHeal( pUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// INSTANT
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateEmpty
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateEmpty::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStatePose
////////////////////////////////////////////////////////////////////////////////////////////////////
CStatePose::CStatePose( NAI::EPose _ePose ):
	ePose( _ePose )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStatePose::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	EUnitAction eAction;
	switch( ePose )
	{
	case NAI::RUN:
		eAction = UA_POSERUN;
		break;
	case NAI::WALK:
		eAction = UA_POSEWALK;
		break;
	case NAI::CROUCH:
		eAction = UA_POSECROUCH;
		break;
	case NAI::CRAWL:
		eAction = UA_POSECRAWL;
		break;
	default:
		eAction = UA_POSEWALK;
		ASSERT( 0 );
	}

	SActionInfo sInfo;
	GetMission()->GetActionInfo( eAction, &sInfo );
	if ( sInfo.eResult != NWorld::UCR_OK )
		return false;

	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );
	for ( vector< CPtr<NGame::IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
	{
		NAI::SUnitPosition uPos = (*iTemp)->GetUnit()->GetPosition();
		uPos.SetPose( ePose );
		pMission->Command( (*iTemp)->GetUnit(), new NWorld::CCmdWishPose( ePose ) );
		pMission->Command( (*iTemp)->GetUnit(), new NWorld::CCmdPath( uPos.pos, NAI::PF_USE_POSEDIR ) );
			//GetMission()->Command( new NWorld::CCmdGo( (*iTemp)->GetUnit() ) );
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateDropCorpse
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateDropCorpse::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	SActionInfo sInfo;
	GetMission()->GetActionInfo( UA_DROPCORPSE, &sInfo );
	if ( sInfo.eResult != NWorld::UCR_OK )
		return false;

	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetMission()->GetSelectedUnits( &unitsSet );
	for ( vector< CPtr<NGame::IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
	{
		if ( (*iTemp)->GetUnit()->IsCarryingCorpse() )
			GetMission()->Command( (*iTemp)->GetUnit(), new NWorld::CCmdDropCorpse );
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStateMoveItem
////////////////////////////////////////////////////////////////////////////////////////////////////
CStateMoveItem::CStateMoveItem( NWorld::CUnit *_pUnit, const NWorld::SItem &_sSource, const NWorld::SItem &_sTarget ):
	pUnit( _pUnit ), sSource( _sSource ), sTarget( _sTarget )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateMoveItem::Initialize( IMission *pMission )
{
	CStateBase::Initialize( pMission );

	GetMission()->Command( pUnit, new NWorld::CCmdMoveInventoryItem( sSource, sTarget ) );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB1007110, CStateTeam )
REGISTER_SAVELOAD_CLASS( 0xB1007111, CStateMove )
REGISTER_SAVELOAD_CLASS( 0xB1007112, CStateAttack )
REGISTER_SAVELOAD_CLASS( 0xB1007113, CStateUse )
REGISTER_SAVELOAD_CLASS( 0xB1007114, CStatePickItem )
REGISTER_SAVELOAD_CLASS( 0xB1007115, CStateDragItem )
REGISTER_SAVELOAD_CLASS( 0xB1007116, CStateSelection )
REGISTER_SAVELOAD_CLASS( 0xB1007117, CStateUnloadItem )
REGISTER_SAVELOAD_CLASS( 0xB1007118, CStateRotate )
REGISTER_SAVELOAD_CLASS( 0xB1007119, CStateFirstAid )
REGISTER_SAVELOAD_CLASS( 0xB100711A, CStateEmpty )
REGISTER_SAVELOAD_CLASS( 0xB100711B, CStatePose )
REGISTER_SAVELOAD_CLASS( 0xB100711C, CStateMoveItem )
REGISTER_SAVELOAD_CLASS( 0xB100711D, CStateWait )
REGISTER_SAVELOAD_CLASS( 0xB100711E, CStateDropCorpse )
REGISTER_SAVELOAD_CLASS( 0xB100711F, CStateSetMine )
REGISTER_SAVELOAD_CLASS( 0xB1007120, CStateUntrap )
REGISTER_SAVELOAD_CLASS( 0xB1007121, CStateFriend )