#include "stdafx.h"
//
#include "..\DBFormat\DataAck.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
#include "time.h"
#include "wAckBase.h"
#include "wDumbUnit.h"
#include "wUnitServer.h"
#include "wMain.h"	// CPlayer roster (CAckNPercentOfGroupIsKilled squad/dead counts)
#include "rpgUnit.h"
#include "rpgUnitMission.h"
#include "..\Misc\EventsBase.h"
#include "eventUnit.h"
//
#include "wAck.h"
//
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckFriendDies: public CAckBase
{
	OBJECT_BASIC_METHODS(CAckFriendDies);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckFriendDies() : CAckBase() {};
	CAckFriendDies( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	virtual void OnUnitDied( CUnitServer *_pUnit )
	{ 
		if ( GetUnit() != _pUnit && GetUnit()->GetPlayer() == _pUnit->GetPlayer()  ) 
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::CAckNPercentOfGroupIsKilled (release-added; NO Jan03 counterpart -- ported from
// retail OnUnitDied @0x3384e0, ctors @0x331ac0/@0x333da0, factory case 0x76 = condition 118, oracle
// s2_cacknpercentofgroupiskilled.h). When a SQUADMATE (same player, different unit) dies: resolve
// the owning CPlayer (retail __RTDynamicCast of GetPlayer @0x738568), take the FULL squad roster
// size and the dead-member count (retail pl sub-obj +0x28 roster vector + vtbl+0x50 FillDeadList),
// and bark iff  atoi(sParam[0]) < 100 - trunc(nDead*100/nTotal).
// DISASM-VERIFIED polarity (0x738626..0x738639: eax = 0x64 - pct; cmp eax,ebp; jle skip): the bark
// fires while the SURVIVOR percentage still EXCEEDS the param -- ported faithfully, not "fixed".
// The retail game.db keys 8 such rows on the enemy voice-holder personas (AckUnit), so this is an
// enemy-chatter bark. ORIGINAL BUG (confirmed @0x738608): retail fidivs by the raw roster size with
// no empty-roster guard; we guard nTotal==0 to avoid the UB while keeping the branch shape.
class CAckNPercentOfGroupIsKilled: public CAckBase
{
	OBJECT_BASIC_METHODS(CAckNPercentOfGroupIsKilled);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckNPercentOfGroupIsKilled() : CAckBase() {};
	CAckNPercentOfGroupIsKilled( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	virtual void OnUnitDied( CUnitServer *_pUnit )
	{
		if ( !IsValid( _pUnit ) )
			return;
		// retail gate order @0x3384e0: unit live -> !IsThis -> IsFriend (same player, different unit)
		if ( GetUnit() == _pUnit || GetUnit()->GetPlayer() != _pUnit->GetPlayer() )
			return;
		CDynamicCast<CPlayer> pOwner( GetUnit()->GetPlayer() );	// retail __RTDynamicCast to CPlayer
		if ( !IsValid( pOwner ) )
			return;
		const int nParam = atoi( GetDBAck()->sParam[0].c_str() );
		// full roster (the dev player list keeps dead members -- only RemoveUnit drops them)
		const vector< CMObj<CUnitServer> > &members = pOwner->GetPlayerUnits();
		int nDead = 0;
		for ( int k = 0; k < members.size(); ++k )
		{
			CUnitServer *pMember = members[k];	// plain extraction (no ternary over CObj -- UAF rule)
			if ( pMember && pMember->IsDead() )
				++nDead;
		}
		int nPct = 0;
		if ( !members.empty() )	// retail divides unguarded (see ORIGINAL BUG note above)
			nPct = int( float( nDead ) / float( members.size() ) * 100.0f );	// trunc, matches fistp RC=11
		if ( nParam < 100 - nPct )
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckFriendTargetMissed: public CAckBase
{
	OBJECT_BASIC_METHODS(CAckFriendTargetMissed);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckFriendTargetMissed() : CAckBase() {};
	CAckFriendTargetMissed( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	virtual void OnTargetMissed( CUnitServer *_pUnit )
	{ 
		if ( GetUnit() != _pUnit && GetUnit()->GetPlayer() == _pUnit->GetPlayer()  ) 
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckEnemyHasBeenInfictedCritical: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckEnemyHasBeenInfictedCritical);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckEnemyHasBeenInfictedCritical() : CAckBase() {};
	CAckEnemyHasBeenInfictedCritical( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	virtual void OnCritical( CUnitServer *_pUnit )
	{ 
		if ( GetUnit()->GetPlayer() != _pUnit->GetPlayer()  ) 
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckFriendGrenadeKillsMoreThanOneEnemy: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckFriendGrenadeKillsMoreThanOneEnemy);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckFriendGrenadeKillsMoreThanOneEnemy() : CAckBase() {};
	CAckFriendGrenadeKillsMoreThanOneEnemy( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	virtual void OnGrenadeExplosion( CUnitServer *_pUnit,
																		int nUnitsDestroyed, int nObjectsDestroyed )
	{ 
		if ( GetUnit() != _pUnit &&  GetUnit()->GetPlayer() == _pUnit->GetPlayer() && nUnitsDestroyed > 1 )
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckGrenadeKillsMoreThanOneEnemy: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckGrenadeKillsMoreThanOneEnemy);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckGrenadeKillsMoreThanOneEnemy() : CAckBase() {};
	CAckGrenadeKillsMoreThanOneEnemy( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	virtual void OnGrenadeExplosion( CUnitServer *_pUnit,
																		int nUnitsDestroyed, int nObjectsDestroyed )
	{ 
		if ( GetUnit()  == _pUnit && nUnitsDestroyed > 1 )
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckFriendGrenadeDestroysALotOfObjects: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckFriendGrenadeDestroysALotOfObjects);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckFriendGrenadeDestroysALotOfObjects() : CAckBase() {};
	CAckFriendGrenadeDestroysALotOfObjects( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	virtual void OnGrenadeExplosion( CUnitServer *_pUnit,
																		int nUnitsDestroyed, int nObjectsDestroyed )
	{ 
		if ( GetUnit() != _pUnit &&  GetUnit()->GetPlayer() == _pUnit->GetPlayer() &&
			nObjectsDestroyed >= atoi( GetDBAck()->sParam[0].c_str() ) )
				PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckGrenadeDestroysALotOfObjects: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckGrenadeDestroysALotOfObjects);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckGrenadeDestroysALotOfObjects() : CAckBase() {};
	CAckGrenadeDestroysALotOfObjects( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	virtual void OnGrenadeExplosion( CUnitServer *_pUnit,
																		int nUnitsDestroyed, int nObjectsDestroyed )
	{ 
		if ( GetUnit()  == _pUnit  && 
			nObjectsDestroyed >= atoi( GetDBAck()->sParam[0].c_str() ) )
				PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckEnemyBecomesVisibleInRealtime: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckEnemyBecomesVisibleInRealtime);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckEnemyBecomesVisibleInRealtime() : CAckBase() {};
	CAckEnemyBecomesVisibleInRealtime( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	//
	virtual void OnEnemyBecomesVisible( CUnitServer *pWatcher, CUnitServer *pTarget, bool bRealTime )
	{
		if ( GetUnit() == pWatcher && bRealTime )
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckEnemyBecomesVisible: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckEnemyBecomesVisible);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckEnemyBecomesVisible() : CAckBase() {};
	CAckEnemyBecomesVisible( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	//
	virtual void OnEnemyBecomesVisible( CUnitServer *pWatcher, CUnitServer *pTarget, bool bRealTime )
	{
		if ( GetUnit() == pWatcher && GetUnit()->GetPlayer() != pTarget->GetPlayer() )
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckWeakEnemyBecomesVisible: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckWeakEnemyBecomesVisible);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckWeakEnemyBecomesVisible() : CAckBase() {};
	CAckWeakEnemyBecomesVisible( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	//
	virtual void OnEnemyBecomesVisible( CUnitServer *pWatcher, CUnitServer *pTarget, bool bRealTime )
	{
		if ( GetUnit() == pWatcher )
		{
			int nWatcherLevel = pWatcher->GetUnitRPG()->GetRPGUnit()->Skills( NDb::ST_LEVEL );
			int nTargetLevel = pTarget->GetUnitRPG()->GetRPGUnit()->Skills( NDb::ST_LEVEL );
			int nDelta = atoi( GetDBAck()->sParam[0].c_str() );
			if ( nWatcherLevel >= nTargetLevel + nDelta ) 
				PlayAck();
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckStrongEnemyBecomesVisible: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckStrongEnemyBecomesVisible);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckStrongEnemyBecomesVisible() : CAckBase() {};
	CAckStrongEnemyBecomesVisible( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	//
	virtual void OnEnemyBecomesVisible( CUnitServer *pWatcher, CUnitServer *pTarget, bool bRealTime )
	{
		if ( GetUnit() == pWatcher )
		{
			int nWatcherLevel = pWatcher->GetUnitRPG()->GetRPGUnit()->Skills( NDb::ST_LEVEL );
			int nTargetLevel = pTarget->GetUnitRPG()->GetRPGUnit()->Skills( NDb::ST_LEVEL );
			int nDelta = atoi( GetDBAck()->sParam[0].c_str() );
			if ( nWatcherLevel + nDelta <= nTargetLevel )
				PlayAck();
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckEnemyBecomesVisibleInTurnbased: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckEnemyBecomesVisibleInTurnbased);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckEnemyBecomesVisibleInTurnbased() : CAckBase() {};
	CAckEnemyBecomesVisibleInTurnbased( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	//
	virtual void OnEnemyBecomesVisible( CUnitServer *pWatcher, CUnitServer *pTarget, bool bRealTime )
	{
		if ( GetUnit() == pWatcher && !bRealTime )
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckCertainTypeEnemyBecomesVisible: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckCertainTypeEnemyBecomesVisible);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckCertainTypeEnemyBecomesVisible() : CAckBase() {};
	CAckCertainTypeEnemyBecomesVisible( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	//
	virtual void OnEnemyBecomesVisible( CUnitServer *pWatcher, CUnitServer *pTarget, bool bRealTime )
	{
		if ( GetUnit() == pWatcher )
		{
			NDb::CRPGClass *pClass = pTarget->GetUnitRPG()->GetRPGUnit()->pClass;
			if ( !pClass )
				return;
			int nTargetClass = pClass->GetRecordID();
			int nClass = atoi( GetDBAck()->sParam[0].c_str() );
			if ( nClass == nTargetClass ) 
				PlayAck();
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckAccidentallyWoundFriend: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckAccidentallyWoundFriend);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckAccidentallyWoundFriend() : CAckBase() {};
	CAckAccidentallyWoundFriend( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	//
	virtual void OnDoAccidentalDamage( CUnitServer *pAttacker, CUnitServer *pTarget )
	{
		if ( !IsValid( pAttacker ) || !IsValid( pTarget ) )
			return;
		if ( GetUnit() == pAttacker && GetUnit()->GetPlayer() == pTarget->GetPlayer() )
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckInflictCriticalDamageToEnemy: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckInflictCriticalDamageToEnemy);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckInflictCriticalDamageToEnemy() : CAckBase() {};
	CAckInflictCriticalDamageToEnemy( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	//
	virtual void OnDoCriticalDamage( CUnitServer *pAttacker, CUnitServer *pTarget )
	{
		if ( !IsValid( pAttacker ) || !IsValid( pTarget ) )
			return;
		if ( GetUnit() == pAttacker && GetUnit()->GetPlayer() != pTarget->GetPlayer() )
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckFriendInflictsCriticalDamageToEnemy: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckFriendInflictsCriticalDamageToEnemy);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckFriendInflictsCriticalDamageToEnemy() : CAckBase() {};
	CAckFriendInflictsCriticalDamageToEnemy( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	//
	virtual void OnDoCriticalDamage( CUnitServer *pAttacker, CUnitServer *pTarget )
	{
		if ( !IsValid( pAttacker ) || !IsValid( pTarget ) )
			return;
		if ( GetUnit() != pAttacker && GetUnit()->GetPlayer() == pAttacker->GetPlayer() 
			&& GetUnit()->GetPlayer() != pTarget->GetPlayer() )
				PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckPCSufferCriticalDamage: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckPCSufferCriticalDamage);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckPCSufferCriticalDamage() : CAckBase() {};
	CAckPCSufferCriticalDamage( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	//
	virtual void OnDoCriticalDamage( CUnitServer *pAttacker, CUnitServer *pTarget )
	{
		// retail CAckBase::IsThis(pTarget) @0x338ec0: only the unit that ACTUALLY suffered the crit barks the
		// "PC suffers critical" line. The Jan03 predecessor filter (GetUnit()->GetPlayer() != pTarget->GetPlayer())
		// made every player-hero ack-owner bark "I'm hurt" whenever an ENEMY was critted (different player).
		// The sibling suffer-acks (CAckSuffersLightDamage/HardDamage) already use GetUnit()==unit -- match them.
		if ( IsValid( pTarget ) && GetUnit() == pTarget )
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckKilledAnEnemy: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckKilledAnEnemy);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckKilledAnEnemy() : CAckBase() {};
	CAckKilledAnEnemy( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	//
	virtual void OnUnitWasKilled( CUnitServer *pAttacker, CUnitServer *pTarget )
	{
		if ( !IsValid( pAttacker ) || !IsValid( pTarget ) )
			return;
		if ( GetUnit() == pAttacker && GetUnit()->GetPlayer() != pTarget->GetPlayer() )
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckKilledCertainTypeEnemy: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckKilledCertainTypeEnemy);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckKilledCertainTypeEnemy() : CAckBase() {};
	CAckKilledCertainTypeEnemy( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	//
	virtual void OnUnitWasKilled( CUnitServer *pAttacker, CUnitServer *pTarget )
	{
		if ( !IsValid( pAttacker ) || !IsValid( pTarget ) )
			return;
		if ( GetUnit() == pAttacker && GetUnit()->GetPlayer() != pTarget->GetPlayer() )
		{
			int nTargetClass = pTarget->GetUnitRPG()->GetRPGUnit()->pClass->GetRecordID();
			int nClass = atoi( GetDBAck()->sParam[0].c_str() );
			if ( nClass == nTargetClass ) 
				PlayAck();
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckKillingNthEnemyInARow: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckKillingNthEnemyInARow);
	ZDATA
	ZPARENT( CAckBase );
	int nKilled;
	bool bKilledLastTurn;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); f.Add(3,&nKilled); f.Add(4,&bKilledLastTurn); return 0; }
public:
	CAckKillingNthEnemyInARow() : CAckBase() {};
	CAckKillingNthEnemyInARow( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): 
		CAckBase( _pUnit, _pDBAck ), nKilled(0), bKilledLastTurn(false) {};
	//
	virtual void OnNewTurnStarted( IPlayer *pPlayer )
	{
		if ( GetUnit()->GetPlayer() == pPlayer )
		{
			if ( !bKilledLastTurn )
				nKilled = 0;
			bKilledLastTurn = false;
		}
	}
	virtual void OnRealTimeStarted()
	{
		bKilledLastTurn = false;
		nKilled = 0;
	}
	virtual void OnUnitWasKilled( CUnitServer *pAttacker, CUnitServer *pTarget )
	{
		if ( !IsValid( pAttacker ) || !IsValid( pTarget ) )
			return;
		if ( GetUnit() == pAttacker )
		{
			if ( GetUnit()->GetPlayer() == pTarget->GetPlayer() )
				nKilled = 0;
			else
			{
				++nKilled;
				bKilledLastTurn = true;
				int nParam = atoi( GetDBAck()->sParam[0].c_str() );
				if ( nParam == nKilled )
					PlayAck();
			}
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckNthUnitWasKilled: public CAckBase
{	
	OBJECT_BASIC_METHODS(CAckNthUnitWasKilled);
	ZDATA
	ZPARENT( CAckBase );
	int nKilled;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); f.Add(3,&nKilled); return 0; }
public:
	CAckNthUnitWasKilled() : CAckBase() {};
	CAckNthUnitWasKilled( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): 
		CAckBase( _pUnit, _pDBAck ), nKilled( 0 ) {};
	//
	virtual void OnUnitWasKilled( CUnitServer *pAttacker, CUnitServer *pTarget )
	{
		if ( !IsValid( pAttacker ) || !IsValid( pTarget ) )
			return;
		if ( GetUnit()->GetPlayer() == pTarget->GetPlayer() )
		{
			++nKilled;
			int nParam = atoi( GetDBAck()->sParam[0].c_str() );
			if ( nKilled == nParam ) 
				PlayAck();
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckEnemyMissedTarget: public CAckBase
{
	OBJECT_BASIC_METHODS(CAckEnemyMissedTarget);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckEnemyMissedTarget() : CAckBase() {};
	CAckEnemyMissedTarget( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	virtual void OnTargetMissed( CUnitServer *_pUnit )
	{ 
		if ( GetUnit()->GetPlayer() != _pUnit->GetPlayer()  ) 
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckInterruptedEnemy: public CAckBase
{
	OBJECT_BASIC_METHODS(CAckInterruptedEnemy);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckInterruptedEnemy() : CAckBase() {};
	CAckInterruptedEnemy( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	virtual void OnInterrupt( CUnitServer *pWho )
	{ 
		if ( GetUnit() == pWho ) 
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckHealOfFriendFinished: public CAckBase
{
	OBJECT_BASIC_METHODS(CAckHealOfFriendFinished);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckHealOfFriendFinished() : CAckBase() {};
	CAckHealOfFriendFinished( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	virtual void OnHealFinished( CUnitServer *pHealer, CUnitServer *pTarget )
	{ 
		if ( GetUnit() == pHealer && pTarget != GetUnit() ) 
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckSelfHealFinished: public CAckBase
{
	OBJECT_BASIC_METHODS(CAckSelfHealFinished);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckSelfHealFinished() : CAckBase() {};
	CAckSelfHealFinished( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	virtual void OnHealFinished( CUnitServer *pHealer, CUnitServer *pTarget )
	{ 
		if ( GetUnit() == pHealer && pTarget == GetUnit() ) 
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckCannotFinishHeal: public CAckBase
{
	OBJECT_BASIC_METHODS(CAckCannotFinishHeal);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckCannotFinishHeal() : CAckBase() {};
	CAckCannotFinishHeal( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	virtual void OnCannotFinishHeal( CUnitServer *pHealer, CUnitServer *pTarget )
	{ 
		if ( GetUnit() == pHealer && pTarget != GetUnit() ) 
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckSuffersLightDamage: public CAckBase
{
	OBJECT_BASIC_METHODS(CAckSuffersLightDamage);
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckSuffersLightDamage() : CAckBase() {};
	CAckSuffersLightDamage( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {};
	virtual void OnCannotFinishHeal( CUnitServer *pHealer, CUnitServer *pTarget )
	{ 
		if ( GetUnit() == pHealer && pTarget == GetUnit() ) 
			PlayAck();
	}
	virtual void OnSuffersLightDamage( CUnitServer *_pUnit )
	{
		if ( GetUnit() == _pUnit ) 
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckLongBurst: public CAckBase
{
	OBJECT_BASIC_METHODS( CAckLongBurst );
	NGlobal::CEventRegister< CAckLongBurst, NWorld::CEventOnUnitLongBurst > registerEvent;
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckLongBurst( CUnitServer *_pUnit = 0, NDb::CDBAck *_pDBAck = 0 ): 
		registerEvent( this, &CAckLongBurst::OnEvent ), CAckBase( _pUnit, _pDBAck ) {}
	//
	void OnEvent( const CEventOnUnitLongBurst &event )
	{
		if ( GetUnit() == event.pWho )
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckUnhide: public CAckBase
{
	OBJECT_BASIC_METHODS( CAckUnhide );
	NGlobal::CEventRegister< CAckUnhide, NWorld::CEventOnUnitUnhide > registerEvent;
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckUnhide( CUnitServer *_pUnit = 0, NDb::CDBAck *_pDBAck = 0 ): 
		registerEvent( this, &CAckUnhide::OnEvent ), CAckBase( _pUnit, _pDBAck ) {}
	//
	void OnEvent( const CEventOnUnitLongBurst &event )
	{
		CDynamicCast<NWorld::CUnitServer> pWho(event.pWho);
		if (pWho)
		{
			if ( GetUnit() == pWho.GetPtr() )
				PlayAck();
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#define DEFINE_UNITSERVER_ACK( AckName ) class CAck##AckName: public CAckBase                 \
{                                                                                             \
	OBJECT_BASIC_METHODS(CAck##AckName);	                                                      \
	ZDATA                                                                                       \
	ZPARENT( CAckBase );                                                                        \
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }           \
public:                                                                                       \
	CAck##AckName() : CAckBase() {};                                                            \
	CAck##AckName( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck ): CAckBase( _pUnit, _pDBAck ) {}; \
	virtual void On##AckName( CUnitServer *_pUnit )                                             \
	{                                                                                           \
		if ( GetUnit() == _pUnit )                                                                \
			PlayAck();                                                                              \
	}                                                                                           \
};                                                                                            
////////////////////////////////////////////////////////////////////////////////////////////////////
DEFINE_UNITSERVER_ACK( LastPieceOfAmmo );
DEFINE_UNITSERVER_ACK( WeaponJammed );
DEFINE_UNITSERVER_ACK( OrderConfirmation );
DEFINE_UNITSERVER_ACK( ImpossibleToPerformAction );
DEFINE_UNITSERVER_ACK( TargetHit );
DEFINE_UNITSERVER_ACK( HardTargetHit );
DEFINE_UNITSERVER_ACK( TargetMissed );
DEFINE_UNITSERVER_ACK( UnitDied );
DEFINE_UNITSERVER_ACK( SuffersHardDamage );
DEFINE_UNITSERVER_ACK( SkillIncreased );
////////////////////////////////////////////////////////////////////////////////////////////////////
// Condition ID-s
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_ENEMY_BECOMES_VISIBLE = 68;
const int N_ENEMY_BECOMES_VISIBLE_IN_REALTIME = 111;
const int N_WEAK_ENEMY_BECOMES_VISIBLE = 69;
const int N_STRONG_ENEMY_BECOMES_VISIBLE = 70;
const int N_CERTAIN_TYPE_ENEMY_BECOMES_VISIBLE = 71;
const int N_ENEMY_BECOMES_VISIBLE_IN_TURNBASED = 72;
const int N_LAST_PIECE_OF_AMMO = 73;
const int N_TARGET_HIT = 75;
const int N_TARGET_MISSED = 76;
const int N_INFLICT_CRITICAL_DAMAGE_TO_ENEMY = 77;
const int N_ACCIDENTALLY_WOUND_FRIEND = 78;
const int N_HARD_TARGET_HIT = 79;
const int N_FRIEND_TARGET_MISSED = 80;
const int N_FRIEND_INFLICTS_CRITICAL_DAMAGE_TO_ENEMY = 81;
const int N_SUFFERS_LIGHT_DAMAGE = 82;
const int N_ENEMY_MISSED_TARGET = 83;
const int N_PC_SUFFER_CRITICAL_DAMAGE = 84;
const int N_SUFFERS_HARD_DAMAGE = 85;
const int N_KILLED_AN_ENEMY = 86;
const int N_FINAL_WORLDS_BEFORE_DEATH = 87;
const int N_KILLED_CERTAIN_TYPE_ENEMY = 88;
const int N_FRIEND_DIES = 89;
const int N_KILLING_Nth_ENEMY_IN_A_ROW = 90;
const int N_Nth_UNIT_WAS_KILLED = 91;
const int N_HEAL_OF_FRIEND_FINISHED = 92;
const int N_CANNOT_FINISH_HEAL = 93;
const int N_SELF_HEAL_FINISHED = 94;
const int N_GRENADE_KILLS_MORE_THAN_ONE_ENEMY = 95;
const int N_FRIEND_GRENADE_KILLS_MORE_THAN_ONE_ENEMY = 96;
const int N_GRENADE_DESTROYS_A_LOT_OF_OBJECTS = 97;
const int N_FRIEND_GRENADE_DESTROYS_A_LOT_OF_OBJECTS = 98;
const int N_INTERRUPTED_ENEMY = 99;
const int N_SKILL_INCREASED = 100;
const int N_IMPOSSIBLE_TO_PERFORM_ACTION = 101;
const int N_ORDER_CONFIRMATION = 102;
const int N_WEAPON_JAMMED = 103;
const int N_ABOUT_TO_TAKE_AN_ENEMY_BY_SURPRISE = 105;
const int N_LONG_BURST = 114;
const int N_UNHIDE = 115;
const int N_N_PERCENT_OF_GROUP_IS_KILLED = 118;	// retail CreateAck @0x330b10 case 0x76 -> CAckNPercentOfGroupIsKilled
////////////////////////////////////////////////////////////////////////////////////////////////////
CAckBase *CreateAck( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck )
{
	ASSERT( IsValid( _pUnit ) );
	ASSERT( IsValid( _pDBAck ) );
	//
	CAckBase *pRes = 0;
	switch ( _pDBAck->nConditionID )
	{
		case N_ENEMY_BECOMES_VISIBLE_IN_REALTIME:
		case N_ABOUT_TO_TAKE_AN_ENEMY_BY_SURPRISE:
			pRes = new CAckEnemyBecomesVisibleInRealtime( _pUnit, _pDBAck );
			break;
		case N_WEAK_ENEMY_BECOMES_VISIBLE:
			pRes = new CAckWeakEnemyBecomesVisible( _pUnit, _pDBAck );
			break;
		case N_STRONG_ENEMY_BECOMES_VISIBLE:
			pRes = new CAckStrongEnemyBecomesVisible( _pUnit, _pDBAck );
			break;
		case N_CERTAIN_TYPE_ENEMY_BECOMES_VISIBLE:
			pRes = new CAckCertainTypeEnemyBecomesVisible( _pUnit, _pDBAck );
			break;
		case N_ENEMY_BECOMES_VISIBLE_IN_TURNBASED:
			pRes = new CAckEnemyBecomesVisibleInTurnbased( _pUnit, _pDBAck );
			break;
		case N_LAST_PIECE_OF_AMMO:
			pRes = new CAckLastPieceOfAmmo( _pUnit, _pDBAck );
			break;
		case N_IMPOSSIBLE_TO_PERFORM_ACTION:
			pRes = new CAckImpossibleToPerformAction( _pUnit, _pDBAck );
			break;
		case N_ORDER_CONFIRMATION:
			pRes = new CAckOrderConfirmation( _pUnit, _pDBAck );
			break;
		case N_WEAPON_JAMMED:
			pRes = new CAckWeaponJammed( _pUnit, _pDBAck );
			break;
		case N_TARGET_HIT:
			pRes = new CAckTargetHit( _pUnit, _pDBAck );
			break;
		case N_HARD_TARGET_HIT:
			pRes = new CAckHardTargetHit( _pUnit, _pDBAck );
			break;
		case N_TARGET_MISSED:
			pRes = new CAckTargetMissed( _pUnit, _pDBAck );
			break;
		case N_FINAL_WORLDS_BEFORE_DEATH:
			pRes = new CAckUnitDied( _pUnit, _pDBAck );
			break;
		case N_FRIEND_DIES:
			pRes = new CAckFriendDies( _pUnit, _pDBAck );
			break;
		case N_FRIEND_TARGET_MISSED:
			pRes = new CAckFriendTargetMissed( _pUnit, _pDBAck );
			break;
		case N_ACCIDENTALLY_WOUND_FRIEND:
			pRes = new CAckAccidentallyWoundFriend( _pUnit, _pDBAck );
			break;
		case N_INFLICT_CRITICAL_DAMAGE_TO_ENEMY:
			pRes = new CAckInflictCriticalDamageToEnemy( _pUnit, _pDBAck );
			break;
		case N_FRIEND_INFLICTS_CRITICAL_DAMAGE_TO_ENEMY:
			pRes = new CAckFriendInflictsCriticalDamageToEnemy( _pUnit, _pDBAck );
			break;
		case N_PC_SUFFER_CRITICAL_DAMAGE:
			pRes = new CAckPCSufferCriticalDamage( _pUnit, _pDBAck );
			break;
		case N_KILLED_AN_ENEMY:
			pRes = new CAckKilledAnEnemy( _pUnit, _pDBAck );
			break;
		case N_KILLED_CERTAIN_TYPE_ENEMY:
			pRes = new CAckKilledCertainTypeEnemy( _pUnit, _pDBAck );
			break;
		case N_KILLING_Nth_ENEMY_IN_A_ROW:
			pRes = new CAckKillingNthEnemyInARow( _pUnit, _pDBAck );
			break;
		case N_Nth_UNIT_WAS_KILLED:
			pRes = new CAckNthUnitWasKilled( _pUnit, _pDBAck );
			break;
		case N_SUFFERS_LIGHT_DAMAGE:
			pRes = new CAckSuffersLightDamage( _pUnit, _pDBAck );
			break;
		case N_SUFFERS_HARD_DAMAGE:
			pRes = new CAckSuffersHardDamage( _pUnit, _pDBAck );
			break;
		case N_ENEMY_MISSED_TARGET:
			pRes = new CAckEnemyMissedTarget( _pUnit, _pDBAck );
			break;
		case N_INTERRUPTED_ENEMY:
			pRes = new CAckInterruptedEnemy( _pUnit, _pDBAck );
			break;
		case N_SKILL_INCREASED:
			pRes = new CAckSkillIncreased( _pUnit, _pDBAck );
			break;
		case N_GRENADE_KILLS_MORE_THAN_ONE_ENEMY:
			pRes = new CAckGrenadeKillsMoreThanOneEnemy( _pUnit, _pDBAck );
			break;
		case N_FRIEND_GRENADE_KILLS_MORE_THAN_ONE_ENEMY:
			pRes = new CAckFriendGrenadeKillsMoreThanOneEnemy( _pUnit, _pDBAck );
			break;
		case N_GRENADE_DESTROYS_A_LOT_OF_OBJECTS:
			pRes = new CAckGrenadeDestroysALotOfObjects( _pUnit, _pDBAck );
			break;
		case N_FRIEND_GRENADE_DESTROYS_A_LOT_OF_OBJECTS:
			pRes = new CAckFriendGrenadeDestroysALotOfObjects( _pUnit, _pDBAck );
			break;
		case N_HEAL_OF_FRIEND_FINISHED:
			pRes = new CAckHealOfFriendFinished( _pUnit, _pDBAck );
			break;
		case N_CANNOT_FINISH_HEAL:
			pRes = new CAckCannotFinishHeal( _pUnit, _pDBAck );
			break;
		case N_SELF_HEAL_FINISHED:
			pRes = new CAckSelfHealFinished( _pUnit, _pDBAck );
			break;
		case N_ENEMY_BECOMES_VISIBLE:
			pRes = new CAckEnemyBecomesVisible( _pUnit, _pDBAck );
			break;
		case N_LONG_BURST:
			pRes = new CAckLongBurst( _pUnit, _pDBAck );
			break;
		case N_UNHIDE:
			pRes = new CAckUnhide( _pUnit, _pDBAck );
			break;
		case N_N_PERCENT_OF_GROUP_IS_KILLED:
			pRes = new CAckNPercentOfGroupIsKilled( _pUnit, _pDBAck );
			break;
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NWorld;
//
REGISTER_SAVELOAD_CLASS( 0x52032120, CAckFriendGrenadeKillsMoreThanOneEnemy );
REGISTER_SAVELOAD_CLASS( 0x52032121, CAckGrenadeKillsMoreThanOneEnemy );
REGISTER_SAVELOAD_CLASS( 0x52912145, CAckEnemyHasBeenInfictedCritical );
REGISTER_SAVELOAD_CLASS( 0x51232163, CAckFriendDies );
REGISTER_SAVELOAD_CLASS( 0x51953110, CAckNPercentOfGroupIsKilled );	// retail classreg id 1368731920
REGISTER_SAVELOAD_CLASS( 0x50732150, CAckEnemyBecomesVisibleInRealtime );
REGISTER_SAVELOAD_CLASS( 0x52912140, CAckEnemyBecomesVisibleInTurnbased );
REGISTER_SAVELOAD_CLASS( 0x52912141, CAckLastPieceOfAmmo );
REGISTER_SAVELOAD_CLASS( 0x52912142, CAckSuffersLightDamage );
REGISTER_SAVELOAD_CLASS( 0x52912143, CAckSuffersHardDamage );
REGISTER_SAVELOAD_CLASS( 0x52912144, CAckUnitDied );
REGISTER_SAVELOAD_CLASS( 0x51232160, CAckTargetHit );
REGISTER_SAVELOAD_CLASS( 0x51232161, CAckHardTargetHit );
REGISTER_SAVELOAD_CLASS( 0x51232162, CAckTargetMissed );
REGISTER_SAVELOAD_CLASS( 0x50542190, CAckFriendGrenadeDestroysALotOfObjects );
REGISTER_SAVELOAD_CLASS( 0x50542191, CAckGrenadeDestroysALotOfObjects );
REGISTER_SAVELOAD_CLASS( 0x50542192, CAckFriendTargetMissed );
REGISTER_SAVELOAD_CLASS( 0x52062150, CAckWeakEnemyBecomesVisible );
REGISTER_SAVELOAD_CLASS( 0x52062151, CAckStrongEnemyBecomesVisible );
REGISTER_SAVELOAD_CLASS( 0x52062152, CAckCertainTypeEnemyBecomesVisible );
REGISTER_SAVELOAD_CLASS( 0x52062090, CAckWeaponJammed );
REGISTER_SAVELOAD_CLASS( 0x52062091, CAckOrderConfirmation );
REGISTER_SAVELOAD_CLASS( 0x52062092, CAckImpossibleToPerformAction );
REGISTER_SAVELOAD_CLASS( 0x52062093, CAckAccidentallyWoundFriend );
REGISTER_SAVELOAD_CLASS( 0x52162090, CAckInflictCriticalDamageToEnemy );
REGISTER_SAVELOAD_CLASS( 0x52162091, CAckFriendInflictsCriticalDamageToEnemy );
REGISTER_SAVELOAD_CLASS( 0x52162092, CAckPCSufferCriticalDamage );
REGISTER_SAVELOAD_CLASS( 0x52162093, CAckKilledAnEnemy );
REGISTER_SAVELOAD_CLASS( 0x52162094, CAckKilledCertainTypeEnemy );
REGISTER_SAVELOAD_CLASS( 0x52162095, CAckKillingNthEnemyInARow );
REGISTER_SAVELOAD_CLASS( 0x52162096, CAckNthUnitWasKilled );
REGISTER_SAVELOAD_CLASS( 0x52162097, CAckEnemyMissedTarget );
REGISTER_SAVELOAD_CLASS( 0x52162098, CAckInterruptedEnemy );
REGISTER_SAVELOAD_CLASS( 0x52162099, CAckSkillIncreased );
REGISTER_SAVELOAD_CLASS( 0x52162100, CAckHealOfFriendFinished );
REGISTER_SAVELOAD_CLASS( 0x52162101, CAckSelfHealFinished );
REGISTER_SAVELOAD_CLASS( 0x52162102, CAckCannotFinishHeal );
REGISTER_SAVELOAD_CLASS( 0x52162050, CAckEnemyBecomesVisible );
REGISTER_SAVELOAD_CLASS( 0x52122180, CAckLongBurst );
REGISTER_SAVELOAD_CLASS( 0x52122181, CAckUnhide );