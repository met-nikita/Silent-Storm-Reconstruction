#include "StdAfx.h"
#include "Transform.h"
#include "GView.h"
#include "G2DView.h"
#include "GSceneUtils.h"
#include "wInterface.h"
#include "Sound.h"
#include "RWGame.h"
#include "RWSound.h"
#include "RPGGame.h"
#include "RPGGlobal.h"
#include "RPGMerc.h"
#include "Interface.h"
#include "iMain.h"
#include "iCharGen.h"
#include "iFaceGen.h"
#include "iHeroMenu.h"
#include "iRenderWorld.h"
#include "iCommonUI.h"
#include "iDesktopWindow.h"
#include "iGlobalMap.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataDifficulty.h"
#include "..\DBFormat\DataSound.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataCamera.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_HEROMENU_CAMERA = 44;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScriptButton
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScriptButton: public CButton
{
	OBJECT_BASIC_METHODS(CScriptButton);
private:
	ZDATA_(CButton)
	CPtr<NGame::CRenderBaseInterface> pInterface;
	////
	bool bHoverNotify;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CButton*)this); f.Add(2,&pInterface); f.Add(3,&bHoverNotify); return 0; }

protected:
	virtual void OnAction();

public:
	CScriptButton() {}
	CScriptButton( const SWindowInfo &sInfo, NGame::CRenderBaseInterface *pInterface );
	// 3-arg form (retail @0x1e7630): same as above plus a hover tooltip built from a DB string -- used for
	// the HeroMenu preset portraits, whose tooltip is the preset's class description. pToolTip may be null.
	CScriptButton( const SWindowInfo &sInfo, NGame::CRenderBaseInterface *pInterface, NDb::CString *pToolTip );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CScriptButton::CScriptButton( const SWindowInfo &sInfo, NGame::CRenderBaseInterface *_pInterface ):
	CButton( sInfo ), pInterface( _pInterface ), bHoverNotify( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// 3-arg ctor (retail @0x1e7630): the 2-arg form plus a hover tooltip built from a DB string. The HeroMenu
// passes pSide->defaultPersToolTipsSet[i] (the preset's class description). GetDBString(NDb::CString*) is
// null-safe (returns an empty string for a null/hidden id), so a side without authored tooltips just shows
// none -- matching retail. SetToolTip stores the CToolTip as a hidden child shown on hover.
CScriptButton::CScriptButton( const SWindowInfo &sInfo, NGame::CRenderBaseInterface *_pInterface, NDb::CString *pToolTip ):
	CButton( sInfo ), pInterface( _pInterface ), bHoverNotify( false )
{
	CPtr<CToolTip> pTT = new CToolTip( SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 0, 0 ), "tooltip", STYLE_ENABLED ) );
	pTT->SetText( GetDBString( pToolTip ) );
	SetToolTip( pTT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScriptButton::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( IsMouseCover() != bHoverNotify )
	{
		NWorld::CCommand *pCmd = 
			new NWorld::CCmdCallScriptFunction( "OnScriptNotify", "si", GetWindowID().c_str(), IsMouseCover() ? 1 : 2 );
		pInterface->Command( pCmd );
	}

	bHoverNotify = IsMouseCover();

	CButton::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScriptButton::OnAction()
{
	NWorld::CCommand *pCmd = new NWorld::CCmdCallScriptFunction( "OnScriptNotify", "si", GetWindowID().c_str(), 0 );
	pInterface->Command( pCmd );
	CButton::OnAction();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeroMenuUI
////////////////////////////////////////////////////////////////////////////////////////////////////
// The retail hero-select container (354) ships no static cancel/customchar/play controls (the dev
// looked them up by name -> dead buttons; same bug class as the main menu + side menu). The release
// rebuilds them PROGRAMMATICALLY in a single CButtonsLine over the "line" template control, and adds a
// selected-nationality member. Base is CDesktopWindow (the script-UI host, as in the side menu).
// operator& 13 -> 15 tags (@0x1e9050): the dev's pSide/pInterface/pSelectedPers reorders to
// pSide/pSelectedPers/pNationality/pInterface and inserts pNationality(4) + pButtonsLine(9).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHeroMenuUI: public CDesktopWindow
{
	OBJECT_BASIC_METHODS(CHeroMenuUI);
private:
	ZDATA_(CDesktopWindow)
	CDBPtr<NDb::CSide> pSide;
	CDBPtr<NDb::CRPGPers> pSelectedPers;
	CDBPtr<NDb::CNationality> pNationality;
	CPtr<NGame::CRenderBaseInterface> pInterface;
	////
	CObj<CHoverButton> pBack;
	CObj<CHoverButton> pPlay;
	CObj<CHoverButton> pCustomChar;
	CObj<CButtonsLine> pButtonsLine;
	CObj<CScriptButton> pNat1Male;
	CObj<CScriptButton> pNat1Female;
	CObj<CScriptButton> pNat2Male;
	CObj<CScriptButton> pNat2Female;
	CObj<CScriptButton> pNat3Male;
	CObj<CScriptButton> pNat3Female;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDesktopWindow*)this); f.Add(2,&pSide); f.Add(3,&pSelectedPers); f.Add(4,&pNationality); f.Add(5,&pInterface); f.Add(6,&pBack); f.Add(7,&pPlay); f.Add(8,&pCustomChar); f.Add(9,&pButtonsLine); f.Add(10,&pNat1Male); f.Add(11,&pNat1Female); f.Add(12,&pNat2Male); f.Add(13,&pNat2Female); f.Add(14,&pNat3Male); f.Add(15,&pNat3Female); return 0; }
public:
	CHeroMenuUI() {}
	CHeroMenuUI( const SWindowInfo &sInfo, NGame::CRenderBaseInterface *pInterface, NDb::CSide *pSide );

	NDb::CRPGPers* GetPers() const;
	NDb::CNationality* GetNationality() const;   // @0x1e6f00: feeds the downstream char/face-gen

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CHeroMenuUI::CHeroMenuUI( const SWindowInfo &sInfo, NGame::CRenderBaseInterface *_pInterface, NDb::CSide *_pSide ):
	CDesktopWindow( sInfo ), pInterface( _pInterface ), pSide( _pSide )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CRPGPers* CHeroMenuUI::GetPers() const
{
	return pSelectedPers;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CNationality* CHeroMenuUI::GetNationality() const
{
	return pNationality;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CHeroMenuUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			// Selecting a nationality portrait sets BOTH the chosen pers and its nationality (the
			// release reads pSide->pNationality{1,2,3} @+0x1c/+0x20/+0x24 + defaultPersesSet[0..5] @+0x40).
			if ( sEvent.szID == "n1male" )
			{
				pNationality = pSide->pNationality1;
				pSelectedPers = pSide->defaultPersesSet[0];
			}
			else if ( sEvent.szID == "n1female" )
			{
				pNationality = pSide->pNationality1;
				pSelectedPers = pSide->defaultPersesSet[1];
			}
			else if ( sEvent.szID == "n2male" )
			{
				pNationality = pSide->pNationality2;
				pSelectedPers = pSide->defaultPersesSet[2];
			}
			else if ( sEvent.szID == "n2female" )
			{
				pNationality = pSide->pNationality2;
				pSelectedPers = pSide->defaultPersesSet[3];
			}
			else if ( sEvent.szID == "n3male" )
			{
				pNationality = pSide->pNationality3;
				pSelectedPers = pSide->defaultPersesSet[4];
			}
			else if ( sEvent.szID == "n3female" )
			{
				pNationality = pSide->pNationality3;
				pSelectedPers = pSide->defaultPersesSet[5];
			}

			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			// The navigable buttons live in one CButtonsLine over the "line" template control (the
			// main-menu/side-menu pattern). Add order = cancel / customchar / play (release @0x1e77b0).
			pButtonsLine = new CButtonsLine( sEvent.pLoader->GetControl( "line" ) );

			// Gold-Impact markup = the shared "Menu - ButtonState - *" strings 11129 Normal / 11130 Hover /
			// 17339 Disabled. The dev's 11133/11134 ids DO NOT EXIST in the Strings table -> empty markup
			// prefix -> BACK/NEXT rendered in the default font. The customchar caption is 17340 "CUSTOM
			// CHARACTER" ([CharGen]); the dev's 11136 also doesn't exist -> the centre "Create Character"
			// button got no caption (+ default font) and looked entirely missing.
			pBack = pButtonsLine->AddHoverButton( "cancel", 19243,
				GetDBString( 11129 ) + GetDBString( 11135 ),
				GetDBString( 11130 ) + GetDBString( 11135 ),
				GetDBString( 17339 ) + GetDBString( 11135 ) );

			pCustomChar = pButtonsLine->AddHoverButton( "customchar", 19244,
				GetDBString( 11129 ) + GetDBString( 17340 ),
				GetDBString( 11130 ) + GetDBString( 17340 ),
				GetDBString( 17339 ) + GetDBString( 17340 ) );

			pPlay = pButtonsLine->AddHoverButton( "play", 19245,
				GetDBString( 11129 ) + GetDBString( 11137 ),
				GetDBString( 11130 ) + GetDBString( 11137 ),
				GetDBString( 17339 ) + GetDBString( 11137 ) );

			// The six in-3D-view nationality portraits (notify the menu script on hover/click), each with
			// the preset's class-description tooltip from pSide->defaultPersToolTipsSet[i] (retail @0x1e77b0
			// passes pSide+0x78[0..5] as the 3rd ctor arg). The Steam game.db authors these per-portrait
			// strings (CSide columns NationalityNMale/FemaleToolTip; e.g. Axis 19237..19242); a side may
			// leave them empty (test/demo sides) -> GetDBString yields an empty tooltip. Bounds-checked so
			// it stays orphan-safe for a short/empty vector or a v0 db.
			const vector<CPtr<NDb::CString> > &tips = pSide->defaultPersToolTipsSet;
			pNat1Male   = new CScriptButton( sEvent.pLoader->GetControl( "n1male" ),   pInterface, tips.size() > 0 ? (NDb::CString*)tips[0] : 0 );
			pNat1Female = new CScriptButton( sEvent.pLoader->GetControl( "n1female" ), pInterface, tips.size() > 1 ? (NDb::CString*)tips[1] : 0 );
			pNat2Male   = new CScriptButton( sEvent.pLoader->GetControl( "n2male" ),   pInterface, tips.size() > 2 ? (NDb::CString*)tips[2] : 0 );
			pNat2Female = new CScriptButton( sEvent.pLoader->GetControl( "n2female" ), pInterface, tips.size() > 3 ? (NDb::CString*)tips[3] : 0 );
			pNat3Male   = new CScriptButton( sEvent.pLoader->GetControl( "n3male" ),   pInterface, tips.size() > 4 ? (NDb::CString*)tips[4] : 0 );
			pNat3Female = new CScriptButton( sEvent.pLoader->GetControl( "n3female" ), pInterface, tips.size() > 5 ? (NDb::CString*)tips[5] : 0 );
			break;
		}
	}

	return CDesktopWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeroMenuUI::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	pPlay->SetStyle( STYLE_ENABLED, IsValid( pSelectedPers ) );
	CDesktopWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeroMenuInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHeroMenuInterface: public CRenderBaseInterface
{
	OBJECT_BASIC_METHODS(CHeroMenuInterface);
private:
	NInput::CBind bindClose, bindPlay, bindCustomChar;

	ZDATA_(CRenderBaseInterface)
	CDBPtr<NDb::CSide> pSide;
	CDBPtr<NDb::CDBDifficulty> pDifficulty;
	////
	CObj<NUI::CHeroMenuUI> pHeroMenuUI;
	CPtr<NRPG::CGlobalPlayer> pGlobalPlayer;
public:
	// operator& @0x1e9240: 1 base, 2 pSide, 3 pDifficulty (NEW), 4 pHeroMenuUI. The release DROPS
	// pGlobalPlayer from serialize (it stays a live member, re-created in Initialize) + shifts pHeroMenuUI to tag 4.
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CRenderBaseInterface*)this); f.Add(2,&pSide); f.Add(3,&pDifficulty); f.Add(4,&pHeroMenuUI); return 0; }

public:
	CHeroMenuInterface();

	void Initialize( NDb::CSide *pSide, NDb::CDBDifficulty *pDifficulty );

	void Step();
	bool ProcessEvent( const NInput::SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeroMenuInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
CHeroMenuInterface::CHeroMenuInterface():
	bindClose( "cancel" ), bindPlay( "play" ), bindCustomChar( "customchar" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeroMenuInterface::Initialize( NDb::CSide *_pSide, NDb::CDBDifficulty *_pDifficulty )
{
	pSide = _pSide;
	pDifficulty = _pDifficulty;   // release @0x1e73d0: stored before CRenderBaseInterface::Initialize

	CRenderBaseInterface::Initialize( pSide->nHeroSelectTemplate );

	CPtr<NDb::CDBCamera> pDBCamera = NDb::GetDBCamera( N_HEROMENU_CAMERA );
	ICamera::SCameraPos sCameraPos( pDBCamera->vAnchor, pDBCamera->fDistance, pDBCamera->fPitch, pDBCamera->fYaw, pDBCamera->fRoll, pDBCamera->fFOV );
	GetCamera()->SetPlacement( sCameraPos );

	pGlobalPlayer = NRPG::CreateGlobalPlayer();

	pHeroMenuUI = new NUI::CHeroMenuUI( NUI::SWindowInfo( GetInterface(), NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "heromenuUI" ), this, pSide );
	NUI::LoadTemplate( pHeroMenuUI, NDb::GetUIContainer( 354 ) );
	pHeroMenuUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CHeroMenuInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	NInput::SetSection( "menu" );

	if ( CRenderBaseInterface::ProcessEvent( sEvent ) )
		return true;

	if ( bindClose.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NMainLoop::CICExitModal() ); 
		return true;
	}
	else if ( bindPlay.ProcessEvent( sEvent ) )
	{
		if ( !IsValid( pHeroMenuUI->GetPers() ) )
			return true;

		// Release @0x1e70b0: create the hero merc from the selected pers + thread it (+ nationality/difficulty)
		// into the merc-based CICFaceGen.
		NMainLoop::Command( new NGame::CICFaceGen( pSide, pHeroMenuUI->GetNationality(), pDifficulty,
			NRPG::CreateMerc( pHeroMenuUI->GetPers(), 0, true ) ) );
		return true;
	}
	else if ( bindCustomChar.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NGame::CICCharGen( pSide, pDifficulty ) );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeroMenuInterface::Step()
{
	CRenderBaseInterface::Step();

	if ( CanRender() )
	{
		// Render the 3D world full-screen (the release has NO Step override @0x1e7xxx -> renders via the
		// base; same fix as the side menu, a5dll 955aa18). The predecessor derived a sub-rect from a
		// "clientview" UI control, but the retail hero-select container (354) ships no such control ->
		// GetUIWindow fell back to a zero-size window -> zero camera screen-rect -> no 3D backdrop +
		// "control clientview not found" spam.
		GetCamera()->SetScreenRect( CTRect<float>( 0.0f, 0.0f, 1.0f, 1.0f ) );

		RenderFrame( GetTime(), GetCamera() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICMainMenu
////////////////////////////////////////////////////////////////////////////////////////////////////
CICHeroMenu::CICHeroMenu( NDb::CSide *_pSide, NDb::CDBDifficulty *_pDifficulty ):
	pSide( _pSide ), pDifficulty( _pDifficulty )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICHeroMenu::Exec()
{
	CHeroMenuInterface *pRes = new CHeroMenuInterface;
	pRes->Initialize( pSide, pDifficulty );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB1112190, CHeroMenuUI );
REGISTER_SAVELOAD_CLASS( 0xB1112191, CScriptButton );
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB111219A, CHeroMenuInterface );
