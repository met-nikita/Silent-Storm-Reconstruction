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
namespace NLSHead
{
class CHeadAnimator;
class CHeadInfo;
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
// release SUnitHeadAnimator (PDB 12 bytes; operator& @0x25e820 tags 2/3/4): records are keyed by the
// unit's per-unit CHeadInfo (the CObj the RPG unit serializes at tag 0x15), so a loaded retail save's
// records resolve to the same CHeadInfo/CHeadAnimator objects the render parts and idle tokens reference.
struct SUnitHeadAnimator
{
	ZDATA
	CPtr<CHeadInfo> pHead;
	CObj<CHeadAnimator> pAnimator;
	// release SUnitHeadAnimator::operator& @0x25e820 tag 4: the head's idle token (weak -- the render
	// sync destinations own it, so it dies with the last view showing this head).
	CPtr<CIdleHead> pIdler;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pHead); f.Add(3,&pAnimator); f.Add(4,&pIdler); return 0; }
};
class CHeadsController: public CObjectBase
{
	OBJECT_BASIC_METHODS(CHeadsController);
	ZDATA
	vector<SUnitHeadAnimator> animators;
	CTimeCounter timer;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&animators); f.Add(3,&timer); return 0; }
	// release GetInfo @0x25dc40: raw-pointer scan of the records by CHeadInfo.
	SUnitHeadAnimator* GetInfo( CHeadInfo *pHeadInfo );
public:
	CHeadsController() {}

	CCTime* GetTime();
	void Advance( STime currentTime );
	// release @0x25de50: fetch-or-create the head's animator record, keyed by the unit's CHeadInfo.
	CHeadAnimator* GetAnimator( CHeadInfo *pHeadInfo );
	// release @0x25df90: (head info, lipsync seq, expression seq, cycle) -- the expression arms as the
	// animator's MASK entry (the per-phrase facial emotion). The 2-arg form keeps the expression null.
	void PlaySequence( CHeadInfo *pHeadInfo, NDb::CSequence *pSeq, NDb::CSequence *pExpr, bool bCycle = false );
	void PlaySequence( CHeadInfo *pHeadInfo, NDb::CSequence *pSeq, bool bCycle = false ) { PlaySequence( pHeadInfo, pSeq, 0, bCycle ); }
	// release @0x25dda0: fetch the head's idle token, re-creating it when missing/dying (no record is
	// created here -- the render path's AddHead/GetAnimator made it first). The caller must register
	// the returned token into its render sync destination.
	CObjectBase* PlayIdle( CHeadInfo *pHeadInfo );
	// release @0x25dc60: force a dead unit's head into the frozen death-mask idle. No dev caller yet
	// (the world-side unit-death render path is a later parity target); kept for retail parity.
	void KillHead( CHeadInfo *pHeadInfo );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif