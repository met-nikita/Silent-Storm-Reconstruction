#ifndef __AIACTION_H_
#define __AIACTION_H_
////////////////////////////////////////////////////////////////////////////////////////////////////
// PHASE-7 SUPERSEDE: the dev predecessor CAIAction (`:CObjectBase` + CPtr<SAIState> pState,
// Do(IAILogContainer*)) is replaced by the release CAICombatLogic-substrate CAIAction
// (aiActionBase.h: rebased on IAIUnit, logs through CAILog, adds the SActionInfo<> memo cache).
// This header is now a thin redirect so the remaining dev includers (aiMoveAction.cpp/.h ...)
// transparently pick up the substrate types. See reconstruction/integration-plan.md.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "aiActionBase.h"
#endif
