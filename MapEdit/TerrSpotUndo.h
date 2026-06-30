#ifndef __TERRSPOTUNDO_H_
#define __TERRSPOTUNDO_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DBWysiwygUndo.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTerrSpotUndo: public CDBWysiwygUndo<NDb::CRndTerrainSpot>
{
	OBJECT_NOCOPY_METHODS(CTerrSpotUndo)
private:
	virtual void Update();
	virtual bool Insert( NDb::CRndTerrainSpot *p );
	virtual bool SetPos( NDb::CRndTerrainSpot *p, int nID );
	virtual bool Delete( int nID );

public:
	CTerrSpotUndo() {}
	CTerrSpotUndo( NDb::CRndTerrainSpot *pStart, NDb::CRndTerrainSpot *pEnd, CWysiwygUndo::EUndoAction eAction, int nDbID = -1 );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __OBJSELECTIONUNDO_H_
