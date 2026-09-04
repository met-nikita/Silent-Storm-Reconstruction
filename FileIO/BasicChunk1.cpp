#include "StdAfx.h"
#include "BasicChunk1.h"
#include "Cruncher.h"   // CNetCompressor -- the packed (retail v1) save-chunk codec
#include <cstdarg>
////////////////////////////////////////////////////////////////////////////////////////////////////
// save-load load-trace diagnostic (dev harness) -- see BasicChunk1.h
bool g_bSaveLoadDiag = false;
bool g_bSaveLoadTeardown = false;   // set during Finish's object-table teardown (see BasicChunk1.h)
void SaveLoadDiag( const char *szFmt, ... )
{
	if ( !g_bSaveLoadDiag )
		return;
	FILE *pF = fopen( "_saveload.log", "a" );
	if ( !pF )
		return;
	va_list ap;
	va_start( ap, szFmt );
	vfprintf( pF, szFmt, ap );
	va_end( ap );
	fclose( pF );   // reopen/close per line so an AV mid-load still leaves a flushed file
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// [HARNESS] wire audit -- see BasicChunk1.h. Findings are aggregated per (class, kind, tag path)
// and dumped once per load so 45k-object saves don't spam the log.
bool g_bWireAudit = false;
#include <map>
#include <string>
namespace NWireAudit
{
	struct SFrame
	{
		char idChunk;
		bool bRaw;                      // frame content consumed as RAW payload (ref/blob/string) -- not chunked
		std::map<char, int> consumed;   // tag id -> highest instance consumed (sequential reads count up)
		SFrame( char id ): idChunk( id ), bRaw( false ) {}
	};
	static std::vector<SFrame> frames;      // mirrors CStructureSaver::chunks on the read path
	static int nTypeId = 0;                 // saveload class id of the object being deserialized
	static std::map<std::string, int> findings;

	static void Finding( const char *szKind, int idTarget, int nSaveLen, int nDevLen )
	{
		char szPath[128];
		int nPos = 0;
		szPath[0] = 0;
		// frames [0]=root, [1]=per-object record (id 1), [2]=the object's op& data (id 1), [3..]=nested
		for ( int i = 3; i < (int)frames.size() && nPos < (int)sizeof(szPath) - 16; ++i )
			nPos += sprintf( szPath + nPos, "%d.", (int)frames[i].idChunk );
		sprintf( szPath + nPos, "%d", idTarget );
		char szKey[256];
		sprintf( szKey, "0x%08X %-6s tag %-10s save=%d dev=%d", nTypeId, szKind, szPath, nSaveLen, nDevLen );
		findings[std::string( szKey )]++;
	}
	static void Dump()
	{
		FILE *pF = fopen( "_wireaudit.log", "a" );
		if ( !pF )
			return;
		for ( std::map<std::string, int>::const_iterator i = findings.begin(); i != findings.end(); ++i )
			fprintf( pF, "%s x%d\n", i->first.c_str(), i->second );
		fprintf( pF, "WIRE-AUDIT-DONE (%d distinct findings)\n", (int)findings.size() );
		fclose( pF );
		findings.clear();
	}
}
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
static bool GetShortChunkSave( CDataStream &file, chunk_id dwID, CMemoryStream &chunk, int nBaseSeek, bool bPacked = false )
{
	chunk_id dwRid;
	file.Seek( nBaseSeek );
	if ( !bPacked )
	{
		while( ReadShortChunkSave( file, dwRid, chunk ) )
		{
			if ( dwRid == dwID )
				return true;
		}
		chunk.Clear();
		return false;
	}

	// retail GetShortChunkSave @0x3f1d80, packed arm: the payload chunk is a CNetCompressor
	// stream ([u32 uncompressed size][LZ-coded data]) -- read the raw chunk into a temp
	// stream, Unpack it into the destination, and rewind the destination.
	CMemoryStream packedChunk;
	while( ReadShortChunkSave( file, dwRid, packedChunk ) )
	{
		if ( dwRid == dwID )
		{
			packedChunk.Seek( 0 );
			chunk.SetSizeDiscard( 0 );
			CNetCompressor cruncher;
			cruncher.Unpack( packedChunk, chunk );
			chunk.Seek( 0 );
			return true;
		}
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
				// [HARNESS] wire audit: record consumption in src's mirror frame. src is either
				// chunks.back() (DataChunk & friends) or its parent (StartChunk pushed the new
				// level before fetching) -- with the audit frame pushed only AFTER a successful
				// StartChunk, both cases map to frames.back(). Marked here (the innermost success)
				// so the cache-miss recursion can't double-count sequential (nNumber==0) reads.
				if ( g_bWireAudit && bIsReading && !NWireAudit::frames.empty() &&
				     ( &src == &chunks.back() || ( chunks.size() >= 2 && &src == &*(++chunks.rbegin()) ) ) )
				{
					int &nSeen = NWireAudit::frames.back().consumed[dwID];
					if ( nNumber == 0 )
						++nSeen;
					else if ( nNumber > nSeen )
						nSeen = nNumber;
				}
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
			if ( g_bWireAudit && res.nLength != nSize )
				NWireAudit::Finding( "SIZE", idChunk, res.nLength, nSize );
			memcpy( pData, data.GetBuffer() + res.nStart, nSize );
		}
		else
		{
			if ( g_bWireAudit && nChunkNumber <= 1 )
				NWireAudit::Finding( "MISS", idChunk, -1, nSize );
			memset( pData, 0, nSize );
		}
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
		if ( g_bWireAudit && !NWireAudit::frames.empty() )
		{
			NWireAudit::frames.back().bRaw = true;
			if ( res.nLength != nSize )
				NWireAudit::Finding( "RAWSZ", NWireAudit::frames.back().idChunk, res.nLength, nSize );
		}
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
		if ( g_bWireAudit && !NWireAudit::frames.empty() )
			NWireAudit::frames.back().bRaw = true;
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
		if ( g_bWireAudit && !NWireAudit::frames.empty() )
			NWireAudit::frames.back().bRaw = true;
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
		storedObjects[pObject] = true; // it is important to assign something
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
		{
			chunks.pop_back();
			if ( g_bWireAudit && nChunkNumber <= 1 && !NWireAudit::frames.empty() )
				NWireAudit::Finding( "MISS", idChunk, -1, 0 );
		}
		else if ( g_bWireAudit )
			NWireAudit::frames.push_back( NWireAudit::SFrame( idChunk ) );
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
		// [HARNESS] wire audit: before closing this frame, report every subchunk the save carries
		// that the class never consumed (a retail field the dev table doesn't know about).
		if ( g_bWireAudit && !NWireAudit::frames.empty() )
		{
			CChunkLevel &cur = chunks.back();
			NWireAudit::SFrame &fr = NWireAudit::frames.back();
			// Frames consumed as RAW payload (object refs, blobs, strings) hold no subchunks --
			// walking their bytes as chunk headers would misparse (a null 4-byte ref reads as two
			// empty id-0 "chunks"). Only frames whose bytes walk as chunks END TO END are analyzed.
			if ( !fr.bRaw )
			{
				std::map<char, int> present, firstLen;
				int nPos = 0;
				CChunkLevel t;
				while ( ReadShortChunk( cur, nPos, t ) )
				{
					int &n = present[t.idChunk];
					if ( ++n == 1 )
						firstLen[t.idChunk] = t.nLength;
				}
				if ( nPos == cur.nLength )
				{
					for ( std::map<char, int>::const_iterator i = present.begin(); i != present.end(); ++i )
					{
						std::map<char, int>::const_iterator c = fr.consumed.find( i->first );
						int nCons = ( c == fr.consumed.end() ) ? 0 : c->second;
						if ( nCons < i->second )
							NWireAudit::Finding( "UNREAD", i->first, firstLen[i->first], i->second - nCons );
					}
				}
			}
			NWireAudit::frames.pop_back();
		}
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
	if ( g_bWireAudit && bRead )
	{
		NWireAudit::frames.clear();
		NWireAudit::frames.push_back( NWireAudit::SFrame( 0 ) );   // mirrors the root chunk level
		NWireAudit::nTypeId = 0;
	}
	if ( !bRead )
	{
		// The dev write path emits unpacked chunks and no pack marker (retail Start @0x3f2730
		// stamps nVersion=1 and Finish packs chunks 0/2/1 behind the "A3\0" marker chunk 3; the
		// content LAYOUT is identical either way -- v1 is just CNetCompressor packing). Reading
		// handles both; writing packed is not implemented, so do not stamp v1 or the marker.
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
		// retail pack marker (top-level chunk id 3, "A3\0"): its presence means the three payload
		// chunks (0 = object table, 2 = per-object data, 1 = main data) are CNetCompressor-packed.
		// Retail Start @0x3f2730 derives the flag exactly like this and hands it to every payload
		// GetShortChunkSave @0x3f1d80; retail-written (v1) saves always pack, dev-written ones don't.
		bool bPacked = false;
		{
			CMemoryStream packMarker;
			bPacked = GetShortChunkSave( res, 3, packMarker, nBaseSeek ) && packMarker.GetSize() > 0;
		}
		// read chunk with objects description
		GetShortChunkSave( res, 0, obj, nBaseSeek, bPacked );
		GetShortChunkSave( res, 2, data, nBaseSeek, bPacked );
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
			// [REGDIAG] a class id dev's registry can't build (CreateObject == null) must NOT enter the
			// invalidate dance below -- CPtr/CObj(null) -> ReleaseObj on a null `this` (Basic2.cpp:56)
			// crashes mid-deserialize. This only bites objects also marked bValid=false (rare) -> the
			// intermittent load crash. Log the missing id so a genuinely-needed class can be registered.
			if ( !pObject )
			{
				if ( g_bSaveLoadDiag )
					SaveLoadDiag( "[REGDIAG] CreateObject returned NULL for id=0x%08X (bValid=%d)\n", nTypeID, (int)bValid );
			}
			else if ( !bValid )
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
		int nCurObj = -1, nCurId = 0;   // diagnostic: track the top-level object being read
		try
		{
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
					if ( g_bSaveLoadDiag ) { nCurObj = i; nCurId = pSSClasses->GetObjectTypeID( pObject ); }
					if ( g_bWireAudit ) NWireAudit::nTypeId = pSSClasses->GetObjectTypeID( pObject );
					if ( StartChunk( 1, 1 ) )
					{
						(*pObject)&( *this );
						FinishChunk();
					}
				}
				FinishChunk();
			}
		}
		catch ( ... )
		{
			SaveLoadDiag( "THREW while reading object #%d/%d id=0x%08X\n", nCurObj, nCount, nCurId );
			throw;
		}
		// read main objects data
		chunks.back().Clear();
		GetShortChunkSave( res, 1, data, nBaseSeek, bPacked );
		chunks.back().nLength = data.GetSize();
		if ( g_bWireAudit )
			NWireAudit::Dump();
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
	// objects.clear() releases the temporary object-table refs; on the READ path that can cascade into
	// destroying objects that still hold a dangling smart-ptr to an already-freed sibling (resource/DG
	// layer). Flag the window so ReleaseObj/Ref skips a release whose target is no longer mapped instead
	// of faulting. Cost is one IsBadReadPtr per release, only during this teardown.
	const bool bWasReading = IsReading();
	if ( bWasReading )
		g_bSaveLoadTeardown = true;
	objects.clear();
	storedObjects.clear();
	toStore.clear();
	chunks.clear();
	if ( bWasReading )
		g_bSaveLoadTeardown = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
