#ifndef __AICOMPOUNDACTION_H_
#define __AICOMPOUNDACTION_H_
////////////////////////////////////////////////////////////////////////////////////////////////////
// PHASE-7 SUPERSEDE: the dev predecessor CAILogic (`: CAIJob`, job-based DoJob/Think/MakeDecision +
// the CreateAttackAILogic/CreateDefendAILogic factories) is replaced by the release command-driven
// behaviour layer: CAILogic/IAILogic base (aiLogic.h) + the concrete combat logics & their factories
// CreateAIAttackLogic/CreateAIDefenceLogic (aiCombatLogic.h). Thin redirect so the remaining dev
// includers compile against the substrate. See reconstruction/integration-plan.md.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "aiLogic.h"
#include "aiCombatLogic.h"
#endif
