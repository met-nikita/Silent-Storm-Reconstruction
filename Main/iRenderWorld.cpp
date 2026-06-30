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

	CPtr<NDb::CMusic> pAmbientMelody = NDb::GetMusic( 1 );
	CPtr<NDb::CTemplVariant> pVar = NDb::GetTemplVariant( nVariantID );
	if ( IsValid( pVar ) && IsValid( pVar->pAmbientMusic ) )
		pAmbientMelody = pVar->pAmbientMusic;

	pScene = NGScene::CreateNewView();
	pSoundScene = NSound::CreateSoundScene( pAmbientMelody );
	pRender = NRender::CreateRenderGame( pWorld, pScene );
	pRenderSound = NRender::CreateRenderSound( pWorld, pSoundScene );

	NDb::CAmbientLightReal *pLight = pWorld->GetDefaultLight();
	if ( pLight )
		pScene->SetAmbient( pLight );
	else
		SetLightMode( 0 );

	pCamera = CreateCamera( CAMERA_PC );

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
void CRenderBaseInterface::SetLightMode( int _nLightMode )
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
void CRenderBaseInterface::OnGetFocus()
{
	pRender->ResetTiming();
	pRenderSound->ResetTiming();
}
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
		pRender->ResetTiming();
		pRenderSound->ResetTiming();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderBaseInterface::RenderFrame( const STime &sTime, ICamera *pCamera )
{
	pRender->UpdateViewWorld( true, sTime, 0, true );

	CTransformStack ts;
	pCamera->GetTransform( &ts, pScene->GetScreenRect() );

	pRenderSound->Update( &ts, sTime );

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
