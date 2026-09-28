#include "StdAfx.h"
//
#include "aiLog.h"
#include "aiUnit.h"
#include "aiMisc.h"
#include "aiPlayer.h"
#include "aiCommander.h"
#include "aiInventory.h"     // CAIInventory::GetBestFireArms (GetDangerousAttackableEnemy)
#include "aiWeapon.h"        // CAIFireArmsWeapon
#include "rpgUnit.h"
#include "wUnitServer.h"
#include "wMain.h"           // NWorld::CWorld::GetDiplomacyState
//
#include "..\DBFormat\DataRPG.h"   // NDb::EShootMode
#include "..\DBFormat\DataMap.h"   // NDb::DS_ENEMY (EDiplomacyState)
//
#include "aiState.h"
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
//	SAIState -- the flat AI-state value struct (see aiState.h). These are the dev CAIState method bodies
//	verbatim (pAIState-> stays this->); the type is now a plain value struct embedded by value in
//	CAICommander. No CObjectBase / no REGISTER_SAVELOAD_CLASS / no CreateAIState heap alloc.
////////////////////////////////////////////////////////////////////////////////////////////////////
SAIState::SAIState(): nCurrentAction( 0 ), nTurnStartAllyHP( 0 ), nTurnStartEnemyHP( 0 )
{
	// retail rebuilds the rosters in Synchronize; dev's Synchronize derefs pAlly/pEnemy directly and the
	// deserialize path uses THIS default ctor (retail no longer serializes them), so create them here too
	// (mirrors the param ctor) -- otherwise a loaded save null-derefs pAlly at aistate.cpp:71.
	pAlly = CreateAIPlayer();
	pEnemy = CreateAIPlayer();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SAIState::SAIState( NWorld::CWorld *_pWorld, CAICommander *_pAICommander ):
	pWorld( _pWorld ), pCurrentUnit( 0 ), nCurrentAction( 0 ), nTurnStartAllyHP( 0 ),
	nTurnStartEnemyHP( 0 ), pAICommander( _pAICommander )
{
	// the ally/enemy roster wrappers; they no longer back-reference the state
	pAlly = CreateAIPlayer();
	pEnemy = CreateAIPlayer();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SAIState::~SAIState()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// operator& @0x38c30 analog -- serialized INLINE as CAICommander tag7 (CallObjectSerialize<SAIState>). The
// pAICommander back-ref (tag10) resolves to the registered owning commander, so there is no save orphan.
////////////////////////////////////////////////////////////////////////////////////////////////////
int SAIState::operator&( CStructureSaver &f )
{
	// retail SAIState::operator& @0x38c30 serializes ONLY {2 pWorld, 3 pPlayer, 4 enemyGroups}. The ally/
	// enemy rosters (pAlly/pEnemy) and per-turn action state are RUNTIME -- Synchronize rebuilds the rosters
	// from the commander's units every segment. dev's old leg serialized 8 extra fields misaligned against
	// retail's 3, so loading a retail save read retail's pPlayer ref into dev's CObj<IAIPlayer> pAlly
	// (dynamic_cast -> null) -> Synchronize null-derefs pAlly (aistate.cpp:71). Match retail: pWorld@2,
	// pPlayer@3, enemyGroups@4. pAlly/pEnemy are created in the ctor now; the pAICommander/pWorld
	// back-refs are runtime (reconnected as the owning commander runs).
	f.Add( 2, &pWorld ); f.Add( 3, &pPlayer ); f.Add( 4, &enemyGroups );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIState::SetCurrentAIUnit( IAIUnit *pAIUnit )
{
	pCurrentUnit = pAIUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SAIState::IsAITurn()
{
	return IsValid( pAICommander ) && pAICommander->IsAITurn();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail v1.2 0x4a99f0 queries world units, not the commander's owned roster.
// "Allies" includes neutrals (any non-enemy); the last flag admits human units.
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIState::GetUnits( vector< CPtr<IAIUnit> > *pUnits, bool bAllies, bool bIncludeNonAI ) const
{
	pUnits->clear();
	if ( !IsValid( pWorld ) )
		return;
	list< CPtr<NWorld::CUnitServer> > units;
	pWorld->GetAllUnits( &units );
	for ( list< CPtr<NWorld::CUnitServer> >::const_iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( !IsValid( *i ) || !(*i)->CanFight() )
			continue;
		if ( (pWorld->GetDiplomacyState( pPlayer, (*i)->GetPlayer() ) != NDb::DS_ENEMY) != bAllies )
			continue;
		IAIUnit *pAI = NAI::GetAIUnit( *i );
		if ( IsValid( pAI ) && (pAI->IsUnderAIControl() || bIncludeNonAI) )
			pUnits->push_back( pAI );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIUnit *SAIState::GetNearestUnit( const CVec3 &pos, bool bAllies, bool bIncludeNonAI, IAIUnit *pExclude ) const
{
	// Retail v1.1 0x4a9a10: linear distance and strict first-wins tie handling.
	vector< CPtr<IAIUnit> > units;
	GetUnits( &units, bAllies, bIncludeNonAI );
	IAIUnit *pBest = 0;
	float fBest = 65535.0f;
	for ( int i = 0; i < units.size(); ++i )
		if ( units[i] != pExclude )
		{
			float fDistance = fabs( pos - units[i]->GetPosition().GetCP() );
			if ( fDistance < fBest )
			{
				fBest = fDistance;
				pBest = units[i];
			}
		}
	return pBest;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Keep the legacy roster adapters for their consumers, without registering
// foreign units on every commander. Only owned units receive this back-pointer.
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIState::Synchronize()
{
	GetUnits( pAlly->GetUnits(), true, true );
	GetUnits( pEnemy->GetUnits(), false, true );
	if ( !IsValid( pAICommander ) )
		return;
	NWorld::CPlayer *pMyPlayer = pAICommander->GetPlayer();
	const vector< CObj<IAIUnit> > &units = pAICommander->GetUnitsList();
	for ( vector< CObj<IAIUnit> >::const_iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( !IsValid( *i ) || !IsValid( (*i)->GetUnitServer() ) )
			continue;
		NWorld::IPlayer *pUP = (*i)->GetUnitServer()->GetPlayer();
		if ( pUP == (NWorld::IPlayer*)pMyPlayer )
			(*i)->SetAIState( this );
	}
	MakeEnemyGroups();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetDangerousAttackableEnemy -- re-homed verbatim from CAITacticalCommander (pAIState-> becomes this->).
// Picks the nearest enemy (within a distance band) with the best to-hit differential.
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIUnit* SAIState::GetDangerousAttackableEnemy( IAIUnit *pAIUnit )
{
	ASSERT( IsValid( pAIUnit ) );
	if ( !IsValid( pAIUnit ) )
		return 0;
	//
	const float fDeltaDistance = 1.5f;
	float fGoodDistance = float( 0xFFF );
	list< CPtr< IAIUnit > > goodEnemies;
	//
	vector< CPtr<IAIUnit> > *pEnemyUnits;
	if ( pAlly->IsContain( pAIUnit ) )
		pEnemyUnits = pEnemy->GetUnits();
	else
		pEnemyUnits = pAlly->GetUnits();
	//
	for ( vector< CPtr<IAIUnit> >::iterator i = pEnemyUnits->begin(); i != pEnemyUnits->end(); ++i )
	{
		CPtr<NWorld::CUnitServer> pUS = (*i)->GetUnitServer();
		if ( IsValid( pUS ) && pUS->CanFight() )
		{
			float fDistance = fabs( (*i)->GetPosition().GetCP() - pAIUnit->GetPosition().GetCP() );
			if ( fabs( fDistance - fGoodDistance ) < fDeltaDistance )
			{
				goodEnemies.push_back( *i );
			}
			else if ( fDistance < fGoodDistance )
			{
				goodEnemies.clear();
				goodEnemies.push_back( *i );
				fGoodDistance = fDistance;
			}
		}
	}
	//
	int nBestD = -1;
	CPtr<IAIUnit> pGoodEnemy;
	for ( list< CPtr< IAIUnit > >::iterator i = goodEnemies.begin(); i != goodEnemies.end(); ++i )
	{
		int nD = 0, nMaxToHit, nHitCover;
		NDb::EShootMode eTmpShootMode;
		CPtr<CAIFireArmsWeapon> pTmpWeapon =
			(*i)->GetAIInventory()->GetBestFireArms( (*i)->GetUnitPosition(), pAIUnit, (*i)->GetAP(), &nHitCover, &nD, &eTmpShootMode, &nMaxToHit );
		//
		if ( nD > nBestD || !IsValid( pGoodEnemy ) )
		{
			nBestD = nD;
			pGoodEnemy = *i;
		}
	}
	//
	return pGoodEnemy;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIUnitGroup::AddUnit( IAIUnit *pUnit )
{
	if ( !IsValid( pUnit ) )
		return;
	int nCount = enemies.size();
	CVec3 ptUnit = pUnit->GetPosition().GetCP();
	// Retail 0x4a9860 uses the PRE-insertion count, including its n==1 behavior.
	ptCenter = nCount == 0 ? ptUnit : (ptCenter * float(nCount - 1) + ptUnit) / float(nCount);
	enemies.push_back( pUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIState::MakeEnemyGroups()
{
	enemyGroups.clear();
	if ( !IsValid( pWorld ) || !IsValid( pPlayer ) )
		return;
	list< CPtr<NWorld::CUnitServer> > units;
	vector< CPtr<NWorld::CUnitServer> > enemies, protectedUnits;
	pWorld->GetAllUnits( &units );
	for ( list< CPtr<NWorld::CUnitServer> >::const_iterator i = units.begin(); i != units.end(); ++i )
	{
		if ( !IsValid( *i ) || !(*i)->CanFight() )
			continue;
		if ( pWorld->GetDiplomacyState( pPlayer, (*i)->GetPlayer() ) == NDb::DS_ENEMY )
			enemies.push_back( *i );
		else
			protectedUnits.push_back( *i );
	}
	for ( int i = 0; i < enemies.size(); ++i )
	{
		CVec3 ptUnit = enemies[i]->GetPosition().GetCP();
		for ( int j = 0; j < enemyGroups.size(); ++j )
			if ( fabs2( ptUnit - enemyGroups[j].ptCenter ) < 2.25f )
				enemyGroups[j].AddUnit( GetAIUnit( enemies[i] ) );
		enemyGroups.push_back( SAIUnitGroup() );
		enemyGroups.back().AddUnit( GetAIUnit( enemies[i] ) );
	}
	for ( int i = 0; i < protectedUnits.size(); ++i )
		for ( int j = 0; j < enemyGroups.size(); ++j )
			enemyGroups[j].fNearestAlly = Min( enemyGroups[j].fNearestAlly,
				fabs( protectedUnits[i]->GetPosition().GetCP() - enemyGroups[j].ptCenter ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIState::OnTurnStarted()
{
	pAlly->OnTurnStarted();
	pEnemy->OnTurnStarted();
	nTurnStartEnemyHP = GetEnemyHP();
	nTurnStartAllyHP = GetAllyHP();
	MakeEnemyGroups();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIState::SetCurrentAIEnemy( IAIUnit *pAIUnit )
{
	pCurrentEnemy = pAIUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int SAIState::GetEnemyHP()
{
	int nRes = 0;
	vector< CPtr<IAIUnit> > *pUnits = pEnemy->GetUnits();
	for ( vector< CPtr<IAIUnit> >::iterator i = pUnits->begin(); i != pUnits->end(); ++i )
		nRes += (*i)->GetHP();
	//
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int SAIState::GetAllyHP()
{
	int nRes = 0;
	vector< CPtr<IAIUnit> > *pUnits = pAlly->GetUnits();
	for ( vector< CPtr<IAIUnit> >::iterator i = pUnits->begin(); i != pUnits->end(); ++i )
		nRes += (*i)->GetHP();
	//
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SAIState::IsContain( IAIUnit *_pAIUnit )
{
	return ( pAlly->IsContain( _pAIUnit ) || pEnemy->IsContain( _pAIUnit ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SAIState::IsPositionLocked( SPathPlace &ptPos, IAIUnit *pAIUnit )
{
	return ( pAlly->IsPositionLocked( ptPos, pAIUnit ) || pEnemy->IsPositionLocked( ptPos, pAIUnit ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SAIState::IsPerformingAction()
{
	return ( pAlly->IsPerformingAction() || pEnemy->IsPerformingAction() );
}
//////////////////////////////////////////////////////////////////////////////////////
bool SAIState::IsSomebodyKilled()
{
	return ( pAlly->IsSomebodyKilled() || pEnemy->IsSomebodyKilled() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SAIState::RemoveUnit( IAIUnit *_pAIUnit )
{
	pAlly->RemoveUnit( _pAIUnit );
	pEnemy->RemoveUnit( _pAIUnit );
}
//////////////////////////////////////////////////////////////////////////////////////
}
