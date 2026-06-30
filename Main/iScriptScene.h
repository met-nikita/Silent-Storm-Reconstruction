#ifndef __A5_ISCRIPTSCENE_H_
#define __A5_ISCRIPTSCENE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "iMain.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICScriptScene  --  the queued main-loop command that opens a scripted scene (a cut-scene style
// menu rendered over the live world).  Reconstructed from the release module
// .\release\iScriptScene.obj (absent from this predecessor tree).  Exec() tears the whole interface
// stack down (ResetStack), builds a CScriptSceneInterface, initialises it with the held scene id,
// and makes it the SOLE interface on the stack (SetInterface).  Transient (a CInterfaceCommand, not
// save-registered -- like CICCreditsScreen / CICClues).   ctor @0x237f50  /  Exec @0x2384e0.
// nID is the scene/world DB template id (passed straight to CRenderBaseInterface::Initialize).
// VA = RVA + 0x400000.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICScriptScene: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICScriptScene);
	int nID;
public:
	CICScriptScene() {}
	CICScriptScene( int _nID ): nID( _nID ) {}

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
