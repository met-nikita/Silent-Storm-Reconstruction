#ifndef __wMINE_H_
#define __wMINE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "wInterface.h"
#include "wInterfaceVisitors.h"
#include "RPGAttackMech.h"
#include "wExplosionPerks.h"   // NWorld::SPerkMineModifiers (placer's explosive-perk damage modifiers)
namespace NDb
{
	class CRPGMine;
	class CModel;
}
namespace NWorld
{
class CWorld;
class CMineTracker;
class CUnitServer;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMine : public IMine, public IVisObj, public NRPG::IAttackable
{
	OBJECT_NOCOPY_METHODS(CMine);
	ZDATA
	CPtr<CWorld> pWorld;
	CVec3 vPlace;
	CDBPtr<NDb::CRPGMine> pMine;
	CSyncSrcBind<IVisObj> bindGlobal;
	int nDC;
	CPtr<NDb::CModel> pModel;
	float fAngle;
	CPtr<CMineTracker> pMineTracker;
	int nFloor;
	// placer's explosive-perk damage modifiers (structure/AE damage + always-human-critical), Filled from the
	// placing unit at set-mine time (CExecSetMine::TimeLabelReached) and applied when the mine detonates
	// (GoBoom -> AddGrenadeExplosion). SERIALIZED at tag 11 as a raw 12-byte chunk -- retail CMine::operator&
	// @0x37f170 stores it there, so a saved+reloaded armed mine RETAINS its perk scaling. Retail SetPerkModifiers
	// @0x37e130 stores it at CMine+0x44. Retail ALSO serializes the placing unit (pMaster @CMine+0x50) at tag 12
	// (CMine::operator& @0x37f170) -- a weak CPtr credited for the blast attribution (GoBoom -> AddGrenadeExplosion).
	SPerkMineModifiers sPerkModifiers;
	CPtr<CUnitServer> pMaster;   // @+0x50: placing unit (weak ref), credited for the detonation's damage
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pWorld); f.Add(3,&vPlace); f.Add(4,&pMine); f.Add(5,&bindGlobal); f.Add(6,&nDC); f.Add(7,&pModel); f.Add(8,&fAngle); f.Add(9,&pMineTracker); f.Add(10,&nFloor); f.Add(11,&sPerkModifiers); f.Add(12,&pMaster); return 0; }
public:
	void SetPerkModifiers( const SPerkMineModifiers &m ) { sPerkModifiers = m; }
	CMine() {}
	CMine( CWorld *_pWorld, const CVec3 &_vPlace, NDb::CRPGMine *pMine, int _nDC, int _nFloor, CUnitServer *_pMaster = 0 );
	~CMine();
	// implement IVisObj
	virtual void Visit( IRenderVisitor* );
	virtual void Visit( IAIVisitor* );
	// implement IAttackable
	virtual int ProcessAttack( int nUserID, NRPG::CAttackPortion *pAttack, NDb::CRPGArmor *pArmor );
	// IMine
	virtual int GetMineDC() { return nDC; }
	virtual CVec3 GetMinePos();
	virtual bool IsMineSet() { return true; }
	virtual NDb::CRPGItem* DisarmMine();
	virtual bool IsHiddenObject() const { return true; }
	//
	const CVec3& GetPlace() const { return vPlace; }
	void GoBoom( CUnitServer *pWho = 0 );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMineTracker : public CObjectBase
{
	OBJECT_NOCOPY_METHODS(CMineTracker);
	typedef vector<CObj<CMine> > CMineSet;
	typedef unordered_map<CVec3, CMineSet, SVec3Hash> CPlaceMinesHash;
	ZDATA
	CPlaceMinesHash mines;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&mines); return 0; }
public:
	void AddMine( CMine *p, const CVec3 &_vPlace );
	void RemoveMine( CMine *p );
	bool GetMines( const vector<CVec3> &places, vector<CPtr<CMine> > *pRes ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IMinesWorld / CMinesWorld  --  the world's mine registry, split out of CWorld in
// the retail build (module wMainMine.obj). In retail CWorld multiply-inherits
// CMinesWorld @+128 and serializes trappedObjects/pMineTracker through it; this
// standalone landing reproduces the same bodies + layout (vtbl@0,
// trappedObjects@+4, pMineTracker@+8, sizeof 12) WITHOUT re-basing CWorld yet
// (that is a save-format change, deferred). Nothing constructs CMinesWorld yet,
// so it stays abstract (GetWorld is the seam the eventual CWorld base provides).
//   NWorld::CMinesWorld::CMinesWorld  @0x37b9b0  ctor: empty list + new CMineTracker
//   NWorld::CMinesWorld::AddMine      @0x37b780  dedup linear scan + tail append
//   NWorld::CMinesWorld::RemoveMine   @0x37b6f0  list::remove + GlobalSituationHasChanged
//   NWorld::CMinesWorld::GetMinesNear @0x37b840  in-radius live-mine query (prunes dead/null)
class IMinesWorld
{
public:
	virtual ~IMinesWorld() {}
	virtual void AddMine( IMine *pMine ) = 0;
	virtual void RemoveMine( IMine *pMine ) = 0;
	virtual void GetMinesNear( const CVec3 &pos, list<CPtr<IMine> > *pRes, float fRadius ) = 0;
	// retail vtbl+0x10: RemoveMine's post-removal notify reaches the TBS world via
	// GetWorld() then calls CTBSWorld::GlobalSituationHasChanged() (world vtbl+0x34).
	virtual CWorld* GetWorld() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMinesWorld : public IMinesWorld
{
	list<CPtr<IMine> > trappedObjects;	// @+4  the mine registry (weak handles)
	CObj<CMineTracker> pMineTracker;	// @+8  owned per-tile spatial index
public:
	CMinesWorld( bool bRegister );
	virtual void AddMine( IMine *pMine );
	virtual void RemoveMine( IMine *pMine );
	virtual void GetMinesNear( const CVec3 &pos, list<CPtr<IMine> > *pRes, float fRadius );
	CMineTracker* GetMineTracker() const { return pMineTracker; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif