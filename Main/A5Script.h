#ifndef __A5Script_H_
#define __A5Script_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
#include "..\Script\Script.h"
#include "..\MiscDll\Commands.h"
//
namespace NScenario
{
	class CScenarioTracker;
}
namespace NRPG
{
	class CUnit;
	class CGlobalGame;
}
//
namespace NAI
{
	class IUnit;
}
//
namespace NWorld
{
	class CWorld;
	class IWorld;
	class CUICmd;
	enum EInterfaceActionType;
}
//
namespace NGame
{
	class IMission;
}
//
namespace NUI
{
	class CInterface;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScript: public Script, public CObjectBase 
{
	NGlobal::CCmd cmdShowError;
	OBJECT_NOCOPY_METHODS(CScript);
	ZDATA_(Script)
public:
	CPtr<NWorld::CWorld> pWorld;
private:
	// release id-queue: live ids of the UI commands currently queued by this script. Replaces the dev's
	// type-counter `vector<int> interfaceActions`. lua WaitForUI(id) polls IsUIActionIDPresent(id) until the
	// command finishes (CWorld::ExecuteCommand -> RemoveUIActionID(id)).
	list<int> interfaceActionIDs;
	list< CPtr<CObjectBase> > miscObjectsHolder;
	CPtr<CObjectBase> pInterface;	// release: CPtr<NUI::CInterface>; CObjectBase here is save-equivalent (CPtr
									// serializes by object id) and avoids a heavy UI include in this TU. Always null in dev.
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(Script*)this); f.Add(2,&pWorld); f.Add(3,&interfaceActionIDs); f.Add(4,&miscObjectsHolder); f.Add(5,&pInterface); return 0; }
	CScript();
	int RunScriptFile( const string &szFileName );
	int RunScriptByID( int nID );
	bool IsUIActionIDPresent( int nID ) const;
	void AddUIActionID( int nID );
	int AddUICommandWithID( NWorld::CUICmd *pCmd );
	void RemoveUIActionID( int nID );
	NScenario::CScenarioTracker *GetScenarioTracker();
	NRPG::CGlobalGame* GetGlobalGame();
	void AddUICommand( NWorld::CUICmd *pCmd );
	// Keep an object alive for the lifetime of the script (release AddMiscObject): the play-sound/
	// play-effect UI commands park their live handle here so the sound/effect keeps playing.
	void AddMiscObject( CObjectBase *pObj );
	// script-UI bridge: the UI interface this script drives (release CScript::pInterface, a
	// CPtr<NUI::CInterface>). GetWindow/GetCursorPos root their lookups here. Set by CMission to the
	// in-mission HUD interface so the bridge is LIVE in-mission (defined in scriptUI.cpp, where the NUI
	// types are complete). pInterface is a weak CPtr saved alongside the mission's own CInterface, so it
	// round-trips through save/load.
	NUI::CInterface* GetScriptInterface();
	void SetScriptInterface( NUI::CInterface *pInterface );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CScript *CreateScript( NWorld::IWorld *pWorld );
void ScriptWarning( const string &message );
void ScriptError( const string &message );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
NScript::CScript *GetScript();
void ProcessCommand( const wstring &szCmd );
#endif