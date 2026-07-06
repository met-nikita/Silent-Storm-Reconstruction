#ifndef __BASICDB_H_
#define __BASICDB_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Misc\basic2.h"
#include "..\FileIO\BasicChunk1.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBRecord;
namespace NDatabase {
	extern CClassFactory<CDBRecord>& GetRecordTypes();
	externA5 bool bIsDatabaseLoading;
	//
	extern void Import();
	void Refresh( int nTableID );
	template<class T> void Refresh( T *pDest = 0 ) { T *p = 0; Refresh( GetRecordTypes().GetTypeID( p ) );	}
	// forward-declared here so CDBTableBase can befriend it (full decl is below, after the class)
	void Serialize( CDataStream &file, CStructureSaver::EMode mode );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBTableBase;
class CDBRecord: public CObjectBase
{
	int nID;
public:
	int GetRecordID() const { return nID; }
	int operator&( CStructureSaver &f ) { ASSERT( NDatabase::bIsDatabaseLoading ); f.Add( 1, &nID ); return 0; }
	virtual void Import() = 0;
	friend class CDBTableBase;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> 
class CDBPtr: public CPtrBase<T, CDBRecord::SRef>
{
	typedef CPtrBase<T, CDBRecord::SRef> CBase;
public:
	CDBPtr() {}
	CDBPtr( T *_ptr ): CBase( _ptr ) {}
	CDBPtr( const CDBPtr &a ): CBase( a.Get() ) {}
	CDBPtr& operator=( T *_ptr ) { Set( _ptr ); return *this; }
	CDBPtr& operator=( const CDBPtr &a ) { SetObject( a.Get() ); return *this; }
	//
	int operator&( CStructureSaver &f ) 
	{ 
		if ( NDatabase::bIsDatabaseLoading )
		{
			ASSERT( 0 );
			f.DoPtr( this );
		}
		else
		{
			if ( f.IsReading() )
			{
				int nID = -1;
				f.Add( 1, &nID );
				CDBTable<T> *pTable = NDatabase::GetTable<T>();
				ASSERT( pTable );
				if ( pTable )
					*this = ( pTable->GetRecord( nID ) );
				ASSERT( nID == -1 || GetPtr() );
			}
			else
			{
				CDBRecord *pRec = (CDBRecord*)GetPtr();
				int nID = pRec ? pRec->GetRecordID() : -1;
				f.Add( 1, &nID );
			}
		}
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBTableBase
{
	typedef std::unordered_map<int, CObj<CDBRecord> > CRecordHash;
	CRecordHash records;
	//
	void PreCreate( int nTypeID );
	void Refresh( int nTypeID );
	void Import();
public:
	CDBRecord* GetDBRecord( int nID );
	int operator&( CStructureSaver &f ) 
	{
		f.Add( 1, &records );
		return 0;
	}
	friend void NDatabase::Import();
	friend void NDatabase::Refresh( int nTableID );
	friend void NDatabase::Serialize( CDataStream &file, CStructureSaver::EMode mode );
  friend class CDBIteratorBase;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
class CDBTable: public CDBTableBase
{
public:
	T* GetRecord( int nID ) 
	{
		CDBRecord *pRes = CDBTableBase::GetDBRecord(nID); 
		ASSERT( pRes ==0 || typeid(*pRes) == typeid(T) ); 
		ASSERT( pRes || nID < 1 );
		return (T*)pRes;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDBIteratorBase
{
  const CDBTableBase &table;
  CDBTableBase::CRecordHash::const_iterator i;
protected:
  CDBRecord* Get() const { return i->second; }
  CDBIteratorBase( const CDBTableBase &t ): table(t) { MoveFirst(); }
public:
  void MoveFirst() { i = table.records.end(); }
  bool MoveNext() 
  { 
    if ( i == table.records.end() ) 
      i = table.records.begin(); 
    else 
      ++i;
    return i != table.records.end();
  }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
class CDBIterator: public CDBIteratorBase
{
public:
	CDBIterator( const CDBTable<T> &t ): CDBIteratorBase( t ) {}
	T* Get() const { return (T*)CDBIteratorBase::Get(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDatabase
{
	typedef CDBRecord* (*RecordCreateFunc)();
  string GetDBConnectionStr( const string &szDBName );
	void SetSource( const char *pszSource );
	void AddTable( int nTableID, const char *pszTableName, RecordCreateFunc newf );
	void AddRelation( const char *pszTableName );
	CDBTableBase* GetTable( int nTableID );
	template<class T>
		CDBTable<T>* GetTable( CDBTable<T> **pDest = 0 )
		{
			T *p = 0;
			CDBTable<T> *pRes = (CDBTable<T>*) GetTable( GetRecordTypes().GetTypeID( p ) );
			if ( pDest )
				*pDest = pRes;
			return pRes;
		}
	inline CDBTableBase* GetTableByRecord( CDBRecord *p ) { return GetTable( GetRecordTypes().GetObjectTypeID( p ) ); }
	void Serialize( CDataStream &file, CStructureSaver::EMode mode );
	// release BasicDB.obj @0x3570 -- drop all tables/records/relations, then re-create one empty
	// table entry per registered descriptor (CModManager::Activate teardown before the DB reload)
	void ClearDatabaseTables();
	//
	// v1.2 @0x7ef6d0-family: scalar field imports report success (the v1.2 IOLETable
	// getters @0x401900..0x402740 return a bool and yield the value via out-param).
	// A missing column returns false and leaves *pData UNTOUCHED, so guarded call
	// sites keep the record's previous value when a (mod) db lacks the column.
	// The record-reference overloads below keep the v1.1 always-assign form.
	bool ImportField( const char *pszFieldName, int *pData );
	bool ImportField( const char *pszFieldName, bool *pData );
	bool ImportField( const char *pszFieldName, float *PData );
	bool ImportField( const char *pszFieldName, std::string *pData );
	bool ImportField( const char *pszFieldName, std::wstring *pData );
	// simple reference
	void ImportField( const char *pszFieldName, CDBRecord **pRef, CDBTableBase *pDestTable );
	template< class T >
	void ImportField( const char *pszFieldName, CPtr<T> *pRef )
	{
		CDBRecord *pRes;
		CDBTable<T> *pTable = NDatabase::GetTable<T>();
		ImportField( pszFieldName, &pRes, pTable );
		*pRef = (T*)pRes;
	}
	// array of refs
	void ImportRelation( CDBRecord *pSrc, CDBTableBase *pDestTable, std::vector< CPtr<CDBRecord> > *pRefs );
	template< class T >
	void ImportRelation( CDBRecord *pSrc, std::vector< CPtr<T> > *pRefs )
	{
		ImportRelation( pSrc, GetTable<T>(), (std::vector<CPtr<CDBRecord> >*) pRefs );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#define REGISTER_DATABASE_CLASS( N, table, name ) NDatabase::AddTable( N, table, \
(NDatabase::RecordCreateFunc)name##::New##name );
#define REGISTER_DATABASE_CLASS_TEMPL( N, table, name,className ) NDatabase::AddTable( N, table, \
(NDatabase::RecordCreateFunc)name##::New##className );
#define REGISTER_DATABASE_RELATION( table ) NDatabase::AddRelation( table );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif