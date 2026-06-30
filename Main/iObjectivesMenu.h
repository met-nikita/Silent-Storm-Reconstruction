#ifndef __A5_OBJECTIVES_H_
#define __A5_OBJECTIVES_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// LUA convergence (objectives/journal): a modal screen that RENDERS the scenario goal/task state the lua
// ScenarioAddGoal / SetGoalComplete / SetTaskComplete family stores (per-zone CScenarioZone::scriptGoals)
// but which the dev otherwise never shows. Reconstructed 1:1 on the working dev sibling iCluesMenu.cpp
// (CCluesItem/CCluesUI/CCluesInterface/CICClues) -- same in-game popup container (181), same list+detail
// widget set -- with the data source swapped from CScenarioClue to CScenarioGoal/CScenarioTask. Opened by
// the lua ShowObjectives() command (NMainLoop::Command(new CICObjectives(globalGame))).
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "iMain.h"
namespace NRPG
{
	class CGlobalGame;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICObjectives: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICObjectives);
private:
	CPtr<NRPG::CGlobalGame> pGame;

public:
	CICObjectives() {}
	CICObjectives( NRPG::CGlobalGame *pGame );
	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
