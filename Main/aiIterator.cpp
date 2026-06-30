#include "StdAfx.h"

#include "wMain.h"
#include "wUnitServer.h"
#include "wMainMoves.h"
#include "wMainPath.h"

#include "..\Misc\RandomGen.h"
#include "..\MiscDll\LogStream.h"

#include "aiLog.h"
#include "aiUnit.h"
#include "aiState.h"
#include "aiPlayer.h"
#include "aiWeapon.h"
#include "aiInventory.h"
#include "aiMultiMoves.h"
#include "aiTacticalCommander.h"

#include "rpgUnitMission.h"
#include "rpgUnitInfo.h"

#include "aiIterator.h"

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
int N_AI_INACTIVE_EXPEDIENCY = 10000;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIIterator
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIIterator: public IAIIterator
{
	ZDATA
public:
	int nMaxAP;
	CPtr<IAIState> pAIState;
	CObj<IAILogContainer> pAILog;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nMaxAP); f.Add(3,&pAIState); f.Add(4,&pAILog); return 0; }
	//
	CAIIterator() {}
	CAIIterator( IAIState *_pAIState ): 
		pAIState(_pAIState), pAILog( CreateAILogContainer() ) {}
	//
	virtual IAILogContainer *GetAILog() { return pAILog; }
	virtual void SetMaxAP( int _nMaxAP ) { nMaxAP = _nMaxAP; }
	virtual void Reset() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIPositionIterator
///////////////////////////////////////////////////////////////////////////////////////////////////
class CAIPositionIterator: public CAIIterator
{
	struct SPlaceWithCost
	{
		NAI::SPathPlace place;
		int nCost;
		SPlaceWithCost() {}
		SPlaceWithCost( const NAI::SPathPlace &_place, int _nCost ): place( _place ), nCost( _nCost ) {}
	};
	typedef vector<SPlaceWithCost> PlaceSet;
	//
	OBJECT_BASIC_METHODS(CAIPositionIterator);
	//
	ZDATA
	ZPARENT(CAIIterator)
	PlaceSet places;
	NAI::CMultiMovesTable MovesTable;
	int nCurrentPlace;
	bool bInactivePose;
	CPtr<IAIUnit> pPrevAIUnit;
	int nPrevMaxAP;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIIterator*)this); f.Add(3,&places); f.Add(4,&MovesTable); f.Add(5,&nCurrentPlace); f.Add(6,&bInactivePose); f.Add(7,&pPrevAIUnit); f.Add(8,&nPrevMaxAP); return 0; }
	//
	void GetPossiblePlaces();
	bool FindNextPlace();
	void SetCurrentUnitPosition();
public:
	//
	CAIPositionIterator() {}
	CAIPositionIterator( IAIState *_pAIState ): 
		CAIIterator( _pAIState ), pPrevAIUnit( 0 ), nPrevMaxAP( -1 ) {}
	// IAIIterator
	virtual void First();
	virtual void Next();
	virtual bool IsEnd() { return places.empty() || nCurrentPlace == places.size(); }
	virtual void Reset() { pPrevAIUnit = 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//const int N_MAX_MOVE_AP = 16;
//

/**/
// CRAP for test. The same class was defined at wUnitMove.cpp
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIPathAPCalcer
{
	int nRes;
	NAI::SUnitPosition currentPos;
	bool bCorpse;
	NWorld::CWorld *pWorld;
	NRPG::IUnitMission *pRPG;
public:
	CAIPathAPCalcer( NWorld::CWorld *_pWorld, NRPG::IUnitMission *_pRPG, const NAI::SUnitPosition &_p, bool _bCorpse )
		: pWorld(_pWorld), pRPG(_pRPG), currentPos(_p), bCorpse(_bCorpse), nRes(0) {}
	void AddPoint( const NAI::SUnitPosition &_pos )
	{
		NRPG::EAction action = NWorld::GetMoveActionType( pWorld->GetPathNetwork(), currentPos, _pos, bCorpse );
		nRes += pRPG->GetActionAP( currentPos.GetPose(), action );
		currentPos = _pos;
	}
	void AddPoint( const NAI::SPathPlace &_p )
	{
		NAI::SUnitPosition pos( currentPos );
		pos.pos.p = _p;;
		AddPoint( pos );
	}
	int GetResult() const { return nRes; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
/**/


void CAIPositionIterator::GetPossiblePlaces()
{
	places.clear();
	nCurrentPlace = 0;
	IAIUnit *pAIUnit = pAIState->GetCurrentAIUnit();
	if ( !IsValid( pAIUnit ) )
		return;
	//
	bInactivePose = pAIUnit->GetPosition().p.GetPose() == NAI::CM_INACTIVE;
	NWorld::CUnitServer *pUS = pAIUnit->GetUnitServer();
	CAIPathAPCalcer calcer( pAIState->GetWorld(), pUS->GetUnitRPG(), pAIUnit->GetUnitPosition(), false );
	CPtr<IAIUnit> pAIEnemy = pAIState->GetCurrentAIEnemy();
	if ( IsValid( pAIEnemy ) )
	{
		CPtr<NWorld::CUnitServer> pEnemy = pAIEnemy->GetUnitServer();
		vector<NAI::SPathPlace> dest;
		dest.push_back( pEnemy->GetPosition().pos.p );
		pUS->SetWishPose( NAI::RUN );
		CPtr<NAI::IPathNetwork> pPathNetwork = pAIState->GetWorld()->GetPathNetwork();
		CPtr<NAI::CPath> pPath = NWorld::FindPath( pAIState->GetWorld()->GetPathNetwork(), pUS, 
			pAIUnit->GetPosition().p, dest, pUS, false, NAI::PF_DEFAULT, false, true, true );
		if ( IsValid( pPath ) )
		{
			for ( vector<SPathPlace>::const_iterator i = pPath->points.begin(); i != pPath->points.end(); ++i )
			{
				NAI::SPathPlace p = *i;
				calcer.AddPoint( p );
				int nCostWalk = calcer.GetResult() + pUS->GetUnitRPG()->GetActionAP( ( NAI::EPose )p.GetPose(), NRPG::AC_POSE_WALK );
				int nCostCrouch = calcer.GetResult() + pUS->GetUnitRPG()->GetActionAP( ( NAI::EPose )p.GetPose(), NRPG::AC_POSE_CROUCH );
				//
				p.SetPose( NAI::CM_STAND );	
				if ( pPathNetwork->IsNativePassable( p ) )
					places.push_back( SPlaceWithCost( p, nCostWalk ) );
				p.SetPose( NAI::CM_CROUCH ); 
				if ( pPathNetwork->IsNativePassable( p ) )
					places.push_back( SPlaceWithCost( p, nCostCrouch ) );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIPositionIterator::FindNextPlace()
{
	IAIUnit *pAIUnit = pAIState->GetCurrentAIUnit();
	if ( !IsValid( pAIUnit ) )
		return false;
	//
	while ( !IsEnd() )
	{
		++nCurrentPlace;
		//
		if ( IsEnd() ) // CRAP
			break;
		//
		SPlaceWithCost &place = places[ nCurrentPlace ];
		NAI::SPathPlace p = place.place;
		int nCost = place.nCost;
		if ( !pAIState->IsPositionLocked( p, pAIUnit ) && p.GetPose() != NAI::CM_LAY && 
			p.GetPose() != NAI::CM_INACTIVE && nCost > nPrevMaxAP && nCost <= nMaxAP )
		{
			return true;
		}
		//
		++nCurrentPlace;
	}
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsSamePos( const SPathPlace &pos1, const SPathPlace &pos2 )
{
	return pos1.GetX() == pos2.GetX() && pos1.GetY() == pos2.GetY() &&
		pos1.GetLayer() == pos2.GetLayer() && pos1.GetDirection() == pos2.GetDirection();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIPositionIterator::SetCurrentUnitPosition()
{
	IAIUnit *pAIUnit = pAIState->GetCurrentAIUnit();
	//
	if ( FindNextPlace() )
	{
		SPosition pos;
		pos.SetNetwork( pAIUnit->GetUnitServer()->GetWorld()->GetPathNetwork() );
		pos.p = places[ nCurrentPlace ].place;
		if ( pos.p.GetPose() == NAI::CM_CROUCH ) // сидеть !
			GetAILog()->Add( new CAILogExpediency( pAIUnit, 100 ), true );
		if ( bInactivePose ) // но не заборе
			GetAILog()->Add( new CAILogExpediency( pAIUnit, N_AI_INACTIVE_EXPEDIENCY ), true );
		//
		if ( !IsSamePos( pAIUnit->GetUnitServer()->GetPosition().pos.p, pos.p ) )
		{
			GetAILog()->Add( new CAILogPosition( pAIUnit, pAIUnit->GetPosition(), pos ), true );
			GetAILog()->Add( new CAILogSpendAP( pAIUnit, places[ nCurrentPlace ].nCost ), true );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIPositionIterator::First()
{
	if ( nPrevMaxAP > nMaxAP )
		nPrevMaxAP = -1;
	//
	CPtr<IAIUnit> pAIUnit = pAIState->GetCurrentAIUnit();
	pAIUnit->SavePrevPosition();
	//
	if ( pPrevAIUnit != pAIUnit )
	{
		GetPossiblePlaces();
		pPrevAIUnit = pAIUnit;
	}
	//
	nCurrentPlace = 0;
	if ( !IsEnd() )
		SetCurrentUnitPosition();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIPositionIterator::Next()
{
	if ( !IsEnd() )
	{
		++nCurrentPlace;
		if ( !IsEnd() )
			SetCurrentUnitPosition();
		else
			nPrevMaxAP = nMaxAP;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAINumberIterator
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAINumberIterator: public CAIIterator
{
	ZDATA
	ZPARENT(CAIIterator);
public:
	int nCount;
	int nCurrent;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIIterator*)this); f.Add(3,&nCount); f.Add(4,&nCurrent); return 0; }
	//
	CAINumberIterator() {}
	CAINumberIterator( IAIState *_pAIState, int _nCount );
	//
	virtual void ModifyState() = 0;
	virtual int GetCount();
	// IAIIterator
	virtual void First();
	virtual void Next();
	virtual bool IsEnd() 
	{
		return nCurrent >= nCount; 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAINumberIterator::CAINumberIterator( IAIState *_pAIState, int _nCount ):
	CAIIterator( _pAIState ), nCount( _nCount )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAINumberIterator::GetCount()
{
	return nCount;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAINumberIterator::First()
{
	nCurrent = 0;
	nCount = GetCount();
  ModifyState();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAINumberIterator::Next()
{
	if ( !IsEnd() )
	{
		++nCurrent;
		if ( !IsEnd() )
			ModifyState();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIActionIterator
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIActionIterator: public CAINumberIterator
{
	OBJECT_BASIC_METHODS(CAIActionIterator);
	//
	ZDATA
	ZPARENT( CAINumberIterator );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAINumberIterator *)this); return 0; }
public:
	//
	CAIActionIterator() {}
	CAIActionIterator( IAIState *_pAIState, int _nCount ):
		CAINumberIterator( _pAIState, _nCount ) {}
	//
	virtual void ModifyState()
	{
		pAIState->SetCurrentAction( nCurrent );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIUnitIterator
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIUnitIterator: public CAINumberIterator
{
	OBJECT_BASIC_METHODS(CAIUnitIterator)
	//
	ZDATA
	ZPARENT( CAINumberIterator );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAINumberIterator *)this); return 0; }
public:
	CAIUnitIterator() {}
	CAIUnitIterator( IAIState *_pAIState ):
		CAINumberIterator( _pAIState, 0 ) {}
	//
	virtual void ModifyState()
	{
		IAIUnit *pAIUnit = (*pAIState->GetAllyAIPlayer()->GetUnits())[nCurrent];
		pAIState->SetCurrentAIUnit( pAIUnit );
		pAIState->SetCurrentAIEnemy( pAIState->GetAITacticalCommander()->GetDangerousAttackableEnemy( pAIUnit ) );
	}
	virtual int GetCount()
	{
		return pAIState->GetAllyAIPlayer()->GetUnits()->size();
	}
};
//////////////////////////////////////////////////////////////////////////////////////
IAIIterator *CreateAIPositionIterator( IAIState *pAIState )
{
	return new CAIPositionIterator( pAIState );
}
//////////////////////////////////////////////////////////////////////////////////////	
IAIIterator *CreateAIUnitIterator( IAIState *pAIState )
{
	return new CAIUnitIterator( pAIState );
}
//////////////////////////////////////////////////////////////////////////////////////	
IAIIterator *CreateAIActionIterator( IAIState *pAIState, int nCount )
{
	return new CAIActionIterator( pAIState, nCount );
}
//////////////////////////////////////////////////////////////////////////////////////	
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x52822131, CAIUnitIterator );
REGISTER_SAVELOAD_CLASS( 0x52822132, CAIPositionIterator );
REGISTER_SAVELOAD_CLASS( 0x50662180, CAIActionIterator );