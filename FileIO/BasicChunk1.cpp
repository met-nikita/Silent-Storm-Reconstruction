#include "StdAfx.h"
#include "BasicChunk1.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// class factory
CClassFactory<CObjectBase> *pSSClasses = 0;
////////////////////////////////////////////////////////////////////////////////////////////////////
void StartRegisterSaveload()
{
	if ( !pSSClasses )
		pSSClasses = new CClassFactory<CObjectBase>;
}
struct SBasicChunkInit {
	~SBasicChunkInit() { if ( pSSClasses ) delete pSSClasses; }
} init;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStructureSaver::CChunkLevel
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver::CChunkLevel::ClearCache() 
{ 
	idLastChunk = (chunk_id)0xff;
	nLastPos = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver::CChunkLevel::Clear() 
{
	idChunk = (chunk_id)0xff; 
	nStart = 0; 
	nLength = 0; 
	ClearCache(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// chunks operations with whole saves
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool ReadShortChunkSave( CDataStream &file, chunk_id &dwID, CMemoryStream &chunk )
{
	DWORD dwLeng = 0;
	try
	{
		file.Read( &dwID, sizeof( dwID ) );
		file.Read( &dwLeng, 1 );
		if ( dwLeng & 1 )
			file.Read( ((char*)&dwLeng)+1, 3 );
		dwLeng >>= 1;
		if ( dwLeng > 100000000 )
			return false;
		chunk.SetSizeDiscard( dwLeng );
		file.Read( chunk.GetBufferForWrite(), dwLeng );
	}
	catch (...)
	{
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool WriteShortChunkSave( CDataStream &file, chunk_id dwID, CMemoryStream &chunk )
{
	DWORD dwLeng;
	file.Write( &dwID, sizeof( dwID ) );
	dwLeng = chunk.GetSize();
	dwLeng <<= 1;
	if ( dwLeng >= 256 )
	{
		dwLeng |= 1;
		file.Write( &dwLeng, sizeof( dwLeng ) );
	}
	else
		file.Write( &dwLeng, 1 );
	file.Write( chunk.GetBuffer(), chunk.GetSize() );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool GetShortChunkSave( CDataStream &file, chunk_id dwID, CMemoryStream &chunk, int nBaseSeek )
{
	chunk_id dwRid;
	file.Seek( nBaseSeek );
	while( ReadShortChunkSave( file, dwRid, chunk ) )
	{
		if ( dwRid == dwID )
			return true;
	}
	chunk.Clear();
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// chunks operations with ChunkLevels
////////////////////////////////////////////////////////////////////////////////////////////////////
static void ReadPtrData( const unsigned char *pData, void *pDst, int &nPos, int nSize )
{
	memcpy( pDst, pData + nPos, nSize );
	nPos += nSize;
}
// should copy data from start
static void WritePtrData( unsigned char *pDst, const void *pSrc, int *nPos, int nSize )
{
	memcpy( pDst + *nPos, pSrc, nSize );
	*nPos += nSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStructureSaver::ReadShortChunk( CChunkLevel &src, int &nPos, CChunkLevel &res )
{
	const unsigned char *pSrc = data.GetBuffer() + src.nStart;
	DWORD dwLeng = 0;
	if ( nPos + 2 > src.nLength )
		return false;
	ReadPtrData( pSrc, &res.idChunk, nPos, sizeof( res.idChunk ) );
	ReadPtrData( pSrc, &dwLeng, nPos, 1 );
	if ( dwLeng & 1 )
		ReadPtrData( pSrc, ((char*)&dwLeng)+1, nPos, 3 );
	dwLeng >>= 1;
	if ( nPos + dwLeng > src.nLength )
		return false;
	res.nStart = nPos + src.nStart;
	res.nLength = dwLeng;
	nPos += dwLeng;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStructureSaver::WriteShortChunk( CChunkLevel &dst, chunk_id dwID, 
																			const unsigned char *pData, int nLength )
{
	DWORD dwLeng;
	data.SetSize( dst.nStart + dst.nLength + 1 + 4 + nLength );
	unsigned char *pDst = data.GetBufferForWrite() + dst.nStart;
	WritePtrData( pDst, &dwID, &dst.nLength, sizeof( dwID ) );
	dwLeng = nLength;
	dwLeng <<= 1;
	if ( dwLeng >= 256 )
	{
		dwLeng |= 1;
		WritePtrData( pDst, &dwLeng, &dst.nLength, sizeof( dwLeng ) );
	}
	else
		WritePtrData( pDst, &dwLeng, &dst.nLength, 1 );
	// prevent copying to itself
	if ( pDst + dst.nLength != pData )
		WritePtrData( pDst, pData, &dst.nLength, nLength );
	else
		dst.nLength += nLength;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStructureSaver::GetShortChunk( CChunkLevel &src, chunk_id dwID, CChunkLevel &res, int nNumber )
{
	ASSERT( dwID != 0xff );
	int nPos = src.nLastPos; // search from last found position
	int nCounter = nNumber;
	if ( src.idLastChunk == dwID )
	{
		if ( nNumber == src.nLastNumber + 1 )
			nCounter = 1;
		else
		{
			// not sequential access, fall back to linear search
			src.ClearCache();
			return GetShortChunk( src, dwID, res, nNumber );
		}
	}
	else 
	{
		if ( nNumber != 0 )
		{
			if ( src.nLastPos != 0 )
			{
				src.ClearCache();
				return GetShortChunk( src, dwID, res, nNumber );
			}
		}
		else
			nCounter = 1;
	}
	while ( ReadShortChunk( src, nPos, res ) )
	{
		if ( res.idChunk == dwID )
		{
			if ( nCounter == 1 )
			{
				src.idLastChunk = dwID;
				src.nLastPos = nPos;
				src.nLastNumber = nNumber;
				return true;
			}
			nCounter--;
		}
	}
	if ( src.nLastPos == 0 )
		return false;
	// search from start
	src.ClearCache();
	return GetShortChunk( src, dwID, res, nNumber );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CStructureSaver::CountShortChunks( CChunkLevel &src, chunk_id dwID )
{
	int nPos = 0, nRes = 0;
	CChunkLevel temp;
	while ( ReadShortChunk( src, nPos, temp ) )
	{
		if ( temp.idChunk == dwID )
			nRes++;
	}
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStructureSaver main methods
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver::DataChunk( const chunk_id idChunk, void *pData, int nSize, int nChunkNumber )
{
#if defined(_DEBUG) && !defined(FAST_DEBUG)
	{
		bool bOk = true;
		try
		{
			//typeid( dynamic_cast<CObjectBase*>( (CObjectBase*)pData ) );
			//CObjectBase *pZdec = dynamic_cast<CObjectBase*>( pData );
			//bOk = false;
		}
		catch(...)
		{
		}
		ASSERT( bOk );
	}
#endif
	CChunkLevel &last = chunks.back();
	if ( IsReading() )
	{
		CChunkLevel res;
		if ( GetShortChunk( last, idChunk, res, nChunkNumber ) )
		{
			ASSERT( res.nLength == nSize );
			memcpy( pData, data.GetBuffer() + res.nStart, nSize );
		}
		else
			memset( pData, 0, nSize );
	}
	else
	{
		ASSERT( CountShortChunks( last, idChunk ) == nChunkNumber - 1 );
		WriteShortChunk( last, idChunk, (const unsigned char*) pData, nSize );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver::WriteRawData( const void *pData, int nSize )
{
	CChunkLevel &res = chunks.back();
	data.SetSize( res.nStart + nSize );
	unsigned char *pDst = data.GetBufferForWrite() + res.nStart;
	WritePtrData( pDst, pData, &res.nLength, nSize );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver::RawData( void *pData, int nSize )
{
	if ( IsReading() )
	{
		CChunkLevel &res = chunks.back();
		ASSERT( res.nLength == nSize );
		memcpy( pData, data.GetBuffer() + res.nStart, nSize );
	}
	else
	{
		WriteRawData( pData, nSize );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver::DataChunkString( stdString &str )
{
	if ( IsReading() )
	{
		CChunkLevel &res = chunks.back();
		const char *pStr = (const char*)( data.GetBuffer() + res.nStart );
		str.assign( pStr, res.nLength );
	}
	else
	{
		WriteRawData( str.data(), str.size() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver::DataChunkString( stdWString &str )
{
	if ( IsReading() )
	{
		CChunkLevel &res = chunks.back();
		const wchar_t *pStr = (wchar_t*) ( data.GetBuffer() + res.nStart );
		str.assign( pStr, res.nLength / 2 );
	}
	else
	{
		WriteRawData( str.data(), str.size() * 2 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver::DataChunkBLOB( CMemoryStream &file )
{
	int nLeng = file.GetSize();
	Add( 1, &nLeng );
	if ( IsReading() )
	{
		file.Clear();
		file.SetSizeDiscard( nLeng );
	}
	DataChunk( 2, file.GetBufferForWrite(), nLeng, 1 );
	file.Seek( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver::StoreObject( CObjectBase *pObject )
{
	if ( pObject != 0 && storedObjects.find( pObject ) == storedObjects.end() )
	{
		toStore.push_back( pObject );
		storedObjects[pObject] = true; // ����� ��������� ���� ���-������
	}
	RawData( &pObject, 4 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CStructureSaver::LoadObject()
{
	void *pServerPtr = 0;
	RawData( &pServerPtr, 4 );
	if ( pServerPtr != 0 )
	{
		CObjectsHash::iterator pFound = objects.find( pServerPtr );
		if ( pFound != objects.end() )
			return pFound->second;
		ASSERT(0);
		// here  we are in problem - stored object does not exist
		// actually i think we got to throw the exception
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStructureSaver::StartChunk( const chunk_id idChunk, int nChunkNumber )
{
	CChunkLevel &last = chunks.back();
	chunks.push_back(CChunkLevel());
	if ( IsReading() ) 
	{
		bool bRes = GetShortChunk( last, idChunk, chunks.back(), nChunkNumber );
		if ( !bRes )
			chunks.pop_back();
		return bRes;
	}
	else 
	{
		ASSERT( CountShortChunks( last, idChunk ) == nChunkNumber - 1 );
		CChunkLevel &newChunk = chunks.back();
		newChunk.idChunk = idChunk;
		newChunk.nStart = last.nStart + last.nLength + sizeof( chunk_id ) + 4;
		return true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver::FinishChunk()
{
	if ( IsReading() ) 
	{
		chunks.pop_back();
	}
	else 
	{
		CChunkLevelReverseIterator it = chunks.rbegin(), it1;
		it1 = it; ++it1;
		WriteShortChunk( *it1, it->idChunk, data.GetBuffer() + it->nStart, it->nLength );
		chunks.pop_back();
		AlignDataFileSize();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver::AlignDataFileSize()
{
	CChunkLevel &last = chunks.back();
	data.SetSize( last.nStart + last.nLength );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CStructureSaver::CountChunks( const chunk_id idChunk )
{
	return CountShortChunks( chunks.back(), idChunk );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver::Start( bool bRead )
{
	int nBaseSeek = destStream.GetPosition();
	CDataStream &res = destStream;
	//
	chunks.clear();
	obj.Clear();
	data.Clear();
	chunks.push_back(CChunkLevel());
	bIsReading = bRead;
	if ( !bRead )
	{
		// We can WRITE only the legacy (v0) content layout (typed-object records). The release's
		// v1 columnar storage is read-only here (no storage-write path), so do NOT stamp v1 - a
		// v1-stamped file with v0 content would misread on load. Dev-written files stay v0.
		nVersion = 0;
		return;
	}
	if ( bRead )
	{
		// read format version (top-level chunk id 4). Absent => legacy v0.
		nVersion = 0;
		{
			CMemoryStream verChunk;
			if ( GetShortChunkSave( res, 4, verChunk, nBaseSeek ) && verChunk.GetSize() >= 4 )
				memcpy( &nVersion, verChunk.GetBuffer(), 4 );
		}
		// read chunk with objects description
		GetShortChunkSave( res, 0, obj, nBaseSeek );
		GetShortChunkSave( res, 2, data, nBaseSeek );
		chunks.back().nLength = data.GetSize();
		// create all objects from obj
		while ( obj.GetPosition() < obj.GetSize() )
		{
			int nTypeID = 0;
			void *pServer = 0;
			bool bValid;
			obj.Read( &nTypeID, 4 );
			obj.Read( &pServer, 4 );
			obj.Read( &bValid,1 );
			CObjectBase *pObject = pSSClasses->CreateObject( nTypeID );
			ASSERT( pObject );
			if ( !bValid )
			{
				// make object invalid
				CPtr<CObjectBase> pTemp( pObject );
				{
					CObj<CObjectBase> pTempObj( (CObjectBase*)pObject );
				}
				pTemp.Extract();
			}
			toStore.push_back( pObject );
			objects[pServer] = pObject;
		}
		// read information about every created object
		int nCount = CountChunks( (chunk_id) 1 );
		for ( int i = 0; i < nCount; i++ )
		{
			void *pServer = 0;
			CObjectBase *pObject;
			StartChunk( (chunk_id) 1, i + 1 );
			DataChunk( 0, &pServer, 4, 1 );
			pObject = objects[pServer];
			ASSERT( pObject );
			if ( pObject )
			{
				if ( StartChunk( 1, 1 ) )
				{
					(*pObject)&( *this );
					FinishChunk();
				}
			}
			FinishChunk();
		}
		// read main objects data
		chunks.back().Clear();
		GetShortChunkSave( res, 1, data, nBaseSeek );
		chunks.back().nLength = data.GetSize();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver::Finish()
{
	CDataStream &res = destStream;
	ASSERT( chunks.size() == 1 );
	if ( !IsReading() )
	{
		// NB: we intentionally do NOT write a version chunk (id 4) - dev output stays legacy v0
		// (typed-object records), which is the only content layout we can write. See Start().
		// save standard data
		AlignDataFileSize();
		WriteShortChunkSave( res, 1, data );
		// store referenced objects
		data.Clear();
		chunks.back().Clear();
		for ( int nObject = 1; !toStore.empty(); ++nObject )
		{
			CObjectBase *pObject = toStore.front();
			toStore.pop_front();
			// save object type and its server pointer
			int nTypeID = pSSClasses->GetObjectTypeID( pObject );
			bool bValid = IsValid( pObject );
			ASSERT( nTypeID != -1 );
			obj.Write( &nTypeID, 4 );
			obj.Write( &pObject, 4 );
			obj.Write( &bValid, 1 );
			// save object data
			StartChunk( (chunk_id) 1, nObject );
			DataChunk( 0, &pObject, 4, 1 );
			//
			if ( StartChunk( 1, 1 ) )
			{
				(*pObject)&( *this );
				FinishChunk();
			}
			FinishChunk();
		}
		// save data into resulting file
		WriteShortChunkSave( res, 0, obj );
		AlignDataFileSize();
		WriteShortChunkSave( res, 2, data );
	}
	obj.Clear();
	data.Clear();
	objects.clear();
	storedObjects.clear();
	toStore.clear();
	chunks.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
