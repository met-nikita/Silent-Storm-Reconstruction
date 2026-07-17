#ifndef __AIACTIONPLACESOURCE_H_
#define __AIACTIONPLACESOURCE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release CAICombatLogic substrate - the "place source" layer (structural port, Approach A).
//
// A place source enumerates the candidate places (pose + position + arrival-AP) from which a unit may
// act this turn. CAICombatLogic owns a vector<CObj<IAIActionPlaceSource>>; CAIChoosePlaceJob binds each
// CAIAction to a place source and, in DoJob, picks the best place per action; the logic's MakeDecision
// then drives the chosen places through the CDecision<CAIAction*> engine (aiDecision.h).
//
// Reconstructed from the release Game.exe - reconstruction/exports/{placesource.c, vtable_placesource.txt}.
// vtable (verified): CAIActionPlaceSource slot4 = Prepare() [PURE, generates places], slot5 = GetPlaces()
// [base impl returns `places`]. Each concrete source overrides Prepare():
//   CAIAttackPlaceSource::Prepare   @0x0048f900   CAICurrentPlaceSource::Prepare @0x0048e980
//   CAINearEnemyPlaceSource::Prepare@0x0048ece0   CAIToPlacePlaceSource::Prepare @0x0048f0e0
//   CAIOnePlacePlaceSource::Prepare @0x004901f0
//
// NOTE: this header is the reverse-engineered CONTRACT (WIP for the substrate port). It is NOT yet in
// Main.vcxproj; the .cpp method bodies (Prepare implementations, AddAllPoses, IsPosDangerousForAllies)
// are reconstructed in the build-settle phase. See reconstruction/integration-plan.md.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "aiActionBase.h"  // SPlaceWithAP (complete), CAIAction, SActionInfo
#include "aiPosition.h"    // SPathPlace, SUnitPosition, IPathNetwork, SPosition
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld { class CUnitServer; }
namespace NAI
{
struct SAIState;
class IAIUnit;
class CAIAction;
class CAIFireArmsWeapon;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitArea - the set of grid places reachable within nAPRadius AP from a centre place. A combat logic
// bound to an area (via CAIAttackPlaceSource::pArea) only considers places inside it, so a unit "holds"
// the area instead of roaming. Release ctor @0x004747f0, Prepare @0x00474880 (a PrepareAllPaths flood),
// IsInArea @0x00473e30 (a hashed-place lookup). `places` is the prepared in-area set, keyed on the
// normalized GetHash key and SERIALIZED (release operator& @0x00474e60 tag 6), so a loaded area gates
// exactly like the saved one without a re-flood.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitArea: public CObjectBase
{
	OBJECT_BASIC_METHODS( CUnitArea );
	ZDATA
	CPtr<NWorld::CUnitServer> pUS;
	SPathPlace                place;        // centre
	int                       nAPRadius;
	int                       wishPose;     // EPose the flood assumes (stored for fidelity)
	unordered_map<unsigned long, int> places; // in-area set keyed on GetHash (release hash_map<ulong,int> @+0x1c, tag 6)
	ZEND int operator&( CStructureSaver &f );
public:
	CUnitArea(): nAPRadius( 0 ), wishPose( 0 ) {}
	CUnitArea( NWorld::CUnitServer *_pUS, const SPathPlace &_place, int _nAPRadius, int _wishPose );
	bool Prepare();                            // (re)flood the reachable set from the centre
	bool IsInArea( const SPathPlace &p ) const; // true if p is in the prepared set (empty set matches nothing, as release)
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IAIActionPlaceSource - abstract source of candidate action-places (the type held by CAICombatLogic
// and bound to actions by CAIChoosePlaceJob). Two pure virtuals beyond CObjectBase:
//   Prepare()  (slot4) - (re)compute the candidate places for this turn
//   GetPlaces()(slot5) - copy the prepared places out
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIActionPlaceSource: public CObjectBase
{
public:
	virtual void Prepare() = 0;                                  // vtbl slot4 (+0x10)
	virtual void GetPlaces( vector<SPlaceWithAP> *pRes ) = 0;    // vtbl slot5 (+0x14)
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIActionPlaceSource - base concrete source (size 0x1c). Holds the unit + the accumulated places and
// the building blocks (AddPlace / AddAllPoses) the concrete Prepare() overrides use to fill `places`.
// Prepare() stays pure here; GetPlaces() is implemented (returns `places`).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIActionPlaceSource: public IAIActionPlaceSource
{
	ZDATA
	vector<SPlaceWithAP> places;       // +0x0c  accumulated candidate places
	CPtr<IAIUnit>        pUnit;        // +0x18  the unit these places are for
public:
	ZEND int operator&( CStructureSaver &f );   // slot3; tag2 places (DoVector), tag3 pUnit
	//
protected:
	SAIState* GetAIState() const;                       // pUnit->GetAIState()   (IAIUnit vtbl 0x78)
	IAIUnit*  GetEnemy() const;                         // pUnit->GetAIUnitState()->enemy (vtbl 0x74, +0x88)
	IAIUnit*  GetUnit() const { return pUnit; }
	// True if `p` is a usable destination for pUnit on `pNet` (rejects blocked / wrong-owner tiles).
	bool      IsPassable( IPathNetwork *pNet, const SPathPlace &p );
	void      ClearPlaces();
	// Add the single place `p` (with arrival AP) if passable.
	void      AddPlace( const SPathPlace &p, int nAP );
	// Add every viable pose (stand/crouch) the unit could adopt at `p`.
	bool      AddAllPoses( const SPathPlace &p, int nStandAction, int nCrouchAction, bool bArg4 );
	// True if shooting `pWeapon` from `pos` would endanger an ally (friendly-fire guard).
	bool      IsPosDangerousForAllies( const SUnitPosition &pos, CAIFireArmsWeapon *pWeapon );
	//
public:
	CAIActionPlaceSource() {}
	CAIActionPlaceSource( IAIUnit *_pUnit ): pUnit( _pUnit ) {}
	//
	virtual void GetPlaces( vector<SPlaceWithAP> *pRes );   // slot5: *pRes = places
	// Prepare() remains pure (slot4) - concrete sources below implement it.
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIAttackPlaceSource (size 0x28) - places to shoot from within a movement budget over a unit-area.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIAttackPlaceSource: public CAIActionPlaceSource
{
	OBJECT_BASIC_METHODS( CAIAttackPlaceSource );
	ZDATA
	ZPARENT( CAIActionPlaceSource );
	int                  nMaxAP;                   // +0x1c
	bool                 bCheckDangerousForAllies; // +0x20
	CPtr<CUnitArea>      pArea;                    // +0x24
	ZEND int operator&( CStructureSaver &f );
public:
	CAIAttackPlaceSource() {}
	CAIAttackPlaceSource( IAIUnit *_pUnit, int _nMaxAP, CUnitArea *_pArea, bool _bCheck );
	virtual void Prepare();                        // @0x0048f900
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAICurrentPlaceSource (size 0x1c) - just the unit's current place.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAICurrentPlaceSource: public CAIActionPlaceSource
{
	OBJECT_BASIC_METHODS( CAICurrentPlaceSource );
	ZDATA
	ZPARENT( CAIActionPlaceSource );
	ZEND int operator&( CStructureSaver &f );
public:
	CAICurrentPlaceSource() {}
	CAICurrentPlaceSource( IAIUnit *_pUnit ): CAIActionPlaceSource( _pUnit ) {}
	virtual void Prepare();                        // @0x0048e980
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAINearEnemyPlaceSource (size 0x1c) - places adjacent to the current enemy (for melee etc.).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAINearEnemyPlaceSource: public CAIActionPlaceSource
{
	OBJECT_BASIC_METHODS( CAINearEnemyPlaceSource );
	ZDATA
	ZPARENT( CAIActionPlaceSource );
	ZEND int operator&( CStructureSaver &f );
public:
	CAINearEnemyPlaceSource() {}
	CAINearEnemyPlaceSource( IAIUnit *_pUnit ): CAIActionPlaceSource( _pUnit ) {}
	virtual void Prepare();                        // @0x0048ece0
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIToPlacePlaceSource (size 0x24) - places along the route to a target place, within nMaxAP.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIToPlacePlaceSource: public CAIActionPlaceSource
{
	OBJECT_BASIC_METHODS( CAIToPlacePlaceSource );
	ZDATA
	ZPARENT( CAIActionPlaceSource );
	SPathPlace           pos;                      // +0x1c  target place
	int                  nMaxAP;                   // +0x20
	ZEND int operator&( CStructureSaver &f );
public:
	CAIToPlacePlaceSource() {}
	CAIToPlacePlaceSource( IAIUnit *_pUnit, const SPathPlace &_pos, int _nMaxAP );
	virtual void Prepare();                        // @0x0048f0e0
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIOnePlacePlaceSource (size 0x24) - exactly the one given place (used by defence to hold a spot).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIOnePlacePlaceSource: public CAIActionPlaceSource
{
	OBJECT_BASIC_METHODS( CAIOnePlacePlaceSource );
	ZDATA
	ZPARENT( CAIActionPlaceSource );
	SPathPlace           place;                    // +0x1c  the one place to act from
	bool                 bCrouch;                  // +0x20  add only the crouch pose at that place
	ZEND int operator&( CStructureSaver &f );
public:
	CAIOnePlacePlaceSource() {}
	CAIOnePlacePlaceSource( IAIUnit *_pUnit, const SPathPlace &_place, bool _bCrouch );
	virtual void Prepare();                        // @0x004901f0
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Factories (return IAIActionPlaceSource*). Signatures verified from the decompiled factory bodies.
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIActionPlaceSource* CreateAttackPlaceSource( IAIUnit *pUnit, CUnitArea *pArea );          // @0x0048e310
IAIActionPlaceSource* CreateCurrentPlaceSource( IAIUnit *pUnit );                           // @0x0048e380
IAIActionPlaceSource* CreateNearEnemyPlaceSource( IAIUnit *pUnit );                         // @0x0048e3c0
IAIActionPlaceSource* CreateToPlacePlaceSource( IAIUnit *pUnit, const SPathPlace &pos, int nMaxAP ); // @0x0048e400
IAIActionPlaceSource* CreateOnePlacePlaceSource( IAIUnit *pUnit, const SPathPlace &pos, bool bArg );  // @0x0048e450
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __AIACTIONPLACESOURCE_H_
