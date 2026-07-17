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
// release GetInfo @0x25dc40: raw-pointer scan of the records by CHeadInfo.
SUnitHeadAnimator* CHeadsController::GetInfo( CHeadInfo *pHeadInfo )
{
	for ( vector<SUnitHeadAnimator>::iterator i = animators.begin(); i != animators.end(); ++i )
	{
		if ( i->pHead == pHeadInfo )
			return &*i;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x25de50: guard (valid CHeadInfo whose CComplexHead carries a CHead record), find the
// existing record via GetInfo, else create the animator from the info's mesh node -- CHeadInfo::pMesh
// is the shared base-pack CHeadMeshLoader for a normal head or the baked CFaceGenMeshHolder for a
// committed AdvFaceGen hero, so the one retail creation covers both.
CHeadAnimator* CHeadsController::GetAnimator( CHeadInfo *pHeadInfo )
{
	if ( !IsValid( pHeadInfo ) || !pHeadInfo->GetHead() || !pHeadInfo->GetHead()->pHead )
		return 0;
	SUnitHeadAnimator *pInfo = GetInfo( pHeadInfo );
	if ( pInfo )
		return pInfo->pAnimator;
	CObj<NLSHead::CHeadAnimator> pAnimator = new NLSHead::CHeadAnimator( timer.GetTime(), pHeadInfo->GetMesh() );
	SUnitHeadAnimator anim;
	anim.pHead = pHeadInfo;
	anim.pAnimator = pAnimator;
	animators.push_back( anim );
	return pAnimator;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHeadsController::PlaySequence( CHeadInfo *pHeadInfo, NDb::CSequence *pSeq, NDb::CSequence *pExpr, bool bCycle )
{
	// release @0x25df90: both sequences go to the animator in one call (the expression = the MASK entry)
	CHeadAnimator *pAnimator = GetAnimator( pHeadInfo );
	if ( pAnimator )
		pAnimator->PlaySequence( pSeq, pExpr, timer.GetTime()->GetValue(), bCycle );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x25dda0: find the head's animator record (raw-pointer compare, exactly the release scan --
// no record is created here: the render path's AddHead/GetAnimator made it before the visitor reaches
// AddHeadIdleAnimator); if its idle token is missing or dying, create a fresh one (whose ctor arms
// IDLE_NORMAL) into the weak pIdler slot, and return it for the caller to own via its sync destination.
CObjectBase* CHeadsController::PlayIdle( CHeadInfo *pHeadInfo )
{
	for ( vector<SUnitHeadAnimator>::iterator i = animators.begin(); i != animators.end(); ++i )
	{
		if ( i->pHead != pHeadInfo )
			continue;
		if ( !IsValid( i->pIdler ) )
			i->pIdler = new CIdleHead( i->pAnimator );
		return i->pIdler;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x25dc60: force the head's animator into the frozen death-mask idle (skipped when it
// is already there). No-op when the head has no animator record.
void CHeadsController::KillHead( CHeadInfo *pHeadInfo )
{
	for ( vector<SUnitHeadAnimator>::iterator i = animators.begin(); i != animators.end(); ++i )
	{
		if ( i->pHead != pHeadInfo )
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
