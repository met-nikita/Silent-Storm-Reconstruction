#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "rpgUnitInfo.h"
#include "wInterface.h"
#include "Sound.h"
#include "Interface.h"
#include "iMission.h"
#include "iCommonUI.h"
#include "iMissionUI.h"
#include "iMissionTrailerUI.h"
#include "..\Misc\StrProc.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataAck.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMissionTrailerUI
////////////////////////////////////////////////////////////////////////////////////////////////////
CMissionTrailerUI::CMissionTrailerUI():
	bindCancel( "hideinterface_trailer" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMissionTrailerUI::CMissionTrailerUI( const SWindowInfo &sInfo, NGame::IMission *_pMission, CDesktopWindow *_pTransition ):
	CDesktopWindow( sInfo ), pMission( _pMission ), pTransition( _pTransition ), bindCancel( "hideinterface_trailer" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionTrailerUI::ShowDesktop()
{
	ShowWindow( SWTYPE_SHOW );
	pMission->PushDesktop( this );

	pTransition->SetStyle( STYLE_VISIBLE, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionTrailerUI::HideDesktop()
{
	pTransition->SetStyle( STYLE_VISIBLE, true );

	pMission->PopDesktop( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionTrailerUI::ProcessEvent( const NInput::SEvent &sEvent )
{
	if ( bindCancel.ProcessEvent( sEvent ) )
	{
		HideDesktop();
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionTrailerUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( pMission->GetState()->GetCursorInfo() );
			break;
		}
	}

	bool bRet = CDesktopWindow::ProcessMessage( sEvent );

	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			CPtr<NGame::IState> pState = pMission->GetState();
			if ( ( pState->GetType() == NGame::IState::FORCED ) || ( pState->GetType() == NGame::IState::TEMPORARY ) )
			{
				SCursorInfo sStateCursor = pState->GetCursorInfo();
				if ( sStateCursor.pTexture != GetInterface()->GetCursorInfo().pTexture )
				{
					sStateCursor.wsText = L"";
					GetInterface()->SetCursorInfo( sStateCursor );
				}
			}
			break;
		}
	}

	if ( bRet )
		return true;

	return pMission->GetState()->ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB3114160, CMissionTrailerUI );
