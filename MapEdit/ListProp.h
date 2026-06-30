#ifndef __LISTPROP_H_
#define __LISTPROP_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "PropMap.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInstancesList: public CListProp
{
	OBJECT_BASIC_METHODS(CInstancesList);
	int nEffectID;
	SListInfo info;
	bool SetParticleEffectID( const CVariant &particle, int nEffectID ) const;
	string GetInstancesTable() const;
public:
	CInstancesList() {}
	CInstancesList( const string &szName, int nPropertyID );
	virtual void SetInfo( int nItemsTable, int nContainerItemID );
	virtual void SetValue( const CVariant &value, bool bModified = false ) const;
	virtual bool GetValues( vector<CVariant> *pVals ) const;
	virtual bool AddValue( CVariant val ) const;
	virtual bool RemoveValue( CVariant val ) const;
	virtual SListInfo GetListInfo() const { return info; }

	virtual CProp* Clone() const;

	virtual void Copy( CListProp *pSrc );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CItemListDBCmdAccessor
{
public:
  LONG  m_nID;
	LONG  m_nContainerID;

BEGIN_ACCESSOR_MAP( CItemListDBCmdAccessor, 2 )
  BEGIN_ACCESSOR( 0, true )
    COLUMN_ENTRY(1, m_nID)
  	COLUMN_ENTRY(2, m_nContainerID)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 1, false )
  	COLUMN_ENTRY(2, m_nContainerID)
  END_ACCESSOR()
END_ACCESSOR_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CItemListDBCmd : public CBaseDBCmd<CAccessor<CItemListDBCmdAccessor> >
{
public:
};
extern CItemListDBCmd dbListPropery;
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __LISTPROP_H_