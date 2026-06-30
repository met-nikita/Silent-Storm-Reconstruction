#include "StdAfx.h"
#include "Transform.h"
#include "GView.h"
#include "G2DView.h"
#include "GSceneUtils.h"
#include "wInterface.h"        // NWorld::IWorld / IPlayer / CCommander (iRenderWorld.h members)
#include "Sound.h"
#include "RWGame.h"            // NRender::IRenderGame (iRenderWorld.h member)
#include "RWSound.h"           // NRender::IRenderSound (iRenderWorld.h member)
#include "RPGGame.h"
#include "RPGGlobal.h"
#include "Interface.h"
#include "iMain.h"
#include "iRenderWorld.h"      // NGame::CRenderBaseInterface
#include "iScriptScene.h"
#include "UIInterface.h"       // NUI::LoadTemplate
#include "UIEvents.h"          // EVENT_TEMPLATELOADCOMPLETE
#include "iDesktopWindow.h"    // NUI::CDesktopWindow (CScriptSceneUI base) + GetUIWindow<>
#include "Camera.h"            // ICamera::SCameraPos
#include "..\Input\Bind.h"     // NInput::CBind / SEvent / SetSection
#include "..\MiscDll\Commands.h"   // NGlobal::RegisterCmd / REGISTER_CMD
#include "..\MiscDll\LogStream.h"  // csSystem / endl
#include "..\DBFormat\DataCamera.h"    // NDb::GetDBCamera / CDBCamera
#include "..\DBFormat\DataFormat.h"    // NDb::GetUIContainer / CUIContainer
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  iScriptScene  --  the engine's "scripted scene" (cut-scene style menu rendered over the live
//  world).  Reconstructed from the release module .\release\iScriptScene.obj (absent from this
//  predecessor tree -- the "scenetemplate" console command did not exist here).  CScriptSceneInterface
//  is a CRenderBaseInterface twin of CCredits (3D world backdrop + a CDesktopWindow UI), opened by
//  the CICScriptScene main-loop command and dismissed by the "cancel" bind.   VA = RVA + 0x400000.
//
//  Release-base note: the release CScriptSceneInterface derives from CRenderBaseInterface : CMissionBase
//  (whose Step renders the backdrop and whose vt[0x50] pushes the camera setup).  This predecessor's
//  CRenderBaseInterface has no CMissionBase, so -- exactly like CCredits -- we reproduce the behaviour
//  with a Step() that sets the full-screen camera rect + RenderFrame, and apply the DB camera through
//  GetCamera()->SetPlacement( ICamera::SCameraPos(...) ) instead of the release's raw 8-float view push.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_SCRIPTSCENE_CAMERA    = 0x97,    // @Initialize mov ecx,0x97  -- NDb::GetDBCamera( 0x97 )
	N_SCRIPTSCENE_CONTAINER = 0x17c;   // @Initialize mov ecx,0x17c -- NDb::GetUIContainer( 0x17c )
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScriptSceneUI  --  the scripted-scene desktop window (saveload id 0xB3204140).
// Adds its OWN "view" client window, distinct from (and shadowing) the base
// CDesktopWindow::pClientWindow -- GetClientWindow() returns this DERIVED slot, and ProcessMessage
// captures it on the loader's template-load-complete notify.   ctor @0x238180.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScriptSceneUI: public CDesktopWindow
{
	OBJECT_BASIC_METHODS(CScriptSceneUI);
private:
	ZDATA_(CDesktopWindow)
	CObj<CWindow> pClientWindow;     // the "view" child window (derived slot, shadows the base)
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDesktopWindow*)this); f.Add(2,&pClientWindow); return 0; }

public:
	CScriptSceneUI() {}
	CScriptSceneUI( const SWindowInfo &sInfo );          // @0x238180

	CWindow* GetClientWindow() const;                    // @0x237fd0  (DERIVED slot)
	bool ProcessMessage( const SEvent &sEvent );         // @0x2383c0
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScriptSceneUI @0x238180 -- the base ctor builds the desktop; the derived "view" slot starts null
// (CObj default-constructs to null).
////////////////////////////////////////////////////////////////////////////////////////////////////
CScriptSceneUI::CScriptSceneUI( const SWindowInfo &sInfo ):
	CDesktopWindow( sInfo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetClientWindow @0x237fd0 -- returns the DERIVED "view" sub-window (not the base slot).
////////////////////////////////////////////////////////////////////////////////////////////////////
CWindow* CScriptSceneUI::GetClientWindow() const
{
	return pClientWindow;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ProcessMessage @0x2383c0 -- on the loader's template-load-complete notify, capture the freshly
// built "view" child into the derived pClientWindow, then always defer to the base.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScriptSceneUI::ProcessMessage( const SEvent &sEvent )
{
	if ( sEvent.nEvent == EVENT_TEMPLATELOADCOMPLETE )
		pClientWindow = GetUIWindow<CWindow>( this, "view" );

	return CDesktopWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NUI
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScriptSceneInterface  --  the scripted-scene interface (3D backdrop + desktop UI).
// saveload id 0xB3204141.   default ctor @0x238460  /  copy ctor @0x2388c0 (compiler-generated, via
// OBJECT_BASIC_METHODS::MakeCopy) / Initialize @0x2381b0 / ProcessEvent @0x237fe0.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScriptSceneInterface: public CRenderBaseInterface
{
	OBJECT_BASIC_METHODS(CScriptSceneInterface);
private:
	NInput::CBind bindCancel;                  // "cancel" -> leave the scene

	// Runtime-only: the scene desktop UI (parented to GetInterface(), which renders it); rebuilt by
	// Initialize and NOT serialized (matches CCredits's pCreditsUI handling).
	CObj<NUI::CScriptSceneUI> pUI;

	ZDATA_(CRenderBaseInterface)
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CRenderBaseInterface*)this); return 0; }

public:
	CScriptSceneInterface();                                 // @0x238460

	void Initialize( int nID );                              // @0x2381b0

	void Step();
	bool ProcessEvent( const NInput::SEvent &sEvent );       // @0x237fe0
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CScriptSceneInterface::CScriptSceneInterface():
	bindCancel( "cancel" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialize @0x2381b0 -- build the 3D world from the scene template id, place the DB camera, then
// build + load + show the scene desktop UI.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScriptSceneInterface::Initialize( int nID )
{
	CRenderBaseInterface::Initialize( nID );      // builds the 3D world from the scene template id

	CPtr<NDb::CDBCamera> pDBCamera = NDb::GetDBCamera( N_SCRIPTSCENE_CAMERA );
	ICamera::SCameraPos sCameraPos( pDBCamera->vAnchor, pDBCamera->fDistance, pDBCamera->fPitch, pDBCamera->fYaw, pDBCamera->fRoll, pDBCamera->fFOV );
	GetCamera()->SetPlacement( sCameraPos );

	pUI = new NUI::CScriptSceneUI( NUI::SWindowInfo( GetInterface(), NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "mainmenuUI", NUI::STYLE_VISIBLE | NUI::STYLE_ENABLED ) );
	NUI::LoadTemplate( pUI, NDb::GetUIContainer( N_SCRIPTSCENE_CONTAINER ) );
	pUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Step -- render the 3D backdrop full-screen (the release relies on CMissionBase::Step; this tree
// reproduces the CCredits pattern: set the camera screen-rect 0,0,1,1 each rendered frame).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScriptSceneInterface::Step()
{
	CRenderBaseInterface::Step();

	if ( CanRender() )
	{
		GetCamera()->SetScreenRect( CTRect<float>( 0.0f, 0.0f, 1.0f, 1.0f ) );
		RenderFrame( GetTime(), GetCamera() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ProcessEvent @0x237fe0 -- route input to the "menu" section; the base handler gets first crack, then
// the "cancel" bind posts the exit-modal command and consumes the event.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScriptSceneInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	NInput::SetSection( "menu" );

	if ( CRenderBaseInterface::ProcessEvent( sEvent ) )
		return true;

	if ( bindCancel.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NMainLoop::CICExitModal() );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICScriptScene::Exec @0x2384e0 -- tear the whole interface stack down, build + initialise the
// scripted-scene interface, and make it the sole interface on the stack.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICScriptScene::Exec()
{
	ResetStack();

	CScriptSceneInterface *pRes = new CScriptSceneInterface;
	pRes->Initialize( nID );
	SetInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// StartScriptScene @0x2380e0 -- the "scenetemplate #template" console command handler.  Parses the
// first argument as a scene id and pushes the CICScriptScene command; logs a usage line otherwise.
////////////////////////////////////////////////////////////////////////////////////////////////////
static void StartScriptScene( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.empty() )
	{
		csSystem << "usage: " << szID << " #template " << endl;
		return;
	}

	NMainLoop::Command( new CICScriptScene( _wtol( paramsSet[0].c_str() ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NGame
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB3204140, CScriptSceneUI );
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB3204141, CScriptSceneInterface );
////////////////////////////////////////////////////////////////////////////////////////////////////
// iScripSceneInit::iScripSceneInit @0x238590 -- file-scope registrar for the "scenetemplate" command
// (START_REGISTER(iScripScene) expands to `struct iScripSceneInit { iScripSceneInit() {...} }` --
// matching the release symbol name, with its release typo "ScripScene").
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(iScripScene)
	REGISTER_CMD( "scenetemplate", StartScriptScene )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
