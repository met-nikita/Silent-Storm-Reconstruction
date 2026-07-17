#ifndef __A5_SHOWOBJECTIVES_H_
#define __A5_SHOWOBJECTIVES_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release-shape "show objectives" modal (compiland iObjectivesmenu) -- ADDITIVE PARITY SURFACE.
//
// This adds the RETAIL class shapes (NGame::CShowObjectivesInterface / NGame::CICShowObjectives /
// NUI::CShowObjectivesUI / NUI::CClueLine / NUI::CTaskLine + the NScenario value model) alongside the
// existing working-but-divergent "LUA convergence" reconstruction in iObjectivesMenu.cpp
// (CObjectivesUI/CObjectivesInterface/CICObjectives). Nothing constructs these new classes from the live
// UI paths yet: the lua ShowObjectives() command and iMission.cpp still build the old
// CICObjectives(globalGame), so this is behaviour-neutral -- it carries the 18 release @rva markers
// WITHOUT performing the same-tag rewrite or rewiring any live call path. (Serialization-convergence W3
// added the four retail serializers + saveload ids 0xB3225130/31/32/3A, so retail savegames carrying the
// modal now reconstruct these classes.) (Per-row textures / template container ids that were lost in the decode
// are documented at each call site in the .cpp; row sub-templates the dev game.db lacks are IsValid-
// guarded, so this is build-GREEN, not pixel-parity.)
//
// Functions carried (VA = RVA + 0x400000):
//   NScenario::SGoalDescription::SGoalDescription(copy)              @0x220370
//   NUI::CShowObjectivesUI::CShowObjectivesUI                        @0x21e550
//   NUI::CShowObjectivesUI::ProcessMessage                          @0x21f3c0
//   NUI::CClueLine::CClueLine(SWindowInfo&,IMission*,SGoalDesc&,bool)@0x21eaf0
//   NUI::CClueLine::CClueLine()                                     @0x220540
//   NUI::CClueLine::ProcessMessage                                  @0x21eb90
//   NUI::CTaskLine::CTaskLine                                       @0x21ea80
//   NUI::CTaskLine::ProcessMessage                                  @0x21f020
//   NGame::CShowObjectivesInterface::CShowObjectivesInterface(def)   @0x21e5b0
//   NGame::CShowObjectivesInterface::CShowObjectivesInterface(copy)  @0x2201b0
//   NGame::CShowObjectivesInterface::OnGetFocus                      @0x21e380
//   NGame::CShowObjectivesInterface::ProcessEvent                    @0x21e450
//   NGame::CShowObjectivesInterface::RenderFrame                     @0x21e4c0
//   NGame::CShowObjectivesInterface::Step                           @0x21e4f0
//   NGame::CShowObjectivesInterface::Initialize                     @0x21e640
//   NGame::CICShowObjectives::CICShowObjectives(payload)            @0x21e9c0
//   NGame::CICShowObjectives::CICShowObjectives(copy)               @0x21ffa0
//   NGame::CICShowObjectives::Exec                                  @0x21ea00
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "iMain.h"
#include "scScenarioTracker.h"		// list/vector/CDBPtr/NDb::CString + forward CScenarioZone
#include "scFlowChartItems.h"		// NScenario::ETaskState + runtime CScenarioGoal/CScenarioTask/CScenarioZone
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame { class IMission; }
namespace NUI   { class CScreenShot; }
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NScenario
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release task/goal completion state (parallels dev ETaskState; same numeric values).
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EScenarioTaskState
{
	STS_UNKNOWN   = 0,
	STS_COMPLETED = 1,
	STS_FAILED    = 2,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline EScenarioTaskState TaskStateToScenario( ETaskState eState )
{
	switch ( eState )
	{
	case TS_COMPLETED:	return STS_COMPLETED;
	case TS_FAILED:		return STS_FAILED;
	default:			return STS_UNKNOWN;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// STaskDescription (release, size 8) -- a goal's sub-task value: localized string + state.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct STaskDescription
{
	CDBPtr<NDb::CString>	pString;
	EScenarioTaskState		state;
	STaskDescription(): state( STS_UNKNOWN ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// SGoalDescription (release, size 20) -- a goal value: localized string + state + its tasks.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGoalDescription
{
	CDBPtr<NDb::CString>		pString;
	EScenarioTaskState			state;
	vector< STaskDescription >	tasks;
	SGoalDescription(): state( STS_UNKNOWN ) {}
	SGoalDescription( const SGoalDescription &src );	// @0x220370
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetGoalsFromZone -- release data bridge, written as a small adapter over the existing dev data
// (CScenarioZone::GetScriptGoals -> CScenarioGoal/CScenarioTask -> GetDBGoal/GetDBTask/GetState).
// (The retail call is pMission->GetScenarioTracker()->GetGoalsFromZone(units,globalGame); dev IMission
// reaches the tracker via GetRPGGame()->pScenarioTracker, but the goal data itself is per-zone, so the
// zone-only adapter is the faithful + dependency-free form.)
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetGoalsFromZone( CScenarioZone *pZone, list< SGoalDescription > *pGoals );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NScenario
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICShowObjectives -- queued main-loop command that builds + pushes a CShowObjectivesInterface modal
// for one scenario zone (carrying an optional frozen screenshot backdrop). Sibling of CICObjectives,
// but per-zone + screenshot like the retail command. Transient command object -- not serialized (matches
// the unregistered sibling CICObjectives), so no REGISTER_SAVELOAD_CLASS / ZDATA here.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICShowObjectives: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICShowObjectives);
private:
	CPtr<IMission>					pMission;
	CPtr<NUI::CScreenShot>			pScreenShot;
	CPtr<NScenario::CScenarioZone>	pZone;

public:
	CICShowObjectives() {}
	CICShowObjectives( IMission *pMission, NScenario::CScenarioZone *pZone, NUI::CScreenShot *pScreenShot = 0 );	// @0x21e9c0
	CICShowObjectives( const CICShowObjectives &src );		// @0x21ffa0

	virtual void Exec();									// @0x21ea00
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NGame
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
