#include "StdAfx.h"
#include "LSHead.h"
#include "LSController.h"
#include "wInterface.h"
#include "..\DBFormat\DataFormat.h"
#include "..\Misc\RandomGen.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NLSHead
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CIdleHead
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x25dd20: adopt the animator and, if it is idling-off, switch it to ambient idling.
CIdleHead::CIdleHead( CHeadAnimator *_pAnimator ): pAnimator( _pAnimator )
{
	if ( IsValid( pAnimator ) && pAnimator->GetIdleType() == IDLE_NONE )
		pAnimator->SetIdleType( IDLE_NORMAL );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x25dca0: the last view showing the head dropped its token -> switch ambient idling back
// off (pruning the armed idle sequences). A KillHead'ed (IDLE_DEATH) animator is left untouched.
CIdleHead::~CIdleHead()
{
	if ( IsValid( pAnimator ) && pAnimator->GetIdleType() == IDLE_NORMAL )
		pAnimator->SetIdleType( IDLE_NONE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHeadController
////////////////////////////////////////////////////////////////////////////////////////////////////
CCTime* CHeadsController::GetTime()
{
	return timer.GetTime();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadsController::Advance( STime currentTime )
{
	timer.Advance( true, currentTime );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CHeadAnimator* CHeadsController::GetAnimator( NWorld::CUnit *pUnit )
{
	if ( !pUnit->GetDBHead() || !pUnit->GetDBHead()->pHead )
		return 0;
	for ( vector<SUnitHeadAnimator>::iterator i = animators.begin(); i != animators.end(); ++i )
	{
		if ( !IsValid(i->pUnit) )
			continue;
		if ( i->pUnit == pUnit )
			return i->pAnimator;
	}
	// A committed advanced-FaceGen hero carries a baked static head (CHeadInfo::pMesh = CFaceGenMeshHolder);
	// build the animator from THAT baked morph instead of the un-morphed base mesh keyed by record id.
	NLSHead::CHeadInfo *pHI = pUnit->GetHeadInfo();
	CObj<NLSHead::CHeadAnimator> pAnimator;
	if ( IsValid( pHI ) && pHI->IsStaticHead() && pHI->GetMesh() )
		pAnimator = new NLSHead::CHeadAnimator( timer.GetTime(), pHI->GetMesh() );
	else
		pAnimator = new NLSHead::CHeadAnimator( timer.GetTime(), pUnit->GetDBHead()->pHead );
	//pAnimator->PlaySequence( NDb::GetSequence( random.Get(3) + 1 ), 0 ); // CRAP
	SUnitHeadAnimator anim;
	anim.pUnit = pUnit;
	anim.pAnimator = pAnimator;
	animators.push_back( anim );
	return pAnimator;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadsController::PlaySequence( NWorld::CUnit *pUnit, NDb::CSequence *pSeq, NDb::CSequence *pExpr, bool bCycle )
{
	// release @0x25df90: both sequences go to the animator in one call (the expression = the MASK entry)
	CHeadAnimator *pAnimator = GetAnimator( pUnit );
	if ( pAnimator )
		pAnimator->PlaySequence( pSeq, pExpr, timer.GetTime()->GetValue(), bCycle );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x25dda0: find the unit's animator record (raw-pointer compare, exactly the release scan --
// no record is created here: the render path's AddHead/GetAnimator made it before the visitor reaches
// AddHeadIdleAnimator); if its idle token is missing or dying, create a fresh one (whose ctor arms
// IDLE_NORMAL) into the weak pIdler slot, and return it for the caller to own via its sync destination.
CObjectBase* CHeadsController::PlayIdle( NWorld::CUnit *pUnit )
{
	for ( vector<SUnitHeadAnimator>::iterator i = animators.begin(); i != animators.end(); ++i )
	{
		if ( i->pUnit != pUnit )
			continue;
		if ( !IsValid( i->pIdler ) )
			i->pIdler = new CIdleHead( i->pAnimator );
		return i->pIdler;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x25dc60: force the unit's head animator into the frozen death-mask idle (skipped when it
// is already there). No-op when the unit has no animator record.
void CHeadsController::KillHead( NWorld::CUnit *pUnit )
{
	for ( vector<SUnitHeadAnimator>::iterator i = animators.begin(); i != animators.end(); ++i )
	{
		if ( i->pUnit != pUnit )
			continue;
		if ( IsValid( i->pAnimator ) && i->pAnimator->GetIdleType() != IDLE_DEATH )
			i->pAnimator->SetIdleType( IDLE_DEATH );
		return;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NLSHead;
REGISTER_SAVELOAD_CLASS( 0x11042140, CHeadsController )
REGISTER_SAVELOAD_CLASS( 0x02353120, CIdleHead )
