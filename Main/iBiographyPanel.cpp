#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "wInterface.h"
#include "RPGUnit.h"
#include "RPGPerk.h"
#include "RPGUnitInfo.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataPerk.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "iMission.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iBiographyPanel.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
CBiographyPanel::CBiographyPanel( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBiographyPanel::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pClose = new CHoverButton( sEvent.pLoader->GetControl( "biography" ) );
			pClose->AddImageState( 1, NDb::GetUITexture( 437 ) );
			pClose->AddImageState( 0, NDb::GetUITexture( 437 ) );

			pPerks = new CHoverFlashButton( sEvent.pLoader->GetControl( "perks" ) );
			pPerks->AddImageState( 1, NDb::GetUITexture( 430 ) );
			pPerks->AddImageState( 0, NDb::GetUITexture( 430 ) );
			pPerks->AddImageState( 2, NDb::GetUITexture( 948 ) );

			pMedals = new CHoverButton( sEvent.pLoader->GetControl( "medals" ) );
			pMedals->AddImageState( 1, NDb::GetUITexture( 400 ) );
			pMedals->AddImageState( 0, NDb::GetUITexture( 400 ) );
			pMedals->AddImageState( 2, NDb::GetUITexture( 418 ) );

			pCharacter = new CHoverButton( sEvent.pLoader->GetControl( "character" ) );
			pCharacter->AddImageState( 1, NDb::GetUITexture( 383 ) );
			pCharacter->AddImageState( 0, NDb::GetUITexture( 383 ) );

			pDescriptionView = new CScrollWindow<CMLText>( sEvent.pLoader->GetControl( "view" ) );
			pDescription = pDescriptionView->GetClientWindow();
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pDescriptionView->SetVScroll( GetUIWindow<CScroll>( this, "scroll" ) );
			break;
		}
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( SCursorInfo() );
			break;
		}
	}

	// Tail "consumed" predicate (release @0x1a51a0): base result OR'd with membership of the mouse-button
	// event range (EVENT_FLAG_ACTIVE|EVENT_FLAG_HITTEST | 0x31..0x36 == LBUTTONUP..RBUTTONDBLCLK).
	if ( CWindow::ProcessMessage( sEvent ) || ( sEvent.nEvent > 0x06000030 && sEvent.nEvent < 0x06000037 ) )
		return true;

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBiographyPanel::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	// Single-selection gate: refresh the body only when exactly one unit is selected AND its tracked
	// unit differs from the one already shown.
	if ( unitsSet.size() == 1 )
	{
		NWorld::CUnit *pTracked = unitsSet[0]->GetUnit();
		if ( pUnit != pTracked )
		{
			pUnit = pTracked;

			SPoint sRealSize;
			pDescription->SetText( GetDBString( 0x4A73 ) + GetDBString( pUnit->GetRPG()->GetRPGUnit()->GetBiography() ), true );
			pDescription->GetRealSize( &sRealSize );
			pDescription->SetSize( sRealSize );
		}
	}

	// Perk-flash: light the perks button when the tracked unit has an unspent perk point.
	if ( IsValid( pUnit ) )
	{
		NRPG::CPerksTree *pPerksTree = pUnit->GetRPG()->GetRPGUnit()->GetPerksTree();
		if ( IsValid( pPerksTree ) )
			pPerks->SetShowFlash( pPerksTree->GetPerkPoints() != 0 );
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB3218140, CBiographyPanel );
