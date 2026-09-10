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
#include "iCommonUI.h"			// NUI::CSlotInfo -- the drag-drop slot target descriptor (CStateDragItem::GetTargetCmd @0x1dc960)
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
	// v1.2 unified team/select: TEAM and TEAM_HILIGHT took the old UnitTracker SELECT/HILIGHT
	// greens (v1.2 .data @0x97bb74/@0x97bb84; the v1.1 values were 0.1,0.1,1,1 / 1,1,1,0.5)
	V_SELECTIONCOLOR_TEAM					= CVec4( 0.0431f, 0.2823f, 0, 1 ),
	V_SELECTIONCOLOR_TEAM_HILIGHT	= CVec4( 0.0215f, 0.1411f, 0, 1 ),
	V_SELECTIONCOLOR_ENEMY				= CVec4( 1, 0.1f, 0.1f, 0.5f ),
	V_SELECTIONCOLOR_CORPSE				= CVec4( 0.1f, 1, 0.1f, 0.5f ),
	V_SELECTIONCOLOR_OBJECT				= CVec4( 0.1f, 1, 0.1f, 0.5f ),
	V_SELECTIONCOLOR_NEUTRAL			= CVec4( 0.1f, 1, 1, 0.5f ),
	// v1.2-only alternative-palette colors (@0x97bbd4/@0x97bbe4/@0x97bbf4)
	V_SELECTIONCOLOR_ALT_HILIGHT	= CVec4( 1, 1, 1, 0.05f ),
	V_SELECTIONCOLOR_SINGLE				= CVec4( 1, 1, 1, 0.3f ),
	V_SELECTIONCOLOR_ALT_OBJECT		= CVec4( 0.0431f, 0.2823f, 0, 1 );
////////////////////////////////////////////////////////////////////////////////////////////////////
// v1.2 selection palette dispatcher @0x5d6130: game_selectionmode remaps the palette.
// Index: 0 team/selected, 1 team-hilight, 2 enemy, 3 corpse, 4 object, 5 neutral.
const CVec4& GetSelectionColor( int nIndex )
{
	if ( NRender::nSelectionMode != 2 )
	{
		if ( NRender::nSelectionMode == 1 )
		{
			switch ( nIndex )
			{
				case 1: return V_SELECTIONCOLOR_ALT_HILIGHT;
				case 2: return V_SELECTIONCOLOR_ENEMY;
				case 3:
				case 4: return V_SELECTIONCOLOR_ALT_OBJECT;
				case 5: return V_SELECTIONCOLOR_NEUTRAL;
			}
		}
		else
		{
			switch ( nIndex )
			{
				case 0: return V_SELECTIONCOLOR_TEAM;
				case 1: return V_SELECTIONCOLOR_TEAM_HILIGHT;
				case 2: return V_SELECTIONCOLOR_ENEMY;
				case 3: return V_SELECTIONCOLOR_CORPSE;
				case 4: return V_SELECTIONCOLOR_OBJECT;
				case 5: return V_SELECTIONCOLOR_NEUTRAL;
			}
		}
	}
	return V_SELECTIONCOLOR_SINGLE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NGame::SayAckForAll @0x1d96d0: an ORDER handler pairs its ShowError/success with a
// per-selected-unit NWorld::CCmdPlayAck dispatched on the mission EVENTS channel
// (mission vtbl+0x2c DoEvent -> commander events -> Segment drain -> ExecuteCommand -> CGlobalAck). It must
// NOT use Command(unit, cmd): that wraps the ack in CCmdSetCommand -> CUnitServer::Do, which
// CANCELS the unit's running executor (the "orders die after one step" regression).
// Retail success barks (IA_CONFIRMATION) exist ONLY for orders that move the unit somewhere:
// move/rotate/use/set-trap/set-mine/first-aid. Attack/pick-item/untrap confirm NOTHING.
static void SayAckForAll( IMission *pMission, NWorld::EInterfaceAcks eAck )
{
	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );
	for ( vector< CPtr<NGame::IUnitTracker> >::iterator i = unitsSet.begin(); i != unitsSet.end(); ++i )
		pMission->DoEvent( new NWorld::CCmdPlayAck( (*i)->GetUnit(), eAck ) );
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
// retail NGame::MakeCursorString @0x1d7990: the shared cursor AP caption. Appends
//   <format/colour DB string> -- 20276 (GREEN) when the action is AP-affordable (sInfo.bEnoughAP),
//                                19807 (RED) otherwise; appended in real time too (it re-issues the
//                                <font> markup the ToHit line relies on),
// then, in TURN-BASED only (mission vtbl+0x50 == false):
//   <"AP: " 19808> + the folded AP -- "%d" when nMinAP == nMaxAP, "%d-%d" for a mixed selection,
//                                     or <"N/A" 19810> when no unit reported an AP (nMaxAP == -1).
// Used by every tactical state that shows an AP cursor caption (attack/use/pick/drag/untrap/
// set-trap/set-mine/first-aid).
static void MakeCursorString( IMission *pMission, const SActionInfo &sInfo, wstring *pRes )
{
	*pRes += NUI::GetDBString( sInfo.bEnoughAP ? 20276 : 19807 );	// AP line: green if affordable, else red

	if ( !pMission->IsRealTime() )
	{
		*pRes += NUI::GetDBString( 19808 );					// "AP: "
		if ( sInfo.nMaxAP >= 0 )
		{
			WCHAR wsAP[64];
			if ( sInfo.nMinAP != sInfo.nMaxAP )
				swprintf( wsAP, L"%d-%d", sInfo.nMinAP, sInfo.nMaxAP );
			else
				swprintf( wsAP, L"%d", sInfo.nMinAP );
			*pRes += wsAP;
		}
		else
			*pRes += NUI::GetDBString( 19810 );				// "N/A"
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NGame::MakeUnitStateToolTip @0x1d7b10 (defined with the CStateFriend leg below); used by
// CStateTeam/CStateFriend/CStateUse/CStateAttack for the "enemyToolTip" hover frame.
static void MakeUnitStateToolTip( IMission *pMission, NWorld::CUnit *pUnit, NUI::CTextFrame *pFrame );
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
	// v1.2 0x5d7d60: GameStep explicitly installs this state while not ready.
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::SCursorInfo CStateWait::GetCursorInfo() const
{
	return NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_BUSY ) );
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

			// retail CStateTeam::Initialize @0x1d97f0: build the "enemyToolTip" CTextFrame on the
			// mission desktop's client window, then fill it from the hovered unit.
			pUnitToolTip = new NUI::CTextFrame( NUI::SWindowInfo( GetMission()->GetDesktop()->GetClientWindow(),
				NUI::SPoint( 0, 0 ), NUI::SPoint( 0, 0 ), "enemyToolTip", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_TRANSPARENT | NUI::STYLE_TOPMOST ) );
			UpdateToolTipInfo();
			return true;
		}
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CStateTeam::UpdateToolTipInfo @0x1d8230: refill the hover tooltip from the state target
// (mission vtbl+0x94) when it is a live unit; a non-unit / dead hover leaves the frame untouched.
void CStateTeam::UpdateToolTipInfo()
{
	CDynamicCast<NWorld::CUnit> pUnit( GetMission()->GetStateTarget() );
	if ( IsValid( pUnit ) && IsValid( pUnitToolTip ) )
		MakeUnitStateToolTip( GetMission(), pUnit, pUnitToolTip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateTeam::Terminate()
{
	CStateBase::Terminate();
	// retail CStateTeam::Terminate @0x1d6c00: release the tooltip frame FIRST, then un-highlight
	pUnitToolTip = 0;
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
		pTraceSelection = GetMission()->GetRenderGame()->Select( pObject, GetSelectionColor( 0 ) );	// v1.2 @0x5d9c00: palette(team)
	else
		pTraceSelection = GetMission()->GetRenderGame()->Select( pObject, GetSelectionColor( 5 ) );	// v1.2 @0x5d9c2a: palette(neutral)

	if ( !pUnit->CanTalk() )
		sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_NORMAL ) );
	else
		sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_TALK ) );	// retail @0x1d90c0: UICursors row 20 "talk" (Talk.cur), not open/close

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

	CObjectBase* pObject = GetMission()->GetStateTarget();
	if ( !IsValid( pObject ) )
		return false;

	CDynamicCast<NWorld::CUnit> pUnit( pObject );
	if ( !IsValid( pUnit ) )
		return true;

	// retail CStateFriend::OnLButtonUp @0x1d9b70: probe whether a hero can talk to the target. On
	// UCR_OK issue CCmdTalk to every selected unit (start the dialog); on UCR_NOT_HERO (0x13) issue
	// CCmdNotHeroWantsToTalk to each instead -- that fires the CAckNPCInteraction bark rather than talk.
	SActionInfo sInfo;
	CObj<NWorld::CCmd> pProbe = new NWorld::CCmdTalk( pUnit );
	GetMission()->CanDoCommand( pProbe, false, &sInfo );
	if ( sInfo.eResult == NWorld::UCR_OK )
	{
		for ( vector< CPtr<NGame::IUnitTracker> >::iterator i = unitsSet.begin(); i != unitsSet.end(); ++i )
			GetMission()->Command( (*i)->GetUnit(), new NWorld::CCmdTalk( pUnit ) );
	}
	else if ( sInfo.eResult == NWorld::UCR_NOT_HERO )
	{
		for ( vector< CPtr<NGame::IUnitTracker> >::iterator i = unitsSet.begin(); i != unitsSet.end(); ++i )
			GetMission()->Command( (*i)->GetUnit(), new NWorld::CCmdNotHeroWantsToTalk() );
	}

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
	// retail @0x1dd170 drops the drag anchor FIRST -- without this, a click whose move FAILS (no
	// state change happens) left bAnchorSet armed, so the next mouse move entered the drag-select
	// band (CStateSelection) with the button already up.
	bAnchorSet = false;

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

	// retail @0x1dbb10 result aggregation: first result seeds the aggregate, any DIFFERING later
	// result degrades it to UCR_GENERAL_FAILURE; bark IA_CONFIRMATION only on uniform UCR_OK,
	// anything else routes through ShowError ("Path not found" for an unreachable click).
	// (dev's UCR_OK is 0, unlike retail's 1 -- seed with an explicit first-result flag, not 0)
	NWorld::EUnitCommandResult eAgg = NWorld::UCR_OK;
	bool bFirst = true;
	for ( int nUnit = 0; nUnit < unitsSet.size(); ++nUnit )
	{
		NWorld::EUnitCommandResult eResult = unitsSet[nUnit]->SetTargetPosition( unitPlaces[nUnit], bInstant );
		if ( bFirst )
		{
			eAgg = eResult;
			bFirst = false;
		}
		else if ( eAgg != eResult )
			eAgg = NWorld::UCR_GENERAL_FAILURE;
	}
	if ( eAgg == NWorld::UCR_OK )
		SayAckForAll( GetMission(), NWorld::IA_CONFIRMATION );	// retail: "order acknowledged" on a move order
	else
		ShowError( GetMission(), eAgg );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateMove::UpdateCursor()
{
	NAI::SPosition pos;
	if ( !GetMission()->GetTracePosition( &pos ) )
	{
		sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_BLOCK ) );
		return;
	}

	NAI::SPathPlace p( pos.p );
	p.SetPose( NAI::CM_CROUCH );
	if ( GetMission()->GetWorld()->GetPathNetwork()->IsNativePassable( p ) )
		sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_MOVE ) );
	else
		sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_BLOCK ) );
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
	UpdateInfo();
	UpdateCursorInfo();
	UpdateCursor();

	// retail CStateAttack::Initialize @0x1dd240: the "enemyToolTip" CTextFrame over the aimed enemy
	// (only the non-FORCED aim-at-unit state has a real unit target).
	if ( GetType() != FORCED )
	{
		pEnemyToolTip = new NUI::CTextFrame( NUI::SWindowInfo( GetMission()->GetDesktop()->GetClientWindow(),
			NUI::SPoint( 0, 0 ), NUI::SPoint( 0, 0 ), "enemyToolTip", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_TRANSPARENT | NUI::STYLE_TOPMOST ) );
		UpdateEnemyStateInfo();
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CStateAttack::UpdateEnemyStateInfo @0x1d8540 (PDB name): refill from the aimed
// (state-target) unit.
void CStateAttack::UpdateEnemyStateInfo()
{
	CDynamicCast<NWorld::CUnit> pUnit( GetMission()->GetStateTarget() );
	if ( IsValid( pUnit ) && IsValid( pEnemyToolTip ) )
		MakeUnitStateToolTip( GetMission(), pUnit, pEnemyToolTip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateAttack::Terminate()
{
	CStateBase::Terminate();
	// retail CStateAttack::Terminate @0x1d6f00 releases pEnemyToolTip BEFORE pTraceSelection
	pEnemyToolTip = 0;
	pTraceSelection = 0;
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

	// retail CStateAttack::UpdateCursor @0x1dc0b0 keys the cursor on the cached sInfo: an
	// UCR_OK_RELOAD result shows the reload cursor (26), !bOk shows the block cursor, else the
	// per-hit-location cursor. (The dev enum carries no UCR_OK_RELOAD -- its CanDo never emits it --
	// so only the !bOk fork is portable; in the retail eResult switch bOk=true implies
	// bAvailable=true, so this equals the old !bActionUnavailable gate exactly.)
	if ( sInfo.bOk )
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

				sCursorInfo.pCursor = NDb::GetUICursor( nDefault );
			}
			break;
		case NAI::HL_HEAD:
			sCursorInfo.pCursor = NDb::GetUICursor( N_CURSOR_ATTACK_HEAD );
			break;
		case NAI::HL_BODY:
			sCursorInfo.pCursor = NDb::GetUICursor( N_CURSOR_ATTACK_BODY );
			break;
		case NAI::HL_LHAND:
			sCursorInfo.pCursor = NDb::GetUICursor( N_CURSOR_ATTACK_LARM );
			break;
		case NAI::HL_RHAND:
			sCursorInfo.pCursor = NDb::GetUICursor( N_CURSOR_ATTACK_RARM );
			break;
		case NAI::HL_LLEG:
			sCursorInfo.pCursor = NDb::GetUICursor( N_CURSOR_ATTACK_LLEG );
			break;
		case NAI::HL_RLEG:
			sCursorInfo.pCursor = NDb::GetUICursor( N_CURSOR_ATTACK_RLEG );
			break;
		default:
			ASSERT( 0 );
		}
	}
	else
		sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_BLOCK ) );
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
					else
					{
						// retail @0x1da200 routes EVERY CCmdShootObject (swung melee weapon OR BARE FISTS)
						// through the object GetToHit unconditionally -- GetCompositeToHit reads the firing
						// unit's own weapon class (GetToHitType -> GetMeleeWeaponItem, which returns the
						// default fists weapon when the hand is empty => TH_MELEE), so an unarmed punch is
						// scored exactly like a knife swing. The old `if (pMelee)` cast tested the INVENTORY
						// active item, which is null for bare fists (nothing equipped), so no calc ran and
						// nToHit stayed 0.
						nToHit = pWorld->GetGame()->GetCompositeToHit((*iTemp)->GetUnit(), pUnit, eHitLocation, pWorld->IsFirstTurn());
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
					else
					{
						// retail UpdateCursorInfo @0x1da200 routes EVERY non-AoE weapon at a tile target
						// through the same NRPG::GetToHit tile overload (via the CCmdShootTile target
						// command) -- the throwing-vs-swinging-vs-UNARMED dispatch happens inside
						// RPGUnitGetTileToHit @0x2b4df0 (TH_THROWING -> knife calcer, TH_MELEE -> 100 when
						// the cover walk connects). Bare fists have NO inventory active item, so the old
						// `if (pMelee)` cast was null and a punch aimed at ground/walls/objects showed 0%;
						// GetTileCompositeToHit derives TH_MELEE from the unit's default fists weapon and
						// casts covers from GetMeleeAttackPos, matching retail's TH_MELEE tile rule.
						nToHit = pWorld->GetGame()->GetTileCompositeToHit((*iTemp)->GetUnit(), pos.GetCP(), NAI::THL_MIDDLE, pWorld->IsFirstTurn());
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

			// BUG 8: the retail cursor caption is built ENTIRELY from DB strings, in retail's order (retail
			// MakeCursorString @0x1d7990 then CStateAttack::UpdateCursorInfo @0x1da200 append):
			//   <format 19807/20276>  then (turn-based only)  <"AP: " 19808> <AP range | "N/A" 19810>  then
			//   <"<br>ToHit: " 19809> <ToHit %>.
			// The DB strings carry BOTH the localized labels AND the font/colour markup. There are TWO full
			// format strings, differing only in <color>: 19807 = RED (0xFFEA511C), 20276 = GREEN (0xFF9BD315).
			// The AP line is green iff the action is affordable (sInfo.bEnoughAP), the ToHit line is green iff
			// the hit chance is non-zero (`cmp nToHit,2; jge green`). The AP part is the shared
			// MakeCursorString over the state's cached sInfo (nMinAP/nMaxAP fold across the selection).
			wstring wsText;
			MakeCursorString( GetMission(), sInfo, &wsText );
			wsText += NUI::GetDBString( nMin >= 2 ? 20276 : 19807 );       // ToHit line: green if hit chance > 0, else red
			wsText += NUI::GetDBString( 19809 );                   // "<br>ToHit: " (localized label)
			WCHAR wsToHit[32];
			if ( unitsSet.size() == 1 )
				swprintf( wsToHit, L"%d%%", nMin );
			else
				swprintf( wsToHit, L"%d-%d%%", nMin, nMax );
			wsText += wsToHit;
			sCursorInfo.wsText = wsText;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CStateAttack::UpdateInfo @0x1da140: reset the cached feasibility to its constructed
// defaults, then refill it from CanDoCommand when the current target command is valid.
void CStateAttack::UpdateInfo()
{
	sInfo = SActionInfo();

	CObj<NWorld::CCmd> pCmd = GetTargetCmd();
	if ( !IsValid( pCmd ) )
		return;

	GetMission()->CanDoCommand( pCmd, false, &sInfo );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateAttack::UpdateTraceSelection()
{
	pTraceSelection = 0;

	CObjectBase* pObject = GetMission()->GetStateTarget();
	if ( !IsValid( pObject ) )
		return;

	if ( pObject )
		pTraceSelection = GetMission()->GetRenderGame()->Select( pObject, GetSelectionColor( 2 ) );	// v1.2 @0x5d79ab: palette(enemy)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateAttack::Step()
{
	CStateBase::Step();

	// retail CStateAttack::Step @0x1dc540: the FORCED state refreshes feasibility, caption AND the
	// enemy tooltip each frame; the non-forced hover state only re-picks the cursor (it is
	// re-Initialized whenever the trace target changes).
	if ( GetType() == FORCED )
	{
		UpdateInfo();
		UpdateCursorInfo();
		UpdateEnemyStateInfo();
	}

	UpdateCursor();
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

	bool bFrom3DWorld = false;
	CObjectBase* pTargetObject = GetMission()->GetStateTarget( &bFrom3DWorld );
	if ( !IsValid( pTargetObject ) || !bFrom3DWorld )
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
	CVec4 vHilightColor( GetSelectionColor( 4 ) );	// v1.2 @0x5d9076: palette(object)
	CDynamicCast<NWorld::CUnit> pDeadUnit(pObject);
	if (pDeadUnit)
	{
		bRet = pDeadUnit->IsDead() || pDeadUnit->IsUnconscious();
		vHilightColor = GetSelectionColor( 3 );	// v1.2 @0x5d90bf: palette(corpse)
		nCursorID = N_CURSOR_USE;			// carry body -> Use.cur (hand): retail default branch, id 21

		// retail CStateUse::Initialize @0x1d85a0: an UNCONSCIOUS unit under the cursor gets the
		// "enemyToolTip" name+VP frame (built BEFORE the bRet gate, disasm order); released in
		// Terminate (@0x1d6fc0).
		if ( pDeadUnit->IsUnconscious() )
		{
			pUnitToolTip = new NUI::CTextFrame( NUI::SWindowInfo( GetMission()->GetDesktop()->GetClientWindow(),
				NUI::SPoint( 0, 0 ), NUI::SPoint( 0, 0 ), "enemyToolTip", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_TRANSPARENT | NUI::STYLE_TOPMOST ) );
			MakeUnitStateToolTip( GetMission(), pDeadUnit, pUnitToolTip );
		}
	}
	else {
		CDynamicCast<NWorld::IObject> pTempObject(pObject);
		if (pTempObject)
		{
			vHilightColor = GetSelectionColor( 4 );	// v1.2 @0x5d9221: palette(object)
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
		// retail @0x1d85a0: the caption is the shared MakeCursorString (@0x1d7990) AP line
		wstring wsText;
		MakeCursorString( GetMission(), sInfo, &wsText );
		sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( nCursorID ), wsText.c_str() );
	}
	else
		sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_BLOCK ) );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateUse::Terminate()
{
	CStateBase::Terminate();
	// retail CStateUse::Terminate @0x1d6fc0: release the tooltip frame first, then the selection
	pUnitToolTip = 0;
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

		// v1.2 @0x5dd044: palette(object); bIgnoreFloorMask=true in BOTH retail links (v1.1 decomp
		// @0x1dc570 `push 1`, v1.2 disasm @0x5dd03d) -- the dev used the default false
		pTraceSelection = GetMission()->GetRenderGame()->Select( pObject, GetSelectionColor( 4 ), true );

		// retail CStatePickItem::Initialize @0x1dc570: cache the cursor from the current move
		// command's feasibility -- bOk -> pickitem cursor (GetUICursor(0x12) @0x5dc6e8) with the
		// MakeCursorString AP caption; !bOk -> block cursor (GetUICursor(4) @0x5dc73f) with L"".
		// A null command leaves the previous cached cursor untouched (retail returns true there).
		CObj<NWorld::CCmd> pCmd = GetTargetCmd();
		if ( !pCmd )
			return true;
		if ( IsValid( pCmd ) )
		{
			SActionInfo sInfo;
			GetMission()->CanDoCommand( pCmd, false, &sInfo );
			if ( sInfo.bOk )
			{
				wstring wsText;
				MakeCursorString( GetMission(), sInfo, &wsText );
				sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_PICKITEM ), wsText.c_str() );
			}
			else
				sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_BLOCK ) );
		}
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
		// retail CStatePickItem::OnLButtonUp @0x5dc911 (cmp eResult,0x12 -> IA literal 3): a full
		// inventory barks IA_NO_PLACE_IN_INVENTORY (drives CGlobalAck::OnNoPlaceInInventory).
		if ( sInfo.eResult == NWorld::UCR_INVENTORY_NO_PLACE )
			SayAckForAll( GetMission(), NWorld::IA_NO_PLACE_IN_INVENTORY );
		// TODO: retail also barks IA_NO_PLACE_IN_INVENTORY from the CInfoPanelSlot inventory producers
		// @0x258540 (secondary path -- not ported here).
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
	return sCursorInfo;	// retail @0x1d6770: return the CACHED cursor (filled by Initialize @0x1dc570)
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
			// Retail CStatePickItem::GetTargetCmd uses UNIT_ANYPLACE when the inventory is
			// closed (v1.1 RVA 0x1da892, v1.2 @0x5db2b2). MoveInventoryItem then tries the
			// two hand slots before falling back to the backpack.
			sTarget.eType = NWorld::SItem::UNIT_ANYPLACE;
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

	NWorld::SItem sInfo;
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
		// retail @0x1dd570: SHMatrix into CModel::SetCameraTransform (no CFBTransform side channel)
		SHMatrix res;
		MakeMatrix( &res, sCamera.fPitch, sCamera.fYaw, sCamera.fRoll, vCP );

		pModel->SetModel( pRPGItem->pModel->CreateModel( &sRnd ) );
		pModel->SetCameraTransform( res );
	}

	// retail CStateDragItem::Initialize @0x1dd570 tail: prime the cached cursor (UpdateCursor
	// @0x1dccf0), then snap the icon to the cursor (UpdatePosition @0x1d5d60). Step() is exactly
	// that pair (retail Step @0x1dd940 == UpdateCursor + UpdatePosition).
	Step();

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateDragItem::Terminate()
{
	pModel = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CStateDragItem::GetTargetCmd(bool bAllowSlot) @0x1dc960: build the "drop the in-hand item
// onto whatever is under the cursor" move command. Decoded flow:
//   (1) GetActivePlayer()->GetPlayer()->GetInHandItem(&src) -- nothing in hand -> null;
//   (2) srcUnit = GetSourceUnit() (@0x1daa20: the in-hand item's live owner) -- dead/null -> null;
//   (3) target = GetStateTarget();
//       dyncast CUnit    -> same player as active? {UNIT_ANYPLACE, unit} : null;
//       dyncast CSlotInfo (ONLY when bAllowSlot; the slot leg is published by the CSlot
//                          CActionDecorator windows -- see iCommonUI.h CSlotInfo):
//           pre-set sTarget.pUnit = info->GetUnit() when live (SLOT/BACKPACK carry the unit);
//           SLOT(1)     -> {SLOT, nSlot = info->GetSlot(), pUnit}
//           STORAGE(2)  -> {STORAGE, sPosition = (-1,-1), pPlayer = srcUnit->GetPlayer()}
//           BACKPACK(3) -> {BACKPACK, pUnit}, gated on the backpack grid FindPlace(item, &sPosition)
//           other kind / !bAllowSlot -> null;
//       neither cast     -> {GROUND};
//   (4) new CCmdMoveInventoryItem( SItem{HAND, srcUnit}, sTarget ).
NWorld::CCmd* CStateDragItem::GetTargetCmd( bool bAllowSlot )
{
	NWorld::SItem sInfo;
	if ( !GetMission()->GetActivePlayer()->GetPlayer()->GetInHandItem( &sInfo ) )
		return 0;
	if ( !IsValid( sInfo.pUnit ) )		// retail: the drag's source unit must be live (@0x1daa20)
		return 0;

	CObjectBase* pTargetObject = GetMission()->GetStateTarget();
	NWorld::SItem sTarget;
	CDynamicCast<NWorld::CUnit> pUnit(pTargetObject);
	if (pUnit)
	{
		if ( pUnit->GetPlayer() != GetMission()->GetActivePlayer()->GetPlayer() )
			return 0;					// retail: a foreign unit under the cursor -> no command
		sTarget.eType = NWorld::SItem::UNIT_ANYPLACE;
		sTarget.pUnit = pUnit;
	}
	else
	{
		CDynamicCast<NUI::CSlotInfo> pSlotInfo(pTargetObject);
		if (pSlotInfo)
		{
			if ( !bAllowSlot )
				return 0;				// peek-only caller never commits a slot drop

			// retail @0x5dcbd6: the slot's owning unit is adopted only when live
			if ( IsValid( pSlotInfo->GetUnit() ) )
				sTarget.pUnit = pSlotInfo->GetUnit();

			switch ( pSlotInfo->GetPlacement() )
			{
			case NUI::CSlotInfo::SLOT:
				sTarget.eType = NWorld::SItem::SLOT;
				sTarget.nSlot = pSlotInfo->GetSlot();
				break;
			case NUI::CSlotInfo::STORAGE:
				sTarget.eType = NWorld::SItem::STORAGE;
				sTarget.sPosition = CTPoint<int>( -1, -1 );
				sTarget.pPlayer = sInfo.pUnit->GetPlayer();		// retail: the store of the dragging unit's player
				break;
			case NUI::CSlotInfo::BACKPACK:
				sTarget.eType = NWorld::SItem::BACKPACK;
				// retail: gate the drop on the backpack grid having a place for the item
				if ( !sTarget.pUnit || !sTarget.pUnit->GetRPG()->GetInventoryInfo()->FindPlace( sInfo.pItem, &sTarget.sPosition ) )
					return 0;
				break;
			default:
				return 0;				// VACUUM / unknown placement -> no command
			}
		}
		else
		{
			sTarget.eType = NWorld::SItem::GROUND;
			sTarget.pUnit = sInfo.pUnit;
		}
	}

	return new NWorld::CCmdMoveInventoryItem( NWorld::SItem( sInfo.pUnit, NWorld::SItem::HAND ), sTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CStateDragItem::UpdateCursor @0x1dccf0: probe the drop command, then cache the cursor --
// bOk -> the "normal" cursor (GetUICursor(2) @0x5dcdbb) with the MakeCursorString AP caption when
// the drop costs AP (nMaxAP > 0); !bOk -> the block cursor (GetUICursor(4) @0x5dce0c) with L"".
void CStateDragItem::UpdateCursor()
{
	SActionInfo sInfo;
	CObj<NWorld::CCmd> pCmd = GetTargetCmd( true );	// retail @0x5dcd11: the cursor peek allows slot targets
	if ( IsValid( pCmd ) )
		GetMission()->CanDoCommand( pCmd, false, &sInfo );

	if ( sInfo.bOk )
	{
		wstring wsText;
		if ( sInfo.nMaxAP > 0 )
			MakeCursorString( GetMission(), sInfo, &wsText );
		sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_NORMAL ), wsText.c_str() );
	}
	else
		sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_BLOCK ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateDragItem::Step()
{
	// retail CStateDragItem::Step @0x1dd940: refresh the cursor art, then reposition the icon
	UpdateCursor();

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
NUI::SCursorInfo CStateDragItem::GetCursorInfo() const
{
	return sCursorInfo;	// retail @0x1d6790: return the cached drag cursor
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateDragItem::OnLButtonDown( int nX, int nY )
{
	// retail CStateDragItem::OnLButtonDown @0x1dd950: fetch the drop command WITHOUT the slot leg
	// (GetTargetCmd(false) -- a click over an inventory slot falls through to the slot window's own
	// drop handler); when it is live, dispatch it on the drag's SOURCE unit (the in-hand item's
	// owner, GetSourceUnit @0x1daa20) -- NOT on the unit under the cursor.
	CObj<NWorld::CCmd> pCmd = GetTargetCmd( false );
	if ( !IsValid( pCmd ) )
		return false;

	NWorld::SItem sInfo;
	if ( !GetMission()->GetActivePlayer()->GetPlayer()->GetInHandItem( &sInfo ) )
		return false;

	GetMission()->Command( sInfo.pUnit, pCmd );
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

	// retail caption: the shared MakeCursorString (@0x1d7990) AP line
	wstring wsText;
	MakeCursorString( GetMission(), sInfo, &wsText );
	sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_HEAL ), wsText.c_str() );
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

	// retail @0x1d73b0: remember WHICH camera the mission selector returned (CObj pLockedCamera,
	// serialized tag 4) and bump ITS scroll-lock count (ICamera vtbl+0x70 = CBaseCamera::Lock
	// @0xcffa0) -- rubber-band drag-select mutes camera input without any pose pin.
	pLockedCamera = GetMission()->GetCamera();
	pLockedCamera->SetLock( true );
	CVec2 vScreenRect = GetMission()->GetScene()->GetScreenRect();
	NUI::SPoint sPoint( vAnchor.x * 1024 / vScreenRect.x, vAnchor.y * 768 / vScreenRect.y );
	pSelection = new NUI::CSelectionWindow( NUI::SWindowInfo( GetMission()->GetDesktop()->GetClientWindow(), sPoint, NUI::SPoint( 0, 0 ), "selection", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_TRANSPARENT | NUI::STYLE_BOTTOMMOST ), this );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateSelection::Terminate()
{
	// retail @0x1d75c0: unlock exactly the camera Initialize locked (NOT whatever the selector
	// returns now -- the active camera may have changed since).
	pLockedCamera->SetLock( false );
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
// v1.2 @0x5e01a0 -- consume both press variants. Otherwise the backpack handles the same press
// after the item model and replaces this state with its ordinary take/move-item state before release.
bool CStateUnloadItem::OnLButtonDown( int nX, int nY )
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStateUnloadItem::OnLButtonDblClk( int nX, int nY )
{
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
	return NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_UNLOAD ) );
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
	return NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_ROTATE ) );
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

	// retail CStateSetTrap::Initialize @0x1d9590 tail: prime the cursor through the shared
	// sLastPosition-gated refresh (the state has a per-frame Step/UpdateCursor like CStateSetMine).
	UpdateCursor();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CStateSetTrap::UpdateCursor (Step @0x1d8cc0 inlines it; mirrors CStateSetMine::UpdateCursor
// @0x1d8cd0): rebuild the cached cursor only when the traced position moved (sLastPosition).
void CStateSetTrap::UpdateCursor()
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
		wstring wsText;
		MakeCursorString( GetMission(), sInfo, &wsText );	// retail caption: shared MakeCursorString @0x1d7990
		sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_HEAL ), wsText.c_str() );
	}
	else
		sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_BLOCK ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStateSetTrap::Step()
{
	// retail CStateSetTrap::Step @0x1d8cc0: per-frame cursor refresh (position-gated)
	CStateBase::Step();
	UpdateCursor();
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
		wstring wsText;
		MakeCursorString( GetMission(), sInfo, &wsText );	// retail caption: shared MakeCursorString @0x1d7990
		sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_HEAL ), wsText.c_str() );
	}
	else
		sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_BLOCK ) );
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
		wstring wsText;
		MakeCursorString( GetMission(), sInfo, &wsText );	// retail caption: shared MakeCursorString @0x1d7990
		sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_HEAL ), wsText.c_str() );
	}
	else
		sCursorInfo = NUI::SCursorInfo( NDb::GetUICursor( N_CURSOR_BLOCK ) );

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
		// retail @0x1db700 anchors the re-path on GetSetPosePosition (CUnit vtbl+0x48), NOT GetPosition:
		// a moving unit re-paths from the tile it is stepping INTO, not backwards to the one it left.
		NAI::SUnitPosition uPos = (*iTemp)->GetUnit()->GetSetPosePosition();
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
