#include "StdAfx.h"
#include "Gfx.h"
#include "iMain.h"
#include "G2DView.h"
#include "RPGGlobal.h"
#include "..\MiscDll\Commands.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "..\DBFormat\DataScenario.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iObjectivesMenu.h"
#include "UIWrap.h"
#include "scScenarioTracker.h"
#include "scFlowChartItems.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// LUA convergence (objectives/journal): a modal that RENDERS the scenario goals/tasks the lua
// ScenarioAddGoal / SetGoalComplete / SetTaskComplete family stores (per-zone CScenarioZone::scriptGoals)
// but which the dev otherwise never shows. It loads the REAL objectives UI container 388 -- the orphan
// container the dev game.db carries with the objectives "cancel/scroll/view" layout (same id the retail
// CShowObjectivesInterface uses; the dev never referenced it because the screen was never coded). The
// modal lifecycle mirrors the working sibling iCluesMenu.cpp (build chrome + screenshot backdrop; on close
// submit a CICExitModal). The list is a flat CListView of goal rows, each followed by its task rows, with a
// per-state marker ([+] done / [-] failed / [ ] open).
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static wstring GetStateMark( NScenario::ETaskState eState )
{
	switch( eState )
	{
	case NScenario::TS_COMPLETED:	return L"[+] ";
	case NScenario::TS_FAILED:		return L"[-] ";
	default:						return L"[ ] ";
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjectivesUI -- the objectives list window. Container 388 controls: "cancel" (close), "view" (the
// scrollable list) and "scroll" (its vertical scrollbar).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectivesUI: public CWindow
{
	OBJECT_NOCOPY_METHODS(CObjectivesUI)
private:
	ZDATA_(CWindow)
	CPtr<NRPG::CGlobalGame> pGame;
	////
	CObj<CListView> pList;
	CObj<CScrollWindow<CListView> > pListView;
	CObj<CFlashButton> pCloseButton;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pGame); f.Add(3,&pList); f.Add(4,&pListView); f.Add(5,&pCloseButton); return 0; }

protected:
	void AddRow( const wstring &wsText, int *pCount );
	void GenerateList();

public:
	CObjectivesUI() {}
	CObjectivesUI( const SWindowInfo &sInfo, NRPG::CGlobalGame *_pGame );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectivesUI::CObjectivesUI( const SWindowInfo &sInfo, NRPG::CGlobalGame *_pGame ):
	CWindow( sInfo ), pGame( _pGame )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectivesUI::AddRow( const wstring &wsText, int *pCount )
{
	int &nCount = (*pCount);

	CPtr<CMLText> pText = new CMLText( SWindowInfo( pList, SPoint( 0, 0 ), SPoint( pList->GetSize().x, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ) );
	pText->SetText( wsText );

	SPoint sSize;
	pText->GetRealSize( &sSize );
	sSize.x = pList->GetSize().x;
	pText->SetSize( sSize );

	pList->AddItem( nCount, pText );
	nCount++;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectivesUI::GenerateList()
{
	pList->RemoveAllItems();

	if ( !IsValid( pGame ) || !IsValid( pGame->pScenarioTracker ) )
		return;

	list< CPtr<NScenario::CScenarioZone> > zonesList;
	pGame->pScenarioTracker->GetAvailableZones( &zonesList );

	int nCount = 0;
	for ( list< CPtr<NScenario::CScenarioZone> >::const_iterator iZone = zonesList.begin(); iZone != zonesList.end(); iZone++ )
	{
		if ( !IsValid( *iZone ) )
			continue;

		const vector< CObj<NScenario::CScenarioGoal> > &goals = (*iZone)->GetScriptGoals();
		for ( int g = 0; g < goals.size(); g++ )
		{
			NScenario::CScenarioGoal *pGoal = goals[ g ];
			if ( !IsValid( pGoal ) )
				continue;

			wstring wsName = L"";
			if ( IsValid( pGoal->GetDBGoal() ) && IsValid( pGoal->GetDBGoal()->pName ) )
				wsName = GetDBString( pGoal->GetDBGoal()->pName );

			WCHAR wsBuffer[ 1024 ];
			swprintf( wsBuffer, L"<font face=Courier size=18pt>%s%s", GetStateMark( pGoal->GetState() ).c_str(), wsName.c_str() );
			AddRow( wsBuffer, &nCount );

			// the goal's tasks, indented under it
			const vector< CObj<NScenario::CScenarioTask> > &tasks = pGoal->GetTasks();
			for ( int t = 0; t < tasks.size(); t++ )
			{
				NScenario::CScenarioTask *pTask = tasks[ t ];
				if ( !IsValid( pTask ) )
					continue;

				wstring wsTask = L"";
				if ( IsValid( pTask->GetDBTask() ) && IsValid( pTask->GetDBTask()->pDescription ) )
					wsTask = GetDBString( pTask->GetDBTask()->pDescription );

				swprintf( wsBuffer, L"<font face=Courier size=18pt>\t%s%s", GetStateMark( pTask->GetState() ).c_str(), wsTask.c_str() );
				AddRow( wsBuffer, &nCount );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CObjectivesUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pCloseButton = new CFlashButton( sEvent.pLoader->GetControl( "cancel" ) );

			pListView = new CScrollWindow<CListView>( sEvent.pLoader->GetControl( "view" ) );
			pList = pListView->GetClientWindow();

			GenerateList();
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pListView->SetVScroll( GetUIWindow<CScroll>( this, "scroll" ) );
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
// CObjectivesInterface -- the modal screen (mirrors CCluesInterface; no event id, exit-modal on close)
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectivesInterface: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CObjectivesInterface);
private:
	NInput::CBind bindClose;

	ZDATA
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	CPtr<NRPG::CGlobalGame> pGame;
	////
	CObj<NUI::CWindow> pObjectivesUI;
	CObj<NUI::CScreenShot> pScreenShot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pCursor); f.Add(3,&pInterface); f.Add(4,&pGame); f.Add(5,&pObjectivesUI); f.Add(6,&pScreenShot); return 0; }

public:
	CObjectivesInterface();

	void Initialize( NRPG::CGlobalGame *pGame );

	void Step();
	void OnGetFocus() {}
	bool ProcessEvent( const NInput::SEvent &eEvent );
	void RenderFrame();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectivesInterface::CObjectivesInterface():
	bindClose( "cancel" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectivesInterface::Initialize( NRPG::CGlobalGame *_pGame )
{
	pGame = _pGame;

	pCursor = NUI::ICursor::Create();
	pInterface = new NUI::CInterface( pCursor );

	pObjectivesUI = new NUI::CObjectivesUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "objectives", NUI::STYLE_ENABLED ), pGame );
	NUI::LoadTemplate( pObjectivesUI, NDb::GetUIContainer( 388 ) );	// the real objectives container (cancel/view/scroll)

	pScreenShot = new NUI::CScreenShot( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "objectives", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_BOTTOMMOST ) );
	pScreenShot->SetMode( NUI::CScreenShot::BLACKANDWHITE, CVec4( 0.5f, 0.5f, 0.5f, 1 ) );
	pScreenShot->Generate();

	pObjectivesUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectivesInterface::Step()
{
	MarkNewDGFrame();
	if ( CanRender() )
	{
		pInterface->UpdateCursor();
		pInterface->Step( GetTime() );
		RenderFrame();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CObjectivesInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	pCursor->ProcessEvent( sEvent );

	if ( pInterface->ProcessEvent( sEvent ) )
		return true;

	if ( bindClose.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NMainLoop::CICExitModal() );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectivesInterface::RenderFrame()
{
	NGScene::ClearScreen( CVec3(0.5f, 0.5f, 0.5f ) );

	pInterface->Draw( GetTime() );

	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICObjectives
////////////////////////////////////////////////////////////////////////////////////////////////////
CICObjectives::CICObjectives( NRPG::CGlobalGame *_pGame ):
	pGame( _pGame )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICObjectives::Exec()
{
	CObjectivesInterface *pRes = new CObjectivesInterface();
	pRes->Initialize( pGame );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0820134, CObjectivesUI );		// LUA convergence (fresh id; objectives journal)
// NOTE: CScrollWindow<CListView> (0xB0820133) is ALREADY registered by iCluesMenu.cpp -- not repeated here.
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB0820136, CObjectivesInterface );	// LUA convergence (fresh id)
