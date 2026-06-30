#ifndef __UNITUNDO_H_
#define __UNITUNDO_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DBWysiwygUndo.h"
#include "..\MapEdit\ObjectMgr.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitUndo: public CDBWysiwygUndo<NDb::CUnit>
{
	OBJECT_NOCOPY_METHODS(CUnitUndo)
private:
	CPropMap props;

	virtual bool Insert( NDb::CUnit *p );
	virtual bool SetPos( NDb::CUnit *p, int nID );
	virtual bool Delete( int nID );
	virtual void Update();

public:
	CUnitUndo() {}
	CUnitUndo( NDb::CUnit *pStart, NDb::CUnit *pEnd, CWysiwygUndo::EUndoAction eAction, int nDbID = -1 );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __UNITUNDO_H_
