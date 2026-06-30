#ifndef __SUBTEMPLATEUNDO_H_
#define __SUBTEMPLATEUNDO_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DBWysiwygUndo.h"
#include "..\MapEdit\ObjectMgr.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSubTemplateUndo: public CDBWysiwygUndo<NDb::CRectangle>
{
	OBJECT_NOCOPY_METHODS(CSubTemplateUndo)
private:
	CPropMap props;

	virtual void Update();
	virtual bool Insert( NDb::CRectangle *p );
	virtual bool SetPos( NDb::CRectangle *p, int nID );
	virtual bool Delete( int nID );

public:
	CSubTemplateUndo() {}
	CSubTemplateUndo( NDb::CRectangle *pStart, NDb::CRectangle *pEnd, CWysiwygUndo::EUndoAction eAction, int nDbID = -1 );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SUBTEMPLATEUNDO_H_
