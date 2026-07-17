#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "wInterface.h"
#include "wMisc.h"			// NWorld::GetDMeshUnit -- clue ("ear") markers over heard-not-seen units
#include "RPGItemInfo.h"
#include "..\Misc\StrProc.h"
#include "..\Input\Bind.h"
#include "..\MiscDll\Commands.h"	// NGlobal::GetVar -- the "ui_showhints" gate of the hint-icon pass (retail @0x2130c0)
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
// CAckIcon -- retail NUI::CAckView (saveload id 0xB0241940; ctor @0x210050): the ack subtitle view,
// a CFrame in retail (the band is the frame's own nine-slice skin). operator& matches retail
// @0x219ef0: {1 CFrame base, 2 pMission, 3 bPlayAck (1-byte chunk), 4 pText, 5 pEvent, 6 pSound}.
// Retail overrides ONLY Update (vftable @0x8be960 slot 11 = @0x2110e0; slot 10 ProcessMessage =
// CWindow's, slot 12 Draw = CFrame::Draw) -- the frame ages the ack in Update and CFrame::Draw
// paints the band. pText is created IN CODE by the SWindowInfo ctor ("acktext", style 0xe); the
// default (load-path) ctor @0x217500 leaves it null -- tag 4 restores it from the save, so the
// subtitle survives save-load with NO post-load rebuild step.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckIcon: public CFrame
{
	OBJECT_NOCOPY_METHODS(CAckIcon);
private:
	ZDATA_(CFrame)
	CPtr<NGame::IMission> pMission;
	bool bPlayAck = false;	// retail CAckView +bPlayAck: latched in Set, consumed by Update once bReady flips
	CObj<CText> pText;
	CPtr<CAckEvent> pEvent;
	CObj<NSound::ISound2D> pSound;	// the deferred 2D voice handle (retail CAckView::pSound)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CFrame*)this); f.Add(2,&pMission); f.Add(3,&bPlayAck); f.Add(4,&pText); f.Add(5,&pEvent); f.Add(6,&pSound); return 0; }

public:
	CAckIcon() {}
	// retail @0x210050: chain CFrame(sInfo) (fills the nine skin slices), then create the subtitle
	// text in code -- "acktext", style 0xe = VISIBLE|ENABLED|TOPMOST, pos/size {0,0}, child of this.
	CAckIcon( const SWindowInfo &sInfo, NGame::IMission *_pMission ): CFrame( sInfo ), pMission( _pMission )
	{
		pText = new CText( SWindowInfo( this, SPoint( 0, 0 ), SPoint( 0, 0 ), "acktext", STYLE_VISIBLE | STYLE_ENABLED | STYLE_TOPMOST ) );
	}

	void Set( CAckEvent *pEvent );
	void PlayAck();	// retail CAckView::PlayAck @0x210ff0 -- deferred voice + heads-controller lipsync
	void Update( const STime &sTime, NGScene::I2DGameView *pView );	// retail @0x2110e0
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
			case 133: // symbol L'...' (ellipsis)
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
	// retail CAckView::Set @0x210e10: accept the ack ONLY when both the ack row and the speaker are
	// live (an invalid ack must NOT displace the current pEvent); latch bPlayAck (voice + lipsync
	// DEFER to PlayAck, fired from Update once the bReady handshake flips, so the mouth moves in
	// lock-step with the portrait turning to camera). Subtitle text + show are gated on the
	// "ui_charresponsessubtitles" option (retail bShowAcksSubtitles, registered @0x215b90, default on).
	NWorld::CAckEvent *pAckEvent = _pEvent->GetAckEvent();
	if ( !IsValid( pAckEvent->pAckInfo ) || !IsValid( pAckEvent->pUnit ) )
		return;

	pEvent = _pEvent;
	bPlayAck = true;

	if ( NGlobal::GetVar( "ui_charresponsessubtitles" ).GetFloat() != 0 )
	{
		// disasm @0x610ea3..0x610f4f: the subtitle is FIVE pieces --
		//   L"<minfontsize size=16>"                                (static literal @VA 0x8be9d8)
		//   + GetDBString( 19329 )   "EnemyTooltip Name Format"  =  "<font face=Courier size=16pt><color=yellow>"
		//   + <speaker name>
		//   + GetDBString( 20257 )   "Ack Format"                =  "<color=beige>: "
		//   + ConvertLineBreaks( <ack body text> )
		wstring wsSubtitle = wstring( L"<minfontsize size=16>" ) + GetDBString( 19329 );
		wsSubtitle += pAckEvent->pUnit->GetRPG()->GetRPGUnit()->GetName();
		wsSubtitle += GetDBString( 20257 );
		wsSubtitle += ConvertLineBreaks( GetDBString( pAckEvent->pAckInfo->pText ) );
		pText->SetText( wsSubtitle );
		SetStyle( STYLE_VISIBLE, true );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NUI::CAckView::PlayAck @0x210ff0: fired from Update once the ack is bReady. Start the 2D voice
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
	if ( IsValid( pMission ) )
		// controller records are keyed by the unit's per-unit CHeadInfo (retail PlaySequence @0x25df90)
		pMission->GetRenderGame()->GetHeadController()->PlaySequence( pAckEvent->pUnit->GetHeadInfo(), voice.pSequence, NDb::GetSequenceByExpression( voice.eExpression ), false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NUI::CAckView::Update @0x2110e0 (the ONLY behaviour override CAckView has: vftable
// @0x8be960 slot 11; slot 10 ProcessMessage = CWindow's, slot 12 Draw = CFrame::Draw): age the
// ack, run the DEFERRED voice/lipsync, reflow the band; release + hide once the event completes.
void CAckIcon::Update( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( IsValid( pEvent ) && !pEvent->IsComplete( sTime ) )
	{
		NWorld::CAckEvent *pAckEvent = pEvent->GetAckEvent();
		if ( !IsValid( pAckEvent ) || !IsValid( pAckEvent->pUnit ) )
			pEvent->Cancel();

		// bReady flips when the single-unit face turns to camera (CUnitFace ACK_WAIT) or when its
		// fallback fires -- only THEN start the voice and route the mouth/gesture through the heads
		// controller. (retail: bPlayAck && pEvent->bReady -> PlayAck.)
		if ( bPlayAck && pEvent->IsReady() )
		{
			bPlayAck = false;
			PlayAck();
		}
		// once the voice has been kicked off (or there was none), retire on TTL after the sound stops.
		if ( !bPlayAck && pEvent->IsTTLComplete( sTime ) && ( !IsValid( pSound ) || !pSound->IsPlaying() ) )
			pEvent->SetComplete( true );

		// reflow (disasm @0x6111a9..0x611255): wrap width = own width - 8, measure, band height =
		// text height + 8, grow UPWARD from the bottom anchor CMissionUI::Update just pushed into
		// this window's position (@0x612176..0x6121f7); text sits at (4,4) inside the band.
		pText->SetSize( SPoint( GetSize().x - 8, 0 ) );
		SPoint sReal( 0, 0 );
		pText->GetRealSize( &sReal );
		int nBandH = sReal.y + 8;
		SetSize( SPoint( GetSize().x, nBandH ) );
		SetPosition( SPoint( GetPosition().x, GetPosition().y - nBandH ) );
		pText->SetSize( SPoint( GetSize().x - 8, sReal.y ) );
		pText->SetPosition( SPoint( 4, 4 ) );
	}
	else
	{
		// retail tail: release the completed event + its voice handle, hide the band.
		pEvent = 0;
		pSound = 0;
		SetStyle( STYLE_VISIBLE, false );
	}

	CWindow::Update( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CItemText
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NUI::CItemText (ctor @0x212300, AddItem @0x212510, GenerateText @0x20fee0, GetTarget @0x20fec0,
// ProcessMessage @0x2101c0): one label per 1.875m-cell GROUP of same-DBItem ground items ("Name x N"),
// parented to the CLIENT window (clicks/hover work through the stock decorator -- same mechanism as the
// enemy/ear icons). Serialized shape = retail's {1:base, 2:itemsList, 3:pMission, 4:pText} @0x21a0a0.
class CItemText: public CActionDecorator<CImage>
{
	OBJECT_BASIC_METHODS(CItemText)
public:
	struct SItem   // retail NUI::CItemText::SItem @0x21a220
	{
		ZDATA
		CPtr<NWorld::IItem> pWorldItem;
		CPtr<NRPG::IInventoryItem> pInvItem;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pWorldItem); f.Add(3,&pInvItem); return 0; }
	};
private:
	ZDATA_(TBaseClass)
	list<SItem> itemsList;
	CPtr<NGame::IMission> pMission;
	CObj<CTextDraw> pText;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); f.Add(2,&itemsList); f.Add(3,&pMission); f.Add(4,&pText); return 0; }
	void GenerateText();

public:
	CItemText() {}
	CItemText( const SWindowInfo &sInfo, NGame::IMission *pMission, NWorld::IItem *pWItem, NRPG::IInventoryItem *pInv );

	bool CanHandleState( NGame::IState *pState ) const;
	CObjectBase* GetTarget();

	void AddItem( NWorld::IItem *pWItem, NRPG::IInventoryItem *pInv );   // retail @0x212510
	const list<SItem>& GetItems() const { return itemsList; }
	const SPoint& GetRealSize( NGScene::I2DGameView *pView ) { return pText->GetSize( pView ); }

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail GenerateText @0x20fee0: first item's DB name; "x N" suffix when the label groups several.
void CItemText::GenerateText()
{
	wstring wsName( L"[UNKNOWN]" );
	if ( !itemsList.empty() && IsValid( itemsList.front().pInvItem )
		&& itemsList.front().pInvItem->GetDBItem() && itemsList.front().pInvItem->GetDBItem()->pName )
		wsName = itemsList.front().pInvItem->GetDBItem()->pName->szStr;
	wchar_t wsText[512];
	if ( itemsList.size() > 1 )
		swprintf( wsText, L"<font face=Courier size=16pt><color=white>%s x %d", wsName.c_str(), (int)itemsList.size() );
	else
		swprintf( wsText, L"<font face=Courier size=16pt><color=white>%s", wsName.c_str() );
	pText = new CTextDraw( SPoint( 0, 0 ), SPoint( -1, -1 ), wsText );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CItemText::CItemText( const SWindowInfo &sInfo, NGame::IMission *_pMission, NWorld::IItem *pWItem, NRPG::IInventoryItem *pInv ):
	TBaseClass( sInfo, _pMission ), pMission( _pMission )
{
	SItem s;
	s.pWorldItem = pWItem;
	s.pInvItem = pInv;
	itemsList.push_back( s );
	GenerateText();
	SetColor( NGfx::SPixel8888( 0x1F, 0x1F, 0x1F, 0xDF ) );   // retail ctor @0x212300: 0xdf1f1f1f
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CItemText::AddItem( NWorld::IItem *pWItem, NRPG::IInventoryItem *pInv )
{
	SItem s;
	s.pWorldItem = pWItem;
	s.pInvItem = pInv;
	itemsList.push_back( s );
	GenerateText();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemText::CanHandleState( NGame::IState *pState ) const
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CItemText::GetTarget()
{	// retail @0x20fec0: the FIRST item's world object (the CDFrozenItem the cursor would pick)
	if ( itemsList.empty() )
		return 0;
	return CDynamicCast<CObjectBase>( itemsList.front().pWorldItem.GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CItemText::ProcessMessage( const SEvent &sEvent )
{
	switch ( sEvent.nEvent )
	{
	case EVENT_MOUSEENTER:
		{
			SetColor( NGfx::SPixel8888( 0xDF, 0x1F, 0x1F, 0xFF ) );   // retail @0x2101c0: 0xff1f1fdf
			break;
		}
	case EVENT_MOUSEEXIT:
		{
			SetColor( NGfx::SPixel8888( 0x1F, 0x1F, 0x1F, 0xDF ) );   // retail @0x2101c0: 0xdf1f1f1f
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
// CProjectedIcon -- retail NUI::CProjectedIcon (ctor @0x2112d0, operator& @0x21a290: 1=CActionDecorator
// <CImage> base, 2=pMission): the common serialized base of every projected in-world marker (the
// unit/enemy, ear, clue, hint and trap icons all chain it as tag 1). Retail also hosts the shared
// projection helpers here (ClampPosition @0x20f580, UpdatePosition @0x20f370, GetPositionInfo
// @0x20f610); the dev icons keep their own per-class Draw clamps, so only the serialized shape lives
// here. NOT save-registered (retail registers no factory for it -- base subobject only). Note the
// pMission member sits ON TOP of the decorator's private pMission, exactly like retail's layout.
class CProjectedIcon: public CActionDecorator<CImage>
{
protected:
	ZDATA_(TBaseClass)
	CPtr<NGame::IMission> pMission;		// retail +0x8c (tag 2)
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); f.Add(2,&pMission); return 0; }

	CProjectedIcon() {}
	CProjectedIcon( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
		TBaseClass( sInfo, _pMission ), pMission( _pMission ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEnemyIcon -- retail renamed it CUnitIcon (same saveload id 0xB0241948; operator& @0x21a2d0:
// 1=CProjectedIcon base, 2=pUnit -- dev pEnemy). Everything else (owner flag, angle, texture pick)
// is rebuilt every CMissionUI update pass, so those members are transient like retail.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEnemyIcon: public CProjectedIcon
{
	OBJECT_BASIC_METHODS(CEnemyIcon)
private:
	ZDATA_(CProjectedIcon)
	CPtr<NWorld::CUnit> pEnemy;				// retail CUnitIcon::pUnit (tag 2)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CProjectedIcon*)this); f.Add(2,&pEnemy); return 0; }
	// transient presentation state (rebuilt by CMissionUI::UpdateEnemies each pass; retail
	// serializes none of these -- the old dev tags 2..6/8 are format violations, dropped):
	CPtr<CMissionUI> pMissionUI;
	bool bOwner;
	float fAngle;
	CPtr<CImage> pImage;
	CDBPtr<NDb::CUITexture> pTexture;

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
	CProjectedIcon( sInfo, _pMission ), pMissionUI( _pMissionUI ), bOwner( false ), fAngle( 0 )
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
	// transient pTexture (set in the update pass); a restored icon Drawn before Set() has it null -> skip.
	if ( !IsValid( pTexture ) )
		return;
	SPoint sNewSize( pTexture->nWidth, pTexture->nHeight );
	SPoint sSize = GetSize();
	SPoint sPosition = GetPosition();

	// position/clamp are CLIENT-window-local now (icons are view children, retail @0x213e70)
	const SPoint &sParentSize = GetParent()->GetSize();	// the client window (serialized pParent; the transient pMissionUI is NULL on a loaded icon)
	SRect sViewRect( 0, 0, sParentSize.x, sParentSize.y );
	sPosition.x = min( max( sViewRect.x1 + sNewSize.x / 2, sPosition.x ), sViewRect.x2 - sNewSize.x / 2 );
	sPosition.y = min( max( sViewRect.y1 + sNewSize.y / 2, sPosition.y ), sViewRect.y2 - sNewSize.y / 2 );

	TBaseClass::SetSize( sNewSize );
	TBaseClass::SetPosition( SPoint( sPosition.x + sSize.x / 2 - sNewSize.x / 2, sPosition.y + sSize.y / 2 - sNewSize.y / 2 ) );

	TBaseClass::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSoundIcon (retail NUI::CSoundIcon; formerly misnamed CClueIcon in this fork) -- the "ear"
// marker drawn over a heard-not-seen noise.
// Textures = the retail Sound Icons folder (CSoundIcon::Draw @0x210960 disasm): on-screen ear 674
// 'Icon - OnScreen', off-screen 8-direction arrows {666,672,673,667,668,669,670,671} for buckets
// 0g,45g..315g. (The 675-683 set belongs to retail's real NUI::CClueIcon @0x2102e0 -- the
// clue-ITEM marker, now ported below.) The icon anchors on the SOUND MARKER (CDMesh) position -- never on the
// live unit -- and its action target is THE MARKER (retail CSoundIcon::GetTarget @0x2165a0), so
// hovering/attacking through the icon can't spoil the hidden unit's identity or movement.
class CSoundIcon: public CProjectedIcon
{
	OBJECT_BASIC_METHODS(CSoundIcon)
private:
	ZDATA_(CProjectedIcon)
	// retail operator& @0x21a470: 1=CProjectedIcon base, 2=pSound (CPtr<NWorld::IVisObj> -- the
	// heard-noise sound marker). This fork's pMarker IS that member (kept as CPtr<CObjectBase>,
	// wire-identical object ref); everything else is transient per-pass presentation state.
	CPtr<CObjectBase> pMarker;	// retail CSoundIcon::pSound (tag 2) -- the heard-noise CDMesh / sound marker (the action target)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CProjectedIcon*)this); f.Add(2,&pMarker); return 0; }
	// transient (rebuilt by the CMissionUI update pass each frame; retail serializes none of these):
	CPtr<CMissionUI> pMissionUI;
	float fAngle;
	CPtr<NWorld::CUnit> pUnit;
	CDBPtr<NDb::CUITexture> pTexture;

public:
	CSoundIcon() {}
	CSoundIcon( const SWindowInfo &sInfo, NGame::IMission *pMission, CMissionUI *pMissionUI );

	bool CanHandleState( NGame::IState *pState ) const;
	CObjectBase* GetTarget();

	void SetPosition( const SPoint &_sPosition );

	void Set( CObjectBase *pMarker, NWorld::CUnit *pUnit, float _fAngle );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CSoundIcon::CSoundIcon( const SWindowInfo &sInfo, NGame::IMission *_pMission, CMissionUI *_pMissionUI ):
	CProjectedIcon( sInfo, _pMission ), pMissionUI( _pMissionUI ), fAngle( 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoundIcon::CanHandleState( NGame::IState *pState ) const
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CSoundIcon::GetTarget()
{
	// retail CSoundIcon::GetTarget @0x2165a0 returns the SOUND MARKER -- handing the unit to the
	// states here would re-open the identity/death spoil the trace-side fix closed.
	return pMarker;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundIcon::SetPosition( const SPoint &_sPosition )
{
	TBaseClass::SetSize( SPoint( 0, 0 ) );
	TBaseClass::SetPosition( _sPosition );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundIcon::Set( CObjectBase *_pMarker, NWorld::CUnit *_pUnit, float _fAngle )
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
bool CSoundIcon::ProcessMessage( const SEvent &sEvent )
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
void CSoundIcon::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	// transient pTexture (set in the update pass); a restored icon Drawn before Set() has it null -> skip.
	if ( !IsValid( pTexture ) )
		return;
	SPoint sNewSize( pTexture->nWidth, pTexture->nHeight );
	SPoint sSize = GetSize();
	SPoint sPosition = GetPosition();

	// position/clamp are CLIENT-window-local now (icons are view children, retail @0x213e70)
	const SPoint &sParentSize = GetParent()->GetSize();	// the client window (serialized pParent; the transient pMissionUI is NULL on a loaded icon)
	SRect sViewRect( 0, 0, sParentSize.x, sParentSize.y );
	sPosition.x = min( max( sViewRect.x1 + sNewSize.x / 2, sPosition.x ), sViewRect.x2 - sNewSize.x / 2 );
	sPosition.y = min( max( sViewRect.y1 + sNewSize.y / 2, sPosition.y ), sViewRect.y2 - sNewSize.y / 2 );

	TBaseClass::SetSize( sNewSize );
	TBaseClass::SetPosition( SPoint( sPosition.x + sSize.x / 2 - sNewSize.x / 2, sPosition.y + sSize.y / 2 - sNewSize.y / 2 ) );

	TBaseClass::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CClueIcon -- retail NUI::CClueIcon (item ctor @0x211410, unit ctor @0x211390, ProcessMessage
// @0x210250, Draw @0x2102e0, GetTarget @0x2162f0 [COMDAT-folded with CUnitIcon's: the +0x90 CPtr as
// CObjectBase], GetIndex @0x210010, saveload id 0xB3123180): the key/bell "clue" marker projected
// over a discovered scenario-CLUE ground item. CMissionUI::UpdateVisibleItems @0x2130c0 rebuilds one
// per visible NRPG::IClueItem -- UNGATED (disasm: the IClueItem branch @0x613800 has no bool test,
// unlike the hint branch @0x613a38 which honours "ui_showhints").
// Draw @0x2102e0 (disasm): anchor z += 0.6 (fadd [0x8b1fe8] = 0.6f @0x61039b), then GetPositionInfo
// @0x60f610; ON-screen (ret != 0 @0x6103c3) -> texture 0x2ab (683); OFF-screen -> sequential arrow
// array 0x2a3..0x2aa (675..682) indexed by Clamp(round(angle/45),0,7) (fmul [0x8be994] = 1/45).
// Retail also builds these icons over heard-not-seen UNITS (@0x211390, clueUnitIconsList) -- that
// role is covered in this fork by the CSoundIcon ear markers above, so the unit flavor is carried
// for shape parity but never constructed here.
class CClueIcon: public CProjectedIcon
{
	OBJECT_BASIC_METHODS(CClueIcon)
private:
	ZDATA_(CProjectedIcon)
	// retail operator& @0x21a370: 1=CProjectedIcon base (carries pMission), 2=pWorldItem,
	// 3=pWorldUnit, 4=pInvItem. pTexture/pMissionUI are transient per-pass state.
	CPtr<NWorld::IItem> pWorldItem;			// retail +0x90 (tag 2)
	CPtr<NWorld::CUnit> pWorldUnit;			// retail +0x94 (tag 3; heard-unit flavor -- unused in this fork)
	CPtr<NRPG::IInventoryItem> pInvItem;	// retail +0x98 (tag 4) -- the UpdateHash reuse key (@0x217220)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CProjectedIcon*)this); f.Add(2,&pWorldItem); f.Add(3,&pWorldUnit); f.Add(4,&pInvItem); return 0; }
	// transient:
	CDBPtr<NDb::CUITexture> pTexture;
	CPtr<CMissionUI> pMissionUI;			// dev icon pattern: client-rect clamp in Draw

public:
	CClueIcon() {}
	CClueIcon( const SWindowInfo &sInfo, NGame::IMission *pMission, NWorld::IItem *pWItem, NRPG::IInventoryItem *pInv, CMissionUI *pMissionUI );

	bool CanHandleState( NGame::IState *pState ) const;		// retail @0x2162e0: true
	CObjectBase* GetTarget();								// retail @0x2162f0: pWorldItem as CObjectBase

	NRPG::IInventoryItem* GetInvItem() const { return pInvItem; }
	void Set( float fAngle );		// texture pick (retail folds it into Draw; dev icons pick in the update pass)
	void SetPosition( const SPoint &sPosition );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CClueIcon::CClueIcon( const SWindowInfo &sInfo, NGame::IMission *_pMission, NWorld::IItem *pWItem, NRPG::IInventoryItem *pInv, CMissionUI *_pMissionUI ):
	CProjectedIcon( sInfo, _pMission ), pWorldItem( pWItem ), pInvItem( pInv ), pMissionUI( _pMissionUI )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CClueIcon::CanHandleState( NGame::IState *pState ) const
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CClueIcon::GetTarget()
{	// retail @0x2162f0 (folded body): the +0x90 CPtr -- the world item -- adjusted to CObjectBase
	return CDynamicCast<CObjectBase>( pWorldItem.GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CClueIcon::Set( float fAngle )
{
	// retail CClueIcon::Draw @0x2102e0 texture pick: on-screen (fAngle == -1 here, i.e.
	// GetPositionInfo returned "unclamped") -> 683 'Icon - OnScreen'; off-screen -> the SEQUENTIAL
	// arrow ids 675..682 (stack array 0x2a3..0x2aa @0x6103cf..0x610415) by Clamp(round(angle/45),0,7).
	if ( fAngle == -1 )
		pTexture = NDb::GetUITexture( 683 );
	else
		pTexture = NDb::GetUITexture( 675 + min( max( Float2Int( fAngle / 45 ), 0 ), 7 ) );

	SetImage( pTexture );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CClueIcon::SetPosition( const SPoint &_sPosition )
{
	TBaseClass::SetSize( SPoint( 0, 0 ) );
	TBaseClass::SetPosition( _sPosition );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CClueIcon::ProcessMessage( const SEvent &sEvent )
{
	// retail @0x210250: SEvent 0x6000034 (== dev EVENT_RBUTTONUP) -> live unit? mission vtbl+0xc0
	// FocusCameraOnUnit : else item? vtbl+0xc4 FocusCameraOnItem; returns true either way.
	// 0x6000035 (== dev EVENT_RBUTTONDOWN) is swallowed. Anything else -> base decorator.
	switch ( sEvent.nEvent )
	{
	case EVENT_RBUTTONDOWN:
		return true;
	case EVENT_RBUTTONUP:
		if ( IsValid( pWorldUnit ) )
			pMission->FocusCameraOnUnit( pWorldUnit );
		else if ( IsValid( pWorldItem ) )
			pMission->FocusCameraOnItem( pWorldItem );
		return true;
	}

	return TBaseClass::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CClueIcon::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	// pTexture is transient per-pass state (Set() picks it in the mission-UI update, retail folds the pick
	// into Draw). A restored icon Drawn before its update-pass Set() runs has a null pTexture -> skip until set.
	if ( !IsValid( pTexture ) )
		return;
	SPoint sNewSize( pTexture->nWidth, pTexture->nHeight );
	SPoint sSize = GetSize();
	SPoint sPosition = GetPosition();

	// position/clamp are CLIENT-window-local (icons are view children, like the ear/enemy icons)
	const SPoint &sParentSize = GetParent()->GetSize();	// the client window (serialized pParent; the transient pMissionUI is NULL on a loaded icon)
	SRect sViewRect( 0, 0, sParentSize.x, sParentSize.y );
	sPosition.x = min( max( sViewRect.x1 + sNewSize.x / 2, sPosition.x ), sViewRect.x2 - sNewSize.x / 2 );
	sPosition.y = min( max( sViewRect.y1 + sNewSize.y / 2, sPosition.y ), sViewRect.y2 - sNewSize.y / 2 );

	TBaseClass::SetSize( sNewSize );
	TBaseClass::SetPosition( SPoint( sPosition.x + sSize.x / 2 - sNewSize.x / 2, sPosition.y + sSize.y / 2 - sNewSize.y / 2 ) );

	TBaseClass::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHintIcon -- retail NUI::CHintIcon (ctor @0x2114a0, ProcessMessage @0x210510, Draw @0x210550,
// GetTarget @0x2162f0 [folded], CanHandleState @0x2162e0, saveload id 0xB3212140): the marker over a
// discovered in-world HINT pickup (an NRPG::IHintItem, retail CSimpleItem<IHintItem>).
// UpdateVisibleItems @0x2130c0 rebuilds one per visible non-clue IHintItem, gated by the
// "ui_showhints" var (disasm @0x613a38: cmp byte bShowHints,0).
// Draw @0x210550 (disasm): anchor = pWorldItem->GetPos, z += 0.6 (fadd [0x8b1fe8] @0x61058a);
// ON-screen -> texture 0x321 (801, @0x610670); OFF-screen -> sequential arrows 0x322..0x329
// (802..809, @0x6105c3..0x610609) by Clamp(round(angle/45),0,7).
class CHintIcon: public CProjectedIcon
{
	OBJECT_BASIC_METHODS(CHintIcon)
private:
	ZDATA_(CProjectedIcon)
	// retail operator& @0x21a3e0: 1=CProjectedIcon base (carries pMission), 2=pWorldItem, 3=pInvItem.
	CPtr<NWorld::IItem> pWorldItem;			// retail +0x90 (tag 2)
	CPtr<NRPG::IInventoryItem> pInvItem;	// retail +0x94 (tag 3) -- the UpdateHash reuse key (@0x217170)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CProjectedIcon*)this); f.Add(2,&pWorldItem); f.Add(3,&pInvItem); return 0; }
	// transient:
	CDBPtr<NDb::CUITexture> pTexture;
	CPtr<CMissionUI> pMissionUI;			// dev icon pattern: client-rect clamp in Draw

public:
	CHintIcon() {}
	CHintIcon( const SWindowInfo &sInfo, NGame::IMission *pMission, NWorld::IItem *pWItem, NRPG::IInventoryItem *pInv, CMissionUI *pMissionUI );

	bool CanHandleState( NGame::IState *pState ) const;		// retail @0x2162e0: true
	CObjectBase* GetTarget();								// retail @0x2162f0: pWorldItem as CObjectBase

	NRPG::IInventoryItem* GetInvItem() const { return pInvItem; }
	void Set( float fAngle );
	void SetPosition( const SPoint &sPosition );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CHintIcon::CHintIcon( const SWindowInfo &sInfo, NGame::IMission *_pMission, NWorld::IItem *pWItem, NRPG::IInventoryItem *pInv, CMissionUI *_pMissionUI ):
	CProjectedIcon( sInfo, _pMission ), pWorldItem( pWItem ), pInvItem( pInv ), pMissionUI( _pMissionUI )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CHintIcon::CanHandleState( NGame::IState *pState ) const
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CHintIcon::GetTarget()
{	// retail @0x2162f0 (folded body): pWorldItem adjusted to CObjectBase
	return CDynamicCast<CObjectBase>( pWorldItem.GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHintIcon::Set( float fAngle )
{
	// retail CHintIcon::Draw @0x210550 texture pick: on-screen -> 801 (0x321 @0x610670);
	// off-screen -> sequential arrows 802..809 (0x322..0x329) by Clamp(round(angle/45),0,7).
	if ( fAngle == -1 )
		pTexture = NDb::GetUITexture( 801 );
	else
		pTexture = NDb::GetUITexture( 802 + min( max( Float2Int( fAngle / 45 ), 0 ), 7 ) );

	SetImage( pTexture );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHintIcon::SetPosition( const SPoint &_sPosition )
{
	TBaseClass::SetSize( SPoint( 0, 0 ) );
	TBaseClass::SetPosition( _sPosition );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CHintIcon::ProcessMessage( const SEvent &sEvent )
{
	// retail @0x210510: 0x6000034 (dev EVENT_RBUTTONUP) -> mission vtbl+0xc4 FocusCameraOnItem(pWorldItem),
	// true; 0x6000035 (dev EVENT_RBUTTONDOWN) swallowed; else base decorator.
	switch ( sEvent.nEvent )
	{
	case EVENT_RBUTTONDOWN:
		return true;
	case EVENT_RBUTTONUP:
		if ( IsValid( pWorldItem ) )
			pMission->FocusCameraOnItem( pWorldItem );
		return true;
	}

	return TBaseClass::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHintIcon::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	// transient pTexture (set in the update pass); a restored icon Drawn before Set() has it null -> skip.
	if ( !IsValid( pTexture ) )
		return;
	SPoint sNewSize( pTexture->nWidth, pTexture->nHeight );
	SPoint sSize = GetSize();
	SPoint sPosition = GetPosition();

	// position/clamp are CLIENT-window-local (icons are view children, like the ear/enemy icons)
	const SPoint &sParentSize = GetParent()->GetSize();	// the client window (serialized pParent; the transient pMissionUI is NULL on a loaded icon)
	SRect sViewRect( 0, 0, sParentSize.x, sParentSize.y );
	sPosition.x = min( max( sViewRect.x1 + sNewSize.x / 2, sPosition.x ), sViewRect.x2 - sNewSize.x / 2 );
	sPosition.y = min( max( sViewRect.y1 + sNewSize.y / 2, sPosition.y ), sViewRect.y2 - sNewSize.y / 2 );

	TBaseClass::SetSize( sNewSize );
	TBaseClass::SetPosition( SPoint( sPosition.x + sSize.x / 2 - sNewSize.x / 2, sPosition.y + sSize.y / 2 - sNewSize.y / 2 ) );

	TBaseClass::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTrapIcon -- retail NUI::CTrapIcon (ctor @0x211530, ProcessMessage @0x210700, Draw @0x210790,
// GetTarget @0x216510, saveload id 0xB3618130, operator& @0x21a430 [base + pItem]): the marker over
// a KNOWN armed trap/mine -- the active player's trapped-objects set (own set traps + spotted enemy
// mines). CMissionUI::UpdateTrappedObjects @0x214990 rebuilds one per GetTrappedObjectsList entry.
// Draw @0x210790 (disasm): SINGLE fixed texture 940 (0x3ac, mov ecx,0x3ac); anchor = the trap
// object's position, z += 0.6 (fadd [0x8b1fe8]); drawn ONLY while on-screen -- unlike the clue/hint
// icons there is NO off-screen arrow set. ProcessMessage @0x210700: 0x6000034 (dev EVENT_RBUTTONUP)
// -> focus the camera on the trap position; 0x6000035 (RBUTTONDOWN) swallowed; else base decorator.
class CTrapIcon: public CProjectedIcon
{
	OBJECT_BASIC_METHODS(CTrapIcon)
private:
	ZDATA_(CProjectedIcon)
	// retail operator& @0x21a430: 1=CProjectedIcon base (carries pMission), 2=pItem.
	CPtr<CObjectBase> pItem;				// retail +0x90 (tag 2) -- the trapped object (door / mine); the UpdateHash reuse key
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CProjectedIcon*)this); f.Add(2,&pItem); return 0; }
	// transient:
	CDBPtr<NDb::CUITexture> pTexture;
	CPtr<CMissionUI> pMissionUI;			// dev icon pattern: client-rect clamp in Draw
	bool bOnScreen;							// transient draw gate (retail folds GetPositionInfo into Draw; not serialized)

public:
	CTrapIcon(): bOnScreen( false ) {}
	CTrapIcon( const SWindowInfo &sInfo, NGame::IMission *pMission, CObjectBase *pItem, CMissionUI *pMissionUI );

	bool CanHandleState( NGame::IState *pState ) const;		// retail @0x2162e0 (folded): true
	CObjectBase* GetTarget();								// retail @0x216510: pItem

	CObjectBase* GetItem() const { return pItem; }
	void Set( bool bOnScreen );
	void SetPosition( const SPoint &sPosition );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CTrapIcon::CTrapIcon( const SWindowInfo &sInfo, NGame::IMission *_pMission, CObjectBase *_pItem, CMissionUI *_pMissionUI ):
	CProjectedIcon( sInfo, _pMission ), pItem( _pItem ), pMissionUI( _pMissionUI ), bOnScreen( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTrapIcon::CanHandleState( NGame::IState *pState ) const
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CTrapIcon::GetTarget()
{	// retail @0x216510: the bare +0x90 CPtr -- the trapped world object (so the disarm state can
	// target the door/mine through its icon)
	return pItem;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTrapIcon::Set( bool _bOnScreen )
{
	// retail CTrapIcon::Draw @0x210790 texture pick: the single marker texture 940 (0x3ac) --
	// no directional arrow variants; an off-screen trap simply doesn't draw.
	bOnScreen = _bOnScreen;
	pTexture = NDb::GetUITexture( 940 );
	SetImage( pTexture );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTrapIcon::SetPosition( const SPoint &_sPosition )
{
	TBaseClass::SetSize( SPoint( 0, 0 ) );
	TBaseClass::SetPosition( _sPosition );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTrapIcon::ProcessMessage( const SEvent &sEvent )
{
	// retail @0x210700: 0x6000034 (dev EVENT_RBUTTONUP) -> focus the camera on the trap position;
	// 0x6000035 (dev EVENT_RBUTTONDOWN) swallowed; else base decorator.
	switch ( sEvent.nEvent )
	{
	case EVENT_RBUTTONDOWN:
		return true;
	case EVENT_RBUTTONUP:
	{
		CDynamicCast<NWorld::IMine> pMine( pItem.GetPtr() );
		if ( pMine && IsValid( pMission->GetCamera() ) )
		{
			ICamera::SCameraPos sPos;
			pMission->GetCamera()->GetPlacement( &sPos );
			sPos.ptAnchor = pMine->GetMinePos();
			pMission->GetCamera()->SetPlacement( sPos );
		}
		return true;
	}
	}

	return TBaseClass::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTrapIcon::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	// retail @0x210790: draw ONLY when the projected anchor is on-screen and the texture is valid
	if ( !bOnScreen || !IsValid( pTexture ) )
		return;

	SPoint sNewSize( pTexture->nWidth, pTexture->nHeight );
	SPoint sSize = GetSize();
	SPoint sPosition = GetPosition();

	// position/clamp are CLIENT-window-local (icons are view children, like the ear/enemy icons)
	const SPoint &sParentSize = GetParent()->GetSize();	// the client window (serialized pParent; the transient pMissionUI is NULL on a loaded icon)
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
	CPtr<NGame::IMission> pMission;
	////
	int nHitValue;
	// retail NUI::CHitText member (same saveload id 0xB0241949): PK hits draw ORANGE
	// (R=243,G=191,B=0 -- CHitText draw colour packing @0x6117c0) instead of the normal colour.
	bool bPK;
	CVec3 vBegPoint;
	STime sBegTime;
	bool bComplete;   // dev-only draw bookkeeping -- NOT in the retail stream, dropped from operator&
	// retail CHitText::operator& @0x219f90: 1=CText 2=pMission 3=nHitValue 4=bPK 5=vBegPoint 6=sBegTime
	// (dev wrote a DEAD pClientWindow at tag 2 -- never assigned -- and bComplete at tag 5; W3 convergence)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CText*)this); f.Add(2,&pMission); f.Add(3,&nHitValue); f.Add(4,&bPK); f.Add(5,&vBegPoint); f.Add(6,&sBegTime); return 0; }

public:
	CHitTracker(): bComplete( false ) {}	// bComplete is a format hole now -- the load path must seed it
	CHitTracker( const SWindowInfo &sInfo, NGame::IMission *pMission, NWorld::CHitLocator *pLocator, const STime &sTime );

	bool IsComplete() const;

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CHitTracker::CHitTracker( const SWindowInfo &sInfo, NGame::IMission *_pMission, NWorld::CHitLocator *pLocator, const STime &sTime ):
	CText( sInfo ), pMission( _pMission ), sBegTime( sTime ), nHitValue( pLocator->nHitValue ), bPK( pLocator->bPK ), vBegPoint( pLocator->vPosition ), bComplete( false )
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
	// retail CHitText colour packing @0x6117c0: a PK hit fades the ORANGE R=243(f3),G=191(bf),B=0
	// channels; a normal hit keeps the dev damage colour.
	if ( bPK )
		swprintf( wsText, L"<color=%.2x%.2x%.2x00>%d", nAlpha, int( ( 1 - fWeight ) * 243 ), int( ( 1 - fWeight ) * 191 ), nHitValue );
	else
		swprintf( wsText, L"<color=%.2xff0000>%d", nAlpha, nHitValue );

	SetPosition( sPosition );
	SetText( wsText );

	bComplete = false;

	CText::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMissionUI
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NUI::CMissionUI::OnSerialize @0x219e60 -- post-serialize fixup called at the tail of
// operator& (@0x218dc0): when pPause is null/dead (an older save without tag 27, or a fresh load),
// re-resolve the HUD "pause" text control by id.
void CMissionUI::OnSerialize( CStructureSaver &f )
{
	if ( !IsValid( pPause ) )
		pPause = GetUIWindow<CText>( this, "pause" );
}
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
		// retail @0x60f807/@0x60f81d: cancel probes and closes with mask -1 (every panel bit), not a named-bit union
		else if ( pMission->GetPanelState( -1 ) != 0 )
		{
			pMission->SetPanelState( -1, false );
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

			// retail @0x214d40: the ack view is built IN CODE (no game.db control) -- "ackview",
			// style 0x2e = VISIBLE|ENABLED|TOPMOST|TRANSPARENT, pos/size {0,0}, parented to this.
			pAck = new CAckIcon( SWindowInfo( this, SPoint( 0, 0 ), SPoint( 0, 0 ), "ackview", STYLE_VISIBLE | STYLE_ENABLED | STYLE_TOPMOST | STYLE_TRANSPARENT ), pMission );

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

			// retail @0x214d40 TEMPLATELOAD: the auto player-switch banner from the "playerswitch"
			// control (its nested container brings the "text"/"ok" children -- CPlayerSwitchUI builds
			// them in its own TEMPLATELOAD arm). Serialized as CMissionUI tag 25.
			pPlayerSwitchUI = new CPlayerSwitchUI( sEvent.pLoader->GetControl( "playerswitch" ), pMission );
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
				if ( sStateCursor.pCursor != GetInterface()->GetCursorInfo().pCursor )
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
	// every frame -- SetSize({(clientRect.x2-100)-(clientRect.x1+100), 0}) then
	// SetPosition({clientRect.x1+100, inventory-button bottom edge}); CAckIcon::Update's reflow
	// then measures the text and grows the band UPWARD from that bottom anchor.
	pAck->SetSize( SPoint( ( sClientRect.x2 - 100 ) - ( sClientRect.x1 + 100 ), 0 ) );
	pAck->SetPosition( SPoint( sClientRect.x1 + 100, pInventory->GetPosition().y + pInventory->GetSize().y ) );

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
	UpdateTraps();	// retail Draw @0x215b20 order: ... UpdateAudibleSounds, UpdateTrappedObjects @0x214990, UpdateCameraScroll
	UpdateCameraScroll( sTime );

	CDesktopWindow::Draw( sTime, pView );
}
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
// shared projection for the item-anchored overlay icons (the dev expression of retail
// CProjectedIcon::GetPositionInfo @0x20f610, mirrored line-for-line from UpdateEnemies/UpdateClues:
// TestRayInFrustrum -> *1024/768 -> in-rect test -> compass bearing -> clamp). Returns the
// off-screen bearing in degrees, or -1 when the projected point lies inside the view; *pIconPos
// receives the clamped 1024x768-space screen point.
static float ProjectOverlayIconPos( const CVec3 &vAnchor, CTransformStack &sTS, const CVec2 &vScreenRect, const SRect &sViewRect, CVec2 *pIconPos )
{
	CVec2 vScreenPos;
	TestRayInFrustrum( vAnchor, &sTS, vScreenRect, &vScreenPos );
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
	*pIconPos = vScreenPos;
	return fAngle;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionUI::UpdateItems( NGScene::I2DGameView *pView )
{
	// retail CMissionUI::UpdateVisibleItems @0x2130c0 -- THREE passes over the ACTIVE player's
	// ACCUMULATED discovered-objects set (GetActivePlayer -> GetPlayer -> GetVisibleObjects, the
	// merged per-unit LOS history):
	//   1. the Alt-held "Name x N" ground-item labels     (gate: bindShowItems @0x61316a/@0x6134bc);
	//   2. a CClueIcon marker per discovered IClueItem    (UNGATED -- the clue branch @0x613800 has
	//      no bool test);
	//   3. a CHintIcon marker per discovered non-clue IHintItem (gate: "ui_showhints" read as
	//      GetVar("ui_showhints").GetFloat() != 0 -- fucompp vs 0.0f @0x61312c..0x613140; NO
	//      tutorial-mode OR here, unlike the script-hint gate in iMission.cpp -- branch @0x613a38).
	// A clue item always wins over a hint item (the RTDynamicCast pair @0x6137b7 tests IClueItem
	// first). All rebuilt lists are assigned at the end of the walk (@0x613c9f).
	const bool bShowHints = NGlobal::GetVar( "ui_showhints" ).GetFloat() != 0;
	const bool bShowItems = bindShowItems.IsActive();

	// the discovered-set enumeration runs BEFORE any gate (@0x61318e..0x6131b4) -- it feeds all passes
	NWorld::IPlayer *pPlayer = 0;
	if ( pMission->GetActivePlayer() )
		pPlayer = pMission->GetActivePlayer()->GetPlayer();
	list< CPtr<CObjectBase> > visObjs;
	if ( pPlayer )
		pPlayer->GetVisibleObjects( &visObjs );

	// retail UpdateHash<CObjectBase,CHintIcon> @0x217170 / <CObjectBase,CClueIcon> @0x217220: key
	// every existing icon by its inventory item so the rebuild reuses the live widget.
	unordered_map<CPtr<NRPG::IInventoryItem>, CPtr<CHintIcon>, SPtrHash> knownHintIcons;
	for ( list<CObj<CHintIcon> >::const_iterator iIcon = hintIconsList.begin(); iIcon != hintIconsList.end(); ++iIcon )
		knownHintIcons[ (*iIcon)->GetInvItem() ] = *iIcon;
	unordered_map<CPtr<NRPG::IInventoryItem>, CPtr<CClueIcon>, SPtrHash> knownClueIcons;
	for ( list<CObj<CClueIcon> >::const_iterator iIcon = clueItemIconsList.begin(); iIcon != clueItemIconsList.end(); ++iIcon )
		knownClueIcons[ (*iIcon)->GetInvItem() ] = *iIcon;

	// PASS-1 keep step (gated): keep an existing label only while ALL its grouped items stay
	// discovered+valid; a new item merges into a label iff same DBItem AND same 1.875m ground cell
	// (F_ITEMS_SECTOR_SIZE @0x57d108, x/y only -- the z-blind grouping is a retail quirk, reproduced).
	// NOT per-selected-unit FindCloseGroundItems (that @0x369970 walk is the pickup-REACH path).
	list<CObj<CItemText> > newItemTextsList;
	unordered_map<CPtr<NRPG::IInventoryItem>, CPtr<CItemText>, SPtrHash> knownItems;
	if ( bShowItems )
	{
		for ( list<CObj<CItemText> >::const_iterator iTemp = itemTextsList.begin(); iTemp != itemTextsList.end(); iTemp++ )
		{
			CItemText *pIt = *iTemp;
			bool bKeep = !pIt->GetItems().empty();
			for ( list<CItemText::SItem>::const_iterator iS = pIt->GetItems().begin(); bKeep && iS != pIt->GetItems().end(); ++iS )
			{
				CObjectBase *pObj = CDynamicCast<CObjectBase>( iS->pWorldItem.GetPtr() );
				bKeep = IsValid( pObj ) && IsInSet( visObjs, pObj );
			}
			if ( !bKeep )
				continue;
			newItemTextsList.push_back( pIt );
			for ( list<CItemText::SItem>::const_iterator iS = pIt->GetItems().begin(); iS != pIt->GetItems().end(); ++iS )
				knownItems[ iS->pInvItem ] = pIt;
		}
	}
	else
		itemTextsList.clear();		// retail @0x6133b3: bind inactive -> drop the labels (icons still rebuild)

	// icon projection setup -- mirrors UpdateEnemies/UpdateClues (the zero-width client rect
	// early-out is the same dev projection guard those passes use)
	CVec2 vIconScreenRect = pMission->GetScene()->GetScreenRect();
	CTransformStack sIconTS = pMission->GetCameraTransform();
	SRect sIconViewRect;
	SPoint sIconViewPosition;
	GetClientWindow()->ClientToScreen( &sIconViewPosition, &sIconViewRect );
	const bool bCanProject = sIconViewRect.Width() != 0;

	list<CObj<CHintIcon> > newHintIconsList;
	list<CObj<CClueIcon> > newClueItemIconsList;

	const float F_ITEMS_SECTOR_SIZE = 1.875f;   // retail .data @0x57d108
	for ( list< CPtr<CObjectBase> >::iterator iObj = visObjs.begin(); iObj != visObjs.end(); ++iObj )
	{
		CDynamicCast<NWorld::IItem> pWItem( iObj->GetPtr() );
		if ( !IsValid( pWItem ) )
			continue;                            // mines/units/non-items fall out here
		NRPG::IInventoryItem *pInv = pWItem->GetInvItem();
		if ( !IsValid( pInv ) )
			continue;
		// PASS 1 (gated @0x6134bc): merge-or-create the ground-item label
		if ( bShowItems && knownItems.find( pInv ) == knownItems.end() )
		{
			int nSX = (int)( pWItem->GetPos().x / F_ITEMS_SECTOR_SIZE + 0.5f );
			int nSY = (int)( pWItem->GetPos().y / F_ITEMS_SECTOR_SIZE + 0.5f );
			CItemText *pMerge = 0;
			for ( list<CObj<CItemText> >::iterator iN = newItemTextsList.begin(); !pMerge && iN != newItemTextsList.end(); ++iN )
			{
				const CItemText::SItem &sF = (*iN)->GetItems().front();
				if ( !IsValid( sF.pInvItem ) || !IsValid( sF.pWorldItem ) )
					continue;
				if ( sF.pInvItem->GetDBItem() != pInv->GetDBItem() )
					continue;
				if ( (int)( sF.pWorldItem->GetPos().x / F_ITEMS_SECTOR_SIZE + 0.5f ) == nSX
				  && (int)( sF.pWorldItem->GetPos().y / F_ITEMS_SECTOR_SIZE + 0.5f ) == nSY )
					pMerge = *iN;
			}
			if ( pMerge )
				pMerge->AddItem( pWItem, pInv );
			else
				// CLIENT-window parent = retail @0x2130c0 (GetClientWindow -> SWindowInfo). A
				// desktop-parented label never receives EVENT_MOUSEENTER (see the enemy-icon note
				// below), which is exactly why the dev labels were unclickable.
				newItemTextsList.push_back( new CItemText( SWindowInfo( GetClientWindow(), SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), pMission, pWItem, pInv ) );
		}

		// PASSES 2+3: classify the INVENTORY item (retail RTDynamicCast pair @0x6137b7; clue wins)
		NRPG::IClueItem *pClue = dynamic_cast<NRPG::IClueItem*>( pInv );
		NRPG::IHintItem *pHint = dynamic_cast<NRPG::IHintItem*>( pInv );
		if ( !bCanProject || ( !pClue && !pHint ) )
			continue;

		// anchor = item pos raised 0.6 (retail CClueIcon::Draw fadd [0x8b1fe8] @0x61039b /
		// CHintIcon::Draw @0x61058a -- both add the same 0.6f before projecting)
		CVec3 vIconAnchor( pWItem->GetPos() );
		vIconAnchor += CVec3( 0, 0, 0.6f );
		CVec2 vIconScreenPos;
		float fAngle = ProjectOverlayIconPos( vIconAnchor, sIconTS, vIconScreenRect, sIconViewRect, &vIconScreenPos );
		SPoint sIconPos;	// icons are CLIENT-window children -- convert the 1024x768 screen point
		GetClientWindow()->ScreenToClient( SPoint( vIconScreenPos.x, vIconScreenPos.y ), &sIconPos );

		if ( pClue )
		{
			// PASS 2 (ungated, @0x613800): reuse the keyed icon or build a fresh one (@0x61391b ctor)
			CClueIcon *pIcon = 0;
			unordered_map<CPtr<NRPG::IInventoryItem>, CPtr<CClueIcon>, SPtrHash>::iterator iKnown = knownClueIcons.find( pInv );
			if ( iKnown != knownClueIcons.end() )
				pIcon = iKnown->second;
			if ( !pIcon )
				pIcon = new CClueIcon( SWindowInfo( GetClientWindow(), SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), pMission, pWItem, pInv, this );
			pIcon->Set( fAngle );
			pIcon->SetPosition( sIconPos );
			newClueItemIconsList.push_back( pIcon );
		}
		else if ( bShowHints )
		{
			// PASS 3 (gated by ui_showhints, @0x613a38): reuse or build (@0x613b5a ctor)
			CHintIcon *pIcon = 0;
			unordered_map<CPtr<NRPG::IInventoryItem>, CPtr<CHintIcon>, SPtrHash>::iterator iKnown = knownHintIcons.find( pInv );
			if ( iKnown != knownHintIcons.end() )
				pIcon = iKnown->second;
			if ( !pIcon )
				pIcon = new CHintIcon( SWindowInfo( GetClientWindow(), SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), pMission, pWItem, pInv, this );
			pIcon->Set( fAngle );
			pIcon->SetPosition( sIconPos );
			newHintIconsList.push_back( pIcon );
		}
	}

	// retail assigns every rebuilt list at the end of the walk (@0x613c9f); with a gate off its
	// fresh list is simply empty, which clears the member -- reproduced.
	hintIconsList = newHintIconsList;
	clueItemIconsList = newClueItemIconsList;

	if ( bShowItems )
	{
		itemTextsList = newItemTextsList;

		const int
			N_X_STEP = 4,
			N_Y_STEP = 16,
			N_X_SIZE = 1024 / N_X_STEP,
			N_Y_SIZE = 768 / N_Y_STEP;

		CArray2D<bool> sMap( N_X_SIZE, N_Y_SIZE );
		sMap.FillEvery( false );

		CVec2 vScreenRect = pView->GetViewportSize();
		CTransformStack sTS = pMission->GetCameraTransform();
		for ( list<CObj<CItemText> >::const_iterator iTemp = itemTextsList.begin(); iTemp != itemTextsList.end(); iTemp++ )
		{
			CItemText *pItemText = *iTemp;

			if ( pItemText->GetItems().empty() || !IsValid( pItemText->GetItems().front().pWorldItem ) )
			{
				pItemText->SetStyle( STYLE_VISIBLE, false );
				continue;
			}
			CVec2 vRes;
			if ( !TestRayInFrustrum( pItemText->GetItems().front().pWorldItem->GetPos(), &sTS, vScreenRect, &vRes ) )
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
			SPoint sClientPos;   // retail ReflowItemTexts @0x20faa0: labels are CLIENT-window children --
			GetClientWindow()->ScreenToClient( SPoint( sItemPos.x * N_X_STEP, sItemPos.y * N_Y_STEP ), &sClientPos );
			pItemText->SetPosition( sClientPos );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionUI::UpdateHits( const STime &sTime )
{
	// retail UpdateHits @0x6129b0: GetActivePlayer()->GetPlayer()->GetVisible() once up front;
	// a hit on a unit NOT in that list spawns no damage number (unit-less hits always spawn)
	NWorld::IPlayer *pPlayer = 0;
	list< CPtr<NWorld::CUnit> > visibleList;
	if ( pMission->GetActivePlayer() )
	{
		pPlayer = pMission->GetActivePlayer()->GetPlayer();
		if ( pPlayer )
			pPlayer->GetVisible( &visibleList );
	}

	CPtr<NWorld::CHitLocator> pTempLocator;
	while( pTempLocator = pMission->GetWorld()->GetHitEvent() )
	{
		if ( pTempLocator->pUnit != 0 && pPlayer != 0 &&
			 find( visibleList.begin(), visibleList.end(), pTempLocator->pUnit ) == visibleList.end() )
			continue;
		hitsList.push_back( new CHitTracker( SWindowInfo( GetClientWindow(), SPoint( 0, 0 ), SPoint( 120, 20 ), "hit", STYLE_ENABLED | STYLE_TRANSPARENT | STYLE_TOPMOST | STYLE_VISIBLE ), pMission, pTempLocator, sTime ) );
	}

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
	list<CObj<CSoundIcon> > newClueIconsList;
	list<CObj<CSoundIcon> >::iterator iOldIcons = clueIconsList.begin();
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

		CSoundIcon *pIcon;
		if ( iOldIcons != clueIconsList.end() )
		{
			pIcon = (*iOldIcons);
			iOldIcons++;
		}
		else
		{
			// client-window parent for the same reason as CEnemyIcon above (retail @0x213e70): the
			// decorator hover push is what makes the heard silhouette targetable through its ear icon.
			pIcon = new CSoundIcon( SWindowInfo( GetClientWindow(), SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), pMission, this );
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
void CMissionUI::UpdateTraps()
{
	// retail CMissionUI::UpdateTrappedObjects @0x214990 (there gated on the bShowIcons global; this
	// fork's overlay-icon passes are ungated, matching UpdateEnemies/UpdateClues): one CTrapIcon per
	// entry of the ACTIVE player's GetTrappedObjectsList (@0x387330: live + IMine + IsMineSet -- own
	// armed traps and spotted enemy mines), keyed-reuse by the trapped object (retail
	// UpdateHash<CObjectBase,CTrapIcon> @0x2173b0), anchored at the trap position z+0.6
	// (CTrapIcon::Draw @0x210790), drawn only while on-screen.
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
		trapIconsList.clear();
		return;
	}

	list< CPtr<CObjectBase> > traps;
	pPlayer->GetTrappedObjectsList( &traps );

	// keyed reuse: index the existing icons by their trapped object (retail UpdateHash @0x2173b0)
	unordered_map<CPtr<CObjectBase>, CPtr<CTrapIcon>, SPtrHash> knownIcons;
	for ( list<CObj<CTrapIcon> >::const_iterator iIcon = trapIconsList.begin(); iIcon != trapIconsList.end(); ++iIcon )
		knownIcons[ (*iIcon)->GetItem() ] = *iIcon;

	list<CObj<CTrapIcon> > newTrapIconsList;
	for ( list< CPtr<CObjectBase> >::iterator iObj = traps.begin(); iObj != traps.end(); ++iObj )
	{
		CObjectBase *pObj = iObj->GetPtr();
		CDynamicCast<NWorld::IMine> pMine( pObj );
		if ( !pMine )
			continue;   // GetTrappedObjectsList already filters, but the cast also yields GetMinePos

		CTrapIcon *pIcon;
		unordered_map<CPtr<CObjectBase>, CPtr<CTrapIcon>, SPtrHash>::iterator iKnown = knownIcons.find( pObj );
		if ( iKnown != knownIcons.end() )
			pIcon = iKnown->second;
		else
			pIcon = new CTrapIcon( SWindowInfo( GetClientWindow(), SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), pMission, pObj, this );

		// anchor: the armed trap's position, +0.6 up (retail CTrapIcon::Draw @0x210790)
		CVec3 vIconPos = pMine->GetMinePos();
		vIconPos.z += 0.6f;

		CVec2 vScreenPos;
		float fAngle = ProjectOverlayIconPos( vIconPos, sTS, vScreenRect, sViewRect, &vScreenPos );

		pIcon->Set( fAngle == -1 );   // on-screen only -- retail draws no off-screen arrows for traps
		SPoint sIconPos;
		GetClientWindow()->ScreenToClient( SPoint( vScreenPos.x, vScreenPos.y ), &sIconPos );
		pIcon->SetPosition( sIconPos );
		newTrapIconsList.push_back( pIcon );
	}

	trapIconsList = newTrapIconsList;
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
	// ACK FOG-OF-WAR (retail CMissionUI::PlayAckEvent @0x211930): bark an ack ONLY if its SPEAKER is
	// currently visible to the active player -- GetActivePlayer()->IsUnitVisible(pEvent->pUnit) (retail
	// mission vtbl+0x5c = GetActivePlayer, tracker vtbl+0x2c = IsUnitVisible). The player's own units are
	// always in its GetVisible set so their acks still bark; enemy chatter from a unit you (and your allies)
	// cannot see is DROPPED (no voice, no subtitle). Retail returns 0 on the gated path, and the caller
	// (CDesktopWindow::Update) just stores it as the active event, so returning 0 is safe.
	if ( IsValid( pEvent ) && IsValid( pMission ) )
	{
		if ( pMission->GetActivePlayer() == 0 ||
			 !pMission->GetActivePlayer()->IsUnitVisible( pEvent->pUnit.GetPtr() ) )
			return 0;
	}
	// retail: the NUI wrapper is constructed around the world-side ack with bReady=false. It is NOT
	// armed here -- bReady flips later (via the single-unit face's ACK_WAIT step, or its fallback),
	// so the deferred voice/lipsync only fire once the speaker's face is on screen.
	CAckEvent *pAckEvent = new CAckEvent( pEvent );

	// (a) the on-screen subtitle band (CAckIcon == retail CAckView: subtitle text + voice)
	pAck->Set( pAckEvent );

	// (b) release CMissionUI::PlayAckEvent @0x211930 also drives the bottom-LEFT single-unit face: the
	// 3D portrait turns to the camera to deliver the line, temporarily swapping in a non-selected (or
	// enemy) speaker's head. Retail gates this on bShowAcks ("ui_charresponses", default on); with the
	// face path off there is no ACK_WAIT handshake, so retail arms the event DIRECTLY here
	// (CAckEvent::Set @0x1d0cf0: bReady=true, TTL = now + 3000ms) -- the voice/subtitle still play.
	if ( NGlobal::GetVar( "ui_charresponses" ).GetFloat() != 0 )
		pUnitPanel->PlayAckEvent( sTime, pAckEvent );
	else
		pAckEvent->Set( sTime );

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
REGISTER_SAVELOAD_CLASS( 0xB3131150, CSoundIcon );	// dev-established id (class formerly named CClueIcon here)
REGISTER_SAVELOAD_CLASS( 0xB3123180, CClueIcon );	// retail NUI::CClueIcon id (gen/classreg.json)
REGISTER_SAVELOAD_CLASS( 0xB3212140, CHintIcon );	// retail NUI::CHintIcon id (gen/classreg.json)
REGISTER_SAVELOAD_CLASS( 0xB3618130, CTrapIcon );	// retail NUI::CTrapIcon id (register thunk @0x4a07b0)
