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
struct SUnitHeadAnimator
{
	ZDATA
	CPtr<NWorld::CUnit> pUnit;
	CObj<CHeadAnimator> pAnimator;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pUnit); f.Add(3,&pAnimator); return 0; }
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
	void PlaySequence( NWorld::CUnit *pUnit, NDb::CSequence *pSeq, bool bCycle = false );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif