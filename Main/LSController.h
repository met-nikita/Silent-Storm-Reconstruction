#ifndef __LSController_H_
#define __LSController_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Time.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CSequence;
}
namespace NWorld
{
	class CUnit;
}
namespace NLSHead
{
class CHeadAnimator;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CIdleHead -- the refcount-scoped ambient-idle token (release ctor @0x25dd20 / dtor @0x25dca0 /
// operator& @0x25e1c0, reg 0x02353120). CHeadsController::PlayIdle lazily creates one per shown head
// and the render sync destination registers it, so facial idling is armed exactly while some view
// renders the head: the ctor flips the animator IDLE_NONE -> IDLE_NORMAL, the dtor flips IDLE_NORMAL
// -> IDLE_NONE (pruning the armed idle sequences). IDLE_DEATH is left alone by both (KillHead owns it).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CIdleHead: public CObjectBase
{
	OBJECT_NOCOPY_METHODS(CIdleHead);
	ZDATA
	CObj<CHeadAnimator> pAnimator;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pAnimator); return 0; }
public:
	CIdleHead() {}
	CIdleHead( CHeadAnimator *_pAnimator );
	virtual ~CIdleHead();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SUnitHeadAnimator
{
	ZDATA
	CPtr<NWorld::CUnit> pUnit;
	CObj<CHeadAnimator> pAnimator;
	// release SUnitHeadAnimator::operator& @0x25e820 tag 4: the head's idle token (weak -- the render
	// sync destinations own it, so it dies with the last view showing this head).
	CPtr<CIdleHead> pIdler;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pUnit); f.Add(3,&pAnimator); f.Add(4,&pIdler); return 0; }
};
class CHeadsController: public CObjectBase
{
	OBJECT_BASIC_METHODS(CHeadsController);
	ZDATA
	vector<SUnitHeadAnimator> animators;
	CTimeCounter timer;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&animators); f.Add(3,&timer); return 0; }
public:
	CHeadsController() {}

	CCTime* GetTime();
	void Advance( STime currentTime );
	CHeadAnimator* GetAnimator( NWorld::CUnit *pUnit );
	// release @0x25df90: (unit, lipsync seq, expression seq, cycle) -- the expression arms as the
	// animator's MASK entry (the per-phrase facial emotion). The 2-arg form keeps the expression null.
	void PlaySequence( NWorld::CUnit *pUnit, NDb::CSequence *pSeq, NDb::CSequence *pExpr, bool bCycle = false );
	void PlaySequence( NWorld::CUnit *pUnit, NDb::CSequence *pSeq, bool bCycle = false ) { PlaySequence( pUnit, pSeq, 0, bCycle ); }
	// release @0x25dda0: fetch-or-create the head's idle token (null when the head has no animator
	// record yet). The caller must register the returned token into its render sync destination.
	// KEYING DEVIATION: retail keys animator records by CHeadInfo*; this tree keys by NWorld::CUnit*
	// (one head per unit -- behaviourally identical for every dev caller), so unit-keyed throughout.
	CObjectBase* PlayIdle( NWorld::CUnit *pUnit );
	// release @0x25dc60: force a dead unit's head into the frozen death-mask idle. No dev caller yet
	// (the world-side unit-death render path is a later parity target); kept for retail parity.
	void KillHead( NWorld::CUnit *pUnit );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif