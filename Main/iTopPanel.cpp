#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "wInterface.h"
#include "RPGUnitInfo.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "..\DBFormat\DataMap.h"		// NDb::EDiplomacyState / DS_ENEMY for the ally/enemy turn split
#include "Sound.h"
#include "iMission.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iTopPanel.h"
#include "..\Misc\StrProc.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_FLASH_TIME = 3000;
enum EEndMissionButtonState
{
	EENDMISSTATE_OK,
	EENDMISSTATE_WARNING,
	EENDMISSTATE_UNAVAILABLE
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAITurnProgressBar (retail @0x24cf60) -- one instance per AI side (ally + enemy); bAllies tags
// which side it represents, the parent CTopBar decides which one is visible.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAITurnProgressBar: public CWindow
{
	OBJECT_BASIC_METHODS(CAITurnProgressBar)
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	////
	bool bAllies;
	CPtr<CProgressBar> pBar;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&bAllies); f.Add(4,&pBar); return 0; }

public:
	CAITurnProgressBar() {}
	CAITurnProgressBar( const SWindowInfo &sInfo, NGame::IMission *pMission, bool bAllies );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAITurnProgressBar::CAITurnProgressBar( const SWindowInfo &sInfo, NGame::IMission *_pMission, bool _bAllies ):
	CWindow( sInfo ), pMission( _pMission ), bAllies( _bAllies )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAITurnProgressBar::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pBar = GetUIWindow<CProgressBar>( this, "bar" );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAITurnProgressBar::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !GetStyle( STYLE_VISIBLE ) )
		return;

	pBar->SetValue( 1.0f );

	if ( pMission->GetActivePlayer()->GetPlayer() == pMission->GetWorld()->GetCurrentPlayer() )
		return;

	int nValue = 0, nMaxValue = 0;
	CPtr<NWorld::IPlayer> pEnemyPlayer = pMission->GetWorld()->GetCurrentPlayer();
	vector< CPtr<NWorld::CUnit> > playerUnitsSet;
	pEnemyPlayer->GetUnits( &playerUnitsSet );
	for ( int nTemp = 0; nTemp < playerUnitsSet.size(); nTemp++ )
	{
		NRPG::SUnitInfo sInfo;
		playerUnitsSet[nTemp]->GetInfo( &sInfo );

		nValue += sInfo.nAP;
		nMaxValue += sInfo.nMaxAP;
	}

	pBar->SetValue( (float)( nMaxValue - nValue) / nMaxValue );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTopBar
////////////////////////////////////////////////////////////////////////////////////////////////////
CTopBar::CTopBar( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission ), eMode( NONE ), sModeTime( 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTopBar::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( SCursorInfo() );
			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pAllyTurn = new CAITurnProgressBar( sEvent.pLoader->GetControl( "allyturn" ), pMission, true );
			pEnemyTurn = new CAITurnProgressBar( sEvent.pLoader->GetControl( "enemyturn" ), pMission, false );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			// NOTE: retail has no "minimap" control here (confirmed absent from both retail iTopPanel.c
			// and the release gamedata); the old minimap fetch only emitted a UI-ERROR + dummy control.
			pEndMission = GetUIWindow<CButton>( this, "endmission" );
			pEndMission->AddImageState( EENDMISSTATE_OK, NDb::GetUITexture( 602 ) );
			pEndMission->AddImageState( EENDMISSTATE_WARNING, NDb::GetUITexture( 601 ) );
			pEndMission->AddImageState( EENDMISSTATE_UNAVAILABLE, NDb::GetUITexture( 600 ) );

			pText = GetUIWindow<CText>( this, "text" );
			pFlash = GetUIWindow<CWindow>( this, "flash" );
			pPlayerTurn = GetUIWindow<CWindow>( this, "playerturn" );
			break;
		}
	}

	if ( CWindow::ProcessMessage( sEvent ) )
		return true;

	switch( sEvent.nEvent )
	{
	case EVENT_LBUTTONUP:
	case EVENT_LBUTTONDOWN:
	case EVENT_LBUTTONDBLCLK:
	case EVENT_RBUTTONUP:
	case EVENT_RBUTTONDOWN:
	case EVENT_RBUTTONDBLCLK:
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTopBar::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	EMode ePrevMode = eMode;
	if ( pMission->IsRealTime() )
		eMode = REALTIME;
	else if ( pMission->GetActivePlayer()->GetPlayer() == pMission->GetWorld()->GetCurrentPlayer() )
		eMode = PLAYER_TURN;
	else if ( pMission->GetWorld()->GetDiplomacyState( pMission->GetActivePlayer()->GetPlayer(), pMission->GetWorld()->GetCurrentPlayer() ) == NDb::DS_ENEMY )
		eMode = ENEMY_TURN;
	else
		eMode = ALLY_TURN;	// retail @0x24d010: non-enemy AI side gets the separate "allyturn" bar

	pEndMission->SetStyle( STYLE_ENABLED, false );
	pEndMission->SetActiveState( EENDMISSTATE_UNAVAILABLE );
	if ( ( eMode == REALTIME ) || ( eMode == PLAYER_TURN ) )
	{
		pEndMission->SetStyle( STYLE_ENABLED, true );

		if ( pMission->GetActivePlayer()->IsPlayerWinner() )
			pEndMission->SetActiveState( EENDMISSTATE_OK );
		else if ( pMission->IsRealTime() )
			pEndMission->SetActiveState( EENDMISSTATE_WARNING );
	}

	if ( eMode != ePrevMode )
	{
		sModeTime = sTime;

		CPtr<NDb::CString> pString;
		if ( pMission->GetWorld()->IsInterrupt() )
			pString = NDb::GetString( 904 );
		else if ( ( eMode == ENEMY_TURN ) || ( eMode == ALLY_TURN ) )
			pString = NDb::GetString( 905 );
		else if ( eMode == PLAYER_TURN )
			pString = NDb::GetString( 903 );

		if ( IsValid( pString ) )
		{
			pText->SetText( pString->szStr );
			pText->SetStyle( STYLE_VISIBLE, true );
		}
		else
			pText->SetStyle( STYLE_VISIBLE, false );
	}

	// retail @0x24d010: reset all turn indicators, then light the one matching the current mode.
	pFlash->SetStyle( STYLE_VISIBLE, false );
	pAllyTurn->SetStyle( STYLE_VISIBLE, false );
	pEnemyTurn->SetStyle( STYLE_VISIBLE, false );
	pPlayerTurn->SetStyle( STYLE_VISIBLE, false );

	if ( pMission->GetWorld()->IsInterrupt() && ( ( sTime - sModeTime ) < N_FLASH_TIME ) )
		pFlash->SetStyle( STYLE_VISIBLE, true );
	else if ( ( eMode == REALTIME ) || ( eMode == PLAYER_TURN ) )
		pPlayerTurn->SetStyle( STYLE_VISIBLE, true );
	else if ( eMode == ALLY_TURN )
		pAllyTurn->SetStyle( STYLE_VISIBLE, true );
	else if ( eMode == ENEMY_TURN )
		pEnemyTurn->SetStyle( STYLE_VISIBLE, true );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0521163, CAITurnProgressBar );
REGISTER_SAVELOAD_CLASS( 0xB0521164, CTopBar );
