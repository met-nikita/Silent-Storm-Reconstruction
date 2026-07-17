#ifndef __AISTATE_H_
#define __AISTATE_H_

namespace NWorld
{
	class CWorld;
	class CPlayer;
	class IPlayer;
	class CUnitServer;
}

namespace NAI
{
struct SPathPlace;
class IAIPlayer;
class IAICriterion;
class IAIMap;
class IAIUnit;
class CAICommander;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAIUnitGroup
{
	ZDATA
	CVec3 ptCenter;
	vector< CPtr<IAIUnit> > enemies;
	vector< CPtr<IAIUnit> > allies;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&ptCenter); f.Add(3,&enemies); f.Add(4,&allies); return 0; }
	SAIUnitGroup(): ptCenter( VNULL3 ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// SAIState -- retail's flat AI-state VALUE struct (operator& @0x38c30). AI-convergence final structural
// item: collapsed from the dev IAIState interface + CAIState impl (both dropped) into a single plain value
// struct so CAICommander can EMBED IT BY VALUE at operator& tag7 (retail CallObjectSerialize<SAIState>),
// exactly as retail. It is NOT a CObjectBase -- nothing refcounts it and nothing standalone-serializes it.
// The only holder is CAIUnit::pAIState, a raw, TRANSIENT (unserialized) weak back-pointer re-established
// every AI segment by Synchronize()'s SetAIState(this). (The dev GetAIState back-refs in since-deleted
// dev-only layers were dead and were removed.) The state's own pAICommander back-ref points at the
// REGISTERED commander, so
// there is no orphan on save/load. See the tag7 note in aiCommander.h.
//
// Special members (ctors / dtor / operator& / the smart-pointer SETTERS) are out-of-line in aistate.cpp:
// assigning a CPtr/CObj member routes through CastToObjectBase, which needs the element type complete, so
// those bodies live where aiUnit.h/aiPlayer.h are included. The pure-return getters are inline (a CPtr/CObj
// -> T* conversion just hands back the stored pointer, no completeness required).
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAIState
{
	ZDATA
	CPtr<NWorld::CWorld> pWorld;
	// retail SAIState (0x14B: pWorld@0, pPlayer@4, enemyGroups@8) keeps the owning player ON THE WIRE
	// (operator& @0x38c30 tag 3). Set once at commander construction (@0x35a10 stores the ctor's player
	// arg into state+4); retail SetPlayer @0x33e60 deliberately does NOT refresh it.
	CPtr<NWorld::IPlayer> pPlayer;
	CObj<IAIPlayer> pAlly;
	CObj<IAIPlayer> pEnemy;
	CPtr<IAIUnit> pCurrentUnit;
	CPtr<IAIUnit> pCurrentEnemy;
	int nCurrentAction;
	int nTurnStartAllyHP, nTurnStartEnemyHP; // HP at the start of the turn
	CPtr<CAICommander> pAICommander;   // back-ref to the OWNING commander (a registered object -> serializes clean)
	vector<SAIUnitGroup> enemyGroups;
	ZEND int operator&( CStructureSaver &f );
	//
	void MakeEnemyGroups();
	//
public:
	SAIState();
	SAIState( NWorld::CWorld *pWorld, CAICommander *_pAICommander );
	~SAIState();
	//
	IAIPlayer *GetAllyAIPlayer() { return pAlly; }
	IAIPlayer *GetEnemyAIPlayer() { return pEnemy; }
	NWorld::CWorld *GetWorld() { return pWorld; }
	// post-load reconnect of the runtime back-refs (retail serializes pPlayer + rebuilds these; dev's
	// CPtr<CWorld> pWorld also fails to resolve a retail save's IWorld*-identity ref -> reconnected in
	// CAICommander::ReconnectWorld from CWorld::CreateRestored, before the first Segment).
	void SetBackRefs( NWorld::CWorld *_pWorld, CAICommander *_pAICommander ) { pWorld = _pWorld; pAICommander = _pAICommander; }
	IAIUnit *GetCurrentAIUnit() { return pCurrentUnit; }
	void SetCurrentAIUnit( IAIUnit *pAIUnit );
	IAIUnit *GetCurrentAIEnemy() { return pCurrentEnemy; }
	void SetCurrentAIEnemy( IAIUnit *pAIUnit );
	bool IsPositionLocked( SPathPlace &ptPos, IAIUnit *pAIUnit );
	bool IsPerformingAction();
	bool IsSomebodyKilled();
	bool IsContain( IAIUnit *_pAIUnit );
	void RemoveUnit( IAIUnit *_pAIUnit );
	int GetEnemyHP();
	int GetAllyHP();
	int GetTurnStartEnemyHP() { return nTurnStartEnemyHP; }
	int GetTurnStartAllyHP() { return nTurnStartAllyHP; }
	int GetCurrentAction() { return nCurrentAction; }
	void SetCurrentAction( int nAction ) { nCurrentAction = nAction; }
	void OnTurnStarted();
	bool IsAITurn();
	void Synchronize();
	IAIUnit* GetDangerousAttackableEnemy( IAIUnit *pAIUnit );
	const vector<SAIUnitGroup>& GetEnemyGroups() const { return enemyGroups; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
// SAIState is a plain value struct, not a CObjectBase -- the framework's generic IsValid( T* ) would
// CastToObjectBase its argument (a bodyless primary template for a non-CObjectBase T => link error). Give
// it a plain non-null overload so the consumers' defensive IsValid( GetAIState() ) null-guards keep
// compiling and behaving (a unit with no state still returns null). Non-template exact match => preferred
// over the generic template.
inline bool IsValid( NAI::SAIState *p ) { return p != 0; }

#endif
