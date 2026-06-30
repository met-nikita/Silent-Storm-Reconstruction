#include "StdAfx.h"
#include "TemplMgr.h"
#include "FinalElem.h"
#include "FinDBCmd.h"
#include "CtrlObjectInspector.h"

static const char LOCAL_FILE[] = __FILE__;
//#include "BugSlayer.h"
//#include "DebugAlloc.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
//  CLASS CFinElement
////////////////////////////////////////////////////////////////////////////////////////////////////
CFinElement::CFinElement( CFinDBCmd *_pDB, int varID, int elemID )
: pDB(_pDB), prntTemplID( - 1 ), nModelPropID( - 1 ), id( varID ), internalID( elemID )
{
  ASSERT( pDB );
  nModelPropID = AddProperty( CVariant::VT_INT, DT_DEC, "ModelID" );
  AddProperty( CVariant::VT_INT, DT_DEC, "Rotation" );
  SetRelation( "ModelID", IDC_MODELS_TREE );
  szTable = FINELEMS_TBL;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CFinElement::~CFinElement()
{
  ClearPropMap();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Выставить ID темплейта, которому принадлежит элемент
void CFinElement::SetTemplate( int templID )
{
  prntTemplID = templID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Связать конечный элемент с новой моделькой
void CFinElement::SetModel( int modelID )
{
  SetValue( "ModelID", modelID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить ID связанной с элементом модельки
int CFinElement::GetModelID() const
{
	const CPropMap *pProps = GetPropList();
	CPropMap::const_iterator it = pProps->find( "ModelID" );
	if ( it == pProps->end() )
		return -1;
  return it->second->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Удаление записи об элементе из таблицы конечных элементов
bool CFinElement::DeleteFromDB()
{
  if ( FAILED( pDB->Open( szTable, GetElementID() ) ) )
    return false;
  return pDB->DeleteElement();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить список редактируемых свойств
// список используется в диалоге редактирования параметров
// конечного элемента
const CPropMap* CFinElement::GetPropList() const
{
	pDB->Open( szTable, GetElementID(), &propMap );
	pDB->ReadProps( &propMap );
  return &propMap;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CFinElement::AddProperty( int type, int viewType, const char *szPropName )
{
  // т.к имена свойств соотвествуют столбцам в базе данных
  // то все св-ва должны быть добавлены до загрузки из базы данных
  
  CFinElemProp *pProp = dbgnew CFinElemProp( szPropName, propMap.size(), type, viewType, this );
  propMap.insert( CPropMap::value_type( szPropName, pProp) );
  
  return propMap.size() - 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Связать св-во szPropName с таблицей nTableID
bool CFinElement::SetRelation( const char *szPropName, int nTableID )
{
  CPropMap::iterator it = propMap.find( szPropName );
  
  if ( it == propMap.end() )
    return false;
  // если поле не может быть индексом, то связь установить нельзя
  if ( it->second->GetType() != CVariant::VT_INT )
    return false;
	it->second->SetGroup( nTableID );
	it->second->SetRelation( nTableID );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFinElement::ClearPropMap()
{
  propMap.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Запись значения св-ва в базу данных ( value по ссылке не передавать!)
void CFinElement::SetValue( const string &szPropName, const CVariant value )
{
	const CPropMap *pProps = GetPropList();
	CPropMap::const_iterator i = pProps->find( szPropName );
	if ( i == pProps->end() )
		return;
	i->second->SetValue( value, false );
	pDB->Open( szTable, GetElementID(), pProps );
	pDB->UpdateElement( pProps );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS CFinElemProp
////////////////////////////////////////////////////////////////////////////////////////////////////
CFinElemProp::CFinElemProp( const string &szName, int nID, int nType, int nViewType, CFinElement *_pElement )
  : CProp( szName, nID, nType, nViewType, false ), pElement( _pElement )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CVariant& CFinElemProp::GetValue() const
{
  ASSERT( pElement );
  return value;
  
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFinElemProp::SetValue( const CVariant &newValue, bool bModified ) const
{
  ASSERT( pElement );
	*(const_cast<CVariant*>(&value)) = newValue;
  if ( bModified )
		pElement->SetValue( GetName(), value );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
