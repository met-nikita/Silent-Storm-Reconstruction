#ifndef __ATTRIBUTES_H_
#define __ATTRIBUTES_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "dbDefs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
class CAttrDBAccessor
{
public:
	LONG  m_attrID;
	CHAR  m_attrName[MAX_DBSTRING];
	
	BEGIN_ACCESSOR_MAP( CAttrDBAccessor, 2 )
		BEGIN_ACCESSOR( 0, true )
			COLUMN_ENTRY(1, m_attrID)
			COLUMN_ENTRY(2, m_attrName)
		END_ACCESSOR()
		BEGIN_ACCESSOR( 1, false )  // используется для записи в базу данных
			COLUMN_ENTRY(2, m_attrName)
		END_ACCESSOR()
	END_ACCESSOR_MAP()
		
	void ClearRecord() { memset(this, 0, sizeof(*this)); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAttrDB : public CBaseDBCmd<CAccessor<CAttrDBAccessor> >
{
};
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef unordered_map<string, int> CAttrMap;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAttributesList
{
	CAttrMap attrs;

public:
	bool ReadListFromDB();
	void GetList( CAttrMap *pList );

	bool AddAttribute( const string &szAttr );
	bool DelAttribute( const string &szAttr );

	int  Size() const { return attrs.size(); }
	bool operator== ( const CAttributesList &op ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __ATTRIBUTES_H_