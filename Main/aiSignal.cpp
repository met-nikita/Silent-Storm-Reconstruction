#include "StdAfx.h"

#include "Time.h"

#include "..\misc\RandomGen.h"

#include "wMain.h"
#include "wUnitServer.h"
#include "wDumbUnit.h"
#include "wUnitCommands.h"

#include "aiUnit.h"
#include "aiUnitState.h"     // SAIUnitState (AI-convergence Stage 2: mark threat state modified)
#include "aiPosition.h"
#include "aiCommander.h"
#include "aiTaskCommand.h"
#include "aiControl.h"
#include "aiRouteLogic.h"    // NAI::CreateAIRouteLogic (signal reactions re-homed onto per-unit route logics)
#include "aiRouteMisc.h"     // NAI::RouteAddLookAround (the vector-based look-around builder)
#include "aiMap.h"
#include "aiNearestPosition.h"

#include "rpgUnitInfo.h"
#include "rpgGame.h"
#include "rpgCheatConstants.h"
#include "rpgUnitMission.h"
#include "rpgUnit.h"

#include "aiSignal.h"

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_AI_LOUD_SOUND = 10.f;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAISignalManager
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAISignalManager: public IAISignalManager
{
	OBJECT_BASIC_METHODS( CAISignalManager );
	ZDATA
	CPtr<NWorld::CWorld> pWorld;
	list< CObj<IAISignal> > Signals;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pWorld); f.Add(3,&Signals); return 0; }
public:
	//
	CAISignalManager( NWorld::CWorld *_pWorld = 0 ): pWorld( _pWorld ) {}
	//
	virtual void Add( IAISignal *pAISignal );
	virtual IAISignal *Get( IAIUnit *pAIUnit );
	virtual void Segment();
	virtual NWorld::CWorld *GetWorld() { return pWorld; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAISignalManager::Segment()
{
	pWorld->ProcessAISignals();
	//
	for ( list< CObj<IAISignal> >::iterator i = Signals.begin(); i != Signals.end(); ++i )
		(*i)->Segment();
	//
	for ( list< CObj<IAISignal> >::iterator i = Signals.begin(); i != Signals.end(); )
		if ( (*i)->IsFinished() )
			i = Signals.erase( i );
		else
			++i;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void CAISignalManager::Add( IAISignal *pAISignal )
{
	ASSERT( IsValid( pAISignal ) );
	//
	pAISignal->SetAISignalManager( this );
	Signals.push_back( pAISignal );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAISignal *CAISignalManager::Get( IAIUnit *pAIUnit )
{
	ASSERT( IsValid( pAIUnit ) );
	if ( !IsValid( pAIUnit ) )
		return 0;
	//
	CPtr<NWorld::CUnitServer> pUS = pAIUnit->GetUnitServer();
	if ( !IsValid( pUS ) )
		return 0;
	//
	bool bCheat = pUS->IsCheatEnabled( NRPG::CHEAT_NOAI ) || pUS->IsCheatEnabled( NRPG::CHEAT_SCRIPTSEQUENCE );
	if ( bCheat || !pAIUnit->IsUnderAIControl() || !pUS->CanFight() )
		return 0;
	int nPriority = -0xFFFF;
	CObj<IAISignal> pSignal = 0;
	for ( list< CObj<IAISignal> >::iterator i = Signals.begin(); i != Signals.end(); ++i )
		if ( !(*i)->IsFinished() && (*i)->IsActive() && (*i)->CanDetect( pAIUnit ) && (*i)->GetPriority() > nPriority )
		{
			nPriority = (*i)->GetPriority();
			pSignal = *i;
		}
	//
	return pSignal;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAISignal
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAISignal: public IAISignal
{
	ZDATA
public:
	CVec3 ptPos;
	CPtr<IAIUnit> pSetter;
	bool bActive;
	bool bFinished;
	CPtr<IAISignalManager> pAISignalManager;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&ptPos); f.Add(3,&pSetter); f.Add(4,&bActive); f.Add(5,&bFinished); f.Add(6,&pAISignalManager); return 0; }
	//
	CAISignal() {}
	CAISignal( IAIUnit *_pSetter, CVec3 _ptPos ): 
		pSetter( _pSetter ), ptPos( _ptPos ), bFinished( false ), bActive( true ) {}
	//
	virtual SPosition GetNearestPosition();
	CAICommander *GetAICommander( IAIUnit *pAIUnit );
	// IAISignal
	virtual bool IsFinished() { return bFinished; }
	virtual bool IsActive() { return bActive; }
	virtual bool CanDetect( IAIUnit *pAIUnit ) { return true; }
	virtual IAIUnit *GetSetter() { return pSetter; }
	virtual void SetSetter( IAIUnit *_pSetter ) { pSetter = _pSetter; }
	virtual CVec3 GetPosition() { return ptPos; }
	virtual void SetPosition( CVec3 _ptPos ) { ptPos = _ptPos; }
	virtual void Process( IAIUnit *pAIUnit ) {}
	virtual int GetPriority() { return 0; }
	virtual void Segment() {}
	virtual IAISignalManager *GetAISignalManager() { return pAISignalManager; }
	virtual void SetAISignalManager( IAISignalManager *_pAISignalManager ) { pAISignalManager = _pAISignalManager; }
	NWorld::CWorld *GetWorld() { return GetAISignalManager()->GetWorld(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAICommander *CAISignal::GetAICommander( IAIUnit *pAIUnit )
{
	CDynamicCast<CAICommander> pAICommander( pAIUnit->GetUnitServer()->GetPlayer()->GetCommander() );
	return pAICommander;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// AI-convergence Stage 2: CAISignal::GetAITacticalCommander REMOVED (dead -- had no callers; the tactical
// commander is gone).
////////////////////////////////////////////////////////////////////////////////////////////////////
SPosition CAISignal::GetNearestPosition()
{
	CPtr<IPathNetwork> pNetwork = GetWorld()->GetPathNetwork();
	return NAI::GetNearestPosition( GetPosition(), pNetwork );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIOneSegmentSignal
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIOneSegmentSignal: public CAISignal
{
	ZDATA
public:
	ZPARENT( CAISignal );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAISignal *)this); return 0; }
	//
	CAIOneSegmentSignal() {}
	CAIOneSegmentSignal( IAIUnit *_pSetter, CVec3 _ptPos ):
		CAISignal( _pSetter, _ptPos ) {}
	//
	virtual void Segment() { bFinished = true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIGrenadeSoundSignal
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIGrenadeSoundSignal: public CAIOneSegmentSignal
{
	OBJECT_BASIC_METHODS( CAIGrenadeSoundSignal );
	ZDATA
	ZPARENT( CAIOneSegmentSignal );
	int nCount;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIOneSegmentSignal *)this); f.Add(3,&nCount); return 0; }
public:
	//
	CAIGrenadeSoundSignal() {}
	CAIGrenadeSoundSignal( CVec3 _ptPos );
	//
	virtual void Process( IAIUnit *pAIUnit );
	virtual bool CanDetect( IAIUnit *pAIUnit );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIGrenadeSoundSignal::CAIGrenadeSoundSignal( CVec3 _ptPos ):
	CAIOneSegmentSignal( 0, _ptPos ), nCount(0)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIGrenadeSoundSignal::CanDetect( IAIUnit *pAIUnit )
{
	// 50% at 15m and 100% at 0m, but no more than 2 units
	++nCount;
	float fDistance = fabs( pAIUnit->GetPosition().GetCP() - ptPos );
	return nCount < 3 && fDistance < 15 && random.Get( 1, 100 ) <= ( 30 - fDistance ) * 3.333f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIGrenadeSoundSignal::Process( IAIUnit *pAIUnit )
{ 
	ASSERT( IsValid( pAIUnit ) );
	ASSERT( IsValid( pAIUnit->GetUnitServer() ) );
	//
	// CAITaskCommander removal: the signal reaction is re-homed onto the unit's OWN route logic (SetLogic)
	// instead of an ERASABLE CTask control in the per-player task queue. Same command list; the look-around
	// now comes from the vector-based RouteAddLookAround builder (the route-logic idiom).
	vector< CPtr<CTaskCommand> > cmds;
	cmds.push_back( new CTaskCommandGoto( GetNearestPosition() ) );
	RouteAddLookAround( true, cmds, 6 );
	if ( IAILogic *pLogic = CreateAIRouteLogic( pAIUnit, cmds, false ) )
		pAIUnit->SetLogic( pLogic );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIRevealEnemySignal
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIRevealEnemySignal: public CAIOneSegmentSignal
{
	OBJECT_BASIC_METHODS( CAIRevealEnemySignal );
	ZDATA
	ZPARENT( CAIOneSegmentSignal );
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIOneSegmentSignal *)this); return 0; }
public:
	//
	CAIRevealEnemySignal() {}
	CAIRevealEnemySignal( CVec3 _ptPos ): CAIOneSegmentSignal( 0, _ptPos ) {}
	//
	virtual void Process( IAIUnit *pAIUnit );
	virtual bool CanDetect( IAIUnit *pAIUnit );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIRevealEnemySignal::Process( IAIUnit *pAIUnit )
{
	ASSERT( IsValid( pAIUnit ) );
	ASSERT( IsValid( pAIUnit->GetUnitServer() ) );
	//
	// AI-convergence Stage 2: no tactical control anymore. Mark the unit's threat state modified so the
	// commander's per-segment reaction pump (OnAISegment -> CheckForUpdates -> updateTracker.Update) re-
	// evaluates it into combat. (This signal is currently dead -- CAIRevealEnemySignal::CanDetect returns
	// false -- so Process is never reached; the re-home keeps it building against the new model.)
	if ( IsValid( pAIUnit ) && pAIUnit->GetAIUnitState() )
		pAIUnit->GetAIUnitState()->selfModified.SetModified();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIRevealEnemySignal::CanDetect( IAIUnit *pAIUnit )
{
	return false;

	float fDistance = fabs( pAIUnit->GetPosition().GetCP() - ptPos );
	return fDistance < 10;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIHitSignal
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIHitSignal: public CAIOneSegmentSignal
{
	OBJECT_BASIC_METHODS( CAIHitSignal );
	ZDATA
	ZPARENT( CAIOneSegmentSignal );
	CPtr<NRPG::IUnitMissionInfo> pAttacker;
	CPtr<NRPG::IUnitMissionInfo> pTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIOneSegmentSignal *)this); f.Add(3,&pAttacker); f.Add(4,&pTarget); return 0; }
public:
	//
	CAIHitSignal() {}
	CAIHitSignal( NRPG::IUnitMissionInfo *_pAttacker, NRPG::IUnitMissionInfo *_pTarget );
	//
	virtual void Process( IAIUnit *pAIUnit );
	virtual bool CanDetect( IAIUnit *pAIUnit );
	virtual int GetPriority() { return 1; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIHitSignal::CAIHitSignal( NRPG::IUnitMissionInfo *_pAttacker, NRPG::IUnitMissionInfo *_pTarget ):
	CAIOneSegmentSignal( 0, CVec3( 0,0,0 ) ), pAttacker( _pAttacker ), pTarget( _pTarget )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIHitSignal::Process( IAIUnit *pAIUnit )
{
	ASSERT( IsValid( pAIUnit ) );
	ASSERT( IsValid( pAIUnit->GetUnitServer() ) );
	ASSERT( IsValid( pAttacker ) );
	ASSERT( IsValid( pTarget ) );
	//
	CPtr<NWorld::CUnitServer> pAttackerUS = GetWorld()->GetUnitServer( pAttacker );
	CPtr<NWorld::CUnitServer> pTargetUS = GetWorld()->GetUnitServer( pTarget );
	//
	EDirection Dir =
		GetWorld()->GetPathNetwork()->GetClosestDir( pTargetUS->GetPosition().pos.p, pAttackerUS->GetPosition().pos.p );
	// re-homed onto the unit's route logic (see CAIGrenadeSoundSignal::Process)
	vector< CPtr<CTaskCommand> > cmds;
	cmds.push_back( new CTaskCommandChangeDirection( Dir ) );
	if ( IAILogic *pLogic = CreateAIRouteLogic( pAIUnit, cmds, false ) )
		pAIUnit->SetLogic( pLogic );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIHitSignal::CanDetect( IAIUnit *pAIUnit )
{
	if ( !IsValid( pAttacker ) || !IsValid( pTarget ) )
		return false;
	return pAIUnit->GetUnitServer() == GetWorld()->GetUnitServer( pTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIUnitSoundSignal
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIUnitSoundSignal: public CAIOneSegmentSignal
{
	OBJECT_BASIC_METHODS( CAIUnitSoundSignal );
	ZDATA
	ZPARENT( CAIOneSegmentSignal );
	float fRadius;
	CPtr<NWorld::CUnitServer> pSource;
	CPtr<NWorld::CUnitServer> pTarget;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIOneSegmentSignal *)this); f.Add(3,&fRadius); f.Add(4,&pSource); f.Add(5,&pTarget); return 0; }
public:
	//
	CAIUnitSoundSignal() {}
	CAIUnitSoundSignal( NWorld::CUnitServer *_pSource, NWorld::CUnitServer *_pTarget, float _fRadius );
	//
	virtual void Process( IAIUnit *pAIUnit );
	virtual bool CanDetect( IAIUnit *pAIUnit );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIUnitSoundSignal::CAIUnitSoundSignal( NWorld::CUnitServer *_pSource, 
	NWorld::CUnitServer *_pTarget, float _fRadius ):
		CAIOneSegmentSignal( 0, CVec3( 0, 0, 0 ) ),
		pSource( _pSource ), pTarget( _pTarget ), fRadius( _fRadius )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIUnitSoundSignal::CanDetect( IAIUnit *pAIUnit )
{
	return pAIUnit->GetUnitServer() == pTarget && 
		( pSource->GetPlayer() != pTarget->GetPlayer() || fRadius > F_AI_LOUD_SOUND );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIUnitSoundSignal::Process( IAIUnit *pAIUnit )
{
	ASSERT( IsValid( pAIUnit ) );
	ASSERT( IsValid( pAIUnit->GetUnitServer() ) );
	//
	CPtr<NWorld::CUnitServer> pUnitServer = pAIUnit->GetUnitServer();
	vector< CPtr<CTaskCommand> > cmds;
	if ( fRadius > F_AI_LOUD_SOUND )
	{
		// go check it out
		cmds.push_back( new CTaskCommandGoto( pSource->GetPosition().pos ) );
		RouteAddLookAround( true, cmds, 6 );
	}
	else
	{
		// turn to face it
		EDirection Dir = GetWorld()->GetPathNetwork()->GetClosestDir( pTarget->GetPosition().pos.p, pSource->GetPosition().pos.p );
		cmds.push_back( new CTaskCommandChangeDirection( Dir ) );
		cmds.push_back( new CTaskCommandWait( 2 ) );
	}
	//
	// re-homed onto the unit's route logic (see CAIGrenadeSoundSignal::Process)
	if ( IAILogic *pLogic = CreateAIRouteLogic( pAIUnit, cmds, false ) )
		pAIUnit->SetLogic( pLogic );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAISoundSignal
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAISoundSignal: public CAIOneSegmentSignal
{
	OBJECT_BASIC_METHODS( CAISoundSignal );
	ZDATA
	ZPARENT( CAIOneSegmentSignal );
	float fRadius;
	CVec3 ptReason;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIOneSegmentSignal *)this); f.Add(3,&fRadius); f.Add(4,&ptReason); return 0; }
public:
	//
	CAISoundSignal() {}
	CAISoundSignal( CVec3 _ptPos, NWorld::CUnitServer *pReason, float _fRadius );
	//
	virtual void Process( IAIUnit *pAIUnit );
	virtual bool CanDetect( IAIUnit *pAIUnit );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAISoundSignal::CAISoundSignal( CVec3 _ptPos, NWorld::CUnitServer *pReason, float _fRadius ):
	CAIOneSegmentSignal( 0, _ptPos ), fRadius( _fRadius )
{
	if ( IsValid( pReason ) )
		ptPos = pReason->GetPosition().GetCP();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAISoundSignal::Process( IAIUnit *pAIUnit )
{
	ASSERT( IsValid( pAIUnit ) );
	//
	CPtr<NWorld::CUnitServer> pUnitServer = pAIUnit->GetUnitServer();
	EDirection Dir = GetWorld()->GetPathNetwork()->GetClosestDir( pUnitServer->GetPosition().pos.p, GetNearestPosition().p );
	// re-homed onto the unit's route logic (see CAIGrenadeSoundSignal::Process)
	vector< CPtr<CTaskCommand> > cmds;
	cmds.push_back( new CTaskCommandChangeDirection( Dir ) );
	cmds.push_back( new CTaskCommandWait( 2 ) );
	if ( IAILogic *pLogic = CreateAIRouteLogic( pAIUnit, cmds, false ) )
		pAIUnit->SetLogic( pLogic );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAISoundSignal::CanDetect( IAIUnit *pAIUnit )
{
	ASSERT( IsValid( pAIUnit ) );
	//
	return fabs2( pAIUnit->GetUnitServer()->GetPosition().GetCP() - ptPos ) <= fRadius * fRadius;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAICorpseSignal
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAICorpseSignal: public CAISignal
{
	OBJECT_BASIC_METHODS( CAICorpseSignal );
	ZDATA
	ZPARENT( CAISignal );
	CPtr<NWorld::CUnitServer> pCorpse;
	CVec3 ptCorpsePosition;
	NAI::SPosition CorpsePosition;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAISignal *)this); f.Add(3,&pCorpse); f.Add(4,&ptCorpsePosition); f.Add(5,&CorpsePosition); return 0; }
	//
	void CalculateCorpsePosition();
public:
	//
	CAICorpseSignal() {}
	CAICorpseSignal( NWorld::CUnitServer *_pCorpse );
	//
	virtual CVec3 GetPosition();
	virtual void Process( IAIUnit *pAIUnit );
	virtual bool CanDetect( IAIUnit *pAIUnit );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CAICorpseSignal::GetPosition() 
{
	CVec3 ptCorpse;
	pCorpse->GetWorld()->GetAIMap()->GetUnitHLPos( &ptCorpse, pCorpse->GetWorld()->GetAIMap()->GetHull(pCorpse), -1 );
	return ptCorpse;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICorpseSignal::CalculateCorpsePosition()
{
	if ( GetPosition() != ptCorpsePosition )
	{
		ptCorpsePosition = GetPosition();
		CorpsePosition = GetNearestPosition();
		CorpsePosition.p.SetPose( NAI::CM_CROUCH );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAICorpseSignal::CAICorpseSignal( NWorld::CUnitServer *_pCorpse ): 
	CAISignal( 0, VNULL3 ), pCorpse( _pCorpse ), ptCorpsePosition( VNULL3 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAICorpseSignal::Process( IAIUnit *pAIUnit )
{
	// go check it out
	CalculateCorpsePosition();
	vector< CPtr<CTaskCommand> > cmds;
	int nProb = Min( 100, ( Max( 0,
		(int)( fabs( ptCorpsePosition - pAIUnit->GetUnitServer()->GetPosition().GetCP() ) - 3 ) ) ) * 25 );
	if ( random.Get( 1, 100 ) <= nProb )
	{
		cmds.push_back( new CTaskCommandChangePose( NAI::RUN ) );
		cmds.push_back( new CTaskCommandGoto( CorpsePosition ) );
	}
	cmds.push_back( new CTaskCommandChangePose( NAI::CROUCH ) );
	RouteAddLookAround( false, cmds, 6 );
	// re-homed onto the unit's route logic (see CAIGrenadeSoundSignal::Process)
	if ( IAILogic *pLogic = CreateAIRouteLogic( pAIUnit, cmds, false ) )
		pAIUnit->SetLogic( pLogic );
	//
	bActive = false;
	bFinished = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAICorpseSignal::CanDetect( IAIUnit *pAIUnit )
{
	if ( pAIUnit->GetUnitServer()->GetPlayer() != pCorpse->GetPlayer() )
		return false;
	if ( IsValid( pCorpse->animator.GetCorpseCarrier() ) )
		return false;
	//
	CalculateCorpsePosition();
	return GetWorld()->GetGame()->CheckPositionVisibility( pAIUnit->GetUnitServer()->GetPosition(), CorpsePosition );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIShootSignal
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIShootSignal: public CAIOneSegmentSignal
{
	OBJECT_BASIC_METHODS( CAIShootSignal );
	ZDATA
	ZPARENT( CAIOneSegmentSignal );
	CPtr<NWorld::CUnitServer> pShooter;
	CRay ray;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIOneSegmentSignal *)this); f.Add(3,&pShooter); f.Add(4,&ray); return 0; }
	//
	float GetDistanceToRay( CVec3 ptPos );
public:
	//
	CAIShootSignal() {}
	CAIShootSignal( NWorld::CUnitServer *_pShooter, const CRay &_ray );
	//
	virtual void Process( IAIUnit *pAIUnit );
	virtual bool CanDetect( IAIUnit *pAIUnit );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIShootSignal::CAIShootSignal( NWorld::CUnitServer *_pShooter, const CRay &_ray ):
	CAIOneSegmentSignal( 0, VNULL3 ), pShooter( _pShooter ), ray( _ray )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CAIShootSignal::GetDistanceToRay( CVec3 ptPos )
{
	CVec3 ptRelativePos = ptPos - ray.ptOrigin;
	CVec3 ptNearestPoint = ( ptRelativePos * ray.ptDir ) * ray.ptDir;
	//DEBUG{
	float fDistance = fabs( ptNearestPoint - ptRelativePos );
	//DEBUG}
	return fabs( ptNearestPoint - ptRelativePos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIShootSignal::Process( IAIUnit *pAIUnit )
{
	ASSERT( IsValid( pAIUnit ) );
	if ( !IsValid( pAIUnit ) )
		return;
	//
	// re-homed onto the unit's route logic (see CAIGrenadeSoundSignal::Process)
	vector< CPtr<CTaskCommand> > cmds;
	cmds.push_back( new CTaskCommandChangePose( NAI::RUN ) );
	cmds.push_back( new CTaskCommandGoto( pShooter->GetPosition().pos ) );
	RouteAddLookAround( true, cmds, 6 );
	if ( IAILogic *pLogic = CreateAIRouteLogic( pAIUnit, cmds, false ) )
		pAIUnit->SetLogic( pLogic );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIShootSignal::CanDetect( IAIUnit *pAIUnit )
{
	ASSERT( IsValid( pAIUnit ) );
	if ( !IsValid( pAIUnit ) )
		return false;
	//
	return pShooter->GetPlayer() != pAIUnit->GetUnitServer()->GetPlayer() && 
		GetDistanceToRay( pAIUnit->GetUnitServer()->GetPosition().GetCenter() ) < 2.5f; 	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAISignalManager *CreateAISignalManager( NWorld::CWorld *_pWorld )
{
	return new CAISignalManager( _pWorld );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAISignal *CreateAIGrenadeSoundSignal( CVec3 _ptPos )
{
	return new CAIGrenadeSoundSignal( _ptPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAISignal *CreateAIRevealEnemySignal( CVec3 _ptPos )
{
	return new CAIRevealEnemySignal( _ptPos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAISignal *CreateAIHitSignal( NRPG::IUnitMissionInfo *pAttacker, NRPG::IUnitMissionInfo *pTarget )
{
	return new CAIHitSignal( pAttacker, pTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAISignal *CreateAIUnitSoundSignal( NWorld::CUnitServer *_pSource, 
	NWorld::CUnitServer *_pTarget, float _fRadius )
{
	return new CAIUnitSoundSignal( _pSource, _pTarget, _fRadius );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAISignal *CreateAISoundSignal( CVec3 ptPos, NWorld::CUnitServer *pReason, float fRadius )
{
	return new CAISoundSignal( ptPos, pReason, fRadius );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAISignal *CreateAICorpseSignal( NWorld::CUnitServer *_pUnitServer )
{
	return new CAICorpseSignal( _pUnitServer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAISignal *CreateAIShootSignal( NWorld::CUnitServer *pShooter, const CRay &ray )
{
	return new CAIShootSignal( pShooter, ray );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x51962130, CAIGrenadeSoundSignal );
REGISTER_SAVELOAD_CLASS( 0x51962150, CAISignalManager );
REGISTER_SAVELOAD_CLASS( 0x52062130, CAIRevealEnemySignal );
REGISTER_SAVELOAD_CLASS( 0x52662180, CAIHitSignal );
REGISTER_SAVELOAD_CLASS( 0x52662181, CAIUnitSoundSignal );
REGISTER_SAVELOAD_CLASS( 0x52662020, CAISoundSignal );
REGISTER_SAVELOAD_CLASS( 0x50372170, CAICorpseSignal );
REGISTER_SAVELOAD_CLASS( 0x50372050, CAIShootSignal );