#include "StdAfx.h"
#include "wInterface.h"
#include "wMain.h"			// NWorld::CWorld::GetOwnScript -- wire the script-UI bridge to the mission HUD
#include "wUnitServer.h"	// temporary GFirst music diagnostics: script unit names
#include "wMainTrace.h"
#include "wMisc.h"			// NWorld::GetDMeshUnit -- heard-not-seen noise-marker pick (TraceCursor)
#include "wUICommands.h"
#include "A5Script.h"		// NScript::CScript::SetScriptInterface
#include "Transform.h"
#include "DiscretePos.h"
#include "GView.h"
#include "G2DView.h"
#include "GSceneUtils.h"
#include "Sound.h"
#include "RWGame.h"
#include "RWSound.h"
#include "RPGGame.h"
#include "RPGGlobal.h"
#include "RPGUnit.h"
#include "RPGMerc.h"
#include "RPGUnitInfo.h"
#include "RPGItemInfo.h"
#include "..\MiscDll\Commands.h"
#include "..\MiscDll\LogStream.h"
#include "..\Misc\StrProc.h"
#include "..\Misc\BasicShare.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataSound.h"
#include "..\DBFormat\DataLight.h"
#include "iMain.h"
#include "iSaveManager.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iMission.h"
#include "iLoading.h"		// NGame::SetLoadingImage / ShowLoadingScreen -- per-mission splash before world load
#include "iInterMission.h"
#include "iMainMenu.h"		// CICMainMenu -- retail @0x20a850: mission exit with no chapter/global map
#include "iGlobalMap.h"
#include "iChapterMap.h"
#include "iSaveLoad.h"
#include "iInGameMenu.h"
#include "iLoseFake.h"
#include "iIntroScreen.h"		// NGame::PlayVideoSequence (PlayVideo dispatch)
#include "iShowHint.h"
#include "iCluesMenu.h"
#include "iObjectivesMenu.h"
#include "iShowObjectives.h"		// NGame::CICShowObjectives -- the retail framed objectives modal (replaces the flat CICObjectives)
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
#include "iMissionInternal.h"
#include "iMissionExec.h"
#include "UnitTracker.h"
#include "Grid.h"				// FP_GRID_STEP / FP_INV_GRID_STEP / FP_TERRAIN_H_SCALE (camera height source)
#include "MapBuildTerrain.h"	// GetMeterHeightCheck (camera height source)
#include "PlayerTracker.h"
#include "MemObject.h"
#include "BSchemaViewer.h"
#include "MakeBuilding.h"
#include "MapBuildingInfo.h"
#include "scScenarioTracker.h"
#include "scFlowChartItems.h"
#include "scFlowChart.h"
#include "RPGDiplomacy.h"
#include "..\DBFormat\DataScenario.h"
#include "rpgCheatConstants.h"
#include "..\DBFormat\DataDifficulty.h"
#include "iShowClue.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_SCROLL_STEP				= 4,
	N_SCROLL_GUARDBAND	= 4;
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool bShowAllCheat = false;
static int nFake = 0;	// retail global @0x9c62a8: the slo-mo frame divider counter (Step)
static float fDefaultCameraFOV = N_FOV;
// NGame::defaultCameraLimits / defaultCameraSoftLimits -- the module default-camera records the
// mission_camera_* console setters below mutate. The SCameraLimits 32->56 ripple LANDED (Camera.h):
// the type now IS the retail widened 56-byte record, so the old module-local SDefaultCameraTuning
// derived hedge is gone. Values seeded from the retail .data image (Game.exe @0x57c8d8 hard /
// @0x57c910 soft, identical to the iBase pair @0x57a970/@0x57a9a8): hard {7, 55, -1.5, -0.1},
// soft {10, 50, -1.5, -0.1}; the tuning tail equals the SCameraLimits ctor defaults (attenuation 1,
// minHeight 4, scrollZoomAccel 1.2, scrollSpeed 1, zoomAccel 5, zoomSpeed 0.1, yawSpeed 1,
// yawZoomAccel 1, pitchSpeed 1), bMovie false. The scroll-zone rect is NOT part of the record any
// more -- it lives on the camera (SetZoneLimits) and comes from the world's map safe zone.
static ICamera::SCameraLimits MakeDefaultCameraLimits( float fMinRod, float fMaxRod, float fMinPitch, float fMaxPitch )
{
	ICamera::SCameraLimits sLimits;
	sLimits.fMinRod = fMinRod;
	sLimits.fMaxRod = fMaxRod;
	sLimits.fMinPitch = fMinPitch;
	sLimits.fMaxPitch = fMaxPitch;
	return sLimits;
}
static ICamera::SCameraLimits defaultCameraLimits = MakeDefaultCameraLimits( 7, 55, -1.5f, -0.1f );
static ICamera::SCameraLimits defaultCameraSoftLimits = MakeDefaultCameraLimits( 10, 50, -1.5f, -0.1f );
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSurrender( const string &szID, const vector<wstring> &paramsSet, void *pContext );
static void CommandTypeCameraParams( const string &szID, const vector<wstring> &paramsSet, void *pContext );
static void CommandSetXPLevel( const string &szID, const vector<wstring> &paramsSet, void *pContext );
static void CommandSummonUnit( const string &szID, const vector<wstring> &paramsSet, void *pContext );
static void CommandUnsummonUnit( const string &szID, const vector<wstring> &paramsSet, void *pContext );
static void CommandGetItem( const string &szID, const vector<wstring> &paramsSet, void *pContext );
////
static void VarCheatSeeAll( const string &szID, const NGlobal::CValue &sValue, void *pContext );
static void VarCheatGodMode( const string &szID, const NGlobal::CValue &sValue, void *pContext );
static void VarCheatTeleport( const string &szID, const NGlobal::CValue &sValue, void *pContext );
static void VarCheatAP( const string &szID, const NGlobal::CValue &sValue, void *pContext );
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMission
////////////////////////////////////////////////////////////////////////////////////////////////////
// W4.2: the base-class members (bPause/bHideInterface/nLightMode/bCheatVisibility/bTutorialMode/...)
// are initialized by the CMissionBase ctor (retail @0x1a2c70); only the CMission-side state stays here.
CMission::CMission():
	actionsInfoSet( UA_MAXVALUE ), nPanelsState( 0 ), nFramesSameCameraPosition(0), vPrevCameraPosition(0,0,0),
	fFOV( fDefaultCameraFOV ), eCameraType( CAMERA_PC ),
	bUpdated( false ), bForceUpdateNextFrame( false ),
	bRealTime( false ), bHasCommands( false ), bTrackIsReady( false ), bTrackActionExecuted( false ),
	bindCancel( "cancel" ),	bindCancelAction( "cancelaction" ),	bindContinue( "continue" ),	bindPause( "pause" ), bindMainMenu( "mainmenu" ), bindEndMission( "endmission" ), bindHideInterface( "hideinterface" ), bindSpecialHideInterface( "hideinterface_special" ), bindTrailerHideInterface( "hideinterface_trailer" ), 
	bindHero1( "hero_1" ), bindHero2( "hero_2" ), bindHero3( "hero_3" ), bindHero4( "hero_4" ), bindHero5( "hero_5" ), bindHero6( "hero_6" ), bindSelPrev( "hero_prev" ), bindSelNext( "hero_next" ),
	bindSnapShot( "weapon_snapshot" ), bindAimedShot( "weapon_aimedshot" ), bindCarefulShot( "weapon_carefulshot" ), bindShortBurst( "weapon_shortburst" ), bindLongBurst( "weapon_longburst" ), bindSnipeShot( "weapon_snipeshot" ), bindWeaponPrevMode( "weapon_prevmode" ), bindWeaponNextMode( "weapon_nextmode" ),
	bindGrenadeModeThrow( "grenade_throw" ), bindGrenadeModeSetTrap( "grenade_settrap" ),
	bindWeaponReload( "weapon_reload" ), bindPrevSlot( "slot_prev" ), bindNextSlot( "slot_next" ), bindItemUnload( "unload" ), bindItemArrange( "arrange" ),
	bindSwitchLighting("switch_lighting"),
	cmdTypeCamera( "camerapos", CommandTypeCameraParams, this ), 
	cmdSurrender( "surrender", CommandSurrender, this ), 
	cmdSummonUnit( "summonunit", CommandSummonUnit, this ), 
	cmdUnsummonUnit( "unsummonunit", CommandUnsummonUnit, this ), 
	cmdGetItem( "getitem", CommandGetItem, this ), 
	cmdSetXPLevel( "setxplevel", CommandSetXPLevel, this ),	
	varCheatGodMode( "godmode", VarCheatGodMode, this, NGlobal::CValue( L"off" ) ), 
	varCheatSeeAll( "seeall", VarCheatSeeAll, this, NGlobal::CValue( L"off" ) ), 
	varCheatTeleport( "teleport", VarCheatTeleport, this, NGlobal::CValue( L"off" ) ),
	varCheatAP( "cheat_ap", VarCheatAP, this, NGlobal::CValue( L"off" ) ),
	bindCluesMenu( "clues" ), bindObjectivesMenu( "objectives" ), bindGameMenu( "gamemenu" ),
	bindStartOfTurn( "startofturn" ), bindEndOfTurn( "endofturn" ), 
	bindSaveMenu( "savemenu" ), bindLoadMenu( "loadmenu" ), 
	bindMove( "move" ), bindAttack( "attack" ), bindSetMine( "setmine" ), bindSetTrap( "settrap" ), bindFirstAid( "firstaid" ), bindDropCorpse( "dropcorpse" ), bindExitPK( "exitpk" ), bindRotate( "unit_rotate" ), bindUseTool( "usetool" ),
	bindSnipeAttack( "snipe_attack" ), bindCollect1AP( "collectap_1ap" ), bindCollect10AP( "collectap_10ap" ), bindCollectMaxAP( "collectap_max" ), bindCollectAllAP( "collectap_all" ),
	bindNormalPose( "pose_normal" ), bindCrawlPose( "pose_crawl" ), bindCrouchPose( "pose_crouch" ), bindRunPose( "pose_run" ), bindStrafe( "pose_strafe" ), bindHide( "hide" ),
	bindFocusUnit( "focus_unit" ), bindAddFloor("next_floor"), bindSubFloor("prev_floor"),
	bindShadows("toggle_shadows"), bindFog("toggle_fog"), bindHSR("toggle_hsr"), 
	bindNextEnemy( "next_enemy" ), bindCheatTeleport( "cheat_teleport" ),bindExplode( "explode" ), bindShowAI( "showai" ), bindRPGStats( "rpg_stats" ), bindShowSchema( "viewSchema" ), bindShowParticles("switch_particles"), bindCheckBuildings( "checkStability" ), 
	bindShowVision( "rpg_vision" ), bindShowRAD( "showrad" ), bindShowVisibility("rpg_visibility"),
	bindTestIsect("test_intersect"), bindClickOfDeath("click_of_death"), bindExplosion("explosion"), bindErasePart("erase_part"),
	bindToggleTransp("toggle_transparent"), bindShowLightmap("toggle_lightmaps"),
	bindShowRain("test_rain"), bindShowSnow("test_snow")
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::Initialize( int _nTemplateID, int _nVariantID, NScenario::CScenarioZone *_pZone, const vector<string> &params, NRPG::CGlobalGame *_pGlobalGame, NDb::CUITexture *_pPWLImage )
{
	pZone = _pZone;
	nTemplateID = _nTemplateID;
	nVariantID = _nVariantID;
	pGlobalGame = _pGlobalGame;
	pPWLImage = _pPWLImage;		// retail @0x200690: this->pPWLImage = param_6 (chapter splash carried into the mission)

	int nMobsLevel = 0;
	SRandomSeed sSeed;
	list< CPtr<NScenario::CScenarioClue> > clues;
	// retail CMission::Initialize @0x200690 clears the previous scenario-zone latch before
	// classifying this mission.  Random encounters have no CScenarioZone; retaining the HQ
	// zone made CWorld::StartGame cache bIsBase=true, which lets its turn controller return
	// to real time even while hostile units are engaged.
	pGlobalGame->pCurrentZone = 0;
	if ( IsValid( pZone ) && pGlobalGame->pScenarioTracker->IsScenarioAvailable() )
	{
		if ( nTemplateID == -1 )
			nTemplateID = pZone->GetDefaultTemplateID();
		pGlobalGame->pScenarioTracker->GetPlacedClues( pZone, nTemplateID, &clues );
		pZone->SetPassed();
		pGlobalGame->pCurrentZone = pZone;
		sSeed = pZone->GetRandomSeedForTemplate( nTemplateID );
		nMobsLevel = pZone->GetDifficulty();
	}
	else
	{
		// calculate the difficulty of the random encounter
		int nDelta = 0;
		for ( int i = 0; i < 10; ++i )
			nDelta += random.Get( 0, Max( 1, 2 * pGlobalGame->pDifficulty->nREDifficulty ) );
		nDelta /= 10; nDelta -= pGlobalGame->pDifficulty->nREDifficulty;
		nMobsLevel = pGlobalGame->nCurrentChapterDifficulty + nDelta;
	}
	// Retail writes this for both scenario missions and template-only random encounters.
	pGlobalGame->nCurrentTemplateID = nTemplateID;

	if ( nVariantID == -1 )
	{
		CPtr<NDb::CTemplate> pTemplate = NDb::GetTemplate( nTemplateID );
		if ( !IsValid( pTemplate ) )
			return false;

		SRand sRand( sSeed );
		vector<int> dummyTemp;
		nVariantID = NDb::GetTemplVariant( pTemplate, dummyTemp, -1, &sRand )->GetRecordID();
	}

	// Tier-1 discrete loading-bar checkpoints (release drives a smooth 25..100 sweep via the banded
	// NGame::CLoadingCounter [25,50]/[50,75]/[75,100]; here we paint the band boundaries directly).
	ShowLoadingScreen( 25 );   // pre-world setup done; world load+build occupies the release band [25,50]

	// retail @0x200690: the re-enter save is per zone AND template ("%d_%d.sav") -- a linked
	// (multi-template) zone saves each sub-map separately; a missing file just leaves pWorld null.
	if ( IsValid( pZone ) )
		LoadWorld( NStr::Format( "%d_%d.sav", pZone->GetDBZone()->GetRecordID(), nTemplateID ) );

	CObj<NWorld::CPostWorldCreateInfo> pPostInfo;
	if ( !IsValid( pWorld ) )
	{
		pWorld = NWorld::CreateWorld( _pGlobalGame );
		pWorld->CreateRandom( nVariantID, params, true, clues,
			nMobsLevel, &pPostInfo, sSeed );
	}
	else
		pWorld->CreateRestored( pGlobalGame );	// retail @0x36e100: rebind the live global game + refresh buildings

	ShowLoadingScreen( 50 );   // world object built (release counter band boundary [50,75])

	// retail @0x200690: ambient/combat are MusicTemplates POOLS -- defaults GetTMusic(1)/GetTMusic(3),
	// overridden by the variant's +0x7c/+0x80 CTMusic refs when non-null -- and the actual track is a
	// weighted roulette pick from the pool (CTMusic::GetMusic). The old GetMusic(1)/(3) shortcut read
	// the MUSIC table with a TEMPLATE id: at the allies base (variant 5246, AmbientMusic = template
	// 124 = the ambient07 pool) it resolved MUSIC record 124 = Combat13.wav, so a combat track
	// launched as the base's load ambient (with the combat predicate legitimately at 0).
	NDb::CTMusic *pAmbientPool = NDb::GetTMusic( 1 );
	NDb::CTMusic *pCombatPool  = NDb::GetTMusic( 3 );
	NDb::CTemplVariant *pVar = NDb::GetTemplVariant( nVariantID );
	if ( pVar )
	{
		if ( IsValid( pVar->pAmbientMusic ) )
			pAmbientPool = pVar->pAmbientMusic;
		if ( IsValid( pVar->pCombatMusic ) )
			pCombatPool  = pVar->pCombatMusic;
		// retail @0x200690: adopt the variant's cut-floor range (max EXCLUSIVE in the db);
		// an unset variant (min >= max) keeps the engine default [-3,4]
		if ( pVar->nMinCutFloor < pVar->nMaxCutFloor )
		{
			nMinCutFloor = pVar->nMinCutFloor;
			nMaxCutFloor = pVar->nMaxCutFloor - 1;
		}
	}

	pScene = NGScene::CreateNewView();
	// retail CBaseCamera::SetCutFloorRange @0xcba10: install the variant's inclusive range on the
	// scene (dev keeps floors on the scene); it re-clamps the current floor and gates EVERY later
	// SetCutFloor (level-switch bar, UICmdSetFloor, unit focus) like retail SetCutFloor @0xd0050 --
	// the bar previously bypassed the key-bind-only clamp, letting the base show a second floor.
	pScene->SetCutFloorRange( nMinCutFloor, nMaxCutFloor );
	// retail keeps the pools on the scene and performs the roulette pick on every music launch.
	pSoundScene = NSound::CreateSoundScene( pAmbientPool, pCombatPool, pWorld->GetAimTime() );
	// retail @0x200690: the two sound mixers (world + fog-gated unit sounds) are owned by the
	// render game -- the old separate CreateRenderSound(pWorld, pSoundScene) union mixer played
	// unit voices (pain/death grunts) for units the player couldn't see.
	pRender = NRender::CreateRenderGame( pWorld, pScene, pSoundScene );
#ifdef _MAPEDIT
	pCursor = NUI::ICursor::CreateEditorCursor();
#else
	pCursor = NUI::ICursor::Create( true, NGfx::GetScreenRect() / 2 );
#endif
	pInterface = new NUI::CInterface( pCursor, pSoundScene );

	ShowLoadingScreen( 75 );   // scene/sound/render/cursor/interface created (release counter band boundary [75,100])

	// Activate the script-UI bridge: the world's own script drives this mission, so point its UI
	// interface at the in-mission HUD. GetWindow / GetCursorPos / CreateWindow then resolve real
	// windows. pInterface is a weak CPtr on the script, saved alongside the mission's own CInterface
	// (same save graph), so it round-trips through save/load.
	{
		CDynamicCast<NWorld::CWorld> pCWorld( (NWorld::IWorld*)pWorld );
		if ( IsValid( pCWorld ) && IsValid( pCWorld->GetOwnScript() ) )
			pCWorld->GetOwnScript()->SetScriptInterface( pInterface );
	}

	playersSet.resize( pGlobalGame->players.size() );
	for ( int nTemp = 0; nTemp < pGlobalGame->players.size(); nTemp++ )
	{
		WCHAR wsString[1024];
		swprintf( wsString, L"Player %d", nTemp );
		playersSet[nTemp] = new CPlayerTracker( this, pGlobalGame->players[nTemp], wsString );
		// retail @0x200690: stamp the variant's inclusive cut-floor range on each tracker's OWN
		// camera right after creation (tracker vtbl+0x18 GetCamera -> camera vtbl+0x3c
		// SetCutFloorRange @0xcba10; retail passes variant min / max-1 -- the same inclusive range
		// resolved into nMinCutFloor/nMaxCutFloor above).
		playersSet[nTemp]->GetCamera()->SetCutFloorRange( nMinCutFloor, nMaxCutFloor );
	}
	pActivePlayer = playersSet.front();
	CommandState( new CStateEmpty );

	TrackChanges();
	bUpdated = false;
	bForceUpdateNextFrame = true;

	NDb::CAmbientLightReal *pLight = GetWorld()->GetDefaultLight();
	if ( pLight )
		GetScene()->SetAmbient( pLight );
	else
		SetLightMode( 0 );

	// The cinematic camera (base pCamera, tag 18). Retail Initialize does NOT copy the tracker's
	// deploy pose into it -- the deploy view IS the active player's own camera (seeded in the
	// CPlayerTracker ctor @0x287d70); the cinematic camera only gets a pose when a script drives it.
	SetCameraParams( CAMERA_PC, fFOV, defaultCameraLimits );

	TraceCursor();

	bWaitForPartFinished = false;
	pMissionUI = new NUI::CMissionUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "missionUI" ), this );
	NUI::LoadTemplate( pMissionUI, NDb::GetUIContainer( 123 ) );
	pMissionUI->ShowWindow( NUI::SWTYPE_SHOW );
	// W4.2 (retail shape): the mission desktop sits at the BOTTOM of the base desktopWindowsList --
	// retail GetDesktop @0x1a2000 returns list.back() unchecked, and base tag 23 serializes the list,
	// so the desktop round-trips through saves without a dedicated pMissionUI tag (dev tag 49 dropped;
	// the raw pMissionUI member above is now a transient live-session cache).
	PushDesktop( pMissionUI );

	vector<CObj<IState> > updatedStatesSet;
	// Wait is installed explicitly while busy, never selected from the hover-state pool.
	updatedStatesSet.push_back( new CStateDragItem() );
	updatedStatesSet.push_back( new CStateUntrap( false ) );	// retail CMission::Initialize @0x200690: the hover-pool untrap is explicitly NON-forced
	updatedStatesSet.push_back( new CStateUse() );
	updatedStatesSet.push_back( new CStateTeam() );
	updatedStatesSet.push_back( new CStateFriend() );
	updatedStatesSet.push_back( new CStateAttack( false ) );
	updatedStatesSet.push_back( new CStatePickItem() );
	updatedStatesSet.push_back( new CStateMove( false ) );
	updatedStatesSet.push_back( new CStateEmpty() );
	SetUpdatedStates( updatedStatesSet );

	pWorld->RunPostInit( pPostInfo );

	// retail zone-entry focus floor: the deploy camera-focus path (CUICmdUnitCameraExec::Update
	// @0x24eae0 -> CCamera::ShowPlacesFromBestPoint @0xcf1c0 tail) sets the camera cut floor to the
	// focused party unit's floor BEFORE the zone script's CameraLock() freeze drains below. Dev has
	// no ShowPlaces* family, so seed the scene cut floor from the party here. Without it a zone with
	// an unset floor range (Min==Max==0 skips the strict `<` install above, e.g. the Axis base,
	// variant 5376 Min0/Max0) freezes the fresh-scene default N_MAX_FLOOR = "locked under the ceiling".
	// NOTE (why the first attempt did nothing): the party MUST be read from the WORLD player
	// (IPlayer::GetUnits) -- the UI trackers' CPlayerTracker::unitsSet is filled only by
	// CPlayerTracker::Update() from CMission::Step (first live frame), so a tracker read here is
	// always EMPTY and the seed silently never fires. The hero is preferred (retail focuses the
	// party lead); dead units are skipped like CPlayerTracker::Update does.
	{
		NWorld::IPlayer::CUnitSet partySet;
		pActivePlayer->GetPlayer()->GetUnits( &partySet );
		NWorld::CUnit *pSeedUnit = 0;
		for ( int nTemp = 0; nTemp < partySet.size(); nTemp++ )
		{
			if ( !IsValid( partySet[nTemp] ) || partySet[nTemp]->IsDead() )
				continue;
			if ( pSeedUnit == 0 )
				pSeedUnit = partySet[nTemp].GetPtr();
			if ( partySet[nTemp]->GetRPG()->IsHero() )
			{
				pSeedUnit = partySet[nTemp].GetPtr();
				break;
			}
		}
		if ( pSeedUnit )
			GetScene()->SetCutFloor( pSeedUnit->GetPosition().pos.GetFloor() );
	}

	// retail @0x200690 (asm 0x601655): BEFORE the InternalStep drain below, Initialize primes the render
	// with ONE pRender->UpdateViewWorld -- proven from disassembly: this->[0x48] is the CreateRenderGame
	// result (pRender), and the call is pRender->vtbl+0x24 (UpdateViewWorld) right before call 0x5a29f0
	// (CMissionBase::InternalStep). This one prime runs while the mission is still in its pre-sequence
	// VIEWER state (bCheatVisibility is false until the movieUI BorderShow fires), so CRenderGame::
	// UpdateVisible switches rUnits OFF its ctor raw-GetUnits() source onto the viewer/heard source
	// (pPrevViewFrom 0 -> active player). A mission that opens straight into a cinematic sequence
	// (GFirst: DelayGameStart + BeginSequence before StartGame) otherwise presents its FIRST rendered
	// frame already cinematic (pViewFrom==0), which MATCHES the ctor state (pPrevViewFrom==0,
	// bPrevShowUnits==true) so the source-switch never fires -- rUnits keeps raw GetUnits and the
	// heard-marker CBoolSyncSrc<CSubtractFunc> is never installed => the cutscene silhouette leak.
	// Dev omitted this prime; adding it (mirroring the Step() call args) establishes the source so the
	// later cinematic frame's player->0 switch installs the subtract.
	pRender->UpdateViewWorld( false, GetTime(), pActivePlayer->GetPlayer(), bShowAllCheat || bCheatVisibility );

	// retail @0x200690 tail runs ONE CMissionBase::InternalStep (@0x1a29f0) here = ProcessWorldCommands
	// + ProcessCameraCommands + camera refresh ONLY. The mission-start script (DelayGameStart ->
	// BeginSequence + the opening CameraSet, Common.l) queued those on the world UI-command list but
	// WaitForUI is a no-op while G_DelayGameStartFlag, so nothing drained them; retail drains them here
	// so the mission presents already letterboxed with the opening camera at first paint.
	// Dev has NO CMissionBase split, and its CMission::InternalStep also runs the interactive frame
	// (TraceCursor / UpdateState / pState->Step / UpdateSound) -- calling that here gave a live camera /
	// half-sequence. So drain ONLY the world + camera commands (retail's base-step subset), not the
	// interactive tail: ExecWorldCommands opens the movieUI letterbox (CUICmdBeginSequence -> nSequence>0
	// = input block + camera lock) and builds pCmdExec from the opening CUICmdMoveCamera; one Update
	// (transitionTime 0) settles it; then refresh the camera transform for the first frame. Also fixes
	// the tutorial's fixed corner camera (its opening CameraSet was the same undrained command).
	// retail @0x200690 tail = ONE CMissionBase::InternalStep (@0x1a29f0): a single world-command
	// drain + one camera-executor pump. NOTE (retail-verified): a zone script that SLEEPS before its
	// opening CameraSet (the tutorial's GroupAIMode = Sleep(2)/villain) has NOT queued the camera yet
	// at this point IN RETAIL EITHER -- retail's first frames show the PLAYER/DEPLOY camera and the
	// scripted camera lands a few live frames later. The dev-only "map corner" first frame is the
	// deploy-camera SEED being degenerate for script-deployed zones (PlayerTracker/GetDeploySpot),
	// which is fixed at the seed -- NOT by pumping extra hidden segments here (a prior dev-invented
	// pump loop was reverted: retail runs no extra warm-up simulation at Initialize).
	ExecWorldCommands();
	if ( IsValid( pCmdExec ) && pCmdExec->Update( GetGameTime() ) )
	{
		pCmdExec->Finished();
		pCmdExec = 0;
	}
	// through the SELECTOR (@0x1a1ee0), like the InternalStep it mirrors: during the opening
	// sequence that is the cinematic camera, else the active player's deploy camera.
	ICamera *pInitCamera = GetCamera();
	pInitCamera->Update( GetTime() );
	vCameraCP = pInitCamera->GetCP();
	sCameraPos = pInitCamera->GetPos();
	pInitCamera->GetTransform( &sTransform, GetScene()->GetScreenRect() );

	// retail @0x60167d: render ONE hidden frame -- RenderFrame(0xb = 2D|3D|bit8-NOFLIP,
	// !IsGamePaused() [vtbl+0x58], GetCamera(), true). Nothing presents (bit 8 skips the Flip; the
	// loading screen stays up); it warms every lazy render resource so the mission's FIRST live
	// frames are FAST. Load-bearing for the sound epoch: a slow second live frame hits the 150ms
	// clamp on the mixer clock while the world timer (input still 0) re-baselines and misses it --
	// a permanent mixer-vs-aim lead that seeks every fresh shot past its attack transient.
	RenderFrame( N_RENDERMODE_2D | N_RENDERMODE_3D | 8, !IsGamePaused(), GetCamera() );

	// retail @0x200690 tail: enqueue the mission-start "restart" snapshot (CICSaveRestartMission
	// @0x20b860 -> writes "restart.sav" @0x1f6a80). Deferred through the command queue ON PURPOSE:
	// it drains AFTER CICBeginMission::Exec has installed this mission into the interface stack,
	// so the snapshot contains the fresh mission (an inline save here would capture the OLD stack).
	// The pause/lose-menu "Restart mission" button loads it back via CICLoadFile.
	NMainLoop::Command( new NMainLoop::CICSaveFile( "restart.sav" ) );

	ShowLoadingScreen( 100 );  // mission fully initialized (release finish helper @0x1fb600 paints 100%)

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::Terminate()
{
	// retail @0x1fbc30: save this zone's world for re-entry ONLY when it is the scenario base,
	// a linked (multi-template) zone, or the script enabled the "reenter" feature. Carried
	// corpses are dropped from the world first (they leave in their carrier's arms), then the
	// players are pulled out, then the world is saved per zone+template.
	if ( ( pWorld->IsBase() || pWorld->IsLinkedZone() || bEnableFeatureReenter )
		&& IsValid( pZone ) )
	{
		pWorld->RemoveCarriedCorpses();

		for ( int nTemp = 0; nTemp < playersSet.size(); nTemp++ )
			pWorld->RemovePlayer( playersSet[nTemp]->GetPlayer() );

		SaveWorld( NStr::Format( "%d_%d.sav", pZone->GetDBZone()->GetRecordID(), nTemplateID ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::Command( NWorld::CCommand *pCmd )
{
	bForceUpdateNextFrame = true;

	ASSERT( pCmd );
	pActivePlayer->GetCommander()->Do( pCmd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::DoEvent @0x1a2fa0 (events channel) + the dev-extra update latch (like Command)
void CMission::DoEvent( NWorld::CCommand *pCmd )
{
	bForceUpdateNextFrame = true;

	ASSERT( pCmd );
	pActivePlayer->GetCommander()->DoEvent( pCmd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::Command( NWorld::CUnit *pUnit, NWorld::CCmd *pCmd, bool bInstantly )
{
	bForceUpdateNextFrame = true;

	ASSERT( pUnit );
	ASSERT( pCmd );
	Command( new NWorld::CCmdSetCommand( pUnit, pCmd ) );

	if ( bInstantly )
		Command( new NWorld::CCmdSetCommand( pUnit, new NWorld::CCmdContinue ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// W4.2: StopAction/IsReady/IsActionExecuted/IsRealTime/PauseGame/IsGamePaused, the player-tracker
// forwards (Get/SetActivePlayer, GetPlayers, GetUnits, GetSelectedUnits, CountSelected, Select,
// SelectNext/Prev) and the trivial accessors moved to CMissionBase (iMissionBase.cpp) -- their
// retail bodies live on the base (@0x1a29d0/@0x1a1e60/@0x1a3e30/@0x1a1900/...).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::IsSequence() const
{
	return nSequence > 0;   // retail mission vtbl+0x44 (@0x1a1e60 IsReady + @0x32d520 UnitTracker::Update gate)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::IsUpdated() const
{
	return bUpdated;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IState* CMission::GetState() const
{
	return pState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::CommandState( IState *pNewState )
{
	CObj<IState> pHold( pNewState );

	if ( IsValid( pState ) && ( pState->GetType() == IState::FORCED ) )
		return false;

	if ( !pNewState->Initialize( this ) )
		return false;

	if ( IsValid( pState ) )
		pState->Terminate();

	pState = pNewState;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::ResetState()
{
	pState->Terminate();
	pState = 0;
	CommandState( new CStateEmpty );
	UpdateState();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SetUpdatedStates( const vector<CObj<IState> > &_updatedStatesSet )
{
	updatedStatesSet = _updatedStatesSet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail GetStateTarget @0x1fbd40: returns pStateTarget (2D UI decorator hover, pbFrom3DWorld=false)
// or sTraceResult.pObject (3D world ray trace, pbFrom3DWorld=true).
CObjectBase* CMission::GetStateTarget( bool *pbFrom3DWorld ) const
{
	if ( pbFrom3DWorld )
		*pbFrom3DWorld = false;

	if ( IsValid( pStateTarget ) )
		return pStateTarget;

	if ( nFramesSameCameraPosition >= 4 && sTraceResult.bObjectSet )
	{
		if ( pbFrom3DWorld )
			*pbFrom3DWorld = true;
		return sTraceResult.pObject;
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SetStateTarget( CObjectBase* pObject )
{
	pStateTarget = pObject;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMission::GetUnitsState()
{
	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() != 1 )
		return N_UNITSTATE_DEFAULT;

	switch ( unitsSet.front()->GetUnit()->GetState() )
	{
	case NWorld::CUnit::ST_NORMAL_KNIFE:
	case NWorld::CUnit::ST_NORMAL_RIFLE:
	case NWorld::CUnit::ST_NORMAL_MELEE:
	case NWorld::CUnit::ST_NORMAL_PISTOL:
	case NWorld::CUnit::ST_NORMAL_GRENADE:
	case NWorld::CUnit::ST_NORMAL_DEFAULT:
	case NWorld::CUnit::ST_NORMAL_SUB_MACHINE_GUN:
	case NWorld::CUnit::ST_NORMAL_HAND_MACHINE_GUN:
	case NWorld::CUnit::ST_NORMAL_RLAUNCHER:
	case NWorld::CUnit::ST_NORMAL_MEDKIT:
	case NWorld::CUnit::ST_NORMAL_MINE:
	case NWorld::CUnit::ST_HEALER:
	case NWorld::CUnit::ST_CARRY_CORPSE:
		return N_UNITSTATE_DEFAULT;
	case NWorld::CUnit::ST_MACHINE_GUN:
		return N_UNITSTATE_CANNON;
	case NWorld::CUnit::ST_SNIPE:
		return N_UNITSTATE_SNIPE;
	default:
		ASSERT( 0 );
	}

	return N_UNITSTATE_DEFAULT;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CUnit::EState CMission::GetUnitsWorldState()
{
	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() != 1 )
		return NWorld::CUnit::ST_NORMAL_DEFAULT;

	return unitsSet.front()->GetUnit()->GetState();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::GetActionInfo( EUnitAction eAction, SActionInfo *pInfo )
{
	*pInfo = actionsInfoSet[eAction];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMission::CanDoCommand @0x1fbf10 (mission vtbl+0xa8): only CCmdEndOfTurn is gated -- a 2s
// cooldown after the world current-player change (UI clock); every other command is true.
bool CMission::CanDoCommand( NWorld::CCommand *pCmd )
{
	CObj<NWorld::CCommand> pHold( pCmd );
	if ( CDynamicCast<NWorld::CCmdEndOfTurn>( pCmd ) )
		return GetUITime() - tLastCurrentPlayerChange > 2000;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::CanDoCommand( NWorld::CCmd *pCmd, bool bNoTarget, SActionInfo *pInfo )
{
	// retail @0x1fb680: the low-level probe fills nMinAP/nMaxAP/bEnoughAPToStart directly in the
	// out struct; bValid is set true on every compute (the actionsInfoSet lazy-cache flag).
	pInfo->eResult = CanDoCommand( pCmd, bNoTarget, &pInfo->nMinAP, &pInfo->nMaxAP, &pInfo->bEnoughAPToStart );

	pInfo->bValid = true;
	pInfo->bOk = false;
	pInfo->bEnoughAP = true;
	pInfo->bAvailable = false;
	switch( pInfo->eResult )
	{
	case NWorld::UCR_UNAVAILABLE:
	case NWorld::UCR_INVALID_COMMAND:
		pInfo->bOk = false;
		pInfo->bAvailable = false;
		break;
	case NWorld::UCR_GENERAL_FAILURE:
		pInfo->bOk = false;
		pInfo->bAvailable = true;
		break;
	case NWorld::UCR_OK:
		pInfo->bOk = true;
		pInfo->bEnoughAP = true;
		pInfo->bAvailable = true;
		break;
	case NWorld::UCR_NO_TARGET:
		pInfo->bOk = bNoTarget;
		pInfo->bEnoughAP = true;
		pInfo->bAvailable = true;
		break;
	case NWorld::UCR_NOT_ENOUGH_AP:
		pInfo->bOk = true;
		pInfo->bEnoughAP = false;
		pInfo->bAvailable = true;
		break;
	case NWorld::UCR_PATH_NOT_FOUND:
	case NWorld::UCR_NEED_RELOAD:
	case NWorld::UCR_NO_EQUIPMENT:
	case NWorld::UCR_WEAPON_JAMMED:
	case NWorld::UCR_CRITICALS_BAN:
	case NWorld::UCR_TARGET_OUT_OF_RANGE:
	case NWorld::UCR_CANT_HEAL:                    // heal target's CanHeal() failed -- blocked but available (retail @0x1fb680 groups it here)
	case NWorld::UCR_NEED_HIGHER_SKILL:           // skill too low to use the tool -- blocked but available
	case NWorld::UCR_NOT_ALL_UNITS_NEAR_PASSAGE:  // not every selected unit is at the passage -- blocked but available
	case NWorld::UCR_PK_BAN:                      // PK-wearer crouch+look banned (CanDo @0x3c1570) -- blocked but available (retail @0x1fb680)
	case NWorld::UCR_DOOR_LOCKED:                 // locked door, no key/picklock -- blocked but available (retail @0x1fb680 groups it here; no message)
		pInfo->bOk = false;
		pInfo->bEnoughAP = true;
		pInfo->bAvailable = true;
		break;
	case NWorld::UCR_NOT_HERO:                     // can talk but not a hero -- SILENT no-op (no feedback)
		pInfo->bOk = false;
		pInfo->bAvailable = false;
		break;
	default:
		ASSERT( 0 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Maps a dev EUnitCommandResult onto its RETAIL ordinal so the retail result-combining rule
// (retail @0x1fc870, see below) can be evaluated exactly. Retail ordinals recovered from the
// binary: the CMission::CanDoCommand @0x1fb680 jump table (eResult-1 -> flag case) plus the
// ShowError @0x1d5fc0 switch (case values 3..0x15) plus the literal compares scattered through
// the state code (INVENTORY_NO_PLACE: `cmp eResult,0x12` @0x5dc911; PK_BAN: silent case 0x13;
// CANT_HEAL: case 0x10; DOOR_LOCKED: dev-doc ordinal 17; NEED_RELOAD: raw code 10 in
// CStateAttack::OnLButtonUp). The dev enum ordinals deliberately diverge (see wUnitCommands.h --
// NEVER reorder them), hence this explicit map. The middle "condition" band (NO_EQUIPMENT..
// TARGET_OUT_OF_RANGE) follows the declaration order shared by Jan03/dev/retail; retail ordinal 9
// is UCR_CANT_SEE_TARGET, a retail-only code the dev enum does not carry.
static int UCRRetailOrdinal( NWorld::EUnitCommandResult eRes )
{
	switch ( eRes )
	{
	case NWorld::UCR_OK:							return 1;
	case NWorld::UCR_NO_TARGET:						return 3;
	case NWorld::UCR_NOT_ENOUGH_AP:					return 4;
	case NWorld::UCR_NOT_HERO:						return 5;
	case NWorld::UCR_GENERAL_FAILURE:				return 6;
	case NWorld::UCR_INVALID_COMMAND:				return 7;
	case NWorld::UCR_PATH_NOT_FOUND:				return 8;
	case NWorld::UCR_NEED_RELOAD:					return 10;
	case NWorld::UCR_NO_EQUIPMENT:					return 11;
	case NWorld::UCR_WEAPON_JAMMED:					return 12;
	case NWorld::UCR_CRITICALS_BAN:					return 13;
	case NWorld::UCR_TARGET_OUT_OF_RANGE:			return 14;
	case NWorld::UCR_NEED_HIGHER_SKILL:				return 15;
	case NWorld::UCR_CANT_HEAL:						return 16;
	case NWorld::UCR_DOOR_LOCKED:					return 17;
	case NWorld::UCR_INVENTORY_NO_PLACE:			return 18;
	case NWorld::UCR_PK_BAN:						return 19;
	case NWorld::UCR_NOT_ALL_UNITS_NEAR_PASSAGE:	return 20;
	case NWorld::UCR_UNAVAILABLE:					return 21;	// dev-reordered; retail keeps it past the messages
	}
	return 21;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMission::CanDoCommand @0x1fc870 (mission vtbl+0xac). Probes the command against EVERY
// selected unit (no early-out): the per-unit full-action AP is folded into *pnMinAP/*pnMaxAP
// (both -1 when no unit reported an AP), *pbEnoughAPToStart AND-accumulates
// (AP-to-start <= unit->GetRPG()->GetAP()) in turn-based only, and the per-unit results combine
// by RETAIL-ordinal priority: the numerically-lowest retail code wins (so one unit's UCR_OK beats
// another's failure); when the kept code and the new code are BOTH hard failures (retail
// ordinal > 4 == beyond NOT_ENOUGH_AP) and differ, the total collapses to UCR_UNAVAILABLE.
NWorld::EUnitCommandResult CMission::CanDoCommand( NWorld::CCmd *pCmd, bool bNoTarget, int *pnMinAP, int *pnMaxAP, bool *pbEnoughAPToStart )
{
	CObj<NWorld::CCmd> pHolder( pCmd );
	vector< CPtr<IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() == 0 )
	{
		// retail @0x1fc870: the outs stay untouched on the empty-selection bail
		return NWorld::UCR_GENERAL_FAILURE;
	}

	if ( pnMinAP )
		*pnMinAP = -1;
	if ( pnMaxAP )
		*pnMaxAP = -1;
	if ( pbEnoughAPToStart )
		*pbEnoughAPToStart = true;

	int nMinAP = -1, nMaxAP = -1;
	bool bHaveAP = false;
	bool bHaveRes = false;
	NWorld::EUnitCommandResult eTotalRes = NWorld::UCR_OK;
	for ( vector< CPtr<IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
	{
		CPtr<NWorld::CUnit> pUnit = (*iTemp)->GetUnit();

		if ( bNoTarget )
		{
			CDynamicCast<NWorld::CCmdPath> pMove(pCmd);
			if (pMove)
			{
				if ( pMove->eParams != NAI::PF_USE_DIR )
					pMove->ptDst = (*iTemp)->GetTargetPosition();
				else
					pMove->ptDst = pUnit->GetPosition().pos;
			}
			else {
				CDynamicCast<NWorld::CCmdLook> pLook(pCmd);
				if (pLook)
					pLook->ptDst = pUnit->GetPosition().pos;
				else {
					CDynamicCast<NWorld::CCmdSetMineOnTile> pMine(pCmd);
					if (pMine)
						pMine->ptDst = pUnit->GetPosition().pos;
				}
			}
		}

		int nStartAP = -1, nFullAP = -1;
		NWorld::EUnitCommandResult eRes = pUnit->CanDo( pCmd, &nStartAP, &nFullAP );

		// retail min/max fold over the full-action AP; units reporting -1 are skipped
		if ( nFullAP != -1 )
		{
			if ( !bHaveAP )
			{
				bHaveAP = true;
				nMinAP = nFullAP;
			}
			else if ( nFullAP < nMinAP )
				nMinAP = nFullAP;
			if ( nFullAP > nMaxAP || nMaxAP == -1 )
				nMaxAP = nFullAP;
		}

		// retail: bEnoughAPToStart &= (AP-to-start <= current AP), evaluated in turn-based only
		// (retail reads the world mode through mission vtbl+0x134 -> world vtbl+0x1a4)
		if ( pbEnoughAPToStart && !IsRealTime() )
			*pbEnoughAPToStart = *pbEnoughAPToStart && ( nStartAP <= pUnit->GetRPG()->GetAP() );

		// retail result combine (order-dependent, reproduced structurally):
		//   total==<none> -> total = eRes
		//   total <= eRes (retail ordinals) -> keep total, EXCEPT two distinct hard failures
		//                                      (both ordinal > 4) collapse to UCR_UNAVAILABLE
		//   total >  eRes                   -> total = eRes (no collapse check in retail)
		if ( !bHaveRes )
		{
			bHaveRes = true;
			eTotalRes = eRes;
		}
		else if ( UCRRetailOrdinal( eTotalRes ) <= UCRRetailOrdinal( eRes ) )
		{
			if ( UCRRetailOrdinal( eTotalRes ) > 4 && UCRRetailOrdinal( eRes ) > 4 && eTotalRes != eRes )
				eTotalRes = NWorld::UCR_UNAVAILABLE;
		}
		else
			eTotalRes = eRes;
	}

	if ( pnMinAP )
		*pnMinAP = nMinAP;
	if ( pnMaxAP )
		*pnMaxAP = nMaxAP;

	return eTotalRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::UpdateActionsInfo()
{
	// retail SActionInfo::SetValid @0x1a18d0 -- the "instant OK, zero AP" filler retail's
	// UpdateActionsInfo (@0x1fb9c0) applies to the always-possible actions
	actionsInfoSet[UA_DEFAULT].SetValid();

	actionsInfoSet[UA_STOP].SetValid();

	CanDoCommand( new NWorld::CCmdContinue(), true, &actionsInfoSet[UA_CONTINUE] );
	CanDoCommand( new NWorld::CCmdReload(), true, &actionsInfoSet[UA_WEAPONRELOAD] );

	CanDoCommand( new NWorld::CCmdPath( NAI::SPosition(), NAI::PF_DEFAULT ), true, &actionsInfoSet[UA_MOVE] );
	CanDoCommand( new NWorld::CCmdLook( NAI::SPosition() ), true, &actionsInfoSet[UA_LOOK] );

	actionsInfoSet[UA_USE].SetValid();	// retail @0x1fb9c0 SetValid slot (see above)
	// the use-tool action (retail action 9) is availability-shaped like USE; the icon-bar button
	// is additionally gated on the unit actually holding a tool (ST_NORMAL_TOOL)
	actionsInfoSet[UA_USETOOL] = actionsInfoSet[UA_USE];

	CanDoCommand( new NWorld::CCmdShootObject( 0, 0 ), true, &actionsInfoSet[UA_ATTACK] );

	CanDoCommand( new NWorld::CCmdHeal( 0 ), true, &actionsInfoSet[UA_HEAL] );
	CanDoCommand( new NWorld::CCmdSetMineOnTile( NAI::SPosition() ), true, &actionsInfoSet[UA_MINE] );
	CanDoCommand( new NWorld::CCmdDropCorpse(), true, &actionsInfoSet[UA_DROPCORPSE] );
	CanDoCommand( new NWorld::CCmdExitPK(), true, &actionsInfoSet[UA_EXITPK] );
	CanDoCommand( new NWorld::CCmdHide(), true, &actionsInfoSet[UA_HIDE] );

	CanDoCommand( new NWorld::CCmdStrafe( true ), true, &actionsInfoSet[UA_STRAFE] );
	CanDoCommand( new NWorld::CCmdWishPose( NAI::RUN ), true, &actionsInfoSet[UA_POSERUN] );
	CanDoCommand( new NWorld::CCmdWishPose( NAI::WALK ), true, &actionsInfoSet[UA_POSEWALK] );
	CanDoCommand( new NWorld::CCmdWishPose( NAI::CROUCH ), true, &actionsInfoSet[UA_POSECROUCH] );
	CanDoCommand( new NWorld::CCmdWishPose( NAI::CRAWL ), true, &actionsInfoSet[UA_POSECRAWL] );

	CanDoCommand( new NWorld::CCmdSnipeAttack(), true, &actionsInfoSet[UA_SNIPE_ATTACK] );
	CanDoCommand( new NWorld::CCmdCollectSnipeAP( NWorld::CSAP_1AP ), true, &actionsInfoSet[UA_COLLECTAP_1AP] );
	CanDoCommand( new NWorld::CCmdCollectSnipeAP( NWorld::CSAP_10AP ), true, &actionsInfoSet[UA_COLLECTAP_10AP] );
	CanDoCommand( new NWorld::CCmdCollectSnipeAP( NWorld::CSAP_MAX ), true, &actionsInfoSet[UA_COLLECTAP_MAX] );
	CanDoCommand( new NWorld::CCmdCollectSnipeAP( NWorld::CSAP_ALL ), true, &actionsInfoSet[UA_COLLECTAP_ALL] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// W4.2: GetCamera moved to CMissionBase (retail @0x1a1ee0).
////////////////////////////////////////////////////////////////////////////////////////////////////
float CMission::GetCameraFOV() const
{
	return fFOV;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::GetCameraParams( ECameraType *pType, float *pFOV, ICamera::SCameraLimits *pLimits )
{
	*pFOV = fFOV;
	*pType = eCameraType;
	*pLimits = cameraLimits;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release keeps the tactical cut floor on the camera; this dev build owns it on the render scene, so the
// camera reaches it through this accessor (installed like the height source) for the two-point framing.
class CMissionCameraCutFloor: public ICameraCutFloor
{
	OBJECT_BASIC_METHODS(CMissionCameraCutFloor);
	CPtr<NGame::IMission> pMission;
public:
	CMissionCameraCutFloor() {}
	CMissionCameraCutFloor( NGame::IMission *_pMission ): pMission( _pMission ) {}
	virtual int  GetCutFloor() const { return IsValid( pMission ) ? pMission->GetCutFloor() : 0; }
	virtual void SetCutFloor( int nFloor ) { if ( IsValid( pMission ) ) pMission->SetCutFloor( nFloor ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// The dev runtime-only camera handles: the world/view/UI handles (retail threads pWorld/pView/pUI
// through the camera factory ctor args @0xcf8e0 -- the dev factory keeps its 2-arg signature, so every
// camera-creation/restore path installs them here instead) and the render cut-floor accessor (the
// documented dev relocation of the live floor onto the scene). The terrain height source that used to
// lead this list is gone -- the camera samples pWorld->GetHeightLayers() directly, as retail does.
static void InstallCameraRuntimeHandles( ICamera *pCam, IMission *pMission )
{
	pCam->SetWorld( pMission->GetWorld() );
	pCam->SetView( pMission->GetScene() );
	pCam->SetUI( pMission->GetInterface() );
	pCam->SetCutFloorSource( new CMissionCameraCutFloor( pMission ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::CreateCamera @0x1a2390 (mission vtbl+0xb4) -- the camera FACTORY: create the
// camera and stamp the module DEFAULT hard limits (cam vtbl+0x64 <- ::defaultCameraLimits), the
// default soft limits (vtbl+0x6c <- ::defaultCameraSoftLimits) and the world's map safe zone as the
// scroll-zone rect (vtbl+0x78 <- world vtbl+0x40 GetMapSafeZone). Every CPlayerTracker creates its
// own camera through this (retail ctor @0x287d70). Body lives in iMission.cpp because the
// default-limit statics and the dev accessor helpers are module-locals here.
ICamera* CMissionBase::CreateCamera( ECameraType eType )
{
	ICamera *pNewCamera = ::CreateCamera( eType );
	if ( !pNewCamera )
	{
		ASSERT( 0 );
		return 0;
	}
	pNewCamera->SetLimits( defaultCameraLimits );
	pNewCamera->SetSoftLimits( defaultCameraSoftLimits );
	pNewCamera->SetZoneLimits( GetWorld()->GetMapSafeZone() );
	InstallCameraRuntimeHandles( pNewCamera, this );
	return pNewCamera;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SetCameraParams( ECameraType eType, float _fFOV, const ICamera::SCameraLimits &limits )
{
	CPtr<ICamera> pNewCamera;

	fFOV = _fFOV;
	eCameraType = eType;
	cameraLimits = limits;

	// the retail factory (CMissionBase::CreateCamera @0x1a2390) stamps the default hard/soft limits,
	// the zone rect and the dev runtime handles; the mission-specific hard limits then override the
	// factory defaults below.
	pNewCamera = CreateCamera( eType );
	if ( !IsValid( pNewCamera ) )
	{
		ASSERT( 0 );
		return;
	}
	ICamera::SCameraPos sPos;
	if ( pCamera )
	{
		pCamera->GetPlacement( &sPos );
		pNewCamera->SetPlacement( sPos );
	}
	pNewCamera->SetLimits( cameraLimits );
	pCamera = pNewCamera;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// A raw snapshot reload (CICLoad "game.sav" / CICLoadFile "restart.sav") resumes this mission IN
// PLACE -- no Initialize runs. Rebuild the two runtime-only caches Initialize normally derives:
//  - every building's SBuildingInfo shell cache (deliberately never serialized; without the
//    rebuild the render pipeline re-Builds from an EMPTY cache and the walls/roof/floors vanish)
//    via CWorld::RestoreRuntimeCaches' building->Update() pass (which also re-binds the world's
//    pGlobalGame weak ref). NOT CreateRestored: its StartGame tail restarted a player turn, so a
//    realtime save loaded into turn-based -- retail's load path runs no world hook at all
//    (CICLoad::Exec @0x1f5fd0 -> CMission::OnLoad @0x1fb8e0 = loading-bar counters only) and the
//    deserialized TBS state resumes untouched;
//  - the camera's terrain height source (runtime-only per Camera.cpp; installed only by
//    SetCameraParams @ iMission.cpp).
void CMission::OnSnapshotRestored()
{
	// Restore render state BEFORE rebuilding/lightmapping buildings, using the saved light
	// and light mode (CGameView tags 21/27). GetDefaultLight is a template roulette for NEW
	// scenes: calling it here rerolls day/night. Retail SetRenderMode @0x5872f0 reapplies
	// pPrevLight with prevLightMode, so it also restores our runtime-only lighting state.
	GetScene()->SetRenderMode( GetScene()->GetRenderMode() );
	if ( IsValid( pWorld ) )
		pWorld->RestoreRuntimeCaches( pGlobalGame );
	// re-wire the runtime-only handles on EVERY deserialized camera: the cinematic camera (base
	// tag 18) AND each player tracker's own camera (tracker tag 10 -- the camera the player drives,
	// CMissionBase::GetCamera selector @0x1a1ee0). Retail needs no equivalent: its cameras carry
	// serialized pWorld/pView CPtrs (CCamera tags 2/3).
	if ( IsValid( pCamera ) )
		InstallCameraRuntimeHandles( pCamera, this );
	for ( vector< CObj<IPlayerTracker> >::iterator iPlayer = playersSet.begin(); iPlayer != playersSet.end(); iPlayer++ )
	{
		if ( IsValid( *iPlayer ) && (*iPlayer)->GetCamera() )
			InstallCameraRuntimeHandles( (*iPlayer)->GetCamera(), this );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// W4.2: FocusCameraOnUnit @0x1a16e0 / FocusCameraOnItem @0x1a1740 moved to CMissionBase
// (iMissionBase.cpp) -- retail keeps the camera-focus family on the base.
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMission::GetCutFloor()
{
	return GetScene()->GetCutFloor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SetCutFloor( int nFloor )
{
	GetScene()->SetCutFloor( nFloor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec3& CMission::GetCameraCP() const
{
	return vCameraCP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SHMatrix& CMission::GetCameraPos() const
{
	return sCameraPos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CTransformStack& CMission::GetCameraTransform() const
{
	return sTransform;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CMission::GetTraceObject() const
{
	return sTraceResult.pObject;	// W4.2: retail STraceResult record (tag 21)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::GetTracePosition( CVec3 *pPos ) const
{
	int nMaxFloor = GetScene()->GetCutFloor();
	vector<NAI::SInterval> intervals;
	pWorld->GetAIMap()->Trace( rTraceRay, &intervals, NWorld::TS_PASS_BLOCKER, NAI::CFloorsSet() );
	for ( int k = 0; k < intervals.size(); ++k )
	{
		NAI::SInterval &interv = intervals[k];
		if ( interv.enter.fT < 0 )
			continue;
		if ( interv.pSrc->nFloor > nMaxFloor )
			continue;
		*pPos = rTraceRay.Get( intervals[k].enter.fT );
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::GetTracePosition( NAI::SPosition *pPos ) const
{
	*pPos = sTraceResult.sTile;
	return sTraceResult.bTileSet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRay CMission::GetTraceRay() const
{
	CRay res;
	CTransformStack sTS = GetCameraTransform();
	MakeProjectiveRay( &res.ptDir, &res.ptOrigin, &sTS, GetScene()->GetScreenRect(), GetCursor()->GetPos() );
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// W4.2: GetCursor/GetInterface/IsInterfaceHidden, Set/IsWaitForPartFinished and the desktop stack
// (PopDesktop @0x1a2fc0 / PushDesktop @0x1a3030 / GetDesktop @0x1a2000) moved to CMissionBase.
// CMission keeps ONLY the GetDesktop override below: the retail mission desktop sits at the bottom
// of the base desktopWindowsList (Initialize pushes it), so the base back() covers the retail path;
// the pMissionUI fallback is the dev live-session cache for the pre-Initialize window.
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::CDesktopWindow* CMission::GetDesktop() const
{
	if ( !desktopWindowsList.empty() )
		return desktopWindowsList.back();

	return pMissionUI;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EActionIconsSet CMission::GetActionIconsSet() const
{
	return eActionIconsSet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SetActionIconsSet( EActionIconsSet eMode )
{
	eActionIconsSet = eMode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMission::GetPanelState( int nMask ) const
{
	return nPanelsState & nMask;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SetPanelState( int nMask, bool bState )
{
	// first-mission HUD (retail @0x1fb7e0 masks 0x14 = retail bits MEDALS(0x4)|BIOGRAPHY(0x10);
	// dev renumbered EPanel, so use the names -- the raw 0x14 wrongly blocked dev PANEL_INVENTORY)
	if ( bSpecialFirstMissionMode && bState )
		nMask &= ~( PANEL_MEDALS | PANEL_BIOGRAPHY );
	if ( bState )
		nPanelsState |= nMask;
	else
		nPanelsState &= ~nMask;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// W4.2: Set/GetCheatVisibility and the subsystem getters (GetRPGGame/GetWorld/GetScene/
// GetSoundScene/GetRenderGame) moved to CMissionBase (iMissionBase.cpp).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::Step()
{
	// release CMissionBase::Step @0x1a3ad0 head -- ALL of this was missing (retail's CMission
	// overrides neither Step nor InternalStep: no such symbol exists, so @0x1a3ad0 IS the mission's
	// Step). Three pieces, in retail's order, before CanRender:
	//
	// 1. The FPS limiter (@0x5a3adb-@0x5a3ae7): skip the ENTIRE frame while less than sMinFrameTime
	//    has elapsed. sMinFrameTime/sFPSLimitLastTime were serialized (tags 36/37) with NO consumer
	//    anywhere -- the 5th "serialized, no consumer" pair found in this tree. The compare is
	//    UNSIGNED (`jb`), so the shipped mission default of 0 makes it a no-op, exactly as in retail
	//    (only CChapterMap/CGlobalMap::Initialize stamp 5 -- .text has no other writer to +0xfc).
	// 2. sTimeCounter (@0x5a3b10) -- the pausable accumulating GAME clock (drives the world via
	//    pTimeFunc + nDeltaTime below), and sUITimeCounter (@0x5a3b25) which runs even while paused
	//    (drives pInterface->Draw via GetUITime). CTimeCounter::Advance is byte-faithful to retail's
	//    @0x70f570 (same 150ms clamp, same accumulate + DG version bump). GetTime() is the raw
	//    main-loop clock like retail (the old dev override that folded nDeltaTime in is GONE).
	if ( GetTime() - sFPSLimitLastTime < sMinFrameTime )
		return;
	sFPSLimitLastTime = GetTime();
	sTimeCounter.Advance( !bPause, GetTime() );
	sUITimeCounter.Advance( true, GetTime() );
	EraseInvalidRefs( &soundsList );	// retail @0x5a3b2a

	if ( CanRender() )
	{
		bool bAdvance = !bPause;
		do
		{
			// retail @0x5a3b50: cinematic slo-mo divider -- while the camera reports a ratio > 1 only
			// every Nth frame advances the world (nFake counter, retail global @0x9c62a8)
			if ( bAdvance && GetCamera()->GetSloMoRatio() > 1 )
			{
				++nFake;
				bAdvance = ( nFake % GetCamera()->GetSloMoRatio() ) == 0;
			}

			// retail @0x5a3c07-@0x5a3c3e: world time = GetGameTime() (sTimeCounter node + nDeltaTime),
			// applied HERE -- GetTime() stays the raw main-loop clock (the old dev GetTime() override
			// folded nDeltaTime into every consumer, leaking the skip fast-forward into the sound clock)
			pRender->UpdateViewWorld( bWaitForPartFinished || bAdvance, GetGameTime(),
				pActivePlayer->GetPlayer(), bShowAllCheat || bCheatVisibility );

			InternalStep();
			if ( !bWaitForPartFinished )
				break;
			MarkNewDGFrame();
			// retail @0x5a3c56: a queued interface command (load/exit) aborts the skip fast-forward
			// without rendering this frame
			if ( NMainLoop::HaveInterfaceCommand() )
				return;
			SetDeltaTime( GetDeltaTime() + 50 );
		} while ( true );

		NUI::CWindow *pClientWindow = GetDesktop()->GetClientWindow();

		NUI::SRect sScrWindow;
		NUI::SPoint sScrPosition;
		const NUI::SPoint &sScrSize = pClientWindow->GetSize();
		pClientWindow->ClientToScreen( &sScrPosition, &sScrWindow );

		NUI::SRect sScrClientRect( sScrPosition.x, sScrPosition.y, sScrPosition.x + sScrSize.x, sScrPosition.y + sScrSize.y );
		GetCamera()->SetScreenRect( CTRect<float>( float( sScrClientRect.x1 ) / 1024.0f, float( sScrClientRect.y1 ) / 768.0f, float( sScrClientRect.x2 ) / 1024.0f, float( sScrClientRect.y2 ) / 768.0f ) );

		int nFlags = bRenderWorld ? N_RENDERMODE_3D : 0;	// retail @0x5a3d11 gates 3D on bRenderWorld
		if ( !bHideInterface )
			nFlags |= N_RENDERMODE_2D;

		NGScene::ClearScreen( CVec3( 0, 0, 0 ) );	// release ClearScreenZBuffer() @0x5a3d26 (absent) -> black clear
		RenderFrame( nFlags, bAdvance, GetCamera() );
	}
	else
	{
		pRender->ResetTiming();	// retail @0x2cb190 forwards to both sound mixers
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::InternalStep()
{
	// retail CMission::GameStep @0x2017a0 head: track the world current-player; on a change stamp
	// tLastCurrentPlayerChange with the UI clock (the @0x1fbf10 end-of-turn 2s cooldown reads it).
	NWorld::IPlayer *pCurrentPlayer = pWorld->GetCurrentPlayer();
	if ( pCurrentPlayer != pPrevCurrentPlayer )
	{
		pPrevCurrentPlayer = pCurrentPlayer;
		if ( pCurrentPlayer )
			tLastCurrentPlayerChange = GetUITime();
	}

	// retail CMissionBase::InternalStep @0x1a29f0: on a turn change (hot-seat) just Deactivate ->
	// swap pActivePlayer -> Activate. NO camera copies -- each tracker OWNS its camera (tag 10) and
	// the GetCamera selector (@0x1a1ee0) switches with the active player automatically.
	if ( ( playersSet.size() > 1 ) && !IsPlayerTurn() )
	{
		pActivePlayer->Deactivate();
		for ( vector< CObj<IPlayerTracker> >::iterator iPlayer = playersSet.begin(); iPlayer != playersSet.end(); iPlayer++ )
		{
			if ( (*iPlayer)->GetPlayer() == pWorld->GetCurrentPlayer() )
				pActivePlayer = (*iPlayer);
		}
		pActivePlayer->Activate();
	}

	ExecWorldCommands();
	// executors run on the GAME clock (retail ProcessWorldCommands @0x1a24a0 passes GetGameTime()
	// @0x1a4500) -- nDeltaTime growth fast-forwards camera transitions through the skip-part loop,
	// releasing their WaitForUI ids; the raw clock is CONSTANT inside that loop and would hang it.
	if ( IsValid( pCmdExec ) )
	{
		if ( pCmdExec->Update( GetGameTime() ) )
		{
			pCmdExec->Finished();
			pCmdExec = 0;
		}
	}
	// release: drive the dedicated camera-locator executor (auto-focus) each frame in its OWN slot, in
	// parallel with pCmdExec (so a shot/death focus never blocks the general UI-command drain).
	if ( IsValid( pExecLocator ) )
	{
		if ( pExecLocator->Update( GetGameTime() ) )
		{
			pExecLocator->Finished();
			pExecLocator = 0;
		}
	}

	// retail CMissionBase::ProcessCameraCommands @0x1a25a0 (called from InternalStep after the
	// world-command drain, BEFORE the camera Update): drain the world earthquake queue into the
	// active camera. GetEarthQuakeEvent pops with ownership transfer; a null/zombie event terminates.
	// (both the drain and the frame update go through the SELECTOR vtbl+0xb8, like retail)
	for ( ;; )
	{
		CPtr<NWorld::CEarthQuakeEvent> pEQ = pWorld->GetEarthQuakeEvent();
		if ( !IsValid( pEQ ) )
			break;
		GetCamera()->AddEarthQuake( pEQ->vPosition, pEQ->fPower );
	}

	// retail InternalStep @0x1a29f0: GetCamera()->Update (cam vtbl+0x80) -- the ACTIVE camera only;
	// the frame transform below is dev bookkeeping recomputed from the same selected camera.
	ICamera *pFrameCamera = GetCamera();
	pFrameCamera->Update( GetTime() );

	vCameraCP = pFrameCamera->GetCP();
	sCameraPos = pFrameCamera->GetPos();
	pFrameCamera->GetTransform( &sTransform, GetScene()->GetScreenRect() );
	if ( vPrevCameraPosition != vCameraCP )
	{
		vPrevCameraPosition = vCameraCP;
		nFramesSameCameraPosition = 0;
	}
	else
		++nFramesSameCameraPosition;

	for ( vector< CObj<IPlayerTracker> >::iterator iPlayer = playersSet.begin(); iPlayer != playersSet.end(); iPlayer++ )
		(*iPlayer)->Update( (*iPlayer)->GetPlayer() == pWorld->GetCurrentPlayer() );

	if ( !bLoseSignalSended && pActivePlayer->IsPlayerLoser() )
	{
		// BUG 3 (delayed Lose dialog) -- retail plumbing now ported (W3.3): GameStep @0x2017a0 (single-player
		// defeat) fires OnPlayerLose wrapped in a CCmdDelayedCallGameOver (nMaxDelay = 4000ms, ctor @0x204b50).
		// CWorld stashes it (pGameOverCall @+0x1c4) and fires it when the HERO's corpse settles
		// (CWorld::InformCorpseStop @0x362180, IsHero gate), capped at 4000ms (CWorld::Segment tail @0x36bce0).
		// The lose UI itself is lua-driven: OnPlayerLose -> ShowLoseDialog(20984/20985) -> CUICmdLoseDialog ->
		// CICLoseMenu (ExecWorldCommand below) -- retail opens no menu here in single-player.
		// Param = bScenarioGameOver (1) vs natural defeat (0). Retail skips the delay wrapper only when
		// bScenarioGameOver is set (scenario failure = immediate game over); the member (retail tag 36,
		// W4.2) has no dev scenario-failure writer yet, so the delayed path is the one that runs.
		bLoseSignalSended = true;
		if ( bScenarioGameOver )
			DoEvent( new NWorld::CCmdCallScriptFunction( "OnPlayerLose", "i", 1 ) );
		else
			DoEvent( new NWorld::CCmdDelayedCallGameOver(
				new NWorld::CCmdCallScriptFunction( "OnPlayerLose", "i", 0 ), 4000 ) );
	}

	pInterface->UpdateCursor();

	sTraceResult.bTileSet = false;	// W4.2: was the dev bTraceOk
	if ( IsReady() )
	{
		TraceCursor();

		bUpdated = TrackChanges() || bForceUpdateNextFrame;
		bForceUpdateNextFrame = false;

		if ( bUpdated )
			UpdateActionsInfo();

		if ( ( pState->GetType() != IState::TEMPORARY ) && ( IsUpdated() || ( pState->GetType() == IState::INSTANT ) ) )
			UpdateState();
	}
	else
	{
		bUpdated = TrackChanges();
		bForceUpdateNextFrame = true;

		for( int nTemp = 0; nTemp < actionsInfoSet.size(); nTemp++ )
			actionsInfoSet[nTemp].bOk = false;

		// v1.2 0x602443..0x6024f3: busy state bypasses normal hover selection.
		if ( IsPlayerTurn() )
			actionsInfoSet[UA_STOP].SetValid();
		ResetState();
		CommandState( new CStateWait() );
	}

	pState->Step();

	UpdateSound();

	// retail base InternalStep @0x5a2b2f-@0x5a2b6e: the interface steps at most every 100ms on the
	// UI counter, or immediately when the mission reports IsUpdated (vtbl+0x80); ONLY those frames
	// reset the state target (vtbl+0x98 SetStateTarget(0)) and restamp sLastUpdateTime.
	{
		STime uiTime = GetUITime();
		if ( uiTime - sLastUpdateTime > 100 || IsUpdated() )
		{
			SetStateTarget( 0 );
			pInterface->Step( uiTime );
			sLastUpdateTime = uiTime;
		}
	}
	// retail @0x5a2bb7: the desktop (movie/dialog fades + their executors) runs on the GAME clock --
	// pTimeFunc + nDeltaTime -- so the skip-part loop's +50/iteration drives the fades to completion.
	// Raw GetTime() here stalled the fade -> WaitForUI never released -> the ESC-skip froze.
	GetDesktop()->UpdateDesktop( GetGameTime() );

	bHasCommands |= pActivePlayer->GetCommander()->HasCommands();

	// retail CMissionBase::InternalStep @0x1a29f0 tail (@0x1a2bd4): while the cinematic camera is
	// NOT the live one and no camera-locator exec is running, park it on the live camera's pose and
	// cut floor. This is why retail shows no cut when the enemy-turn/interrupt handover swaps to it:
	// it was already sitting exactly where the user left his own camera.
	if ( GetCamera() != pCamera && !IsValid( pExecLocator ) )
	{
		ICamera::SCameraPos sPos;
		GetCamera()->GetPlacement( &sPos );
		pCamera->SetPlacement( sPos );							// @0x1a2c41, bOnTerrain = false
		pCamera->SetCutFloor( GetCamera()->GetCutFloor() );		// @0x1a2c57/@0x1a2c5d
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x1fcbd0 (mission vtbl+0xe0). Status-string ids verified against the retail Strings table:
// 17855 "You haven't found critical information clue yet.", 17433 "Can't leave the area during combat",
// 20238 "You can leave the combat area.", 20239 "Can't leave the area. Not all units at the border."
bool CMission::CanLeaveZone( wstring *pwsReason )
{
	vector< CPtr<NWorld::CUnit> > unitsSet;
	GetActivePlayer()->GetPlayer()->GetUnits( &unitsSet );

	// scenario gate: leaving must not strand a key clue (also refreshes bScenarioGameOver)
	if ( IsValid( pZone ) )
	{
		if ( !GetRPGGame()->pScenarioTracker->CanLeaveZone( unitsSet, pZone, &bScenarioGameOver ) )
		{
			*pwsReason = NUI::GetDBString( 17855 );
			return false;
		}
	}

	// script block (SetLeaveZoneMode)
	if ( bLeaveBlockedByScript )
	{
		if ( IsValid( pBlockReason ) )
			*pwsReason = NUI::GetDBString( pBlockReason );
		else
			*pwsReason = NUI::GetDBString( 17855 );
		return false;
	}

	// classify live units against the map safe zone: at-border = outside the interior rect
	bool bAllAtBorder = true;
	bool bSomeAtBorder = false;
	const CTRect<float> &rcSafe = GetWorld()->GetMapSafeZone();
	for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
	{
		if ( unitsSet[nTemp]->IsDead() )
			continue;	// a dead unit touches neither flag (retail unit vtbl+4 skip)
		CVec3 vCP = unitsSet[nTemp]->GetPosition().GetCP();
		if ( vCP.x <= rcSafe.minx || vCP.x >= rcSafe.maxx || vCP.y <= rcSafe.miny || vCP.y >= rcSafe.maxy )
			bSomeAtBorder = true;
		else
			bAllAtBorder = false;
	}

	if ( GetActivePlayer()->IsPlayerLoser() )
	{
		*pwsReason = NUI::GetDBString( 17433 );
		return false;
	}

	if ( IsRealTime() || GetActivePlayer()->IsPlayerWinner() || bAllAtBorder )
	{
		*pwsReason = NUI::GetDBString( 20238 );
		return true;
	}

	if ( bSomeAtBorder )
		*pwsReason = NUI::GetDBString( 20239 );
	else
		*pwsReason = NUI::GetDBString( 17433 );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::ProcessEvent( const NInput::SEvent &sEvent )
{
	NInput::SetSection( "game" );

	pCursor->ProcessEvent( sEvent );

	if ( bindHideInterface.ProcessEvent( sEvent ) )
	{
		bHideInterface = !bHideInterface;
		return true;
	}
	else if ( bindSpecialHideInterface.ProcessEvent( sEvent ) )
	{
		bSpecialHideInterface = !bSpecialHideInterface;
		return true;
	}
	else if ( bindPause.ProcessEvent( sEvent ) )
	{
		bPause = !bPause;
		return true;
	}

	// "mainmenu" is the F10 key; "gamemenu" is the on-screen Pause-Menu (HUD) button -- both open the in-game menu.
	if ( bindMainMenu.ProcessEvent( sEvent ) || bindGameMenu.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICInGameMenu( GetActivePlayer()->GetGlobalPlayer(), true /*bAllowRestart: opened from a mission*/, bCanSave ) );
		return true;
	}
	else if ( bindSaveMenu.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICSaveLoadMenu( SAVE, 0, bCanSave ) );   // retail CMissionBase::ProcessEvent passes this->bCanSave (disasm mov al,[esi+0xe4])
		return true;
	}
	else if ( bindLoadMenu.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICSaveLoadMenu( LOAD, 0, bCanSave ) );
		return true;
	}
	// Retail @0x202400: the UI/console (CInterface::ProcessEvent) gets the event AFTER the
	// menu binds (mainmenu/save/load) and BEFORE the letter-keyed clues/objectives hotkeys,
	// so an open in-mission console consumes the typed EVENT_CHAR before the journal/objectives
	// binds can match it (otherwise e.g. 'J' opens the Journal instead of typing into the console).
	if ( pInterface->ProcessEvent( sEvent ) )
		return true;

	if ( bindCluesMenu.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICClues( GetRPGGame() ) );
//		NMainLoop::Command( new CICTeamMngMenu( pActivePlayer->GetGlobalPlayer(), this ) );
//		pMissionDlgUI->ShowDialog();
		return true;
	}
	// the on-screen Objectives-Menu (HUD) button -- open the objectives/journal modal (renders stored goals/tasks).
	else if ( bindObjectivesMenu.ProcessEvent( sEvent ) )
	{
		// retail: the HUD Objectives button opens the FRAMED objectives modal (CICShowObjectives ->
		// CShowObjectivesUI: CClueLine "?" title rows + numbered CTaskLine rows in bordered boxes, only the
		// next open task per goal), not the old flat CText list (CICObjectives). Exec no-ops when not in a zone.
		NMainLoop::Command( new NGame::CICShowObjectives( this, GetRPGGame()->pCurrentZone ) );
		return true;
	}

/// SYSTEM( No unit or player allowed! )
	if ( bindShadows.ProcessEvent( sEvent ) )
		pScene->SetNextShadowsMode();
	else if (	bindToggleTransp.ProcessEvent( sEvent ) )
		pScene->SetNextTranspRenderMode();
	else if ( bindShowLightmap.ProcessEvent( sEvent ) )
		pScene->SetNextLightmapViewMode();
	else if ( bindHSR.ProcessEvent( sEvent ) )
		pScene->SetNextHSRMode();
	else if ( bindFog.ProcessEvent( sEvent ) )
		pScene->SetNextFogMode();
	// retail clamps the cut floor to the template variant's [MinCutFloor, MaxCutFloor) range
	// (CBaseCamera::SetCutFloor @0xd0050 against the range CMission::Initialize @0x200690 installs);
	// dev keeps floors on the scene, so the same clamp is applied here.
	else if ( bindAddFloor.ProcessEvent( sEvent ) )
		pScene->SetCutFloor( Min( pScene->GetCutFloor() + 1, nMaxCutFloor ) );
	else if ( bindSubFloor.ProcessEvent( sEvent ) )
		pScene->SetCutFloor( Max( pScene->GetCutFloor() - 1, nMinCutFloor ) );
	else if ( bindShowParticles.ProcessEvent( sEvent ) )
		pScene->SetParticleShow( !pScene->GetParticleShow() );
	else if ( bindShowAI.ProcessEvent( sEvent ) )
	{
		ICamera::SCameraPos cameraPos;
		GetCamera()->GetPlacement( &cameraPos );   // the SELECTED (visible) camera's pose
		NMainLoop::Command( new CICAIView(
			pWorld->GetAIMap(), 
			pWorld->GetPathNetwork(), 
			pWorld->GetGame()->GetVisionTracker(), 
			cameraPos ) );
		return true;
	}
	else if ( bindShowRAD.ProcessEvent( sEvent ) )
	{
		ICamera::SCameraPos cameraPos;
		GetCamera()->GetPlacement( &cameraPos );   // the SELECTED (visible) camera's pose
		NMainLoop::Command( new CICRadTest( pScene, cameraPos ) );
		return true;
	}
	else if ( bindShowVisibility.ProcessEvent( sEvent ) )
		ShowVisibleFrom();
	else if ( bindShowVision.ProcessEvent( sEvent ) )
	{
		if ( !pVisibleTracker )
		{
			pVisibleTracker = new CVisibleTracker( CVisibleTracker::VISIBLE );
			pVisibleTracker->Update( this );
		}
		else
			pVisibleTracker = 0;
	}
	else if ( bindShowSchema.ProcessEvent( sEvent ) )
	{
		if ( !buildingSchemas.empty() )
		{
			buildingSchemas.clear();
			return true;
		}

		CViewBuildingScheme b( pWorld->GetActive(), pScene, &buildingSchemas );
		b.Sync();
	}
	else if ( bindCheckBuildings.ProcessEvent( sEvent ) )
	{
		CUpdateBuildingStability b( pWorld->GetActive() );
		b.Sync();
	}
	else if ( bindTestIsect.ProcessEvent( sEvent ) )
		TestIntersection();
	else if ( bindShowRain.ProcessEvent( sEvent ) )
		ShowWeatherEffect( 817 );
	else if ( bindShowSnow.ProcessEvent( sEvent ) )
		ShowWeatherEffect( 732 );
	else if ( bindClickOfDeath.ProcessEvent( sEvent ) )
		ClickOfDeath();
	else if ( bindExplosion.ProcessEvent( sEvent ) )
		Explosion();
	else if ( bindErasePart.ProcessEvent( sEvent ) )
		ErasePartUnderCursor();
	else if ( bindRPGStats.ProcessEvent( sEvent ) )
	{
		vector< CPtr<IPlayerTracker> > playersSet;
		GetPlayers( &playersSet );
		for ( int i = 0; i < playersSet.size(); ++i )
		{
			if ( !IsValid( playersSet[i] ) )
				continue;
			csRPG << "<color=cyan>" << "PLAYER " << i << ":\n";
			vector< CPtr<IUnitTracker> > units;
			playersSet[i]->GetUnits( &units );
			for ( int j = 0; j < units.size(); ++j )
			{
				if ( !IsValid( units[j] ) || !IsValid( units[j]->GetUnit() ) )
					continue;
				csRPG << "<color=green>" << "Unit " << j << ":";
				units[j]->GetUnit()->GetRPG()->DumpStats();
			}
		}
		csRPG << "\n";
		return true;
	}
	else if ( bindExplode.ProcessEvent( sEvent ) )
	{
		vector< CPtr<NGame::IUnitTracker> > unitsSet;
		GetSelectedUnits( &unitsSet );
		for ( vector< CPtr<NGame::IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
			Command( (*iTemp)->GetUnit(), new NWorld::CCmdExplode );
		return true;
	}

	if ( bindSwitchLighting.ProcessEvent( sEvent ) )
		SetLightMode( nLightMode + 1 );

	if ( IsReady() )
	{
		if ( pState->ProcessEvent( sEvent ) )
			return true;
	}

	if ( GetDesktop()->ProcessEvent( sEvent ) )
		return true;

	// retail CMission::ProcessEvent @0x202400: the event goes UNCONDITIONALLY to the SELECTED camera
	// (GetCamera() vtbl+0xb8 -> ProcessEvent vtbl+0x84). During an enemy-turn follow the selected
	// camera is the locked cinematic one -- ProcessEvent latching its unlock-intent pair (@0xcbd00)
	// is exactly what makes the next GetCamera() hand control back to the player's camera (the
	// "interrupt the enemy-turn cinematic" feature, UserWantedItToUnlock @0xd0020).
	// Dev adaptation kept: while a sequence runs, retail's movieUI DESKTOP is modal and consumes the
	// event before this point; the dev desktop chain passes unconsumed events through, so the
	// explicit sequence gate stands in for that modality.
	if ( !IsSequence() && !pWorld->IsSequence() )
		GetCamera()->ProcessEvent( sEvent );

//////////////////////////////////////////
/// Unit control
	// retail @0x202400: the unit-control binds below are gated on IsPlayerTurn (mission vtbl+0x48
	// @0x1a1910)
	if ( !IsPlayerTurn() )
		return false;

	if ( bindCancel.ProcessEvent( sEvent ) || bindCancelAction.ProcessEvent( sEvent ) )
	{
		if ( pState->GetType() == IState::FORCED )
			ResetState();
		else if ( IsActionExecuted() )
			StopAction();
		else
		{
			vector<CPtr<IUnitTracker> > unitsSet;
			GetSelectedUnits( &unitsSet );

			for ( vector<CPtr<IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
			{
				NWorld::CUnit::EState eState = (*iTemp)->GetUnit()->GetState();
				if ( eState == NWorld::CUnit::ST_MACHINE_GUN )
					Command( (*iTemp)->GetUnit(), new NWorld::CCmdExitCannon );
				else if ( ( eState == NWorld::CUnit::ST_SNIPE ) || ( eState == NWorld::CUnit::ST_HEALER ) || !(*iTemp)->IsPathComplete() ) 
					Command( new NWorld::CCmdCancel( (*iTemp)->GetUnit() ) );
			}
		}

		return true;
	}

	if ( !IsRealTime() && IsActionExecuted() )
		return false;

	if ( IsRealTime() )
	{
		if ( bindStartOfTurn.ProcessEvent( sEvent ) )
		{
			vector< CPtr<IUnitTracker> > unitsSet;
			GetUnits( &unitsSet );
			if ( unitsSet.size() > 0 )
				Command( unitsSet.front()->GetUnit(), new NWorld::CCmdStartCombat );
			return true;
		}
	}
	else 
	{
		if ( bindEndOfTurn.ProcessEvent( sEvent ) )
		{
			Command( new NWorld::CCmdEndOfTurn );
			return true;
		}
	}

	if ( bindEndMission.ProcessEvent( sEvent ) )
	{
		// retail CMission::ProcessEvent @0x202f96: a blocked exit prints the CanLeaveZone reason into the
		// on-screen game log; an allowed one fires the engine->script hook OnExit(). The campaign OnExit()
		// -> ShowLeaveZoneDialog(17041) -> CUICmdLeaveZoneDlg -> CICLeaveZoneMenu -> (confirm) CICEndMission.
		wstring wsReason;
		if ( !CanLeaveZone( &wsReason ) )
		{
			csGame << wsReason << endl;
			return true;
		}
		DoEvent( new NWorld::CCmdCallScriptFunction( "OnExit", "" ) );
		return true;
	}

	if ( bindContinue.ProcessEvent( sEvent ) )
	{
		SActionInfo sInfo;
		GetActionInfo( UA_CONTINUE, &sInfo );
		if ( sInfo.bAvailable && sInfo.bOk )
		{
			vector<CPtr<IUnitTracker> > unitsSet;
			GetSelectedUnits( &unitsSet );

			for ( vector<CPtr<IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
				Command( (*iTemp)->GetUnit(), new NWorld::CCmdContinue() );

			return true;
		}
	}

	/// check for selection
	int nSelection = -1;
	if ( bindHero1.ProcessEvent( sEvent ) )
		nSelection = 0;
	else if ( bindHero2.ProcessEvent( sEvent ) )
		nSelection = 1;
	else if ( bindHero3.ProcessEvent( sEvent ) )
		nSelection = 2;
	else if ( bindHero4.ProcessEvent( sEvent ) )
		nSelection = 3;
	else if ( bindHero5.ProcessEvent( sEvent ) )
		nSelection = 4;
	else if ( bindHero6.ProcessEvent( sEvent ) )
		nSelection = 5;

	if ( nSelection != -1 )
	{
		vector< CPtr<IUnitTracker> > unitsSet;
		GetUnits( &unitsSet );
		if ( nSelection < unitsSet.size() )
		{
			if ( !unitsSet[nSelection]->IsSelected() || ( CountSelected() != 1 ) )
				Select( unitsSet[nSelection]->GetUnit(), false );
			else
				FocusCameraOnUnit( unitsSet[nSelection]->GetUnit() );
		}
	}
	else if ( bindSelPrev.ProcessEvent( sEvent ) )
		SelectPrev();
	else if ( bindSelNext.ProcessEvent( sEvent ) )
		SelectNext();

	if ( bindPrevSlot.ProcessEvent( sEvent ) )
		SelectSlot( -1 );
	if ( bindNextSlot.ProcessEvent( sEvent ) )
		SelectSlot( 1 );

	if ( bindWeaponReload.ProcessEvent( sEvent ) )
		WeaponReload();
	if ( bindItemUnload.ProcessEvent( sEvent ) )
	{
		CDynamicCast<CStateUnloadItem> pState(GetState());
		if (pState)
		{
			ResetState();
		}
		else
		{
			ResetState();
			CommandState( new CStateUnloadItem );
		}
	}
	if ( bindItemArrange.ProcessEvent( sEvent ) )
	{
		vector<CPtr<IUnitTracker> > unitsSet;
		GetSelectedUnits( &unitsSet );

		for ( vector<CPtr<IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
			Command( (*iTemp)->GetUnit(), new NWorld::CCmdArrangeInventory() );
	}

	if ( bindSnapShot.ProcessEvent( sEvent ) )
		NewWeaponMode( NDb::SM_Snap );
	else if ( bindAimedShot.ProcessEvent( sEvent ) )
		NewWeaponMode( NDb::SM_Aimed );
	else if ( bindCarefulShot.ProcessEvent( sEvent ) )
		NewWeaponMode( NDb::SM_Careful );
	else if ( bindShortBurst.ProcessEvent( sEvent ) )
		NewWeaponMode( NDb::SM_ShortBurst );
	else if ( bindLongBurst.ProcessEvent( sEvent ) )
		NewWeaponMode( NDb::SM_LongBurst );
	else if ( bindSnipeShot.ProcessEvent( sEvent ) )
		NewWeaponMode( NDb::SM_Snipe );

	if ( bindGrenadeModeThrow.ProcessEvent( sEvent ) )
		NewGrenadeMode( NRPG::GM_THROW );
	if ( bindGrenadeModeSetTrap.ProcessEvent( sEvent ) )
		NewGrenadeMode( NRPG::GM_SETTRAP );

	if ( bindWeaponPrevMode.ProcessEvent( sEvent ) )
		SelectWeaponMode( -1 );
	else if ( bindWeaponNextMode.ProcessEvent( sEvent ) )
		SelectWeaponMode( 1 );

	/// Forced actions
	if ( bindMove.ProcessEvent( sEvent ) )
		CommandState( new CStateMove( true ) );
	else if ( bindRotate.ProcessEvent( sEvent ) )
		CommandState( new CStateRotate() );
	else if ( bindAttack.ProcessEvent( sEvent ) )
		CommandState( new CStateAttack( true ) );
	else if ( bindSetMine.ProcessEvent( sEvent ) )
		CommandState( new CStateSetMine() );
	else if ( bindUseTool.ProcessEvent( sEvent ) )
		CommandState( new CStateUntrap( true ) );	// use the held tool on a target (disassemble/disarm/mount) -- retail @0x202600 constructs the FORCED untrap here
	else if ( bindSetTrap.ProcessEvent( sEvent ) )
		CommandState( new CStateSetTrap() );
	else if ( bindFirstAid.ProcessEvent( sEvent ) )
		CommandState( new CStateFirstAid() );
	else if ( bindDropCorpse.ProcessEvent( sEvent ) )
		CommandState( new CStateDropCorpse() );
	else if ( bindExitPK.ProcessEvent( sEvent ) ) //// CRAP
	{
		vector< CPtr<NGame::IUnitTracker> > unitsSet;
		GetSelectedUnits( &unitsSet );
		for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
			Command( unitsSet[nTemp]->GetUnit(), new NWorld::CCmdExitPK() );
	}

	if ( bindNormalPose.ProcessEvent( sEvent ) )
		CommandState( new CStatePose( NAI::WALK ) );
	else if ( bindCrawlPose.ProcessEvent( sEvent ) )
		CommandState( new CStatePose( NAI::CRAWL ) );
	else if ( bindCrouchPose.ProcessEvent( sEvent ) )
		CommandState( new CStatePose( NAI::CROUCH ) );
	else if ( bindRunPose.ProcessEvent( sEvent ) )
		CommandState( new CStatePose( NAI::RUN ) );
	else if ( bindStrafe.ProcessEvent( sEvent ) )
	{
		SActionInfo sAction;
		GetActionInfo( UA_STRAFE, &sAction );
		if ( sAction.eResult == NWorld::UCR_OK )
		{
			vector< CPtr<NGame::IUnitTracker> > unitsSet;
			GetSelectedUnits( &unitsSet );
			if ( unitsSet.size() == 1 )
			{
				NWorld::CUnit *pUnit = unitsSet.front()->GetUnit();
				Command( pUnit, new NWorld::CCmdStrafe( !pUnit->IsStrafing() ) );
			}
		}
		else
			ShowError( this, sAction.eResult );
	}
	else if ( bindHide.ProcessEvent( sEvent ) )
	{
		SActionInfo sAction;
		GetActionInfo( UA_STRAFE, &sAction );
		if ( sAction.eResult == NWorld::UCR_OK )
		{
			vector< CPtr<NGame::IUnitTracker> > unitsSet;
			GetSelectedUnits( &unitsSet );
			if ( unitsSet.size() == 1 )
				Command( unitsSet.front()->GetUnit(), new NWorld::CCmdHide() );
		}
		else
			ShowError( this, sAction.eResult );
	}

	if ( bindSnipeAttack.ProcessEvent( sEvent ) )
	{
		SActionInfo sAction;
		GetActionInfo( UA_SNIPE_ATTACK, &sAction );
		if ( sAction.eResult == NWorld::UCR_OK )
		{
			vector< CPtr<NGame::IUnitTracker> > unitsSet;
			GetSelectedUnits( &unitsSet );
			if ( unitsSet.size() == 1 )
				Command( unitsSet.front()->GetUnit(), new NWorld::CCmdSnipeAttack() );
		}
		else
			ShowError( this, sAction.eResult );
	}
	else if ( bindCollect1AP.ProcessEvent( sEvent ) )
		UnitCollectAP( NWorld::CSAP_1AP );
	else if ( bindCollect10AP.ProcessEvent( sEvent ) )
		UnitCollectAP( NWorld::CSAP_10AP );
	else if ( bindCollectMaxAP.ProcessEvent( sEvent ) )
		UnitCollectAP( NWorld::CSAP_MAX );
	else if ( bindCollectAllAP.ProcessEvent( sEvent ) )
		UnitCollectAP( NWorld::CSAP_ALL );

	if ( bindNextEnemy.ProcessEvent( sEvent ) )
	{
		vector< CPtr<NGame::IUnitTracker> > unitsSet;
		GetSelectedUnits( &unitsSet );
		if ( unitsSet.size() == 1 )
		{
			CPtr<NWorld::CUnit> pEnemy = unitsSet[0]->GetNextVisibleEnemy();
			if ( IsValid( pEnemy ) )
				FocusCameraOnUnit( pEnemy );
		}
	}
	else if ( bindCheatTeleport.ProcessEvent( sEvent ) )
	{
		vector< CPtr<NGame::IUnitTracker> > unitsSet;
		GetSelectedUnits( &unitsSet );
		if ( unitsSet.size() == 1 )
		{
			NAI::SUnitPosition sPosition = unitsSet[0]->GetUnit()->GetPosition();
			if( GetTracePosition( &sPosition.pos ) )
				Command( unitsSet[0]->GetUnit(), new NWorld::CCmdTeleport( sPosition ) );
		}

		return true;
	}
	else if ( bindFocusUnit.ProcessEvent( sEvent ) )
	{
		vector< CPtr<IUnitTracker> > unitsSet;
		GetSelectedUnits( &unitsSet );
		if ( unitsSet.size() == 1 )
		{
			for ( vector< CPtr<IUnitTracker> >::iterator iUnit = unitsSet.begin(); iUnit != unitsSet.end(); iUnit++ )
				FocusCameraOnUnit( (*iUnit)->GetUnit() );
		}
	}


//// Some special binds
	if ( bindTrailerHideInterface.ProcessEvent( sEvent ) )
	{
		CPtr<NUI::CMissionTrailerUI> pMissionTrailerUI = new NUI::CMissionTrailerUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "trailerUI", NUI::STYLE_ENABLED ), this, GetDesktop() );
		NUI::LoadTemplate( pMissionTrailerUI, NDb::GetUIContainer( 376 ) );
		pMissionTrailerUI->ShowDesktop();
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// W4.2: RenderFrame moved to CMissionBase (retail @0x1a19f0 keeps the frame render on the base;
// the body only touched base members).
////////////////////////////////////////////////////////////////////////////////////////////////////
// Internal
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMission::TrackChanges()
{
	bool bUpdated = false;

	CPtr<NWorld::IPlayer> pNewTrackPlayer = pWorld->GetCurrentPlayer();
	if ( pNewTrackPlayer != pTrackPlayer )
	{
		bUpdated = true;
		pTrackPlayer = pNewTrackPlayer;
	}

	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );
	if ( unitsSet != selectedUnits )
	{
		bUpdated = true;
		selectedUnits = unitsSet;
	}

	// W4.2: retail TrackChanges @0x1fcf60 caches IsReady() into bTrackIsReady (tag 10) with a PLAIN
	// store -- a readiness flip alone does not raise the update flag in retail either.
	bTrackIsReady = IsReady();

	// dev-extra realtime tracker (retail has no bRealTime member; kept transient)
	bool bNewRealTime = IsRealTime();
	if ( bNewRealTime != bRealTime )
	{
		bUpdated = true;
		bRealTime = bNewRealTime;
	}

	NWorld::SItem sItem;
	CPtr<CObjectBase> pNewPlayerInHand;
	if ( pActivePlayer->GetPlayer()->GetInHandItem( &sItem ) )
		pNewPlayerInHand = sItem.pItem;
	if ( pNewPlayerInHand != pTrackPlayerInHand )
	{
		bUpdated = true;
		pTrackPlayerInHand = pNewPlayerInHand;
	}

	// dev-extra commander-queue tracker (retail folds it into IsActionExecuted; kept transient)
	bool bNewHasCommands = pActivePlayer->GetCommander()->HasCommands();
	if ( bNewHasCommands != bHasCommands )
	{
		bUpdated = true;
		bHasCommands = bNewHasCommands;
	}

	bool bNewActionExecuted = IsActionExecuted();
	if ( bNewActionExecuted != bTrackActionExecuted )
	{
		bUpdated = true;
		bTrackActionExecuted = bNewActionExecuted;
	}

	CPtr<CObjectBase> pNewTrackTarget = GetStateTarget();
	if ( pNewTrackTarget != pTrackTarget )
	{
		bUpdated = true;
		pTrackTarget = pNewTrackTarget;
	}

	return bUpdated;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::UpdateState()
{
	if ( ( pState->GetType() == IState::FORCED ) && pState->Initialize( this ) )
		return;
	if ( ( pState->GetType() == IState::TEMPORARY ) && pState->Initialize( this ) )
		return;

	pState->Terminate();
	pState = 0;
	for ( int nTemp = 0; nTemp < updatedStatesSet.size(); nTemp++ )
	{
		if ( CommandState( updatedStatesSet[nTemp] ) )
			return;
	}

	ASSERT( 0 );
	CommandState( new CStateEmpty );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::UpdateSound()
{
	int nEWatch = pWorld->GetEnemyWatchers( pActivePlayer->GetPlayer() );
	list< CPtr<NWorld::CUnit> > visible;
	int nVisibleEnemy = 0;
	pActivePlayer->GetPlayer()->GetVisible( &visible );
	for ( list< CPtr<NWorld::CUnit> >::const_iterator it = visible.begin(); it != visible.end(); ++it )
		// retail UpdateSound @0x1feb50 counts only DIPLOMACY enemies (per-unit query == DS_ENEMY) --
		// the old "any other player's unit" identity test made every visible ALLIED NPC (the base
		// commander, the 3646 mission commander) trigger combat music and the in-combat state.
		if ( !(*it)->IsDead() && !(*it)->IsUnconscious() && pActivePlayer->GetUnitDiplomacy( *it ) == NDb::DS_ENEMY )
			++nVisibleEnemy;

	// retail @0x1feb50 tail: one edge-triggered SetMusic(EMusicType) call -- the scene's music
	// machine (Sound.cpp, retail CSoundScene @0x304db0) handles combat/ambient switching, the
	// record-driven 20s fade, and the play/silence rotation. (The old SetMusic(record)/
	// FadeOutMusic pair maps onto the same machine via adapters; this is the retail shape.)
	pSoundScene->SetMusic( ( nEWatch > 0 || nVisibleEnemy > 0 ) ? NDb::MT_COMBAT : NDb::MT_AMBIENT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::WeaponReload()
{
	SActionInfo sInfo;
	GetActionInfo( UA_WEAPONRELOAD, &sInfo );
	if ( sInfo.eResult != NWorld::UCR_OK )
	{
		ShowError( this, sInfo.eResult );
		return;
	}

	vector< CPtr<IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );
	for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
		Command( unitsSet[nTemp]->GetUnit(), new NWorld::CCmdReload );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SelectSlot( int nInc )
{
	vector< CPtr<IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() != 1 )
		return;

	int nSlot = unitsSet[0]->GetUnit()->GetRPG()->GetInventoryInfo()->GetActiveSlot();
	nSlot += nInc;
	if ( nSlot >= NDb::N_SLOTS )
		nSlot = 0;
	if ( nSlot < 0 )
		nSlot = NDb::N_SLOTS - 1;

	Command( unitsSet[0]->GetUnit(), new NWorld::CCmdSetActiveItem( NDb::ESlot( nSlot ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::NewWeaponMode( NDb::EShootMode eMode )
{
	vector< CPtr<IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );
	for ( vector< CPtr<IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
		Command( (*iTemp)->GetUnit(), new NWorld::CCmdShootMode( eMode ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::NewGrenadeMode( NRPG::EGrenadeMode eMode )
{
	vector< CPtr<IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );
	for ( vector< CPtr<IUnitTracker> >::iterator iTemp = unitsSet.begin(); iTemp != unitsSet.end(); iTemp++ )
		Command( (*iTemp)->GetUnit(), new NWorld::CCmdGrenadeMode( eMode ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SelectWeaponMode( int nInc )
{
	vector< CPtr<IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );

	if ( unitsSet.size() != 1 )
		return;

	CPtr<NRPG::IInventoryItem> pItem = unitsSet[0]->GetUnit()->GetRPG()->GetInventoryInfo()->GetActive();
	if ( IsValid( pItem ) )
	{
		CDynamicCast<NRPG::IWeaponItemInfo> pWeapon(pItem);
		if (pWeapon)
		{
			int nMode = pWeapon->GetShootMode();
			for ( int nTemp = 0; nTemp < NDb::SM_MAXVALUE; nTemp++ )
			{
				nMode += nInc;
				if ( nMode < 0 )
					nMode = NDb::SM_MAXVALUE - 1;
				if ( nMode >= NDb::SM_MAXVALUE )
					nMode = 0;

				if ( pWeapon->GetDBWeapon()->shootModes[nMode] )
				{
					NewWeaponMode( NDb::EShootMode( nMode ) );
					break;
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// W4.2: SetLightMode moved to CMissionBase (retail @0x1a2640; the identical CRenderBaseInterface
// copy was consolidated into the same base body).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::TraceCursor()
{
	// retail TraceCursor @0x1fec80: the tile ray is ALWAYS re-cast against the LIVE camera every ready
	// frame (no camera-still gate here) -- that blanket early-return froze rTraceRay/sTraceTile at the
	// pre-move values, so a click while the camera eased pathfound against a stale/off-map tile ->
	// "Path not found". The camera-still counter gates ONLY the hovered OBJECT (see the tail), matching
	// retail GetStateTarget @0x1fbd40 (which suppresses sTraceResult.bObjectSet while nFrames <= 3).
	// W4.2: the result now lives in the retail NWorld::STraceResult record (CMission tag 21).
	CTransformStack sTS;
	GetCamera()->GetTransform( &sTS, GetScene()->GetScreenRect() );   // retail @0x1fec80 traces against the SELECTED camera (vtbl+0xb8)
	MakeProjectiveRay( &rTraceRay.ptDir, &rTraceRay.ptOrigin, &sTS, GetScene()->GetScreenRect(), GetCursor()->GetPos() );
	sTraceResult.bTileSet = NWorld::TraceTile( GetWorld(), rTraceRay, &sTraceResult.sTile, GetScene()->GetCutFloor() );

	list< CPtr<NWorld::CUnit> > visibleList;
	GetActivePlayer()->GetPlayer()->GetVisible( &visibleList );

	CPtr<CObjectBase> pTraceObject;
	vector<CObjectBase*> objectsSet;
	NWorld::TraceObjects( GetWorld(), rTraceRay, &objectsSet, GetScene()->GetCutFloor() );
	if ( !objectsSet.empty() )
	{
		for ( vector<CObjectBase*>::iterator iTemp = objectsSet.begin(); iTemp != objectsSet.end(); iTemp++ )
		{
			NWorld::CUnit *pTempUnit = dynamic_cast<NWorld::CUnit*>( *iTemp );
			if ( pTempUnit )
			{
				if ( find( visibleList.begin(), visibleList.end(), pTempUnit ) != visibleList.end() )
				{
					pTraceObject = *iTemp;
					break;
				}
			}

			CDynamicCast<NWorld::IObject> pTempObject( *iTemp );
			if ( pTempObject && pTempObject->IsTargetable() )
			{
				pTraceObject = *iTemp;
				break;
			}
			CDynamicCast<NWorld::IItem> pTempItem(*iTemp);
			if ( pTempItem )
			{
				pTraceObject = *iTemp;
				break;
			}

			// retail NWorld::Trace @0x37d760: a heard-not-seen unit's noise marker (CDMesh) is a
			// valid pick. News gate (retail's sound-interval filter): skip when the heard unit is
			// one of MINE or is ALREADY VISIBLE, and only accept a marker present in the player's
			// audible sound set (GetSounds). CRITICAL: the trace object stays THE MARKER (retail
			// pObject = the IAISound) -- every state except CStateAttack CUnit-casts and bails, so
			// no name tooltip / carry-body / friend state can spoil the hidden unit's identity.
			// INTENTIONAL DIVERGENCE from retail: retail targets a heard-not-seen unit ONLY through
			// its ear icon (CSoundIcon CActionDecorator hover -> SetStateTarget); this world-trace
			// pick of the silhouette CDMesh itself is an ADDITION so the model is hover-targetable
			// too. Keep both paths (the ear icon path lives in iMissionUI.cpp CSoundIcon).
			NWorld::CUnit *pHeardUnit = dynamic_cast<NWorld::CUnit*>( NWorld::GetDMeshUnit( *iTemp ) );
			if ( pHeardUnit )
			{
				NWorld::IPlayer::CUnitSet myUnits;
				GetActivePlayer()->GetPlayer()->GetUnits( &myUnits );
				vector<NWorld::IVisObj*> soundsList;
				GetActivePlayer()->GetPlayer()->GetSounds( &soundsList );
				if ( find( myUnits.begin(), myUnits.end(), pHeardUnit ) == myUnits.end()
					&& find( visibleList.begin(), visibleList.end(), pHeardUnit ) == visibleList.end()
					&& find( soundsList.begin(), soundsList.end(), CDynamicCast<NWorld::IVisObj>( *iTemp ) ) != soundsList.end() )
				{
					pTraceObject = *iTemp;
					break;
				}
			}
		}
	}

	if ( !IsValid( pTraceObject ) )
	{
		NAI::SUnitPosition sPosition;
		sPosition.pos = sTraceResult.sTile;
		sPosition.bRun = false;
		NWorld::CUnit *pTempUnit = GetWorld()->GetUnitInTile( sPosition );
		if ( find( visibleList.begin(), visibleList.end(), pTempUnit ) != visibleList.end() )
			pTraceObject = pTempUnit;
	}

	// retail: suppress the hovered OBJECT while the camera is still moving (a moving camera must not
	// snap-target a unit under the transiently-passing cursor), but leave the tile/point trace live so
	// a click resolves to the correct ground tile. Do this BEFORE the object snaps the tile below.
	if ( nFramesSameCameraPosition < 4 )
		pTraceObject = 0;

	if ( pTraceObject != 0 )
	{
		NWorld::CUnit *pTempUnit = dynamic_cast<NWorld::CUnit*>( pTraceObject.GetPtr() );
		if ( pTempUnit )
		{
			sTraceResult.bTileSet = true;
			sTraceResult.sTile = pTempUnit->GetPosition().pos;
		}
	}

	sTraceResult.pObject = pTraceObject;
	sTraceResult.bObjectSet = IsValid( pTraceObject );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::UnitCollectAP( NWorld::ECollectSnipeAP eAP )
{
	EUnitAction eAction;
	switch( eAP )
	{
	case NWorld::CSAP_1AP:
		eAction = UA_COLLECTAP_1AP;
		break;
	case NWorld::CSAP_10AP:
		eAction = UA_COLLECTAP_10AP;
		break;
	case NWorld::CSAP_MAX:
		eAction = UA_COLLECTAP_MAX;
		break;
	case NWorld::CSAP_ALL:
		eAction = UA_COLLECTAP_ALL;
		break;
	default:
		ASSERT( 0 );
		return;
	}

	SActionInfo sAction;
	GetActionInfo( eAction, &sAction );
	if ( sAction.eResult != NWorld::UCR_OK )
	{
		ShowError( this, sAction.eResult );
		return;
	}

	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	GetSelectedUnits( &unitsSet );
	if ( unitsSet.size() == 1 )
		Command( unitsSet.front()->GetUnit(), new NWorld::CCmdCollectSnipeAP( eAP ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::ExecWorldCommands()
{
	while( !IsValid( pCmdExec ) )
	{
		CPtr<NWorld::CUICmd> pCmd = pWorld->GetUICommand();
		if ( !IsValid( pCmd ) )
			return;
		CDynamicCast<NWorld::CUICmdPartFinished> pPartFinished(pCmd);
		if (pPartFinished)
		{
			bWaitForPartFinished = false;
		}
		// LUA convergence PART B: the script Pause() command toggles the mission pause
		// (retail @0x1fd8c0 dispatch: PartFinished -> Pause -> BeginSequence ...).
		else if ( CDynamicCast<NWorld::CUICmdPause>( pCmd ) )
		{
			PauseGame( !IsGamePaused() );
		}
		// CameraLock(bLock) -> retail ICamera vtbl+0x74 = CBaseCamera::FreezeCamera @0xcffc0 (vftable
		// dump @0x4b529c; +0x70 is the separate scroll-lock Lock @0xcffa0 -- the round-4 SetLock
		// routing was ONE SLOT OFF). FreezeCamera is a bare refcount, NO pose pin: CCamera::Update
		// bails before the movement/approach tail while frozen (SetPlacement stays ungated, so the
		// HQ per-room CameraSet still lands and HOLDS), and retail's SetCutFloor no-ops while frozen
		// -- mirrored onto the scene: THAT is the base's one-floor lock (EBase OnEnterZone calls
		// CameraLock() once and never unlocks; EFirst doesn't, so its two floors switch freely).
		else if ( CDynamicCast<NWorld::CUICmdLockCamera>( pCmd ) )
		{
			bool bLock = CDynamicCast<NWorld::CUICmdLockCamera>( pCmd )->bLock;
			// retail CMissionBase::ExecWorldCommand @0x1a30c0, CUICmdLockCamera branch @0x5a3367
			// (RTTI descriptor @0x97aaa4): the freeze targets the SELECTED camera (GetCamera vtbl+0xb8
			// @0x5a337e -> FreezeCamera vtbl+0x74 @0x5a338c) -- during a sequence the cinematic camera,
			// during normal play the active player's.
			GetCamera()->FreezeCamera( bLock );
			// ...and in TUTORIAL mode retail freezes the ACTIVE TRACKER's camera as well -- a SECOND
			// freeze the dev tree never had. @0x5a338f tests [this+0xe5] = bTutorialMode (PDB offset
			// 229, between bCanSave@228 and bCanRestart@230); @0x5a33a1 is IPlayerTracker vtbl+0x18 =
			// GetCamera, @0x5a33ac the FreezeCamera on it. Both calls push the same pCmd->bLock
			// (CUICmdLockCamera+0x10). Targeting the tracker EXPLICITLY is what makes a tutorial
			// CameraLock/Unlock pair balance even when the selector returns a different camera for the
			// lock than for the unlock.
			if ( bTutorialMode )
				pActivePlayer->GetCamera()->FreezeCamera( bLock );
			pScene->SetCutFloorLock( bLock );
		}
		// release CMissionBase::ExecWorldCommand @0x1a30c0: a camera-locator command (CUICmdUnitCamera
		// auto-focus) goes to the dedicated pExecLocator slot, NOT the general pCmdExec -- so it never stalls
		// the UI-command drain and is preempted by priority (MustReplaceCameraExecutor: higher wins; equal
		// wins unless the newcomer is priority 2). Keep the running exec unless the newcomer must replace it.
		else if ( CDynamicCast<NWorld::CUICmdCameraLocator>( pCmd ) )
		{
			NWorld::CUICmdCameraLocator *pLoc = CDynamicCast<NWorld::CUICmdCameraLocator>( pCmd );
			if ( IsValid( pExecLocator ) && !NGame::MustReplaceCameraExecutor( pExecLocator->GetPriority(), pLoc->GetPriority() ) )
				continue;                                   // keep the current focus, drop the newcomer
			if ( IsValid( pExecLocator ) )
				pExecLocator->Cancel();
			pExecLocator = NGame::CreateCameraExecutor( pLoc, this );
			// does NOT stall pCmdExec: continue draining the queue (the focus runs in its own slot)
		}
		// LUA convergence PART B: PlaySound opens a sound channel on the mission's sound scene and
		// parks the live handle in the command (retail CMissionBase::ExecWorldCommand @0x1a30c0:
		// CUICmdPlaySound -> soundsList add channel). StopSound later releases pChannel to stop it.
		// (Only the 2D path here; the 3D path -- Add3DSound, managed by the render-sound sync -- is a
		// follow-up once Play3DSound lands.)
		else if ( CDynamicCast<NWorld::CUICmdPlaySound>( pCmd ) )
		{
			NWorld::CUICmdPlaySound *pPlaySound = CDynamicCast<NWorld::CUICmdPlaySound>( pCmd );
			if ( IsValid( pPlaySound->pSound ) )
			{
				if ( pPlaySound->b3DSound )
					// NSound::CSound is opaque here (defined in Sound.cpp); it derives from CObjectBase
					// at offset 0 (single inheritance), so reinterpret to park it in the generic handle.
					pPlaySound->pChannel = reinterpret_cast<CObjectBase *>(
						GetSoundScene()->Add3DSound( pPlaySound->pSound, new NGScene::CCVec3( pPlaySound->vPos ), GetTime() ) );
				else
					pPlaySound->pChannel = GetSoundScene()->Add2DSound( pPlaySound->pSound );
			}
		}
		// LUA convergence PART B: PlayEffect spawns a particle effect at the command's position and parks the
		// live handle (retail CMissionBase::ExecWorldCommand @0x1a30c0: GetEffect + MakeTransform + render).
		// Mirrors CMission::ShowWeatherEffect; StopEffect later releases pHandle to remove it.
		else if ( CDynamicCast<NWorld::CUICmdPlayEffect>( pCmd ) )
		{
			NWorld::CUICmdPlayEffect *pPlayEffect = CDynamicCast<NWorld::CUICmdPlayEffect>( pCmd );
			if ( IsValid( pPlayEffect->pEffect ) )
			{
				SFBTransform place;
				MakeMatrix( &place, CVec3( 1, 1, 1 ), pPlayEffect->vPos, 0 );
				SRand rnd;
				pPlayEffect->pHandle = GetScene()->CreateParticles( pPlayEffect->pEffect->GetEffect( &rnd ), 0, pRender->GetTime(), place );
			}
		}
		// LUA convergence PART B: SetupAmbientLight sets the scene's ambient light from the record
		// (retail CMissionBase::ExecWorldCommand @0x1a30c0: GetLight + scene->SetAmbient; mirrors SetLightMode).
		else if ( CDynamicCast<NWorld::CUICmdSetAmbient>( pCmd ) )
		{
			NWorld::CUICmdSetAmbient *pSetAmbient = CDynamicCast<NWorld::CUICmdSetAmbient>( pCmd );
			if ( IsValid( pSetAmbient->pLight ) )
			{
				SRand rnd;
				CPtr<NDb::CAmbientLightReal> pLight = pSetAmbient->pLight->GetLight( &rnd );
				GetScene()->SetAmbient( pLight );
			}
			else
				SetLightMode( nLightMode );   // SetTimeOfDay queues a null-light cmd -> recompute the current ambient (retail: not a no-op)
		}
		// LUA convergence: SetAmbientEffect(id) sets a single, replaceable scene-wide ambient effect from the
		// record (retail CMissionBase::ExecWorldCommand @0x1a30c0 -> CGameView::SetAmbientEffect @0x1895f0). The
		// retail keeps it in a scene slot + builds a CLightAnimator from the effect's first light; the dev parks
		// the live render handle on the mission and creates the full effect at the scene origin via the proven
		// CreateParticles path (same call as PlayEffect). A null/invalid record clears it (SetAmbientEffect(-1)).
		else if ( CDynamicCast<NWorld::CUICmdSetAmbientEffect>( pCmd ) )
		{
			NWorld::CUICmdSetAmbientEffect *pSetAmbientEffect = CDynamicCast<NWorld::CUICmdSetAmbientEffect>( pCmd );
			pAmbientEffect = 0;		// release the previous ambient effect -> removes it from the scene
			if ( IsValid( pSetAmbientEffect->pEffect ) )
			{
				SFBTransform place;
				MakeMatrix( &place, CVec3( 1, 1, 1 ), CVec3( 0, 0, 0 ), 0 );
				SRand rnd;
				pAmbientEffect = GetScene()->CreateParticles( pSetAmbientEffect->pEffect->GetEffect( &rnd ), 0, pRender->GetTime(), place );
			}
		}
		// LUA convergence PART B: BeginZone(zoneName) begins the named scenario zone
		// (retail CMissionBase::ExecWorldCommand @0x1a30c0: GetZoneByName -> CICBeginMission).
		else if ( CDynamicCast<NWorld::CUICmdBeginZone>( pCmd ) )
		{
			NWorld::CUICmdBeginZone *pBeginZone = CDynamicCast<NWorld::CUICmdBeginZone>( pCmd );
			if ( !pGlobalGame->pScenarioTracker->IsScenarioAvailable() )
				csSystem << "ERROR: Scenario not available!" << endl;
			else
			{
				NScenario::CScenarioZone *pZone = pGlobalGame->pScenarioTracker->GetZoneByName( pBeginZone->szZone );
				if ( IsValid( pZone ) )
				{
					vector<string> params;
					// bEmulateChapter=true: direct mission->zone jump, run the leave-zone bookkeeping
					NMainLoop::Command( new CICBeginMission( pZone, -1, params, pGlobalGame, true ) );
				}
				else
					csSystem << "ERROR: Zone not found " << pBeginZone->szZone << " !" << endl;
			}
		}
		// LUA convergence PART B: FadeOut(BeginFade) spawns the fade desktop window; FadeIn(EndFade) tells the
		// running fade to fade back out (retail CMission::ExecWorldCommand @0x1fd8c0 BeginFade/EndFade arms).
		else if ( CDynamicCast<NWorld::CUICmdBeginFade>( pCmd ) )
		{
			NWorld::CUICmdBeginFade *pBeginFade = CDynamicCast<NWorld::CUICmdBeginFade>( pCmd );
			NUI::CMissionFadeUI *pFadeUI = new NUI::CMissionFadeUI(
				NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "fadeUI", NUI::STYLE_ENABLED ),
				this, GetDesktop(), pBeginFade->vColor, pBeginFade->sFadeTime );
			// Retail v1.1 @0x5fdd91 / v1.2 @0x5fe694: load the camera-view control before pushing the fade.
			NUI::LoadTemplate( pFadeUI, NDb::GetUIContainer( 377 ) );
			pFadeUI->ShowDesktop( pBeginFade->GetID() );
		}
		else if ( CDynamicCast<NWorld::CUICmdEndFade>( pCmd ) )
		{
			for ( list<CObj<NUI::CDesktopWindow> >::reverse_iterator iTemp = desktopWindowsList.rbegin(); iTemp != desktopWindowsList.rend(); iTemp++ )
			{
				CDynamicCast<NUI::CMissionFadeUI> pFadeUI( *iTemp );
				if ( pFadeUI )
					pFadeUI->HideDesktop( pCmd->GetID() );
			}
		}
		// LUA convergence PART B: ShowLoseDialog posts the "mission lost" menu (retail CMission::
		// ExecWorldCommand @0x1fd8c0: new CICLoseMenu(nStringID, screenshot) -> NMainLoop::Command). Thread the
		// scripted lose REASON id (the campaign's ShowLoseDialog(id), e.g. "Main Hero died!") into the dialog;
		// the squad-wipe path (~line 1004) keeps the default -1 (no reason -> just the "Game Over!" header).
		else if ( CDynamicCast<NWorld::CUICmdLoseDialog>( pCmd ) )
		{
			CDynamicCast<NWorld::CUICmdLoseDialog> pLoseCmd( pCmd );
			NMainLoop::Command( new NGame::CICLoseMenu( pLoseCmd->nRecordID ) );
		}
		// LUA convergence PART B: ShowLeaveZoneDialog posts the "leave zone" modal (retail CMission::
		// ExecWorldCommand @0x1fd8c0: new CICLeaveZoneMenu(this, title, autosave, screenshot)). The retail's
		// autosave flag + title DB-string + screenshot ids are not recovered, so the dev posts the bare
		// modal (confirm still closes it + ends the mission via CICEndMission).
		else if ( CDynamicCast<NWorld::CUICmdLeaveZoneDlg>( pCmd ) )
		{
			// retail CMission::ExecWorldCommand @0x1fd8c0 passes the prompt title (the DB string the script named
			// via ShowLeaveZoneDialog -> CUICmdLeaveZoneDlg::nRecordID = NDb::GetString(id)) so the modal renders
			// its message + labelled buttons. (Previously dropped -> the dialog showed blank/invisible.)
			NWorld::CUICmdLeaveZoneDlg *pDlg = CDynamicCast<NWorld::CUICmdLeaveZoneDlg>( pCmd );
			NMainLoop::Command( new NGame::CICLeaveZoneMenu( this, NDb::GetString( pDlg->nRecordID ), false, 0 ) );
		}
		// LUA convergence (hint machinery): SetTutorialMode(b) stores the flag on the mission (retail
		// CMissionBase::ExecWorldCommand @0x1a30c0). Its sole consumer is the ShowHint gate just below.
		else if ( CDynamicCast<NWorld::CUICmdTutorialMode>( pCmd ) )
		{
			bTutorialMode = CDynamicCast<NWorld::CUICmdTutorialMode>( pCmd )->bTutorial;
		}
		// LUA convergence: SetFirstMissionMode(b) -- close the medals/biography panels now + latch the flag
		// (retail CMission::ExecWorldCommand @0x1fd8c0: SetPanelState(0x14,0) -- retail bits MEDALS|BIOGRAPHY --
		// then bSpecialFirstMissionMode=b).
		else if ( CDynamicCast<NWorld::CUICmdFirstMissionMode>( pCmd ) )
		{
			SetPanelState( PANEL_MEDALS | PANEL_BIOGRAPHY, false );
			bSpecialFirstMissionMode = CDynamicCast<NWorld::CUICmdFirstMissionMode>( pCmd )->bMode;
		}
		// LUA convergence: EnableFeature("reenter") -- allow re-entering this zone's templates (retail
		// CMission::ExecWorldCommand sets bEnableFeatureReenter when the feature id is 0).
		else if ( CDynamicCast<NWorld::CUICmdEnableFeature>( pCmd ) )
		{
			if ( CDynamicCast<NWorld::CUICmdEnableFeature>( pCmd )->nFeature == 0 )
				bEnableFeatureReenter = true;
		}
		// LUA convergence: SetLeaveZoneMode -- block/allow leaving the zone (retail CMission::ExecWorldCommand
		// @0x1fd8c0 sets bLeaveBlockedByScript = !bMode + pBlockReason = GetString(nReason)). The flag feeds
		// CMission::CanLeaveZone @0x1fcbd0 (exit-button gate + tooltip status + blocked-click log).
		else if ( CDynamicCast<NWorld::CUICmdLeaveZoneMode>( pCmd ) )
		{
			NWorld::CUICmdLeaveZoneMode *pLeaveMode = CDynamicCast<NWorld::CUICmdLeaveZoneMode>( pCmd );
			bLeaveBlockedByScript = !pLeaveMode->bMode;
			// W4.2: store the DB string like retail (tag 33 CDBPtr<NDb::CString>), not the raw id
			if ( pLeaveMode->nReason > 0 )
				pBlockReason = NDb::GetString( pLeaveMode->nReason );
			else
				pBlockReason = 0;
		}
		// LUA convergence: PlayVideo -- play the named .seq cut-scene via the existing iIntroScreen sequence
		// player (retail CMissionBase::ExecWorldCommand @0x1a30c0 -> new CICPlaySequence(this, nID, file, false)).
		else if ( CDynamicCast<NWorld::CUICmdPlayVideo>( pCmd ) )
		{
			NWorld::CUICmdPlayVideo *pVideo = CDynamicCast<NWorld::CUICmdPlayVideo>( pCmd );
			NGame::PlayVideoSequence( this, pVideo->GetID(), pVideo->szFileName, false );
		}
		// LUA convergence (hint machinery): ShowHint(id) shows the hint modal (NGame::CICShowHint, which awards
		// the hint's XP), BUT only when the "ui_showhints" game option is on OR we're in tutorial mode (retail
		// CMissionBase::ExecWorldCommand @0x1a30c0 gate). When hints are suppressed it still releases the lua
		// WaitForUI(id) immediately by posting the interface event itself (the screen would otherwise do that).
		else if ( CDynamicCast<NWorld::CUICmdShowHint>( pCmd ) )
		{
			NWorld::CUICmdShowHint *pShowHint = CDynamicCast<NWorld::CUICmdShowHint>( pCmd );
			bool bShow = ( NGlobal::GetVar( "ui_showhints" ).GetFloat() == 1.0f ) || bTutorialMode;
			if ( bShow )
				NMainLoop::Command( new NGame::CICShowHint( this, pShowHint->GetID(), pShowHint->pHint, pGlobalGame ) );
			else
				DoEvent( new NWorld::CCmdInterfaceEvent( pShowHint->GetID() ) );
		}
		// LUA convergence: campaign OnRealExit() -> ExitToChapter() (scriptScenario.cpp:96) queues a
		// CUICmdContinueChapter. retail CMission::ExecWorldCommand @0x1fd8c0 posts a CICRealEndMission and
		// returns -- teardown is DEFERRED to NMainLoop. (Calling Terminate() inline here is unsafe:
		// ExecWorldCommands runs early in InternalStep, which keeps using players/world afterwards.) This is
		// what makes the mission exit fully script-driven; it also pre-empts the old CUICmdExecContinueChapter
		// fall-through executor.
		else if ( CDynamicCast<NWorld::CUICmdContinueChapter>( pCmd ) )
		{
			NMainLoop::Command( new CICRealEndMission( this ) );
		}
		else {
			CDynamicCast<NWorld::CUICmdBeginSequence> pBeginSequence(pCmd);
			if (pBeginSequence)
			{
				// retail CMission::ExecWorldCommand @0x1fd8c0: depth 1 clears the camera limits (saved in
				// cameraLimits, restored by the matching EndSequence); EVERY begin then pushes the current
				// camera pose onto the per-mission pose stack AND stacks a fresh movieUI letterbox (nested
				// dialog sequences fade invisibly over the outer bars) -- EndSequence hides the TOP one.
				// The retail depth-1 limits are DEFAULT limits with bMovie=TRUE (the decomp's bStack_38
				// store at +0x10 of the fresh SCameraLimits before the camera SetLimits vtbl+0x64): bMovie
				// is what makes CCamera::Update (@0xcd930) skip the terrain/approach tail so scripted poses
				// HOLD. With the widened SCameraLimits the flag now rides the limits, exactly like retail.
				// retail @0x1fd8c0: nSequence++ FIRST, so every camera access below goes through the
				// SELECTOR (@0x1a1ee0) with IsSequence() already true = the CINEMATIC camera. Depth 1
				// saves the camera's CURRENT limits (vtbl+0x60 -> this->cameraLimits, restored by the
				// matching EndSequence) before installing the fresh bMovie=TRUE defaults.
				if (nSequence++ == 0)
				{
					GetCamera()->GetLimits(&cameraLimits);
					ICamera::SCameraLimits sMovieLimits;
					sMovieLimits.bMovie = true;
					GetCamera()->SetLimits(sMovieLimits);
				}
				////
				ICamera::SCameraPos sSeqPose;
				GetCamera()->GetPlacement(&sSeqPose);
				cameraPosStack.push_back(sSeqPose);
				////
				CPtr<NUI::CMissionMovieUI> pMissionMovieUI = new NUI::CMissionMovieUI(NUI::SWindowInfo(pInterface, NUI::SPoint(0, 0), NUI::SPoint(1024, 768), "movieUI", NUI::STYLE_ENABLED), this, GetDesktop(), pBeginSequence->bSkipFadeOut);
				// retail loads container 344 ("Movie") for the letterbox; 364 is the DIALOG container --
				// using it made the bars visibly change when leaving a dialog.
				NUI::LoadTemplate(pMissionMovieUI, NDb::GetUIContainer(344));
				pMissionMovieUI->ShowDesktop(pBeginSequence->GetID());
			}
			else {
				CDynamicCast<NWorld::CUICmdEndSequence> pEndSequence(pCmd);
				if (pEndSequence)
				{
					// retail @0x1fd8c0: EndSequence hides the TOP desktop iff it is a movieUI. Camera pose
					// semantics: pop the matching BeginSequence pose -- bRestoreCamera ? SetPlacement(popped)
					// : commit the CURRENT (cutscene-end) pose as the active player's gameplay camera via
					// SetCamera, so the camera does NOT jump back after a scripted move. Depth 1->0 restores
					// the saved camera limits.
					CDynamicCast<NUI::CMissionMovieUI> pMissionMovieUI(GetDesktop());
					if (pMissionMovieUI)
					{
						// (nSequence is still >= 1 here, so GetCamera() = the cinematic camera)
						if (!cameraPosStack.empty())
						{
							if (pEndSequence->bRestoreCamera)
								GetCamera()->SetPlacement(cameraPosStack.back());
							cameraPosStack.pop_back();
						}
						if (!pEndSequence->bRestoreCamera)
						{
							// retail @0x1fd8c0: commit the cutscene-end pose INTO THE ACTIVE PLAYER'S
							// OWN camera (tracker vtbl+0x18 GetCamera -> camera vtbl+0x50 SetPlacement)
							// so gameplay resumes from the scripted view instead of jumping back.
							ICamera::SCameraPos sSeqPose;
							GetCamera()->GetPlacement(&sSeqPose);
							pActivePlayer->GetCamera()->SetPlacement(sSeqPose);
						}
						if (nSequence == 1)
						{
							// retail @0x1fd8c0: the LAST EndSequence restores the saved (bMovie=false)
							// mission limits -- the terrain/approach tail resumes from the committed pose.
							GetCamera()->SetLimits(cameraLimits);
						}
						pMissionMovieUI->SetSkipFade(pEndSequence->bSkipFade);
						pMissionMovieUI->HideDesktop(pEndSequence->GetID());
						if (nSequence > 0)
							--nSequence;
					}
					else
					{
						csSystem << CC_RED << "ERROR: unexpected CUICmdEndSequence!" << endl;
						// dev safety net (retail leaks the id here): release the wait id so lua
						// WaitForUI(EndSequence()) cannot hang on the dropped command.
						DoEvent(new NWorld::CCmdInterfaceEvent(pEndSequence->GetID()));
					}
				}
				else {
					CDynamicCast<NWorld::CUICmdPlayDialog> pDialog(pCmd);
					if (pDialog)
					{
						if (bWaitForPartFinished)
						{
							csSystem << CC_RED << "WARNING: Dialog in in WaitForPartFinished mode ignored!" << endl;
							// release the DialogPlay wait id now (the dialog UI that would do it is never created)
							if (pDialog->GetID() >= 0)
								DoEvent(new NWorld::CCmdInterfaceEvent(pDialog->GetID()));
							continue;
						}
						////
						// pass the wait id so CMissionDlgUI::EndDialog releases WaitForUI(DialogPlay(...))
						CPtr<NUI::CMissionDlgUI> pMissionDlgUI = new NUI::CMissionDlgUI(NUI::SWindowInfo(pInterface, NUI::SPoint(0, 0), NUI::SPoint(1024, 768), "dialogUI", NUI::STYLE_ENABLED), this, GetDesktop(), pDialog->szDialogCode, pDialog->units, pDialog->phrases, pDialog->GetID());
						NUI::LoadTemplate(pMissionDlgUI, NDb::GetUIContainer(364));
						pMissionDlgUI->ShowDesktop();
					}
					else {
						CDynamicCast<NWorld::CUICmdPlayAck> pAck(pCmd);
						if (pAck)
							GetDesktop()->PlayAck(pAck->phrases.front());
						else {
							CDynamicCast<NWorld::CUICmdSetFloor> pFloor(pCmd);
							if (pFloor)
								pScene->SetCutFloor(pFloor->nFloor);
							else {
								CDynamicCast<NWorld::CUICmdShowClue> pClue(pCmd);
								if (pClue)
								{
									if (IsValid(pClue->pClue))
										NMainLoop::Command(new NGame::CICShowClue(pGlobalGame, pClue->pClue));
								}
								// W5 serialization-convergence: the NGame UICmdExec wrappers are gone -- retail
								// CMission::ExecWorldCommand @0x1fd8c0 executes these three INLINE:
								// LoadTemplate -> NMainLoop::Command(new CICBeginMission(pZone, nTemplateID,
								// vector<string>(), GetRPGGame())); ShowStore -> the immediate panel open
								// (mission vtbl+0x68 == SetPanelState(PANEL_STORE|PANEL_INVENTORY, true));
								// ShowTeamMng -> NMainLoop::Command(new CICTeamMngMenu(globalPlayer, this, nID))
								// -- NB retail passes the command's wait id as the 3rd arg.
								else {
									CDynamicCast<NWorld::CUICmdLoadTemplate> pLoadTemplate(pCmd);
									if (pLoadTemplate)
										NMainLoop::Command(new CICBeginMission(pLoadTemplate->pZone,
											pLoadTemplate->nTemplateID, vector<string>(), GetRPGGame()));
									else {
										CDynamicCast<NWorld::CUICmdShowStore> pShowStore(pCmd);
										if (pShowStore)
										{
											NWorld::CPlayer *pPlayer = dynamic_cast<NWorld::CPlayer*>( GetActivePlayer()->GetPlayer() );
											if ( pPlayer )
												pPlayer->UpdateStore();
											SetPanelState(PANEL_STORE | PANEL_INVENTORY, true);
										}
										else {
											CDynamicCast<NWorld::CUICmdShowTeamMng> pShowTeamMng(pCmd);
											if (pShowTeamMng)
												NMainLoop::Command(new CICTeamMngMenu(
													GetActivePlayer()->GetGlobalPlayer(), this, pShowTeamMng->GetID()));
											else
												pCmdExec = GetDesktop()->CreateExecutor(pCmd);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::SaveWorld( const string &szFile )
{
	NMainLoop::CSaveManager *pSaveManager = NMainLoop::GetSaveManager();

	pSaveManager->PrepareSlot( NMainLoop::S_SLOT_ACTIVE );

//#ifndef _DEBUG
	try
//#endif
	{
		const string szPath = pSaveManager->GetSlotFilePath( NMainLoop::S_SLOT_ACTIVE, szFile );
		// retail @0x1a4040: clear + delete any stale zone save before writing (a read-only or
		// leftover file must not survive and be loaded as the zone's world on the next entry)
		::SetFileAttributesA( szPath.c_str(), FILE_ATTRIBUTE_NORMAL );
		::DeleteFileA( szPath.c_str() );

		CFileStream sFile;
		sFile.OpenWrite( szPath.c_str() );

		CStructureSaver sSaver( sFile, CStructureSaver::WRITE );
		sSaver.Add( 2, &pWorld );
		SerializeShared( &sSaver );
	}
//#ifndef _DEBUG
	catch(...)
	{
		csSystem << "WARNING: Can't save zone" << szFile << endl;
	}
//#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::LoadWorld( const string &szFile )
{
	NMainLoop::CSaveManager *pSaveManager = NMainLoop::GetSaveManager();

	pSaveManager->PrepareSlot( NMainLoop::S_SLOT_ACTIVE );

//#ifndef _DEBUG
	try
//#endif
	{
		CFileStream sFile;
		sFile.OpenRead( pSaveManager->GetSlotFilePath( NMainLoop::S_SLOT_ACTIVE, szFile ).c_str() );

		CSharedHolder hold;
		CStructureSaver sSaver( sFile, CStructureSaver::READ );
		sSaver.Add( 2, &pWorld );
		SerializeShared( &sSaver );
	}
//#ifndef _DEBUG
	catch(...)
	{
		csSystem << "WARNING: Can't load zone" << szFile << endl;
	}
//#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::TestIntersection()
{
	CRay r;
	// CRAP - have to make special function in pGame for this
	CTransformStack sTS = GetCameraTransform();
	MakeProjectiveRay( &r.ptDir, &r.ptOrigin, &sTS, pScene->GetScreenRect(), pCursor->GetPos() );
	r.ptDir *= 100;
	float fT;
	CVec3 vNormal, vColor;
	if ( pScene->TraceScene( r, &fT, &vNormal, &vColor ) )
	{
		CVec3 vPoint = r.Get( fT );
		CPtr<CMemObject> pModel = new CMemObject;
		pModel->CreateSphere( vPoint, 0.06f );
		pIntersectHolder = pScene->CreateMesh( pModel, CVec4(vColor,1), 0 );
		vector<CVec3> points;
		points.push_back( vPoint );
		points.push_back( vPoint + vNormal * 0.2f );
		pIntersectLineHolder = pScene->CreatePolyline( points, CVec3(1,1,1) );
	}
	else
	{
		pIntersectHolder = 0;
		pIntersectLineHolder = 0;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::ShowWeatherEffect( int nID )
{
	ICamera::SCameraPos cameraPos;
	GetCamera()->GetPlacement( &cameraPos );
	SFBTransform pos;
	MakeMatrix( &pos, CVec3(1,1,1), cameraPos.ptAnchor, 0 );
	NDb::CTEffect *pTEffect = NDb::GetTEffect( nID );
	SRand rnd;
	// W4.2: retail ShowWeatherEffect @0x201c60 keeps a VECTOR (tag 26): clear the previous test
	// effects, then push the one new handle.
	testWeatherEffect.clear();
	testWeatherEffect.push_back( GetScene()->CreateParticles( pTEffect->GetEffect( &rnd ), 0, pRender->GetTime(), pos ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::ShowVisibleFrom()
{
	if ( !pVisibleTracker )
	{
		CRay r;
		CTransformStack sTS = GetCameraTransform();
		MakeProjectiveRay( &r.ptDir, &r.ptOrigin, &sTS, pScene->GetScreenRect(), pCursor->GetPos() );
		r.ptDir *= 100;
		float fT;
		CVec3 vNormal, vColor;
		if ( pScene->TraceScene( r, &fT, &vNormal, &vColor ) )
		{
			static int nMask = 1;
			pVisibleTracker = new CVisibleTracker( CVisibleTracker::VISIBLE_FROM, 7/*nMask*/, r.Get(fT), 5 );
			pVisibleTracker->Update( this );
			nMask <<= 1;
			if ( nMask == 8 )
				nMask = 1;
		}
	}
	else
		pVisibleTracker = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::ClickOfDeath()
{
	pWorld->ClickOfDeath( GetTraceRay(), pScene->GetCutFloor() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::Explosion()
{
	pWorld->MakeExplosion( GetTraceRay(), pScene->GetCutFloor() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMission::ErasePartUnderCursor()
{
	CRay r;
	// CRAP - have to make special function in pGame for this
	CTransformStack sTS = GetCameraTransform();
	MakeProjectiveRay( &r.ptDir, &r.ptOrigin, &sTS, pScene->GetScreenRect(), pCursor->GetPos() );
	r.ptDir *= 100;
	float fT;
	CVec3 vNormal, vColor;
	CObjectBase *pTarget;
	if ( pScene->TraceScene( r, &fT, &vNormal, &vColor, NGScene::SPS_ALL, &pTarget ) )
		CMObj<CObjectBase> pKillerLoop( pTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVisibleTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVisibleTracker::Update( IMission* pMission )
{
	CVec3 vColors[4] = { CVec3( 1, 0, 0 ), CVec3( 0, 1, 0 ), CVec3( 1, 1, 1 ), CVec3( 0, 1, 0 ) };

	bUpdated = false;

	vector< CPtr<IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	if ( unitsSet != selectedUnits )
	{
		selectedUnits = unitsSet;
		bUpdated = true;
	}

	if ( !bUpdated )
		return;

	nodesSet.clear();
	for ( int nTemp = 0; nTemp < selectedUnits.size(); nTemp++ )
	{
		vector<NRPG::SVisibilitySpot> spotsSet;
		if ( type == VISIBLE )
			pMission->GetWorld()->GetGame()->GetVisibilityArea( &spotsSet, unitsSet[nTemp]->GetUnit() );
		else
			pMission->GetWorld()->GetGame()->GetVisibleFromArea( &spotsSet, unitsSet[nTemp]->GetUnit(), vFrom, fRadius, nMask );

		for ( int nSpot = 0; nSpot < spotsSet.size(); nSpot++ )
		{
			CPtr<CMemObject> pModelBuilder = new CMemObject;
			pModelBuilder->CreateSphere( spotsSet[nSpot].ptPos, 0.06f, 0 );
			CVec3 cr = vColors[spotsSet[nSpot].nCanSee];
			nodesSet.push_back( pMission->GetScene()->CreateMesh( pModelBuilder, CVec4( cr, 1 ), 0 ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CViewBuildingScheme
////////////////////////////////////////////////////////////////////////////////////////////////////
CViewBuildingScheme::CViewBuildingScheme( CSyncSrc<NWorld::IVisObj> *pSrc, NGScene::IGameView *_pScene, vector<CObj<CObjectBase> > *_pRes ):
	CSyncDst<NWorld::IVisObj>(pSrc), pScene(_pScene), pRes(_pRes)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CViewBuildingScheme::AddBuilding( const SMapBuilding &info )
{
	pRes->push_back( ViewBuildingSchema( pScene, info.pSWMap, info.pVariant->GetRecordID(), info.pGrid, info.pPos->pos ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUpdateBuildingStability
////////////////////////////////////////////////////////////////////////////////////////////////////
CUpdateBuildingStability::CUpdateBuildingStability( CSyncSrc<NWorld::IVisObj> *pSrc ):
	CSyncDst<NWorld::IVisObj>(pSrc)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUpdateBuildingStability::AddBuilding( const SMapBuilding &info )
{
	NBuilding::UpdateBuildingStability( info.pVariant->GetRecordID(), info.pGrid, info.pSWMap );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// COMMANDS
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarCheatSeeAll( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	CObjectBase *pBase = (CObjectBase*)pContext;
	IMission *pGame = dynamic_cast<IMission*>( pBase );
	ASSERT( pGame != 0 );

	if ( sValue.GetFloat() > 0 )
		pGame->Command( new NWorld::CCmdCheat( NRPG::CHEAT_SEEALL, true ) );
	else
		pGame->Command( new NWorld::CCmdCheat( NRPG::CHEAT_SEEALL, false ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarCheatShowAll( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	bShowAllCheat = false;
	if ( sValue.GetFloat() != 0 )
		bShowAllCheat = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarCheatGodMode( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	CObjectBase *pBase = (CObjectBase*)pContext;
	IMission *pGame = dynamic_cast<IMission*>( pBase );
	ASSERT( pGame != 0 );

	if ( sValue.GetFloat() > 0 )
		pGame->Command( new NWorld::CCmdCheat( NRPG::CHEAT_GODMODE, true ) );
	else
		pGame->Command( new NWorld::CCmdCheat( NRPG::CHEAT_GODMODE, false ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarCheatAP( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	CObjectBase *pBase = (CObjectBase*)pContext;
	IMission *pGame = dynamic_cast<IMission*>( pBase );
	ASSERT( pGame != 0 );
	//
	if ( sValue.GetFloat() > 0 )
		pGame->Command( new NWorld::CCmdCheat( NRPG::CHEAT_AP, true ) );
	else
		pGame->Command( new NWorld::CCmdCheat( NRPG::CHEAT_AP, false ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarCheatTeleport( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	CObjectBase *pBase = (CObjectBase*)pContext;
	IMission *pGame = dynamic_cast<IMission*>( pBase );
	ASSERT( pGame != 0 );

	if ( sValue.GetFloat() > 0 )
		pGame->Command( new NWorld::CCmdCheat( NRPG::CHEAT_TELEPORT, true ) );
	else
		pGame->Command( new NWorld::CCmdCheat( NRPG::CHEAT_TELEPORT, false ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandTypeCameraParams( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	CObjectBase *pBase = (CObjectBase*)pContext;
	IMission *pGame = dynamic_cast<IMission*>( pBase );
	ASSERT( pGame != 0 );

	ICamera::SCameraPos pos;
	pGame->GetCamera()->GetPlacement( &pos );
	csSystem << "current camera params:" << endl;
	csSystem << "pitch = " << pos.fPitch << "  yaw = " << pos.fYaw << endl;
	csSystem << "rod = " << pos.fRod << endl;
	csSystem << "anchor = ( " << pos.ptAnchor.x << ", " << pos.ptAnchor.y << ", " << pos.ptAnchor.z << " )" << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::FillLimits @0x20abb0 -- the shared FOV+min/max-rod/pitch fill the release factored out of
// CommandSetDefaultCameraLimits (and reused by softlimits). Writes the module FOV global + the limits'
// first four floats from paramsSet[0..4].
static void FillLimits( ICamera::SCameraLimits &limits, const vector<wstring> &paramsSet )
{
	fDefaultCameraFOV = atof( NStr::ToAscii( paramsSet[0] ).data() );
	limits.fMinRod = atof( NStr::ToAscii( paramsSet[1] ).data() );
	limits.fMaxRod = atof( NStr::ToAscii( paramsSet[2] ).data() );
	limits.fMinPitch = atof( NStr::ToAscii( paramsSet[3] ).data() );
	limits.fMaxPitch = atof( NStr::ToAscii( paramsSet[4] ).data() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetDefaultCameraLimits( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() != 5 )
	{
		csSystem << "Usage: mission_camera_limits FOV minRod maxRod minPitch maxPitch" << endl;
		return;
	}
	FillLimits( defaultCameraLimits, paramsSet );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetDefaultCameraSoftLimits( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() != 5 )
	{
		csSystem << "Usage: mission_camera_softlimits FOV minRod maxRod minPitch maxPitch" << endl;
		return;
	}
	FillLimits( defaultCameraSoftLimits, paramsSet );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetDefaultCameraAttenuation( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() != 1 )
	{
		csSystem << "Usage: mission_camera_attenuation <attenuation>" << endl;
		csSystem << "attenuation=" << defaultCameraLimits.fAttenuation << endl;
		return;
	}
	defaultCameraLimits.fAttenuation = atof( NStr::ToAscii( paramsSet[0] ).data() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetDefaultCameraSoftAttenuation( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() != 1 )
	{
		csSystem << "Usage: mission_camera_softattenuation <attenuation>" << endl;
		csSystem << "softattenuation=" << defaultCameraSoftLimits.fAttenuation << endl;
		return;
	}
	defaultCameraSoftLimits.fAttenuation = atof( NStr::ToAscii( paramsSet[0] ).data() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetDefaultCameraMinHeight( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() != 1 )
	{
		csSystem << "Usage: mission_camera_minheight <height>" << endl;
		csSystem << "min height=" << defaultCameraLimits.fMinHeight << endl;
		return;
	}
	defaultCameraLimits.fMinHeight = atof( NStr::ToAscii( paramsSet[0] ).data() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetDefaultCameraScrollAcceleration( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() != 1 )
	{
		csSystem << "Usage: mission_camera_scroll_acceleration <acceleration>" << endl;
		csSystem << "scroll acceleration=" << defaultCameraLimits.fScrollZoomAcceleration << endl;
		return;
	}
	defaultCameraLimits.fScrollZoomAcceleration = atof( NStr::ToAscii( paramsSet[0] ).data() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetDefaultCameraScrollSpeed( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() != 1 )
	{
		csSystem << "Usage: mission_camera_scroll_speed <speed>" << endl;
		csSystem << "scroll speed=" << defaultCameraLimits.fScrollSpeed << endl;
		return;
	}
	defaultCameraLimits.fScrollSpeed = atof( NStr::ToAscii( paramsSet[0] ).data() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetDefaultCameraZoomAcceleration( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() != 1 )
	{
		csSystem << "Usage: mission_camera_zoom_acceleration <acceleration>" << endl;
		csSystem << "zoom acceleration=" << defaultCameraLimits.fZoomAcceleration << endl;
		return;
	}
	defaultCameraLimits.fZoomAcceleration = atof( NStr::ToAscii( paramsSet[0] ).data() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetDefaultCameraZoomSpeed( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() != 1 )
	{
		csSystem << "Usage: mission_camera_zoom_speed <speed>" << endl;
		csSystem << "zoom speed=" << defaultCameraLimits.fZoomSpeed << endl;
		return;
	}
	defaultCameraLimits.fZoomSpeed = atof( NStr::ToAscii( paramsSet[0] ).data() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetDefaultCameraYawSpeed( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() != 1 )
	{
		csSystem << "Usage: mission_camera_yaw_speed <speed>" << endl;
		csSystem << "yaw speed=" << defaultCameraLimits.fYawSpeed << endl;
		return;
	}
	defaultCameraLimits.fYawSpeed = atof( NStr::ToAscii( paramsSet[0] ).data() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetDefaultCameraYawAcceleration( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() != 1 )
	{
		csSystem << "Usage: mission_camera_yaw_acceleration <acceleration>" << endl;
		csSystem << "yaw acceleration=" << defaultCameraLimits.fYawZoomAcceleration << endl;
		return;
	}
	defaultCameraLimits.fYawZoomAcceleration = atof( NStr::ToAscii( paramsSet[0] ).data() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetDefaultCameraPitchSpeed( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() != 1 )
	{
		csSystem << "Usage: mission_camera_pitch_speed <speed>" << endl;
		csSystem << "pitch speed=" << defaultCameraLimits.fPitchSpeed << endl;
		return;
	}
	defaultCameraLimits.fPitchSpeed = atof( NStr::ToAscii( paramsSet[0] ).data() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Console commands
////////////////////////////////////////////////////////////////////////////////////////////////////
static NRPG::CGlobalPlayer* CreateGlobalPlayer( const string &szID, const vector<wstring> &paramsSet, int *pNumber )
{
	int &n = *pNumber;
	if ( n >= paramsSet.size() )
		return 0;
	vector<int> pers;
	while ( n < paramsSet.size() )
	{
		int nPers = wcstol( paramsSet[n].data(), 0, 10 );
		if ( paramsSet[n] == L"vs" || paramsSet[n] == L"with" )
			break;
		++n;
		pers.push_back( nPers );
	}
	if ( pers.empty() )
		return 0;
	return NRPG::CreateGlobalPlayer( pers );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AnalyzeMapStartParams( const string &szID, NRPG::CGlobalGame *pGlobalGame, const vector<wstring> &paramsSet, 
	vector<string> *pTemplParams )
{
	int nParam = 1;
	for(;;)
	{
		CPtr<NRPG::CGlobalPlayer> pGlobalPlayer = CreateGlobalPlayer( szID, paramsSet, &nParam );
		if ( IsValid( pGlobalPlayer ) )
			pGlobalGame->players.push_back( pGlobalPlayer.GetPtr() );
		if ( nParam == paramsSet.size() )
			break;
		if ( paramsSet[ nParam ] == L"vs" )
		{
			++nParam;
			continue;
		}
		if ( paramsSet[ nParam ] == L"with" )
		{
			for ( int k = nParam + 1; k < paramsSet.size(); ++k )
				pTemplParams->push_back( NStr::ToAscii( paramsSet[k] ) );
			break;
		}
	}
	if ( pGlobalGame->players.empty() )
		pGlobalGame->players.push_back( NRPG::CreateGlobalPlayer() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandStartMission( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() == 0 )
	{
		csSystem << "usage: #zone [PersID [PersID]] [vs [PersID [PersID]]] [with [paramNames]]" << endl;
		return;
	}

	int nTemp = wcstol( paramsSet.front().data(), 0, 10 );

	vector<string> templParams;
	CPtr<NRPG::CGlobalGame> pGlobalGame = NRPG::CreateGlobalGame();
	AnalyzeMapStartParams( szID, pGlobalGame, paramsSet, &templParams );
	//
	csSystem << CC_BLUE << "Loading world ( variant " << nTemp << " ) ";
	if ( templParams.size() )
	{
		csSystem << "with ";
		for ( int k = 0; k < templParams.size(); ++k )
			csSystem << templParams[k] << ", ";
	}
	csSystem << "..." << endl;
	//
	NMainLoop::Command( new CICBeginMission( -1, nTemp, templParams, pGlobalGame ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandStartMissionByTemplateID( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() == 0 )
	{
		csSystem << "usage: #zone [PersID [PersID]] [vs [PersID [PersID]]]" << endl;
		return;
	}

	int nTemp = wcstol( paramsSet.front().data(), 0, 10 );

	vector<string> templParams;
	CPtr<NRPG::CGlobalGame> pGlobalGame = NRPG::CreateGlobalGame();
	AnalyzeMapStartParams( szID, pGlobalGame, paramsSet, &templParams );
	//
	csSystem << CC_BLUE << "Loading world ( template " << nTemp << " ) ";
	if ( templParams.size() )
	{
		csSystem << "with ";
			for ( int k = 0; k < templParams.size(); ++k )
				csSystem << templParams[k] << ", ";
	}
	csSystem << "..." << endl;
	//
	NMainLoop::Command( new CICBeginMission( nTemp, -1, templParams, pGlobalGame ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSurrender( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	CObjectBase *pBase = (CObjectBase*)pContext;
	IMission *pGame = dynamic_cast<IMission*>( pBase );
	ASSERT( pGame != 0 );

	NMainLoop::Command( new CICEndMission( pGame ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetXPLevel( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() < 1 )
		return;
	//
	CObjectBase *pObject = (CObjectBase *)pContext;
	CDynamicCast<CMission> pMission(pObject);
	if (pMission)
	{
		CPtr<NWorld::IWorld> pWorld = pMission->GetWorld();
		CPtr<IPlayerTracker> pTracker = pMission->GetActivePlayer();
		if ( !IsValid( pTracker ) )
			return;
		//
		vector< CPtr<NWorld::CUnit> > units;
		CPtr<NWorld::IPlayer> pActivePlayer = pTracker->GetPlayer();
		if ( paramsSet.size() == 1 )
			pActivePlayer->GetUnits( &units );
		else if ( paramsSet.size() == 2 )
		{
			if ( paramsSet[0] == L"ally" )
				pActivePlayer->GetUnits( &units );
			else if ( paramsSet[0] == L"enemy" )
			{
				vector< CPtr<NWorld::CUnit> > tmpUnits;
				pWorld->GetAllUnits( &tmpUnits );
				for ( vector< CPtr<NWorld::CUnit> >::iterator i = tmpUnits.begin(); 
					i != tmpUnits.end(); ++i )
				{
					if ( (*i)->GetPlayer() != pActivePlayer )
						units.push_back( *i );
				}
			}
		}
		//
		int nLevel = wcstol( paramsSet[	paramsSet.size() - 1 ].c_str(), 0, 10 );
		for ( vector< CPtr<NWorld::CUnit> >::iterator i = units.begin(); i != units.end(); ++i )
			(*i)->GetRPG()->GetRPGUnit()->SetXPLevel( nLevel );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSummonUnit( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() < 1 )
		return;
	//
	CObjectBase *pObject = (CObjectBase *)pContext;
	CDynamicCast<CMission> pMission(pObject);
	if (pMission)
	{
		// retail @0x20b5f0: bad/unknown pers ID -> console error, no spawn (unguarded CreateMerc(0) crashed)
		CPtr<NDb::CRPGPers> pPers = NDb::GetPers( _wtol( paramsSet[0].c_str() ) );
		if ( !IsValid( pPers ) )
		{
			csSystem << CC_RED << "ERROR: Invalid ID" << endl;
			return;
		}
		pMission->GetActivePlayer()->AddUnit( NRPG::CreateMerc( pPers ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandUnsummonUnit( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() < 1 )
		return;
	//
	int nTemp = _wtol( paramsSet[0].c_str() );
	//
	CObjectBase *pObject = (CObjectBase *)pContext;
	CDynamicCast<CMission> pMission(pObject);
	if (pMission)
	{
		vector< CPtr<NGame::IUnitTracker> > unitsSet;
		pMission->GetUnits( &unitsSet );

		if ( nTemp < unitsSet.size() )
			pMission->GetActivePlayer()->RemoveUnit( unitsSet[nTemp] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandGetItem( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() < 1 )
		return;
	//
	int nID = _wtol( paramsSet[0].c_str() );
	//
	CObjectBase *pBase = (CObjectBase*)pContext;
	IMission *pMission = dynamic_cast<IMission*>( pBase );
	ASSERT( pMission != 0 );

	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	for ( int nTemp = 0; nTemp < unitsSet.size(); nTemp++ )
		pMission->Command( unitsSet[nTemp]->GetUnit(), new NWorld::CCmdCreateInventoryItem( NDb::GetRPGItem( nID ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICBeginMission
////////////////////////////////////////////////////////////////////////////////////////////////////
CICBeginMission::CICBeginMission( int _nTemplateID, int _nVariantID, const vector<string> &_params, NRPG::CGlobalGame *_pGlobalGame, NDb::CUITexture *_pPWLImage ):
	nTemplateID( _nTemplateID ), nVariantID( _nVariantID ), pGlobalGame( _pGlobalGame ), params(_params), pPWLImage( _pPWLImage )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CICBeginMission::CICBeginMission( NScenario::CScenarioZone *_pZone,
		int _nTemplateID, const vector<string> &_params, NRPG::CGlobalGame *_pGlobalGame, bool _bEmulateChapter, NDb::CUITexture *_pPWLImage ):
	pZone( _pZone ), nTemplateID( _nTemplateID ), nVariantID( -1 ), pGlobalGame( _pGlobalGame ), params(_params),
	bEmulateChapter( _bEmulateChapter ), pPWLImage( _pPWLImage )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICBeginMission::Exec()
{
	// retail NGame::CICBeginMission::Exec @0x20ba00 -- per-mission splash before the world load. The splash
	// is the zone's PreWorldLoad image (CDBScenarioZone::pPWLImage, release [pDBZone+0x48]); a null/dead image
	// makes SetLoadingImage fall back to the default texture 0x373. Then paint the first loading frame at 0%
	// (disasm: xor ecx,ecx -> ShowLoadingScreen(0)). Replaces the placeholder ShowLogo() (a no-op in retail).
	// retail @0x20ba00 3-way pick: (1) the zone's own PWL image [pDBZone+0x48]; else (2) this->pPWLImage
	// [+0x30] the chapter/caller-supplied splash; else (3) SetLoadingImage(NULL) falls back to 0x373.
	NDb::CUITexture *pSplash = 0;
	if ( IsValid( pZone ) && IsValid( pZone->GetDBZone() ) )
		pSplash = pZone->GetDBZone()->pPWLImage;
	if ( !IsValid( pSplash ) && IsValid( pPWLImage ) )
		pSplash = pPWLImage;
	SetLoadingImage( pSplash );
	ShowLoadingScreen( 0 );

	// retail @0x20ba00 bEmulateChapter arm: a direct mission->zone transition (script BeginZone)
	// runs the leave-zone bookkeeping the chapter map would have done. The load-bearing call is
	// UpdateScenarioOnLeaveZone: it clears every merc's deployData.pCorpse, so a CARRIED BODY does
	// not re-materialize in the next zone (retail's InitPlayerCorpseCarrying on AddPlayer would
	// otherwise re-spawn it -- e.g. the carried commander duplicating his base instance).
	if ( bEmulateChapter && IsValid( pGlobalGame ) )
	{
		pGlobalGame->HealOnLeaveZone();
		pGlobalGame->UpdateScenarioOnLeaveZone();
	}

	// Retail 1.1 0x60bb3d / 1.2 0x60c28d: scenario-zone parameters
	// replace the caller's parameters (vector assignment, not append). This
	// carries the authored TimeOfDay into world creation and light selection.
	vector<string> missionParams( params );
	if ( IsValid( pZone ) && pZone->GetDBZone() )
		missionParams = pZone->GetDBZone()->vszParams;
	CMission *pRes = new CMission();
	if ( pRes->Initialize( nTemplateID, nVariantID, pZone, missionParams, pGlobalGame, pPWLImage ) )
		SetInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICEndMission
////////////////////////////////////////////////////////////////////////////////////////////////////
CICEndMission::CICEndMission( IMission *_pMission ):
	pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICEndMission::Exec()
{
	// retail CICEndMission::Exec @0x20b360: fire ONLY the engine->script hook OnRealExit(), QUEUED through the
	// mission command path (vtbl+0x2c). The campaign OnRealExit() -> ExitToChapter() now drives the actual
	// teardown+transition: ExitToChapter queues a CUICmdContinueChapter, ExecWorldCommands posts a
	// CICRealEndMission, and THAT does Terminate()+transition at the safe NMainLoop level. Exit is fully
	// script-driven (no Terminate / no transition here). The mission stays alive and ticking after this returns
	// (the leave-zone modal was popped first via CICExitModal), so the queued OnRealExit drains normally.
	pMission->DoEvent( new NWorld::CCmdCallScriptFunction( "OnRealExit", "" ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NGame::CICRealEndMission::Exec @0x20a850 -- the deferred, NMainLoop-level teardown queued by the
// CUICmdContinueChapter handler. Runs between frames (iMain.cpp ProcessInterfaceCmds), so RemovePlayer +
// SaveWorld are safe; the CICContinueChapter/Global it posts is drained in the same batch -> SetInterface
// swaps the mission out before it is ever Step()d again. No mid-Segment teardown, no double-transition.
void CICRealEndMission::Exec()
{
	pMission->Terminate();					// RemovePlayer(...) + SaveWorld(...), exactly once

	NRPG::CGlobalGame *pGame = pMission->GetRPGGame();
	if ( pGame->bChapterMapSet )
		NMainLoop::Command( new CICContinueChapter( pGame ) );
	else if ( pGame->bGlobalMapSet )
		NMainLoop::Command( new CICContinueGlobal( pGame ) );
	else
		NMainLoop::Command( new CICMainMenu() );	// retail @0x20a850: no chapter/global map (tutorial) -> main menu
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICBeginMission
////////////////////////////////////////////////////////////////////////////////////////////////////
CICMapEditBeginMission::CICMapEditBeginMission( int nVariantID, NRPG::CGlobalGame *pGlobalGame ):
	CICBeginMission( -1, nVariantID, vector<string>(), pGlobalGame )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICMapEditBeginMission::Exec()
{
	// retail NGame::CICMapEditBeginMission::Exec @0x20a920 -- the map-editor start uses the DEFAULT splash
	// (SetLoadingImage(NULL) -> texture 0x373) and seeds the bar at 10% (disasm: xor ecx,ecx call
	// SetLoadingImage; mov ecx,0xa call ShowLoadingScreen). Replaces the placeholder ShowLogo().
	SetLoadingImage( 0 );
	ShowLoadingScreen( 10 );
	CMission *pRes = new CMission();
	pRes->Initialize( -1, nVariantID, 0, params, pGlobalGame );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandStartScenarioZone( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() < 2 )
	{
		csSystem << "usage: zone [scenario] [zone] [clue1] [clue2] ... [pers1] [pers2] ... " << endl;
		return;
	}
	//
	vector<int> perses;
	vector<string> clues;
	int nSize = paramsSet.size();
	for ( int i = 2; i < nSize; ++i )
	{
		string szStr = NStr::ToAscii( paramsSet[ i ] );
		int n = atoi( szStr.c_str() );
		if ( n == 0 )
			clues.push_back( szStr );
		else
			perses.push_back( n );
	}
	//
	CPtr<NRPG::CGlobalGame> pGlobalGame = NRPG::CreateGlobalGame();	
	if ( perses.empty() )
		pGlobalGame->players.push_back( NRPG::CreateGlobalPlayer() );
	else
		pGlobalGame->players.push_back( NRPG::CreateGlobalPlayer( perses ) );
	//
	CPtr<NScenario::CScenarioTracker> pScenario = pGlobalGame->pScenarioTracker;
	string szScenarioName = NStr::ToAscii( paramsSet[ 0 ] );
	pScenario->CreateScenario( szScenarioName );
	CDBPtr<NDb::CSide> pSide = NScenario::GetSideForScenario( pScenario );
	if ( !IsValid( pSide ) )
		return;
	pGlobalGame->players.front()->pSide = pSide;
	if ( pScenario->IsScenarioAvailable() )
	{
		string szZoneName = NStr::ToAscii( paramsSet[ 1 ] );
		CPtr<NScenario::CScenarioZone> pZone = pScenario->GetZoneByName( szZoneName );
		if ( IsValid( pZone ) )
		{
			if ( nSize > 2 )
			{
				// remove clues
				vector< CPtr<NScenario::CScenarioClue> > tmpClues = pZone->GetClues();
				for ( vector< CPtr<NScenario::CScenarioClue> >::iterator i  = tmpClues.begin(); i != tmpClues.end(); ++i )
					pZone->RemoveClue( *i );
				// add clues
				for ( vector<string>::iterator i = clues.begin(); i != clues.end(); ++i )
				{
					CPtr<NScenario::CScenarioClue> pClue = pScenario->GetClueByName( *i );
					if ( IsValid( pClue ) )
						pZone->PlaceClue( pClue );
					else
						csSystem << CC_RED << "Error : " << CC_GREY << " clue " << *i << " not found" << endl;
				}
			}
			//
			NMainLoop::GetSaveManager()->ClearSlot( NMainLoop::S_SLOT_ACTIVE );
			NMainLoop::Command( new CICBeginMission( pZone, -1, vector<string>(), pGlobalGame ) );
		}
		else
			csSystem << CC_RED << "Error : " << CC_GREY << " scenario zone " << szZoneName << " not found" << endl;
	}
	else
		csSystem << CC_RED << "Error : " << CC_GREY << " scenario " << szScenarioName << " not found" << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(iMission)
	REGISTER_CMD( "map", CommandStartMission )
	REGISTER_CMD( "template", CommandStartMissionByTemplateID )
	REGISTER_CMD( "zone", CommandStartScenarioZone )
	REGISTER_CMD( "mission_camera_limits", CommandSetDefaultCameraLimits )
	REGISTER_CMD( "mission_camera_softlimits", CommandSetDefaultCameraSoftLimits )
	REGISTER_CMD( "mission_camera_attenuation", CommandSetDefaultCameraAttenuation )
	REGISTER_CMD( "mission_camera_softattenuation", CommandSetDefaultCameraSoftAttenuation )
	REGISTER_CMD( "mission_camera_minheight", CommandSetDefaultCameraMinHeight )
	REGISTER_CMD( "mission_camera_scroll_acceleration", CommandSetDefaultCameraScrollAcceleration )
	REGISTER_CMD( "mission_camera_scroll_speed", CommandSetDefaultCameraScrollSpeed )
	REGISTER_CMD( "mission_camera_zoom_acceleration", CommandSetDefaultCameraZoomAcceleration )
	REGISTER_CMD( "mission_camera_zoom_speed", CommandSetDefaultCameraZoomSpeed )
	REGISTER_CMD( "mission_camera_yaw_speed", CommandSetDefaultCameraYawSpeed )
	REGISTER_CMD( "mission_camera_yaw_acceleration", CommandSetDefaultCameraYawAcceleration )
	REGISTER_CMD( "mission_camera_pitch_speed", CommandSetDefaultCameraPitchSpeed )
	REGISTER_VAR( "cheat_showall", VarCheatShowAll, 0, false )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB251121B, CMission )
REGISTER_SAVELOAD_CLASS( 0x02511218, CVisibleTracker )
