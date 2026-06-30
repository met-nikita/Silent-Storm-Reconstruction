#ifndef __FRAGMENTSELUNDO_H_
#define __FRAGMENTSELUNDO_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "WysiwygUndo.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFragmentSelUndo: public CWysiwygUndo
{
	OBJECT_NOCOPY_METHODS(CFragmentSelUndo)
private:
	int nID;
	int nBuildingID;
	bool bWall;
	NBuilding::SBuildFragment start;
	NBuilding::SBuildFragment end;

	NBuilding::SBuildFragment* FindFragment();
	NBuilding::CBuildInfo* GetInfo();
	bool DeleteFragment();
	bool InsertFragment();
	void Update( NMapEditor::CPostProcessQueue *pQueue );

public:
	CFragmentSelUndo() {}
	CFragmentSelUndo( int nBuildingID, const NBuilding::SBuildFragment *pStart, const NBuilding::SBuildFragment *pEnd, CWysiwygUndo::EUndoAction eAction );

	virtual bool DoUndo( NMapEditor::CPostProcessQueue *pQueue );
	virtual bool DoRedo( NMapEditor::CPostProcessQueue *pQueue );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __FRAGMENTSELUNDO_H_
