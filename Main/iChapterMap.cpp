#include "StdAfx.h"
#include "iMain.h"
#include "G2DView.h"
#include "RPGGlobal.h"
#include "Sound.h"
#include "..\Input\Bind.h"
#include "ChapterInfo.h"
#include "iInterMission.h"
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
class CChapterMap: public IChapterMap
{
	OBJECT_NOCOPY_METHODS(CChapterMap);
private:
	NInput::CBind bindShowGlobal;
	NInput::CBind bindMenu, bindJournal;
	ZDATA
	CPtr<NRPG::CGlobalGame> pGame;
	//// chapter
	CDBPtr<NDb::CChapterMap> pChapterMap;
	CDGPtr<CPtrFuncBase<CChapterInfo> > pChapterInfo;
	//// interface
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	CObj<NUI::CChapterMapUI> pChapterMapUI;
	//// sound
	CObj<NSound::ISoundScene> pSoundScene;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pGame); f.Add(3,&pChapterMap); f.Add(4,&pChapterInfo); f.Add(5,&pCursor); f.Add(6,&pInterface); f.Add(7,&pChapterMapUI); f.Add(8,&pSoundScene); return 0; }

	void UpdateChapterDifficulty();

protected:
	void RenderFrame( const STime &sTime );

public:
	CChapterMap();

	bool Initialize( NRPG::CGlobalGame* pGame );

	NUI::ICursor* GetCursor() const;
	NUI::CInterface* GetInterface() const;

	NRPG::CGlobalGame* GetGlobalGame() const;
	NRPG::CGlobalPlayer* GetGlobalPlayer() const;
	NSound::ISoundScene* GetSoundScene() const;
	NDb::CChapterMap* GetChapterMap() const;
	CPtrFuncBase<CChapterInfo>* GetChapterInfo() const;

	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &sEvent );
	void Step();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CChapterMap::CChapterMap():
	bindShowGlobal( "showglobal" ), bindMenu( "menu" ), bindJournal( "clues" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterMap::UpdateChapterDifficulty()
{
	int nDifficulty = 0;
	if ( pGame->pScenarioTracker->IsScenarioAvailable() )
	{
		int nZoneCount = 0;
		pChapterInfo.Refresh();
		const vector<SChapterSector> &sectors = pChapterInfo->GetValue()->sectorsSet;
		for ( vector<SChapterSector>::const_iterator i = sectors.begin(); i != sectors.end(); ++i )
			if ( i->eType == ZONE )
			{
				CPtr<NScenario::CScenarioZone> pZone = 
					pGame->pScenarioTracker->GetZoneByDBZone( NDb::GetDBScenarioZone( i->nTemplate ) );
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
	pGame->nCurrentChapterDifficulty = nDifficulty;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChapterMap::Initialize( NRPG::CGlobalGame *_pGame )
{
	pGame = _pGame;

	pChapterMap = NDb::GetChapterMap( pGame->nChapterMapID );
	pChapterInfo = shareChapterInfo.Get( pGame->nChapterMapID );
	if ( !IsValid( pChapterMap ) || !IsValid( pChapterInfo ) )
		return false;

	UpdateChapterDifficulty();

	pSoundScene = NSound::CreateSoundScene( 0 ); // CRAP
#ifdef _MAPEDIT
	pCursor = NUI::ICursor::CreateEditorCursor();
#else
	pCursor = NUI::ICursor::Create();
#endif

	pInterface = new NUI::CInterface( pCursor );

	pChapterMapUI = new NUI::CChapterMapUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "chaptermapUI" ), this );
	NUI::LoadTemplate( pChapterMapUI, NDb::GetUIContainer( 147 ) );
	pChapterMapUI->ShowWindow( NUI::SWTYPE_SHOW );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::ICursor* CChapterMap::GetCursor() const
{
	return pCursor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NUI::CInterface* CChapterMap::GetInterface() const
{
	return pInterface;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::CGlobalGame* CChapterMap::GetGlobalGame() const
{
	return pGame;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::CGlobalPlayer* CChapterMap::GetGlobalPlayer() const
{
	ASSERT( pGame->players.size() == 1 );
	return pGame->players.front();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NSound::ISoundScene* CChapterMap::GetSoundScene() const
{
	return pSoundScene;
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
void CChapterMap::OnGetFocus()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChapterMap::ProcessEvent( const NInput::SEvent &sEvent )
{
	NInput::SetSection( "game" );

	pCursor->ProcessEvent( sEvent );

	if ( pInterface->ProcessEvent( sEvent ) )
		return true;

	if ( bindMenu.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICInGameMenu( pGame->players.front() ) ); 
		return true;
	}
	else if ( bindJournal.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICClues( pGame ) ); 
		return true;
	}
	else if ( bindShowGlobal.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new CICShowGlobal( pGame ) ); 
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterMap::Step()
{
	if ( CanRender() )
	{
		pInterface->UpdateCursor();
		pInterface->Step( GetTime() );
		RenderFrame( GetTime() );
	}
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
