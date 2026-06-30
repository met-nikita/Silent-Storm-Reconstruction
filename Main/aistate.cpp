#include "StdAfx.h"
//
#include "aiLog.h"
#include "aiUnit.h"
#include "aiPlayer.h"
#include "aiTacticalCommander.h"
#include "rpgUnit.h"
#include "wUnitServer.h"
//
#include "aiState.h"
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
//	CAIState
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIState: public IAIState
{
	OBJECT_BASIC_METHODS(CAIState);
	ZDATA
	CPtr<NWorld::CWorld> pWorld;
	CObj<IAIPlayer> pAlly;
	CObj<IAIPlayer> pEnemy;
	CPtr<IAIUnit> pCurrentUnit;
	CPtr<IAIUnit> pCurrentEnemy;
	int nCurrentAction;
	int nTurnStartAllyHP, nTurnStartEnemyHP; // HP в начале хода
	CPtr<CAITacticalCommander> pAITacticalCommander;
	vector<SAIUnitGroup> enemyGroups;	
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pWorld); f.Add(3,&pAlly); f.Add(4,&pEnemy); f.Add(5,&pCurrentUnit); f.Add(6,&pCurrentEnemy); f.Add(7,&nCurrentAction); f.Add(8,&nTurnStartAllyHP); f.Add(9,&nTurnStartEnemyHP); f.Add(10,&pAITacticalCommander); f.Add(11,&enemyGroups); return 0; }
	//
	void MakeEnemyGroups();
	//
public:
	CAIState() {}
	CAIState( NWorld::CWorld *pWorld, CAITacticalCommander *_pAITacticalCommander );
	// IAIState
	virtual IAIPlayer *GetAllyAIPlayer() { return pAlly; }
	virtual IAIPlayer *GetEnemyAIPlayer() { return pEnemy; }
	virtual NWorld::CWorld *GetWorld() { return pWorld; }
	virtual IAIUnit *GetCurrentAIUnit() { return pCurrentUnit; }
	virtual void SetCurrentAIUnit( IAIUnit *pAIUnit ) { pCurrentUnit = pAIUnit; }
	virtual IAIUnit *GetCurrentAIEnemy() { return pCurrentEnemy; }
	virtual void SetCurrentAIEnemy( IAIUnit *pAIUnit );
	virtual bool IsPositionLocked( SPathPlace &ptPos, IAIUnit *pAIUnit );
	virtual bool IsPerformingAction();
	virtual bool IsSomebodyKilled();
	virtual bool IsContain( IAIUnit *_pAIUnit );	
	virtual void RemoveUnit( IAIUnit *_pAIUnit );
	virtual int GetEnemyHP();
	virtual int GetAllyHP();
	virtual int GetTurnStartEnemyHP() { return nTurnStartEnemyHP; }
	virtual int GetTurnStartAllyHP() { return nTurnStartAllyHP; }
	virtual int GetCurrentAction() { return nCurrentAction; }
	virtual void SetCurrentAction( int nAction ) { nCurrentAction = nAction; }
	virtual CAITacticalCommander *GetAITacticalCommander() { return pAITacticalCommander; }
	virtual void OnTurnStarted();
	virtual bool IsAITurn() { return GetAITacticalCommander()->IsAITurn(); }
	virtual const vector<SAIUnitGroup>& GetEnemyGroups() const { return enemyGroups; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIState::CAIState( NWorld::CWorld *_pWorld, CAITacticalCommander *_pAITacticalCommander ):
	pWorld(_pWorld), pCurrentUnit(0), pAITacticalCommander( _pAITacticalCommander )
{
	// создаем AIUnit-ы 
	pAlly = CreateAIPlayer( this );
	pEnemy = CreateAIPlayer( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIState::MakeEnemyGroups()
{
	const float F_SQR_ENEMY_GROUP_DISTANCE = 2.25; // distance = 1.5 m
	const float F_SQR_ALLY_GROUP_DISTANCE = 9; // distance = 3 m
	//
	enemyGroups.clear();
	vector< CPtr<IAIUnit> > &units = *( pEnemy->GetUnits() );
	vector< CPtr<IAIUnit> > &allies = *( pAlly->GetUnits() );
	for ( vector< CPtr<IAIUnit> >::iterator i = units.begin(); i != units.end(); ++i )
	{
		ASSERT( IsValid( *i ) );
		if ( !IsValid( *i ) )
			continue;
		//
		CPtr<IAIUnit> unitI = *i;
		enemyGroups.push_back( SAIUnitGroup() );
		enemyGroups.back().enemies.push_back( unitI );
		enemyGroups.back().ptCenter = unitI->GetPosition().GetCP();
		vector< CPtr<IAIUnit> >::iterator j = i;
		for ( ++j; j != units.end(); ++j )
		{
			CPtr<IAIUnit> unitJ = *j;
			float fSqrDistance = fabs2( unitI->GetPosition().GetCP() - unitJ->GetPosition().GetCP() );
			if ( fSqrDistance < F_SQR_ALLY_GROUP_DISTANCE )
			{
				enemyGroups.back().enemies.push_back( unitJ );
				enemyGroups.back().ptCenter += unitJ->GetPosition().GetCP();
			}
		}
		enemyGroups.back().ptCenter /= enemyGroups.back().enemies.size();
		for ( vector< CPtr<IAIUnit> >::iterator j = allies.begin(); j != allies.end(); ++j )
		{
			CPtr<IAIUnit> unitJ = *j;
			float fSqrDistance = fabs2( unitI->GetPosition().GetCP() - unitJ->GetPosition().GetCP() );
			if ( fSqrDistance < F_SQR_ALLY_GROUP_DISTANCE )
				enemyGroups.back().allies.push_back( unitJ );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIState::OnTurnStarted()
{
	pAlly->OnTurnStarted();
	pEnemy->OnTurnStarted();
	nTurnStartEnemyHP = GetEnemyHP();
	nTurnStartAllyHP = GetAllyHP();
	MakeEnemyGroups();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIState::SetCurrentAIEnemy( IAIUnit *pAIUnit )
{ 
	pCurrentEnemy = pAIUnit; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIState::GetEnemyHP()
{
	int nRes = 0;
	vector< CPtr<IAIUnit> > *pUnits = pEnemy->GetUnits();
	for ( vector< CPtr<IAIUnit> >::iterator i = pUnits->begin(); i != pUnits->end(); ++i )
		nRes += (*i)->GetHP();
	//
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAIState::GetAllyHP()
{
	int nRes = 0;
	vector< CPtr<IAIUnit> > *pUnits = pAlly->GetUnits();
	for ( vector< CPtr<IAIUnit> >::iterator i = pUnits->begin(); i != pUnits->end(); ++i )
		nRes += (*i)->GetHP();
	//
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIState::IsContain( IAIUnit *_pAIUnit )
{
	return ( pAlly->IsContain( _pAIUnit ) || pEnemy->IsContain( _pAIUnit ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIState::IsPositionLocked( SPathPlace &ptPos, IAIUnit *pAIUnit )
{
	return ( pAlly->IsPositionLocked( ptPos, pAIUnit ) || pEnemy->IsPositionLocked( ptPos, pAIUnit ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIState::IsPerformingAction()
{
	return ( pAlly->IsPerformingAction() || pEnemy->IsPerformingAction() );
}
//////////////////////////////////////////////////////////////////////////////////////	
bool CAIState::IsSomebodyKilled()
{
	return ( pAlly->IsSomebodyKilled() || pEnemy->IsSomebodyKilled() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIState::RemoveUnit( IAIUnit *_pAIUnit )
{
	pAlly->RemoveUnit( _pAIUnit );
	pEnemy->RemoveUnit( _pAIUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIState *CreateAIState( NWorld::CWorld *pWorld, CAITacticalCommander *pAITacticalCommander )
{
	return new CAIState( pWorld, pAITacticalCommander );
}
//////////////////////////////////////////////////////////////////////////////////////	
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x52822120, CAIState );