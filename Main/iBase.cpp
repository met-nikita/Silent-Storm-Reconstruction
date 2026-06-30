#include "StdAfx.h"
#include <stdio.h>
#include "Gfx.h"							// NGfx::MakeScreenShot / NGfx::SPixel8888 (framebuffer grab)
#include "GView.h"							// NGScene::IGameView (+ SDrawInfo / Draw / GetScreenRect)
#include "Camera.h"							// ICamera (GetTransform / GetScreenRect)
#include "Transform.h"						// CTransformStack
#include "bmpfile.h"						// WriteBMP( CArray2D<NGfx::SPixel8888>&, const char* )
#include "..\Misc\2Darray.h"				// CArray2D
#include "..\Misc\Geom.h"					// CVec2 / CTRect<float>
#include "..\FileIO\Streams.h"				// CFileStream::TryOpenRead
#include "..\MiscDll\LogStream.h"			// csGame / endl
////////////////////////////////////////////////////////////////////////////////////////////////////
// iBase -- release compiland .\release\iBase.obj, SAFE ADDITIVE SUBSET converged into the dev engine.
//
// iBase.obj's principal content is the release mission base class NGame::CMissionBase (the per-frame
// mission pump: ProcessEvent/Step/InternalStep/ExecWorldCommand/RenderFrame/SetLightMode/CreateCamera/
// GetCamera, the in-game save/load/menu dispatch, plus ~30 trivial virtual accessors). That class does
// NOT exist in the dev tree: the dev keeps the PREDECESSOR MONOLITH `class CMission: public IMission`
// (iMissionInternal.h:68) which reproduces all of CMissionBase-derived behaviour inline. Splitting
// CMissionBase back out would re-parent CMission and migrate its FLAT operator& save format (tags 2..68,
// iMissionInternal.h:208) to a base/derived split -- i.e. it would change the member layout + operator&
// tag set of a LIVE save/load class read from the running game.db. That is a forbidden hard-constraint
// crossing, so the CMissionBase mission-pump core is DEFERRED (already covered, behaviourally, by the dev
// CMission monolith). This file lands only iBase's standalone, behaviour-neutral free-function helpers.
//
// LANDED here (faithful ports against verified dev seams; VA = RVA + 0x400000):
//   NGame::WriteHQShot( CArray2D<NGfx::SPixel8888>* )        @0x1a17a0
//   NGame::MakeHQShot ( ICamera*, NGScene::IGameView* )      @0x1a1c50
//   NGame::MakeHQShot ( CTransformStack*, NGScene::IGameView* ) @0x1a1da0
// These are the developer "HQ screenshot" helpers. They are file-scope free functions with NO live
// consumer in the dev tree (their only release caller is CMissionBase::ProcessEvent's screenshot-on-menu
// wiring, which lives in the deferred core), so landing them changes zero runtime behaviour -- it gives
// the iBase compiland's helper source a real dev-tree home and keeps the build GREEN.
//
// DELIBERATE DEVIATION (documented; the dev render stack differs from the MSVC7.1 release):
//   * The release MakeHQShot rendered the scene OFF-SCREEN straight into a CPU pixel buffer through an
//     IGameView render-to-image slot (release vtbl +0xc0). That slot is ABSENT from the dev IGameView,
//     which exposes only the on-screen Draw( const SDrawInfo& ) (GView.h:128). The faithful-INTENT dev
//     path therefore Draw()s the view and reads the framebuffer back with NGfx::MakeScreenShot(...) --
//     the identical idiom the dev itself uses for the in-game screenshot key (iMain.cpp:84). Net effect
//     is behaviour-equivalent (a .bmp of the rendered scene); the only loss is the release's
//     supersampled "HQ" resolution, which has no dev seam. Both overloads carry this deviation.
//
// NOT landed (and WHY -- so the deferral is auditable):
//   * NGame::iBaseInit::iBaseInit @0x1a4610 registered the cvars "cheat_showall" (-> bCheatShowAll) and
//     "ui_followcamera" (-> bCameraShowEnemyActions). BOTH are ALREADY registered by the dev inline --
//     "cheat_showall" at iMission.cpp:2934 and "ui_followcamera" at iMissionExec.cpp:411 -- so a second
//     registrar would DOUBLE-REGISTER the console vars and risk the running game. Intentionally omitted.
//   * CPtrBase<NGScene::CScreenshotTexture,CObjectBase::SRefO>::AddRef @0x1a4550 and
//     NDatabase::GetTable<NDb::CTAmbientLight> @0x1a4b50 are COMDAT/template instances the dev compiler
//     already emits from existing CObj/CPtr and NDatabase usage; no source artifact is needed.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::WriteHQShot @0x1a17a0
//
// Find the first free "hq%d.bmp" filename (probe hq0.bmp, hq1.bmp, ... while each already exists), dump
// the pixel buffer there with WriteBMP, then log "HQScreenShot created". The release probed existence by
// opening each candidate for read (CFileStream::TryOpenRead, which SEH-catches the open miss and returns
// false) and advanced while the open succeeded; a fresh CFileStream is scoped per probe so each handle is
// closed (its dtor CloseFile()s) before the next probe -- exactly the release's construct/destruct loop.
////////////////////////////////////////////////////////////////////////////////////////////////////
void WriteHQShot( const CArray2D<NGfx::SPixel8888> &image )
{
	char szName[100];
	int n = 0;
	for ( ;; )
	{
		sprintf( szName, "hq%d.bmp", n );
		CFileStream probe;
		if ( !probe.TryOpenRead( szName ) )
			break;
		++n;
	}
	WriteBMP( image, szName );
	csGame << "HQScreenShot created" << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::MakeHQShot( ICamera*, NGScene::IGameView* ) @0x1a1c50
//
// Build the draw description from a camera + view, render it, and dump a HQ screenshot:
//   1. ask the view for the target screen rect it draws into          (release view vtbl +0x70)
//   2. have the camera build a CTransformStack for that rect          (release cam  vtbl +0x20)
//   3. read the camera's own draw rectangle (CTRect<float>)           (release cam  vtbl +0x24)
//   4. fill SDrawInfo { pTS, vOrigin=(minx,miny), vSize=(w,h) }  (clear-colour/overlay = SDrawInfo
//      defaults, which already match the release's 0,0,0 / false / use-default-clear)
//   5. render the view + read the framebuffer back, then WriteHQShot  (see DELIBERATE DEVIATION above)
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeHQShot( ICamera *pCamera, NGScene::IGameView *pView )
{
	CVec2 vScreenSize = pView->GetScreenRect();

	CTransformStack sTransform;
	pCamera->GetTransform( &sTransform, vScreenSize );

	const CTRect<float> &rect = pCamera->GetScreenRect();

	NGScene::IGameView::SDrawInfo drawInfo;
	drawInfo.pTS = &sTransform;
	drawInfo.vOrigin.x = rect.minx;
	drawInfo.vOrigin.y = rect.miny;
	drawInfo.vSize.x = rect.maxx - rect.minx;
	drawInfo.vSize.y = rect.maxy - rect.miny;

	pView->Draw( drawInfo );

	CArray2D<NGfx::SPixel8888> pixels;
	NGfx::MakeScreenShot( &pixels, false );

	WriteHQShot( pixels );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NGame::MakeHQShot( CTransformStack*, NGScene::IGameView* ) @0x1a1da0
//
// The "transform already known" overload: skip the camera and render with the caller-supplied transform
// over the whole view -- SDrawInfo { pTS = given stack, vOrigin=(0,0), vSize=(1,1) } (the SDrawInfo
// default ctor already seeds those, matching the release). Then render + read back + WriteHQShot.
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeHQShot( CTransformStack *pTransform, NGScene::IGameView *pView )
{
	NGScene::IGameView::SDrawInfo drawInfo;
	drawInfo.pTS = pTransform;

	pView->Draw( drawInfo );

	CArray2D<NGfx::SPixel8888> pixels;
	NGfx::MakeScreenShot( &pixels, false );

	WriteHQShot( pixels );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NGame
////////////////////////////////////////////////////////////////////////////////////////////////////
