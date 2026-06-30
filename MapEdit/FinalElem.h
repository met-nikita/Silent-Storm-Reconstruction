#ifndef __FINALELEM_H__
#define __FINALELEM_H__

#include <string>
#include <map>
#include "PropMap.h"

class CFinDBCmd;

class CFinElement;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFinElemProp : public CProp
{
	OBJECT_BASIC_METHODS(CFinElemProp);
private:
  CFinElement *pElement;
	CVariant     value;
  
public:
	CFinElemProp() {}
  CFinElemProp( const string &szName, int nID, int nType, int nViewType, CFinElement *pElement );
  
  const CVariant& GetValue() const;
	const CVariant GetDefValue() const { return CVariant(); }
  void SetValue( const CVariant &value, bool bModified = true ) const;
	CProp* Clone() const { return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFinDBCmd;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFinElement
{
  int       id;                       // ID варианта - индекс в таблице вариантов расстановок.
                                      // Множество всех ID FinElement'ов не должно пересекаться
                                      // со множество ID'шников расстановок CPlacement

  int       internalID;               // id элемента в таблице элементов в базе данных
  int       prntTemplID;              // ID темплейта, которому принадлежит элемент

  CPropMap  propMap;								  // редактирумые св-ва объекта

  friend class CFinElemProp;
  void  SetValue( const string &szPropName, const CVariant value );
  void  MakePropMap();
  
protected:
  CFinDBCmd *pDB;
  string    szTable;
  int       nModelPropID;             // id свойства, в котором указана моделька для элемента

  void ClearPropMap();
  int  AddProperty( int type, int viewType, const char *szPropName );
  bool SetRelation( const char *szPropName, int nTableID );

public:
  CFinElement( CFinDBCmd *pDB, int varID, int elemID );
  virtual ~CFinElement();
  
  int  GetID() const;
  int  GetElementID() const;
  virtual int GetModelID() const;
  int  GetTemplateID() const;
  
  void SetTemplate( int templID );
  void SetModel( int modelID );
  
  const CPropMap* GetPropList() const;
  virtual bool DeleteFromDB();
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  Inline Functions
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить ID расстановки для элемента
inline int CFinElement::GetID() const
{
  return id;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Пулучить ID элемента в таблице конечных элементов
inline int CFinElement::GetElementID() const
{
  return internalID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает ID темплейта, которому принадлежит данный конечный элемент
inline int CFinElement::GetTemplateID() const
{
  return prntTemplID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __FINALELEM_H__