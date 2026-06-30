#ifndef __WMAINMOVES_H_
#define __WMAINMOVES_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "wInterface.h"
#include "aiPosition.h"
namespace NRPG
{
	enum EAction;
}
namespace NWorld
{
	NRPG::EAction GetMoveActionType( const NAI::IPathNetwork *pNet, 
		const NAI::SUnitPosition &src, const NAI::SUnitPosition &dst, bool bCorpse );
	void SelectFindPathUnits( CUnit *pWho, list<CObjectBase*> *pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif