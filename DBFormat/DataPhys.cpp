#include "StdAfx.h"
#include "..\Misc\StrProc.h"
#include "DataPhys.h"
//
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBPhysParams
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBPhysParams::Import()
{
	NDatabase::ImportField( "UserName", &szName );
	NDatabase::ImportField( "Friction", &fFriction );
	NDatabase::ImportField( "Resistance", &fResistance );
	NDatabase::ImportField( "DeltaT", &DELTA_T );
	DELTA_T_DWORD = (DWORD)( DELTA_T * 1000.0f + 0.5f ); // release used ROUND(); equivalent for positive DELTA_T
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Looks the record up by szName (the release scans the table and falls back to record 0).
CDBPhysParams *GetDBPhysParams( const char *pszName )
{
	CDBTable<CDBPhysParams> *pTable = NDatabase::GetTable<CDBPhysParams>();
	if ( pTable )
	{
		for ( CDBIterator<CDBPhysParams> it( *pTable ); it.MoveNext(); )
			if ( it.Get()->szName == pszName )
				return it.Get();
		return pTable->GetRecord( 0 );
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NDb;
//
REGISTER_SAVELOAD_CLASS( 0x72532130, CDBPhysParams );
