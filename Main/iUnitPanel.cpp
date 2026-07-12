#include "StdAfx.h"
#include "GSceneUtils.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "DiscretePos.h"
#include "wInterface.h"
#include "RPGItemInfo.h"
#include "RPGUnitInfo.h"
#include "RWGame.h"
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataSound.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "..\DBFormat\DataCamera.h"    // NDb::GetDBCamera -- the HUD face camera (release @0x254cc0: record 0x13a1)
#include "Sound.h"
#include "iMission.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iMissionUI.h"
#include "iUnitPanel.h"
#include "iCriticalIcons.h"
#include "iUnitIconBar.h"
#include "iGameStates.h"
#include "iActionDecorator.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_BASELEVEL = 3,
	N_MAXLEVELS_COUNT = 8,
	N_MAXUNITS_COUNT = 7,
	N_NUM_CRITICALS_ICONS = 6;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitTab
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitTab: public CActionDecorator<CWindow>
{
	OBJECT_BASIC_METHODS(CUnitTab);
private:
	ZDATA_(TBaseClass)
	CPtr<NGame::IMission> pMission;
	////
	CPtr<NGame::IUnitTracker> pUnit;
	CPtr<CText> pAP;
	CPtr<CText> pName;
	CPtr<CImage> pDisable;
	CPtr<CImage> pSelectedTab;
	CPtr<CImage> pUnselectedTab;
	CObj<CLineBar> pLife;
	CObj<CLineBar> pHealedLife;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); f.Add(2,&pMission); f.Add(3,&pUnit); f.Add(4,&pAP); f.Add(5,&pName); f.Add(6,&pDisable); f.Add(7,&pSelectedTab); f.Add(8,&pUnselectedTab); f.Add(9,&pPKLifeBackground); f.Add(10,&pBarBackground); f.Add(11,&pLifeBackground); f.Add(12,&pLife); f.Add(13,&pHealedLife); f.Add(14,&pPKLife); return 0; }
	CObj<CImage> pPKLifeBackground;
	CObj<CImage> pBarBackground;
	CObj<CImage> pLifeBackground;
	CObj<CLineBar> pPKLife;

public:
	CUnitTab() {}
	CUnitTab( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool CanHandleState( NGame::IState *pState ) const;
	CObjectBase* GetTarget();

	NGame::IUnitTracker* Get() const;
	void Set( NGame::IUnitTracker *pUnit );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitTab::CUnitTab( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	TBaseClass( sInfo, _pMission ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitTab::CanHandleState( NGame::IState *pState ) const
{
	CDynamicCast<NGame::CStateMove> pMove(pState);
	if (pMove)
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CUnitTab::GetTarget()
{
	return pUnit->GetUnit();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGame::IUnitTracker* CUnitTab::Get() const
{
	return pUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTab::Set( NGame::IUnitTracker *_pUnit )
{
	pUnit = _pUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitTab::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pPKLife = new CLineBar( sEvent.pLoader->GetControl( "pklife" ) );
			pPKLifeBackground = new CImage( sEvent.pLoader->GetControl( "pklife_background" ) );
			pLife = new CLineBar( sEvent.pLoader->GetControl( "life" ) );
			pHealedLife = new CLineBar( sEvent.pLoader->GetControl( "life_healed" ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pAP = GetUIWindow<CText>( this, "ap" );
			pName = GetUIWindow<CText>( this, "name" );

			pDisable = GetUIWindow<CImage>( this, "disable" );
			pSelectedTab = GetUIWindow<CImage>( this, "ut_selected" );
			pUnselectedTab = GetUIWindow<CImage>( this, "ut_unselected" );

			pBarBackground = GetUIWindow<CImage>( this, "bar_background" );
			pLifeBackground = GetUIWindow<CImage>( this, "life_background" );
			break;
		}
	}

	return TBaseClass::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTab::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !IsValid( pUnit ) )
	{
		SetStyle( STYLE_VISIBLE, false );
		return;
	}

	SetStyle( STYLE_VISIBLE, true );
	pDisable->SetStyle( STYLE_VISIBLE, !pUnit->IsActive() );
	pSelectedTab->SetStyle( STYLE_VISIBLE, pUnit->IsSelected() );
	pUnselectedTab->SetStyle( STYLE_VISIBLE, !pUnit->IsSelected() );

	NRPG::SUnitInfo sUnitInfo;
	pUnit->GetUnit()->GetInfo( &sUnitInfo );

	WCHAR wsText[256];

	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", sUnitInfo.nAP );
	pAP->SetText( wsText );
	pAP->SetStyle( STYLE_VISIBLE, !pMission->IsRealTime() );

	pName->SetText( L"<font face=Courier size=10pt><nowrap>" + pUnit->GetUnit()->GetRPG()->GetName() );

	pLife->Set( float( sUnitInfo.nHP ) / sUnitInfo.nMaxHP );
	pHealedLife->Set( float( sUnitInfo.nHP + sUnitInfo.nHealedHP ) / sUnitInfo.nMaxHP );

	// retail CUnitTab::Draw @0x254810: the PK flag (unit wears a Panzerklein) selects the bar
	// layout. SetImage draws the texture at its natural size, so the texture set is what makes the
	// life bar tall (one bar fills the height of two) or short (paired with the PK-armor bar).
	pPKLife->SetStyle( STYLE_VISIBLE, sUnitInfo.bWearingPK );
	pPKLifeBackground->SetStyle( STYLE_VISIBLE, sUnitInfo.bWearingPK );
	if ( !sUnitInfo.bWearingPK )
	{
		// on-foot: single double-height life bar, PK-armor bar hidden.
		pLife->SetImage( NDb::GetUITexture( 897 ) );
		pHealedLife->SetImage( NDb::GetUITexture( 896 ) );
		pBarBackground->SetImage( NDb::GetUITexture( 895 ) );
		pLifeBackground->SetImage( NDb::GetUITexture( 898 ) );
	}
	else
	{
		// piloting a Panzerklein: short life bar + the PK-armor bar fed from the worn PK's VP.
		pLife->SetImage( NDb::GetUITexture( 893 ) );
		pHealedLife->SetImage( NDb::GetUITexture( 891 ) );
		pBarBackground->SetImage( NDb::GetUITexture( 889 ) );
		pLifeBackground->SetImage( NDb::GetUITexture( 890 ) );
		pPKLife->Set( sUnitInfo.nMaxPKLife ? float( sUnitInfo.nPKLife ) / sUnitInfo.nMaxPKLife : 0.f );
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitsTab
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitsTabBar: public CWindow
{
	OBJECT_BASIC_METHODS(CUnitsTabBar)
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	////
	vector< CPtr<CUnitTab> > tabsSet;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&tabsSet); return 0; }

public:
	CUnitsTabBar() {}
	CUnitsTabBar( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitsTabBar::CUnitsTabBar( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission ), tabsSet( N_MAXUNITS_COUNT )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitsTabBar::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_LBUTTONUP:
		{
			for ( int nTemp = 0; nTemp < tabsSet.size(); nTemp++ )
			{
				NGame::IUnitTracker *pUnit = tabsSet[nTemp]->Get();
				if ( tabsSet[nTemp]->HitTest( sEvent.nX, sEvent.nY ) && IsValid( pUnit ) && pUnit->IsSelected() )
					pMission->FocusCameraOnUnit( pUnit->GetUnit() );
			}

			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			for ( int nTemp = 0; nTemp < tabsSet.size(); nTemp++ )
			{
				tabsSet[nTemp] = new CUnitTab( sEvent.pLoader->GetControl( NStr::Format( "hero_%d", ( nTemp + 1 ) ) ), pMission );
				tabsSet[nTemp]->Set( 0 );
			}

			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitsTabBar::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetUnits( &unitsSet );

	int nCount = 0;
	for ( int nTemp = 0; nTemp < tabsSet.size(); nTemp++ )
	{
		tabsSet[nTemp]->Set( 0 );
		if ( nTemp < unitsSet.size() )
		{
			if ( !unitsSet[nTemp]->GetUnit()->IsDead() )
			{
				tabsSet[nCount]->Set( unitsSet[nTemp] );
				tabsSet[nCount]->SetStyle( STYLE_VISIBLE, true );
				tabsSet[nCount]->SetStyle( STYLE_TOPMOST, unitsSet[nTemp]->IsSelected() );
				nCount++;
			}
		}
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitFace
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitFace: public CActionDecorator<CUnitView>
{
	OBJECT_BASIC_METHODS(CUnitFace)
private:
	ZDATA_(TBaseClass)
	CPtr<NGame::IUnitTracker> pUnit;
	////
	// retail reworked CUnitFace into an ack-effect face animation with NO life bars: retail
	// CUnitFace::ProcessMessage @0x254d20 fetches no life/life_healed and Draw @0x254df0 is the
	// eStage ack-effect state machine. Unit life shows only on the hero tabs (CUnitTab), never on
	// the single-/multi-unit face (retail CInfoPanelSingleUnit @0x25cd30 has no life member), so
	// the dev's pLife/pHealedLife were dropped for retail parity -- they also caused the
	// "face (life,life_healed)" container-not-found errors. The ack-effect ANIMATION behavior
	// (members below, already save-reconciled tags 1-9) is a release-new feature -- deferred.
	enum EStage { ST_NONE=0, ST_SOURCE_HIDE=1, ST_TARGET_SHOW=2, ST_ACK_WAIT=3, ST_TARGET_HIDE=4, ST_SOURCE_SHOW=5, ST_ACK_TERMINATE=6 };
	STime sEventTime;
	EStage eStage = ST_NONE;
	CPtr<NUI::CAckEvent> pEvent;
	CObj<NUI::CImage> pAckEffect;
	bool bDoNotPlayEffect = false;
	CPtr<NWorld::CUnit> pFaceUnit;
	CObj<NUI::CImage> pRedImage;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); f.Add(2,&pUnit); f.Add(3,&sEventTime); f.Add(4,&eStage); f.Add(5,&pEvent); f.Add(6,&pAckEffect); f.Add(7,&bDoNotPlayEffect); f.Add(8,&pFaceUnit); f.Add(9,&pRedImage); return 0; }

	// release CUnitFace::SetUnitFace @0x254cc0: point the 3D face at a raw world unit (the ack speaker
	// -- may NOT be the selected/tracked unit, even an enemy) and play its talk gesture.
	void SetUnitFace( NWorld::CUnit *pWorldUnit );

public:
	CUnitFace() {}
	CUnitFace( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool CanHandleState( NGame::IState *pState ) const;
	CObjectBase* GetTarget();

	void SetUnit( NGame::IUnitTracker *pUnit );
	// release CUnitFace::PlayAckEvent @0x254ba0: arm the ack-effect state machine (Draw @0x254df0)
	void PlayAckEvent( const STime &sTime, NUI::CAckEvent *pEvent );
	bool IsPlayingAck() const { return eStage != ST_NONE; }

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitFace::CUnitFace( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	TBaseClass( sInfo, _pMission )
{
	// PORTRAIT-LIPSYNC ROOT CAUSE: CActionDecorator's ctor constructs the wrapped CUnitView with
	// Type(sInfo) only, leaving CUnitView::pRenderGame NULL. SetUnit -> NRender::CreateShowUnit(
	// ..., pRenderGame=0 ) then builds a PRIVATE `new CHeadsController` for the portrait head, so
	// (a) nothing ever Advance()s it (only the mission CRenderGame's own controller is advanced in
	// UpdateViewWorld, RWGame.cpp) -- the portrait head's sequence clock is frozen -- and (b) it is
	// NOT the controller CAckIcon::PlayAck lipsyncs through (pMission->GetRenderGame()->
	// GetHeadController()), so the ack sequence played only on the invisible world-model head.
	// Retail parity: retail CRenderGame::PlaySequence @0x2cb150 keys the ONE shared controller by
	// the pers' CHeadInfo (+0x90) -- shared between the world unit and the portrait clone. Dev keys
	// by NWorld::CUnit* (LSController.h), equivalent PROVIDED the controller instance is shared --
	// which this line restores.
	if ( IsValid( _pMission ) )
		pRenderGame = _pMission->GetRenderGame();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitFace::CanHandleState( NGame::IState *pState ) const
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CUnitFace::GetTarget()
{
	return pUnit->GetUnit();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitFace::SetUnit( NGame::IUnitTracker *_pUnit )
{
	// Guard on change (retail SetUnit @0x254c80): the dev called this every frame, re-creating the
	// show-unit and resetting the skeletal animation to frame 0 -> the face looked frozen. Build the
	// face model once per unit so its looping idle (CFakeWorldUnit::Update) can play out.
	if ( pUnit == _pUnit )
		return;

	pUnit = _pUnit;
	// retail SetUnit @0x254c80: a unit change while an ack is in flight arms ST_ACK_TERMINATE --
	// Draw restores the (new) unit's face and retires the event; setting the view here directly
	// would clobber the speaker's face mid-crossfade.
	if ( eStage != ST_NONE )
	{
		eStage = ST_ACK_TERMINATE;
		return;
	}
	if ( IsValid( pUnit ) )
	{
		// release CUnitFace::SetUnitFace @0x254cc0: pFaceUnit = unit; then
		// CUnitView::SetUnit(unit, GetDBCamera(0x13a1 = 5025 "PersInventory"), false, true, true)
		// -> bItems=false, bShowCap=true, bPlayIdle=true: the face body plays the looping
		// INTERFACE_IDLE clip instead of a frozen POSE stand.
		pFaceUnit = pUnit->GetUnit();
		CUnitView::SetUnit( pUnit->GetUnit(), NDb::GetDBCamera( 5025 ), false, true, true );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitFace::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_LBUTTONDOWN:
		return true;
	case EVENT_LBUTTONUP:
		{
			SendMessage( GetParent(), SEvent( EVENT_NOTIFY, GetWindowID() ) );
			return true;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// release CUnitFace::SetUnitFace @0x254cc0: bind the 3D face to a raw world unit (bItems=false,
// bShowCap=true, bPlayIdle=true -> INTERFACE_IDLE) and record it as the face unit. Used by the ack
// state machine to swap the portrait to the speaker (who may be a non-selected unit or an enemy).
void CUnitFace::SetUnitFace( NWorld::CUnit *pWorldUnit )
{
	pFaceUnit = pWorldUnit;
	if ( IsValid( pWorldUnit ) )
		CUnitView::SetUnit( pWorldUnit, NDb::GetDBCamera( 5025 ), false, true, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail ack-crossfade overlay ramp (CUnitFace::Draw @0x254df0; disasm-verified 0x654e5c..0x654e7f
// fade-out and 0x654f3b..0x654f59 fade-in): the "ackeffect" CImage (UITexture 951 "AckSwitch",
// chunk-verified in the Steam game.db) sits TOPMOST over the portrait and is WHITE-modulated with
//   alpha = t/500 * 255           while a face fades OUT  (SOURCE_HIDE / TARGET_HIDE)
//   alpha = (1 - t/500) * 255     while a face fades IN   (TARGET_SHOW / SOURCE_SHOW)
// (retail constants 0.002f @0x8b9d8c and 255.0f @0x8b56b8; color dword (a<<24)|0xFFFFFF). The head
// SWAP happens behind the fully-opaque overlay -- that is the retail "crossfade" the instant cut
// was missing. Retail truncates the alpha to AL (movzx al) -- the stage flips before it can wrap;
// we clamp instead so a frame hitch cannot flash-wrap.
static void SetAckEffectRamp( CImage *pImg, const STime &sTime, const STime &sEventTime, bool bFadeToWhite )
{
	if ( !IsValid( pImg ) )
		return;
	float f = float( sTime - sEventTime ) * 0.002f;
	if ( !bFadeToWhite )
		f = 1.f - f;
	int nA = int( f * 255.f );
	if ( nA < 0 ) nA = 0;
	if ( nA > 255 ) nA = 255;
	pImg->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, (unsigned char)nA ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release CUnitFace::PlayAckEvent @0x254ba0: arm the ack. The effect is suppressed (bDoNotPlayEffect)
// when the speaker is ALREADY the shown unit -- then the portrait just plays the talk gesture without
// the swap-out/swap-in. Otherwise the state machine (Draw) hides the current face, shows the speaker's,
// waits out the bark, then restores the selected unit.
void CUnitFace::PlayAckEvent( const STime &sTime, NUI::CAckEvent *_pEvent )
{
	pEvent = _pEvent;
	if ( !IsValid( pEvent ) )
		return;

	// retail ctor @0x256610 creates the crossfade overlay up front: CImage "ackeffect", full face
	// rect, style TOPMOST only (starts hidden), skinned with UITexture 951 "AckSwitch" (disasm
	// literal `mov ecx,0x3b7` @0x6567ae -- the Ghidra output register-noised the id). Created
	// LAZILY here instead so worlds restored from pre-crossfade saves (serialized tag 6 = null)
	// heal themselves on the first bark.
	if ( !IsValid( pAckEffect ) )
	{
		pAckEffect = new CImage( SWindowInfo( this, SPoint( 0, 0 ), GetSize(), "ackeffect", STYLE_TOPMOST ) );
		pAckEffect->SetImage( NDb::GetUITexture( 951 ) );
	}

	// retail PlayAckEvent @0x254ba0: play the full portrait "turn to camera + talk" dance ONLY when
	// there is a valid tracked face AND a valid speaker; otherwise fall through to the FALLBACK below.
	NWorld::CAckEvent *pInner = pEvent->GetAckEvent();
	if ( IsValid( pUnit ) && IsValid( pInner ) && IsValid( pInner->pUnit ) )
	{
		bDoNotPlayEffect = ( pInner->pUnit.GetPtr() == pUnit->GetUnit() );
		sEventTime = sTime;
		eStage = ST_SOURCE_HIDE;
		// retail @0x254ba0 tail: show the flash overlay for the dance (hidden when the speaker IS
		// the shown unit); Draw's per-frame ramp drives the alpha from ~0.
		if ( IsValid( pAckEffect ) )
		{
			pAckEffect->SetStyle( STYLE_VISIBLE, !bDoNotPlayEffect );
			pAckEffect->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0 ) );
		}
		return;
	}

	// retail FALLBACK (@0x254ba0 tail: CAckEvent::Set): the dance can't play, but we MUST still flip
	// bReady so the ack's voice + subtitle still play (without the portrait dance). CRITICAL under the
	// deferred scheme: skipping Set() here leaves bReady=false and the voice NEVER starts.
	pEvent->Set( sTime );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitFace::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	// release CUnitFace::Draw @0x254df0 -- the FULL 6-stage ack-effect machine. Retail stage order
	// (each transition 500ms): SOURCE_HIDE -> [SetUnitFace(speaker) + StartTalk, non-loop] ->
	// TARGET_SHOW -> [PoseTalk LOOP] -> ACK_WAIT (bark TTL) -> [EndTalk = turn away] ->
	// TARGET_HIDE -> [SetUnitFace(selected) + PlayAnimation(null) = reset to idle] ->
	// SOURCE_SHOW -> NONE. There is NO head-yaw API -- the signature "turns her head to the
	// screen" IS a skeletal clip. DISASM CORRECTION (@0x654f06/0x655000/0x65505d): the Ghidra
	// decomp printed GetAnimation(0)/(1) but DROPPED the fastcall ECX id (same lie as the
	// GetDBCamera(0)-vs-0x13a1 one in SetUnitFace @0x254cc0). The REAL retail ids -- all three
	// verified present in the Steam game.db Animations table, skeleton 8, clip
	// Characters\Skeletons01\BaseSkeleton\Clips\Weapons\NoWeapon\Special\TalkInterface.ma:
	//   0x11c2 = 4546 "StartTalk" (frames   0- 500, non-loop) -- TURN head to the camera
	//   0x11c4 = 4548 "PoseTalk"  (frame  500- 500, LOOP)     -- hold the facing pose while talking
	//   0x11c3 = 4547 "EndTalk"   (frames 500-1000, non-loop) -- turn back away
	// bDoNotPlayEffect gates only the pAckEffect crossfade image (unported cosmetics), NEVER
	// the animations. Life bars were removed for retail parity (see class comment).
	const int N_ANIM_START_TALK = 4546;	// 0x11c2 "StartTalk" -- turn-to-camera gesture
	const int N_ANIM_END_TALK   = 4547;	// 0x11c3 "EndTalk"   -- turn-away gesture
	const int N_ANIM_POSE_TALK  = 4548;	// 0x11c4 "PoseTalk"  -- held facing pose (looped)
	const STime N_ACK_STEP = 500;
	switch ( eStage )
	{
	case ST_SOURCE_HIDE:
		if ( !IsValid( pEvent ) )
		{
			eStage = ST_TARGET_HIDE;	// cancelled mid-fade: fall through the restore path (retail @0x654e3e)
			sEventTime = sTime;
			break;
		}
		// retail @0x654e5c: fade the CURRENT face TO WHITE over the 500ms step.
		SetAckEffectRamp( pAckEffect, sTime, sEventTime, true );
		if ( sTime - sEventTime > N_ACK_STEP )
		{
			// swap the portrait to the speaker + play the turn-to-camera gesture (retail
			// GetAnimation(0x11c2) "StartTalk" @0x654f06, non-loop). The mouth/visemes are the
			// separate CHeadsController lipsync driven from CAckIcon::PlayAck (started by the
			// bReady handshake below); THIS skeletal clip is what visibly rotates the head/neck
			// bones toward the screen for the bark.
			// retail @0x654ecb: pin the overlay FULLY OPAQUE (0xffffffff) so the head swap happens
			// behind solid white -- this is what makes it a crossfade instead of an instant cut.
			if ( IsValid( pAckEffect ) )
				pAckEffect->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ) );
			NWorld::CAckEvent *pInner = pEvent->GetAckEvent();
			if ( IsValid( pInner ) && IsValid( pInner->pUnit ) )
				SetUnitFace( pInner->pUnit );
			PlayAnimation( NDb::GetAnimation( N_ANIM_START_TALK ), false );
			eStage = ST_TARGET_SHOW;
			sEventTime = sTime;
		}
		break;
	case ST_TARGET_SHOW:
		if ( !IsValid( pEvent ) )
		{
			eStage = ST_TARGET_HIDE;
			sEventTime = sTime;
			break;
		}
		// retail @0x654f3b: reveal the SPEAKER from white -- alpha (1 - t/500)*255.
		SetAckEffectRamp( pAckEffect, sTime, sEventTime, false );
		// retail @0x654f84: if the event went bReady early (another component Set() it -- e.g. a
		// terminate elsewhere), abort the dance: full white, restore the selected face, fade out.
		if ( pEvent->IsReady() )
		{
			if ( IsValid( pAckEffect ) )
				pAckEffect->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ) );
			if ( IsValid( pUnit ) )
				SetUnitFace( pUnit->GetUnit() );
			eStage = ST_SOURCE_SHOW;
			sEventTime = sTime;
			break;
		}
		if ( sTime - sEventTime > N_ACK_STEP )
		{
			// retail ST_TARGET_SHOW -> ACK_WAIT @0x254df0: the speaker's face is now on screen, so
			// flip bReady (CAckEvent::Set) -- THIS is the handshake that starts the voice + the
			// CHeadsController lipsync over in CAckIcon::PlayAck.
			pEvent->Set( sTime );
			// retail @0x654fc6-region: overlay OFF for the talking hold (SetStyle(VISIBLE,false) +
			// color alpha 0).
			if ( IsValid( pAckEffect ) )
			{
				pAckEffect->SetStyle( STYLE_VISIBLE, false );
				pAckEffect->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0 ) );
			}
			// retail GetAnimation(0x11c4) "PoseTalk" @0x655000, LOOPED: hold the turned-to-camera
			// pose (frame 500 of TalkInterface.ma) for the whole bark.
			PlayAnimation( NDb::GetAnimation( N_ANIM_POSE_TALK ), true );
			eStage = ST_ACK_WAIT;
			sEventTime = sTime;
		}
		break;
	case ST_ACK_WAIT:
		// hold the speaker until the ack is truly COMPLETE (bReady && TTL elapsed && voice finished --
		// retail IsComplete @0x1d0dc0) or it is cancelled; NOT a fixed end-time.
		if ( !IsValid( pEvent ) || pEvent->IsComplete( sTime ) )
		{
			// retail @0x655040-region: re-show the flash overlay for the fade-back (same
			// bDoNotPlayEffect gate as the way in).
			if ( IsValid( pAckEffect ) )
				pAckEffect->SetStyle( STYLE_VISIBLE, !bDoNotPlayEffect );
			// retail GetAnimation(0x11c3) "EndTalk" @0x65505d, non-loop: turn the head back away.
			PlayAnimation( NDb::GetAnimation( N_ANIM_END_TALK ), false );
			eStage = ST_TARGET_HIDE;
			sEventTime = sTime;
		}
		break;
	case ST_TARGET_HIDE:
		// retail: fade the SPEAKER to white over the 500ms step.
		SetAckEffectRamp( pAckEffect, sTime, sEventTime, true );
		if ( sTime - sEventTime > N_ACK_STEP )
		{
			// retail: pin full white for the swap back, then restore the selected unit's face;
			// SetUnitFace's bPlayIdle rearms INTERFACE_IDLE (retail plays a null clip here).
			if ( IsValid( pAckEffect ) )
				pAckEffect->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ) );
			if ( IsValid( pUnit ) )
				SetUnitFace( pUnit->GetUnit() );
			eStage = ST_SOURCE_SHOW;
			sEventTime = sTime;
		}
		break;
	case ST_SOURCE_SHOW:
		// retail: reveal the restored face from white.
		SetAckEffectRamp( pAckEffect, sTime, sEventTime, false );
		if ( sTime - sEventTime > N_ACK_STEP )
		{
			pEvent = 0;
			eStage = ST_NONE;
			// retail @0x6551xx: overlay OFF once the dance is over.
			if ( IsValid( pAckEffect ) )
				pAckEffect->SetStyle( STYLE_VISIBLE, false );
		}
		break;
	case ST_ACK_TERMINATE:
		// retail SetUnit @0x254c80: the tracked unit changed while an ack was in flight -- restore the
		// (new) unit's face immediately and retire the event from the face machine. FALLBACK: still
		// flip bReady (Set) so the ack's voice/subtitle play out through CAckIcon even though the
		// portrait dance was aborted -- Cancel() alone would leave bReady=false and hang the un-played
		// ack (its voice would NEVER start under the deferred scheme). CAckIcon keeps its own ref to
		// the same event, so the voice continues there after we drop ours.
		// retail ACK_TERMINATE @0x655xxx: overlay OFF.
		if ( IsValid( pAckEffect ) )
			pAckEffect->SetStyle( STYLE_VISIBLE, false );
		if ( IsValid( pUnit ) )
			SetUnitFace( pUnit->GetUnit() );
		if ( IsValid( pEvent ) )
			pEvent->Set( sTime );
		pEvent = 0;
		eStage = ST_NONE;
		break;
	default:
		break;
	}

	// DEAD/UNCONSCIOUS PORTRAIT RED TINT (retail CUnitFace::Draw @0x254df0 tail): show a translucent red
	// overlay over the 3D portrait iff the shown face unit CANNOT fight -- i.e. it is dead, unconscious, or
	// dying (retail tests !CanFight(); CUnit vtbl[0]=CanFight @0x3c6840 == !IsDead() && !IsUnconscious()).
	// This covers both reported cases: a dying unit speaking its ack (pFaceUnit = the speaker) and an
	// unconscious party member selected (pFaceUnit = the selected unit). Retail ctor @0x256610 makes
	// pRedImage a full-face-rect CImage with color 0x33fa3500 = SPixel8888(0xFA,0x35,0x00,0x33) (red-orange
	// ~20% alpha) and NO texture -- the color-fill renders as a flat quad (CImageDraw fills sColor when the
	// texture is null). Created lazily (like pAckEffect above) so pre-tint saves (serialized tag 9 = null)
	// heal on the first Draw.
	if ( !IsValid( pRedImage ) )
	{
		pRedImage = new CImage( SWindowInfo( this, SPoint( 0, 0 ), GetSize(), "redimage", STYLE_TOPMOST ) );
		pRedImage->SetColor( NGfx::SPixel8888( 0xFA, 0x35, 0x00, 0x33 ) );
	}
	pRedImage->SetStyle( STYLE_VISIBLE,
		IsValid( pFaceUnit ) && ( pFaceUnit->IsDead() || pFaceUnit->IsUnconscious() ) );

	TBaseClass::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSlotReloadImage
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSlotReloadButton: public CButton
{
	OBJECT_NOCOPY_METHODS(CSlotReloadButton)
private:
	ZDATA_(CButton)
	CPtr<NGame::IMission> pMission;
	////
	CObj<CModel> pModel;
	CObj<CToolTip> pToolTip;
	CDBPtr<NDb::CRPGItem> pItem;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CButton*)this); f.Add(2,&pMission); f.Add(3,&pIcon); f.Add(4,&pModel); f.Add(5,&pToolTip); f.Add(6,&pItem); f.Add(7,&nSlot); return 0; }
	CObj<CImage> pIcon;
	int nSlot = 0;

public:
	CSlotReloadButton() {}
	CSlotReloadButton( const SWindowInfo &sInfo, NGame::IMission *pMission );

	void Set( NDb::CRPGItem *pItem );
	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CSlotReloadButton::CSlotReloadButton( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CButton( sInfo ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlotReloadButton::Set( NDb::CRPGItem *_pItem )
{
	if ( _pItem == pItem )
		return;

	pItem = _pItem;
	if ( pItem->pModel )
	{
		const NDb::SCameraParams &sCamera = pItem->sCameras[NDb::CAMERA_RELOADBUTTON];

		CVec3 vForwardDir;
		CQuat q = CQuat( sCamera.fYaw, V3_AXIS_Z ) * CQuat( sCamera.fPitch, V3_AXIS_X );
		q.GetYAxis( &vForwardDir );

		CVec3 vCP( sCamera.vAnchor - vForwardDir * sCamera.fDistance );
		SFBTransform res;
		MakeMatrix( &res, sCamera.fPitch, sCamera.fYaw, sCamera.fRoll, vCP );

		SRand sRnd;
		pModel->SetModel( pItem->pModel->CreateModel( &sRnd ) );
		pModel->SetTransform( new CFBTransform( res ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSlotReloadButton::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATECREATE:
		{
			pModel = new CModel( SWindowInfo( this, SPoint( 0, 0 ), GetSize(), "", STYLE_ENABLED | STYLE_VISIBLE | STYLE_TRANSPARENT | STYLE_TOPMOST ) );
			pToolTip = new CToolTip( SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 0, 0 ), "tooltip", STYLE_ENABLED ) );
			SetToolTip( pToolTip );
			break;
		}
	}

	return CButton::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlotReloadButton::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	ASSERT( unitsSet.size() == 1 );
	if ( unitsSet.size() != 1 )
		return;

	CPtr<NGame::IUnitTracker> pUnit = unitsSet.front();

	int nActionAP = 0;
	NWorld::EUnitCommandResult eResult = pUnit->GetUnit()->CanDo( new NWorld::CCmdReload, 0, &nActionAP );

	CPtr<NDb::CString> pString;
	if ( ( eResult == NWorld::UCR_OK ) || ( eResult == NWorld::UCR_NOT_ENOUGH_AP ) )
		pString = NDb::GetString( 4313 );
	else
		pString = NDb::GetString( 4314 );

	wstring wsToolTipTemplate( L"%s" );
	if ( IsValid( pString ) )
		wsToolTipTemplate = pString->szStr;

	if ( nActionAP != -1 )
		pToolTip->SetVal( L"ap", nActionAP );
	else
		pToolTip->SetVal( L"ap", L"N/A" );

	NGfx::SPixel8888 sColor( 0xFF, 0xFF, 0xFF, 0xFF );
	if ( eResult == NWorld::UCR_NOT_ENOUGH_AP )
		sColor = NGfx::SPixel8888( 0x5F, 0x5F, 0xBF, 0xFF );

	SetColor( sColor );
	pModel->SetColor( sColor );

	CButton::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInfoPanelSlot
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInfoPanelSlot: public CSlot
{
	OBJECT_NOCOPY_METHODS(CInfoPanelSlot)
private:
	ZDATA_(CSlot)
	CPtr<NGame::IMission> pMission;
	////
	NDb::ESlot eType;
	CPtr<NWorld::CUnit> pUnit;
	////
	CPtr<CWindow> pFade;
	CObj<CMLText> pAmmo;
	CObj<CSlotReloadButton> pReload;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CSlot*)this); f.Add(2,&pMission); f.Add(3,&eType); f.Add(4,&pUnit); f.Add(5,&pFade); f.Add(6,&pAmmo); f.Add(7,&pReload); return 0; }

public:
	CInfoPanelSlot() {}
	CInfoPanelSlot( const SWindowInfo &sInfo, NGame::IMission *pMission, NDb::ESlot eType );

	void Set( NWorld::CUnit *pUnit );

	void Take( int nX, int nY );
	void Place( int nX, int nY, const NWorld::SItem &sItem );
	bool CanPlace( int nX, int nY, const NWorld::SItem &sItem, int *nAP = 0 );
	void GetItemsList( vector<SItem> *pItemsSet );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInfoPanelSlot
////////////////////////////////////////////////////////////////////////////////////////////////////
CInfoPanelSlot::CInfoPanelSlot( const SWindowInfo &sInfo, NGame::IMission *_pMission, NDb::ESlot _eType ):
	CSlot( sInfo, _pMission, 1, 1, NDb::CAMERA_SLOT, false ), pMission( _pMission ), eType( _eType )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelSlot::Set( NWorld::CUnit *_pUnit )
{
	pUnit = _pUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelSlot::Take( int nX, int nY )
{
	NWorld::SItem sSource;
	sSource.eType = NWorld::SItem::SLOT;
	sSource.nSlot = eType;
	sSource.pItem = pUnit->GetRPG()->GetInventoryInfo()->Get( eType );
	sSource.pUnit = pUnit;
	pMission->CommandState( new NGame::CStateMoveItem( pUnit, sSource, NWorld::SItem( pUnit, NWorld::SItem::HAND ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelSlot::Place( int nX, int nY, const NWorld::SItem &sItem )
{
	ASSERT( IsValid( pUnit ) );

	NWorld::SItem sTarget;
	sTarget.eType = NWorld::SItem::SLOT;
	sTarget.nSlot = eType;
	sTarget.pUnit = pUnit;

	pMission->Command( pUnit, new NWorld::CCmdMoveInventoryItem( NWorld::SItem( sItem.pUnit, NWorld::SItem::HAND, sItem.pItem.GetPtr() ), sTarget ) );
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInfoPanelSlot::CanPlace( int nX, int nY, const NWorld::SItem &sItem, int *nAP )
{
	NWorld::SItem sTarget;
	sTarget.eType = NWorld::SItem::SLOT;
	sTarget.nSlot = eType;
	sTarget.pUnit = pUnit;

	NWorld::EUnitCommandResult eRes = pUnit->CanDo( new NWorld::CCmdMoveInventoryItem( NWorld::SItem( sItem.pUnit, NWorld::SItem::HAND, sItem.pItem ), sTarget ) );
	if ( eRes != NWorld::UCR_OK )
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelSlot::GetItemsList( vector<SItem> *pItemsSet )
{
	ASSERT( IsValid( pUnit ) );

	CPtr<NRPG::IInventoryInfo> pInfo = pUnit->GetRPG()->GetInventoryInfo();
	CPtr<NRPG::IInventoryItem> pItem = pInfo->Get( eType );
	if ( IsValid( pItem ) )
	{
		SItem &sItem = *pItemsSet->insert( pItemsSet->end(), SItem());
		sItem.sPos = CTPoint<int>( 0, 0 );
		sItem.pItem = pItem;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInfoPanelSlot::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_LBUTTONDOWN:
		{
			NWorld::IPlayer::SItemInfo sInfo;
			if ( !GetDragItem( &sInfo ) && ( pUnit->GetRPG()->GetInventoryInfo()->GetActiveSlot() != eType ) )
			{
				pMission->Command( pUnit, new NWorld::CCmdSetActiveItem( eType ) );
				return true;
			}

			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pAmmo = new CMLText( sEvent.pLoader->GetControl( "ammo" ) );
			pReload = new CSlotReloadButton( sEvent.pLoader->GetControl( "weapon_reload" ), pMission );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pFade = GetUIWindow<CWindow>( this, "fade" );
			break;
		}
	}

	if ( CSlot::ProcessMessage( sEvent ) )
		return true;

	switch( sEvent.nEvent )
	{
	case EVENT_RBUTTONDOWN:
	case EVENT_LBUTTONDOWN:
		{
			NWorld::IPlayer::SItemInfo sInfo;
			if ( GetDragItem( &sInfo ) )
			{
				NWorld::SItem sSource( sInfo.pUnit, NWorld::SItem::HAND, sInfo.pItem );

				CDynamicCast<NRPG::IClipItem> pClip( sInfo.pItem );
				CDynamicCast<NRPG::IWeaponItemInfo> pWeapon( pUnit->GetRPG()->GetInventoryInfo()->Get( eType ) );
				if ( IsValid( pClip ) && IsValid( pWeapon ) )
					pMission->Command( pUnit, new NWorld::CCmdLoadWeapon( pWeapon, sSource ) );
				else
				{
					if ( CanPlace( sEvent.nX, sEvent.nY, sSource ) )
						Place( sEvent.nX, sEvent.nY, sSource );
					else
						PlaySound( NDb::GetSound( NGame::N_SOUND_ERROR ) );
				}
			}
			else if ( pUnit->GetRPG()->GetInventoryInfo()->GetActiveSlot() == eType )
				Take( sEvent.nX, sEvent.nY );
			return true;
		}
	case EVENT_RBUTTONUP:
	case EVENT_LBUTTONUP:
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelSlot::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !IsValid( pUnit ) )
		return;

	CPtr<NRPG::IInventoryInfo> pInventory = pUnit->GetRPG()->GetInventoryInfo();
	pFade->SetStyle( STYLE_VISIBLE, pInventory->GetActiveSlot() != eType );

	CPtr<NRPG::IInventoryItem> pItem = pInventory->Get( eType );
	CDynamicCast<NRPG::IWeaponItemInfo> pWeapon(pItem);
	if (pWeapon)
	{
		CPtr<NRPG::IClipItem> pRPGClipItem = pWeapon->GetInnerClip();
		pReload->SetStyle( STYLE_VISIBLE, true );
		pReload->Set( pRPGClipItem->GetDBItem() );

		WCHAR wsBuffer[256];
		swprintf( wsBuffer, L"<color=FFB4997C><font face=Impact size=36pt outlinesize=2 outlinecolor=FF513E2B><left>%d/%d", pRPGClipItem->GetIncQuantity(), pRPGClipItem->GetMaxIncQuantity() );
		pAmmo->SetText( wsBuffer );
		pAmmo->SetStyle( STYLE_VISIBLE, true );
	}
	else
	{
		pAmmo->SetStyle( STYLE_VISIBLE, false );
		pReload->SetStyle( STYLE_VISIBLE, false );
	}

	CSlot::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInfoPanelSpecialSlot
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInfoPanelSpecialSlot: public CWindow
{
	OBJECT_NOCOPY_METHODS(CInfoPanelSpecialSlot)
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	////
	CPtr<NWorld::CUnit> pUnit;
	////
	CObj<CText> pWeaponAmmoText;
	CObj<CShowItemModel> pItemModel;
	CObj<CSlotReloadButton> pReload;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pUnit); f.Add(4,&pWeaponAmmoText); f.Add(5,&pItemModel); f.Add(6,&pReload); return 0; }

public:
	CInfoPanelSpecialSlot() {}
	CInfoPanelSpecialSlot( const SWindowInfo &sInfo, NGame::IMission *pMission );

	void Set( NWorld::CUnit *pUnit );
	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInfoPanelSpecialSlot
////////////////////////////////////////////////////////////////////////////////////////////////////
CInfoPanelSpecialSlot::CInfoPanelSpecialSlot( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelSpecialSlot::Set( NWorld::CUnit *_pUnit )
{
	pUnit = _pUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInfoPanelSpecialSlot::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pReload = new CSlotReloadButton( sEvent.pLoader->GetControl( "weapon_reload" ), pMission );
			pItemModel = new CShowItemModel( sEvent.pLoader->GetControl( "view" ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pWeaponAmmoText = GetUIWindow<CText>( this, "ammo" );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelSpecialSlot::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	CPtr<NRPG::IWeaponItemInfo> pItem = pUnit->GetRPG()->GetCannonItemInfo();

	pItemModel->Set( pItem, NDb::CAMERA_SLOT, pUnit );   // the owning unit -> live "familiarity" in the tooltip

	CPtr<NRPG::IClipItem> pRPGClipItem = pItem->GetInnerClip();
	pReload->Set( pRPGClipItem->GetDBItem() );

	WCHAR wsBuffer[1024];
	swprintf( wsBuffer, L"<font face=Courier size=16pt><center>%d/%d", pRPGClipItem->GetIncQuantity(), pRPGClipItem->GetMaxIncQuantity() );
	pWeaponAmmoText->SetText( wsBuffer );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInfoPanelSingleUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInfoPanelSingleUnit: public CWindow
{
	OBJECT_BASIC_METHODS(CInfoPanelSingleUnit)
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	////
	CPtr<NGame::IUnitTracker> pUnit;
	////
	CPtr<CMLText> pAP;
	CObj<CUnitFace> pUnitFace;
	CObj<CInfoPanelSlot> pLeftSlot;
	CObj<CInfoPanelSlot> pRightSlot;
	CObj<CInfoPanelSpecialSlot> pSpecialSlot;
	////
	vector<CObj<CInfoPanelCritical> > criticalIconsSet;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pUnit); f.Add(4,&pAP); f.Add(5,&pUnitFace); f.Add(6,&pLeftSlot); f.Add(7,&pRightSlot); f.Add(8,&pSpecialSlot); f.Add(9,&criticalIconsSet); return 0; }

public:
	CInfoPanelSingleUnit() {}
	CInfoPanelSingleUnit( const SWindowInfo &sInfo, NGame::IMission *pMission );

	// release CInfoPanelSingleUnit::PlayAckEvent @0x256360 -> forward to the unit face's ack animation
	void PlayAckEvent( const STime &sTime, NUI::CAckEvent *pEvent ) { if ( IsValid( pUnitFace ) ) pUnitFace->PlayAckEvent( sTime, pEvent ); }

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CInfoPanelSingleUnit::CInfoPanelSingleUnit( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInfoPanelSingleUnit::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "face" )
			{
				pMission->FocusCameraOnUnit( pUnit->GetUnit() );
				return true;
			}
			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pAP = new CMLText( sEvent.pLoader->GetControl( "ap" ) );
			pUnitFace = new CUnitFace( sEvent.pLoader->GetControl( "face" ), pMission );
			pLeftSlot = new CInfoPanelSlot( sEvent.pLoader->GetControl( "slot_left" ), pMission, NDb::SLOT_1 );
			pRightSlot = new CInfoPanelSlot( sEvent.pLoader->GetControl( "slot_right" ), pMission, NDb::SLOT_2 );
			pSpecialSlot = new CInfoPanelSpecialSlot( sEvent.pLoader->GetControl( "slot_double" ), pMission );

			criticalIconsSet.resize( N_NUM_CRITICALS_ICONS );
			for ( int nTemp = 0; nTemp < N_NUM_CRITICALS_ICONS; nTemp++ )
				criticalIconsSet[nTemp] = new CInfoPanelCritical( sEvent.pLoader->GetControl( NStr::Format( "critical_%d", nTemp ) ) );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelSingleUnit::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	pUnit = unitsSet[0];

	UpdateCriticalIcons( pUnit, criticalIconsSet ); // release @0x1cda90 shared helper (iCriticalIcons)


	CPtr<NRPG::IWeaponItemInfo> pItem = pUnit->GetUnit()->GetRPG()->GetCannonItemInfo();
	pSpecialSlot->Set( pUnit->GetUnit() );
	pSpecialSlot->SetStyle( STYLE_VISIBLE, IsValid( pItem ) );

	pAP->SetStyle( STYLE_VISIBLE, !pMission->IsRealTime() );
	if ( !pMission->IsRealTime() )
	{
		NRPG::SUnitInfo sUnitInfo;
		pUnit->GetUnit()->GetInfo( &sUnitInfo );

		WCHAR wsBuffer[256];
		swprintf( wsBuffer, L"<color=FFB4997C><font face=Impact size=36pt outlinesize=2 outlinecolor=FF513E2B><wrapright>%d<br><left>AP", sUnitInfo.nAP );
		pAP->SetText( wsBuffer );
	}

	pLeftSlot->Set( pUnit->GetUnit() );
	pRightSlot->Set( pUnit->GetUnit() );
	pSpecialSlot->Set( pUnit->GetUnit() );
	// while an ack is playing, the face is showing the SPEAKER (state machine owns it); don't yank it
	// back to the selected unit every frame -- the ack's ST_SOURCE_SHOW step restores it on finish.
	if ( !pUnitFace->IsPlayingAck() )
		pUnitFace->SetUnit( pUnit );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInfoPanelMultipleUnits
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInfoPanelMultipleUnits: public CWindow
{
	OBJECT_BASIC_METHODS(CInfoPanelMultipleUnits)
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	CPtr<NGame::IUnitTracker> pUnit;
	////
	vector<CPtr<CImage> > selectionsSet;
	vector<CObj<CUnitFace> > facesSet;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pUnit); f.Add(4,&selectionsSet); f.Add(5,&facesSet); return 0; }

public:
	CInfoPanelMultipleUnits() {}
	CInfoPanelMultipleUnits( const SWindowInfo &sInfo, NGame::IMission *pMission );

	void Set( NGame::IUnitTracker *pUnit );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CInfoPanelMultipleUnits::CInfoPanelMultipleUnits( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission ), selectionsSet( N_MAXUNITS_COUNT ), facesSet( N_MAXUNITS_COUNT )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInfoPanelMultipleUnits::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			for ( int nTemp = 0; nTemp < facesSet.size(); nTemp++ )
			{
				facesSet[nTemp] = new CUnitFace( sEvent.pLoader->GetControl( NStr::Format( "hero_%d", ( nTemp + 1 ) ) ), pMission );
				facesSet[nTemp]->SetUnit( 0 );
			}
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			for ( int nTemp = 0; nTemp < facesSet.size(); nTemp++ )
				selectionsSet[nTemp] = GetUIWindow<CImage>( this, NStr::Format( "hero_%d_selection", ( nTemp + 1 ) ) );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelMultipleUnits::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetUnits( &unitsSet );
	for ( int nTemp = 0; nTemp < facesSet.size(); nTemp++ )
	{
		if ( nTemp < unitsSet.size() )
		{
			facesSet[nTemp]->SetUnit( unitsSet[nTemp] );
			facesSet[nTemp]->SetStyle( STYLE_VISIBLE, true );
			selectionsSet[nTemp]->SetStyle( STYLE_VISIBLE, unitsSet[nTemp]->IsSelected() );
		}
		else
		{
			facesSet[nTemp]->SetStyle( STYLE_VISIBLE, false );
			selectionsSet[nTemp]->SetStyle( STYLE_VISIBLE, false );
		}
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLevelSwitchBar
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLevelSwitchBar: public CWindow
{
	OBJECT_BASIC_METHODS(CLevelSwitchBar)
private:
	// retail CLevelSwitchBar (ctor @0x258cc0, Draw @0x254420): ALWAYS 8 buttons (geometry from the
	// "levelswitch" template), but each button's image state is recomputed EVERY Draw from the live
	// cut-floor range: floor_i = min+i; below-ground floors use the basement textures. That is what
	// makes the bar look "refitted" per map (a 2-floor map renders 2 live slots + 6 hidden-texture
	// slots). Slot -> floor mapping is min+i, NOT the old fixed i-N_BASELEVEL.
	enum
	{
		STATE_FLOOR_VISIBLE    = 0,
		STATE_FLOOR_HIDDEN     = 1,
		STATE_BASEMENT_VISIBLE = 2,
		STATE_BASEMENT_HIDDEN  = 3
	};
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	////
	CPtr<CButton> pUp;
	CPtr<CButton> pDown;
	vector<CPtr<CButton> > buttonsSet;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pUp); f.Add(4,&pDown); f.Add(5,&buttonsSet); return 0; }

public:
	CLevelSwitchBar() {}
	CLevelSwitchBar( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CLevelSwitchBar::CLevelSwitchBar( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission ), buttonsSet( N_MAXLEVELS_COUNT )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLevelSwitchBar::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			// retail: up/down step the cut floor; level_N maps slot->floor as rangeMin + (N-1).
			// Clamping is fully delegated to the scene's SetCutFloor (@0xd0050 range clamp).
			int nRangeMin = 0, nRangeMax = 0;
			pMission->GetScene()->GetCutFloorRange( &nRangeMin, &nRangeMax );
			if ( sEvent.szID.compare( "up" ) == 0 )
			{
				pMission->GetScene()->SetCutFloor( pMission->GetScene()->GetCutFloor() + 1 );
				return true;
			}
			if ( sEvent.szID.compare( "down" ) == 0 )
			{
				pMission->GetScene()->SetCutFloor( pMission->GetScene()->GetCutFloor() - 1 );
				return true;
			}
			for ( int nBtn = 0; nBtn < N_MAXLEVELS_COUNT; ++nBtn )
			{
				if ( sEvent.szID.compare( NStr::Format( "level_%d", nBtn + 1 ) ) == 0 )
				{
					pMission->GetScene()->SetCutFloor( nRangeMin + nBtn );
					return true;
				}
			}
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pUp = GetUIWindow<CButton>( this, "up" );
			pUp->AddImageState( 0, NDb::GetUITexture( 399 ) );
			pDown = GetUIWindow<CButton>( this, "down" );
			pDown->AddImageState( 0, NDb::GetUITexture( 398 ) );

			// retail: every button carries ALL FOUR image states -- which slot is a basement
			// depends on the LIVE range (min+i < 0), decided per frame in Draw, not per slot here.
			for ( int nTemp = 0; nTemp < buttonsSet.size(); nTemp++ )
			{
				buttonsSet[nTemp] = GetUIWindow<CButton>( this, NStr::Format( "level_%d", ( nTemp + 1 ) ) );
				buttonsSet[nTemp]->AddImageState( STATE_FLOOR_VISIBLE,    NDb::GetUITexture( 404 ) );
				buttonsSet[nTemp]->AddImageState( STATE_FLOOR_HIDDEN,     NDb::GetUITexture( 406 ) );
				buttonsSet[nTemp]->AddImageState( STATE_BASEMENT_VISIBLE, NDb::GetUITexture( 405 ) );
				buttonsSet[nTemp]->AddImageState( STATE_BASEMENT_HIDDEN,  NDb::GetUITexture( 407 ) );
			}

			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLevelSwitchBar::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	// retail Draw @0x254420: floor_i = rangeMin + i; basement (floor_i < 0) picks the basement
	// texture pair; floors above the current cut render "hidden". With the base's [0,1] range this
	// gives 2 live slots + 6 hidden slots -- the "refitted" bar look.
	int nCut = pMission->GetScene()->GetCutFloor();
	int nRangeMin = 0, nRangeMax = 0;
	pMission->GetScene()->GetCutFloorRange( &nRangeMin, &nRangeMax );

	for ( int nTemp = 0; nTemp < buttonsSet.size(); nTemp++ )
	{
		int nFloor = nRangeMin + nTemp;
		if ( nFloor < 0 )
			buttonsSet[nTemp]->SetActiveState( nFloor > nCut ? STATE_BASEMENT_HIDDEN : STATE_BASEMENT_VISIBLE );
		else
			buttonsSet[nTemp]->SetActiveState( nFloor > nCut ? STATE_FLOOR_HIDDEN : STATE_FLOOR_VISIBLE );
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitPanel
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitPanel::CUnitPanel( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release CUnitPanel::PlayAckEvent @0x2563e0: forward the ack to the single-unit face panel.
void CUnitPanel::PlayAckEvent( const STime &sTime, CAckEvent *pEvent )
{
	if ( IsValid( pInfoPanelSingleUnit ) )
		pInfoPanelSingleUnit->PlayAckEvent( sTime, pEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitPanel::ProcessMessage( const SEvent &sEvent )
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
			pUnitIconsBar = new CUnitIconsBar( sEvent.pLoader->GetControl( "uniticonbar" ), pMission );
			pLevelSwitchBar = new CLevelSwitchBar( sEvent.pLoader->GetControl( "levelswitch" ), pMission );

			pUnitsTabBar = new CUnitsTabBar( sEvent.pLoader->GetControl( "unitstabbar" ), pMission );
			pInfoPanelSingleUnit = new CInfoPanelSingleUnit( sEvent.pLoader->GetControl( "infopanel_singleunit" ), pMission );
			pInfoPanelMultipleUnits = new CInfoPanelMultipleUnits( sEvent.pLoader->GetControl( "infopanel_multipleunits" ), pMission );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pEndOfTurn = GetUIWindow<CButton>( this, "endofturn" );
			pEndOfTurn->AddImageState( 0, NDb::GetUITexture( 379 ) );

			pStartOfTurn = GetUIWindow<CButton>( this, "startofturn" );
			pStartOfTurn->AddImageState( 0, NDb::GetUITexture( 469 ) );

			pBackgroundSingleUnit = GetUIWindow<CImage>( this, "background_single" );
			pBackgroundMultipleUnits = GetUIWindow<CImage>( this, "background_multi" );
			// release @0x2592c0: the THIRD backdrop, "background_empty" (retail-added ctrl 2726,
			// tex 962, depth 2, template-VISIBLE). It sits ABOVE background_single/multi, so if it
			// is never toggled off it covers them with its plain plate every frame -- wiping the
			// portrait frame + criticals-grid art that is baked into background_single's texture.
			// (Control exists in the retail/Steam game.db the runtime loads; a dev-generated db
			// lacks it -> the fetch logs a "not found" UI-ERROR dummy there, which is harmless.)
			pBackgroundEmpty = GetUIWindow<CImage>( this, "background_empty" );
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
void CUnitPanel::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	int nCountSelected = pMission->CountSelected();

	bool bSignlePanel = false, bMultiPanel = false;
	if ( nCountSelected == 1 )
		bSignlePanel = true;
	else if ( nCountSelected > 1 )
		bMultiPanel = true;

	pEndOfTurn->SetStyle( STYLE_VISIBLE, !pMission->IsRealTime() );
	pStartOfTurn->SetStyle( STYLE_VISIBLE, pMission->IsRealTime() );

	pInfoPanelSingleUnit->SetStyle( STYLE_VISIBLE, bSignlePanel );
	pBackgroundSingleUnit->SetStyle( STYLE_VISIBLE, bSignlePanel );

	pInfoPanelMultipleUnits->SetStyle( STYLE_VISIBLE, bMultiPanel );
	pBackgroundMultipleUnits->SetStyle( STYLE_VISIBLE, bMultiPanel );

	// release CUnitPanel::Draw @0x2542e0: the 3-way backdrop toggle -- the empty plate shows only
	// with NO selection; without this it stays template-visible and overdraws the other two.
	pBackgroundEmpty->SetStyle( STYLE_VISIBLE, nCountSelected == 0 );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0521151, CUnitTab );
REGISTER_SAVELOAD_CLASS( 0xB0521152, CUnitsTabBar );
REGISTER_SAVELOAD_CLASS( 0xB0521153, CUnitFace );
REGISTER_SAVELOAD_CLASS( 0xB0521155, CInfoPanelSlot );
REGISTER_SAVELOAD_CLASS( 0xB0521156, CInfoPanelSingleUnit );
REGISTER_SAVELOAD_CLASS( 0xB0521157, CInfoPanelMultipleUnits );
REGISTER_SAVELOAD_CLASS( 0xB0521158, CUnitPanel );
REGISTER_SAVELOAD_CLASS( 0xB0521159, CSlotReloadButton );
REGISTER_SAVELOAD_CLASS( 0xB052115A, CInfoPanelSpecialSlot );
REGISTER_SAVELOAD_CLASS( 0xB0241943, CLevelSwitchBar );
// 0xB0241944 CInfoPanelCritical + 0xB3515150 CMissionCriticalBleedingFake are registered in iCriticalIcons.cpp
