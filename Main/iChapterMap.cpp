#include "StdAfx.h"
#include "iMain.h"
// NGScene::IGameView must be COMPLETE here, not forward-declared: reparenting onto CMissionBase
// instantiates its wire in this TU, and that serializes a CObj<IGameView>. CastToObjectBase picks
// its overload on whether T* converts to CObjectBase*, so against an incomplete type it silently
// selects the declaration-only void* overload -> unresolved CastToObjectBaseImpl at link time.
// iMissionBase.cpp (which owns CMissionBase and serializes the same wire) includes GView.h likewise.
#include "GView.h"
#include "G2DView.h"
#include "RPGGlobal.h"
#include "Sound.h"
#include "..\Input\Bind.h"
#include "ChapterInfo.h"
#include "iInterMission.h"
#include "iMission.h"			// NGame::CMissionBase -- the CChapterMap base (retail @0x1a8480 tag 1)
#include "wInterface.h"
#include "wUICommands.h"
#include "RWGame.h"
#include "PlayerTracker.h"
#include "iGameStates.h"
#include "iTeamMngMenu.h"
#include "iShowHint.h"
#include "iGlobalMap.h"
#include "iChapterMap.h"
#include "iCluesMenu.h"
#include "iInGameMenu.h"
#include "Interface.h"
#include "iChapterMapUI.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\Misc\BasicShare.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataScenario.h"
#include "scScenarioTracker.h"
#include "scFlowChartItems.h"
#include "RPGMerc.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
CBasicShare<int, CChapterInfoLoader> shareChapterInfo(140);
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterMap
////////////////////////////////////////////////////////////////////////////////////////////////////
class CChapterMap: public CMissionBase
{
	OBJECT_NOCOPY_METHODS(CChapterMap);
private:
	NInput::CBind bindShowGlobal;
	NInput::CBind bindMenu, bindJournal;
	ZDATA_(CMissionBase)
	//// chapter
	CDBPtr<NDb::CChapterMap> pChapterMap;
	CDGPtr<CPtrFuncBase<CChapterInfo> > pChapterInfo;
	//// interface
	CObj<NUI::CChapterMapUI> pChapterMapUI;
	// v1.2 EnableFeature("reenter") flag, set by ExecWorldCommand @0x5a7140.
	bool bV12WorldCmdFlag;
	// retail NGame::CChapterMap::operator& @0x1a8480 -- 4 tags, tag 1 = the CMissionBase base chunk
	// (emitted non-polymorphically, exactly like CMission's @0x1a03a0).
	//
	// REPARENTED onto CMissionBase (wire-audit wave 3, slots 5+10 -- the two STRATEGY saves). This
	// class used to be a standalone NGame::IChapterMap whose four shared members (game/sound/cursor/
	// interface) were faked into the tag-1 chunk by an `SBaseChunk` shim emitting only base tags
	// 2/4/15/16. That shim dropped the OTHER 30 base tags on the floor: the audit's UNREAD rows
	// 1.3 1.5 1.7..1.14 1.18..1.26 1.28..1.38 -- exactly the complement of the four it read. There was
	// no corruption (CStructureSaver is tag-addressed, so an unknown tag is skipped gracefully); it
	// was silent DATA LOSS of the strategy-campaign camera/time/desktop/player bookkeeping, because
	// THE MEMBER DID NOT EXIST HERE TO READ INTO. Deriving from the fork's CMissionBase -- which
	// already carries the complete retail 34-tag base wire (iMission.h, retail
	// CMissionBase::operator& @0x19f3f0, 34 tags, gaps at 6/17/27 preserved) -- collapses the shim to
	// `f.Add(1,(CMissionBase*)this)` and gives all 30 tags a real home at once.
	//
	// Target shape is PDB-confirmed: NGame::CChapterMap size 296, base NGame::CMissionBase size 264
	// -> own members start at 0x108, and the retail ctor @0x1a6a60 (raw disasm) lays them out exactly
	// so: bindJournal@0x108 ("clues"), bindShowGlobal@0x110 ("showglobal"), pChapterMap@0x118,
	// pChapterInfo@0x11c/0x120, pChapterMapUI@0x124 -> 0x128 = 296. Byte-walk of slot 5 corroborates
	// the wire: tag 1 = 235B carrying tags 2..38 with gaps at 6/17/27; 1.19 len=56 = cameraLimits at
	// its retail 0x38 size; 1.30 = 01 = bCanSave (retail default true, ctor @0x1a2c70); 1.35 = 19150
	// = sLastUpdateTime; 1.36 = 5 = sMinFrameTime.
	//
	ZEND int operator&( CStructureSaver &f )
	{
		f.Add(1,(CMissionBase*)this);	// retail CMissionBase base chunk (@0x19f3f0)
		f.Add(2,&pChapterMap);
		f.Add(3,&pChapterInfo);
		f.Add(4,&pChapterMapUI);
		f.Add(5,&bV12WorldCmdFlag);		// v1.2 addition; v1.1's table @0x1a8480 emits 1..4
		return 0;
	}

	void UpdateChapterDifficulty();
	void ProcessWorldCommands();

protected:
	void RenderFrame( const STime &sTime );

public:
	CChapterMap();

	bool Initialize( NRPG::CGlobalGame* pGame );

	// GetCursor / GetInterface / GetSoundScene / GetRPGGame are NOT overridden here: retail's
	// CChapterMap vftable (@0x4b9adc) leaves those slots pointing at the CMissionBase bodies
	// (GetSoundScene @0x19df20, GetRPGGame @0x19df10, ...), which read the very same base members.
	// The old IChapterMap-era duplicates (GetGlobalGame + flat pGame/pCursor/pInterface/pSoundScene)
	// are gone; the base's pGlobalGame/pCursor/pInterface/pSoundScene are the real storage.
	// Only these two slots are genuinely CChapterMap's (vtbl+0x14c / +0x150):
	NDb::CChapterMap* GetChapterMap() const;
	bool CanReenterZone() const { return bV12WorldCmdFlag; }
	CPtrFuncBase<CChapterInfo>* GetChapterInfo() const;

	bool ProcessEvent( const NInput::SEvent &sEvent );
	void Step();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CChapterMap::CChapterMap():
	bindShowGlobal( "showglobal" ), bindMenu( "gamemenu" ), bindJournal( "clues" ),
	bV12WorldCmdFlag( false )	// v1.2 ctor @0x5a7560
{
	// retail ctor @0x1a6a60 tail: `mov byte ptr [esi + 0x4d], bl` with bl = 0 (xor ebx,ebx) --
	// i.e. CMissionBase::bRenderWorld = false (base layout: bPause@0x4c, bRenderWorld@0x4d). The
	// chapter map paints a 2D map, never the world. Confirmed by RAW DISASM: the Ghidra decomp
	// renders this store as `*(undefined1 *)((int)&this->_padding_ + 1) = 0`, i.e. byte `this+1`
	// (inside the vftable pointer) -- one of Ghidra's known `this`-rendering lies; the instruction
	// is unambiguous.
	// NOTE: retail's CChapterMap has NO bindMenu -- its ProcessEvent @0x1a6550 chains
	// CMissionBase::ProcessEvent @0x1a2010 first, and the base owns the game-menu bind. This fork's
	// CMissionBase does not carry the mission pump / exit-save-load binds yet (documented deferred
	// behaviour leg in iMission.h), so bindMenu stays here as the fork's stand-in until that lands;
	// dropping it now would silently cost the chapter map its in-game-menu key.
	bRenderWorld = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterMap::UpdateChapterDifficulty()
{
	int nDifficulty = 0;
	if ( pGlobalGame->pScenarioTracker->IsScenarioAvailable() )
	{
		int nZoneCount = 0;
		pChapterInfo.Refresh();
		const vector<SChapterSector> &sectors = pChapterInfo->GetValue()->sectorsSet;
		for ( vector<SChapterSector>::const_iterator i = sectors.begin(); i != sectors.end(); ++i )
			if ( i->eType == ZONE )
			{
				CPtr<NScenario::CScenarioZone> pZone = 
					pGlobalGame->pScenarioTracker->GetZoneByDBZone( NDb::GetDBScenarioZone( i->nTemplate ) );
				if ( IsValid( pZone ) )
				{
					nDifficulty += pZone->GetDifficulty();
					++nZoneCount;
				}
			}
		//
		if ( nZoneCount > 0 )
			nDifficulty = Float2Int( float( nDifficulty ) / nZoneCount );
	}
	pGlobalGame->nCurrentChapterDifficulty = nDifficulty;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChapterMap::Initialize( NRPG::CGlobalGame *_pGame )
{
	pGlobalGame = _pGame;

	pChapterMap = NDb::GetChapterMap( pGlobalGame->nChapterMapID );
	pChapterInfo = shareChapterInfo.Get( pGlobalGame->nChapterMapID );
	if ( !IsValid( pChapterMap ) || !IsValid( pChapterInfo ) )
		return false;

	// Retail v1.2 0x5a770b..0x5a79a6: even this 2D screen owns a world,
	// players, render/sound scene and cameras. CMissionBase's retail pump
	// dereferences them after load; serializing nulls here made dev saves crash.
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
	CommandState( new CStateEmpty ); // retail base intentionally consumes/discards it
	pScene = NGScene::CreateNewView();
	pSoundScene = NSound::CreateSoundScene( NDb::GetTMusic( 15 ), 0, pWorld->GetAimTime() );
	pRender = NRender::CreateRenderGame( pWorld, pScene, pSoundScene );
	pCamera = CreateCamera( CAMERA_PC );
	ICamera::SCameraLimits limits;
	limits.bMovie = true;
	pCamera->SetLimits( limits );
	pCamera->SetLock( true );

	UpdateChapterDifficulty();

#ifdef _MAPEDIT
	pCursor = NUI::ICursor::CreateEditorCursor();
#else
	pCursor = NUI::ICursor::Create();
#endif

	pInterface = new NUI::CInterface( pCursor, pSoundScene );

	pChapterMapUI = new NUI::CChapterMapUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "chaptermapUI" ), this );
	NUI::LoadTemplate( pChapterMapUI, NDb::GetUIContainer( 147 ) );
	pChapterMapUI->ShowWindow( NUI::SWTYPE_SHOW );
	PushDesktop( pChapterMapUI );
	pWorld->RunPostInitScript( pChapterMap->pScript );
	sMinFrameTime = 5;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CChapterMap* CChapterMap::GetChapterMap() const
{
	return pChapterMap;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPtrFuncBase<CChapterInfo>* CChapterMap::GetChapterInfo() const
{
	return pChapterInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChapterMap::ProcessEvent( const NInput::SEvent &sEvent )
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

	if ( bindJournal.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICClues( pGlobalGame ) ); 
		return true;
	}
	else if ( bindShowGlobal.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICShowGlobal( pGlobalGame ) ); 
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail chapter dispatch (0x5a7140) plus its CMissionBase handlers. Drain the
// world's script queue before drawing; the Jan03 2D-only loop never did so.
void CChapterMap::ProcessWorldCommands()
{
	while ( true )
	{
		CPtr<NWorld::CUICmd> pCmd = pWorld->GetUICommand();
		if ( !IsValid( pCmd ) )
			return;
		if ( !GetDesktop()->IsValidCommand( pCmd ) )
			continue;
		if ( ExecWorldBeginZoneCommand( pCmd ) || ExecWorldSoundCommand( pCmd ) )
			continue;
		if ( CDynamicCast<NWorld::CUICmdLoadTemplate> pLoad = pCmd )
		{
			NMainLoop::CommandWithAutoSave( NStr::ToAscii( NUI::GetDBString( 20243 ).c_str() ),
				new CICBeginMission( (NScenario::CScenarioZone*)0, pLoad->nTemplateID, vector<string>(), pGlobalGame ) );
		}
		else if ( CDynamicCast<NWorld::CUICmdShowTeamMng> pTeam = pCmd )
			NMainLoop::Command( new CICTeamMngMenu( pActivePlayer->GetGlobalPlayer(), this, pTeam->GetID() ) );
		else if ( CDynamicCast<NWorld::CUICmdEnableFeature> pFeature = pCmd )
		{
			if ( pFeature->nFeature == 0 )
				bV12WorldCmdFlag = true;
		}
		else if ( CDynamicCast<NWorld::CUICmdShowHint> pHint = pCmd )
		{
			if ( NGlobal::GetVar( "ui_showhints" ).GetFloat() == 1.0f || bTutorialMode )
				NMainLoop::Command( new CICShowHint( this, pHint->GetID(), pHint->pHint, pGlobalGame ) );
			else
				DoEvent( new NWorld::CCmdInterfaceEvent( pHint->GetID() ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterMap::Step()
{
	// The chapter shares retail's mission clocks/world update even though it
	// renders only 2D. This also advances Sleep and WaitForUI in chapter scripts.
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
		GetDesktop()->UpdateDesktop( GetGameTime() );
		pInterface->UpdateCursor();
		pInterface->Step( GetUITime() );
		RenderFrame( GetUITime() );
	}
	else
		pRender->ResetTiming();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterMap::RenderFrame( const STime &sTime )
{
	NGScene::ClearScreen( CVec3(0.5f, 0.5f, 0.5f ) );
	pInterface->Draw( sTime );
	NGScene::Flip();
	MarkNewDGFrame();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICChapterMap
////////////////////////////////////////////////////////////////////////////////////////////////////
CICBeginChapter::CICBeginChapter( int _nTemplateID, NRPG::CGlobalGame *_pGlobalGame ):
	nTemplateID( _nTemplateID ), pGlobalGame( _pGlobalGame )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICBeginChapter::Exec()
{
	pGlobalGame->bChapterMapSet = true;
	pGlobalGame->nChapterMapID = nTemplateID;

	CDGPtr<CPtrFuncBase<CChapterInfo> > pChapterInfo = shareChapterInfo.Get( nTemplateID );
	pChapterInfo.Refresh();
	pGlobalGame->vChapterPos = pChapterInfo->GetValue()->vDeployPos;

	CChapterMap *pRes = new CChapterMap();
	if ( !pRes->Initialize( pGlobalGame ) )
	{
		ASSERT( 0 );
		csSystem << CC_RED << L"ERROR: Can't create chapter. Check DataBase!" << endl;
		return;
	}

	SetInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICChapterMap
////////////////////////////////////////////////////////////////////////////////////////////////////
CICContinueChapter::CICContinueChapter( NRPG::CGlobalGame *_pGlobalGame ):
	pGlobalGame( _pGlobalGame )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICContinueChapter::Exec()
{
	if ( !pGlobalGame->bChapterMapSet )
	{
		csSystem << CC_RED << L"ERROR: Can't continue chapter! No chapters set!" << endl;
		return;
	}
	//
	pGlobalGame->HealOnLeaveZone();
	pGlobalGame->UpdateScenarioOnLeaveZone();
	pGlobalGame->UpdateMedalsOnLeaveZone();
	//
	CChapterMap *pRes = new CChapterMap();
	if ( !pRes->Initialize( pGlobalGame ) )
	{
		ASSERT( 0 );
		csSystem << CC_RED << L"ERROR: Can't create chapter. Check DataBase!" << endl;
		return;
	}

	SetInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB2301230, CChapterMap )
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandStartChapter( const string &szID, const vector<wstring> &paramsSet, void *pContext );
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(iChapterMap)
	REGISTER_CMD( "chapter", CommandStartChapter )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandStartChapter( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.empty() )
	{
		csSystem << "usage:" << szID << "#chapter" << endl;
		return;
	}

	int nTemp = wcstol( paramsSet.front().data(), 0, 10 );
	csSystem << CC_BLUE << "Loading chapter ( template " << nTemp << " ) ..." << endl;
	CPtr<NRPG::CGlobalGame> pGlobalGame = NRPG::CreateGlobalGame();
	pGlobalGame->players.push_back( NRPG::CreateGlobalPlayer() );
	NMainLoop::Command( new CICBeginChapter( nTemp, pGlobalGame ) );
}
