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
// iAutoPlay -- release compiland .\release\iAutoPlay.obj converged into the dev engine.
//
// The "autoplay" developer attract-mode: a console command that boots a real CMission on a randomly
// chosen map template, fills the global game with four random AI squads, overlays a publisher-logo
// interface, and dismisses on the first real input event. Reconstructed in the dev idiom (free CMission
// subclass + transient CInterfaceCommand + console registration), modelled on iLoseFake.cpp / iChapterMap.cpp.
//
// Faithful ports (RVA markers; VA = RVA + 0x400000):
//   NGame::CAutoPlayInterface::CAutoPlayInterface   @0x19c960  (default ctor)
//   NGame::CAutoPlayInterface::ProcessEvent         @0x19c6b0  (latch + CICExitModal on first non-CT_TIME event)
//   NGame::CAutoPlayInterface::RenderFrame          @0x19c770  (base render w/ forced flag bit 8 + logo step/draw + Flip)
//   NGame::CAutoPlayInterface::ParseNumbers         @0x19c7f0  (carries the consecutive-space ORIGINAL BUG)
//   NGame::CAutoPlayInterface::AddPlayer            @0x19c9d0  (random 4..7-merc squad over CreateGlobalPlayer)
//   NGame::CAutoPlayInterface::GoToNextMap          @0x19cb00  ({CICExitModal, CICAutoPlay} container command)
//   NGame::CAutoPlayInterface::Initialize           @0x19ce60  (parse vars -> game + squads -> CMission::Initialize -> logo UI)
//   NGame::CICAutoPlay::CICAutoPlay                 @0x19c710
//   NGame::CICAutoPlay::Exec                        @0x19d3f0
//   iAutoPlayInit::iAutoPlayInit                    @0x19d550  (console "autoplay" cmd + autoplay_units/templates vars)
//
// DELIBERATE DEVIATIONS (documented; dev type/idiom differs from the MSVC7.1 release):
//   * CMissionBase: since serialization-convergence W4.2 the dev CMission IS split onto NGame::CMissionBase
//     (iMission.h) with the retail tag tables; CAutoPlayInterface derives the dev CMission directly (byte-exact
//     PDB layout 0x5a8 is still not a goal -- functional parity).
//   * GameStep @0x19cc80 (the finished/progress>100/20-min watchdog) is NOT reconstructed: it walks the live
//     world via opaque vtbl slots (collect NWorld::CPlayer, CPlayer-finished, GetGame()->GetProgress()) and
//     resolves a DG CCTime node through CMissionBase::GetUITime -- none of which are surfaced as reachable dev
//     public methods (the dev CMission exposes no GetUITime / world-player-finished gate). Its tail GoToNextMap
//     is reconstructed for parity but is therefore unreferenced here.
//   * AddPlayer drops the per-player AI flag: the release set CGlobalPlayer+0x54 = 1; the dev CGlobalPlayer
//     (rpgGlobal.h) is the release-faithful type MINUS any AI flag, so there is no field to set.
//   * Initialize omits: the per-unit "set level 30" walk (no NRPG::CUnit level setter is surfaced here); the
//     logo NUI::CFlashImage template (the dev CFlashImage is file-local to iMainMenu.cpp, not a shared type);
//     and the release's private-flag writes (bLoseSignalSended / bHideInterface) + the pWorld vtbl[0x1f0] call
//     (those CMission members are private in the dev and have no public setter). pGlobalGame/pWorld/pScene/
//     pSoundScene are instead set, faithfully, by routing through the public CMission::Initialize(...).
//   * Registration + serialization: retail registers CAutoPlayInterface under 0xB3723140 and serializes it
//     (operator& @0x1a13e0: 1=CMission base, 2=bSignalSent, 3=sAutoPlayTime, 4=pLogoCursor, 5=pLogoInterface,
//     6=pFlashImage). Registration landed in serialization-convergence W2, the operator& body in W3.
//     Retail tag 6 is CObj<NUI::CFlashImage>; the dev member is the CObj<NUI::CWindow> parity placeholder
//     (see above) -- the object reference serializes identically through the class registry.
////////////////////////////////////////////////////////////////////////////////////////////////////
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
	CTimeCounter sAutoPlayTime;				// @+0x594: wall-clock watchdog timer (consumed by the deferred GameStep)
	CObj<NUI::ICursor> pLogoCursor;			// @+0x59c
	CObj<NUI::CInterface> pLogoInterface;	// @+0x5a0
	CObj<NUI::CWindow> pFlashImage;			// @+0x5a4: parity placeholder; the real logo type NUI::CFlashImage is file-local to iMainMenu.cpp

	void ParseNumbers( const wstring &szStr, vector<int> &outSet );
	void AddPlayer( NRPG::CGlobalGame *pGame, const vector<int> &templateIDs );
	void GoToNextMap();

public:
	CAutoPlayInterface();

	// retail @0x1a13e0: 1=CMission base, 2=bSignalSent, 3=sAutoPlayTime, 4=pLogoCursor,
	// 5=pLogoInterface, 6=pFlashImage
	int operator&( CStructureSaver &f ) { f.Add(1,(CMission*)this); f.Add(2,&bSignalSent); f.Add(3,&sAutoPlayTime); f.Add(4,&pLogoCursor); f.Add(5,&pLogoInterface); f.Add(6,&pFlashImage); return 0; }

	bool Initialize();

	bool ProcessEvent( const NInput::SEvent &sEvent );
	void RenderFrame( int nMode, const STime &sTime, ICamera *pCamera, bool bShowUnits );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICAutoPlay -- the queued main-loop command that (re-)enters the auto-play logo screen.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICAutoPlay: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICAutoPlay)
public:
	CICAutoPlay() {}

	virtual void Exec();
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
// global player from them and append it to the game's player list. The release also set the player's AI flag
// (CGlobalPlayer+0x54 = 1); the dev CGlobalPlayer carries no such field, so that store is dropped.
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
	pGame->players.push_back( pPlayer );				// CObj<> stores an owning AddRef'd reference
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CAutoPlayInterface::Initialize  @0x19ce60
//
// The auto-play boot sequence: parse the two console vars, gate on both being non-empty, build a fresh global
// game with four random AI squads, pick a random starting map template and boot the real mission on it, then
// raise the logo cursor/interface overlay. See the file header for the omitted release tail (per-unit level
// set, logo flash-image template, private-flag writes + pWorld vtbl call).
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

	SRandomSeed sSeed( GetTickCount() );
	SRand sRand( sSeed );							// (split avoids the most-vexing-parse on SRandomSeed(...))
	int nMapID = templatesSet[ sRand.Get( (int)templatesSet.size() ) ];

	vector<string> paramsSet;
	paramsSet.push_back( "Day" );						// the release daytime param
	CMission::Initialize( nMapID, -1, 0, paramsSet, pGame );	// sets pGlobalGame / pWorld / pScene / pSoundScene

	pLogoCursor = NUI::ICursor::Create();
	pLogoInterface = new NUI::CInterface( pLogoCursor, GetSoundScene() );

	SetCheatVisibility( true );							// release bCheatVisibility = true
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CAutoPlayInterface::GoToNextMap  @0x19cb00
//
// Queue, as a single container command, "dismiss the current logo modal then re-enter auto-play" -- advancing
// the intro to its next random logo/map. (Reachable in retail from GameStep, which is deferred here.)
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
// NGame::CAutoPlayInterface::RenderFrame  @0x19c770
//
// Render the mission with the AutoPlay render-mode flag (bit 8) forced on, step+draw the live logo interface,
// then present -- but only when the caller did not already set bit 8 (the top-level render path).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAutoPlayInterface::RenderFrame( int nMode, const STime &sTime, ICamera *pCamera, bool bShowUnits )
{
	CMission::RenderFrame( nMode | 8, sTime, pCamera, bShowUnits );

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
