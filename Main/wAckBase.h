#ifndef __WACKBASE_H_
#define __WACKBASE_H_
#include "..\DBFormat\DataAck.h"

namespace NDb
{
	class CDBAckSequence;
	class CDBAck;
	class CDBAckInfo;
	class CRPGPers;
}

namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_ACK_CRITICAL = 0;
const int N_ACK_DEATH = 1;
const int N_ACK_SKILL = 2;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnit;
class CWorld;
class IPlayer;
class CUnitServer;
class CDumbUnitServer;
////////////////////////////////////////////////////////////////////////////////////////////////////
// IAck           ( Ack == Acknowledgement )
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAck: virtual public CObjectBase
{
public:
	//
	virtual NDb::CDBAck *GetDBAck() { return 0; }
	// event handlers
	virtual void OnSegment() {} // segment
	virtual void OnEnemyBecomesVisible( CUnitServer *pWatcher, 
		CUnitServer *pTarget, bool bRealTime ) {}
	virtual void OnLastPieceOfAmmo( CUnitServer *pUnit ) {} // out of ammo
	virtual void OnWeaponJammed( CUnitServer *pUnit ) {} // weapon jammed
	virtual void OnOrderConfirmation( CUnitServer *pUnit ) {} // order confirmation
	virtual void OnImpossibleToPerformAction( CUnitServer *pUnit ) {} // cannot perform command
	virtual void OnSuffersLightDamage( CUnitServer *pUnit ) {} // took light damage
	virtual void OnSuffersHardDamage( CUnitServer *pUnit ) {} // took heavy damage
	virtual void OnUnitDied( CUnitServer *pUnit ) {} // unit died
	virtual void OnTargetHit( CUnitServer *pUnit ) {} // Unit hit the target
	virtual void OnHardTargetHit( CUnitServer *pUnit ) {} // Unit hit a hard target
	virtual void OnTargetMissed( CUnitServer *pUnit ) {} // Unit missed the target
	virtual void OnGrenadeExplosion( CUnitServer *pUnit, int nUnitsDestroyed, int nObjectsDestroyed ) {} // damage from grenade
	virtual void OnDoDamage( CUnitServer *pAttacker, CUnitServer *pTarget ) {}
	virtual void OnDoAccidentalDamage( CUnitServer *pAttacker, CUnitServer *pTarget ) {}
	virtual void OnDoCriticalDamage( CUnitServer *pAttacker, CUnitServer *pTarget ) {}
	virtual void OnUnitWasKilled( CUnitServer *pAttacker, CUnitServer *pTarget ) {}
	virtual void OnNewTurnStarted( IPlayer *pPlayer ) {}
	virtual void OnRealTimeStarted() {}
	virtual void OnInterrupt( CUnitServer *pWho ) {}
	virtual void OnSkillIncreased( CUnitServer *pWho ) {}
	virtual void OnCannotFinishHeal( CUnitServer *pHealer, CUnitServer *pTarget ) {}
	virtual void OnHealFinished( CUnitServer *pHealer, CUnitServer *pTarget ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGlobalAck
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGlobalAck: public IAck
{
  OBJECT_BASIC_METHODS(CGlobalAck);
	// Retail @0x33a350: a queued ack-sequence now also carries the unit that triggered it (both the
	// CPtr<CUnitServer> and the CDBPtr<CDBAck> are refcounted), so the global ack can tie a pending bark
	// to its speaker. Serialized as { tag2 pUS, tag3 pAck } -- mirrors NWorld::CGlobalAck::SAck::operator&.
	struct SAck
	{
		CPtr<CUnitServer> pUS;
		CDBPtr<NDb::CDBAck> pAck;
		SAck() : pUS(0), pAck(0) {}
		SAck( CUnitServer *_pUS, NDb::CDBAck *_pAck ) : pUS(_pUS), pAck(_pAck) {}
		int operator&( CStructureSaver &f ) { f.Add(2,&pUS); f.Add(3,&pAck); return 0; }
	};
	ZDATA
	vector< CObj<IAck> > vAck;
	list< SAck > sequences; // accumulated sequences with the highest priority from triggered acks
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&vAck); f.Add(3,&sequences); return 0; }

	bool IsContainUnit( const list< CPtr<CUnit> > &visibleUnits, int nRPGPersID );
	bool IsSequenceVisible( const list< CPtr<CUnit> > &visibleUnits, NDb::CDBAckSequence *pSequence );
	void RemoveInvisibleSequences( IPlayer *pPlayer );
	// retail @0x339150: prune to the top-priority set (priority = seq.nPriority>0 else CONDITION
	// priority) and return that highest (>= 0). With the Steam game.db all AckSeqs priorities are 0,
	// so the condition fallback IS the retail priority system.
	int FetchHighestAcks();
public:
	CGlobalAck() {}
	// add to vAck
	void AddAck( CUnitServer *pUnit );
	// add to sequences
	void AddAckSequence( CUnitServer *pUnit, NDb::CDBAck *pAck ); 
	// get a random one from the accumulated sequences, accounting for who is watching
	// retail @0x339230 hands back the SPEAKER too (bool GetSequence(CUnitServer**, CDBAckSequence**, int*)):
	// the ack ROWS are keyed by the voice-DONOR pers id (GetAckPersID), which belongs to no live unit,
	// so the speaker can only come from the queued SAck itself -- re-deriving it by pers id finds nobody.
	// pnPriority = the retail int* out param: the FetchHighestAcks highest (the CONDITION-backed
	// priority CheckForAcks must seed the CAckEvent with -- the sequence's own field is always 0).
	virtual NDb::CDBAckSequence *GetSequence( IPlayer *pPlayer, CUnitServer **ppSpeaker = 0, int *pnPriority = 0 );
	void RemoveUnitAcks( CUnitServer *pUnit );
	// IAck
	virtual void OnSegment();
	virtual void OnEnemyBecomesVisible( CUnitServer *pWatcher, 
		CUnitServer *pTarget, bool bRealTime );
	virtual void OnLastPieceOfAmmo( CUnitServer *pUnit );
	virtual void OnWeaponJammed( CUnitServer *pUnit );
	virtual void OnOrderConfirmation( CUnitServer *pUnit );
	virtual void OnImpossibleToPerformAction( CUnitServer *pUnit );
	virtual void OnSuffersLightDamage( CUnitServer *pUnit );
	virtual void OnSuffersHardDamage( CUnitServer *pUnit );
	virtual void OnUnitDied( CUnitServer *pUnit );
	virtual void OnTargetHit( CUnitServer *pUnit );
	virtual void OnHardTargetHit( CUnitServer *pUnit );
	virtual void OnTargetMissed( CUnitServer *pUnit );
	virtual void OnGrenadeExplosion( CUnitServer *pUnit, int nUnitsDestroyed, int nObjectsDestroyed );
	virtual void OnDoDamage( CUnitServer *pAttacker, CUnitServer *pTarget );
	virtual void OnDoAccidentalDamage( CUnitServer *pAttacker, CUnitServer *pTarget );
	virtual void OnDoCriticalDamage( CUnitServer *pAttacker, CUnitServer *pTarget );
	virtual void OnUnitWasKilled( CUnitServer *pAttacker, CUnitServer *pTarget );
	virtual void OnNewTurnStarted( IPlayer *pPlayer );
	virtual void OnRealTimeStarted();
	virtual void OnInterrupt( CUnitServer *pWho );
	virtual void OnSkillIncreased( CUnitServer *pWho );
	virtual void OnCannotFinishHeal( CUnitServer *pHealer, CUnitServer *pTarget );
	virtual void OnHealFinished( CUnitServer *pHealer, CUnitServer *pTarget );
	virtual void SayAck( CUnitServer *pWho, int nConditionID );
	// does any listener in vAck belong to this unit? (save-migration probe -- CWorld::CreateRestored
	// re-runs AddAck for listener-less units restored from a pre-AcksHolder-fix save)
	bool HasAcksFor( CUnitServer *pUnit );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAckBase
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckBase: public IAck
{
	ZDATA
	CPtr<CUnitServer> pUnit; // field needed to remove the ack when the corresponding Unit dies
	CDBPtr<NDb::CDBAck> pDBAck; // ack parameters
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pUnit); f.Add(3,&pDBAck); return 0; }

	CAckBase() : pUnit(0), pDBAck(0) { }
	CAckBase( CUnitServer *_pUnit, NDb::CDBAck *_pDBAck );
	CUnitServer *GetUnit();
	// IAck
	virtual NDb::CDBAck *GetDBAck();
	virtual CWorld *GetWorld();
	virtual void PlayAck();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif