#ifndef __wUnitSounds_H_
#define __wUnitSounds_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "aiPosition.h"
#include "rpgCheatConstants.h"

namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTimedObject;
class CUnitServer;
template<class TUnit>
struct SAISound
{
	ZDATA
	vector<CObj<CTimedObject> > objects;
	CPtr<TUnit> pWho;
	NAI::SPathPlace place;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&objects); f.Add(3,&pWho); f.Add(4,&place); return 0; }
	SAISound() {}
	SAISound( const vector<CObj<CTimedObject> > &_objects, CUnitServer *_pUS, const NAI::SPathPlace &_place )
		: pWho(_pUS), objects(_objects), place(_place) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TUnit>
inline void FilterSounds( list<SAISound<TUnit> > *pRes, const list<CPtr<TUnit> > &visible )
{
	for ( list<SAISound<TUnit> >::iterator i = pRes->begin(); i != pRes->end(); )
	{
		CDumbUnitServer *p = i->pWho;
		if ( !IsValid(p) )
		{
			i = pRes->erase( i );
			continue;
		}
		bool bCheat = p->GetUnitRPG()->GetRPGUnit()->IsCheatEnabled( NRPG::CHEAT_SCRIPTSEQUENCE );
		if ( !bCheat && find( visible.begin(), visible.end(), p ) == visible.end() )
			++i;
		else
			i = pRes->erase( i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TUnit>
class CSoundsTracker
{
	ZDATA
	list<SAISound<TUnit> > aiSounds;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&aiSounds); return 0; }
	typedef SAISound<TUnit> TSound;
	
	void HearSound( const vector<CObj<CTimedObject> > &stuff, TUnit *pWho, const NAI::SPathPlace &_place )
	{
		for ( list<TSound>::iterator i = aiSounds.begin(); i != aiSounds.end(); ++i )
		{
			TSound &s = *i;
			if ( s.pWho == pWho )
			{
				s.objects = stuff;
				s.place = _place;
				return;
			}
		}
	//	if ( find( visible.begin(), visible.end(), pWho ) != visible.end() )
	//		return;
		aiSounds.push_back( TSound( stuff, pWho, _place ) );
	}
	void ClearSound( TUnit *pWho )
	{
		for ( list<TSound>::iterator i = aiSounds.begin(); i != aiSounds.end(); )
		{
			TSound &s = *i;
			if ( s.pWho == pWho )
				i = aiSounds.erase( i );
			else
				++i;
		}		
	}
	void AddSounds( list<TSound> *pRes )
	{
		for ( list<TSound>::iterator i = aiSounds.begin(); i != aiSounds.end(); ++i )
		{
			bool bFound = false;
			for ( list<TSound>::iterator k = pRes->begin(); k != pRes->end(); ++k )
			{
				if ( k->pWho == i->pWho )
				{
					bFound = true;
					break;
				}
			}
			if ( !bFound )
				pRes->push_back( *i );
		}
	}
/*	bool CanHearSound( const CVec3 &ptFrom, NDb::CAISound *pSound )
	{
		return true;
	}*/
	void FilterSounds( const list<CPtr<TUnit> > &visible ) { NWorld::FilterSounds( &aiSounds, visible ); }
	virtual list<SAISound<TUnit> > *GetSounds() { return &aiSounds; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// множество чуваков, все звуки которых мы слышим
template<class TUnit>
class CAudibleSet
{
	ZDATA
	list< CPtr<TUnit> > audibleUnits; // слышимые вражеские Unit-ы
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&audibleUnits); return 0; }
	bool IsAudible( const TUnit *pUnitServer ) const
	{
		return find( audibleUnits.begin(), audibleUnits.end(), pUnitServer ) != audibleUnits.end();
	}
	void SetAudible( TUnit *pUnitServer, bool bAudible )
	{
		list< CPtr<TUnit> >::iterator i = find( audibleUnits.begin(), audibleUnits.end(), pUnitServer );
		if ( bAudible )
		{
			if ( i == audibleUnits.end() )
				audibleUnits.push_back( pUnitServer );
		}
		else
		{
			if ( i != audibleUnits.end() )
				audibleUnits.erase( i );		
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
