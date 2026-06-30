#include "StdAfx.h"
#include "wUnitServer.h"     // NWorld::CUnitServer (+ CDumbUnitServer::GetWorld) -- complete before wMain.h
#include "wMain.h"           // NWorld::CWorld (: IWorld)
#include "wInterface.h"      // NWorld::IWorld::InformCorpseStop

namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// WorldInformCorpseStop @0x35e940 (retail-new wInformCorpseStop.obj) -- notify the unit's world that a
// corpse has settled. The disasm is a single virtual hop: mov ecx,[pUS+0x2c] (pWorld) ; call [vtbl+0x1f8].
// Faithful dispatcher onto the IWorld::InformCorpseStop slot (added at the end of the dev IWorld). The
// real CWorld-side handler (the corpse-cap game-over arming via pGameOverCall + the corpse-settled gate)
// lives in wMain.obj and is deferred with the wCheckTooMuchCorpses arming path -- so for now the slot is a
// no-op default and this is behaviour-neutral.
////////////////////////////////////////////////////////////////////////////////////////////////////
void WorldInformCorpseStop( CUnitServer *pUS )
{
	pUS->GetWorld()->InformCorpseStop( pUS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}  // namespace NWorld
