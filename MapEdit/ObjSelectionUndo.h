#ifndef __OBJSELECTIONUNDO_H_
#define __OBJSELECTIONUNDO_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DBWysiwygUndo.h"
#include "..\MapEdit\ObjectMgr.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjSelectionUndo: public CDBWysiwygUndo<NDb::CFinalElement>
{
	OBJECT_NOCOPY_METHODS(CObjSelectionUndo)
private:
	CPropMap props;

	virtual void Update();
	virtual bool Insert( NDb::CFinalElement *pFin );
	virtual bool SetPos( NDb::CFinalElement *p, int nID );
	virtual bool Delete( int nID );

public:
	CObjSelectionUndo() {}
	CObjSelectionUndo( NDb::CFinalElement *pStart, NDb::CFinalElement *pEnd, CWysiwygUndo::EUndoAction eAction, int nDbID = -1 );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __OBJSELECTIONUNDO_H_
