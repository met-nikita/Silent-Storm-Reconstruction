#include "StdAfx.h"
#include "iMain.h"
#include "GView.h"
#include "G2DView.h"
#include "RPGGlobal.h"
#include "Sound.h"
#include "..\Input\Bind.h"
#include "iSaveManager.h"
#include "iInterMission.h"
#include "iGlobalMap.h"
#include "iCluesMenu.h"
#include "iMission.h"
#include "wInterface.h"
#include "wUICommands.h"
#include "RWGame.h"
#include "PlayerTracker.h"
#include "iGameStates.h"
#include "iShowHint.h"
#include "iInGameMenu.h"
#include "Interface.h"
#include "iGlobalMapUI.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\Misc\BasicShare.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataScenario.h"
#include "scFlowChartItems.h"
#include "scScenarioTracker.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
static CBasicShare<int, CGlobalInfoLoader> shareGlobalInfo(141);
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGlobalMap
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGlobalMap: public CMissionBase
{
	OBJECT_NOCOPY_METHODS(CGlobalMap);
private:
	// Retail owns the game-menu bind on CMissionBase. Keep this one non-serialized stand-in until
	// the common mission pump is moved there, as CChapterMap currently does.
	NInput::CBind bindClose, bindMenu, bindJournal, bindBaseZone;
	NGlobal::CCmd cmdSetDifficulty;
	ZDATA_(CMissionBase)
	bool bShowMode;
	//// global
	CDBPtr<NDb::CGlobalMap> pGlobalMap;
	CDGPtr<CPtrFuncBase<CGlobalInfo> > pGlobalInfo;
	//// interface
	CObj<NUI::CGlobalMapUI> pGlobalMapUI;
	// Retail NGame::CGlobalMap::operator& @0x1e4180: tag 1 is the COMPLETE 34-tag
	// CMissionBase chunk. The former four-member SBaseChunk silently discarded the other 30 tags
	// whenever a campaign was saved on the global map.
	ZEND int operator&( CStructureSaver &f )
	{
		f.Add(1,(CMissionBase*)this);
		f.Add(2,&bShowMode);
		f.Add(3,&pGlobalMap);
		f.Add(4,&pGlobalInfo);
		f.Add(5,&pGlobalMapUI);
		return 0;
	}

protected:
	void RenderFrame( const STime &sTime );
	void ProcessWorldCommands();

public:
	CGlobalMap();

	void Initialize( NRPG::CGlobalGame* pGame, bool bShowMode = false );

	bool IsGlobalMapShowMode() const;
	NDb::CGlobalMap* GetGlobalMap() const;
	CPtrFuncBase<CGlobalInfo>* GetGlobalInfo() const;

	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &sEvent );
	void Step();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetDifficulty( const string &szID, const vector<wstring> &paramsSet, void *pContext );
////////////////////////////////////////////////////////////////////////////////////////////////////
CGlobalMap::CGlobalMap():
	bindClose( "cancel" ), bindMenu( "gamemenu" ), bindJournal( "clues" ), bindBaseZone( "basezone" ),
	cmdSetDifficulty( "difficulty", CommandSetDifficulty, this ), bShowMode( false )
{
	bRenderWorld = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalMap::Initialize( NRPG::CGlobalGame *_pGame, bool _bShowMode )
{
	bShowMode = _bShowMode;
	pGlobalGame = _pGame;

	pGlobalMap = NDb::GetGlobalMap( pGlobalGame->nGlobalMapID );
	pGlobalInfo = shareGlobalInfo.Get( pGlobalGame->nGlobalMapID );

	// Retail CGlobalMap::Initialize: the 2D map still owns a world and players
	// so its database script can Sleep, show hints and receive UI completion events.
	pWorld = NWorld::CreateWorld( pGlobalGame );
	pWorld->CreateDefault();
	playersSet.resize( pGlobalGame->players.size() );
	for ( int nPlayer = 0; nPlayer < pGlobalGame->players.size(); ++nPlayer )
	{
		WCHAR wsName[32];
		swprintf( wsName, L"Player %d", nPlayer );
		playersSet[nPlayer] = new CPlayerTracker( this, pGlobalGame->players[nPlayer], wsName );
	}
	pActivePlayer = playersSet.front();
	CommandState( new CStateEmpty );
	pScene = NGScene::CreateNewView();
	pSoundScene = NSound::CreateSoundScene( NDb::GetTMusic( 15 ), 0, pWorld->GetAimTime() );
	pRender = NRender::CreateRenderGame( pWorld, pScene, pSoundScene );
	pCamera = CreateCamera( CAMERA_PC );
	ICamera::SCameraLimits limits;
	limits.bMovie = true;
	pCamera->SetLimits( limits );
	pCamera->SetLock( true );

#ifdef _MAPEDIT
	pCursor = NUI::ICursor::CreateEditorCursor();
#else
	pCursor = NUI::ICursor::Create( true );
#endif

	pInterface = new NUI::CInterface( pCursor, pSoundScene );

	pGlobalMapUI = new NUI::CGlobalMapUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "globalmapUI" ), this );
	NUI::LoadTemplate( pGlobalMapUI, NDb::GetUIContainer( 175 ) );
	pGlobalMapUI->ShowWindow( NUI::SWTYPE_SHOW );
	pWorld->RunPostInitScript( pGlobalMap->pScript );
	sMinFrameTime = 5;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGlobalMap::IsGlobalMapShowMode() const
{
	return bShowMode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CGlobalMap* CGlobalMap::GetGlobalMap() const
{
	return pGlobalMap;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPtrFuncBase<CGlobalInfo>* CGlobalMap::GetGlobalInfo() const
{
	return pGlobalInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalMap::OnGetFocus()
{
	CMissionBase::OnGetFocus();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGlobalMap::ProcessEvent( const NInput::SEvent &sEvent )
{
	NInput::SetSection( "game" );

	// Retail routes CMissionBase's gamemenu bind before cursor/UI events.
	if ( bindMenu.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICInGameMenu( pGlobalGame->players.front(), false, bCanSave ) );
		return true;
	}

	pCursor->ProcessEvent( sEvent );

	if ( pInterface->ProcessEvent( sEvent ) )
		return true;

	if ( bShowMode && bindClose.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NMainLoop::CICExitModal() ); 
		return true;
	}

	if ( bindJournal.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICClues( pGlobalGame ) );
		return true;
	}

	if ( bindBaseZone.ProcessEvent( sEvent ) )
	{
		vector<string> templParams;
		CPtr<NScenario::CScenarioZone> pZone = pGlobalGame->pScenarioTracker->GetZoneByDBZone( pGlobalMap->pBaseZone );
		if ( IsValid( pZone ) )
			NMainLoop::Command( new NGame::CICBeginMission( pZone, -1, templParams, pGlobalGame ) );
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalMap::ProcessWorldCommands()
{
	while ( true )
	{
		CPtr<NWorld::CUICmd> pCmd = pWorld->GetUICommand();
		if ( !IsValid( pCmd ) )
			return;
		if ( ExecWorldBeginZoneCommand( pCmd ) || ExecWorldSoundCommand( pCmd ) )
			continue;
		if ( CDynamicCast<NWorld::CUICmdShowHint> pHint = pCmd )
		{
			if ( NGlobal::GetVar( "ui_showhints" ).GetFloat() == 1.0f || bTutorialMode )
				NMainLoop::Command( new CICShowHint( this, pHint->GetID(), pHint->pHint, pGlobalGame ) );
			else
				DoEvent( new NWorld::CCmdInterfaceEvent( pHint->GetID() ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalMap::Step()
{
	if ( GetTime() - sFPSLimitLastTime < sMinFrameTime )
		return;
	sFPSLimitLastTime = GetTime();
	sTimeCounter.Advance( !bPause, GetTime() );
	sUITimeCounter.Advance( true, GetTime() );
	EraseInvalidRefs( &soundsList );
	if ( CanRender() )
	{
		pRender->UpdateViewWorld( !bPause, GetGameTime(), pActivePlayer->GetPlayer(), bCheatVisibility );
		ProcessWorldCommands();
		if ( NMainLoop::HaveInterfaceCommand() )
			return;
		for ( int nPlayer = 0; nPlayer < playersSet.size(); ++nPlayer )
			playersSet[nPlayer]->Update( IsRealTime() );
		pInterface->UpdateCursor();
		pInterface->Step( GetUITime() );
		RenderFrame( GetUITime() );
	}
	else
		pRender->ResetTiming();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalMap::RenderFrame( const STime &sTime )
{
	NGScene::ClearScreen( CVec3(0.5f, 0.5f, 0.5f ) );
	pInterface->Draw( sTime );
	NGScene::Flip();
	MarkNewDGFrame();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICBeginGlobal
////////////////////////////////////////////////////////////////////////////////////////////////////
CICBeginGame::CICBeginGame( int _nTemplateID, const vector<CObj<NRPG::CGlobalPlayer> > &_playersSet ):
	nTemplateID( _nTemplateID ), playersSet( _playersSet )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CICBeginGame::CICBeginGame( int _nTemplateID, const vector<CObj<NRPG::CGlobalPlayer> > &_playersSet, NDb::CDBDifficulty *_pDifficulty ):
	nTemplateID( _nTemplateID ), playersSet( _playersSet ), pDifficulty( _pDifficulty )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICBeginGame::Exec()
{
	int nScenarioID = -1;
	CPtr<NDb::CGlobalMap> pGlobalMap = NDb::GetGlobalMap( nTemplateID );
	if ( IsValid( pGlobalMap ) && IsValid( pGlobalMap->pScenario ) )
		nScenarioID = pGlobalMap->pScenario->GetRecordID();

	ResetStack(); // just the way to unregister "scenario" command
	CPtr<NRPG::CGlobalGame> pGame = NRPG::CreateGlobalGame( nScenarioID );
	pGame->players = playersSet;

	// Apply the chosen difficulty (release: CreateGlobalGame(id, pDifficulty) stores it on the global
	// game; here CreateGlobalGame defaults pDifficulty, so override it with the menu's choice when set).
	// CGlobalGame::pDifficulty is read downstream by the to-hit/AI subsystems, so the selection now
	// actually takes effect in-game.
	if ( IsValid( pDifficulty ) )
		pGame->pDifficulty = pDifficulty;

	pGame->bGlobalMapSet = true;
	pGame->nGlobalMapID = nTemplateID;

	// release CICBeginGame::Exec @0x1e2640: start the faction INTRO mission (StartZone) first; the base is
	// reached only after the intro mission completes (the existing zone-transition flow). The dev loaded
	// pBaseZone here, jumping straight to the base and skipping the faction-specific first mission.
	CPtr<NScenario::CScenarioZone> pZone = pGame->pScenarioTracker->GetZoneByDBZone( pGlobalMap->pStartZone );
	if ( !IsValid( pZone ) )
	{
		ASSERT( 0 );
		csSystem << CC_RED << L"ERROR: Can't start game! No start zone set!" << endl;
		return;
	}

	NMainLoop::CSaveManager *pSaveManager = NMainLoop::GetSaveManager();
	pSaveManager->ClearSlot( NMainLoop::S_SLOT_ACTIVE );

	vector<string> templParams;
	NMainLoop::Command( new NGame::CICBeginMission( pZone, -1, templParams, pGame ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICContinueGlobal
////////////////////////////////////////////////////////////////////////////////////////////////////
CICContinueGlobal::CICContinueGlobal( NRPG::CGlobalGame *_pGame ):
	pGame( _pGame )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICContinueGlobal::Exec()
{
	if ( !pGame->bGlobalMapSet )
	{
		csSystem << CC_RED << L"ERROR: Can't continue global! No global set!" << endl;
		return;
	}

	CGlobalMap *pRes = new CGlobalMap();
	pRes->Initialize( pGame );
	SetInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICShowGlobal
////////////////////////////////////////////////////////////////////////////////////////////////////
CICShowGlobal::CICShowGlobal( NRPG::CGlobalGame *_pGame ):
	pGame( _pGame )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICShowGlobal::Exec()
{
	if ( !pGame->bGlobalMapSet )
	{
		ASSERT( 0 );
		return;
	}

	CGlobalMap *pRes = new CGlobalMap();
	pRes->Initialize( pGame, true );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSetDifficulty( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() < 1 )
		return;
	//
	CObjectBase *pObject = (CObjectBase *)pContext;
	CDynamicCast<CGlobalMap> pMap(pObject);
	if (pMap)
		pMap->GetRPGGame()->ChangeDifficulty( wcstol( paramsSet[ 0 ].c_str(), 0, 10 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB1808180, CGlobalMap )
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandStartGlobal( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.empty() )
	{
		csSystem << "usage:" << szID << "#global" << endl;
		return;
	}

	int nTemp = wcstol( paramsSet.front().data(), 0, 10 );
	csSystem << CC_BLUE << "Loading global ( template " << nTemp << " ) ..." << endl;

	vector<CObj<NRPG::CGlobalPlayer> > playersSet;
	playersSet.push_back( NRPG::CreateGlobalPlayer() );
	NMainLoop::Command( new CICBeginGame( nTemp, playersSet ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(iGlobalMap)
	REGISTER_CMD( "global", CommandStartGlobal )
FINISH_REGISTER
