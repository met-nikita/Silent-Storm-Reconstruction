#include "dbDefs.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// „тение данных из записи и сохранение их в списке свойств pPropMap
template<class PROP_MAP> bool ReadPropsFromDB( CCommand<CDynamicAccessor > *pDB, const PROP_MAP *pPropMap )
{
	USES_CONVERSION;
  if ( !pPropMap )
    return true;
  //
  for ( PROP_MAP::const_iterator it = pPropMap->begin(); it != pPropMap->end(); ++it )
  {
    TCHAR    colName[128];    
		DBSTATUS status;
		DBTYPE dbtype;
		DBORDINAL col;

    _tcsncpy( colName, it->first.c_str(), sizeof( colName ) );
		if ( !pDB->GetStatus( colName, &status ) )
			continue;
		pDB->GetOrdinal( colName, &col );
		pDB->GetColumnType( col, &dbtype );
		if ( DBSTATUS_S_ISNULL == status )
		{
			it->second->SetValue( CVariant(), false );
			continue;
		}
		if ( DBSTATUS_S_OK != status )
			continue;
		//
		switch ( dbtype )
		{
			default:
				switch ( it->second->GetType() )
				{
				case CVariant::VT_BOOL:
					{
						USHORT val;
						if ( !pDB->GetValue( colName, &val ) )
							continue;
						it->second->SetValue( (bool)val, false );
					}
					break;
				case CVariant::VT_INT:
					{
						int val = 0;
						if ( !pDB->GetValue( colName, &val ) )
							continue;
						it->second->SetValue( val, false );
					}
					break;
				case CVariant::VT_FLOAT:
					{
						float val;
						if ( !pDB->GetValue( colName, &val ) )
							continue;
						it->second->SetValue( val, false );
					}
					break;
				case CVariant::VT_STR:
					{
						ULONG l;
						pDB->GetLength( colName, &l );
						vector<char> vStr( l + 1 );
						char *pStr = (char*)pDB->GetValue( colName );
						if ( !pStr )
							continue;
						memcpy( &vStr[0], pStr, l );
						vStr[l] = 0;
						it->second->SetValue( &vStr[0], false );
					}      
					break;
				}
				break;
			case DBTYPE_IUNKNOWN:
				{
					// first we have to determine what was the column's type originally reported by the provider
					CComHeapPtr<DBCOLUMNINFO> spColumnInfo;
					CComHeapPtr<OLECHAR> spStringsBuffer;
					DBORDINAL nColumns = 0;
					
					HRESULT hres = pDB->CDynamicAccessor::GetColumnInfo( pDB->m_spRowset, &nColumns, &spColumnInfo, &spStringsBuffer );
					ATLASSERT( SUCCEEDED( hres ) );
					ATLASSERT( col <= nColumns );
					DBTYPE wType = spColumnInfo[col-1].wType;

          IUnknown* pUnknown = *(IUnknown**)pDB->GetValue( colName );
					ATLASSERT( pUnknown != NULL );
					if ( !pUnknown )
						break;
					// First, try to obtain the ISequentialStream pointer
					CComPtr<ISequentialStream> spSequentialStream;
					hres = pUnknown->QueryInterface( __uuidof(ISequentialStream), (void**)&spSequentialStream );
					vector<char> vStr;
					if( SUCCEEDED(hres) && spSequentialStream )
					{
						switch( wType )
						{
						case DBTYPE_STR:
							{
								vector<char> buffer(101);
								ULONG cbRead = 0;
								hres = spSequentialStream->Read( (void*)&buffer[0], 100, &cbRead );
								while( SUCCEEDED(hres) && cbRead > 0 )
								{
									buffer[cbRead] = 0;
									vStr.insert( vStr.end(), buffer.begin(), buffer.begin() + cbRead );
									hres = spSequentialStream->Read( (void*)&buffer[0], 100, &cbRead );
								}
								//it->second->SetValue( &vStr[0], false );
							}
						case DBTYPE_WSTR:
							{
								vector<wchar_t> buffer(201);
								ULONG cbRead = 0;
								hres = spSequentialStream->Read( (void*)&buffer[0], 200, &cbRead );
								while( SUCCEEDED(hres) && cbRead > 0 )
								{
									buffer[cbRead/2] = L'\0';
									LPSTR pName = W2A( &buffer[0] );
									string str = pName;
									vStr.insert( vStr.end(), str.begin(), str.end() );
									hres = spSequentialStream->Read( (void*)&buffer[0], 200, &cbRead );
								}
							}
							/*
						case DBTYPE_BYTES:
							{
								BYTE buffer[100];
								ULONG cbRead = 0;
								hres = spSequentialStream->Read( (void*)buffer, 100, &cbRead );
								while( SUCCEEDED(hres) && cbRead > 0 )
								{
									char szHex[3];
									szHex[2] = 0;
									for( ULONG n = 0; n < cbRead; n++ )
									{
										d2h( buffer[n], szHex );
										printf( "%s", szHex );
									}
									hres = spSequentialStream->Read( (void*)buffer, 100, &cbRead );
								}
							}
							*/
						}
						vStr.push_back( '\0' );
						it->second->SetValue( &vStr[0], false );
					}
				}

				break;
		}
  }
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSeqStream: public ISequentialStream
{
	int nRefCount;
	int nPos;
public:
	vector<BYTE> data;

	CSeqStream(): nRefCount(0), nPos(0) { AddRef(); }
	virtual HRESULT STDMETHODCALLTYPE Read( void *pv, ULONG cb, ULONG *pcbRead) 
	{
		int nBytes = Min( (int)cb, int(data.size() - nPos) );
		memcpy( pv, &data[nPos], nBytes );
		nPos += nBytes;
		if ( pcbRead )
			*pcbRead = nBytes;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE Write( const void *pv, ULONG cb, ULONG *pcbWritten) 
	{ 
		return E_FAIL; 
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, void __RPC_FAR *__RPC_FAR *ppvObject) 
	{
			*ppvObject = this;
			AddRef();
			return S_OK;
	};
	    
	virtual ULONG STDMETHODCALLTYPE AddRef( void) { ++nRefCount; return nRefCount; }
	    
	virtual ULONG STDMETHODCALLTYPE Release( void) { --nRefCount; if ( 0 == nRefCount ) delete this; return nRefCount; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool WriteBLOB( const CVariant &val, DBORDINAL iColumn, CCommand<CDynamicAccessor > *pDB )
{
	USES_CONVERSION;
	CSeqStream *pISeqStr = new CSeqStream;
	string str = val;

	switch( val.GetType() ) 
	{
		case CVariant::VT_STR:
			{
				LPWSTR pStr = A2W( str.c_str() );
				int nBytes = sizeof( wchar_t ) * wcslen( pStr );
				pISeqStr->data.resize( nBytes );
				memcpy( &pISeqStr->data[0], pStr, nBytes );
			}
			break;
		default:
			pISeqStr->data.insert( pISeqStr->data.end(), str.begin(), str.end() );
	}
	HROW hrow = pDB->m_hRow;

	HACCESSOR hAccessor;
	DBBINDSTATUS rgStatus[1];
	DBOBJECT ObjectStruct;
	DBBINDING rgBinding[1] = {
		iColumn, // Column 1
		0, // Offset to data
		sizeof(IUnknown*), // obLength length field
		0, // Ignore status field
		NULL, // No type info
		&ObjectStruct, // Object structure
		NULL, // Ignore binding extension
		DBPART_VALUE|DBPART_LENGTH, // Bind value and length
		DBMEMOWNER_CLIENTOWNED, // Consumer owned memory
		DBPARAMIO_NOTPARAM, // Not a parameter binding
		0, // Ignore maxlength
		0, // Reserved
		DBTYPE_IUNKNOWN, // Type DBTYPE_IUNKNOWN
		0, // Precision not applicable
		0, // Scale not applicable
	};

	ObjectStruct.dwFlags = STGM_READ;
	ObjectStruct.iid = IID_ISequentialStream;
	IAccessor* pIAccessor;
	HRESULT hrr = pDB->m_spRowsetChange->QueryInterface(IID_IAccessor, (void**) &pIAccessor);
	hrr = pIAccessor->CreateAccessor(DBACCESSOR_ROWDATA, 1, rgBinding, sizeof(IUnknown *) + sizeof(ULONG), &hAccessor, rgStatus);
	if ( FAILED( hrr ) )
	{
		DisplayOLEDBErrorRecords( hrr );
		return false;
	}  

	hrr = pIAccessor->Release();
	// Assume you already have a row handle (HROW). Call
	// SetData and pass it the address of the pointer to the
	// object ISequentialStream not in the rowset. The rowset
	// will copy the data from the ISequentialStream object to
	// the rowset. Set up pData row buffer.
	BYTE* pData=(BYTE*)CoTaskMemAlloc(sizeof(IUnknown*)+sizeof(ULONG));
	// Value - pass ISequentialStream pointer to the provider.
	*(ISequentialStream**)(pData+rgBinding[0].obValue)=pISeqStr;
	// LENGTH - Some providers need to know the length of the
	// stream ahead of time...
	*(ULONG*)(pData+rgBinding[0].obLength)=pISeqStr->data.size();
	// SetData - The provider will then do an ISequentialStream::Read
	// on the pISeqStr pointer passed in...
	hrr = pDB->m_spRowsetChange->SetData(hrow, hAccessor, pData);
	if ( FAILED( hrr ) )
	{
		DisplayOLEDBErrorRecords( hrr );
		return false;
	}  
	CoTaskMemFree(pData);
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool GetOrdinal( const TCHAR *colName, DBORDINAL *pOrdinal, DBCOLUMNINFO *pColumnInfo, DBORDINAL nColumns )
{
	USES_CONVERSION;
	LPWSTR pName = A2W( colName );
  for ( int i = 0; i < nColumns; ++i )
  {
		if ( wcscmp( pName, pColumnInfo[i].pwszName ) == 0 )
		{
			*pOrdinal = pColumnInfo[i].iOrdinal;
			return true;
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsBLOB( const string &szColumn, DBCOLUMNINFO* pColumnInfo, DBORDINAL nColumns )
{
	USES_CONVERSION;
	LPWSTR pName = A2W( szColumn.c_str() );
  for ( int i = 0; i < nColumns; ++i )
  {
		if ( wcscmp( pName, pColumnInfo[i].pwszName ) == 0 )
			return pColumnInfo[i].dwFlags & DBCOLUMNFLAGS_ISLONG;
  }
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// «аполнение полей данными из списка свойств pPropMap
// чтобы сохраненить изменени€ в базе небходимо вызвать pDB->SetData()
// bDefValues - см. CItemsDBCmd::UpdateItem()
template<class PROP_MAP> bool WritePropsToDB( CCommand<CDynamicAccessor > *pDB, const PROP_MAP *pPropMap, bool bDefValues = false )
{
  if ( !pPropMap )
    return true;
  //
  DBCOLUMNINFO* pColumnInfo;
  DBORDINAL nColumns;
	OLECHAR *pStrings;
 	((CDynamicAccessor*)pDB)->GetColumnInfo( pDB->GetInterface(), &nColumns, &pColumnInfo, &pStrings );
	//
  for ( PROP_MAP::const_iterator it = pPropMap->begin(); it != pPropMap->end(); ++it )
  {
    TCHAR colName[128];    
		DBORDINAL col;

    _tcsncpy( colName, it->first.c_str(), sizeof( colName ) );

		CVariant varValue = bDefValues ? it->second->GetDefValue() : it->second->GetValue();
		bool bRet = GetOrdinal( colName, &col, pColumnInfo, nColumns );

		if ( bRet && IsBLOB( colName, pColumnInfo, nColumns ) )
		{
			WriteBLOB( varValue, col, pDB );
			continue;
		}
		// провер€ем само значение на пустое
		if ( CVariant::VT_NULL == varValue.GetType() )
		{
			//if (  !bDefValues)
			/*
			if ( it->second->GetType() == CVariant::VT_STR )
			{
				pDB->SetStatus( colName, DBSTATUS_S_OK );
				continue;
			}
			*/
			DBSTATUS status;
			bool bRet = pDB->GetStatus( colName, &status );
			bRet = pDB->SetStatus( colName, DBSTATUS_S_ISNULL );
			bRet = pDB->GetStatus( colName, &status );
			continue;
		}
		//
		switch ( it->second->GetType() )
		{
		case CVariant::VT_BOOL:
			{
				USHORT val = (bool)varValue;
				if ( !pDB->SetValue( colName, val ) )
					return false;
			}
			break;
		case CVariant::VT_INT:
			{
				int val = varValue;
				if ( !pDB->SetValue( colName, val ) )
					return false;
			}
			break;
		case CVariant::VT_FLOAT:
			{
				float val = varValue;
				if ( !pDB->SetValue( colName, val ) )
					return false;
			}
			break;
		case CVariant::VT_STR:
			{
				char *pStr = (char*)pDB->GetValue( colName );
				strcpy( pStr, varValue );
				pDB->SetLength( colName, strlen( pStr ) );
			}      
			break;
		}
		pDB->SetStatus( colName, DBSTATUS_S_OK );		
  }
	CoTaskMemFree(pColumnInfo);
  return true;
}
