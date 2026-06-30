#include "StdAfx.h"

#include "aiUnit.h"
#include "aiState.h"
#include "aiPosition.h"

#include "wUnitServer.h"
#include "wInterface.h"

#include "aiPlayer.h"

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIPlayer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIPlayer: public IAIPlayer
{
	OBJECT_BASIC_METHODS( CAIPlayer );
	ZDATA
	CPtr<IAIState> pAIState;
	vector< CPtr<IAIUnit> > playerUnits;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pAIState); f.Add(3,&playerUnits); return 0; }
public:
	CAIPlayer( IAIState *_pAIState = 0 ) : pAIState(_pAIState) {};
	// IAIPlayer
	virtual vector< CPtr<IAIUnit> > *GetUnits() { return &playerUnits; }
	virtual void AddUnit( IAIUnit *_pAIUnit );
	virtual IAIUnit *GetNearestUnit( IAIUnit *pUnit, float *fDistance ); 
	virtual IAIState *GetAIState() { return pAIState; }
	virtual void Synchronize();
	virtual bool IsPositionLocked( SPathPlace &ptPos, IAIUnit *pAIUnit );
	virtual bool IsPerformingAction();
	virtual bool IsSomebodyKilled();
	virtual bool IsContain( NWorld::CUnitServer *pUnit );	
	virtual bool IsContain( IAIUnit *pAIUnit );
	virtual void RemoveUnit( IAIUnit *_pAIUnit );	
	virtual void OnTurnStarted();
	virtual void DebugOutput();
	virtual void CancelActions();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIPlayer::CancelActions()
{
	for ( vector< CPtr<IAIUnit> >::iterator i = playerUnits.begin(); i != playerUnits.end(); ++i )
		(*i)->GetUnitServer()->Do( new NWorld::CCmdCancel( (*i)->GetUnitServer() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIPlayer::AddUnit( IAIUnit *_pAIUnit ) 
{ 
	ASSERT( IsValid( _pAIUnit ) );
	if ( IsValid( _pAIUnit ) )
		playerUnits.push_back( _pAIUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIPlayer::OnTurnStarted()
{
	for ( vector< CPtr<IAIUnit> >::iterator i = playerUnits.begin(); i != playerUnits.end(); ++i )
		(*i)->OnTurnStarted();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIPlayer::DebugOutput()
{
	char buf[128];
	for ( vector< CPtr<IAIUnit> >::iterator i = playerUnits.begin(); i != playerUnits.end(); ++i )
	{
		SPathPlace ptPos = (*i)->GetPosition().p;
		sprintf( buf, "(x:%d, y:%d)", ptPos.GetX(), ptPos.GetY() );
		OutputDebugString( buf );
	}
	OutputDebugString( "\n" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIUnit *CAIPlayer::GetNearestUnit( IAIUnit *pUnit, float *fDistance )
{
	IAIUnit *pRes = 0;
	float fTmpDistance, fMinDistance = 0xFFFF;
	for ( vector< CPtr<IAIUnit> >::iterator i = playerUnits.begin(); i != playerUnits.end(); ++i )
	{
		fTmpDistance = fabs2( (*i)->GetPosition().GetCP() - pUnit->GetPosition().GetCP() );
		if ( fMinDistance > fTmpDistance && ( (*i) != pUnit ) )
		{
			fMinDistance = fTmpDistance;
			pRes = *i;
		}
	}
	*fDistance = fMinDistance;
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIPlayer::IsPerformingAction()
{
	for ( vector< CPtr<IAIUnit> >::iterator i = playerUnits.begin(); i != playerUnits.end(); )
		if ( (*i)->IsPerformingAction() )
			return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIPlayer::IsContain( IAIUnit *pAIUnit )
{
	return find( playerUnits.begin(), playerUnits.end(), pAIUnit ) != playerUnits.end();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIPlayer::IsContain( NWorld::CUnitServer *pUnit )
{
	for ( vector< CPtr<IAIUnit> >::iterator i = playerUnits.begin(); i != playerUnits.end(); ++i )	
		if ( (*i)->GetUnitServer() == pUnit )
			return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIPlayer::RemoveUnit( IAIUnit *_pAIUnit )
{
	for ( vector< CPtr<IAIUnit> >::iterator i = playerUnits.begin(); i != playerUnits.end(); )
		if ( (*i) == _pAIUnit )
			i = playerUnits.erase( i );
		else
			++i;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIPlayer::Synchronize() 
{
	for ( vector< CPtr<IAIUnit> >::iterator i = playerUnits.begin(); i != playerUnits.end(); )
		if ( !(*i)->GetUnitServer()->CanFight() )
			i = playerUnits.erase( i );
		else
		{	
			(*i)->Synchronize();
			++i;
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIPlayer::IsSomebodyKilled()
{
	bool bRes = false;
	for ( vector< CPtr<IAIUnit> >::iterator i = playerUnits.begin(); i != playerUnits.end(); )
		if ( !(*i)->GetUnitServer()->CanFight() )
		{
			i = playerUnits.erase( i );
			bRes = true;
		}
			else
		++i;
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIPlayer::IsPositionLocked( SPathPlace &ptPos, IAIUnit *pAIUnit )
{
	for ( vector< CPtr<IAIUnit> >::iterator i = playerUnits.begin(); i != playerUnits.end(); ++i )
	{
		if ( (*i) != pAIUnit &&
				 (*i)->GetPosition().p.GetLayer() == ptPos.GetLayer() &&
				 (*i)->GetPosition().p.GetX() == ptPos.GetX() &&
				 (*i)->GetPosition().p.GetY() == ptPos.GetY() )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIPlayer *CreateAIPlayer( IAIState *pAIState )
{
	ASSERT( IsValid( pAIState ) );
	//
	return new CAIPlayer( pAIState );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x52822101, CAIPlayer );