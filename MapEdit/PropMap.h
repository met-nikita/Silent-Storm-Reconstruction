#ifndef __PROPMAP_H__
#define __PROPMAP_H__

#include "Variant.h"
#include "PropMapTypedef.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
struct SOwner
{
	int nItemID;
	int nVariantID;

	SOwner() : nItemID( -1 ), nVariantID( -1 ) {}
	SOwner( int nItem, int nVariant ) : nItemID( nItem ), nVariantID( nVariant ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CProp : public CObjectBase
{
  SOwner owner;              // итем, которому принадлежит данное св-во
  int nPropertyID;
  string szName;
  int nType;                // тип значени€
  int nViewType;            // способ представлени€ и редактировани€ значени€
  int nRelation;            // ID таблицы, на которую указывает свойство
  int nGroupID;             // если у нескольких св-в один и тот же nRelation, то они
                            // объедин€ютс€ в группу
  string szPrefix;          // дл€ browse св-в можно указать директорию-префикс
  vector<string> szStrings; // опции в комбобоксе
	bool bReadOnly;
	string szTip;							// тултип

protected:
	// т.к. наследуетс€ от CObjectBase и нет макроса OBJECT_BASIC_METHODS,
	// то во избежание ошибок св€занных с повторным удалением объ€вле€м protected деструктор
	virtual ~CProp() {}

public:
  enum { PROPDEF_GROUP = 0 };

	CProp() {}
  CProp( const string &_szName, int _nPropertyID, int _nType, int _nViewType, bool _bReadOnly ) 
    : szName(_szName), nPropertyID( _nPropertyID ), nType( _nType ), nViewType( _nViewType ), bReadOnly( _bReadOnly )
  {
    nRelation = -1;
    nGroupID  = PROPDEF_GROUP;
  }

  virtual const CVariant& GetValue() const = 0;
	virtual const CVariant GetDefValue() const = 0;
  virtual void SetValue( const CVariant &value, bool bModified = true ) const = 0;
	virtual CProp* Clone() const = 0;
  //
  int  GetType() const { return nType; }
  int  GetViewType() const { return nViewType; }
  int  GetID() const { return nPropertyID; }
  string GetName() const { return szName; }
  SOwner GetOwnerID() const { return owner; }
  int  GetRelation() const { return nRelation; }
  int  GetGroup() const { return nGroupID; }
  string GetPrefix() const { return szPrefix; }
	string GetTip() const { return szTip; }
  const vector<string>& GetStrings() const { return szStrings; }
  //
  void SetOwner( const SOwner &ow ) { owner = ow; }
  void SetRelation( int nTableID ) { nRelation = nTableID; }
  void SetGroup( int _nGroupID ) { nGroupID = _nGroupID; }
  void SetPrefix( const string &szStr ) { szPrefix = szStr; }
	void SetTip( const string &szToolTip ) { szTip = szToolTip; }
  void AddString( const string &szStr ) { szStrings.push_back( szStr ); }
  //
  bool IsRelatedTable( int nTableID ) const { return nTableID == nRelation; }
	bool IsReadOnly() const { return bReadOnly; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SListInfo
{
	int nItemsTable;
	int nContainerItemID;
};
class CListProp: public CProp
{
public:
	CListProp() {}
  CListProp( const string &szName, int nPropertyID, int nType, int nViewType, bool bReadOnly ) 
    : CProp( szName, nPropertyID, nType, nViewType, bReadOnly )
  {
  }
	virtual const CVariant& GetValue() const { static CVariant v; return v; }
	virtual const CVariant GetDefValue() const { return CVariant(); }
	virtual void SetValue( const CVariant &value, bool bModified = true ) const = 0;
	virtual bool GetValues( vector<CVariant> *pVals ) const = 0;
	virtual bool AddValue( CVariant val ) const = 0;
	virtual bool RemoveValue( CVariant val ) const = 0;
	virtual void SetInfo( int nItemsTable, int nContainerItemID ) = 0;
	virtual SListInfo GetListInfo() const = 0;
	virtual void Copy( CListProp *pSrc ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// клонирование "sample"
inline CPropMap* CreatePropMap( const CPropMap &sample )
{
	CPropMap *pProps = new CPropMap;

	for ( CPropMap::const_iterator it = sample.begin(); it != sample.end(); ++it )
		pProps->insert( CPropMap::value_type( it->first, it->second->Clone() ) );
	return pProps;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __PROPMAP_H__