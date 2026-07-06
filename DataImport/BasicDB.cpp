#include "StdAfx.h"
#include "..\ADOImport\BasicDB.h"
#include "..\Misc\BasicFactory.h"
#include "..\FileIO\BasicChunk1.h"
#include <strstream>

#import "C:\Program Files (x86)\Common Files\System\ADO\msado15.dll" no_namespace rename("EOF", "EndOfFile")
#include <ole2.h>
#include <conio.h>

struct SInitBasicDB
{
	SInitBasicDB() { CoInitialize(NULL); }
	~SInitBasicDB() { CoUninitialize(); }
};
static SInitBasicDB init;
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void TESTHR(HRESULT x) {if FAILED(x) _com_issue_error(x);};
////////////////////////////////////////////////////////////////////////////////////////////////////
static _ConnectionPtr pConnection;
static void EstablishConnection( const char *pszSource )
{
	_bstr_t connect(pszSource);
	TESTHR(pConnection.CreateInstance(__uuidof(Connection)));
	//pConnection->Open( connect, "Admin", "", adConnectUnspecified );
	pConnection->CursorLocation = adUseClient;
	pConnection->Open( connect, "sa", "simple", adConnectUnspecified );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CloseConnection()
{
	pConnection->Close();
	pConnection = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class COLETable
{
	long nIndex, nRecords;
	_variant_t avarRecords, fieldData;
	vector<string> fields;
	vector<int> fieldIndices;
	int nCurrentField;
#ifdef _DEBUG
	vector<string> fieldNames;
#endif
	//
	void ReadField( int nField, VARTYPE fieldType );
	int GetFieldIndex( const char *pszField );
public:
	bool Open( const char *pszTable, int nRecordID = -1 );
	void Close();
	void MoveFirst();
	void MoveNext() { ++nIndex; nCurrentField = 0; }
	bool IsEof() { return nIndex >= nRecords; }
	//
	int GetInt( int nField );
	bool GetBool( int nField );
	float GetFloat( int nField );
	_bstr_t GetString( int nField );
	int GetInt( const char *pszField ) { return GetInt( GetFieldIndex( pszField) ); }
	bool GetBool( const char *pszField ) { return GetBool( GetFieldIndex( pszField) ); }
	float GetFloat( const char *pszField ) { return GetFloat( GetFieldIndex( pszField ) ); }
	_bstr_t GetString( const char *pszField ) { return GetString( GetFieldIndex( pszField ) ); }
	const string& GetFieldName( int n ) { return fields[n]; }
	// v1.2 @0x401900..0x402740: column-presence probe backing ImportField's success flag
	// (does not touch the GetFieldIndex replay cache; presence is constant per table)
	bool HasField( const char *pszField ) const { return find( fields.begin(), fields.end(), pszField ) != fields.end(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static void PrintProviderError(_ConnectionPtr pConnection)
{
	ErrorPtr  pErr  = NULL;

	if( (pConnection->Errors->Count) > 0)
	{
		long nCount = pConnection->Errors->Count;
		// Collection ranges from 0 to nCount -1.
		for(long i = 0; i < nCount; i++)
		{
			char szBuf[1024];
			pErr = pConnection->Errors->GetItem(i);
			sprintf( szBuf, "\t Error number: %x\t%s", pErr->Number, (LPCSTR) pErr->Description );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void PrintComError(_com_error &e)
{
	_bstr_t bstrSource(e.Source());
	_bstr_t bstrDescription(e.Description());
	const char *pszSource = (LPCSTR) bstrSource;
	const char *pszDescr = (LPCSTR) bstrDescription;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COLETable::MoveFirst()
{ 
	nIndex = 0; 
	nCurrentField = 0; 
	fieldIndices.clear(); 
#ifdef _DEBUG
	fieldNames.clear();
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int COLETable::GetFieldIndex( const char *pszField )
{
	if ( nIndex > 0 )
	{
#ifdef _DEBUG
		if ( nCurrentField >= fieldIndices.size() || fieldNames[nCurrentField] != pszField )
		{
			ASSERT(0);
			return 0;
		}
#endif
		return fieldIndices[nCurrentField++];
	}
	vector<string>::iterator r = find( fields.begin(), fields.end(), pszField );
	int nPos = 0;
	if ( r == fields.end() )
	{
		ASSERT( 0 );
	}
	else
	{
		nPos = r - fields.begin();
	}
	fieldIndices.push_back( nPos );
#ifdef _DEBUG
	fieldNames.push_back( pszField );
#endif
	return nPos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COLETable::ReadField( int nField, VARTYPE fieldType )
{
	long rgIndices[2];
	rgIndices[0] = nField;
	rgIndices[1] = nIndex;
	//fieldData.vt = fieldType;
	HRESULT hr= SafeArrayGetElement(avarRecords.parray, rgIndices, &fieldData );
	ASSERT( SUCCEEDED(hr) );
	if ( fieldData.vt == VT_NULL )
	{
		switch ( fieldType )
		{
			case VT_I4: fieldData = (long)0; break;
			case VT_R4: fieldData = (float)0; break;
			case VT_BOOL: fieldData = (bool)false; break;
			case VT_BSTR: fieldData = ""; break;
		}
	}
	else
		ASSERT( fieldType == fieldData.vt );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool COLETable::Open( const char *pszTable, int nRecordID )
{
	HRESULT hr = S_OK;
	_RecordsetPtr pTable = 0;
	FieldsPtr pFields = 0;
	FieldPtr pField;
	bool bRet = false;

	try 
	{
		// Open recordset with names and hire dates from Employees table.
		TESTHR(pTable.CreateInstance(__uuidof(Recordset)));

		// Use client cursor to improve performance
		pTable->CursorType = adOpenForwardOnly;//adOpenStatic;
		pTable->CursorLocation = adUseClient;
		pTable->CacheSize = 100;
		_bstr_t source("SELECT * FROM ");
		source += pszTable;
		if ( nRecordID != -1 )
		{
			source += " WHERE ID=";
			char buf[32];
			source += itoa( nRecordID, buf, 10 );
		}

		pTable->Open( source, _variant_t((IDispatch*)pConnection), adOpenForwardOnly, adLockReadOnly, adCmdText);

		if ( pTable->GetRecordCount() > 0 )
		{
			bRet = true;
			avarRecords = pTable->GetRows(-1);
			HRESULT hr = SafeArrayGetUBound(avarRecords.parray, 2, &nRecords );
			nRecords++;
			pFields = pTable->GetFields();
			int nFields = pFields->GetCount();
			fields.resize( nFields );
			for ( int i = 0; i < nFields; ++i )
			{
				_variant_t n( (long) i );
				FieldPtr pField = pFields->GetItem( n );
				fields[i] = (const char*) pField->GetName();
			}
		}
		else
		{
			SAFEARRAYBOUND b[2];
			Zero( b );
			avarRecords = SafeArrayCreate( VT_VARIANT, 2, b );
			nRecords = 0;
			fields.resize(0);
		}
		pTable->Close();
		MoveFirst();
	}
	catch(_com_error &e)
	{
		// Notify the user of errors if any.
		// Pass a connection pointer accessed from the Recordset.
		_variant_t vtConnect = pTable->GetActiveConnection();

		// GetActiveConnection returns connect string if connection
		// is not open, else returns Connection object.
		switch(vtConnect.vt)
		{
			case VT_BSTR:
				PrintComError(e);
				break;
			case VT_DISPATCH:
				PrintProviderError(vtConnect);
				break;
			default:
				printf("Errors occured.");
				break;
		}
	}
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COLETable::Close()
{
	avarRecords.Clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int COLETable::GetInt( int nField )
{
	ReadField( nField, VT_I4 );
	return (long)fieldData;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool COLETable::GetBool( int nField )
{
	ReadField( nField, VT_BOOL );
	return (bool)fieldData;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float COLETable::GetFloat( int nField )
{
	ReadField( nField, VT_R4 );
	return (float)fieldData;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
_bstr_t COLETable::GetString( int nField )
{
	ReadField( nField, VT_BSTR );
	if ( fieldData.vt != VT_BSTR || !fieldData.bstrVal )
		return "";
	return fieldData;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDatabase
{
	CClassFactory<CDBRecord>& GetRecordTypes()
	{
		static CClassFactory<CDBRecord> recordTypes;
		return recordTypes;
	}
	typedef unordered_map< int, CDBTableBase > CTablesHash;
	CTablesHash& GetTables() 
	{
		static CTablesHash tables; // maps record type to table
		return tables;
	}
	//////////////////////////////////////////////////////////////////////////////////////
	struct STableDescr
	{
		int nTableID;
		string szTable;
	};
	//////////////////////////////////////////////////////////////////////////////////////
	struct SRelation
	{
		string szTable;               // name of ADO table
		CDBTableBase *pLeft, *pRight;  // appropriate tables
		struct SElement
		{
			int nLeft, nRight;
		};
		vector< SElement > data;
	};
	list<STableDescr>& GetTableDescrs()
	{
		static list<STableDescr> tableDescrs;
		return tableDescrs;
	}
	list<SRelation>& GetRelations()
	{
		static list< SRelation > relations;
		return relations;
	}
	static string szDataSource;
	bool bIsDatabaseLoading = false;
	COLETable table;

	static CDBTableBase* GetTableByName( const char *pszTable );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NDatabase::SetSource( const char *pszSource )
{
	szDataSource = pszSource;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NDatabase::AddTable( int nTableID, const char *pszTableName, 
												RecordCreateFunc newf )
{
	CTablesHash &tables = GetTables();
	list<STableDescr> &tableDescrs = GetTableDescrs();
	CTablesHash::iterator i = tables.find( nTableID );
	if ( i != tables.end() )
	{
		ASSERT(0); // already registered
		return;
	}
	ASSERT( pszTableName[ strlen( pszTableName ) - 1 ] == 's' );
	GetRecordTypes().RegisterTypeSafe( nTableID, newf );
	STableDescr &t = *tableDescrs.insert( tableDescrs.end(), STableDescr());
	t.nTableID = nTableID;
	t.szTable = pszTableName;
	tables[nTableID];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CDBTableBase* NDatabase::GetTable( int nTableID )
{
	CTablesHash &tables = GetTables();
	CTablesHash::iterator i = tables.find( nTableID );
	if ( i != tables.end() )
		return &i->second;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static CDBTableBase* NDatabase::GetTableByName( const char *pszTable )
{
	list<STableDescr> &tableDescrs = GetTableDescrs();
	list< STableDescr >::iterator i;
	for ( i = tableDescrs.begin(); i != tableDescrs.end(); ++i )
	{
		if ( i->szTable == pszTable )
			return GetTable( i->nTableID );
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NDatabase::AddRelation( const char *pszTableName )
{
	list< SRelation > &relations = GetRelations();
	SRelation &rel = *relations.insert( relations.end(), SRelation());
	rel.szTable = pszTableName;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static string RelationField2TableName( const string &src )
{
	ASSERT( src.substr( src.length() - 2, 2 ) == "ID" );
	return src.substr( 0, src.length() - 2 ) + "s";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NDatabase::Import()
{
	bIsDatabaseLoading = true;
	list<STableDescr> &tableDescrs = GetTableDescrs();
	list< SRelation > &relations = GetRelations();
	EstablishConnection( szDataSource.c_str() );
	list<STableDescr>::iterator i;
	list<SRelation>::iterator k;
	//
	// 1st phase - create all records & load relations
	for ( i = tableDescrs.begin(); i != tableDescrs.end(); ++i )
	{
		STableDescr &t = *i;
		CDBTableBase *pTable = GetTable( t.nTableID );
		ASSERT( pTable );

		table.Open( t.szTable.c_str() );

		pTable->PreCreate( t.nTableID );
	}
	for ( k = relations.begin(); k != relations.end(); ++k )
	{
		SRelation &t = *k;

		table.Open( t.szTable.c_str() );
		// determine left/right sides
		t.pLeft = GetTableByName( RelationField2TableName( table.GetFieldName( 0 ) ).c_str() );
		t.pRight = GetTableByName( RelationField2TableName( table.GetFieldName( 1 ) ).c_str() );
		if ( t.pLeft == 0 || t.pRight == 0 )
		{
			ASSERT(0); // field names in relation does not match table names
			k = relations.erase( k );
		}
    t.data.clear();
		// load data into SRelation
		for ( ; !table.IsEof(); table.MoveNext() )
		{
			SRelation::SElement &res = *t.data.insert( t.data.end(), SRelation::SElement());
			//
			res.nLeft = table.GetInt( 0 );
			res.nRight = table.GetInt( 1 );
		}
	}

	// 2nd phase - read data for each record
	for ( i = tableDescrs.begin(); i != tableDescrs.end(); ++i )
	{
		STableDescr &t = *i;
		CDBTableBase *pTable = GetTable( t.nTableID );
		ASSERT( pTable );

		table.Open( t.szTable.c_str() );

		pTable->Import();
	}
	table.Close();
	CloseConnection();
	bIsDatabaseLoading = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NDatabase::Refresh( int nTableID )
{
	list<STableDescr> &tableDescrs = GetTableDescrs();
	EstablishConnection( szDataSource.c_str() );
	// 1st phase - refresh records
	list<STableDescr>::iterator i;
	for ( i = tableDescrs.begin(); i != tableDescrs.end(); ++i )
		if ( nTableID == i->nTableID )
			break;
	if ( i == tableDescrs.end() )
	{
		ASSERT(0);
		return;
	}
	STableDescr &t = *i;
	CDBTableBase *pTable = GetTable( t.nTableID );
	ASSERT( pTable );
	table.Open( t.szTable.c_str() );
	pTable->Refresh( t.nTableID );
	// 2nd phase - read data for each record
	table.Close();
	CloseConnection();
	EstablishConnection( szDataSource.c_str() );
	table.Open( t.szTable.c_str() );
 	pTable->Import();
	table.Close();
	CloseConnection();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// v1.2 @0x7ef6d0-family: the scalar ImportFields now report success -- a missing
// column returns false and leaves *pData untouched (see ADOImport\BasicDB.cpp).
bool NDatabase::ImportField( const char *pszFieldName, int *pData )
{
	if ( !table.HasField( pszFieldName ) )
		return false;
	*pData = table.GetInt( pszFieldName );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool NDatabase::ImportField( const char *pszFieldName, bool *pData )
{
	if ( !table.HasField( pszFieldName ) )
		return false;
	*pData = table.GetBool( pszFieldName );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool NDatabase::ImportField( const char *pszFieldName, float *pData )
{
	if ( !table.HasField( pszFieldName ) )
		return false;
	*pData = table.GetFloat( pszFieldName );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool NDatabase::ImportField( const char *pszFieldName, std::string *pData )
{
	if ( !table.HasField( pszFieldName ) )
		return false;
	*pData = (const char*)table.GetString( pszFieldName );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool NDatabase::ImportField( const char *pszFieldName, std::wstring *pData )
{
	if ( !table.HasField( pszFieldName ) )
		return false;
	*pData = (const wchar_t*)table.GetString( pszFieldName );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NDatabase::ImportRelation( CDBRecord *pSrc, CDBTableBase *pDestTable, std::vector< CPtr<CDBRecord> > *pRefs )
{
	list< SRelation > &relations = GetRelations();
	ASSERT( pDestTable );
	std::vector< CPtr<CDBRecord> > &refs = *pRefs;
	list<SRelation>::iterator k;
	CDBTableBase *pLeft = GetTableByRecord( pSrc );
	CDBTableBase *pRight = pDestTable;
	refs.clear();
	if ( pLeft == 0 || pRight == 0 || pLeft == pRight )
	{
		ASSERT(0); // desired relation exists
		return;
	}
	bool bDone = false;
	for ( k = relations.begin(); k != relations.end(); ++k )
	{
		if ( k->pLeft == pLeft && k->pRight == pRight ) 
		{ // normal order
			ASSERT( !bDone );
			bDone = true;
			int nLeftID = pSrc->GetRecordID();
			for ( int z = 0; z < k->data.size(); z++ )
			{
				if ( k->data[z].nLeft == nLeftID )
				{
					CDBRecord *pAdd = pRight->GetDBRecord( k->data[z].nRight );
					if ( pAdd != 0 )
						refs.push_back( pAdd );
					else
						ASSERT(0); // relation points to non existing record
				}
			}
		}
		if ( k->pLeft == pRight && k->pRight == pLeft ) 
		{ // reverse order
			ASSERT( !bDone );
			bDone = true;
			int nLeftID = pSrc->GetRecordID();
			for ( int z = 0; z < k->data.size(); z++ )
			{
				if ( k->data[z].nRight == nLeftID )
				{
					CDBRecord *pAdd = pRight->GetDBRecord( k->data[z].nLeft );
					if ( pAdd != 0 )
						refs.push_back( pAdd );
					else
						ASSERT(0); // relation points to non existing record
				}
			}
		}
	}
	ASSERT( bDone );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NDatabase::ImportField( const char *pszFieldName, CDBRecord **pRef, CDBTableBase *pDestTable )
{
	ASSERT( pDestTable );
	*pRef = 0;
	int nID = table.GetInt( pszFieldName );
	if ( pDestTable )
		*pRef = pDestTable->GetDBRecord( nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NDatabase::Serialize( CDataStream &file, CStructureSaver::EMode mode )
{
	CTablesHash &tables = GetTables();
	NDatabase::bIsDatabaseLoading = true;
	{
		CStructureSaver f( file, mode );
		f.Add( 1, &tables );
	}
	NDatabase::bIsDatabaseLoading = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBTableBase
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NDatabase;
void CDBTableBase::PreCreate( int nTypeID )
{
  records.clear();
	// iterate through recordset & create records
	for ( ; !table.IsEof(); table.MoveNext() )
	{
		CDBRecord *pRes = GetRecordTypes().CreateObject( nTypeID );
		ASSERT( pRes );
		if ( !pRes )
			break;
		pRes->nID = table.GetInt( "ID" );
#ifdef _DEBUG
		CRecordHash::iterator i = records.find( pRes->nID );
		if ( i != records.end() )
			ASSERT(0);  // record with this ID already created
#endif
		records[ pRes->nID ] = pRes;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBTableBase::Refresh( int nTypeID )
{
	Sleep(0); // �������� ���������� ������� ������ (������ � ���� ������ ���������� �� � ������� ������)
	const CRecordHash copy = records;
  records.clear();
	// iterate through recordset & create records
	for ( ; !table.IsEof(); table.MoveNext() )
	{
		CObj<CDBRecord> pRes = GetRecordTypes().CreateObject( nTypeID );
		ASSERT( pRes );
		if ( !IsValid( pRes ) )
			break;
		pRes->nID = table.GetInt( "ID" );
		CRecordHash::const_iterator it = copy.find( pRes->nID );
		if ( it != copy.end() && IsValid( it->second ) )
			pRes = it->second;
#ifdef _DEBUG
		CRecordHash::iterator i = records.find( pRes->nID );
		if ( i != records.end() )
			ASSERT(0);  // record with this ID already created
#endif
		records[ pRes->nID ] = pRes;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBTableBase::Import()
{
	// iterate through records & Import() them
	for ( ; !table.IsEof(); table.MoveNext() )
	{
		int nID = table.GetInt( "ID" );
		CRecordHash::iterator i = records.find( nID );
		if ( i == records.end() ) 
		{
			ASSERT(0); // this record should be created on PreCreate stage
			continue;
		}
		i->second->Import();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CDBRecord* CDBTableBase::GetDBRecord( int nID )
{
	CRecordHash::iterator i = records.find( nID );
	if ( i == records.end() ) 
	{
#ifdef _MAPEDIT
		if ( !bIsDatabaseLoading && nID > 0 )
		{
			CTablesHash& tables = GetTables();
			for ( CTablesHash::const_iterator it = tables.begin(); it != tables.end(); ++it )
				if ( &it->second == this )
				{
					list<STableDescr> &tableDescrs = GetTableDescrs();
					for ( list<STableDescr>::iterator i = tableDescrs.begin(); i != tableDescrs.end(); ++i )
						if ( it->first == i->nTableID )
						{
							COLETable tbl;
							bool bClose = false;
							CDBRecord *pRes = 0;

							if ( !pConnection.GetInterfacePtr() )
							{
								EstablishConnection( szDataSource.c_str() );
								bClose = true;
							}
							if ( tbl.Open( i->szTable.c_str(), nID ) )
							{
								pRes = GetRecordTypes().CreateObject( it->first );
								ASSERT( pRes );
								if ( pRes )
								{
									pRes->nID = tbl.GetInt( "ID" );
									records[ pRes->nID ] = pRes;
									COLETable holder;
									holder = table; // CRAP
									table = tbl;
									pRes->Import();
									table = holder;
								}
							}
							if ( bClose )
								CloseConnection();
							return pRes;
						}
					return 0;
				}
		}
#endif
		return 0;
	}
	return i->second;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string NDatabase::GetDBConnectionStr( const string &szDBName )
{
	string szRet = "DRIVER=SQL Server;SERVER=";
	szRet += szDBName;
	szRet += ";UID=sa;PWD=simple;DATABASE=A5GAME_English (USA);";
	return szRet;
//	return "DRIVER=SQL Server;SERVER=localhost;UID=sa;DATABASE=A5GAME;";
//	return "DRIVER=SQL Server;SERVER=localhost;UID=sa;PWD=simple;DATABASE=A5GAME;";
//  return string( "DBQ=" ) + szDBName
//	 + ";DRIVER=Microsoft Access Driver (*.mdb);UserCommitSync=Yes;Threads=3;"
//	 + "SafeTransactions=0;PageTimeout=5;MaxScanRows=8;MaxBufferSize=2048;"
//	 + "FIL=MS Access;DriverId=25;";
//   + ";Driver={Driver do Microsoft Access (*.mdb)};DriverId=25;FIL=MS Access;"
//   + "MaxBufferSize=2048;MaxScanRows=8;PageTimeout=5;SafeTransactions=0;Threads=3;";
}
