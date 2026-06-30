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
	void PrepareAllPaths( NAI::IPathNetwork *pPathNetwork, NAI::CMultiMovesTable *pTable, list<NAI::SPathPlace> *pResult, CUnit *pWho,
		const NAI::SPathPlace &ptSrc, int nPriceLimit, CUnit *pIgnore, bool bCheckSuicide );

	NAI::CPath* FindPath( NAI::IPathNetwork *pPathNetwork, CUnit *pWho, const NAI::SPathPlace &ptSrc, 
		const vector<NAI::SPathPlace> &ptDst, CUnit *pIgnore, bool bCheckSuicide = false, 
		NAI::EFindPathParams eParams = NAI::PF_DEFAULT, bool bStrafe = false, 
		bool bCanFindNotExactPath = false, bool bIgnoreAllUnits = false );

	bool IsValidPath( NAI::IPathNetwork *pPathNetwork, const NAI::CPath &path, CUnit *pWho );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif