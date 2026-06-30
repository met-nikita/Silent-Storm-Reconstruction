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
#include "UIBaseCtrls.h"
#include "UICommCtrls.h"
#include "UIWrap.h"
#include "iMission.h"
#include "scScenarioTracker.h"
#include "scFlowChartItems.h"
#include "iShowObjectives.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release-shape iObjectivesmenu (ADDITIVE PARITY SURFACE -- see iShowObjectives.h banner). Reconstructed
// over the REAL dev engine types, mirroring the proven idioms in the divergent iObjectivesMenu.cpp.
// Nothing constructs these classes, so the build is behaviour-neutral.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NScenario
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// NScenario::SGoalDescription::SGoalDescription( const SGoalDescription& )   @0x220370
//   Compiler-generated member-wise copy: the CDBPtr AddRef rides CDBPtr's copy ctor; the vector copies.
////////////////////////////////////////////////////////////////////////////////////////////////////
SGoalDescription::SGoalDescription( const SGoalDescription &src )
	: pString( src.pString ), state( src.state ), tasks( src.tasks )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NScenario::GetGoalsFromZone -- aggregate a zone's runtime script goals/tasks into the release value
// model (adapter over GetScriptGoals/CScenarioGoal/CScenarioTask, exactly the data the divergent
// CObjectivesUI::GenerateList already walks).
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetGoalsFromZone( CScenarioZone *pZone, list< SGoalDescription > *pGoals )
{
	if ( !IsValid( pZone ) || pGoals == 0 )
		return;

	const vector< CObj<CScenarioGoal> > &goals = pZone->GetScriptGoals();
	for ( int g = 0; g < goals.size(); ++g )
	{
		CScenarioGoal *pGoal = goals[ g ];
		if ( !IsValid( pGoal ) )
			continue;

		SGoalDescription sGoal;
		if ( IsValid( pGoal->GetDBGoal() ) )
			sGoal.pString = pGoal->GetDBGoal()->pName;
		sGoal.state = TaskStateToScenario( pGoal->GetState() );

		const vector< CObj<CScenarioTask> > &tasks = pGoal->GetTasks();
		for ( int t = 0; t < tasks.size(); ++t )
		{
			CScenarioTask *pTask = tasks[ t ];
			if ( !IsValid( pTask ) )
				continue;

			STaskDescription sTask;
			if ( IsValid( pTask->GetDBTask() ) )
				sTask.pString = pTask->GetDBTask()->pDescription;
			sTask.state = TaskStateToScenario( pTask->GetState() );
			sGoal.tasks.push_back( sTask );
		}

		pGoals->push_back( sGoal );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NScenario
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CClueLine -- one goal row: type/state/background images (weak child refs) + a description text widget.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CClueLine: public CWindow
{
	OBJECT_NOCOPY_METHODS(CClueLine)
private:
	CPtr<NGame::IMission>		pMission;
	NScenario::SGoalDescription	goal;
	CPtr<CImage>				pType;
	CPtr<CImage>				pState;
	CPtr<CImage>				pBackgroundImage;
	CObj<CText>					pDescription;
	bool						bDoubleLine;

public:
	CClueLine(): bDoubleLine( false ) {}		// @0x220540  (default ctor: null members, empty goal)
	CClueLine( const SWindowInfo &sInfo, NGame::IMission *pMission,
			   const NScenario::SGoalDescription &goal, bool bDoubleLine );	// @0x21eaf0

	bool ProcessMessage( const SEvent &sEvent );	// @0x21eb90
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// NUI::CClueLine::CClueLine( SWindowInfo&, IMission*, SGoalDescription&, bool )   @0x21eaf0
//   CWindow base from the SWindowInfo; AddRef the mission (CPtr op=); copy the goal value; null widgets.
////////////////////////////////////////////////////////////////////////////////////////////////////
CClueLine::CClueLine( const SWindowInfo &sInfo, NGame::IMission *pMission,
					  const NScenario::SGoalDescription &goal, bool bDoubleLine )
	: CWindow( sInfo ), pMission( pMission ), goal( goal ), bDoubleLine( bDoubleLine )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NUI::CClueLine::ProcessMessage   @0x21eb90
//   TEMPLATELOAD       : build the description CText from the loader "text" control.
//   TEMPLATELOADCOMPLETE: bind the named child images, push the goal text, branch on goal.state.
// ORIGINAL DATA LOST: the per-state CClueLine UI-texture record ids were zeroed in the decode (only the
// sibling CTaskLine ids survived), so the COMPLETED/FAILED type+state SetImage selections cannot be
// reconstructed. The recovered behaviour kept here is the active/unknown branch hiding the state icon.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CClueLine::ProcessMessage( const SEvent &sEvent )
{
	switch ( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		pDescription = new CText( sEvent.pLoader->GetControl( "text" ) );
		break;

	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pType            = GetUIWindow<CImage>( this, "type" );
			pState           = GetUIWindow<CImage>( this, "state" );
			pBackgroundImage = GetUIWindow<CImage>( this, "background" );

			if ( IsValid( pDescription ) )
			{
				NDb::CString *pStr = goal.pString;
				pDescription->SetText( IsValid( pStr ) ? GetDBString( pStr ) : wstring( L"" ) );
			}

			switch ( goal.state )
			{
			case NScenario::STS_COMPLETED:
			case NScenario::STS_FAILED:
				// per-state type/state UI-texture ids LOST in decode -- SetImage selections elided.
				break;
			default:	// STS_UNKNOWN / active: hide the state icon (recovered behaviour).
				if ( IsValid( pState ) )
					pState->SetStyle( STYLE_VISIBLE, false );
				break;
			}
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskLine -- one task row: state + background images (weak child refs) + a numbered description text.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTaskLine: public CWindow
{
	OBJECT_NOCOPY_METHODS(CTaskLine)
private:
	int							nNumber;
	CPtr<NGame::IMission>		pMission;
	NScenario::STaskDescription	task;
	CPtr<CImage>				pState;
	CPtr<CImage>				pBackgroundImage;
	CObj<CText>					pDescription;
	bool						bDoubleLine;

public:
	CTaskLine(): nNumber( 0 ), bDoubleLine( false ) {}
	CTaskLine( const SWindowInfo &sInfo, NGame::IMission *pMission,
			   const NScenario::STaskDescription &task, int nNumber, bool bDoubleLine );	// @0x21ea80

	bool ProcessMessage( const SEvent &sEvent );	// @0x21f020
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// NUI::CTaskLine::CTaskLine   @0x21ea80
////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskLine::CTaskLine( const SWindowInfo &sInfo, NGame::IMission *pMission,
					  const NScenario::STaskDescription &task, int nNumber, bool bDoubleLine )
	: CWindow( sInfo ), nNumber( nNumber ), pMission( pMission ), task( task ), bDoubleLine( bDoubleLine )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NUI::CTaskLine::ProcessMessage   @0x21f020
//   TEMPLATELOAD       : build the description CText from the loader "text" control.
//   TEMPLATELOADCOMPLETE: bind "state"/"background" child images, push the "<n>. <desc>" text, then
//   select the per-(state,double-line) UI textures. The ids/deltas were RECOVERED from the disassembly
//   (0x61f1b2/0x61f222/0x61f292): UNKNOWN bg 0x2ba(+0x105 dbl), COMPLETED state 0x2bc / bg 0x33c(+0x84),
//   FAILED state 0x2be / bg 0x33d(+0x84).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskLine::ProcessMessage( const SEvent &sEvent )
{
	switch ( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		pDescription = new CText( sEvent.pLoader->GetControl( "text" ) );
		break;

	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pState           = GetUIWindow<CImage>( this, "state" );
			pBackgroundImage = GetUIWindow<CImage>( this, "background" );

			if ( IsValid( pDescription ) )
			{
				WCHAR wsBuffer[ 1024 ];
				NDb::CString *pStr = task.pString;
				swprintf( wsBuffer, L"%d. %s", nNumber, IsValid( pStr ) ? GetDBString( pStr ).c_str() : L"" );
				pDescription->SetText( wsBuffer );
			}

			const int nDbl = bDoubleLine ? 1 : 0;
			switch ( task.state )
			{
			case NScenario::STS_COMPLETED:
				if ( IsValid( pState ) )			pState->SetImage( NDb::GetUITexture( 0x2bc ) );
				if ( IsValid( pBackgroundImage ) )	pBackgroundImage->SetImage( NDb::GetUITexture( 0x33c + nDbl * 0x84 ) );
				break;
			case NScenario::STS_FAILED:
				if ( IsValid( pState ) )			pState->SetImage( NDb::GetUITexture( 0x2be ) );
				if ( IsValid( pBackgroundImage ) )	pBackgroundImage->SetImage( NDb::GetUITexture( 0x33d + nDbl * 0x84 ) );
				break;
			default:	// STS_UNKNOWN: hide the state icon, paint the generic background.
				if ( IsValid( pState ) )			pState->SetStyle( STYLE_VISIBLE, false );
				if ( IsValid( pBackgroundImage ) )	pBackgroundImage->SetImage( NDb::GetUITexture( 0x2ba + nDbl * 0x105 ) );
				break;
			}
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CShowObjectivesUI -- the objectives list window: a scrollable CListView of CClueLine goal rows, each
// followed by its CTaskLine task rows, plus a "cancel" close button.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShowObjectivesUI: public CWindow
{
	OBJECT_NOCOPY_METHODS(CShowObjectivesUI)
private:
	CPtr<NGame::IMission>					pMission;
	CPtr<NScenario::CScenarioZone>			pZone;
	CObj<CListView>							pObjectives;
	CObj<CScrollWindow<CListView> >			pObjectivesView;
	CObj<CFlashButton>						pCloseButton;

public:
	CShowObjectivesUI() {}
	CShowObjectivesUI( const SWindowInfo &sInfo, NGame::IMission *pMission,
					   NScenario::CScenarioZone *pZone );	// @0x21e550

	bool ProcessMessage( const SEvent &sEvent );	// @0x21f3c0
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// NUI::CShowObjectivesUI::CShowObjectivesUI   @0x21e550
//   CWindow base from the SWindowInfo; AddRef the mission + zone (CPtr op=); null the child slots.
////////////////////////////////////////////////////////////////////////////////////////////////////
CShowObjectivesUI::CShowObjectivesUI( const SWindowInfo &sInfo, NGame::IMission *pMission,
									  NScenario::CScenarioZone *pZone )
	: CWindow( sInfo ), pMission( pMission ), pZone( pZone )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NUI::CShowObjectivesUI::ProcessMessage   @0x21f3c0
//   TEMPLATELOAD       : build the close button + scrollable list, then one CClueLine per goal and one
//                        CTaskLine per task from GetGoalsFromZone( pZone ).
//   TEMPLATELOADCOMPLETE: wire the panel's vertical scrollbar.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CShowObjectivesUI::ProcessMessage( const SEvent &sEvent )
{
	switch ( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pCloseButton = new CFlashButton( sEvent.pLoader->GetControl( "cancel" ) );

			pObjectivesView = new CScrollWindow<CListView>( sEvent.pLoader->GetControl( "view" ) );
			pObjectives = pObjectivesView->GetClientWindow();

			list< NScenario::SGoalDescription > goals;
			NScenario::GetGoalsFromZone( pZone, &goals );

			int nCount = 0;
			for ( list< NScenario::SGoalDescription >::const_iterator iGoal = goals.begin(); iGoal != goals.end(); ++iGoal )
			{
				pObjectives->AddItem( nCount++, new CClueLine(
					SWindowInfo( pObjectives, SPoint( 0, 0 ), SPoint( pObjectives->GetSize().x, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ),
					pMission, *iGoal, false ) );

				int nTask = 1;
				const vector< NScenario::STaskDescription > &tasks = iGoal->tasks;
				for ( int t = 0; t < tasks.size(); ++t )
				{
					pObjectives->AddItem( nCount++, new CTaskLine(
						SWindowInfo( pObjectives, SPoint( 0, 0 ), SPoint( pObjectives->GetSize().x, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ),
						pMission, tasks[ t ], nTask++, false ) );
				}
			}
			break;
		}

	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pObjectivesView->SetVScroll( GetUIWindow<CScroll>( this, "scroll" ) );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NUI
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CShowObjectivesInterface -- the modal "show objectives" screen (IInterfaceBase). Sibling of the
// divergent CObjectivesInterface; carries the per-zone payload (mission + zone + reusable screenshot)
// and exits the modal on close (no nEventID). Transient -- not serialized.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShowObjectivesInterface: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CShowObjectivesInterface);
private:
	NInput::CBind						bindClose;
	CPtr<IMission>						pMission;
	CPtr<NScenario::CScenarioZone>		pZone;
	CObj<NUI::ICursor>					pCursor;
	CObj<NUI::CInterface>				pInterface;
	CObj<NUI::CWindow>					pUI;
	CObj<NUI::CScreenShot>				pScreenShot;

public:
	CShowObjectivesInterface(): bindClose( "cancel" ) {}		// @0x21e5b0
	CShowObjectivesInterface( const CShowObjectivesInterface &src );	// @0x2201b0

	void Initialize( IMission *pMission, NScenario::CScenarioZone *pZone, NUI::CScreenShot *pSrcShot );	// @0x21e640

	void Step();										// @0x21e4f0
	void OnGetFocus() {}								// @0x21e380  (no-op)
	bool ProcessEvent( const NInput::SEvent &sEvent );		// @0x21e450
	void RenderFrame();									// @0x21e4c0
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CShowObjectivesInterface::CShowObjectivesInterface( const& )   @0x2201b0
//   Field copy: POD bind + each owned smart pointer (CPtr/CObj op= AddRefs on copy).
////////////////////////////////////////////////////////////////////////////////////////////////////
CShowObjectivesInterface::CShowObjectivesInterface( const CShowObjectivesInterface &src )
	: NMainLoop::IInterfaceBase( src ),
	  bindClose( src.bindClose ),
	  pMission( src.pMission ),
	  pZone( src.pZone ),
	  pCursor( src.pCursor ),
	  pInterface( src.pInterface ),
	  pUI( src.pUI ),
	  pScreenShot( src.pScreenShot )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CShowObjectivesInterface::Initialize   @0x21e640
//   Stash mission+zone, build cursor/widget-interface, the "objectives" screenshot backdrop (reuse the
//   supplied frozen shot if present, else a fresh black-and-white capture), then the objectives window
//   from container 388 (the dev's orphan objectives container -- the retail container id was lost).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowObjectivesInterface::Initialize( IMission *_pMission, NScenario::CScenarioZone *_pZone, NUI::CScreenShot *_pSrcShot )
{
	pMission = _pMission;
	pZone    = _pZone;

	pCursor    = NUI::ICursor::Create();
	pInterface = new NUI::CInterface( pCursor );

	pScreenShot = new NUI::CScreenShot( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "objectives", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_BOTTOMMOST ) );
	if ( IsValid( _pSrcShot ) )
	{
		pScreenShot->SetTexture( _pSrcShot->GetTexture() );		// reuse the supplied frozen backdrop
	}
	else
	{
		pScreenShot->SetMode( NUI::CScreenShot::BLACKANDWHITE, CVec4( 0.5f, 0.5f, 0.5f, 1 ) );
		pScreenShot->Generate();
	}

	pUI = new NUI::CShowObjectivesUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "objectivesUI", NUI::STYLE_ENABLED ), pMission, pZone );
	NUI::LoadTemplate( pUI, NDb::GetUIContainer( 388 ) );
	pUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CShowObjectivesInterface::Step   @0x21e4f0
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowObjectivesInterface::Step()
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
// NGame::CShowObjectivesInterface::ProcessEvent   @0x21e450
//   Cursor first look (ignored); widget interface may consume; else the close bind exits the modal.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CShowObjectivesInterface::ProcessEvent( const NInput::SEvent &sEvent )
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
// NGame::CShowObjectivesInterface::RenderFrame   @0x21e4c0
////////////////////////////////////////////////////////////////////////////////////////////////////
void CShowObjectivesInterface::RenderFrame()
{
	NGScene::ClearScreen( CVec3( 0.5f, 0.5f, 0.5f ) );
	pInterface->Draw( GetTime() );
	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CICShowObjectives::CICShowObjectives( IMission*, CScenarioZone*, CScreenShot* )   @0x21e9c0
////////////////////////////////////////////////////////////////////////////////////////////////////
CICShowObjectives::CICShowObjectives( IMission *_pMission, NScenario::CScenarioZone *_pZone, NUI::CScreenShot *_pScreenShot )
	: pMission( _pMission ), pScreenShot( _pScreenShot ), pZone( _pZone )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CICShowObjectives::CICShowObjectives( const& )   @0x21ffa0
////////////////////////////////////////////////////////////////////////////////////////////////////
CICShowObjectives::CICShowObjectives( const CICShowObjectives &src )
	: NMainLoop::CInterfaceCommand( src ),
	  pMission( src.pMission ),
	  pScreenShot( src.pScreenShot ),
	  pZone( src.pZone )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::CICShowObjectives::Exec   @0x21ea00
//   Guard on the zone (IsValid == not null + not being deleted, the retail byte[pZone+7]&0x80 destroyed
//   check), then build + initialize + push the modal objectives screen.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICShowObjectives::Exec()
{
	if ( !IsValid( pZone ) )
		return;

	CShowObjectivesInterface *pRes = new CShowObjectivesInterface();
	pRes->Initialize( pMission, pZone, pScreenShot );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NGame
