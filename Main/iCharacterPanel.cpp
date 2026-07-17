#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "wInterface.h"
#include "RPGUnitInfo.h"
#include "RPGUnit.h"        // NRPG::CUnit complete type (GetRPGUnit()->GetName() for the fullname header)
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Sound.h"
#include "iMission.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iCriticalIcons.h"
#include "iCharacterPanel.h"
#include "..\Misc\StrProc.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x1b0810: the character panel carries 12 wound icons ("critical_0".."critical_11")
const int N_NUM_CRITICALS_ICONS = 12;
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
	case EVENT_TEMPLATELOAD:
		{
			// release @0x1b0810: the four tab buttons are BUILT here (new CHoverButton /
			// CHoverFlashButton over the loader control), each with its retail image states
			// (disasm-recovered ids; state order per button matches the retail store order):
			//   character  {HOVER:437, NORMAL:437}
			//   perks      {HOVER:430, NORMAL:430, DISABLED:948}
			//   medals     {HOVER:400, NORMAL:400, DISABLED:418}
			//   biography  {HOVER:381, NORMAL:381, DISABLED:429}
			// The Jan03 "background" CButton (tex 429) is GONE in retail -- 429 is really the
			// biography DISABLED art ('Biographi Disabled'); there is no backdrop fetch at all
			// (the panel artwork is the template's own unnamed images, tex 361/362).
			pClose = new CHoverButton( sEvent.pLoader->GetControl( "character" ) );
			pClose->AddImageState( CHoverButton::STATE_HOVER, NDb::GetUITexture( 437 ) );
			pClose->AddImageState( CHoverButton::STATE_NORMAL, NDb::GetUITexture( 437 ) );

			pPerks = new CHoverFlashButton( sEvent.pLoader->GetControl( "perks" ) );
			pPerks->AddImageState( CHoverButton::STATE_HOVER, NDb::GetUITexture( 430 ) );
			pPerks->AddImageState( CHoverButton::STATE_NORMAL, NDb::GetUITexture( 430 ) );
			// retail adds AddImageState( STATE_DISABLED, GetUITexture( 948 ) ) here; texture 948
			// ('Perks Disabled') does not exist in the dev content DB yet -- skipped until the
			// asset lands (a null texture would just draw an empty state anyway).

			pMedals = new CHoverButton( sEvent.pLoader->GetControl( "medals" ) );
			pMedals->AddImageState( CHoverButton::STATE_HOVER, NDb::GetUITexture( 400 ) );
			pMedals->AddImageState( CHoverButton::STATE_NORMAL, NDb::GetUITexture( 400 ) );
			pMedals->AddImageState( CHoverButton::STATE_DISABLED, NDb::GetUITexture( 418 ) );

			pBiography = new CHoverButton( sEvent.pLoader->GetControl( "biography" ) );
			pBiography->AddImageState( CHoverButton::STATE_HOVER, NDb::GetUITexture( 381 ) );
			pBiography->AddImageState( CHoverButton::STATE_NORMAL, NDb::GetUITexture( 381 ) );
			pBiography->AddImageState( CHoverButton::STATE_DISABLED, NDb::GetUITexture( 429 ) );

			// release @0x1b0810 (tail of EVENT_TEMPLATELOAD): the wound icons are created from
			// the template controls (they must be CInfoPanelCritical wrappers, not the loader's
			// plain image windows).
			criticalIconsSet.resize( N_NUM_CRITICALS_ICONS );
			for ( int nTemp = 0; nTemp < N_NUM_CRITICALS_ICONS; nTemp++ )
				criticalIconsSet[nTemp] = new CInfoPanelCritical( sEvent.pLoader->GetControl( NStr::Format( "critical_%d", nTemp ) ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			// release @0x1b0810 (EVENT_TEMPLATELOADCOMPLETE): resolve every named child --
			// level/class/fullname/level_bar, the 6 primary stats (text+bar+icon), then the
			// 10 skills (text+bar+icon). Retail fetches NO "background" control here.
			pLevel = GetUIWindow<CText>( this, "level" );
			pClass = GetUIWindow<CImage>( this, "class" );
			pFullName = GetUIWindow<CText>( this, "fullname" );
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

			// release @0x1b0810: the per-stat level-up indicator images ("<stat>_icon",
			// template tex 417 'SkillStat FAKE'; retail UpdateSkillUI swaps them per state)
			pEvasionIcon = GetUIWindow<CImage>( this, "evasion_icon" );
			pStrengthIcon = GetUIWindow<CImage>( this, "strength_icon" );
			pDexterityIcon = GetUIWindow<CImage>( this, "dexterity_icon" );
			pIntelligenceIcon = GetUIWindow<CImage>( this, "intelligence_icon" );
			pActionPointsIcon = GetUIWindow<CImage>( this, "ap_icon" );
			pVitalityPointsIcon = GetUIWindow<CImage>( this, "vp_icon" );

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

			// release @0x1b0810: the per-skill level-up indicator images
			pHideIcon = GetUIWindow<CImage>( this, "hide_icon" );
			pSpotIcon = GetUIWindow<CImage>( this, "spot_icon" );
			pBurstIcon = GetUIWindow<CImage>( this, "burst_icon" );
			pMeleeIcon = GetUIWindow<CImage>( this, "melee_icon" );
			pSnipeIcon = GetUIWindow<CImage>( this, "snipe_icon" );
			pMedicineIcon = GetUIWindow<CImage>( this, "medicine_icon" );
			pShootingIcon = GetUIWindow<CImage>( this, "shooting_icon" );
			pThrowingIcon = GetUIWindow<CImage>( this, "throwing_icon" );
			pInterruptIcon = GetUIWindow<CImage>( this, "interrupt_icon" );
			pEngineeringIcon = GetUIWindow<CImage>( this, "engineering_icon" );

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

	// retail @0x1b0420: medals/biography tabs grayed while first-mission (tutorial) mode (mission vtbl+0xf8)
	pMedals->SetStyle( STYLE_ENABLED, !pMission->IsSpecialFirstMissionMode() );
	pBiography->SetStyle( STYLE_ENABLED, !pMission->IsSpecialFirstMissionMode() );

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

	// release UpdateStats @0x1aff20 step (1): the full-name header above the level bar
	// (control "fullname", tag-4 member). Format follows the panel's own Courier/center style.
	NRPG::CUnit *pRPGUnit = pUnitInfo->GetRPGUnit();
	if ( IsValid( pRPGUnit ) )
	{
		swprintf( wsText, L"<font face=Courier size=16pt><center>%s", pRPGUnit->GetName().c_str() );
		pFullName->SetText( wsText );
	}

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

	// release @0x1b0420: refresh the wound/critical icon column every visible frame
	UpdateCriticalIcons( pUnit, criticalIconsSet );

	CWindow::Update( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0521171, CCharacterPanel );
