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
#include "Interface.h"
#include "iMain.h"
#include "iMainMenu.h"
#include "iRenderWorld.h"
#include "iSaveLoad.h"
#include "iCommonUI.h"
#include "iSideMenu.h"
#include "iCustomGameMenu.h"   // NGame::CICCustomGameMenu (the custom-game / mods-browser screen)
#include "iOptionsMenu.h"
#include "iCreditsScreen.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataSound.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataCamera.h"
#include "A5Script.h"
#include "wMain.h"
#include "wUICommands.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_MAINMENU_CAMERA = 26,
	N_MAINMENU_TEMPLATE = 2425,
	N_MAINMENU_SCRIPT = 85,			// game.db "MainMenu" script: PlayMenuMan animates the background "Man"
	N_LOGO_FLASHTIME = 2000;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFlashImage
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFlashImage: public CWindow
{
	OBJECT_BASIC_METHODS(CFlashImage);
private:
	ZDATA_(CWindow)
	float fCoeff;
	STime sMorphTime;
	CPtr<CImage> pActive;
	CPtr<CImage> pBackground;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&fCoeff); f.Add(3,&sMorphTime); f.Add(4,&pActive); f.Add(5,&pBackground); return 0; }

public:
	CFlashImage() {}
	CFlashImage( const SWindowInfo &sInfo );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CFlashImage::CFlashImage( const SWindowInfo &sInfo ):
	CWindow( sInfo ), fCoeff( 0 ), sMorphTime( 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFlashImage::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
		case EVENT_TEMPLATELOADCOMPLETE:
		{
			pActive = GetUIWindow<CImage>( this, "active" );
			pBackground = GetUIWindow<CImage>( this, "background" );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFlashImage::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	float fTargetCoeff = float( sTime % ( N_LOGO_FLASHTIME * 2 ) ) / N_LOGO_FLASHTIME;
	if ( fTargetCoeff > 1 )
		fTargetCoeff = 2 - fTargetCoeff;

	fCoeff = CalcFlashCoeff( fCoeff, fTargetCoeff, sTime, sMorphTime );
	sMorphTime = sTime;

	pActive->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF * fCoeff ) );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMainMenuUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMainMenuUI: public CWindow
{
	OBJECT_BASIC_METHODS(CMainMenuUI);
private:
	ZDATA_(CWindow)
	CObj<CFlashImage> pLogo;
	CObj<CButtonsLine> pButtonsLine1;
	CObj<CButtonsLine> pButtonsLine2;
	CObj<CHoverButton> pTutorial;
	CObj<CHoverButton> pCampaign;
	CObj<CHoverButton> pCustomGame;
	CObj<CHoverButton> pLoadGame;
	CObj<CHoverButton> pOptions;
	CObj<CHoverButton> pCredits;
	CObj<CHoverButton> pQuit;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pLogo); f.Add(3,&pButtonsLine1); f.Add(4,&pButtonsLine2); f.Add(5,&pTutorial); f.Add(6,&pCampaign); f.Add(7,&pCustomGame); f.Add(8,&pLoadGame); f.Add(9,&pOptions); f.Add(10,&pCredits); f.Add(11,&pQuit); return 0; }
public:
	CMainMenuUI() {}
	CMainMenuUI( const SWindowInfo &sInfo );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CMainMenuUI::CMainMenuUI( const SWindowInfo &sInfo ): 
	CWindow( sInfo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMainMenuUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
		case EVENT_TEMPLATELOAD:
		{
			// The retail menu wraps the line_1/line_2 template controls in two CButtonsLine and fills
			// them with hover-buttons programmatically (the per-button static controls the predecessor
			// looked up by name do not exist in the retail container). Per button the three captions are
			// GetDBString(markup) + GetDBString(name): the per-state markup string (a <font face=Impact
			// size=36pt outline...> tag, ids NORMAL 0x2b79 / HOVER 0x2b7a / DISABLED 0x43bb) MUST precede
			// the caption text so the font/colour applies (same order as the predecessor's prefix+text).
			// The 2nd arg is the tooltip DB-string id. Ids from CMainMenuUI::ProcessMessage @0x1f79d0.
			pLogo = new CFlashImage( sEvent.pLoader->GetControl( "logo" ) );

			pButtonsLine1 = new CButtonsLine( sEvent.pLoader->GetControl( "line_1" ) );
			pButtonsLine2 = new CButtonsLine( sEvent.pLoader->GetControl( "line_2" ) );

			pTutorial   = pButtonsLine1->AddHoverButton( "tutorial",   0x4bed, GetDBString( 0x2b79 ) + GetDBString( 0x4559 ), GetDBString( 0x2b7a ) + GetDBString( 0x4559 ), GetDBString( 0x43bb ) + GetDBString( 0x4559 ) );
			pCampaign   = pButtonsLine1->AddHoverButton( "campaign",   0x4be9, GetDBString( 0x2b79 ) + GetDBString( 0x2a96 ), GetDBString( 0x2b7a ) + GetDBString( 0x2a96 ), GetDBString( 0x43bb ) + GetDBString( 0x2a96 ) );
			pCustomGame = pButtonsLine1->AddHoverButton( "customgame", 0x4dc0, GetDBString( 0x2b79 ) + GetDBString( 0x4dc1 ), GetDBString( 0x2b7a ) + GetDBString( 0x4dc1 ), GetDBString( 0x43bb ) + GetDBString( 0x4dc1 ) );
			pLoadGame   = pButtonsLine1->AddHoverButton( "loadgame",   0x4bea, GetDBString( 0x2b79 ) + GetDBString( 0x2a97 ), GetDBString( 0x2b7a ) + GetDBString( 0x2a97 ), GetDBString( 0x43bb ) + GetDBString( 0x2a97 ) );

			pOptions    = pButtonsLine2->AddHoverButton( "options",    0x4beb, GetDBString( 0x2b79 ) + GetDBString( 0x2a98 ), GetDBString( 0x2b7a ) + GetDBString( 0x2a98 ), GetDBString( 0x43bb ) + GetDBString( 0x2a98 ) );
			pCredits    = pButtonsLine2->AddHoverButton( "credits",    0x4bee, GetDBString( 0x2b79 ) + GetDBString( 0x2a99 ), GetDBString( 0x2b7a ) + GetDBString( 0x2a99 ), GetDBString( 0x43bb ) + GetDBString( 0x2a99 ) );
			pQuit       = pButtonsLine2->AddHoverButton( "quit",       0x4bec, GetDBString( 0x2b79 ) + GetDBString( 0x2a9a ), GetDBString( 0x2b7a ) + GetDBString( 0x2a9a ), GetDBString( 0x43bb ) + GetDBString( 0x2a9a ) );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMainMenuInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMainMenuInterface: public CRenderBaseInterface
{
	OBJECT_BASIC_METHODS(CMainMenuInterface);
private:
	NInput::CBind bindCampaign, bindCustomGame, bindLoadGame, bindOptions, bindCredits, bindQuitGame;

	// Menu camera-fly executor state (runtime only). The release menu inherits CMissionBase's per-frame
	// ExecWorldCommand queue-drain + camera executor; the dev menu (CRenderBaseInterface) has none, so we run a
	// minimal executor in Step(): PlayMenuCamera queues CUICmdSetCameraClipDistance + CUICmdScriptMoveCamera on
	// the world's own script (via the id-queue), and we drain+play them here. NOT serialized (menu isn't saved).
	CPtr<NWorld::CUICmdScriptMoveCamera> pActiveCameraCmd;
	int nActiveWaypoint;
	STime sWaypointStartTime;
	ICamera::SCameraPos sWaypointStartPos;

	ZDATA_(CRenderBaseInterface)
	CObj<NUI::CMainMenuUI> pMainMenuUI;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CRenderBaseInterface*)this); f.Add(2,&pMainMenuUI); return 0; }

public:
	CMainMenuInterface();

	void Initialize();

	void Step();
	bool ProcessEvent( const NInput::SEvent &sEvent );
private:
	void ExecMenuCommands();						// drain+execute the menu world's queued camera UI commands
	bool UpdateMenuCamera( const STime &sTime );	// tick the active camera fly; returns true when finished
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMainMenuInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
CMainMenuInterface::CMainMenuInterface():
	bindCampaign( "campaign" ), bindCustomGame( "customgame" ), bindLoadGame( "loadgame" ), bindOptions( "options" ), bindCredits( "credits" ), bindQuitGame( "quit" ),
	nActiveWaypoint( 0 ), sWaypointStartTime( 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainMenuInterface::Initialize()
{
	CRenderBaseInterface::Initialize( N_MAINMENU_TEMPLATE );

	CPtr<NDb::CDBCamera> pDBCamera = NDb::GetDBCamera( N_MAINMENU_CAMERA );
	ICamera::SCameraPos sCameraPos( pDBCamera->vAnchor, pDBCamera->fDistance, pDBCamera->fPitch, pDBCamera->fYaw, pDBCamera->fRoll, pDBCamera->fFOV );
	GetCamera()->SetPlacement( sCameraPos );

	pMainMenuUI = new NUI::CMainMenuUI( NUI::SWindowInfo( GetInterface(), NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "mainmenuUI" ) );
	NUI::LoadTemplate( pMainMenuUI, NDb::GetUIContainer( 347 ) );
	pMainMenuUI->ShowWindow( NUI::SWTYPE_SHOW );

	// Animate the 3D background "Man". The MainMenu animation is a scenario script (PlayMenuMan loops
	// ObjectPlayAnimation + WaitForObject over a list of poses). It must run on the menu world's OWN script
	// (pOwnScript): that script is bound to this world -- so GetObject('Man') resolves; the retail
	// luaGetObject @0x2e77a0 requires a non-null world, so the retail UI-script path (CWindow::ProcessMessage
	// creates a NULL-world script) cannot animate a world object -- and it autoloads Scripts\Common.l, which
	// defines the WaitForObject helper. NScript::lua_dobuffer QUEUES the chunk; it runs (and StartThreads
	// PlayMenuMan) when the world's Segment ticks pOwnScript->ExecuteThreads(), and Segment clears each
	// object's action flag so WaitForObject advances through the pose list. This game.db's container 347
	// carries no ScriptID, so we launch the MainMenu DB script (id 85) directly here.
	// NOTE: id 85's PlayMenuCamera camera-fly thread additionally needs the release-new CameraSequence /
	// CameraSetClipping / IsUIActionIDPresent bindings + CScript id-queue (a separate save-format leg); they
	// are absent here, so that thread logs one benign "nil value" and the camera stays static. The Man loop
	// (PlayMenuMan) is independent and unaffected.
	if ( NWorld::CWorld *pCWorld = CDynamicCast<NWorld::CWorld>( GetWorld() ) )
		if ( NScript::CScript *pWorldScript = pCWorld->GetOwnScript() )
			pWorldScript->RunScriptByID( N_MAINMENU_SCRIPT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMainMenuInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	NInput::SetSection( "menu" );

	if ( CRenderBaseInterface::ProcessEvent( sEvent ) )
		return true;

	if ( bindCampaign.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICSideMenu() );
		return true;
	}
	else if ( bindCustomGame.ProcessEvent( sEvent ) )   // retail CMainMenuInterface::ProcessEvent @0x1f7540: new CICCustomGameMenu
	{
		NMainLoop::Command( new CICCustomGameMenu() );
		return true;
	}
	else if ( bindLoadGame.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NGame::CICSaveLoadMenu( NGame::LOAD ) ); 
		return true;
	}
	else if ( bindOptions.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NGame::CICOptions( NGame::OS_PROFILE ) );
		return true;
	}
	else if ( bindCredits.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NGame::CICCreditsScreen( false ) );
		return true;
	}
	else if ( bindQuitGame.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( 0 ); 
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainMenuInterface::Step()
{
	CRenderBaseInterface::Step();

	if ( CanRender() )
	{
		// Advance the menu world's script coroutines (PlayMenuMan) each rendered frame. The world's Segment
		// (driven by RenderFrame->UpdateViewWorld->UpdateWorld) also ticks pOwnScript and clears the object
		// action flags, but tick here too so the menu animation keeps advancing on frames the world does not
		// segment. (lua_dobuffer queues chunks; ExecuteThreads is what actually runs them + the StartThread'd
		// PlayMenuMan loop.)
		if ( NWorld::CWorld *pCWorld = CDynamicCast<NWorld::CWorld>( GetWorld() ) )
			pCWorld->ExecuteOwnScript();	// sets the active-script context first; a bare ExecuteThreads() crashes after returning from a pushed interface (GetScript()==0)

		// Execute the camera UI commands PlayMenuCamera just queued (clip planes + the waypoint fly) and tick
		// the active fly. The menu is not a CMissionBase, so this minimal executor stands in for the inherited
		// ExecWorldCommand path; on completion it drops the queued id so the script's WaitForUI(id) unblocks.
		ExecMenuCommands();

		// Render the 3D menu world full-screen. The predecessor derived a sub-rect from a "clientview" UI
		// control, but the retail menu container (347) ships no such control (-> GetUIWindow fell back to a
		// zero-size window -> zero camera screen-rect -> RenderFrame skipped pScene->Draw -> no 3D backdrop,
		// and the "control clientview not found" log spam). The release sets a full-screen rect directly.
		GetCamera()->SetScreenRect( CTRect<float>( 0.0f, 0.0f, 1.0f, 1.0f ) );

		RenderFrame( GetTime(), GetCamera() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainMenuInterface::ExecMenuCommands()
{
	NWorld::CWorld *pCWorld = CDynamicCast<NWorld::CWorld>( GetWorld() );
	if ( !pCWorld )
		return;
	NScript::CScript *pWorldScript = pCWorld->GetOwnScript();
	//
	// Drain everything PlayMenuCamera queued (clip planes + the waypoint fly) onto the world's UI command list.
	for ( ; ; )
	{
		CPtr<NWorld::CUICmd> pCmd = pCWorld->GetUICommand();
		if ( !IsValid( pCmd ) )
			break;
		//
		CDynamicCast<NWorld::CUICmdSetCameraClipDistance> pClip( pCmd );
		if ( pClip )
		{
			GetCamera()->SetClipDistance( pClip->fMinDistance, pClip->fMaxDistance );
			continue;
		}
		CDynamicCast<NWorld::CUICmdScriptMoveCamera> pMove( pCmd );
		if ( pMove )
		{
			// start this fly; if one was already running, finish it (drop its id) first.
			if ( IsValid( pActiveCameraCmd ) && IsValid( pWorldScript ) )
				pWorldScript->RemoveUIActionID( pActiveCameraCmd->nID );
			pActiveCameraCmd = pMove;
			nActiveWaypoint = 0;
			sWaypointStartTime = 0;
			continue;
		}
		// other UI commands have no menu executor -- ignore them.
	}
	//
	// Tick the active camera fly; when it finishes, drop its id so the script's WaitForUI(id) unblocks.
	if ( IsValid( pActiveCameraCmd ) && UpdateMenuCamera( GetTime() ) )
	{
		if ( IsValid( pWorldScript ) )
			pWorldScript->RemoveUIActionID( pActiveCameraCmd->nID );
		pActiveCameraCmd = 0;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMainMenuInterface::UpdateMenuCamera( const STime &sTime )
{
	const vector<ICamera::SCameraPos> &positions = pActiveCameraCmd->positions;
	if ( positions.empty() )
		return true;
	//
	if ( sWaypointStartTime == 0 )
	{
		sWaypointStartTime = sTime;
		GetCamera()->GetPlacement( &sWaypointStartPos );	// seed the start pose from the live camera
	}
	STime transitionTime = pActiveCameraCmd->transitionTime;	// per-waypoint duration
	float fCoeff = ( transitionTime <= 0 ) ? 1.0f : Min( float( sTime - sWaypointStartTime ) / transitionTime, 1.0f );
	//
	const ICamera::SCameraPos &target = positions[ nActiveWaypoint ];
	ICamera::SCameraPos cur;
	cur.fRod     = target.fRod   * fCoeff + sWaypointStartPos.fRod   * ( 1 - fCoeff );
	cur.fPitch   = target.fPitch * fCoeff + sWaypointStartPos.fPitch * ( 1 - fCoeff );
	cur.fYaw     = target.fYaw   * fCoeff + sWaypointStartPos.fYaw   * ( 1 - fCoeff );
	cur.fRoll    = target.fRoll  * fCoeff + sWaypointStartPos.fRoll  * ( 1 - fCoeff );
	cur.fFOV     = target.fFOV   * fCoeff + sWaypointStartPos.fFOV   * ( 1 - fCoeff );
	cur.ptAnchor = target.ptAnchor * fCoeff + sWaypointStartPos.ptAnchor * ( 1 - fCoeff );
	GetCamera()->SetPlacement( cur );
	//
	if ( fCoeff >= 1.0f )
	{
		sWaypointStartPos = target;			// next segment starts from this waypoint
		sWaypointStartTime = sTime;
		++nActiveWaypoint;
		if ( nActiveWaypoint >= ( int )positions.size() )
			return true;					// whole sequence played
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICMainMenu
////////////////////////////////////////////////////////////////////////////////////////////////////
CICMainMenu::CICMainMenu()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICMainMenu::Exec()
{
	ResetStack();
	CMainMenuInterface *pRes = new CMainMenuInterface;
	pRes->Initialize();
	SetInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB2540910, CMainMenuUI );
REGISTER_SAVELOAD_CLASS( 0xB2540911, CFlashImage );
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB2540912, CMainMenuInterface );
////////////////////////////////////////////////////////////////////////////////////////////////////
// Start mainmenu
////////////////////////////////////////////////////////////////////////////////////////////////////
static void StartMainMenu( const string &szID, const vector<wstring> &szParams, void *pContext )
{
	NMainLoop::Command( new NGame::CICMainMenu() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(iMainMenu)
	REGISTER_CMD( "mainmenu", StartMainMenu )
FINISH_REGISTER
