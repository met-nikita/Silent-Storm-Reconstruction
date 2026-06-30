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
void CHeadsController::PlaySequence( NWorld::CUnit *pUnit, NDb::CSequence *pSeq, bool bCycle )
{
	CHeadAnimator *pAnimator = GetAnimator( pUnit );
	if ( pAnimator )
		pAnimator->PlaySequence( pSeq, timer.GetTime()->GetValue(), bCycle );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NLSHead;
REGISTER_SAVELOAD_CLASS( 0x11042140, CHeadsController )
