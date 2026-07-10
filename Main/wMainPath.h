#ifndef __WMAINPATH_H_
#define __WMAINPATH_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "aiPosition.h"
#include "wInterface.h"
namespace NAI
{
	class CMultiMovesTable;
	enum EFindPathParams;
}
namespace NWorld
{
	// retail @0x37d1b0: bNoDynamicLocks SKIPS the SelectFindPathUnits dynamic-obstacle gather entirely
	// (the wave then ignores every transient unit lock). NAI::GetNearestPlaces @0xa05d0 passes true
	// (disasm-verified, oracle s2_routemisc.h/s2_mainpath.h); all other callers keep the default false.
	void PrepareAllPaths( NAI::IPathNetwork *pPathNetwork, NAI::CMultiMovesTable *pTable, list<NAI::SPathPlace> *pResult, CUnit *pWho,
		const NAI::SPathPlace &ptSrc, int nPriceLimit, CUnit *pIgnore, bool bCheckSuicide, bool bNoDynamicLocks = false );

	NAI::CPath* FindPath( NAI::IPathNetwork *pPathNetwork, CUnit *pWho, const NAI::SPathPlace &ptSrc, 
		const vector<NAI::SPathPlace> &ptDst, CUnit *pIgnore, bool bCheckSuicide = false, 
		NAI::EFindPathParams eParams = NAI::PF_DEFAULT, bool bStrafe = false, 
		bool bCanFindNotExactPath = false, bool bIgnoreAllUnits = false );

	bool IsValidPath( NAI::IPathNetwork *pPathNetwork, const NAI::CPath &path, CUnit *pWho );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif