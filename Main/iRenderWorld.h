#ifndef __A5_IRENDERWORLD_H_
#define __A5_IRENDERWORLD_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
class ICamera;
#include "iMission.h"	// NGame::CMissionBase (serialization-convergence W4.2 base)
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRenderBaseInterface -- the 3D-backdrop menu base. Serialization-convergence W4.2: reparented onto
// the retail NGame::CMissionBase (retail layout = CMissionBase + bindShadows/bindSwitchLighting,
// NOTHING else); its operator& @0x1cce20 is the tag-1 base chunk ONLY. The old dev members
// (pScene/pSoundScene/pRender/pWorld/pCursor/pInterface/pCamera/nLightMode/pLightSource, dev tags
// 2-4/6/9-13) now live on the base (base tags 3/4/5/10/15/16/18/24/25). The dev-extra pPlayer (dev
// tag 7) / pCommander (dev tag 8) have NO retail member -- kept TRANSIENT below.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRenderBaseInterface: public CMissionBase
{
	OBJECT_BASIC_METHODS(CRenderBaseInterface);
private:
	NInput::CBind bindShadows, bindSwitchLighting;

	ZDATA_(CMissionBase)
	// dev-extra (TRANSIENT): the backdrop world's single player + commander -- retail
	// CRenderBaseInterface has no such members (its Initialize @0x22ef80 leaves the player on the
	// world only). Rebuilt by Initialize; a re-serialized menu re-runs Initialize in dev flows.
	CPtr<NWorld::IPlayer> pPlayer;
	CPtr<NWorld::CCommander> pCommander;
public:
	// retail NGame::CRenderBaseInterface::operator& @0x1cce20 -- the base chunk only
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CMissionBase*)this); return 0; }

public:
	CRenderBaseInterface();

	// retail CRenderBaseInterface vtbl+0x44 (IsSequence) is COMDAT-folded onto a `return true` stub
	// (@0x088d00): the 3D-backdrop menus ALWAYS short-circuit the CMissionBase::GetCamera selector
	// (@0x1a1ee0) to their own pCamera -- they have no player trackers (pActivePlayer is null).
	virtual bool IsSequence() const { return true; }

	void Initialize( int nTemplate );

	void Command( NWorld::CCommand *pCmd );
	void DoEvent( NWorld::CCommand *pCmd );	// events channel twin (script OnScriptNotify posts ride it)

	void Step();
	// (OnGetFocus/OnLostFocus inherit the CMissionBase bodies -- ResetTiming + sound-scene pause pair)
	bool ProcessEvent( const NInput::SEvent &sEvent );
	void RenderFrame( const STime &sTime, ICamera *pCamera );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif