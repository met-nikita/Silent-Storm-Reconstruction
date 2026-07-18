#include "StdAfx.h"
#include "Transform.h"
#include "GView.h"
#include "G2DView.h"
#include "GSceneUtils.h"
#include "wInterface.h"
#include "wMainTrace.h"
#include "Sound.h"
#include "RWGame.h"
#include "RPGGame.h"
#include "RPGGlobal.h"
#include "Interface.h"
#include "iMain.h"
#include "iMission.h"
#include "iMissionExec.h"
#include "iCommonUI.h"
#include "iDesktopWindow.h"
#include "..\MiscDll\Commands.h"
#include "..\MiscDll\LogStream.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataLight.h"
#include "..\FileIO\BasicChunk1.h"   // START_REGISTER / FINISH_REGISTER (the ui_followcamera cvar)
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CMissionBase -- the retail mission base class bodies (release iBase.obj; serialization-
// convergence W4.2 structural split; see iMission.h for the class banner and the tag table).
//
// Everything here only touches CMissionBase's own members. The retail mission pump (Step @0x1a3ad0 /
// InternalStep @0x1a29f0 / ProcessEvent @0x1a2010 / ExecWorldCommand @0x1a30c0) and the exit/save/
// load bind dispatch stay on the dev CMission monolith -- deferred behaviour leg.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NGame::iBaseInit::iBaseInit @0x1a4610 (iBase.obj): registers the "ui_followcamera" cvar
// (default 1) backing bCameraShowEnemyActions -- the "camera follows enemy actions" game option.
// CMissionBase::GetFollowCameraState @0x1a1790 returns it; it gates the GetCamera selector's
// enemy-turn-follow and camera-locator legs (@0x1a1ee0). Retail binds the bool directly through
// RegisterVar; the dev RegisterVar is handler-based (same pattern as cheat_showall).
static bool bCameraShowEnemyActions = true;
static void VarFollowCamera( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	bCameraShowEnemyActions = sValue.GetFloat() != 0;
}
START_REGISTER(iMissionBase)
	REGISTER_VAR( "ui_followcamera", VarFollowCamera, 1, true )   // retail iBaseInit @0x1a4610: bSave=true (line in config.cfg)
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::CMissionBase @0x1a2c70 (default ctor member defaults; the retail exit/save/
// load binds constructed there stay on the dev CMission). The time counters are wired exactly like
// retail: pTimeFunc/pUITimeFunc adopt the counters' CCTime nodes.
CMissionBase::CMissionBase():
	bPause( false ), bRenderWorld( true ), nDeltaTime( 0 ),
	bHideInterface( false ), bSpecialHideInterface( false ),
	bWaitForPartFinished( false ), nLightMode( 0 ), bCheatVisibility( false ), bTutorialMode( false ),
	bCanSave( true ), bCanRestart( false ),
	sLastUpdateTime( 0 ), sMinFrameTime( 0 ), sFPSLimitLastTime( 0 )
{
	pTimeFunc = sTimeCounter.GetTime();
	pUITimeFunc = sUITimeCounter.GetTime();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::Command @0x1a18f0: forward to the active player's commander
void CMissionBase::Command( NWorld::CCommand *pCmd )
{
	ASSERT( pCmd );
	pActivePlayer->GetCommander()->Do( pCmd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::DoEvent @0x1a2fa0 (mission vtbl+0x2c): forward to the active player's
// commander EVENTS channel -- never forces an interrupt, drained unconditionally each CWorld::Segment
void CMissionBase::DoEvent( NWorld::CCommand *pCmd )
{
	ASSERT( pCmd );
	pActivePlayer->GetCommander()->DoEvent( pCmd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::OnGetFocus @0x1a19c0: back on top of the interface stack -- re-baseline the
// render/mixer timers over the covered gap and resume every frozen channel
void CMissionBase::OnGetFocus()
{
	pRender->ResetTiming();	// retail @0x2cb190 forwards to both sound mixers
	pSoundScene->Pause( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::OnLostFocus @0x1a19e0: a menu covered the mission -- freeze every live channel
// (this is why retail goes silent in the pause MENU but not on the Pause/Break realtime pause)
void CMissionBase::OnLostFocus()
{
	pSoundScene->Pause( true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::Command @0x1a15b0: wrap in CCmdSetCommand (+ CCmdContinue when instant),
// dispatched through the virtual single-command slot
void CMissionBase::Command( NWorld::CUnit *pUnit, NWorld::CCmd *pCmd, bool bInstantly )
{
	ASSERT( pUnit );
	ASSERT( pCmd );
	Command( new NWorld::CCmdSetCommand( pUnit, pCmd ) );

	if ( bInstantly )
		Command( new NWorld::CCmdSetCommand( pUnit, new NWorld::CCmdContinue ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::StopAction @0x1a29d0
void CMissionBase::StopAction()
{
	pActivePlayer->GetCommander()->StopAction();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionBase::IsReady() const
{
	// retail CMissionBase::IsReady @0x1a1e60: NOT ready while a sequence runs -> stops TraceCursor/
	// UpdateState so units cannot be hover-highlighted / selected during a sequence. Cover BOTH the
	// client sequence (nSequence, via CUICmdBeginSequence) AND a world-level / interrupt sequence
	// (CWorld::IsSequence) -- a cutscene that raises only the world predicate would otherwise leave
	// hover live.
	if ( IsSequence() || pWorld->IsSequence() )
		return false;

	if ( !IsRealTime() && ( IsActionExecuted() || ( pWorld->GetCurrentPlayer() != pActivePlayer->GetPlayer() ) ) )
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::IsActionExecuted @0x1a3e30
bool CMissionBase::IsActionExecuted() const
{
	return pWorld->IsExecuting() || pActivePlayer->GetCommander()->HasCommands();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::IsRealTime @0x1a1900
bool CMissionBase::IsRealTime() const
{
	return pWorld->GetCurrentPlayer() == 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::PauseGame @0x1a16a0 / IsGamePaused @0x1a16b0
void CMissionBase::PauseGame( bool bState )
{
	bPause = bState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionBase::IsGamePaused() const
{
	return bPause;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::GetActivePlayer @0x1a1950
IPlayerTracker* CMissionBase::GetActivePlayer() const
{
	ASSERT( pActivePlayer );
	return pActivePlayer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::SetActivePlayer @0x1a2480
void CMissionBase::SetActivePlayer( IPlayerTracker* pPlayer )
{
	ASSERT( find( playersSet.begin(), playersSet.end(), pPlayer ) != playersSet.end() );
	pActivePlayer = pPlayer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::GetPlayers @0x1a3f90
void CMissionBase::GetPlayers( vector< CPtr<IPlayerTracker> > *pPlayersSet ) const
{
	ASSERT( pPlayersSet );
	pPlayersSet->clear();
	pPlayersSet->resize( playersSet.size() );
	for ( int nTemp = 0; nTemp < playersSet.size(); nTemp++ )
		(*pPlayersSet)[nTemp] = playersSet[nTemp].GetPtr();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::GetUnits @0x1a1960 / GetSelectedUnits @0x1a1970 / CountSelected @0x1a1980 /
// Select @0x1a1990 / SelectNext @0x1a19a0 / SelectPrev @0x1a19b0 -- forwards to the active tracker
void CMissionBase::GetUnits( vector< CPtr<IUnitTracker> > *pUnits ) const
{
	pActivePlayer->GetUnits( pUnits );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionBase::GetSelectedUnits( vector< CPtr<IUnitTracker> > *pUnits ) const
{
	pActivePlayer->GetSelectedUnits( pUnits );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMissionBase::CountSelected()
{
	return pActivePlayer->CountSelected();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionBase::Select( NWorld::CUnit *pUnit, bool bAdditive )
{
	pActivePlayer->Select( pUnit, bAdditive );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionBase::SelectNext()
{
	pActivePlayer->SelectNext();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionBase::SelectPrev()
{
	pActivePlayer->SelectPrev();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::IsPlayerTurn @0x1a1910: false only in turn-based play while the world's
// current player (world vtbl+0x60 GetCurrentPlayer) is not the active tracker's player.
bool CMissionBase::IsPlayerTurn() const
{
	if ( !IsRealTime() && pWorld->GetCurrentPlayer() != pActivePlayer->GetPlayer() )
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::GetFollowCameraState @0x1a1790: the ui_followcamera option flag.
bool CMissionBase::GetFollowCameraState() const
{
	return bCameraShowEnemyActions;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::GetCamera @0x1a1ee0 -- the camera SELECTOR. The mission owns TWO kinds of
// camera: its own pCamera (base tag 18) is the CINEMATIC camera (sequences, enemy-turn follow,
// camera-locator focus -- the camera the scripts lock/steer, routinely saved with nLockCount >= 1),
// and each player tracker owns the camera the PLAYER drives (tracker tag 10). Selection:
//  - a scripted sequence -> the cinematic camera, unconditionally;
//  - enemy turn with ui_followcamera on -> the cinematic camera;
//  - a live camera-locator exec with ui_followcamera on, outside the HQ base (world vtbl+0x1dc =
//    CWorld::IsBase @0x376e20) -> the cinematic camera;
//  - else -> the ACTIVE PLAYER's camera; the cinematic camera's unlock-intent latches are cleared
//    by a net-zero FreezeCamera(true)/(false) pair (@0xcffc0 clears them on the freeze).
// Tail: if the chosen camera reports UserWantedItToUnlock (@0xd0020 -- the user gave camera input
// while it was locked/following), hand control back to the player's camera, carrying the current
// pose over on the one-shot bJustWanted so the view does not jump.
ICamera* CMissionBase::GetCamera() const
{
	if ( IsSequence() )
		return pCamera;

	ICamera *pCam;
	if ( GetFollowCameraState() && !IsPlayerTurn() )
		pCam = pCamera;
	else if ( GetFollowCameraState() && IsValid( pExecLocator ) && !GetWorld()->IsBase() )
		pCam = pCamera;
	else
	{
		pCamera->FreezeCamera( true );
		pCamera->FreezeCamera( false );
		pCam = pActivePlayer->GetCamera();
	}

	bool bJustWanted = false;
	if ( pCam->UserWantedItToUnlock( &bJustWanted ) )
	{
		if ( bJustWanted && pCam != pActivePlayer->GetCamera() )
		{
			ICamera::SCameraPos sPos;
			pCam->GetPlacement( &sPos );
			pActivePlayer->GetCamera()->SetPlacement( sPos );
		}
		pCam = pActivePlayer->GetCamera();
	}
	return pCam;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::FocusCameraOnUnit @0x1a16e0: anchor the camera on the unit + cut the scene
// to the unit's floor. Retail focuses the LIVE camera (@0x5a16e8 GetCamera, mission vtbl+0xb8), not
// pCamera -- which is usually NOT the rendered one, so focusing it moved nothing.
void CMissionBase::FocusCameraOnUnit( NWorld::CUnit *pUnit )
{
	ICamera *pCam = GetCamera();
	ICamera::SCameraPos sPos;
	pCam->GetPlacement( &sPos );
	sPos.ptAnchor = pUnit->GetPosition().pos.GetCP();
	sPos.ptAnchor.z += 0.5f;								// @0x5a1704: the unit path's z lift
	pCam->SetPlacement( sPos );
	GetScene()->SetCutFloor( pUnit->GetPosition().pos.GetFloor() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::FocusCameraOnItem @0x1a1740 (mission vtbl+0xc4): anchor the camera on the
// item's position and cut the scene to the item's floor -- the same camera anchor+floor call as
// FocusCameraOnUnit @0x1a16e0, minus that path's +0.5z lift (items anchor on their pos).
// Fired by the in-world hint/clue icons' RBUTTONUP (retail SEvent 0x6000034).
void CMissionBase::FocusCameraOnItem( NWorld::IItem *pItem )
{
	ICamera *pCam = GetCamera();							// @0x5a1740: the LIVE camera, as above
	ICamera::SCameraPos sPos;
	pCam->GetPlacement( &sPos );
	sPos.ptAnchor = pItem->GetPos();
	pCam->SetPlacement( sPos );
	GetScene()->SetCutFloor( pItem->GetFloor() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::ICursor* CMissionBase::GetCursor() const
{
	return pCursor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::CInterface* CMissionBase::GetInterface() const
{
	return pInterface;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::IsInterfaceHidden @0x19de80
bool CMissionBase::IsInterfaceHidden() const
{
	return bHideInterface;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::SetWaitForPartFinished @0x19de90 / IsWaitForPartFinished @0x19dea0
void CMissionBase::SetWaitForPartFinished( bool bState )
{
	bWaitForPartFinished = bState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionBase::IsWaitForPartFinished() const
{
	return bWaitForPartFinished;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::PopDesktop @0x1a2fc0: pop from the back until the requested desktop popped
void CMissionBase::PopDesktop( NUI::CDesktopWindow *pDesktop )
{
	for ( int nTemp = 0; nTemp < desktopWindowsList.size(); nTemp++ )
	{
		NUI::CDesktopWindow *pTempWnd = desktopWindowsList.back();
		desktopWindowsList.pop_back();

		if ( pTempWnd == pDesktop )
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::PushDesktop @0x1a3030
void CMissionBase::PushDesktop( NUI::CDesktopWindow *pDesktop )
{
	desktopWindowsList.push_back( pDesktop );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::GetDesktop @0x1a2000 returns the list back UNCHECKED (the retail mission
// desktop always sits at the bottom of the list); the dev adds the empty guard for the
// CRenderBaseInterface-family, whose list stays empty.
NUI::CDesktopWindow* CMissionBase::GetDesktop() const
{
	if ( !desktopWindowsList.empty() )
		return desktopWindowsList.back();

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::SetCheatVisibility @0x19deb0 / GetCheatVisibility @0x19dec0
void CMissionBase::SetCheatVisibility( bool bState )
{
	bCheatVisibility = bState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionBase::GetCheatVisibility() const
{
	return bCheatVisibility;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::GetRPGGame @0x19df10 / GetWorld / GetScene @0x103d20 / GetSoundScene
// @0x19df20 / GetRenderGame @0x19df30
NRPG::CGlobalGame* CMissionBase::GetRPGGame() const
{
	return pGlobalGame;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::IWorld* CMissionBase::GetWorld() const
{
	return pWorld;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGScene::IGameView* CMissionBase::GetScene() const
{
	return pScene;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NSound::ISoundScene* CMissionBase::GetSoundScene() const
{
	return pSoundScene;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRender::IRenderGame* CMissionBase::GetRenderGame() const
{
	return pRender;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail COMDAT-stub default (out-of-line: CTransformStack is forward-declared in iMission.h);
// CMission overrides with its real per-frame transform.
const CTransformStack& CMissionBase::GetCameraTransform() const
{
	static CTransformStack ts;
	return ts;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::SetLightMode @0x1a2640 -- ONE body on the base (the dev previously carried
// identical copies on CMission and CRenderBaseInterface; consolidated here)
void CMissionBase::SetLightMode( int _nLightMode )
{
	CDBTable<NDb::CTAmbientLight> *pTable = NDatabase::GetTable<NDb::CTAmbientLight>();
	CDBIterator<NDb::CTAmbientLight> it( *pTable );
	int nCount = _nLightMode;
	while ( it.MoveNext() )
	{
		SRand rnd;
		CPtr<NDb::CAmbientLightReal> pLight = it.Get()->GetLight( &rnd );
		if ( !pLight->bInGameUse )
			continue;
		if ( nCount == 0 )
		{
			nLightMode = _nLightMode;
			pLightSource = 0;
			csSystem << "Light with ID = " << it.Get()->GetRecordID() << " selected" << endl;
			GetScene()->SetAmbient( pLight );
			return;
		}
		nCount--;
	}
	if ( _nLightMode == 0 )
	{
		// no lighting in table, using default one
		nLightMode = 0;
		GetScene()->SetAmbient( 0 );
		pLightSource = GetScene()->AddDirectionalLight( CVec3(0.5f,0.4f,0.45f), CVec3( 0.6f,1.4f,-1), CVec3(5,5,0), CVec2( 150, 150 ), 20 );
		GetScene()->SetAmbient( CVec3( 0.20f, 0.20f, 0.20f ), CVec3( 0.20f, 0.20f, 0.20f ) );
	}
	else
		SetLightMode( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CMissionBase::RenderFrame @0x1a19f0 (was the dev CMission::RenderFrame -- the body only
// touches base members, so it lands here per the retail placement; CMission no longer overrides it).
// Param 2 is the ADVANCE flag (retail PDB: (int,bool,ICamera*,bool)) -- the old dev `const STime&`
// was a mis-decode. The body picks its own clocks: sound = raw GetTime(), interface = GetUITime().
void CMissionBase::RenderFrame( int nMode, bool bAdvanceTime, ICamera *pCamera, bool bShowUnits )
{
	if ( nMode & N_RENDERMODE_3D )
	{
		CTransformStack ts;
		pCamera->GetTransform( &ts, pScene->GetScreenRect() );

		pRender->UpdateSound( bAdvanceTime, &ts, GetTime() );	// retail @0x1a19f0 -> @0x2cb1c0: raw main-loop clock + advance flag

		const CTRect<float> &rScreen = pCamera->GetScreenRect();
		if ( ( rScreen.Width() != 0 ) && ( rScreen.Height() != 0 ) )
		{
			NGScene::IGameView::SDrawInfo drawInfo;
			drawInfo.pTS = &ts;
			drawInfo.vOrigin = CVec2( rScreen.x1, rScreen.y1 );
			drawInfo.vSize = CVec2( rScreen.x2 - rScreen.x1, rScreen.y2 - rScreen.y1 );
			drawInfo.bUseDefaultClearColor = true;
			drawInfo.vClearColor = CVec3(0.25f,0.25f,0.25f); // not used due to using default clear color
			pScene->Draw( drawInfo );
		}
	}

	if ( !bHideInterface && !bSpecialHideInterface && ( nMode & N_RENDERMODE_2D ) )
		pInterface->Draw( GetUITime() );	// retail @0x1a19f0: the always-running UI counter, not the raw clock

	float fFrameTime = NGScene::GetFrameTime();
	static float fMinFrameTime = 1, fMaxFrameTime = 1e-4f, fElapsed = 0;
	static int nFrames;
	fMinFrameTime = Min( fMinFrameTime, fFrameTime );
	fMaxFrameTime = Max( fMaxFrameTime, fFrameTime );
	fElapsed += fFrameTime;
	++nFrames;
	if ( fElapsed > 3 )
	{
		DebugTrace( "min %f max %f average %f\n", 1 / fMaxFrameTime, 1 / fMinFrameTime, nFrames / fElapsed );
		fMinFrameTime = 1;
		fMaxFrameTime = 1e-4f;
		fElapsed = 0;
		nFrames = 0;
	}

	if ( !( nMode & 8 ) )	// retail @0x5a1c29: bit 8 (CAutoPlayInterface logo frame) suppresses the present
		NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
