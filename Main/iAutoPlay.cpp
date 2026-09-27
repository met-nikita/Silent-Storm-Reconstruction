#include "StdAfx.h"
// CAutoPlayInterface subclasses NGame::CMission (iMissionInternal.h), whose definition needs the SAME
// large prelude iMission.cpp builds up (IUnitTracker/CSyncDst/IMission/IRenderGame/CTransformStack/
// CMissionUI/CPolyline/IRenderSound ... are not self-declared there). Mirror iMission.cpp's include
// order verbatim so iMissionInternal.h resolves, then iAutoPlay's own extras.
#include "wInterface.h"
#include "wMain.h"
#include "wMainTrace.h"
#include "wUICommands.h"
#include "A5Script.h"
#include "Transform.h"
#include "DiscretePos.h"
#include "GView.h"
#include "G2DView.h"					// NGScene::Flip
#include "GSceneUtils.h"
#include "Sound.h"
#include "RWGame.h"
#include "RWSound.h"
#include "RPGGame.h"
#include "RPGGlobal.h"					// NRPG::CGlobalGame / CGlobalPlayer + CreateGlobalGame / CreateGlobalPlayer
#include "RPGUnit.h"
#include "RPGMerc.h"
#include "RPGUnitInfo.h"
#include "RPGItemInfo.h"
#include "..\MiscDll\Commands.h"		// NGlobal::Register* / CValue / GetVar
#include "..\MiscDll\LogStream.h"
#include "..\Misc\StrProc.h"
#include "..\Misc\BasicShare.h"
#include "..\Input\Bind.h"				// NInput::SEvent / NInput::CT_TIME
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataSound.h"
#include "..\DBFormat\DataLight.h"
#include "..\DBFormat\DataInterface.h"
#include "iMain.h"
#include "iAutoPlay.h"
#include "iFlashImage.h"
#include "iSaveManager.h"
#include "Interface.h"					// NUI::CInterface / NUI::ICursor / ICursor::Create
#include "iCommonUI.h"
#include "iMission.h"
#include "iInterMission.h"
#include "iGlobalMap.h"
#include "iChapterMap.h"
#include "iSaveLoad.h"
#include "iInGameMenu.h"
#include "iLoseFake.h"
#include "iIntroScreen.h"
#include "iShowHint.h"
#include "iCluesMenu.h"
#include "iObjectivesMenu.h"
#include "iCharGen.h"
#include "iTeamMngMenu.h"
#include "iAIViewer.h"
#include "iRadTest.h"
#include "iGameStates.h"
#include "aiInterval.h"
#include "aiMap.h"
#include "iMissionUI.h"
#include "iMissionDlgUI.h"
#include "iMissionMovieUI.h"
#include "iMissionTrailerUI.h"
#include "iMissionExec.h"				// complete NGame::CUICmdExec (CMission's serialized uiCmds use the CObjectBase cast path)
#include "iMissionInternal.h"			// NGame::CMission (dev base class)
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail idle demonstration: four AI squads, a logo-only overlay, and input dismissal.
// v1.2 Initialize 0x59d940; main-menu trigger 0x5f79a0. GameStep is dispatched
// separately from CMission's interactive logic, while keeping the common world/camera pump.
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAutoPlayInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAutoPlayInterface: public CMission
{
	OBJECT_BASIC_METHODS(CAutoPlayInterface)
	//// auto-play state (release CAutoPlayInterface members past the CMission base)
	bool bSignalSent;						// @+0x590: latched on the first dismiss-worthy event
	CTimeCounter sAutoPlayTime;				// @+0x594: demo watchdog
	CObj<NUI::ICursor> pLogoCursor;			// @+0x59c
	CObj<NUI::CInterface> pLogoInterface;	// @+0x5a0
	CObj<NUI::CFlashImage> pFlashImage;

	void ParseNumbers( const wstring &szStr, vector<int> &outSet );
	void AddPlayer( NRPG::CGlobalGame *pGame, const vector<int> &templateIDs );
	void GoToNextMap();
	void GameStep() override;

public:
	CAutoPlayInterface();

	// retail @0x1a13e0: 1=CMission base, 2=bSignalSent, 3=sAutoPlayTime, 4=pLogoCursor,
	// 5=pLogoInterface, 6=pFlashImage
	int operator&( CStructureSaver &f ) { f.Add(1,(CMission*)this); f.Add(2,&bSignalSent); f.Add(3,&sAutoPlayTime); f.Add(4,&pLogoCursor); f.Add(5,&pLogoInterface); f.Add(6,&pFlashImage); return 0; }

	bool Initialize();

	bool ProcessEvent( const NInput::SEvent &sEvent );
	void RenderFrame( int nMode, bool bAdvanceTime, ICamera *pCamera, bool bShowUnits );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CAutoPlayInterface::CAutoPlayInterface  @0x19c960
////////////////////////////////////////////////////////////////////////////////////////////////////
CAutoPlayInterface::CAutoPlayInterface():
	bSignalSent( false )
{
	// CMission() (the dev base ctor: ~50 input binds + cross-subsystem init) runs implicitly; the CObj logo
	// handles default to null and sAutoPlayTime to its CTimeCounter default -- matching the release ctor.
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CAutoPlayInterface::ParseNumbers  @0x19c7f0
//
// Splits a space-separated UTF-16 integer list, appending each base-10 value to outSet.
// ORIGINAL BUG (confirmed via decomp+disasm @0x59c7f0): the substr COUNT argument is the *absolute* index of
// the next space, not a remaining length. Single-space input parses correctly; consecutive spaces yield an
// empty segment so wcstol skips ahead to the following number -- e.g. "12  34" decodes to {12, 3, 34}. The
// loop always pushes at least once (an empty / whitespace string yields a single 0). Carried faithfully.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAutoPlayInterface::ParseNumbers( const wstring &szStr, vector<int> &outSet )
{
	const unsigned int nNpos = 0xffffffffu;
	const unsigned int nLen  = (unsigned int)szStr.size();
	unsigned int nLastSpace = 0;
	unsigned int nPos = 0;
	for ( ;; )
	{
		unsigned int nSpaceIdx;
		if ( nLastSpace + 1 < nLen )
		{
			unsigned int i = nLastSpace + 1;
			while ( i < nLen && szStr[ i ] != L' ' )
				++i;
			nSpaceIdx = ( i < nLen ) ? i : nNpos;
		}
		else
			nSpaceIdx = nNpos;
		nLastSpace = nSpaceIdx;

		// substr( nPos, nSpaceIdx ) -- COUNT == the absolute space index (the release quirk), clamped by substr.
		outSet.push_back( (int)wcstol( szStr.substr( nPos, nSpaceIdx ).c_str(), 0, 10 ) );

		if ( nSpaceIdx == nNpos )
			return;
		nPos = nSpaceIdx;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CAutoPlayInterface::AddPlayer  @0x19c9d0
//
// Roll a 4..7-strong AI squad of random template ids (drawn with repetition from templateIDs), build the
// global player from them and append it to the game's player list with AI control enabled.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAutoPlayInterface::AddPlayer( NRPG::CGlobalGame *pGame, const vector<int> &templateIDs )
{
	SRandomSeed sSeed( GetTickCount() );
	SRand sRand( sSeed );							// (split avoids the most-vexing-parse on SRandomSeed(...))
	int nCount = sRand.Get( 4 ) + 4;					// 4..7 mercs

	vector<int> selSet;
	for ( int i = 0; i < nCount; ++i )
		selSet.push_back( templateIDs[ sRand.Get( (int)templateIDs.size() ) ] );

	NRPG::CGlobalPlayer *pPlayer = NRPG::CreateGlobalPlayer( selSet );
	pPlayer->bAIPlayer = true;
	pGame->players.push_back( pPlayer );				// CObj<> stores an owning AddRef'd reference
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CAutoPlayInterface::Initialize  @0x19ce60
//
// The auto-play boot sequence: parse the two console vars, gate on both being non-empty, build a fresh global
// game with four random AI squads, pick a random starting map template and boot the real mission on it, then
// raise the logo-only interface overlay.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAutoPlayInterface::Initialize()
{
	vector<int> unitsSet, templatesSet;
	ParseNumbers( NGlobal::GetVar( "autoplay_units" ).GetString(), unitsSet );
	ParseNumbers( NGlobal::GetVar( "autoplay_templates" ).GetString(), templatesSet );
	if ( unitsSet.empty() || templatesSet.empty() )
		return false;

	CPtr<NRPG::CGlobalGame> pGame = NRPG::CreateGlobalGame();
	for ( int i = 0; i < 4; ++i )						// four AI squads
		AddPlayer( pGame, unitsSet );
	for ( int i = 0; i < pGame->players.size(); ++i )
		for ( int j = 0; j < pGame->players[i]->mercs.size(); ++j )
			pGame->players[i]->mercs[j]->SetXPLevel( 30 );

	SRandomSeed sSeed( GetTickCount() );
	SRand sRand( sSeed );							// (split avoids the most-vexing-parse on SRandomSeed(...))
	int nMapID = templatesSet[ sRand.Get( (int)templatesSet.size() ) ];

	vector<string> paramsSet;
	paramsSet.push_back( "Day" );						// the release daytime param
	bLoseSignalSended = true;
	CMission::Initialize( nMapID, -1, 0, paramsSet, pGame );	// sets pGlobalGame / pWorld / pScene / pSoundScene

	pLogoCursor = NUI::ICursor::Create( false, NGfx::GetScreenRect() * 0.5f );
	pLogoInterface = new NUI::CInterface( pLogoCursor, GetSoundScene() );
	pFlashImage = new NUI::CFlashImage( NUI::SWindowInfo( pLogoInterface,
		NUI::SPoint( 768, 0 ), NUI::SPoint( 256, 128 ), "logo",
		NUI::STYLE_VISIBLE | NUI::STYLE_ENABLED | NUI::STYLE_TOPMOST ) );
	NUI::LoadTemplate( pFlashImage, NDb::GetUIContainer( 372 ) );
	nSequence = 1;
	bHideInterface = true;
	static_cast<NWorld::CWorld*>( GetWorld() )->ScriptWantTurnBased( true );

	SetCheatVisibility( true );							// release bCheatVisibility = true
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CAutoPlayInterface::GoToNextMap  @0x19cb00
//
// Queue, as a single container command, "dismiss the current logo modal then re-enter auto-play" -- advancing
// the intro to its next random logo/map.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAutoPlayInterface::GoToNextMap()
{
	vector<CPtr<NMainLoop::CInterfaceCommand> > cmdsSet;
	cmdsSet.push_back( new NMainLoop::CICExitModal() );
	cmdsSet.push_back( new CICAutoPlay() );
	NMainLoop::Command( new NMainLoop::CICContainer( cmdsSet ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CAutoPlayInterface::ProcessEvent  @0x19c6b0
//
// The first non-time (real input) event latches bSignalSent and fires exactly one ExitModal command; every
// later event and every CT_TIME tick is a no-op. The method never consumes the event (always returns false)
// -- the base ProcessEvent is intentionally not called.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAutoPlayInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	if ( !bSignalSent && sEvent.mMessage.cType != NInput::CT_TIME )
	{
		bSignalSent = true;
		NMainLoop::Command( new NMainLoop::CICExitModal() );
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAutoPlayInterface::GameStep()
{
	if ( bSignalSent )
		return;
	vector< CPtr<IPlayerTracker> > players;
	GetPlayers( &players );
	for ( int i = 0; i < players.size(); ++i )
	{
		if ( players[i]->IsPlayerWinner() )
		{
			bSignalSent = true;
			GoToNextMap();
			return;
		}
	}
	if ( static_cast<NWorld::CWorld*>( GetWorld() )->GetTurnID() > 100 )
	{
		bSignalSent = true;
		GoToNextMap();
		return;
	}
	sAutoPlayTime.Advance( true, GetUITime() );
	CDGPtr<CCTime> pTime = sAutoPlayTime.GetTime();
	pTime.Refresh();
	if ( pTime->GetValue() > 1200000 )
	{
		bSignalSent = true;
		GoToNextMap();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CAutoPlayInterface::RenderFrame  @0x19c770
//
// Render the mission with the AutoPlay render-mode flag (bit 8) forced on, step+draw the live logo interface,
// then present -- but only when the caller did not already set bit 8 (the top-level render path).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAutoPlayInterface::RenderFrame( int nMode, bool bAdvanceTime, ICamera *pCamera, bool bShowUnits )
{
	// No mission HUD/client-view margins in the demo.
	if ( pCamera )
		pCamera->SetScreenRect( CTRect<float>( 0, 0, 1, 1 ) );
	CMission::RenderFrame( nMode | 8, bAdvanceTime, pCamera, bShowUnits );

	if ( IsValid( pLogoInterface ) )
	{
		pLogoInterface->Step( GetTime() );				// GetTime() re-read before each (matches the binary)
		pLogoInterface->Draw( GetTime() );
	}

	if ( ( nMode & 8 ) == 0 )
		NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CICAutoPlay::Exec  @0x19d3f0
//
// Build the auto-play logo screen, initialise it, and -- on success -- push it as the new active front-end
// interface. A scoped CObj keeps the freshly-new'd interface alive across Initialize/PushInterface; a failed
// Initialize never pushes, so the scoped ref drops to zero and frees it (the release ++[p+4]/ReleaseRef bracket).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICAutoPlay::Exec()
{
	CAutoPlayInterface *pRes = new CAutoPlayInterface();
	CObj<CAutoPlayInterface> pHold = pRes;				// scoped hold: refcount -> 1
	if ( pRes->Initialize() )
		PushInterface( pRes );							// the stack takes its own reference
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NGame
////////////////////////////////////////////////////////////////////////////////////////////////////
// iAutoPlayInit::iAutoPlayInit  @0x19d550 -- module static-init registrar.
//
// Registers the "autoplay" console command (its handler posts a CICAutoPlay onto the main loop, run safely
// between frames) plus the two string config vars, with their exact release default id strings.
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AutoPlayCommand( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	NMainLoop::Command( new NGame::CICAutoPlay() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(iAutoPlay)
	REGISTER_CMD( "autoplay", AutoPlayCommand )
	REGISTER_VAR( "autoplay_units", 0, NGlobal::CValue( L"968 970 974 1020 973 979 978 980 982 983 984 985 986 989 990 993 994 1000 996 997 998 999 1001 1004 1002 1003 1009 1008 1010 1011 1012 1013 1014 1016 1017 1018 1019" ), false )
	REGISTER_VAR( "autoplay_templates", 0, NGlobal::CValue( L"4412 4413 4414" ), false )
FINISH_REGISTER
// retail saveload id (serialization-convergence W2; operator& @0x1a13e0 landed in W3)
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB3723140, CAutoPlayInterface )
