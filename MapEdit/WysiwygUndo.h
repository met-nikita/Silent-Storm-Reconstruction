#ifndef __WYSIWYGUNDO_H_
#define __WYSIWYGUNDO_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "MEUndoRedo.h"
namespace NDb
{
	class CFinalElement;
	class CUnit;
	class CRndTerrainSpot;
	class CRectangle;
}
namespace NBuilding
{
	struct SBuildFragment;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWysiwygUndo: public NMapEditor::CUndoRedo
{
public:
	enum EUndoAction
	{
		UA_CHANGE_POS,
		UA_INSERT,
		UA_DELETE,
	};
private:
	EUndoAction eUndoAction;

public:
	CWysiwygUndo() {}
	CWysiwygUndo( EUndoAction eAction ): eUndoAction(eAction) {}

	EUndoAction GetAction() const { return eUndoAction; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
void Refresh( int nID )
{
	Sleep( 30 );
	for ( int i = 0; i < 3; ++i )
	{
		NDatabase::Refresh<T>();
		CDBTable<T> *pModels = NDatabase::GetTable<T>();
		if ( !pModels )
			return;
		T *pRes = pModels->GetRecord( nID );
		if ( pRes != 0 )
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWysiwygUndo* CreateObjSelectionUndo( CWysiwygUndo::EUndoAction eAction, NDb::CFinalElement *pStart, NDb::CFinalElement *pEnd, int nDbID = -1 );
CWysiwygUndo* CreateFragmentSelUndo( CWysiwygUndo::EUndoAction eAction, int nBuildingID, const NBuilding::SBuildFragment *pStart, const NBuilding::SBuildFragment *pEnd );
CWysiwygUndo* CreateUnitUndo( CWysiwygUndo::EUndoAction eAction, NDb::CUnit *pStart, NDb::CUnit *pEnd, int nDbID = -1 );
CWysiwygUndo* CreateTerrSpotUndo( CWysiwygUndo::EUndoAction eAction, NDb::CRndTerrainSpot *pStart, NDb::CRndTerrainSpot *pEnd, int nDbID = -1 );
CWysiwygUndo* CreateSubTemplateUndo( CWysiwygUndo::EUndoAction eAction, NDb::CRectangle *pStart, NDb::CRectangle *pEnd, int nDbID = -1 );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __WYSIWYGUNDO_H_
