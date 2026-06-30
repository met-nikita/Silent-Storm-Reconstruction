#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "rpgUnitInfo.h"
#include "RPGUnit.h"        // NRPG::CUnit complete type (GetRPGUnit()->GetVoice() for the in-game ack voice)
#include "wInterface.h"
#include "wUICommands.h"
#include "Sound.h"
#include "Interface.h"
#include "iMission.h"
#include "iCommonUI.h"
#include "iMissionUI.h"
#include "iMissionMovieUI.h"
#include "iMissionExec.h"
#include "..\Misc\StrProc.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataAck.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
const int
	N_FADE_STAGE_TIME	= 1000;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMissionMovieUI
////////////////////////////////////////////////////////////////////////////////////////////////////
CMissionMovieUI::CMissionMovieUI():
	bindCancel( "cancel" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMissionMovieUI::CMissionMovieUI( const SWindowInfo &sInfo, NGame::IMission *_pMission, CDesktopWindow *_pTransition ):
	CDesktopWindow( sInfo ), pMission( _pMission ), pTransition( _pTransition ),
	eStage( START ), sStageTime( 0 ), bindCancel( "cancel" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionMovieUI::ShowDesktop()
{
	eStage = START;

	ShowWindow( SWTYPE_SHOW );
	pMission->PushDesktop( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionMovieUI::HideDesktop()
{
	eStage = FINISH;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionMovieUI::UpdateDesktop( const STime &sTime )
{
	switch( eStage )
	{
	case START:
		{
			eStage = FADEIN;
			sStageTime = sTime;
			nPanelsStateSave = pMission->GetPanelState( NGame::PANEL_ALL );
			pMission->SetPanelState( NGame::PANEL_ALL, false );
			pMission->SetCheatVisibility( true );
		}
	case FADEIN:
		{
			if ( sStageTime + N_FADE_STAGE_TIME > sTime )
			{
				float fCoeff = float( sTime - sStageTime ) / N_FADE_STAGE_TIME;
				pTopBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF * fCoeff ) );
				pBottomBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF * fCoeff ) );
				break;
			}

			eStage = SHOWSCRIPT;
			sStageTime = sTime;
			pTopBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF ) );
			pBottomBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF ) );
			pTransition->SetStyle( STYLE_VISIBLE, false );
			break;
		}
	case FINISH:
		{
			eStage = FADEOUT;
			sStageTime = sTime;
			pTransition->SetStyle( STYLE_VISIBLE, true );
		}
	case FADEOUT:
		{
			if ( sStageTime + N_FADE_STAGE_TIME > sTime )
			{
				float fCoeff = 1.0f - float( sTime - sStageTime ) / N_FADE_STAGE_TIME;
				pTopBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF * fCoeff ) );
				pBottomBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF * fCoeff ) );
				break;
			}

			pMission->SetCheatVisibility( false );
			pMission->SetPanelState( nPanelsStateSave, true );
			pMission->PopDesktop( this );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGame::CUICmdExec* CMissionMovieUI::CreateExecutor( NWorld::CUICmd *pCmd )
{
	CDynamicCast<NWorld::CUICmdTurn> pTurn(pCmd);
	if (pTurn)
		return false;
	CDynamicCast<NWorld::CUICmdUnit> pUnit(pCmd);
	if (pUnit)
		return false;

	return NGame::CreateExecutor( pCmd, pMission );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionMovieUI::ProcessEvent( const NInput::SEvent &sEvent )
{
	if ( bindCancel.ProcessEvent( sEvent ) )
	{
		pMission->SetWaitForPartFinished( true );
		return true;
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionMovieUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( SCursorInfo() );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pTopBackground = GetUIWindow<CImage>( this, "top_background" );
			pBottomBackground = GetUIWindow<CImage>( this, "bottom_background" );
			break;
		}
	}

	return CDesktopWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAckEvent* CMissionMovieUI::PlayAckEvent( const STime &sTime, NWorld::CAckEvent *pEvent )
{
	if ( IsValid( pEvent->pAckInfo ) && IsValid( pEvent->pUnit ) )
	{
		// The UNIT's chosen voice (NRPG::CUnit::nVoice via GetRPGUnit()->GetVoice()), not the persona's preset, so
		// the player's FaceGen voice choice is heard in movie/cutscene acks (retail uses GetRPGUnit()'s nVoice).
		const NDb::SAckVoice &voice = pEvent->pAckInfo->GetVoice( pEvent->pUnit->GetRPG()->GetRPGUnit()->GetVoice() );
		PlaySound( voice.pSound );
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMissionFadeUI
////////////////////////////////////////////////////////////////////////////////////////////////////
CMissionFadeUI::CMissionFadeUI()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMissionFadeUI::CMissionFadeUI( const SWindowInfo &sInfo, NGame::IMission *_pMission, CDesktopWindow *_pTransition,
	const CVec3 &_vColor, STime _sFadeTime ):
	CDesktopWindow( sInfo ), pMission( _pMission ), pTransition( _pTransition ),
	vColor( _vColor ), sFadeTime( _sFadeTime ), eStage( START ), sStageTime( 0 ), nNotifyID( 0 )
{
	// The full-window colour overlay whose alpha the state machine ramps in/out. It is also tagged "view"
	// and registered as the desktop's client window, because the mission's render loop (CMission::Step)
	// reads GetDesktop()->GetClientWindow()->GetSize() for the world view rect -- a desktop with no client
	// window (we load no template) would null-deref there. CDesktopWindow sets pClientWindow from the
	// "view" child on EVENT_TEMPLATELOADCOMPLETE, so we fire that once now.
	pFade = new CImage( SWindowInfo( this, SPoint( 0, 0 ), GetSize(), "view", STYLE_ENABLED | STYLE_VISIBLE | STYLE_TRANSPARENT ) );
	CDesktopWindow::ProcessMessage( SEvent( EVENT_TEMPLATELOADCOMPLETE ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static NGfx::SPixel8888 FadePixel( const CVec3 &c, int nAlpha )
{
	return NGfx::SPixel8888( ( int )( c.x * 255.0f ), ( int )( c.y * 255.0f ), ( int )( c.z * 255.0f ), nAlpha );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionFadeUI::ShowDesktop( int _nNotifyID )
{
	eStage = START;
	nNotifyID = _nNotifyID;

	ShowWindow( SWTYPE_SHOW );
	pMission->PushDesktop( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionFadeUI::HideDesktop( int _nNotifyID )
{
	eStage = FINISH;
	nNotifyID = _nNotifyID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionFadeUI::UpdateDesktop( const STime &sTime )
{
	switch( eStage )
	{
	case START:
		{
			eStage = FADEIN;
			sStageTime = sTime;
		}
	case FADEIN:
		{
			if ( sStageTime + sFadeTime > sTime )
			{
				float fCoeff = float( sTime - sStageTime ) / sFadeTime;
				pFade->SetColor( FadePixel( vColor, ( int )( 0xFF * fCoeff ) ) );
				break;
			}

			eStage = DUMMY;
			sStageTime = sTime;
			pFade->SetColor( FadePixel( vColor, 0xFF ) );		// fully tinted
			pTransition->SetStyle( STYLE_VISIBLE, false );
			pMission->Command( new NWorld::CCmdInterfaceEvent( nNotifyID ) );	// unblock WaitForUI(id)
			break;
		}
	case DUMMY:
		break;
	case FINISH:
		{
			eStage = FADEOUT;
			sStageTime = sTime;
			pTransition->SetStyle( STYLE_VISIBLE, true );
		}
	case FADEOUT:
		{
			if ( sStageTime + sFadeTime > sTime )
			{
				float fCoeff = 1.0f - float( sTime - sStageTime ) / sFadeTime;
				pFade->SetColor( FadePixel( vColor, ( int )( 0xFF * fCoeff ) ) );
				break;
			}

			pMission->Command( new NWorld::CCmdInterfaceEvent( nNotifyID ) );	// unblock WaitForUI(id)
			pMission->PopDesktop( this );
			break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Mirror CMissionMovieUI::CreateExecutor: while the fade screen is the top desktop, world UI commands
// route through it, so it must build executors (camera moves etc.) -- otherwise a CameraSet/CameraMove
// issued between FadeOut and FadeIn is consumed with no executor and its WaitForUI id never releases.
NGame::CUICmdExec* CMissionFadeUI::CreateExecutor( NWorld::CUICmd *pCmd )
{
	CDynamicCast<NWorld::CUICmdTurn> pTurn(pCmd);
	if (pTurn)
		return 0;
	CDynamicCast<NWorld::CUICmdUnit> pUnit(pCmd);
	if (pUnit)
		return 0;

	return NGame::CreateExecutor( pCmd, pMission );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionFadeUI::ProcessMessage( const SEvent &sEvent )
{
	if ( sEvent.nEvent == EVENT_MOUSEMOVE )
		GetInterface()->SetCursorInfo( SCursorInfo() );		// blank cursor while the fade owns the screen

	return CDesktopWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB1212160, CMissionMovieUI );
REGISTER_SAVELOAD_CLASS( 0xB3122170, CMissionFadeUI );		// LUA convergence PART B (release id)
