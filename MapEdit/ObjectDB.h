#ifndef __OBJECTDB_H__
#define __OBJECTDB_H__

#include "dbDefs.h"
#include "Variant.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectDB: public CBaseDBCmd<CDynamicAccessor >
{
	string MakeQuery( const string &szTable, const string &szColumn, int nObjectID );
	CVariant ReadValue( int type, const string &szColumn );
public:
	CVariant GetValue( /*CVariant::EVarialeType*/ int nType, const string &szTable, const string &szColumn, int nObjectID );
	struct SValue
	{
		int nType;
		const string szColumn;
		CVariant value;
		SValue( int type, const string &szClmn ): nType(type), szColumn(szClmn) {}
	};
	bool GetValueBatch( vector<SValue> *pValues, const string &szTable, int nObjectID );
	bool SetValue( /*CVariant::EVarialeType*/ int nType, const string &szTable, const string &szColumn, int nObjectID, CVariant var );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __OBJECTDB_H__