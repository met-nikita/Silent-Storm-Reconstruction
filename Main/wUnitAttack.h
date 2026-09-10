#ifndef __WUNITATTACK_H_
#define __WUNITATTACK_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
	class IGrenadeItem;
}
namespace NDb
{
	class CRPGGrenade;
}
namespace NAI
{
	class IAIMap;
	enum EFindPathParams;
	class CPath;
	struct SUnitPosition;
	struct SPathPlace;
}
#include "wEActiveItem.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EUnitCommandResult;
class CCmd;
class CWorld;
class CCannon;
class CCommand;
class CUnitServer;
class CCommandExecute;
bool CanDropCorpse( const NAI::SUnitPosition &from, NAI::IAIMap *pAIMap );
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EActionType
{
	AT_SHOOT,
	AT_BAZOOKA,
	AT_MELEE,
	AT_GRENADE,
	AT_FIRSTAID,
	AT_CANNON,
	AT_THROW,
	AT_SNIPE,
	AT_MINE,
	AT_TOOL,
	AT_KEY,
	AT_NONE
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CCommandExecute* CreateMoveExecutor( CUnitServer *_pUS, NAI::CPath *pPath, NAI::EFindPathParams _eParams, ENeedActiveItem eActive, EUnitCommandResult *pError, bool bCheckCanRotate = true );
CCommandExecute* CreateActionExecutor( CUnitServer *pUS, CCmd *pCmd, EUnitCommandResult *pError );
////////////////////////////////////////////////////////////////////////////////////////////////////
EActionType GetActionType( CUnitServer *pUS );
// the places near ptTarget from which pUS could melee it (the "melee ring"); used by the AI
// CAINearEnemyPlaceSource to path toward an enemy's melee ring.
void GetMeleeAttackPlaces( CUnitServer *pUS, const CVec3 &ptTarget, vector<NAI::SPathPlace> *pRes );
// can the cannon engage a target at ptTarget (UCR_OK / UCR_NEED_RELOAD / ...); used by the AI heavy-gun
// actions to decide whether a manned cannon can hit the current enemy.
EUnitCommandResult CanAttackWithCannon( CCannon *pCannon, const CVec3 &ptTarget );
// script-driven instant grenade throw (no inventory grenade / AP / wind-up animation); used by the lua
// UnitGrenadeToUnit / UnitGrenadeToWaypoint handlers. retail NWorld::UnitThrowGrenade @0x3ac740.
void UnitThrowGrenade( CUnitServer *pUS, NDb::CRPGGrenade *pGrenade, const CVec3 &ptTarget, int nReserved = 0 );
//EUnitCommandResult CanDoFirstAid( CUnitServer *pUS, const NAI::SUnitPosition &from, CUnitServer *pTarget );
//EUnitCommandResult CanUnitThrowGrenade( CUnitServer *pUS, const NAI::SUnitPosition &from, const CVec3 &ptTarget, NRPG::IGrenadeItem *pGrenade ); // AI
//EUnitCommandResult CanAttackWithCannon( CCannon *pCannon, const CVec3 &ptTarget ); // AI
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
