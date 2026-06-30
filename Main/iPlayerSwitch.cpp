#include "StdAfx.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iMission.h"
#include "iPlayerSwitch.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release key-press event id. UIEvents.h names only EVENT_CHAR (base 0x20 | EVENT_FLAG_ACTIVE); the
// release build delivers the key-down this dialog reacts to as base id 0x22. ProcessMessage @0x22e5b0
// tests this exact value (0x02000022), so it is reproduced verbatim, tied to the real dev flag.
const int EVENT_PS_KEYPRESS = 0x00000022 | EVENT_FLAG_ACTIVE;
////////////////////////////////////////////////////////////////////////////////////////////////////
CPlayerSwitchUI::CPlayerSwitchUI( const SWindowInfo &sInfo, NGame::IMission *pMission ):
	CWindow( sInfo ), bPause( false ), pMission( pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerSwitchUI::OnOK()
{
	// Restore the pause state that was in effect before the banner appeared, then hide the banner.
	pMission->PauseGame( bPause );
	SetStyle( STYLE_VISIBLE, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlayerSwitchUI::Show()
{
	SetStyle( STYLE_VISIBLE, true );
	// Title = localized prefix (DB string 0x51F3) + the active player's name.
	pText->SetText( GetDBString( 0x51F3 ) + pMission->GetActivePlayer()->GetPlayer()->GetPlayerName() );
	// Remember the current pause state, then freeze the game while the banner is up.
	bPause = pMission->IsGamePaused();
	pMission->PauseGame( true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlayerSwitchUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pText = new CText( sEvent.pLoader->GetControl( "text" ) );

			pOk = new CHoverButton( sEvent.pLoader->GetControl( "ok" ) );
			pOk->AddTextState( CHoverButton::STATE_HOVER,  GetDBString( 0x41C7 ) + GetDBString( 0x2B88 ) );
			pOk->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 0x41C7 ) + GetDBString( 0x2B87 ) );
			break;
		}
	case EVENT_PS_KEYPRESS:
		// ORIGINAL BUG (confirmed @0x22e5b0): returns true for EVERY key, swallowing all input while up;
		// only Enter actually accepts.
		if ( sEvent.nVal == VK_RETURN )
			OnOK();
		return true;
	case EVENT_NOTIFY:
		// ORIGINAL BUG (confirmed @0x22e5b0): ANY notification is treated as the OK click (no szID test).
		OnOK();
		return true;
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NUI
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB3723130, CPlayerSwitchUI );
