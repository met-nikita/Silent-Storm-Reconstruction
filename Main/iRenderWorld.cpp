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
#include "iRenderWorld.h"
#include "iCommonUI.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataLight.h"
#include "..\DBFormat\DataSound.h"
#include "..\DBFormat\DataFormat.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRenderBaseInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
CRenderBaseInterface::CRenderBaseInterface():
	bindShadows("toggle_shadows"), bindSwitchLighting( "switch_lighting" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderBaseInterface::Initialize( int nTemplateID )
{
	SRandomSeed sSeed;
	const vector<string> params;
	CObj<NWorld::CPostWorldCreateInfo> pPostInfo;
	list< CPtr<NScenario::CScenarioClue> > clues;

	int nVariantID = -1;
	CPtr<NDb::CTemplate> pTemplate = NDb::GetTemplate( nTemplateID );
	if ( IsValid( pTemplate ) )
	{
		SRand sRand( sSeed );
		vector<int> dummyTemp;
		nVariantID = NDb::GetTemplVariant( pTemplate, dummyTemp, -1, &sRand )->GetRecordID();
	}

	pWorld = NWorld::CreateWorld( NRPG::CreateGlobalGame() );
	pWorld->CreateRandom( nVariantID, params, false, clues, 0, &pPostInfo, sSeed, true );

	pCommander = new NWorld::CCommander;
	pPlayer = pWorld->AddPlayer( L"Goga mega player", NRPG::CreateGlobalPlayer(), pCommander );

	// retail semantics: ambient is a MusicTemplates POOL (default GetTMusic(1), variant override),
	// the track is a roulette pick from it -- see CMission::Initialize for the mainline version.
	NDb::CTMusic *pAmbientPool = NDb::GetTMusic( 1 );
	CPtr<NDb::CTemplVariant> pVar = NDb::GetTemplVariant( nVariantID );
	if ( IsValid( pVar ) && IsValid( pVar->pAmbientMusic ) )
		pAmbientPool = pVar->pAmbientMusic;
	pScene = NGScene::CreateNewView();
	pSoundScene = NSound::CreateSoundScene( pAmbientPool, 0, pWorld->GetAimTime() );
	// retail CRenderBaseInterface::Initialize @0x22ef80: the sound mixers are owned by the render game
	pRender = NRender::CreateRenderGame( pWorld, pScene, pSoundScene );

	NDb::CAmbientLightReal *pLight = pWorld->GetDefaultLight();
	if ( pLight )
		pScene->SetAmbient( pLight );
	else
		SetLightMode( 0 );

	pCamera = CreateCamera( CAMERA_PC );
	// retail @0x22ef80 (@0x62f497-0x62f510): the factory stamps the GAMEPLAY limits, so a 3D-backdrop
	// menu immediately overrides them with the unbounded ctor defaults + bMovie, then LOCKS the camera.
	// Both halves are load-bearing: bMovie skips the terrain/approach tail, the lock skips the
	// clamps -- without them the menu's DB placement is dragged to the gameplay pitch limit.
	ICamera::SCameraLimits sMenuLimits;
	sMenuLimits.bMovie = true;					// @0x62f4ff
	pCamera->SetLimits( sMenuLimits );			// @0x62f507, cam vtbl+0x64
	pCamera->SetLock( true );					// @0x62f510, cam vtbl+0x70 (retail Lock)

	pCursor = NUI::ICursor::Create();
	// Wire the sound scene into the UI interface so CWindow::PlaySound (-> GetInterface()->GetSound()->Add2DSound)
	// actually plays UI/voice-preview sounds on these 3D-backdrop menus. The dev created pSoundScene (above) but
	// never connected it to the interface, so GetSound() was null and every menu PlaySound silently dropped. Retail
	// passes null to the CInterface ctor then wires the scene separately; the dev's single-arg ctor takes it directly.
	pInterface = new NUI::CInterface( pCursor, pSoundScene );

	pWorld->RunPostInit( pPostInfo );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderBaseInterface::Command( NWorld::CCommand *pCmd )
{
	ASSERT( pCmd );
	pCommander->Do( pCmd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderBaseInterface::DoEvent( NWorld::CCommand *pCmd )
{
	ASSERT( pCmd );
	pCommander->DoEvent( pCmd );	// drained by the front-end world's Segment like any player commander
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// W4.2: SetLightMode consolidated onto CMissionBase (retail @0x1a2640 -- one body on the base; the
// CMission copy was identical).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRenderBaseInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	pCursor->ProcessEvent( sEvent );

	if ( pInterface->ProcessEvent( sEvent ) )
		return true;

	if ( bindShadows.ProcessEvent( sEvent ) )
		pScene->SetNextShadowsMode();
	else if ( bindSwitchLighting.ProcessEvent( sEvent ) )
		SetLightMode( nLightMode + 1 );

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderBaseInterface::Step()
{
	if ( CanRender() )
	{
		pCamera->Update( GetTime() );

		pInterface->UpdateCursor();
		pInterface->Step( GetTime() );
	}
	else
	{
		pRender->ResetTiming();	// retail @0x2cb190 forwards to both sound mixers
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderBaseInterface::RenderFrame( const STime &sTime, ICamera *pCamera )
{
	pRender->UpdateViewWorld( true, sTime, 0, true );

	CTransformStack ts;
	pCamera->GetTransform( &ts, pScene->GetScreenRect() );

	pRender->UpdateSound( true, &ts, sTime );	// front-end views never pause the mixer clock

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

	pInterface->Draw( sTime );

	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB3140918, CRenderBaseInterface );
////////////////////////////////////////////////////////////////////////////////////////////////////
