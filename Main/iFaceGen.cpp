#include "StdAfx.h"
#include "Gfx.h"
#include "Transform.h"
#include "GView.h"
#include "GSceneUtils.h"
#include "iMain.h"
#include "G2DView.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataDifficulty.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "wInterface.h"
#include "Sound.h"
#include "RPGUnit.h"
#include "RPGMerc.h"
#include "RPGGlobal.h"
#include "RWGame.h"
#include "RWSound.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iDesktopWindow.h"
#include "iFaceGen.h"
#include "iAdvFaceGen.h"
#include "iRenderWorld.h"
#include "iGlobalMap.h"
#include "..\DBFormat\DataCamera.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
const int
	N_MAX_HEADS = 6,
	N_MAX_VOICES = 3;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFaceGenScroll
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFaceGenScroll: public CScroll
{
	OBJECT_BASIC_METHODS(CFaceGenScroll);
private:
	ZDATA_(CScroll)
	CPtr<CProgressBar> pBar;
	CObj<CFlashButton> pPlus;
	CObj<CFlashButton> pMinus;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CScroll*)this); f.Add(2,&pBar); f.Add(3,&pPlus); f.Add(4,&pMinus); return 0; }

public:
	CFaceGenScroll() {}
	CFaceGenScroll( const SWindowInfo &sInfo );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CFaceGenScroll::CFaceGenScroll( const SWindowInfo &sInfo ):
	CScroll( sInfo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFaceGenScroll::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pPlus = new CFlashButton( sEvent.pLoader->GetControl( "plus" ) );
			pMinus = new CFlashButton( sEvent.pLoader->GetControl( "minus" ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pBar = GetUIWindow<CProgressBar>( this, "progress" );
			break;
		}
	}

	return CScroll::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFaceGenScroll::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	SetPageStep( 1 );

	pBar->SetValue( float( GetValue() + 1 ) / ( GetMaxValue() + 1 ) );
	pPlus->SetStyle( STYLE_VISIBLE, GetValue() != GetMaxValue() );
	pMinus->SetStyle( STYLE_VISIBLE, GetValue() != 0 );

	CScroll::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFaceGenUI
////////////////////////////////////////////////////////////////////////////////////////////////////
// Reparented to CDesktopWindow + merc-based ctor (release @0x1d3540: CFaceGenUI(SWindowInfo, CUnit*)). The
// pers/head are derived from the merc. The customhead button + the 3 voice CHoverCheckButton (release controls)
// are gated by the still-absent CICAdvFaceGen + NUI::CHoverCheckButton -- for now the working dev controls
// (play/cancel + face/voice CFaceGenScroll) drive the display via the CreateMerc recreate path.
class CFaceGenUI: public CDesktopWindow
{
	OBJECT_BASIC_METHODS(CFaceGenUI);
private:
	ZDATA_(CDesktopWindow)
	int nVoice;
	CObj<NRPG::CUnit> pMerc;
	CDBPtr<NDb::CComplexHead> pHead;
	vector<CDBPtr<NDb::CComplexHead> > customHeads;
	////
	bool bChanged;
	////
	CObj<NRPG::CUnit> pTempMerc;
	CObj<CHoverButton> pPlay;
	CObj<CHoverButton> pBack;
	CObj<CHoverButton> pCustomHead;    // opens the advanced ("custom head") face editor (NGame::CICAdvFaceGen)
	CObj<CUnitView> pUnitView;
	CObj<CFaceGenScroll> pFaceScroll;
	CObj<CHoverCheckButton> pVoice1;   // release: the voice scroll was replaced by 3 voice checkbuttons
	CObj<CHoverCheckButton> pVoice2;
	CObj<CHoverCheckButton> pVoice3;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDesktopWindow*)this); f.Add(2,&nVoice); f.Add(3,&pMerc); f.Add(4,&pHead); f.Add(5,&customHeads); f.Add(6,&bChanged); f.Add(7,&pTempMerc); f.Add(8,&pPlay); f.Add(9,&pBack); f.Add(10,&pUnitView); f.Add(11,&pFaceScroll); f.Add(12,&pVoice1); f.Add(13,&pVoice2); f.Add(14,&pVoice3); f.Add(15,&pCustomHead); return 0; }

protected:
	void UpdateUnit();
	void UpdateVoiceChecks();

public:
	CFaceGenUI() {}
	CFaceGenUI( const SWindowInfo &sInfo, NRPG::CUnit *pMerc );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CFaceGenUI::CFaceGenUI( const SWindowInfo &sInfo, NRPG::CUnit *_pMerc ):
	CDesktopWindow( sInfo ), pMerc( _pMerc ), nVoice( 0 ), bChanged( false )
{
	NDb::CRPGPers *pPers = pMerc->GetPers();
	pHead = pPers->pHead;

	// The retail CNationality DROPPED its per-nationality customMale/FemaleHeads vectors; the selectable
	// heads are now gathered from the GLOBAL CComplexHead table, filtered to listable heads matching the
	// pers's gender (release CFaceGenUI ctor @0x1d3540: skip !bCanBeListed, keep bIsFemale == pPers->bIsFemale).
	CDBTable<NDb::CComplexHead> *pHeadTable = NDatabase::GetTable<NDb::CComplexHead>();
	CDBIterator<NDb::CComplexHead> iTempHead( *pHeadTable );
	while( iTempHead.MoveNext() )
	{
		NDb::CComplexHead *pComplexHead = iTempHead.Get();
		if ( !pComplexHead->bCanBeListed )
			continue;
		if ( pComplexHead->bIsFemale != pPers->bIsFemale )
			continue;

		customHeads.push_back( pComplexHead );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFaceGenUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "face" )
			{
				int nValue = pFaceScroll->GetValue();
				ASSERT( nValue < customHeads.size() );
				if ( nValue < customHeads.size() )
					pHead = customHeads[nValue];

				UpdateUnit();
				return true;
			}
			// Release: the voice scroll became 3 voice checkbuttons (voice1/2/3 -> nVoice 0/1/2), a radio
			// group. Retail CFaceGenUI::ProcessMessage @0x1d27e0 on a voice click does three things: set
			// nVoice, propagate it to the merc (CUnit::SetVoice @0x2bb1f0), and PLAY that voice's "command
			// acknowledged" preview ack (GetPersAck @0x2452c0 -> CWindow::PlaySound). Mirrors the advanced
			// editor (iAdvFaceGen.cpp, commit 4f15d64). No unit recreate -- voice does not affect the head,
			// and retail returns right after PlaySound -- so the old UpdateUnit() (a wasteful CreateMerc) is
			// dropped.
			else if ( sEvent.szID == "voice1" || sEvent.szID == "voice2" || sEvent.szID == "voice3" )
			{
				nVoice = ( sEvent.szID == "voice1" ) ? 0 : ( sEvent.szID == "voice2" ) ? 1 : 2;
				if ( IsValid( pMerc ) )
				{
					pMerc->SetVoice( nVoice );
					PlaySound( GetPersVoiceAck( pMerc ) );   // PlaySound null-guards a missing ack
				}
				UpdateVoiceChecks();
				return true;
			}

			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			// Gold-Impact button markup = the shared "Menu - ButtonState - *" strings (11129 Normal /
			// 11130 Hover / 17339 Disabled). The dev's 10892/10893/10894 ids DON'T EXIST in the Strings
			// table -> empty markup prefix -> play/cancel/customhead rendered in the default font.
			pPlay = new CHoverButton( sEvent.pLoader->GetControl( "play" ) );
			pPlay->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11130 ) + GetDBString( 10896 ) );
			pPlay->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11129 ) + GetDBString( 10896 ) );
			pPlay->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 17339 ) + GetDBString( 10896 ) );

			pBack = new CHoverButton( sEvent.pLoader->GetControl( "cancel" ) );
			pBack->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11130 ) + GetDBString( 10895 ) );
			pBack->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11129 ) + GetDBString( 10895 ) );
			pBack->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 17339 ) + GetDBString( 10895 ) );

			// "customhead" button -> opens the advanced face editor. Its click fires the "customhead" input
			// action, caught by CFaceGenMenuInterface::bindCustomHead. Caption = GetDBString(18753) "CUSTOM HEAD"
			// (verified from the retail content DB; the retail decomp gives it HOVER + NORMAL text states, no
			// DISABLED, mirroring pBack); the rich hover tooltip (string 19441) is data-driven by the control.
			pCustomHead = new CHoverButton( sEvent.pLoader->GetControl( "customhead" ) );
			pCustomHead->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11130 ) + GetDBString( 18753 ) );
			pCustomHead->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11129 ) + GetDBString( 18753 ) );

			pFaceScroll = new CFaceGenScroll( sEvent.pLoader->GetControl( "face" ) );
			pFaceScroll->SetStyle( SCRLSTYLE_HORZ, true );
			// The 3 voice checkbuttons (release controls voice1/2/3). Container 361 gives them NO art
			// (Texture0-2 = 0), so retail (CFaceGenUI::ProcessMessage @0x1d27e0) loads each one's per-state
			// art explicitly via AddImageState -- state 0 NORMAL_UP / 1 NORMAL_DOWN / 3 CHECKED_DOWN -- with
			// the per-voice UITexture ids 908-910 / 911-913 / 914-916. Without these the buttons drew nothing
			// (invisible-but-pressable). (Retail also SetStyle(VISIBLE,!bGermanVersion); no German build here.)
			pVoice1 = new CHoverCheckButton( sEvent.pLoader->GetControl( "voice1" ) );
			pVoice1->AddImageState( 0, NDb::GetUITexture( 908 ) );
			pVoice1->AddImageState( 1, NDb::GetUITexture( 909 ) );
			pVoice1->AddImageState( 3, NDb::GetUITexture( 910 ) );
			pVoice2 = new CHoverCheckButton( sEvent.pLoader->GetControl( "voice2" ) );
			pVoice2->AddImageState( 0, NDb::GetUITexture( 911 ) );
			pVoice2->AddImageState( 1, NDb::GetUITexture( 912 ) );
			pVoice2->AddImageState( 3, NDb::GetUITexture( 913 ) );
			pVoice3 = new CHoverCheckButton( sEvent.pLoader->GetControl( "voice3" ) );
			pVoice3->AddImageState( 0, NDb::GetUITexture( 914 ) );
			pVoice3->AddImageState( 1, NDb::GetUITexture( 915 ) );
			pVoice3->AddImageState( 3, NDb::GetUITexture( 916 ) );

			pUnitView = new CUnitView( sEvent.pLoader->GetControl( "unitshow" ), 0, 0.5f );
			UpdateUnit();
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pFaceScroll->SetMaxValue( customHeads.size() );
			UpdateVoiceChecks();   // initial check on the selected voice (default 0)
			break;
		}
	}

	return CDesktopWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFaceGenUI::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	CDesktopWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFaceGenUI::UpdateVoiceChecks()
{
	// Radio-check the 3 voice buttons on the selected voice (release SetVoice + the per-button checkmark).
	if ( IsValid( pVoice1 ) ) pVoice1->SetChecked( nVoice == 0 );
	if ( IsValid( pVoice2 ) ) pVoice2->SetChecked( nVoice == 1 );
	if ( IsValid( pVoice3 ) ) pVoice3->SetChecked( nVoice == 2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFaceGenUI::UpdateUnit()
{
	pTempMerc = NRPG::CreateMerc( pMerc->GetPers(), pHead );
	// release CFaceGenUI::ProcessMessage @0x1d27e0: the FaceGen unit view uses the GLOBAL DataCamera 5024
	// ("PersCustomHead", GetDBCamera 0x13a0) via the CDBCamera* SetUnit overload -- NOT the per-character pers
	// camera. CAMERA_FACEGEN read pUnit->GetPers()->sFaceGenCamera -> the wrong (per-pers) angle.
	pUnitView->SetUnit( pTempMerc, NDb::GetDBCamera( 5024 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFaceGenMenuInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
// Reparented to CRenderBaseInterface (release): the face-gen screen renders a 3D backdrop world from the
// nationality's nFaceGenTemplate (the hero-menu pattern). operator& @0x1d5660: 1 base, 2 pMerc, 3 pSide,
// 4 pNationality, 5 pDifficulty, 6 pMenuUI (the IInterfaceBase cursor/CInterface are now in the base).
class CFaceGenMenuInterface: public CRenderBaseInterface
{
	OBJECT_BASIC_METHODS(CFaceGenMenuInterface);
private:
	NInput::CBind bindClose, bindPlay, bindCustomHead;

	ZDATA_(CRenderBaseInterface)
	CObj<NRPG::CUnit> pMerc;            // the merc created upstream (HeroMenu/CharGen) + threaded in
	CDBPtr<NDb::CSide> pSide;
	CDBPtr<NDb::CNationality> pNationality;
	CDBPtr<NDb::CDBDifficulty> pDifficulty;   // chosen difficulty, carried to CICBeginGame on "play"
	////
	CObj<NUI::CFaceGenUI> pMenuUI;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CRenderBaseInterface*)this); f.Add(2,&pMerc); f.Add(3,&pSide); f.Add(4,&pNationality); f.Add(5,&pDifficulty); f.Add(6,&pMenuUI); return 0; }

public:
	CFaceGenMenuInterface();

	void Initialize( NDb::CSide *pSide, NDb::CNationality *pNationality, NDb::CDBDifficulty *pDifficulty, NRPG::CUnit *pMerc );

	void Step();
	bool ProcessEvent( const NInput::SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CFaceGenMenuInterface::CFaceGenMenuInterface():
	bindClose( "cancel" ), bindPlay( "play" ), bindCustomHead( "customhead" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFaceGenMenuInterface::Initialize( NDb::CSide *_pSide, NDb::CNationality *_pNationality, NDb::CDBDifficulty *_pDifficulty, NRPG::CUnit *_pMerc )
{
	pMerc = _pMerc;
	pSide = _pSide;
	pNationality = _pNationality;
	pDifficulty = _pDifficulty;

	// Render a 3D backdrop world from the nationality's face-gen template (release @0x1d3760:
	// CRenderBaseInterface::Initialize(pNationality->nFaceGenTemplate), now readable from the retail db).
	CRenderBaseInterface::Initialize( IsValid( pNationality ) ? pNationality->nFaceGenTemplate : 0 );

	CPtr<NDb::CDBCamera> pDBCamera = NDb::GetDBCamera( 5023 );   // N_FACEGEN_CAMERA (release GetDBCamera 0x139f)
	if ( IsValid( pDBCamera ) )
	{
		ICamera::SCameraPos sCameraPos( pDBCamera->vAnchor, pDBCamera->fDistance, pDBCamera->fPitch, pDBCamera->fYaw, pDBCamera->fRoll, pDBCamera->fFOV );
		GetCamera()->SetPlacement( sCameraPos );
	}

	// The merc-based face-gen UI (release @0x1d3540) derives the pers/head from the merc; it recreates a temp
	// merc to display + edit face/voice (the byte-faithful persistent-merc mutation is gated by the CUnit
	// head-info re-architecture).
	pMenuUI = new NUI::CFaceGenUI( NUI::SWindowInfo( GetInterface(), NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "facegenUI", NUI::STYLE_ENABLED ), pMerc );
	NUI::LoadTemplate( pMenuUI, NDb::GetUIContainer( 361 ) );
	pMenuUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFaceGenMenuInterface::Step()
{
	CRenderBaseInterface::Step();

	if ( CanRender() )
	{
		// Render the 3D backdrop full-screen (the side-menu / hero-menu pattern).
		GetCamera()->SetScreenRect( CTRect<float>( 0.0f, 0.0f, 1.0f, 1.0f ) );
		RenderFrame( GetTime(), GetCamera() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFaceGenMenuInterface::ProcessEvent( const NInput::SEvent &sEvent )
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
		NRPG::CGlobalPlayer *pPlayer = NRPG::CreateGlobalPlayer( pSide );
		pPlayer->mercs.push_back( pMerc );   // the threaded merc goes into the player (was a recreate)

		vector<CObj<NRPG::CGlobalPlayer> > playersSet;
		playersSet.push_back( pPlayer );

		NMainLoop::Command( new NGame::CICBeginGame( pSide->nGlobalMapID, playersSet, pDifficulty ) );
		return true;
	}
	else if ( bindCustomHead.ProcessEvent( sEvent ) )
	{
		// Open the advanced ("custom head") face editor for the same merc / nationality / side / difficulty.
		NMainLoop::Command( new NGame::CICAdvFaceGen( pSide, pNationality, pDifficulty, pMerc ) );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICMission
////////////////////////////////////////////////////////////////////////////////////////////////////
CICFaceGen::CICFaceGen( NDb::CSide *_pSide, NDb::CNationality *_pNationality, NDb::CDBDifficulty *_pDifficulty, NRPG::CUnit *_pMerc ):
	pMerc( _pMerc ), pSide( _pSide ), pNationality( _pNationality ), pDifficulty( _pDifficulty )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICFaceGen::Exec()
{
	ASSERT( IsValid( pMerc ) );
	if ( !IsValid( pMerc ) )
	{
		csSystem << CC_RED << L"ERROR: FaceGen merc not set!" << endl;
		return;
	}

	CFaceGenMenuInterface *pRes = new CFaceGenMenuInterface();
	pRes->Initialize( pSide, pNationality, pDifficulty, pMerc );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB1204020, CFaceGenUI );
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB1204021, CFaceGenMenuInterface );
