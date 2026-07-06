#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "wInterface.h"
#include "wMisc.h"			// NWorld::GetDMeshUnit -- clue ("ear") markers over heard-not-seen units
#include "RPGItemInfo.h"
#include "..\Misc\StrProc.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataAck.h"
#include "..\DBFormat\DataInterface.h"
#include "..\DBFormat\DataRPG.h"
#include "Sound.h"
#include "RWGame.h"			// NRender::IRenderGame::GetHeadController (the shared portrait heads controller)
#include "LSController.h"	// NLSHead::CHeadsController::PlaySequence -- drives the HUD portrait's mouth/gesture
#include "iMission.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iMissionUI.h"
#include "iMissionExec.h"
#include "iLogPanel.h"
#include "iTopPanel.h"
#include "iUnitPanel.h"
#include "iPerksPanel.h"
#include "iStorePanel.h"
#include "iInventoryPanel.h"
#include "iCharacterPanel.h"
#include "iMedalsPanel.h"
#include "iBiographyPanel.h"
#include "iActionDecorator.h"
#include "UIWrap.h"
#include "rpgUnitInfo.h"
#include "RPGUnit.h"        // NRPG::CUnit complete type (GetRPGUnit()->GetVoice() for the in-game ack voice)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_ASK_TTL = 3000;
const int
	N_LOGPANEL_PAD = 20; // pad-zone in points
const int
	N_SCROLL_STEP				= 4,
	N_SCROLL_GUARDBAND	= 4;
const int
	N_HITPTRACKER_TTL		= 2000;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAckIcon
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckIcon: public CWindow
{
	OBJECT_NOCOPY_METHODS(CAckIcon);
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	////
	CPtr<CAckEvent> pEvent;
	////
	CPtr<CMLText> pText;
	CPtr<CUnitHead> pHead;
	CPtr<CImage> pStrip;	// the dark subtitle band the text sits on (retail: CAckView's own frame skin)
	bool bPlayAck = false;	// retail CAckView +bPlayAck: latched in Set, consumed by Draw once bReady flips
	CObj<NSound::ISound2D> pSound;	// the deferred 2D voice handle (retail CAckView::pSound, transient)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pEvent); f.Add(4,&pText); f.Add(5,&pHead); f.Add(6,&pStrip); f.Add(7,&bPlayAck); return 0; }
	// retail CMissionUI::Update @0x211d60 (disasm @0x612176..0x6121f7) repositions the ack view EVERY
	// frame: x = clientRect.x1 + 100, width = (clientRect.x2 - 100) - (clientRect.x1 + 100), and the
	// BOTTOM anchor = the inventory button's bottom edge (pInventory pos.y + size.y, desktop coords).
	// CMissionUI::Update pushes those three numbers here each frame (runtime-only -- recomputed per
	// frame, so kept OUT of operator& to preserve the save format); Draw's reflow consumes them.
	int nStripX = 0;
	int nStripWidth = 0;
	int nStripBottom = 0;

public:
	CAckIcon() {}
	CAckIcon( const SWindowInfo &sInfo, NGame::IMission *_pMission ): CWindow( sInfo ), pMission( _pMission ) {}

	void Set( CAckEvent *pEvent );
	void PlayAck();	// retail CAckView::PlayAck @0x210ff0 -- deferred voice + heads-controller lipsync
	// retail ack reposition (CMissionUI::Update @0x211d60 tail): the subtitle band sits between the
	// clue-icon column and the inventory button, bottom-anchored to the inventory button's bottom.
	void SetStripPlacement( int nX, int nWidth, int nBottom ) { nStripX = nX; nStripWidth = nWidth; nStripBottom = nBottom; }
	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline wstring ConvertLineBreaks( const wstring &szStr )
{
	wstring szRet;
	for ( wstring::const_iterator i = szStr.begin(); i != szStr.end(); )
	{
		switch ( wchar_t(*i) )
		{
			case L'\n':
				szRet += L"<br>";
				break;
			case L'\r':
				szRet += L"<br>";
				++i;
				if ( i != szStr.end() && *i == L'\n' )
					++i;
				continue;
			case 133: // symbol L'�'
				szRet += L"...";
				break;
			default:
				szRet += *i;
				break;
		}
		++i;
	}
	return szRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAckIcon::Set( CAckEvent *_pEvent )
{
	pEvent = _pEvent;

	NWorld::CAckEvent *pAckEvent = pEvent->GetAckEvent();
	if ( !IsValid( pAckEvent->pAckInfo ) )
		return;

	if ( pAckEvent->pUnit )
	{
		// retail CAckView::Set @0x210e10: bind the head + latch bPlayAck here, but DEFER the voice +
		// lipsync to PlayAck (fired from Draw once the ack's bReady handshake flips) so the mouth
		// moves in lock-step with the portrait turning to camera. The immediate PlaySound + SetSequence
		// were removed -- they fired before the speaker's face was on screen.
		pHead->SetUnit( pAckEvent->pUnit );
		bPlayAck = true;
	}

	// retail CAckView::Set @0x210e10 (disasm @0x610ea3..0x610f4f): the subtitle is FIVE pieces --
	//   L"<minfontsize size=16>"                                (static literal @VA 0x8be9d8)
	//   + GetDBString( 19329 )   "EnemyTooltip Name Format"  =  "<font face=Courier size=16pt><color=yellow>"
	//   + <speaker name>
	//   + GetDBString( 20257 )   "Ack Format"                =  "<color=beige>: "
	//   + ConvertLineBreaks( <ack body text> )
	// -> Courier 16pt, LEFT-aligned, YELLOW name, beige ": " + body. (The old 11209 dialog prefix
	// carried "<font face=Impact size=24pt>...<center>" -- that's what made the dev line huge and
	// centered with no yellow name.)
	wstring wsSubtitle = wstring( L"<minfontsize size=16>" ) + GetDBString( 19329 );
	if ( pAckEvent->pUnit && IsValid( pAckEvent->pUnit->GetRPG() ) )
		wsSubtitle += pAckEvent->pUnit->GetRPG()->GetRPGUnit()->GetName();
	wsSubtitle += GetDBString( 20257 );
	wsSubtitle += ConvertLineBreaks( GetDBString( pAckEvent->pAckInfo->pText ) );
	if ( IsValid( pText ) )
	{
		pText->SetText( wsSubtitle );
		pText->SetStyle( STYLE_VISIBLE, true );
	}
	if ( IsValid( pStrip ) )
		pStrip->SetStyle( STYLE_VISIBLE, true );

	SetStyle( STYLE_VISIBLE, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NUI::CAckView::PlayAck @0x210ff0: fired from Draw once the ack is bReady. Start the 2D voice
// and route the phrase's face SEQUENCE through the SHARED heads controller (retail
// CRenderGame::PlaySequence @0x2cb150 -> NLSHead::CHeadsController::PlaySequence, LSController.cpp:67)
// -- the SAME controller that blinks the portrait -- so the bottom-left 3D head MOVES ITS MOUTH in
// step with the voice. (The dev's old GetAnimation(0/1) path was a dead no-op: records 0/1 are CUT
// content in the retail game.db.)
void CAckIcon::PlayAck()
{
	NWorld::CAckEvent *pAckEvent = pEvent->GetAckEvent();
	if ( !IsValid( pAckEvent->pAckInfo ) || !IsValid( pAckEvent->pUnit ) )
		return;

	const NDb::SAckVoice &voice = pAckEvent->pAckInfo->GetVoice( pAckEvent->pUnit->GetRPG()->GetRPGUnit()->GetVoice() );
	pSound = GetInterface()->GetSound()->Add2DSound( voice.pSound );
	pHead->SetSequence( voice.pSequence );
	if ( IsValid( pMission ) )
		pMission->GetRenderGame()->GetHeadController()->PlaySequence( pAckEvent->pUnit, voice.pSequence, NDb::GetSequenceByExpression( voice.eExpression ), false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAckIcon::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
		case EVENT_LBUTTONDOWN:
			return true;
		case EVENT_LBUTTONUP:
			pEvent->Cancel();
			return true;
		case EVENT_MOUSEMOVE:
			GetInterface()->SetCursorInfo( SCursorInfo() );
			break;
		case EVENT_TEMPLATELOAD:
			pHead = new CUnitHead( sEvent.pLoader->GetControl( "face" ), pMission->GetRenderGame(), 1.8f );
			break;
		case EVENT_TEMPLATELOADCOMPLETE:
			{
				// retail CAckView ctor @0x210050 creates its subtitle text IN CODE ("acktext", the
				// IML/markup CText, style 0xe, pos/size {0,0}) -- the game.db "ack" container has NO
				// "text" child. Retail's view itself is a CFrame ("ackview", style 0x2e, pos/size
				// {0,0}) parented DIRECTLY to CMissionUI (@0x214d40) and repositioned every frame by
				// CMissionUI::Update; the dark band is that frame's skin. Here it is a translucent
				// black CImage on the SAME parent (the mission desktop, so the retail desktop-space
				// coords apply verbatim); geometry is all zero at creation -- the per-frame
				// SetStripPlacement + the Draw reflow (retail LAB_006111a9) lay it out.
				pStrip = new CImage( SWindowInfo( GetParent(), SPoint( 0, 0 ), SPoint( 0, 0 ), "ackstrip", STYLE_ENABLED | STYLE_TOPMOST ) );
				pStrip->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xA0 ) );
				pStrip->SetStyle( STYLE_VISIBLE, false );
				pText = new CMLText( SWindowInfo( pStrip, SPoint( 4, 4 ), SPoint( 0, 0 ), "acktext", STYLE_ENABLED | STYLE_TRANSPARENT | STYLE_TOPMOST ) );
				pText->SetStyle( STYLE_VISIBLE, false );
			}
			break;
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAckIcon::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	// retail NUI::CAckView::Update @0x2110e0: run the DEFERRED voice/lipsync, then age the ack out.
	bool bAlive = false;
	if ( IsValid( pEvent ) && !pEvent->IsComplete( sTime ) )
	{
		NWorld::CAckEvent *pAckEvent = pEvent->GetAckEvent();
		if ( !IsValid( pAckEvent ) || !IsValid( pAckEvent->pUnit ) )
			pEvent->Cancel();

		// bReady flips when the single-unit face turns to camera (CUnitFace ACK_WAIT) or when its
		// fallback fires -- only THEN start the voice and route the mouth/gesture through the heads
		// controller. (retail CAckView::Update: bPlayAck && pEvent->bReady -> PlayAck.)
		if ( bPlayAck && pEvent->IsReady() )
		{
			bPlayAck = false;
			PlayAck();
		}
		// once the voice has been kicked off (or there was none), retire on TTL after the sound stops.
		if ( !bPlayAck && pEvent->IsTTLComplete( sTime ) && ( !IsValid( pSound ) || !pSound->IsPlaying() ) )
			pEvent->SetComplete( true );

		bAlive = !pEvent->IsComplete( sTime );
	}
	if ( !bAlive )
	{
		// retail Update tail: release the completed event + its voice handle.
		pEvent = 0;
		pSound = 0;
	}

	SetStyle( STYLE_VISIBLE, bAlive );
	if ( IsValid( pText ) )
		pText->SetStyle( STYLE_VISIBLE, bAlive );	// subtitle lives exactly as long as the ack
	if ( IsValid( pStrip ) )
	{
		pStrip->SetStyle( STYLE_VISIBLE, bAlive );
		if ( bAlive && IsValid( pText ) && nStripWidth > 0 )
		{
			// retail CAckView re-flow (CAckView::Update @0x2110e0, LAB_006111a9): wrap width = band
			// width - 8, measure, band height = text height + 8, grow UPWARD from the bottom anchor.
			// Band x/width and the bottom anchor (the inventory button's bottom edge) come from
			// CMissionUI::Update via SetStripPlacement -- the retail per-frame reposition
			// (@0x612176..0x6121f7: x = clientRect.x1+100, width = clientWidth-200, y = inv bottom).
			pText->SetSize( SPoint( nStripWidth - 8, 0 ) );
			SPoint sReal( 0, 0 );
			pText->GetRealSize( &sReal );
			int nBandH = sReal.y + 8;
			pStrip->SetSize( SPoint( nStripWidth, nBandH ) );
			pStrip->SetPosition( SPoint( nStripX, nStripBottom - nBandH ) );
			pText->SetSize( SPoint( nStripWidth - 8, sReal.y ) );
			pText->SetPosition( SPoint( 4, 4 ) );
		}
	}
	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CItemText
////////////////////////////////////////////////////////////////////////////////////////////////////
class CItemText: public CActionDecorator<CImage>
{
	OBJECT_BASIC_METHODS(CItemText)
private:
	ZDATA_(TBaseClass)
	CPtr<CMissionUI> pMissionUI;
	CPtr<NGame::IMission> pMission;
	////
	NWorld::SItem sItem;
	CObj<CTextDraw> pText;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); f.Add(2,&pMissionUI); f.Add(3,&pMission); f.Add(4,&sItem); f.Add(5,&pText); return 0; }

public:
	CItemText() {}
	CItemText( const SWindowInfo &sInfo, NGame::IMission *pMission, CMissionUI *pMissionUI, const NWorld::SItem &sItem );

	bool CanHandleState( NGame::IState *pState ) const;
	CObjectBase* GetTarget();

	const NWorld::SItem& GetItem() const { return sItem; }
	const SPoint& GetRealSize( NGScene::I2DGameView *pView ) { return pText->GetSize( pView ); }

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CItemText::CItemText( const SWindowInfo &sInfo, NGame::IMission *_pMission, CMissionUI *_pMissionUI, const NWorld::SItem &_sItem ):
	TBaseClass( sInfo, _pMission ), pMission( _pMission ), pMissionUI( _pMissionUI ), sItem( _sItem )
{
	wstring wsText( L"<font face=Courier size=16pt><color=white>[UNKNOWN]" );
	if ( sItem.pItem->GetDBItem()->pName )
		wsText = L"<font face=Courier size=16pt><color=white>" + sItem.pItem->GetDBItem()->pName->szStr;

	pText = new CTextDraw( SPoint( 0, 0 ), SPoint( -1, -1 ), wsText );
	SetColor( NGfx::SPixel8888( 0x1F, 0x1F, 0x1F, 0xDF ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemText::CanHandleState( NGame::IState *pState ) const
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CItemText::GetTarget()
{
	return CDynamicCast<CObjectBase>( sItem.pWorldItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemText::ProcessMessage( const SEvent &sEvent )
{
	switch ( sEvent.nEvent )
	{
	case EVENT_MOUSEENTER:
		{
			SetColor( NGfx::SPixel8888( 0x1F, 0x1F, 0xDF, 0xFF ) );
			break;
		}
	case EVENT_MOUSEEXIT:
		{
			SetColor( NGfx::SPixel8888( 0x1F, 0x1F, 0x1F, 0xDF ) );
			break;
		}
	}

	return TBaseClass::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemText::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	SetSize( pText->GetSize( pView ) );

	TBaseClass::Draw( sTime, pView );

	pText->Draw( this, sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CItemText
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEnemyIcon: public CActionDecorator<CImage>
{
	OBJECT_BASIC_METHODS(CEnemyIcon)
private:
	ZDATA_(TBaseClass)
	CPtr<CMissionUI> pMissionUI;
	CPtr<NGame::IMission> pMission;
	////
	bool bOwner;
	float fAngle;
	CPtr<CImage> pImage;
	CPtr<NWorld::CUnit> pEnemy;
	CDBPtr<NDb::CUITexture> pTexture;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); f.Add(2,&pMissionUI); f.Add(3,&pMission); f.Add(4,&bOwner); f.Add(5,&fAngle); f.Add(6,&pImage); f.Add(7,&pEnemy); f.Add(8,&pTexture); return 0; }

public:
	CEnemyIcon() {}
	CEnemyIcon( const SWindowInfo &sInfo, NGame::IMission *pMission, CMissionUI *pMissionUI );

	bool CanHandleState( NGame::IState *pState ) const;
	CObjectBase* GetTarget();

	void SetPosition( const SPoint &_sPosition );

	void Set( NWorld::CUnit *pEnemy, bool bOwner, float _fAngle );
	NWorld::CUnit* GetUnit() const;

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CEnemyIcon::CEnemyIcon( const SWindowInfo &sInfo, NGame::IMission *_pMission, CMissionUI *_pMissionUI ):
	TBaseClass( sInfo, _pMission ), pMission( _pMission ), pMissionUI( _pMissionUI )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CEnemyIcon::CanHandleState( NGame::IState *pState ) const
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CEnemyIcon::GetTarget()
{
	return pEnemy;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEnemyIcon::SetPosition( const SPoint &_sPosition )
{
	TBaseClass::SetSize( SPoint( 0, 0 ) );
	TBaseClass::SetPosition( _sPosition );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEnemyIcon::Set( NWorld::CUnit *_pEnemy, bool _bOwner, float _fAngle )
{
	int pNormalIcons[8] = { 446, 447, 448, 449, 450, 451, 452, 453 };
	int pDisabledIcons[8] = { 454, 455, 456, 457, 458, 459, 460, 461 };

	fAngle = _fAngle;
	bOwner = _bOwner;
	pEnemy = _pEnemy;

	int nID = min( max( Float2Int( fAngle / 45 ), 0 ), 7 );

	if ( bOwner )
	{
		if ( fAngle == -1 )
			pTexture = NDb::GetUITexture( 464 );
		else
			pTexture = NDb::GetUITexture( pNormalIcons[nID] );
	}
	else
	{
		if ( fAngle == -1 )
			pTexture = NDb::GetUITexture( 463 );
		else
			pTexture = NDb::GetUITexture( pDisabledIcons[nID] );
	}

	SetImage( pTexture );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CEnemyIcon::ProcessMessage( const SEvent &sEvent )
{
	switch ( sEvent.nEvent )
	{
	case EVENT_RBUTTONDOWN:
		return true;
	case EVENT_RBUTTONUP:
		pMission->FocusCameraOnUnit( pEnemy );
		return true;
	}

	return TBaseClass::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEnemyIcon::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	SPoint sNewSize( pTexture->nWidth, pTexture->nHeight );
	SPoint sSize = GetSize();
	SPoint sPosition = GetPosition();

	// position/clamp are CLIENT-window-local now (icons are view children, retail @0x213e70)
	const SPoint &sParentSize = pMissionUI->GetClientWindow()->GetSize();
	SRect sViewRect( 0, 0, sParentSize.x, sParentSize.y );
	sPosition.x = min( max( sViewRect.x1 + sNewSize.x / 2, sPosition.x ), sViewRect.x2 - sNewSize.x / 2 );
	sPosition.y = min( max( sViewRect.y1 + sNewSize.y / 2, sPosition.y ), sViewRect.y2 - sNewSize.y / 2 );

	TBaseClass::SetSize( sNewSize );
	TBaseClass::SetPosition( SPoint( sPosition.x + sSize.x / 2 - sNewSize.x / 2, sPosition.y + sSize.y / 2 - sNewSize.y / 2 ) );

	TBaseClass::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CClueIcon (retail name: NUI::CSoundIcon) -- the "ear" marker drawn over a heard-not-seen noise.
// Textures = the retail Sound Icons folder (CSoundIcon::Draw @0x210960 disasm): on-screen ear 674
// 'Icon - OnScreen', off-screen 8-direction arrows {666,672,673,667,668,669,670,671} for buckets
// 0g,45g..315g. (The 675-683 set is the UNRELATED scenario-clue key/bell marker of retail's real
// CClueIcon @0x2102e0.) The icon anchors on the SOUND MARKER (CDMesh) position -- never on the
// live unit -- and its action target is THE MARKER (retail CSoundIcon::GetTarget @0x2165a0), so
// hovering/attacking through the icon can't spoil the hidden unit's identity or movement.
class CClueIcon: public CActionDecorator<CImage>
{
	OBJECT_BASIC_METHODS(CClueIcon)
private:
	ZDATA_(TBaseClass)
	CPtr<CMissionUI> pMissionUI;
	CPtr<NGame::IMission> pMission;
	////
	float fAngle;
	CPtr<NWorld::CUnit> pUnit;
	CDBPtr<NDb::CUITexture> pTexture;
	CPtr<CObjectBase> pMarker;	// the heard-noise CDMesh this ear sits over (the action target)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); f.Add(2,&pMissionUI); f.Add(3,&pMission); f.Add(4,&fAngle); f.Add(5,&pUnit); f.Add(6,&pTexture); f.Add(7,&pMarker); return 0; }

public:
	CClueIcon() {}
	CClueIcon( const SWindowInfo &sInfo, NGame::IMission *pMission, CMissionUI *pMissionUI );

	bool CanHandleState( NGame::IState *pState ) const;
	CObjectBase* GetTarget();

	void SetPosition( const SPoint &_sPosition );

	void Set( CObjectBase *pMarker, NWorld::CUnit *pUnit, float _fAngle );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CClueIcon::CClueIcon( const SWindowInfo &sInfo, NGame::IMission *_pMission, CMissionUI *_pMissionUI ):
	TBaseClass( sInfo, _pMission ), pMission( _pMission ), pMissionUI( _pMissionUI )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CClueIcon::CanHandleState( NGame::IState *pState ) const
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CClueIcon::GetTarget()
{
	// retail CSoundIcon::GetTarget @0x2165a0 returns the SOUND MARKER -- handing the unit to the
	// states here would re-open the identity/death spoil the trace-side fix closed.
	return pMarker;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CClueIcon::SetPosition( const SPoint &_sPosition )
{
	TBaseClass::SetSize( SPoint( 0, 0 ) );
	TBaseClass::SetPosition( _sPosition );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CClueIcon::Set( CObjectBase *_pMarker, NWorld::CUnit *_pUnit, float _fAngle )
{
	// retail CSoundIcon::Draw @0x210960 (disasm 0x610a04..0x610ab1): the Sound Icons (ear) set --
	// directional buckets 0g..315g map to {666,672,673,667,668,669,670,671}, on-screen ear = 674.
	int pEarIcons[8] = { 666, 672, 673, 667, 668, 669, 670, 671 };

	fAngle = _fAngle;
	pUnit = _pUnit;
	pMarker = _pMarker;

	if ( fAngle == -1 )
		pTexture = NDb::GetUITexture( 674 );
	else
	{
		int nID = min( max( Float2Int( fAngle / 45 ), 0 ), 7 );
		pTexture = NDb::GetUITexture( pEarIcons[nID] );
	}

	SetImage( pTexture );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CClueIcon::ProcessMessage( const SEvent &sEvent )
{
	switch ( sEvent.nEvent )
	{
	case EVENT_RBUTTONDOWN:
		return true;
	case EVENT_RBUTTONUP:
		pMission->FocusCameraOnUnit( pUnit );
		return true;
	}

	return TBaseClass::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CClueIcon::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	SPoint sNewSize( pTexture->nWidth, pTexture->nHeight );
	SPoint sSize = GetSize();
	SPoint sPosition = GetPosition();

	// position/clamp are CLIENT-window-local now (icons are view children, retail @0x213e70)
	const SPoint &sParentSize = pMissionUI->GetClientWindow()->GetSize();
	SRect sViewRect( 0, 0, sParentSize.x, sParentSize.y );
	sPosition.x = min( max( sViewRect.x1 + sNewSize.x / 2, sPosition.x ), sViewRect.x2 - sNewSize.x / 2 );
	sPosition.y = min( max( sViewRect.y1 + sNewSize.y / 2, sPosition.y ), sViewRect.y2 - sNewSize.y / 2 );

	TBaseClass::SetSize( sNewSize );
	TBaseClass::SetPosition( SPoint( sPosition.x + sSize.x / 2 - sNewSize.x / 2, sPosition.y + sSize.y / 2 - sNewSize.y / 2 ) );

	TBaseClass::Draw( sTime, pView );
}
//////////////////////////////////////////////////////////////////////////////////////
class CHitTracker: public CText
{
	OBJECT_BASIC_METHODS(CHitTracker);
private:
	ZDATA_(CText)
	CPtr<CWindow> pClientWindow;
	CPtr<NGame::IMission> pMission;
	////
	int nHitValue;
	bool bComplete;
	CVec3 vBegPoint;
	STime sBegTime;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CText*)this); f.Add(2,&pClientWindow); f.Add(3,&pMission); f.Add(4,&nHitValue); f.Add(5,&bComplete); f.Add(6,&vBegPoint); f.Add(7,&sBegTime); return 0; }

public:
	CHitTracker() {}
	CHitTracker( const SWindowInfo &sInfo, NGame::IMission *pMission, NWorld::CHitLocator *pLocator, const STime &sTime );

	bool IsComplete() const;

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CHitTracker::CHitTracker( const SWindowInfo &sInfo, NGame::IMission *_pMission, NWorld::CHitLocator *pLocator, const STime &sTime ):
	CText( sInfo ), pMission( _pMission ), sBegTime( sTime ), nHitValue( pLocator->nHitValue ), vBegPoint( pLocator->vPosition ), bComplete( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CHitTracker::IsComplete() const
{
	return bComplete;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHitTracker::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( bComplete )
		return;

	bComplete = true;

	CVec2 vScreenPoint;
	CVec2 vScreenRect = pMission->GetScene()->GetScreenRect();
	CTransformStack sTS = pMission->GetCameraTransform();
	if ( !TestRayInFrustrum( vBegPoint, &sTS, vScreenRect, &vScreenPoint ) )
		return;

	vScreenPoint.x = vScreenPoint.x * 1024 / vScreenRect.x;
	vScreenPoint.y = vScreenPoint.y * 768 / vScreenRect.y;

	SPoint sPosition;
	GetParent()->ScreenToClient( SPoint( vScreenPoint.x, vScreenPoint.y ), &sPosition );

	float fWeight = float( sTime - sBegTime ) / N_HITPTRACKER_TTL;
	if ( fWeight > 1 )
		return;

	sPosition.x += 40 * fWeight;
	sPosition.y += -40 * fWeight;

	WCHAR wsText[256];
	int nAlpha = ( 1 - fWeight ) * 0xFF;
	swprintf( wsText, L"<color=%.2xff0000>%d", nAlpha, nHitValue );

	SetPosition( sPosition );
	SetText( wsText );

	bComplete = false;

	CText::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMissionUI
////////////////////////////////////////////////////////////////////////////////////////////////////
CMissionUI::CMissionUI():
	bindCancel( "cancel" ),	bindShowItems( "showitems" ),
	bindPerks( "perks" ), bindStore( "store" ), bindInventory( "inventory" ), bindCharacter( "character" ),
	bindMedals( "medals" ), bindBiography( "biography" ),
	bindPoseSubMenu( "submenu_poseselect" ), bindWeaponModeSubMenu( "submenu_weaponmode" ), bindGrenadeModeSubMenu( "submenu_grenademode" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMissionUI::CMissionUI( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CDesktopWindow( sInfo ), pMission( _pMission ), sCameraScrollUpdate( 0 ),
	bindCancel( "cancel" ),	bindShowItems( "showitems" ),
	bindPerks( "perks" ), bindStore( "store" ), bindInventory( "inventory" ), bindCharacter( "character" ),
	bindMedals( "medals" ), bindBiography( "biography" ),
	bindPoseSubMenu( "submenu_poseselect" ), bindWeaponModeSubMenu( "submenu_weaponmode" ), bindGrenadeModeSubMenu( "submenu_grenademode" )

{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGame::CUICmdExec* CMissionUI::CreateExecutor( NWorld::CUICmd *pCmd )
{
	return NGame::CreateExecutor( pCmd, pMission );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NUI::ActivateCharacterSubPanel @0x20f140 -- EXCLUSIVE switching between the character
// sub-panels (skills/perks/medals/biography + store): clicking an inactive tab clears every
// character-family bit and sets only that tab; clicking the active tab closes it. Retail clears
// mask 0xFFFF, which deliberately SPARES the inventory bit (retail 0x10000) -- inventory can stay
// open next to any character panel -- so the dev clear mask is the character family, not PANEL_ALL.
static void ActivateCharacterSubPanel( NGame::IMission *pMission, NGame::EPanel ePanel )
{
	const int nCharacterFamily = NGame::PANEL_STORE | NGame::PANEL_PERKS | NGame::PANEL_CHARACTER
		| NGame::PANEL_MEDALS | NGame::PANEL_BIOGRAPHY;
	if ( pMission->GetPanelState( ePanel ) == 0 )
	{
		pMission->SetPanelState( nCharacterFamily, false );
		pMission->SetPanelState( ePanel, true );
	}
	else
		pMission->SetPanelState( ePanel, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionUI::ProcessEvent( const NInput::SEvent &sEvent )
{
	if ( !pMission->IsReady() )
		return false;

	if ( bindCancel.ProcessEvent( sEvent ) )
	{
		NGame::EActionIconsSet eIconsSet = pMission->GetActionIconsSet();
		if ( ( eIconsSet == NGame::AIS_POSES ) || ( eIconsSet == NGame::AIS_WEAPONMODES ) || ( eIconsSet == NGame::AIS_GRENADEMODES ) )
		{
			pMission->SetActionIconsSet( NGame::AIS_MAIN );
			return true;
		}
		else if ( pMission->GetPanelState( NGame::PANEL_STORE | NGame::PANEL_PERKS | NGame::PANEL_INVENTORY | NGame::PANEL_CHARACTER | NGame::PANEL_MEDALS | NGame::PANEL_BIOGRAPHY ) != 0 )
		{
			pMission->SetPanelState( NGame::PANEL_STORE | NGame::PANEL_PERKS | NGame::PANEL_INVENTORY | NGame::PANEL_CHARACTER | NGame::PANEL_MEDALS | NGame::PANEL_BIOGRAPHY, false );
			return true;
		}
	}

	if ( bindStore.ProcessEvent( sEvent ) )
	{
		pMission->SetPanelState( NGame::PANEL_STORE | NGame::PANEL_INVENTORY, false );
		return true;
	}
	if ( bindPerks.ProcessEvent( sEvent ) )
	{
		ActivateCharacterSubPanel( pMission, NGame::PANEL_PERKS );
		return true;
	}
	else if ( bindInventory.ProcessEvent( sEvent ) )
	{
		// retail CMissionUI::ProcessEvent (iMissionUI.c:542): capture prior inventory-panel state, toggle, then
		// fire the matching engine->script hook so the campaign lua can react to the inventory open/close.
		bool bWasOpen = ( pMission->GetPanelState( NGame::PANEL_INVENTORY ) != 0 );
		pMission->SetPanelState( NGame::PANEL_INVENTORY, !bWasOpen );
		pMission->Command( new NWorld::CCmdCallScriptFunction( bWasOpen ? "OnCloseInventory" : "OnOpenInventory", "" ) );
		return true;
	}
	else if ( bindCharacter.ProcessEvent( sEvent ) )
	{
		ActivateCharacterSubPanel( pMission, NGame::PANEL_CHARACTER );
		return true;
	}
	else if ( bindMedals.ProcessEvent( sEvent ) )
	{
		ActivateCharacterSubPanel( pMission, NGame::PANEL_MEDALS );
		return true;
	}
	else if ( bindBiography.ProcessEvent( sEvent ) )
	{
		ActivateCharacterSubPanel( pMission, NGame::PANEL_BIOGRAPHY );
		return true;
	}

	if ( bindPoseSubMenu.ProcessEvent( sEvent ) )
		pMission->SetActionIconsSet( NGame::AIS_POSES );
	else if ( bindWeaponModeSubMenu.ProcessEvent( sEvent ) )
		pMission->SetActionIconsSet( NGame::AIS_WEAPONMODES );
	else if ( bindGrenadeModeSubMenu.ProcessEvent( sEvent ) )
		pMission->SetActionIconsSet( NGame::AIS_GRENADEMODES );

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( pMission->GetState()->GetCursorInfo() );
			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pLogPanel = new CLogPanel( SWindowInfo( this, SPoint( 0, 0 ), SPoint( 0, 0 ), "logpanel", STYLE_ENABLED | STYLE_VISIBLE | STYLE_TOPMOST | STYLE_TRANSPARENT ), STREAM_GAME );

			pAck = new CAckIcon( sEvent.pLoader->GetControl( "ack" ), pMission );

			pTopBar = new CTopBar( sEvent.pLoader->GetControl( "topbar" ), pMission );
			pUnitPanel = new CUnitPanel( sEvent.pLoader->GetControl( "unitpanel" ), pMission );
			pPerksPanel = new CPerksPanel( sEvent.pLoader->GetControl( "perkspanel" ), pMission );
			pStorePanel = new CStorePanel( sEvent.pLoader->GetControl( "storepanel" ), pMission );
			pInventoryPanel = new CInventoryPanel( sEvent.pLoader->GetControl( "inventorypanel" ), pMission );
			pCharacterPanel = new CCharacterPanel( sEvent.pLoader->GetControl( "characterpanel" ), pMission );
			// retail @0x214d40: the medals + biography sub-panels are hosted alongside the others
			pMedalsPanel = new CMedalsPanel( sEvent.pLoader->GetControl( "medalspanel" ), pMission );
			pBiographyPanel = new CBiographyPanel( sEvent.pLoader->GetControl( "biographypanel" ), pMission );

			pInventory = new CHoverButton( sEvent.pLoader->GetControl( "inventory" ) );
			pInventory->AddImageState( CHoverButton::STATE_HOVER, NDb::GetUITexture( 395 ) );
			pInventory->AddImageState( CHoverButton::STATE_NORMAL, NDb::GetUITexture( 395 ) );
			pInventory->AddImageState( CHoverButton::STATE_DISABLED, NDb::GetUITexture( 647 ) );
			pInventory->SetCursorInfo( GetInterface()->GetDefaultCursorInfo() );

			pCharacter = new CHoverButton( sEvent.pLoader->GetControl( "character" ) );
			pCharacter->AddImageState( CHoverButton::STATE_HOVER, NDb::GetUITexture( 383 ) );
			pCharacter->AddImageState( CHoverButton::STATE_NORMAL, NDb::GetUITexture( 383 ) );
			pCharacter->AddImageState( CHoverButton::STATE_DISABLED, NDb::GetUITexture( 428 ) );
			pCharacter->SetCursorInfo( GetInterface()->GetDefaultCursorInfo() );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pPause = GetUIWindow<CText>( this, "pause" );   // "pause" is a UI_TEXT control -> CText, not CImage
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
void CMissionUI::Update( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( pMission->CountSelected() != 1 )
		pMission->SetPanelState( NGame::PANEL_STORE | NGame::PANEL_PERKS | NGame::PANEL_INVENTORY | NGame::PANEL_CHARACTER | NGame::PANEL_MEDALS | NGame::PANEL_BIOGRAPHY, false );
	if ( pMission->GetPanelState( NGame::PANEL_STORE | NGame::PANEL_INVENTORY ) == NGame::PANEL_STORE )
		pMission->SetPanelState( NGame::PANEL_STORE, false );
	// (the old "PERKS requires CHARACTER" auto-clear is gone: retail's panel bits are EXCLUSIVE
	// -- ActivateCharacterSubPanel @0x20f140 -- and that clear is what closed the whole panel
	// when the perks screen's back-to-skills tab dropped the CHARACTER bit.)

	const SPoint &sSize = GetClientWindow()->GetSize();
	const SPoint &sPosition = GetClientWindow()->GetPosition();
	SRect sClientRect( 0, 0, 1024, 768 );
	if ( !pMission->IsInterfaceHidden() )
	{
		sClientRect = SRect( 0, 32, 1024, 596 );
		if ( pMission->GetPanelState( NGame::PANEL_INVENTORY ) != 0 )
			sClientRect.x2 = 512;
		if ( pMission->GetPanelState( NGame::PANEL_STORE | NGame::PANEL_PERKS | NGame::PANEL_CHARACTER | NGame::PANEL_MEDALS | NGame::PANEL_BIOGRAPHY ) != 0 )
			sClientRect.x1 = 512;
	}

	GetClientWindow()->SetSize( SPoint( sClientRect.Width(), sClientRect.Height() ) );
	GetClientWindow()->SetPosition( SPoint( sClientRect.x1, sClientRect.y1 ) );

	pPause->SetStyle( STYLE_VISIBLE, pMission->IsGamePaused() );

	pInventory->SetStyle( STYLE_VISIBLE, ( pMission->GetPanelState( NGame::PANEL_INVENTORY ) == 0 ) );
	pInventory->SetStyle( STYLE_ENABLED, pMission->IsReady() );
	pCharacter->SetStyle( STYLE_VISIBLE, ( pMission->GetPanelState( NGame::PANEL_STORE | NGame::PANEL_PERKS | NGame::PANEL_CHARACTER | NGame::PANEL_MEDALS | NGame::PANEL_BIOGRAPHY ) == 0 ) );
	pCharacter->SetStyle( STYLE_ENABLED, pMission->IsReady() );

	// retail Update @0x211d60: each character sub-panel shows iff its bit is the SOLE family bit
	// (exclusive switching); the inventory bit is outside the family mask and coexists.
	const int nCharacterFamily = NGame::PANEL_STORE | NGame::PANEL_PERKS | NGame::PANEL_CHARACTER
		| NGame::PANEL_MEDALS | NGame::PANEL_BIOGRAPHY;
	pPerksPanel->SetStyle( STYLE_VISIBLE, ( pMission->GetPanelState( nCharacterFamily ) == NGame::PANEL_PERKS ) );
	pStorePanel->SetStyle( STYLE_VISIBLE, ( pMission->GetPanelState( nCharacterFamily ) == NGame::PANEL_STORE ) );
	pInventoryPanel->SetStyle( STYLE_VISIBLE, ( pMission->GetPanelState( NGame::PANEL_INVENTORY ) != 0 ) );
	pCharacterPanel->SetStyle( STYLE_VISIBLE, ( pMission->GetPanelState( nCharacterFamily ) == NGame::PANEL_CHARACTER ) );
	pMedalsPanel->SetStyle( STYLE_VISIBLE, ( pMission->GetPanelState( nCharacterFamily ) == NGame::PANEL_MEDALS ) );
	pBiographyPanel->SetStyle( STYLE_VISIBLE, ( pMission->GetPanelState( nCharacterFamily ) == NGame::PANEL_BIOGRAPHY ) );

	// retail Update @0x211d60 tail (disasm @0x612176..0x6121f7): reposition the ack subtitle band
	// every frame -- x = clientRect.x1 + 100, width = (clientRect.x2 - 100) - (clientRect.x1 + 100)
	// (i.e. 100px inset from BOTH client edges: between the clue-icon column and the inventory
	// button), bottom anchor = the inventory button's bottom edge (pos.y + size.y, desktop coords).
	// CAckIcon::Draw's reflow grows the band upward from that anchor (retail CAckView::Update).
	if ( IsValid( pAck ) && IsValid( pInventory ) )
		pAck->SetStripPlacement( sClientRect.x1 + 100, ( sClientRect.x2 - 100 ) - ( sClientRect.x1 + 100 ),
			pInventory->GetPosition().y + pInventory->GetSize().y );

	SRect sLogRect( sClientRect );
	sLogRect.x1 += N_LOGPANEL_PAD;
	sLogRect.y1 += N_LOGPANEL_PAD;
	sLogRect.x2 -= N_LOGPANEL_PAD;
	sLogRect.y2 -= N_LOGPANEL_PAD;
	sLogRect.y2 = sLogRect.y1 + sLogRect.Height() / 2;
	pLogPanel->SetSize( SPoint( sLogRect.Width(), sLogRect.Height() ) );
	pLogPanel->SetPosition( SPoint( sLogRect.x1, sLogRect.y1 ) );

	CDesktopWindow::Update( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionUI::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	UpdateHits( sTime );
	UpdateItems( pView );
	UpdateEnemies();
	UpdateClues();	// retail @0x213e70: the clue ("ear") markers rebuild together with the unit icons
	UpdateCameraScroll( sTime );

	CDesktopWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SInvItemSort
{
	bool operator()( CItemText *p1, CItemText *p2 ) const 
	{
		const SPoint &sSize1 = p1->GetItem().pItem->GetSize();
		const SPoint &sSize2 = p2->GetItem().pItem->GetSize();

		int nW1 = Max( sSize1.x, sSize1.y ) + sSize1.x * sSize1.y;
		int nW2 = Max( sSize2.x, sSize2.y ) + sSize2.x * sSize2.y;

		return nW1 > nW2; 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CanPlace( const CArray2D<bool> &sMap, const SPoint &sPos, const SPoint &sSize )
{
	if ( ( sPos.x < 0 ) || ( sPos.y < 0 ) || ( sPos.x + sSize.x > sMap.GetXSize() ) || ( sPos.y + sSize.y > sMap.GetYSize() ) )
		return false;

	for( int nTempY = 0; nTempY < sSize.y; nTempY++ )
	{
		for( int nTempX = 0; nTempX < sSize.x; nTempX++ )
		{
			if ( sMap[sPos.y + nTempY][sPos.x + nTempX] )
				return false;
		}
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void Place( CArray2D<bool> *pMap, const SPoint &sPos, const SPoint &sSize )
{
	if ( ( sPos.x < 0 ) || ( sPos.y < 0 ) || ( sPos.x + sSize.x > pMap->GetXSize() ) || ( sPos.y + sSize.y > pMap->GetYSize() ) )
	{
		ASSERT( 0 );
		return;
	}

	for( int nTempY = 0; nTempY < sSize.y; nTempY++ )
		for( int nTempX = 0; nTempX < sSize.x; nTempX++ )
			(*pMap)[sPos.y + nTempY][sPos.x + nTempX] = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionUI::UpdateItems( NGScene::I2DGameView *pView )
{
	if ( bindShowItems.IsActive() )
	{
		list<CObj<CItemText> > newItemTextsList;

		unordered_map<CPtr<NRPG::IInventoryItem>, CPtr<CItemText>, SPtrHash> itemsMap;
		for ( list<CObj<CItemText> >::const_iterator iTemp = itemTextsList.begin(); iTemp != itemTextsList.end(); iTemp++ )
		{
			CItemText *pItemText = *iTemp;
			itemsMap[ pItemText->GetItem().pItem ] = pItemText;
		}

		vector<CPtr<NGame::IUnitTracker> > unitsSet;
		pMission->GetSelectedUnits( &unitsSet );
		for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
		{
			vector<NWorld::SItem> tempItems;
			pMission->GetWorld()->FindCloseGroundItems( unitsSet[nTemp]->GetUnit(), &tempItems );

			for ( int nTemp = 0; nTemp < tempItems.size(); nTemp++ )
			{
				const NWorld::SItem &sItem = tempItems[nTemp];

				if ( !IsValid( sItem.pItem ) )
					continue;
				if ( !IsValid( sItem.pWorldItem ) )
					continue;

				unordered_map<CPtr<NRPG::IInventoryItem>, CPtr<CItemText>, SPtrHash>::iterator iFindRes = itemsMap.find( sItem.pItem );
				if ( iFindRes == itemsMap.end() )
					newItemTextsList.push_back( new CItemText( SWindowInfo( this, SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), pMission, this, sItem ) );
				else
					newItemTextsList.push_back( iFindRes->second.GetPtr() );
			}
		}

		itemTextsList = newItemTextsList;

		const int 
			N_X_STEP = 4,
			N_Y_STEP = 16,
			N_X_SIZE = 1024 / N_X_STEP,
			N_Y_SIZE = 768 / N_Y_STEP;

		CArray2D<bool> sMap( N_X_SIZE, N_Y_SIZE );
		sMap.FillEvery( false );

		itemTextsList.sort( SInvItemSort() );

		CVec2 vScreenRect = pView->GetViewportSize();
		CTransformStack sTS = pMission->GetCameraTransform();
		for ( list<CObj<CItemText> >::const_iterator iTemp = itemTextsList.begin(); iTemp != itemTextsList.end(); iTemp++ )
		{
			CItemText *pItemText = *iTemp;

			CVec2 vRes;
			if ( !TestRayInFrustrum( pItemText->GetItem().pWorldItem->GetPos(), &sTS, vScreenRect, &vRes ) )
			{
				pItemText->SetStyle( STYLE_VISIBLE, false );
				continue;
			}

			SPoint sRealSize = pItemText->GetRealSize( pView );
			sRealSize.x += N_X_STEP;

			SPoint sItemPos( vRes.x * N_X_SIZE / vScreenRect.x, vRes.y * N_Y_SIZE / vScreenRect.y );
			SPoint sItemSize( float( Max( sRealSize.x, N_X_STEP ) ) / N_X_STEP, float( Max( sRealSize.y, N_Y_STEP ) ) / N_Y_STEP );

			bool bComplete = false;
			for ( int nTempY = 0; nTempY < N_Y_SIZE / 4; nTempY++ )
			{
				for ( int nTempX = 0; nTempX < N_X_SIZE / 4; nTempX++ )
				{
					SPoint sTestPos;

					sTestPos = SPoint( sItemPos.x + nTempX, sItemPos.y + nTempY );
					if ( CanPlace( sMap, sTestPos, sItemSize ) )
					{
						bComplete = true;
						Place( &sMap, sTestPos, sItemSize );
						sItemPos = sTestPos;
						break;
					}

					sTestPos = SPoint( sItemPos.x - nTempX, sItemPos.y + nTempY );
					if ( CanPlace( sMap, sTestPos, sItemSize ) )
					{
						bComplete = true;
						Place( &sMap, sTestPos, sItemSize );
						sItemPos = sTestPos;
						break;
					}

					sTestPos = SPoint( sItemPos.x + nTempX, sItemPos.y - nTempY );
					if ( CanPlace( sMap, sTestPos, sItemSize ) )
					{
						bComplete = true;
						Place( &sMap, sTestPos, sItemSize );
						sItemPos = sTestPos;
						break;
					}

					sTestPos = SPoint( sItemPos.x - nTempX, sItemPos.y - nTempY );
					if ( CanPlace( sMap, sTestPos, sItemSize ) )
					{
						bComplete = true;
						Place( &sMap, sTestPos, sItemSize );
						sItemPos = sTestPos;
						break;
					}
				}

				if ( bComplete )
					break;
			}

			pItemText->SetStyle( STYLE_VISIBLE, bComplete );
			pItemText->SetPosition( SPoint( sItemPos.x * N_X_STEP, sItemPos.y * N_Y_STEP ) );
		}
	}
	else
		itemTextsList.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionUI::UpdateHits( const STime &sTime )
{
	CPtr<NWorld::CHitLocator> pTempLocator;
	while( pTempLocator = pMission->GetWorld()->GetHitEvent() )
		hitsList.push_back( new CHitTracker( SWindowInfo( GetClientWindow(), SPoint( 0, 0 ), SPoint( 120, 20 ), "hit", STYLE_ENABLED | STYLE_TRANSPARENT | STYLE_TOPMOST | STYLE_VISIBLE ), pMission, pTempLocator, sTime ) );

	for ( list<CObj<CHitTracker> >::iterator iTemp = hitsList.begin(); iTemp != hitsList.end(); )
	{
		if ( !(*iTemp)->IsComplete() )
			iTemp++;
		else
			iTemp = hitsList.erase( iTemp );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionUI::UpdateEnemies()
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetUnits( &unitsSet );

	CVec2 vScreenRect = pMission->GetScene()->GetScreenRect();
	CTransformStack sTS = pMission->GetCameraTransform();

	SRect sViewRect;
	SPoint sViewPosition;
	GetClientWindow()->ClientToScreen( &sViewPosition, &sViewRect );

	if ( sViewRect.Width() == 0 )
	{
		enemyIconsList.clear();
		return;
	}

	unordered_map<CPtr<NWorld::CUnit>,bool,SPtrHash> enemySet;
	for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
	{
		list<CPtr<NWorld::CUnit> > visibleUnits;
		unitsSet[nTemp]->GetVisibleEnemiesList( &visibleUnits );

		for ( list<CPtr<NWorld::CUnit> >::const_iterator iEnemy = visibleUnits.begin(); iEnemy != visibleUnits.end(); iEnemy++ )
		{
			bool &bValue = enemySet[*iEnemy];
			if ( unitsSet[nTemp]->IsSelected() )
				bValue = true;
		}
	}

	list<CObj<CEnemyIcon> > newEnemyIconsList;
	list<CObj<CEnemyIcon> >::iterator iOldIcons = enemyIconsList.begin();
	for (unordered_map<CPtr<NWorld::CUnit>,bool,SPtrHash>::iterator iTemp = enemySet.begin(); iTemp != enemySet.end(); iTemp++ )
	{
		bool bVisible = iTemp->second;
		CPtr<NWorld::CUnit> pEnemy = iTemp->first;

		if ( pEnemy->IsDead() || pEnemy->IsUnconscious() )
			continue;

		CEnemyIcon *pIcon;
		if ( iOldIcons != enemyIconsList.end() )
		{
			pIcon = (*iOldIcons);
			iOldIcons++;
		}
		else
		{
			// retail CMissionUI::UpdateVisibleUnits @0x213e70 parents the icons to the CLIENT ("view")
			// window, not the desktop. Only the topmost HitTest hit in child order receives mouse
			// events, and the screen-covering view window is created first -- a desktop-parented icon
			// never gets EVENT_MOUSEENTER, so the CActionDecorator hover -> SetStateTarget(GetTarget())
			// push (the whole "target the enemy by his overhead icon" mechanism) stays inert.
			pIcon = new CEnemyIcon( SWindowInfo( GetClientWindow(), SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), pMission, this );
		}

		CVec2 vScreenPos;
		CVec3 vEyePosition( pEnemy->GetPosition().GetEyePosition() );
		vEyePosition += CVec3( 0, 0, 0.6f );
		TestRayInFrustrum( vEyePosition, &sTS, vScreenRect, &vScreenPos );
		vScreenPos.x = vScreenPos.x * 1024 / vScreenRect.x;
		vScreenPos.y = vScreenPos.y * 768 / vScreenRect.y;

		bool bRet = false;
		if ( ( sViewRect.x1 < vScreenPos.x ) && ( sViewRect.x2 > vScreenPos.x ) && ( sViewRect.y1 < vScreenPos.y ) && ( sViewRect.y2 > vScreenPos.y ) )
			bRet = true;

		float fAngle = -1;
		if ( !bRet )
		{
			fAngle = ToDegree( atan2( vScreenPos.x - ( sViewRect.x2 - sViewRect.x1 ) / 2, -( vScreenPos.y - ( sViewRect.y2 - sViewRect.y1 ) / 2 ) ) );
			if ( fAngle < 0 )
				fAngle += 360;
		}

		const NUI::SPoint &sSize = pIcon->GetSize();
		vScreenPos.x = max( min( vScreenPos.x, sViewRect.x2 ), sViewRect.x1 );
		vScreenPos.y = max( min( vScreenPos.y, sViewRect.y2 ), sViewRect.y1 );

		pIcon->Set( pEnemy, bVisible, fAngle );
		SPoint sIconPos;	// icon is a CLIENT-window child now -- convert the 1024x768 screen point
		GetClientWindow()->ScreenToClient( SPoint( vScreenPos.x, vScreenPos.y ), &sIconPos );
		pIcon->SetPosition( sIconPos );
		newEnemyIconsList.push_back( pIcon );
	}

	enemyIconsList = newEnemyIconsList;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionUI::UpdateClues()
{
	// retail CMissionUI::UpdateVisibleUnits @0x213e70 (second list): rebuild the clue ("ear")
	// markers over the heard-not-seen set. The set is derived from the SAME GetSounds feed as the
	// heard-silhouette render and the TraceCursor heard pick, so an eared unit is exactly the one
	// the cursor can highlight/attack. Placement/projection mirrors UpdateEnemies (eye pos + 0.6).
	CVec2 vScreenRect = pMission->GetScene()->GetScreenRect();
	CTransformStack sTS = pMission->GetCameraTransform();

	SRect sViewRect;
	SPoint sViewPosition;
	GetClientWindow()->ClientToScreen( &sViewPosition, &sViewRect );

	NWorld::IPlayer *pPlayer = 0;
	if ( pMission->GetActivePlayer() )
		pPlayer = pMission->GetActivePlayer()->GetPlayer();

	if ( ( sViewRect.Width() == 0 ) || ( pPlayer == 0 ) )
	{
		clueIconsList.clear();
		return;
	}

	list< CPtr<NWorld::CUnit> > visibleList;
	pPlayer->GetVisible( &visibleList );
	NWorld::IPlayer::CUnitSet myUnits;
	pPlayer->GetUnits( &myUnits );
	vector<NWorld::IVisObj*> soundsList;
	pPlayer->GetSounds( &soundsList );

	// One ear per audible noise MARKER (retail CMissionUI::UpdateAudibleSounds @0x214530 keys its
	// CSoundIcon hash by the sound IVisObj). The icon anchors on the MARKER's position -- the live
	// unit's position would leak its movement while unseen. No dead/unconscious filtering either:
	// hiding the ear on death would itself leak the death.
	list<CObj<CClueIcon> > newClueIconsList;
	list<CObj<CClueIcon> >::iterator iOldIcons = clueIconsList.begin();
	for ( int nTemp = 0; nTemp < soundsList.size(); nTemp++ )
	{
		CObjectBase *pMarker = soundsList[nTemp];
		NWorld::CUnit *pHeard = dynamic_cast<NWorld::CUnit*>( NWorld::GetDMeshUnit( pMarker ) );
		CVec3 vMarkerPos;
		if ( !pHeard || !NWorld::GetDMeshPos( pMarker, &vMarkerPos ) )
			continue;
		if ( find( myUnits.begin(), myUnits.end(), pHeard ) != myUnits.end() )
			continue;
		if ( find( visibleList.begin(), visibleList.end(), pHeard ) != visibleList.end() )
			continue;

		CClueIcon *pIcon;
		if ( iOldIcons != clueIconsList.end() )
		{
			pIcon = (*iOldIcons);
			iOldIcons++;
		}
		else
		{
			// client-window parent for the same reason as CEnemyIcon above (retail @0x213e70): the
			// decorator hover push is what makes the heard silhouette targetable through its ear icon.
			pIcon = new CClueIcon( SWindowInfo( GetClientWindow(), SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), pMission, this );
		}

		CVec2 vScreenPos;
		CVec3 vIconPos( vMarkerPos );
		vIconPos += CVec3( 0, 0, 2.2f );   // above the silhouette's head (marker stands on the ground)
		TestRayInFrustrum( vIconPos, &sTS, vScreenRect, &vScreenPos );
		vScreenPos.x = vScreenPos.x * 1024 / vScreenRect.x;
		vScreenPos.y = vScreenPos.y * 768 / vScreenRect.y;

		bool bRet = false;
		if ( ( sViewRect.x1 < vScreenPos.x ) && ( sViewRect.x2 > vScreenPos.x ) && ( sViewRect.y1 < vScreenPos.y ) && ( sViewRect.y2 > vScreenPos.y ) )
			bRet = true;

		float fAngle = -1;
		if ( !bRet )
		{
			fAngle = ToDegree( atan2( vScreenPos.x - ( sViewRect.x2 - sViewRect.x1 ) / 2, -( vScreenPos.y - ( sViewRect.y2 - sViewRect.y1 ) / 2 ) ) );
			if ( fAngle < 0 )
				fAngle += 360;
		}

		vScreenPos.x = max( min( vScreenPos.x, sViewRect.x2 ), sViewRect.x1 );
		vScreenPos.y = max( min( vScreenPos.y, sViewRect.y2 ), sViewRect.y1 );

		pIcon->Set( pMarker, pHeard, fAngle );
		SPoint sIconPos;	// icon is a CLIENT-window child now -- convert the 1024x768 screen point
		GetClientWindow()->ScreenToClient( SPoint( vScreenPos.x, vScreenPos.y ), &sIconPos );
		pIcon->SetPosition( sIconPos );
		newClueIconsList.push_back( pIcon );
	}

	clueIconsList = newClueIconsList;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionUI::UpdateCameraScroll( const STime &sTime )
{
	STime sDelta = sTime - sCameraScrollUpdate;
	sDelta = Min( sDelta, (STime)100 );
	sCameraScrollUpdate = sTime;

	CPtr<ICamera> pCamera = pMission->GetCamera();
	CPtr<NUI::ICursor> pCursor = pMission->GetCursor();
	CPtr<NGScene::IGameView> pView = pMission->GetScene();

	CVec2 vScreenRect = pView->GetScreenRect();
	const CVec2 &vPos = pCursor->GetPos();

	CVec3 vStrafeDir( pCamera->GetStrafeDir() ), vForwardDir( pCamera->GetForwardDir() );
	vStrafeDir.z = 0;
	vForwardDir.z = 0;
	Normalize( &vStrafeDir );
	Normalize( &vForwardDir );

	// release @0x210b40: accumulate ONE combined edge-scroll delta and feed it to
	// ICamera::ScrollAnchor (vtbl[0x54], non-immediate) -- it pans only the DESIRED anchor and
	// CCamera::Update (@0xcd930) eases the live camera into it. (The previous GetPlacement/
	// SetPlacement round-trip snapped the live placement and bypassed the easing entirely.)
	CVec3 vScrollDelta( 0, 0, 0 );
	int nMask = 0;
	float fStep = (float)( N_SCROLL_STEP * sDelta ) / 1000.0f;
	if ( vPos.x < N_SCROLL_GUARDBAND )
	{
		nMask |= 1;
		vScrollDelta -= vStrafeDir * fStep;
	}
	if ( vPos.x > vScreenRect.x - N_SCROLL_GUARDBAND )
	{
		nMask |= 2;
		vScrollDelta += vStrafeDir * fStep;
	}
	if ( vPos.y < N_SCROLL_GUARDBAND )
	{
		nMask |= 4;
		vScrollDelta += vForwardDir * fStep;
	}
	if ( vPos.y > vScreenRect.y - N_SCROLL_GUARDBAND )
	{
		nMask |= 8;
		vScrollDelta -= vForwardDir * fStep;
	}

	pCamera->ScrollAnchor( vScrollDelta, false, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAckEvent* CMissionUI::PlayAckEvent( const STime &sTime, NWorld::CAckEvent *pEvent )
{
	// retail: the NUI wrapper is constructed around the world-side ack with bReady=false. It is NOT
	// armed here -- bReady flips later (via the single-unit face's ACK_WAIT step, or its fallback),
	// so the deferred voice/lipsync only fire once the speaker's face is on screen.
	CAckEvent *pAckEvent = new CAckEvent( pEvent );

	// (a) the on-screen subtitle bubble (dev CAckIcon: head-in-bubble + subtitle text + voice)
	pAck->Set( pAckEvent );

	// (b) release CMissionUI::PlayAckEvent @0x211930 also drives the bottom-LEFT single-unit face: the
	// 3D portrait turns to the camera to deliver the line, temporarily swapping in a non-selected (or
	// enemy) speaker's head. (Retail gates this on the bShowAcks setting; the dev has no such toggle, so
	// it is always on, matching the retail default.)
	if ( IsValid( pUnitPanel ) )
		pUnitPanel->PlayAckEvent( sTime, pAckEvent );

	return pAckEvent;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0241940, CAckIcon );
REGISTER_SAVELOAD_CLASS( 0xB0241942, CMissionUI );
REGISTER_SAVELOAD_CLASS( 0xB0241947, CItemText );
REGISTER_SAVELOAD_CLASS( 0xB0241948, CEnemyIcon );
REGISTER_SAVELOAD_CLASS( 0xB0241949, CHitTracker );
REGISTER_SAVELOAD_CLASS( 0xB024194A, CClueIcon );
