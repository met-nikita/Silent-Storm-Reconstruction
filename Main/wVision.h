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
class IVisible : virtual public CObjectBase
{
public:
	virtual CVec3 GetVisiblePos() const = 0;
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
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&visible); f.Add(3,&visibleObjects); f.Add(4,&trappedObjects); f.Add(5,&addToVisibleTraps); return 0; }
	const list<CPtr<TUnit> >& GetTBSVisible() const { return visible; }
	const list<CPtr<CObjectBase> >& GetTBSVisibleObjects() const { return visibleObjects; }
	const list<CPtr<CObjectBase> >& GetTBSTrappedObjects() const { return trappedObjects; }
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
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&addToVisible); f.Add(3,&visible); f.Add(4,&visibleObjects); f.Add(5,&prevTrappedObjects); f.Add(6,&trappedObjects); f.Add(7,&addToVisibleTraps); return 0; }
private:
	void AddVisibleCorpsesToAddVisible()
	{
		for ( TUnitList::iterator i = visible.begin(); i != visible.end(); ++i )
		{
			if ( !(*i)->CanFight() )
			{
				if ( !IsInSet( addToVisible, *i ) )
				{
					addToVisible.push_back( *i );
				}
			}
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
		visible = addToVisible;
		
		AddVisibleTrapsToAddVisible();
		trappedObjects = addToVisibleTraps;
		
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
