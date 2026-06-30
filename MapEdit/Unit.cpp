#include "StdAfx.h"
#include "Unit.h"
#include "FinDBCmd.h"
#include "CtrlObjectInspector.h"
#include "MapEdit.h"
#include "ItemsMgr.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitElement::CUnitElement( CFinDBCmd *pDB, int varID, int elemID )
: CFinElement( pDB, varID, elemID )
{
  ClearPropMap();
  nModelPropID = AddProperty( CVariant::VT_INT, DT_DEC, "MonsterID" );
  AddProperty( CVariant::VT_INT, DT_DEC, "Rotation" );

  SetRelation( "MonsterID", IDC_RPG_PERS_TREE );

  szTable = UNITS_TBL;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// откапываем id модельки через таблицу монстров
int CUnitElement::GetModelID() const
{
  const SResTree *pTree = theApp.GetResTree( IDC_RPG_PERS_TREE );

  if ( !pTree || !pTree->pItemsTree)
    return -1;
	int nMonster = -1;
	{
		const CPropMap *pProps = GetPropList();
		CPropMap::const_iterator it = pProps->find( "MonsterID" );
		if ( it == pProps->end() )
			return -1;
		nMonster = it->second->GetValue();
	}
  const CPropMap *pProps = pTree->pItemsTree->GetPropList( nMonster );
  if ( !pProps )
    return -1;
  CPropMap::const_iterator i = pProps->find( "ModelID" );
  if ( i != pProps->end() )
    return i->second->GetValue();
  return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitElement::DeleteFromDB()
{
  if ( FAILED( pDB->Open( szTable, GetElementID() ) ) )
    return false;
  bool ret = pDB->DeleteElement();
  pDB->Close();
  return ret;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
