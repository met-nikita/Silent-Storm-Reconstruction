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
#include "RPGItemInfo.h"	// item interfaces for GetRatingDifference (grenade/melee/weapon)
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
		// retail @0x331c00: the single IsFriend gate (both units alive, different units, same
		// player) -- the Jan03 raw player compare had no liveness gates.
		if ( IsFriend( _pUnit ) )
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
		// retail gate order @0x3384e0: OWNER live -> !IsThis(victim) -> IsFriend(victim)
		// (the helpers carry the liveness gates; Jan03 raw-compared players instead)
		if ( !IsValid( GetUnit() ) )
			return;
		if ( IsThis( _pUnit ) || !IsFriend( _pUnit ) )
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
		// retail: ICF-folded with CAckFriendDies::OnUnitDied @0x331c00 (vftable VA 0x8c7e0c slot
		// +0x2c proves it) -- the single IsFriend gate, replacing the Jan03 raw player compare.
		if ( IsFriend( _pUnit ) )
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
	// NOTE: dead in retail exactly as here -- CreateAck @0x330b10 has no case for this class (it is
	// classreg-only, id 0x52912145) and nothing dispatches OnCritical; the body is kept source-1:1.
	virtual void OnCritical( CUnitServer *_pUnit )
	{
		// retail @0x333f20: IsEnemy first, then the CanSee sight gate (Jan03: raw player compare)
		if ( IsEnemy( _pUnit ) && CanSee( _pUnit ) )
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
		// retail @0x331cc0: IsFriend(thrower) + more than one unit destroyed (Jan03: raw compares)
		if ( IsFriend( _pUnit ) && nUnitsDestroyed > 1 )
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
		// retail @0x331d90: IsThis(thrower) -- adds the owner-liveness gate over the Jan03 raw compare
		if ( IsThis( _pUnit ) && nUnitsDestroyed > 1 )
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
		// retail @0x331e60: IsFriend(thrower), then sParam[0] <= objects destroyed
		if ( IsFriend( _pUnit ) &&
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
		// retail @0x331f40: IsThis(thrower), then sParam[0] <= objects destroyed
		if ( IsThis( _pUnit ) &&
			nObjectsDestroyed >= atoi( GetDBAck()->sParam[0].c_str() ) )
				PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail reworked the whole "enemy becomes visible" family from the Jan03 IAck virtual
// (OnEnemyBecomesVisible, fanned out by CGlobalAck) onto the typed event bus: each class embeds a
// CEventRegister<class, CEventOnSeeNewEnemy> (ctors @0x331f80/@0x334150 et al.), the retail IAck
// vftable has NO OnEnemyBecomesVisible slot (24 handler slots, mapping complete via the CGlobalAck
// dispatchers @0x3389c0..0x338df0), and the event is raised by CUnitServer::UpdateVisible (the
// dev ThrowEvent site already matches retail). Predicates moved onto the CAckBase helpers.
class CAckEnemyBecomesVisibleInRealtime: public CAckBase
{
	OBJECT_BASIC_METHODS(CAckEnemyBecomesVisibleInRealtime);
	NGlobal::CEventRegister< CAckEnemyBecomesVisibleInRealtime, NWorld::CEventOnSeeNewEnemy > registerEvent;
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckEnemyBecomesVisibleInRealtime( CUnitServer *_pUnit = 0, NDb::CDBAck *_pDBAck = 0 ):
		registerEvent( this, &CAckEnemyBecomesVisibleInRealtime::OnEvent ), CAckBase( _pUnit, _pDBAck ) {}
	//
	void OnEvent( const CEventOnSeeNewEnemy &event )
	{
		// retail @0x3316a0: IsThis(watcher) + the real-time flag; the target is not consulted
		if ( IsThis( event.pWatcher ) && event.bRealTime )
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::CAckPCHasBeenSpottedByTheEnemy (release-added; NO Jan03 counterpart, NO CreateAck
// case -- classreg-only, retail id 0x51953111 raw-disasm'd from the registrar init stub @0x4a6eb0,
// factory NewCAckPCHasBeenSpottedByTheEnemy @0x3363f0, default ctor @0x3342c0). Subscribed to the
// same CEventOnSeeNewEnemy bus with the roles SWAPPED: MY unit is the freshly-SPOTTED target and
// the hostile watcher just acquired it.
class CAckPCHasBeenSpottedByTheEnemy: public CAckBase
{
	OBJECT_BASIC_METHODS(CAckPCHasBeenSpottedByTheEnemy);
	NGlobal::CEventRegister< CAckPCHasBeenSpottedByTheEnemy, NWorld::CEventOnSeeNewEnemy > registerEvent;
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckPCHasBeenSpottedByTheEnemy( CUnitServer *_pUnit = 0, NDb::CDBAck *_pDBAck = 0 ):
		registerEvent( this, &CAckPCHasBeenSpottedByTheEnemy::OnEvent ), CAckBase( _pUnit, _pDBAck ) {}
	//
	void OnEvent( const CEventOnSeeNewEnemy &event )
	{
		// retail @0x3316d0: IsThis(target) gates first, then IsEnemy(watcher)
		if ( IsThis( event.pTarget ) && IsEnemy( event.pWatcher ) )
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckEnemyBecomesVisible: public CAckBase
{
	OBJECT_BASIC_METHODS(CAckEnemyBecomesVisible);
	NGlobal::CEventRegister< CAckEnemyBecomesVisible, NWorld::CEventOnSeeNewEnemy > registerEvent;
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckEnemyBecomesVisible( CUnitServer *_pUnit = 0, NDb::CDBAck *_pDBAck = 0 ):
		registerEvent( this, &CAckEnemyBecomesVisible::OnEvent ), CAckBase( _pUnit, _pDBAck ) {}
	//
	void OnEvent( const CEventOnSeeNewEnemy &event )
	{
		// retail @0x331720: IsThis(watcher) then IsEnemy(target) -- diplomacy, not the Jan03
		// "different player" compare (allied players no longer count as enemies)
		if ( IsThis( event.pWatcher ) && IsEnemy( event.pTarget ) )
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckWeakEnemyBecomesVisible: public CAckBase
{
	OBJECT_BASIC_METHODS(CAckWeakEnemyBecomesVisible);
	NGlobal::CEventRegister< CAckWeakEnemyBecomesVisible, NWorld::CEventOnSeeNewEnemy > registerEvent;
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckWeakEnemyBecomesVisible( CUnitServer *_pUnit = 0, NDb::CDBAck *_pDBAck = 0 ):
		registerEvent( this, &CAckWeakEnemyBecomesVisible::OnEvent ), CAckBase( _pUnit, _pDBAck ) {}
	//
	void OnEvent( const CEventOnSeeNewEnemy &event )
	{
		// retail @0x331790: IsThis(watcher) + the release-ADDED IsEnemy(target) gate, then the
		// level test  delta + targetLevel <= watcherLevel  (identical math to Jan03)
		if ( IsThis( event.pWatcher ) && IsEnemy( event.pTarget ) )
		{
			int nWatcherLevel = event.pWatcher->GetUnitRPG()->GetRPGUnit()->Skills( NDb::ST_LEVEL );
			int nTargetLevel = event.pTarget->GetUnitRPG()->GetRPGUnit()->Skills( NDb::ST_LEVEL );
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
	NGlobal::CEventRegister< CAckStrongEnemyBecomesVisible, NWorld::CEventOnSeeNewEnemy > registerEvent;
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckStrongEnemyBecomesVisible( CUnitServer *_pUnit = 0, NDb::CDBAck *_pDBAck = 0 ):
		registerEvent( this, &CAckStrongEnemyBecomesVisible::OnEvent ), CAckBase( _pUnit, _pDBAck ) {}
	//
	void OnEvent( const CEventOnSeeNewEnemy &event )
	{
		// retail @0x331840: IsThis(watcher) + the release-ADDED IsEnemy(target) gate, then the
		// level test  delta + watcherLevel <= targetLevel  (identical math to Jan03)
		if ( IsThis( event.pWatcher ) && IsEnemy( event.pTarget ) )
		{
			int nWatcherLevel = event.pWatcher->GetUnitRPG()->GetRPGUnit()->Skills( NDb::ST_LEVEL );
			int nTargetLevel = event.pTarget->GetUnitRPG()->GetRPGUnit()->Skills( NDb::ST_LEVEL );
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
		// retail @0x332840: IsThis + CanSee + IsEnemy. The Jan03 predecessor compared raw players
		// (any different player counted, and no sight gate); retail barks only about a critical
		// the owner inflicted on a SEEN unit its diplomacy calls ENEMY.
		if ( IsThis( pAttacker ) && CanSee( pTarget ) && IsEnemy( pTarget ) )
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
		// retail @0x3326a0: IsFriend(attacker) + CanSee(target) + IsEnemy(target) -- the OWNER
		// must see the victim; the Jan03 predecessor only compared players (no sight gate).
		if ( IsFriend( pAttacker ) && CanSee( pTarget ) && IsEnemy( pTarget ) )
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
		// retail @0x332780: IsThis(pTarget) -- only the unit that ACTUALLY suffered the crit barks the
		// "PC suffers critical" line. The Jan03 predecessor filter (GetUnit()->GetPlayer() != pTarget->GetPlayer())
		// made every player-hero ack-owner bark "I'm hurt" whenever an ENEMY was critted (different player).
		if ( IsThis( pTarget ) )
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
		// retail @0x332840 (body ICF-folded with CAckInflictCriticalDamageToEnemy::OnDoCriticalDamage;
		// proven via the CAckKilledAnEnemy vftable 0x8c8604 slot +0x40): IsThis + CanSee + IsEnemy.
		// The killer barks only about a kill he SAW of a unit his diplomacy calls ENEMY -- no bark
		// when the victim was never visible (e.g. suppressive fire at a heard-only target).
		if ( IsThis( pAttacker ) && CanSee( pTarget ) && IsEnemy( pTarget ) )
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
		// retail @0x332930: the same IsThis + CanSee + IsEnemy gate as CAckKilledAnEnemy,
		// then the victim's RPG class record id must match the ack's sParam[0].
		if ( IsThis( pAttacker ) && CanSee( pTarget ) && IsEnemy( pTarget ) )
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
		// retail @0x332a80: IsThis + CanSee + IsEnemy, then count toward sParam[0]. NOTE the
		// Jan03 predecessor RESET the streak on a same-player kill; retail has no such clause
		// (the streak only resets via OnNewTurnStarted/OnRealTimeStarted).
		if ( IsThis( pAttacker ) && CanSee( pTarget ) && IsEnemy( pTarget ) )
		{
			++nKilled;
			bKilledLastTurn = true;
			int nParam = atoi( GetDBAck()->sParam[0].c_str() );
			if ( nParam == nKilled )
				PlayAck();
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
		// retail @0x332ba0: counts deaths of FRIENDS only -- pTarget valid, NOT the owner itself
		// (IsThis excluded), same player (IsFriend). pAttacker is ignored. The Jan03 predecessor
		// also counted the owner's own death.
		if ( !IsValid( pTarget ) )
			return;
		if ( IsThis( pTarget ) || !IsFriend( pTarget ) )
			return;
		++nKilled;
		int nParam = atoi( GetDBAck()->sParam[0].c_str() );
		if ( nKilled == nParam )
			PlayAck();
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
		// retail @0x332ca0: CanSee + IsEnemy -- the owner must SEE the enemy who missed. The
		// Jan03 predecessor compared raw players (and deref'd _pUnit with no null guard).
		if ( CanSee( _pUnit ) && IsEnemy( _pUnit ) )
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
		// retail @0x332d70: pWho valid, IsThis(pWho), AND the world says the owner is the ACTIVE
		// unit (IWorld::IsUnitActive via the unit's world ptr; slot pinned between IsFirstTurn/
		// IsInterrupt in the IWorld vftable). Jan03 barked on the bare owner match.
		if ( !IsValid( pWho ) )
			return;
		if ( IsThis( pWho ) && GetWorld()->IsUnitActive( pWho ) )
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
// retail NWorld::CAckSuccessfulMeleeAttack (release-added; NO Jan03 counterpart -- factory case 74,
// ctors @0x333180/@0x335520, OnEvent COMDAT-folded @0x331700). Event-bus ack bound to
// CEventOnUnitSuccessfulMelee: the unit that just landed a melee blow barks. Modeled on CAckLongBurst.
class CAckSuccessfulMeleeAttack: public CAckBase
{
	OBJECT_BASIC_METHODS( CAckSuccessfulMeleeAttack );
	NGlobal::CEventRegister< CAckSuccessfulMeleeAttack, NWorld::CEventOnUnitSuccessfulMelee > registerEvent;
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckSuccessfulMeleeAttack( CUnitServer *_pUnit = 0, NDb::CDBAck *_pDBAck = 0 ):
		registerEvent( this, &CAckSuccessfulMeleeAttack::OnEvent ), CAckBase( _pUnit, _pDBAck ) {}
	//
	void OnEvent( const CEventOnUnitSuccessfulMelee &event )
	{
		if ( GetUnit() == event.pWho )		// retail folds this through CAckBase::IsThis @0x331700
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::CAckDiscoveringMineNearby (release-added; factory case 106, ctors @0x332060/@0x334440,
// OnEvent folded @0x331700). Bound to CEventOnSpotMineOrTrap (fired by CUnitServer::UpdateVisible when
// a unit newly spots a set mine/trap): that unit barks. Modeled on CAckLongBurst.
class CAckDiscoveringMineNearby: public CAckBase
{
	OBJECT_BASIC_METHODS( CAckDiscoveringMineNearby );
	NGlobal::CEventRegister< CAckDiscoveringMineNearby, NWorld::CEventOnSpotMineOrTrap > registerEvent;
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckDiscoveringMineNearby( CUnitServer *_pUnit = 0, NDb::CDBAck *_pDBAck = 0 ):
		registerEvent( this, &CAckDiscoveringMineNearby::OnEvent ), CAckBase( _pUnit, _pDBAck ) {}
	//
	void OnEvent( const CEventOnSpotMineOrTrap &event )
	{
		if ( GetUnit() == event.pWho )		// retail folds this through CAckBase::IsThis @0x331700
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::CAckNPCInteraction (release-added; factory case 113, ctors @0x333340/@0x335800,
// OnEvent folded @0x331700). Bound to CEventOnNotHeroWantsToTalk (fired when a non-hero unit is told
// to talk): that unit barks. Modeled on CAckLongBurst.
class CAckNPCInteraction: public CAckBase
{
	OBJECT_BASIC_METHODS( CAckNPCInteraction );
	NGlobal::CEventRegister< CAckNPCInteraction, NWorld::CEventOnNotHeroWantsToTalk > registerEvent;
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckNPCInteraction( CUnitServer *_pUnit = 0, NDb::CDBAck *_pDBAck = 0 ):
		registerEvent( this, &CAckNPCInteraction::OnEvent ), CAckBase( _pUnit, _pDBAck ) {}
	//
	void OnEvent( const CEventOnNotHeroWantsToTalk &event )
	{
		if ( GetUnit() == event.pWho )		// retail folds this through CAckBase::IsThis @0x331700
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::GetRating @0x330840 -- an item's store rating, or 0 for a null/dead item or one with
// no store record (CRPGItem::pStoreItem->nRating, @0x4280a0).
static int GetRating( NRPG::IInventoryItem *pItem )
{
	if ( !IsValid( pItem ) )
		return 0;
	NDb::CRPGItem *pDBItem = pItem->GetDBItem();
	if ( !IsValid( pDBItem ) )
		return 0;
	NDb::CRPGStoreItem *pStore = pDBItem->pStoreItem;	// plain extraction (no ternary over CPtr -- UAF rule)
	if ( !IsValid( pStore ) )
		return 0;
	return pStore->nRating;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::GetRatingDifference @0x3308d0 -- how much better the freshly-given item (pNew) rates
// than the one it replaced (pOld). Same-kind grenades and (matching) melee/firearms compare by store
// rating; a couple of firearm-class transitions carry a fixed +/-99 verdict. Anything mismatched -> 0.
// The SMG(3)<->rocket-launcher(7) special cases and the direction are taken from the retail decomp
// (call convention: pNew in EAX, pOld on the stack).
static int GetRatingDifference( NRPG::IInventoryItem *pNew, NRPG::IInventoryItem *pOld )
{
	if ( !IsValid( pNew ) || !IsValid( pOld ) )
		return 0;
	// both grenades -> plain rating difference
	CDynamicCast<NRPG::IGrenadeItemInfo> pGrenNew( pNew ), pGrenOld( pOld );
	if ( pGrenNew && pGrenOld )
		return GetRating( pNew ) - GetRating( pOld );
	// both melee weapons -> require the same bThrowing class, then rating difference
	CDynamicCast<NRPG::IMeleeWeaponItem> pMeleeNew( pNew ), pMeleeOld( pOld );
	if ( pMeleeNew && pMeleeOld )
	{
		if ( pMeleeNew->GetDBMeleeWeapon()->bThrowing == pMeleeOld->GetDBMeleeWeapon()->bThrowing )
			return GetRating( pNew ) - GetRating( pOld );
		return 0;
	}
	// both firearms -> require the same bBazookaLogic; bazooka logic compares by rating, otherwise the
	// anim-weapon-type must match (rating diff) except the SMG<->rocket-launcher swaps (+/-99).
	CDynamicCast<NRPG::IWeaponItemInfo> pWpnNew( pNew ), pWpnOld( pOld );
	if ( pWpnNew && pWpnOld )
	{
		NDb::CRPGWeapon *pDBNew = pWpnNew->GetDBWeapon();
		NDb::CRPGWeapon *pDBOld = pWpnOld->GetDBWeapon();
		if ( pDBNew->bBazookaLogic != pDBOld->bBazookaLogic )
			return 0;
		if ( !pDBNew->bBazookaLogic )
		{
			NDb::EWeaponType eNew = pDBNew->pAnimWeaponType->type;
			NDb::EWeaponType eOld = pDBOld->pAnimWeaponType->type;
			if ( eNew != eOld )
			{
				if ( eOld == NDb::WT_SUB_MACHINE_GUN && eNew == NDb::WT_RLAUNCHER )
					return 99;
				if ( eOld != NDb::WT_RLAUNCHER )
					return 0;
				if ( eNew != NDb::WT_SUB_MACHINE_GUN )
					return 0;
				return -99;
			}
		}
		return GetRating( pNew ) - GetRating( pOld );
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::CAckGoodItemGiven (release-added; factory case 107, ctors @0x333420/@0x335970,
// OnEvent @0x331940). Bound to CEventOnItemGiven: the receiving unit barks when the freshly-equipped
// item out-ranks the one it replaced by more than one step. Modeled on CAckLongBurst.
class CAckGoodItemGiven: public CAckBase
{
	OBJECT_BASIC_METHODS( CAckGoodItemGiven );
	NGlobal::CEventRegister< CAckGoodItemGiven, NWorld::CEventOnItemGiven > registerEvent;
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckGoodItemGiven( CUnitServer *_pUnit = 0, NDb::CDBAck *_pDBAck = 0 ):
		registerEvent( this, &CAckGoodItemGiven::OnEvent ), CAckBase( _pUnit, _pDBAck ) {}
	//
	void OnEvent( const CEventOnItemGiven &event )
	{
		if ( GetUnit() == event.pUnit && GetRatingDifference( event.pNewItem, event.pOldItem ) > 1 )
			PlayAck();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::CAckBadItemGiven (release-added; factory case 108, ctors @0x333500/@0x335ae0,
// OnEvent @0x331980). Mirror of CAckGoodItemGiven: barks when the freshly-equipped item rates more
// than one step BELOW the one it replaced.
class CAckBadItemGiven: public CAckBase
{
	OBJECT_BASIC_METHODS( CAckBadItemGiven );
	NGlobal::CEventRegister< CAckBadItemGiven, NWorld::CEventOnItemGiven > registerEvent;
	ZDATA
	ZPARENT( CAckBase );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAckBase *)this); return 0; }
public:
	CAckBadItemGiven( CUnitServer *_pUnit = 0, NDb::CDBAck *_pDBAck = 0 ):
		registerEvent( this, &CAckBadItemGiven::OnEvent ), CAckBase( _pUnit, _pDBAck ) {}
	//
	void OnEvent( const CEventOnItemGiven &event )
	{
		if ( GetUnit() == event.pUnit && GetRatingDifference( event.pNewItem, event.pOldItem ) < -1 )
			PlayAck();
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
// retail NWorld::CAckNoPlaceInInventory (release-added; factory case 104, ctors @0x333c40/@0x336150).
// A bare CAckBase leaf: the unit barks when its inventory has no room for an offered item. Broadcast
// through CGlobalAck::OnNoPlaceInInventory @0x338c30 (a plain vAck fan-out, see wAckBase.cpp).
DEFINE_UNITSERVER_ACK( NoPlaceInInventory );
////////////////////////////////////////////////////////////////////////////////////////////////////
// Condition ID-s
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_ENEMY_BECOMES_VISIBLE = 68;
const int N_ENEMY_BECOMES_VISIBLE_IN_REALTIME = 111;
const int N_SUCCESSFUL_MELEE_ATTACK = 74;	// retail CreateAck case -> CAckSuccessfulMeleeAttack
const int N_NO_PLACE_IN_INVENTORY = 104;	// retail CreateAck case -> CAckNoPlaceInInventory
const int N_DISCOVERING_MINE_NEARBY = 106;	// retail CreateAck case -> CAckDiscoveringMineNearby
const int N_GOOD_ITEM_GIVEN = 107;			// retail CreateAck case -> CAckGoodItemGiven
const int N_BAD_ITEM_GIVEN = 108;			// retail CreateAck case -> CAckBadItemGiven
const int N_NPC_INTERACTION = 113;			// retail CreateAck case -> CAckNPCInteraction
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
		case N_SUCCESSFUL_MELEE_ATTACK:
			pRes = new CAckSuccessfulMeleeAttack( _pUnit, _pDBAck );
			break;
		case N_DISCOVERING_MINE_NEARBY:
			pRes = new CAckDiscoveringMineNearby( _pUnit, _pDBAck );
			break;
		case N_NO_PLACE_IN_INVENTORY:
			pRes = new CAckNoPlaceInInventory( _pUnit, _pDBAck );
			break;
		case N_GOOD_ITEM_GIVEN:
			pRes = new CAckGoodItemGiven( _pUnit, _pDBAck );
			break;
		case N_BAD_ITEM_GIVEN:
			pRes = new CAckBadItemGiven( _pUnit, _pDBAck );
			break;
		case N_NPC_INTERACTION:
			pRes = new CAckNPCInteraction( _pUnit, _pDBAck );
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
// retail classreg ids (release-added ack family)
REGISTER_SAVELOAD_CLASS( 0x51953112, CAckSuccessfulMeleeAttack );
REGISTER_SAVELOAD_CLASS( 0x51953113, CAckDiscoveringMineNearby );
REGISTER_SAVELOAD_CLASS( 0x230753C0, CAckNoPlaceInInventory );
REGISTER_SAVELOAD_CLASS( 0x23074C00, CAckGoodItemGiven );
REGISTER_SAVELOAD_CLASS( 0x23074C01, CAckBadItemGiven );
REGISTER_SAVELOAD_CLASS( 0x50133130, CAckNPCInteraction );