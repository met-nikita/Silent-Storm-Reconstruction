#ifndef __wVision_H_
#define __wVision_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// vision support
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail IVisible (Game.exe vftable @0x8c9d2c, the CDFrozenItem IVisible-base table; disasm-proven):
//   slot 0  void GetVisiblePos( vector<CVec3>* )  -- fills the LOS PROBE POINTS of the object
//           (CDFrozenItem @0x749e50: the model's mass-sphere centres world-transformed, fallback the
//            matrix translation; CDItem @0x58b790: `ret 4` == adds NO points, dynamics are never rayed);
//   slot 1  bool IsTemporaryVisible()             -- route to the per-update tempVisibleObjects list
//           instead of the PERSISTENT visibleObjects (CDFrozenItem @0x74c2d0 reads bIsTemporaryVisible
//            @+0x88; CDItem @0x488d00 returns true);
//   slot 2  CObjectBase* GetVisibilityParent()    -- the object an in-flight item flew off of
//           (CDItem @0x773810 reads pVisibilityParent @+0x2c; CDFrozenItem @0x7538d0 returns 0).
// The Jan03 single-point `CVec3 GetVisiblePos()` does not exist in retail.
class IVisible : virtual public CObjectBase
{
public:
	virtual void GetVisiblePos( vector<CVec3> *pRes ) const = 0;
	virtual bool IsTemporaryVisible() const = 0;
	virtual CObjectBase* GetVisibilityParent() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TUnit, class TPlayer>
class CTBSUnitVision
{
protected:
	ZDATA
	list<CPtr<TUnit> > visible;
	list<CPtr<CObjectBase> > visibleObjects, trappedObjects;
	list<CPtr<CObjectBase> > addToVisibleTraps;
	// retail CTBSUnitVision carries a FIFTH list (PDB member order: visible, visibleObjects,
	// trappedObjects, tempVisibleObjects @+0xc, addToVisibleTraps): the IsTemporaryVisible items this
	// unit sees THIS update. Rebuilt from scratch every CUnitServer::UpdateVisible @0x3c4450 (cleared
	// @0x7c5032), NOT serialized (retail operator& @0x3c7330 saves only the other four -- tags 2..5).
	list<CPtr<CObjectBase> > tempVisibleObjects;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&visible); f.Add(3,&visibleObjects); f.Add(4,&trappedObjects); f.Add(5,&addToVisibleTraps); return 0; }
	const list<CPtr<TUnit> >& GetTBSVisible() const { return visible; }
	const list<CPtr<CObjectBase> >& GetTBSVisibleObjects() const { return visibleObjects; }
	const list<CPtr<CObjectBase> >& GetTBSTrappedObjects() const { return trappedObjects; }
	const list<CPtr<CObjectBase> >& GetTBSTempVisibleObjects() const { return tempVisibleObjects; }
	bool CanSeePlayer( TPlayer *pThisPlayer ) const
	{
		for ( list<CPtr<TUnit> >::const_iterator k = visible.begin(); k != visible.end(); ++k )
		{
			if ( (*k)->GetTBSPlayer() != pThisPlayer ) 
				return true;
		}
		return false;
	}
	void AddToVisibleTraps( CObjectBase *p ) { addToVisibleTraps.push_back( p ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TOut, class TIn>
inline bool MergeSets( TOut *pRes, const TIn &src )
{
	bool bRes = false;
	for ( TIn::const_iterator i = src.begin(); i != src.end(); ++i )
	{
		if ( !IsInSet( *pRes, *i ) )
		{
			pRes->push_back( *i );
			bRes = true;
		}
	}
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TUnit>
class CPlayerBaseVision
{
public:
	typedef list< CPtr<TUnit> > TUnitList;
	typedef list< CPtr<CObjectBase> > TObjectList;
protected:
	ZDATA
	TUnitList addToVisible;
	TUnitList visible;
	TObjectList visibleObjects;
	TObjectList prevTrappedObjects, trappedObjects, addToVisibleTraps;
	// retail CPlayerBaseVision carries a FIFTH object list (PDB +0x10, temporaryVisibleObjects):
	// the per-update union of the watchers' tempVisibleObjects. Rebuilt every UpdateVisible
	// @0x3726b0 (clear + a 4th per-watcher MergeSets), NOT serialized (retail operator& @0x3781a0
	// saves only the six lists above -- tags 2..7).
	TObjectList temporaryVisibleObjects;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&addToVisible); f.Add(3,&visible); f.Add(4,&visibleObjects); f.Add(5,&prevTrappedObjects); f.Add(6,&trappedObjects); f.Add(7,&addToVisibleTraps); return 0; }
private:
	void AddVisibleCorpsesToAddVisible()
	{
		for ( TUnitList::iterator i = visible.begin(); i != visible.end(); ++i )
		{
			// retail @0x7713f2: a corpse on someone's shoulders is not surfaced into the
			// persistent set (CUnit vtbl+0x24 == GetCorpseCarrier).
			if ( !(*i)->CanFight() && !(*i)->GetCorpseCarrier() )
			{
				if ( !IsInSet( addToVisible, *i ) )
				{
					addToVisible.push_back( *i );
				}
			}
		}
	}
	// retail CPlayerBaseVision<CUnitServer>::EraseCarriedCorpses @0x3704e0 (ret 4; `this` in ecx is
	// never read -- the method only filters the list handed in). Callers must run EraseInvalidRefs
	// first: the CanFight() dispatch @0x7704f5 has no validity check.
	void EraseCarriedCorpses( TUnitList *pList )
	{
		for ( TUnitList::iterator i = pList->begin(); i != pList->end(); )
		{
			TUnit *p = *i;
			if ( !p->CanFight() && p->GetCorpseCarrier() )
				i = pList->erase( i );
			else
				++i;
		}
	}
	void AddVisibleTrapsToAddVisible()
	{
		for ( TObjectList::iterator i = addToVisibleTraps.begin(); i != addToVisibleTraps.end(); )
		{
			CObjectBase *p = *i;
			if ( IsValid(p) )
			{
				CDynamicCast<IMine> pMine( p );
				if ( pMine->IsMineSet() )
					++i;
				else
					i = addToVisibleTraps.erase( i );
			}
			else
				i = addToVisibleTraps.erase( i );
		}
		for ( TObjectList::iterator i = trappedObjects.begin(); i != trappedObjects.end(); ++i )
		{
			CDynamicCast<IMine> pMine( *i );
			if ( pMine->IsMineSet() && !IsInSet( addToVisibleTraps, *i ) )
				addToVisibleTraps.push_back( *i );
		}
	}
	void SeeNoticedHiddenMines()
	{
		for ( list<CPtr<CObjectBase> >::const_iterator i = trappedObjects.begin(); i != trappedObjects.end(); ++i )
		{
			CObjectBase *p = *i;
			CDynamicCast<IMine> pMine(p);
			if (pMine)
			{
				if ( pMine->IsHiddenObject() && !IsInSet( visibleObjects, p ) )
					visibleObjects.push_back( p );
			}
		}
	}
public:

	bool CanSee( TUnit *p ) { return IsInSet( visible, p ); }
	bool CanSeeObject( CObjectBase *p ) { return IsInSet( visibleObjects, p ); }
	bool CanSeeTrap( CObjectBase *p ) { return IsInSet( trappedObjects, p ); }
	const TUnitList& GetTBSVisible() const { return visible; }
	virtual void GetUnits( vector<CPtr<TUnit> > *pRes ) const = 0;

	void UpdateVisible()
	{
		prevTrappedObjects = trappedObjects;

		AddVisibleCorpsesToAddVisible();
		// retail @0x7726e6 / @0x7726ee: prune the persistent corpse memory before it seeds
		// `visible` -- dead refs first (EraseCarriedCorpses dereferences unguarded).
		EraseInvalidRefs( &addToVisible );
		EraseCarriedCorpses( &addToVisible );
		visible = addToVisible;
		
		AddVisibleTrapsToAddVisible();
		trappedObjects = addToVisibleTraps;
		// retail @0x772713 (`lea ecx,[edi+0xc]`): visibleObjects is persistent and never cleared;
		// dead refs are pruned here, exactly as the unit-level sibling does (wUnitServer.cpp:1272).
		EraseInvalidRefs( &visibleObjects );
		// retail UpdateVisible @0x3726b0: the per-update temporary set restarts empty, then a FOURTH
		// per-watcher MergeSets folds each fighting watcher's tempVisibleObjects in (decomp-confirmed
		// additions (b)+(c) over the Jan03 shape -- see s2_scratch/src/s2_playervision.h).
		temporaryVisibleObjects.clear();

		vector<CPtr<TUnit> > units;
		GetUnits( &units );
		for ( int k = 0; k < units.size(); ++k )
		{
			TUnit *pWatcher = units[k];
			if ( !IsInSet( visible, pWatcher ) )
				visible.push_back( pWatcher );
			if ( pWatcher->CanFight() )
			{
				MergeSets( &visible, pWatcher->GetTBSVisible() );
				MergeSets( &visibleObjects, pWatcher->GetTBSVisibleObjects() );
				MergeSets( &trappedObjects, pWatcher->GetTBSTrappedObjects() );
				MergeSets( &temporaryVisibleObjects, pWatcher->GetTBSTempVisibleObjects() );
			}
		}

		SeeNoticedHiddenMines();
	}
	bool MergeVisibility( CPlayerBaseVision<TUnit> *pFrom )
	{
		bool bRes = false;
		bRes |= MergeSets( &visible, pFrom->visible );
		MergeSets( &visibleObjects, pFrom->visibleObjects );
		MergeSets( &trappedObjects, pFrom->trappedObjects );
		return bRes;
	}
	bool CanSeeNewTraps()
	{
		for ( TObjectList::const_iterator i = trappedObjects.begin(); i != trappedObjects.end(); ++i )
		{
			if ( !IsInSet( prevTrappedObjects, *i ) )
				return true;
		}
		return false;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
