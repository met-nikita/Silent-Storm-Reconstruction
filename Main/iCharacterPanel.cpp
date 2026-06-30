#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "wInterface.h"
#include "RPGUnitInfo.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Sound.h"
#include "iMission.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iCharacterPanel.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
CCharacterPanel::CCharacterPanel( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCharacterPanel::ProcessMessage( const SEvent &sEvent )
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
			pClose = GetUIWindow<CButton>( this, "character" );
			pClose->AddImageState( 0, NDb::GetUITexture( 437 ) );
			pPerks = GetUIWindow<CButton>( this, "perks" );
			pPerks->AddImageState( 0, NDb::GetUITexture( 430 ) );
			pMedals = GetUIWindow<CButton>( this, "medals" );
			pMedals->SetStyle( STYLE_ENABLED, false );
			pMedals->AddImageState( 0, NDb::GetUITexture( 418 ) );
			pBackground = GetUIWindow<CButton>( this, "background" );
			pBackground->SetStyle( STYLE_ENABLED, false );
			pBackground->AddImageState( 0, NDb::GetUITexture( 429 ) );

			pLevel = GetUIWindow<CText>( this, "level" );
			pClass = GetUIWindow<CImage>( this, "class" );
			pLevelBar = GetUIWindow<CProgressBar>( this, "level_bar" );

			pEvasion = GetUIWindow<CText>( this, "evasion" );
			pStrength = GetUIWindow<CText>( this, "strength" );
			pDexterity = GetUIWindow<CText>( this, "dexterity" );
			pIntelligence = GetUIWindow<CText>( this, "intelligence" );
			pActionPoints = GetUIWindow<CText>( this, "ap" );
			pVitalityPoints = GetUIWindow<CText>( this, "vp" );
			pEvasionBar = GetUIWindow<CProgressBar>( this, "evasion_bar" );
			pStrengthBar = GetUIWindow<CProgressBar>( this, "strength_bar" );
			pDexterityBar = GetUIWindow<CProgressBar>( this, "dexterity_bar" );
			pIntelligenceBar = GetUIWindow<CProgressBar>( this, "intelligence_bar" );
			pActionPointsBar = GetUIWindow<CProgressBar>( this, "ap_bar" );
			pVitalityPointsBar = GetUIWindow<CProgressBar>( this, "vp_bar" );

			pHide = GetUIWindow<CText>( this, "hide" );
			pSpot = GetUIWindow<CText>( this, "spot" );
			pBurst = GetUIWindow<CText>( this, "burst" );
			pMelee = GetUIWindow<CText>( this, "melee" );
			pSnipe = GetUIWindow<CText>( this, "snipe" );
			pMedicine = GetUIWindow<CText>( this, "medicine" );
			pShooting = GetUIWindow<CText>( this, "shooting" );
			pThrowing = GetUIWindow<CText>( this, "throwing" );
			pInterrupt = GetUIWindow<CText>( this, "interrupt" );
			pEngineering = GetUIWindow<CText>( this, "engineering" );
			pHideBar = GetUIWindow<CProgressBar>( this, "hide_bar" );
			pSpotBar = GetUIWindow<CProgressBar>( this, "spot_bar" );
			pBurstBar = GetUIWindow<CProgressBar>( this, "burst_bar" );
			pMeleeBar = GetUIWindow<CProgressBar>( this, "melee_bar" );
			pSnipeBar = GetUIWindow<CProgressBar>( this, "snipe_bar" );
			pMedicineBar = GetUIWindow<CProgressBar>( this, "medicine_bar" );
			pShootingBar = GetUIWindow<CProgressBar>( this, "shooting_bar" );
			pThrowingBar = GetUIWindow<CProgressBar>( this, "throwing_bar" );
			pInterruptBar = GetUIWindow<CProgressBar>( this, "interrupt_bar" );
			pEngineeringBar = GetUIWindow<CProgressBar>( this, "engineering_bar" );

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
void CCharacterPanel::Update( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !GetStyle( STYLE_VISIBLE ) )
		return;

	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() != 1 )
	{
		SetStyle( STYLE_VISIBLE, false );
		return;
	}

	CPtr<NGame::IUnitTracker> pUnit = unitsSet[0];

	NRPG::SUnitInfo sUnitInfo;
	pUnit->GetUnit()->GetInfo( &sUnitInfo );

	CPtr<NRPG::IUnitMissionInfo> pUnitInfo = pUnit->GetUnit()->GetRPG();

	WCHAR wsText[256];

	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_LEVEL ) );
	pLevel->SetText( wsText );
	pLevelBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_LEVEL ) );
	if ( pUnitInfo->GetRPGPers()->pClass )
		pClass->SetImage( pUnitInfo->GetRPGPers()->pClass->pIcon );

	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_STR ) );
	pStrength->SetText( wsText );
	pStrengthBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_STR ) );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_DEX ) );
	pDexterity->SetText( wsText );
	pDexterityBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_DEX ) );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_INT ) );
	pIntelligence->SetText( wsText );
	pIntelligenceBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_INT ) );

	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_IC ) );
	pEvasion->SetText( wsText );
	pEvasionBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_IC ) );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_AP ) );
	pActionPoints->SetText( wsText );
	pActionPointsBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_AP ) );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_VP ) );
	pVitalityPoints->SetText( wsText );
	pVitalityPointsBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_VP ) );

	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_STEALTH ) );
	pHide->SetText( wsText );
	pHideBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_STEALTH ) );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_SPOT ) );
	pSpot->SetText( wsText );
	pSpotBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_SPOT ) );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_BURST ) );
	pBurst->SetText( wsText );
	pBurstBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_BURST ) );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_MELEE ) );
	pMelee->SetText( wsText );
	pMeleeBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_MELEE ) );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_SNIPE ) );
	pSnipe->SetText( wsText );
	pSnipeBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_SNIPE ) );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_MEDICINE ) );
	pMedicine->SetText( wsText );
	pMedicineBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_MEDICINE ) );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_SHOOTING ) );
	pShooting->SetText( wsText );
	pShootingBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_SHOOTING ) );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_THROWING ) );
	pThrowing->SetText( wsText );
	pThrowingBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_THROWING ) );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_INTERRUPT ) );
	pInterrupt->SetText( wsText );
	pInterruptBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_INTERRUPT ) );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", pUnitInfo->GetSkillMaxValue( NDb::ST_ENGINEERING ) );
	pEngineering->SetText( wsText );
	pEngineeringBar->SetValue( pUnitInfo->GetSkillProgress( NDb::ST_ENGINEERING ) );

	CWindow::Update( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0521171, CCharacterPanel );
