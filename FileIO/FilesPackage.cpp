#include "StdAfx.h"
#include "FilesPackage.h"
#include "BasicChunk1.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
const DWORD DW_PACKAGE_SIGNATURE = 0x95938921;
// The release/Steam .res packages bump the signature by +0x01010101 (a per-byte format-version
// marker; the per-resource blobs inside are CStructureSaver v1, handled by CStructureSaver::nVersion).
// The header index itself is unchanged (v0 map<FILE_ID,SFileInfo>), so we just accept the bumped sig.
const DWORD DW_PACKAGE_SIGNATURE_V1 = 0x96948A22;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFilesPackage
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFileInfo
{
	unsigned int nStart;
	unsigned int nLength;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFileHash
{
	int operator()( const FILE_ID &a ) const { return hash<int>()( (int)a ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IFilesPackage : public CObjectBase
{
protected:
	typedef unordered_map< FILE_ID, SFileInfo, SFileHash > CFileInfoHash;
	typedef unordered_map< FILE_ID, FILETIME, SFileHash > CFileTimeHash;
	CFileInfoHash files;
public:
	virtual void Read( unsigned int nPos, void *pDest, unsigned int nSize ) = 0;
	virtual void Write( unsigned int nPos, const void *pSrc, unsigned int nSize ) = 0;
	SFileInfo* GetFileInfo( FILE_ID nFileID )
	{
		CFileInfoHash::iterator it = files.find( nFileID );
		if ( it == files.end() )
			return 0;
		return &it->second;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TStream>
class TFilesPackage : public IFilesPackage
{
protected:
	TStream f;
	//
	void ReadHeader( CMemoryStream *pHeader, int *pnPos )
	{
		int nSignature, nPos;
		f.Read( &nSignature, 4 );
		if ( nSignature != DW_PACKAGE_SIGNATURE && nSignature != DW_PACKAGE_SIGNATURE_V1 )
			throw SFileIOError( "wrong signature" );
		f.Read( &nPos, 4 );
		if ( pnPos )
			*pnPos = nPos;
		f.Seek( nPos );
		pHeader->SetSizeDiscard( f.GetSize() - nPos );
		f.Read( pHeader->GetBufferForWrite(), pHeader->GetSize() );
		pHeader->Seek(0);
	}
	void WriteHeader( CMemoryStream *pHeader, int nPos )
	{
		f.Seek( 4 );
		f.Write( &nPos, 4 );
		f.Seek( nPos );
		f.Write( pHeader->GetBuffer(), pHeader->GetSize() );
	}
	void LoadHeaderInfo()
	{
		CMemoryStream info;
		ReadHeader( &info, 0 );
		CStructureSaver ss( info, CStructureSaver::READ );
		ss.Add( 1, &files );
	}
public:
	virtual void Read( unsigned int nPos, void *pDest, unsigned int nSize ) { f.Seek( nPos ); f.Read( pDest, nSize ); }
	virtual void Write( unsigned int nPos, const void *pSrc, unsigned int nSize ) { f.Seek( nPos ); f.Write( pSrc, nSize ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFilesPackage : public TFilesPackage<CFileStream>
{
	OBJECT_NOCOPY_METHODS( CFilesPackage );
public:
	bool Open( const char *pszFileName );
	bool Update( CDataStream *pErr, const char *pszFileName, const char *pszDir );
	bool RescanDir( CDataStream *pErr, const char *pszDir, CFileTimeHash *pDates, int *pnStartPos );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFilesPackage::Open( const char *pszFileName ) 
{
	try
	{
		f.OpenRead( pszFileName );
		LoadHeaderInfo();
	}
	catch(...)
	{
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsNumber( const char *pszFileName )
{
	char szBuf[16];
	int n = atoi( pszFileName );
	itoa( n, szBuf, 10 );
	return strcmp( szBuf, pszFileName ) == 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static FILE_ID GetFileID( const char *pszFileName )
{
	return atoi( pszFileName );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool operator==( const _FILETIME &a, const _FILETIME & b )
{
	return memcmp( &a, &b, sizeof(_FILETIME) ) == 0;
}
CDataStream& operator << ( CDataStream &a, const char *pszSmth )
{
	a.Write( pszSmth, strlen( pszSmth ) );
	return a;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFilesPackage::RescanDir( CDataStream *pErr, const char *pszDir, 
															CFileTimeHash *pDates, int *pnStartPos )
{
	bool bChanged = false;
	CDataStream &err = *pErr;
	WIN32_FIND_DATA ff;
	HANDLE hf = FindFirstFile( (string(pszDir) + "\\*.*").c_str(), &ff );
	unordered_map<FILE_ID, bool, SFileHash> foundFiles;
	if ( hf != INVALID_HANDLE_VALUE )
	{
		for(;;)
		{
			if ( IsNumber( ff.cFileName ) )
			{
				bool bAdd = true;
				FILE_ID nFileID = atoi( ff.cFileName );
				foundFiles[nFileID];
				SFileInfo *pInfo = GetFileInfo( nFileID );
				if ( pInfo )
				{
					CFileTimeHash::iterator k = pDates->find( nFileID );
					if ( k != pDates->end() && k->second == ff.ftLastWriteTime )
						bAdd = false;
				}
				if ( bAdd )
				{
					CMemoryStream file;
					CFileStream src;
					if ( !src.TryOpenRead( ( string(pszDir) + "\\" + ff.cFileName ).c_str() ) )
					{
						err << "Can not open " << ff.cFileName << "\n";
					}
					else
					{
						bool bReadOk = true;
						try
						{
							file.WriteFrom( src );
						}
						catch(...)
						{
							bReadOk = false;
							err << "Failed to read " << ff.cFileName << "\n";
						}
						if ( bReadOk )
						{
							bChanged = true;
							if ( pInfo && pInfo->nLength >= ff.nFileSizeLow )
							{
								f.Seek( pInfo->nStart );
								f.Write( file.GetBuffer(), file.GetSize() );
								SFileInfo &fileInfo = files[nFileID];
								fileInfo.nLength = file.GetSize();
								(*pDates)[nFileID] = ff.ftLastWriteTime;
							}
							else
							{
								f.Seek( *pnStartPos );
								f.Write( file.GetBuffer(), file.GetSize() );
								SFileInfo &fileInfo = files[nFileID];
								fileInfo.nStart = *pnStartPos;
								fileInfo.nLength = file.GetSize();
								(*pDates)[nFileID] = ff.ftLastWriteTime;
								*pnStartPos += file.GetSize();
							}
						}
					}
				}
			}
			else
			{
				if ( ff.cFileName[0] != '.' )
					err << "ignoring " << ff.cFileName << "\n";
			}
			if ( !FindNextFile( hf, &ff ) )
				break;
		}
		FindClose( hf );
	}
	// remove non existing files
	for ( CFileInfoHash::iterator i = files.begin(); i != files.end(); )
	{
		if ( foundFiles.find( i->first ) == foundFiles.end() )
		{
			bChanged = true;
			CFileTimeHash::iterator k = pDates->find( i->first );
			if ( k != pDates->end() )
				pDates->erase( k );
			CFileInfoHash::iterator idel = i++;
			files.erase( idel );
		}
		else
			++i;
	}
	return bChanged;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFilesPackage::Update( CDataStream *pErr, const char *pszFileName, const char *pszDir )
{
	CDataStream &err = *pErr;
	try
	{
		int nStartPos = 8;
		CMemoryStream infoFile;
		CFileTimeHash dates;
		// read info on them from .pkg
		if ( !f.TryOpen( pszFileName ) )
		{
			if ( !f.TryOpenWrite( pszFileName ) )
			{
				err << "Can not open " << pszFileName << "\n";
				ASSERT(0);
				return false;
			}
			f.CloseFile();
			if ( !f.TryOpen( pszFileName ) )
			{
				err << "Can not open created file " << pszFileName << "\n";
				ASSERT(0);
				return false;
			}
			unsigned int nSignature = DW_PACKAGE_SIGNATURE;
			f.Write( &nSignature, 4 );
		}
		else
		{
			ReadHeader( &infoFile, &nStartPos );
			CStructureSaver ss( infoFile, CStructureSaver::READ );
			ss.Add( 1, &files );
			ss.Add( 2, &dates );
		}
		bool bUpdated = RescanDir( pErr, pszDir, &dates, &nStartPos );
		if ( bUpdated )
		{
			// write new header
			infoFile.SetSize(0);
			{
				CStructureSaver ss( infoFile, CStructureSaver::WRITE );
				ss.Add( 1, &files );
				ss.Add( 2, &dates );
			}
			WriteHeader( &infoFile, nStartPos );
		}
	}
	catch(...)
	{
		err << "Failed to write into " << pszFileName << ", package is corrupted" << "\n";
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCachedFilesPackage : public TFilesPackage<CMemoryStream>
{
	OBJECT_NOCOPY_METHODS( CCachedFilesPackage );
public:
	bool Open( const char *pszFileName );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCachedFilesPackage::Open( const char *pszFileName )
{
	try
	{
		CFileStream fs;
		fs.OpenRead( pszFileName );
		f.WriteFrom( fs );
		f.Seek(0);
		LoadHeaderInfo();
	}
	catch(...)
	{
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPackageStream
////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned int CPackageStream::DoRead( unsigned int nPos, void *pDest, unsigned int nSize )
{
	pPack->Read( nPos + nOffset, pDest, nSize );
	return nSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned int CPackageStream::DoWrite( unsigned int nPos, const void *pSrc, unsigned int nSize )
{
	ASSERT(0);
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CPackageStream::CPackageStream( IFilesPackage *_pPack, FILE_ID nFileID ): pPack(_pPack)
{
	ASSERT( IsValid( pPack ) );
	if ( !IsValid( pPack ) )
		throw SFileIOError( "file not found in package" );
	nFlags = F_CanRead;
	SFileInfo *pInfo = pPack->GetFileInfo( nFileID );
	if ( pInfo )
	{
		nOffset = pInfo->nStart;
		StartAccess( pInfo->nLength, 1024 );
	}
	else
	{
		throw SFileIOError( "file not found in package" ); //SetFailed();
		nOffset = 0x7fffffff;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
IFilesPackage* OpenFilesPackage( const char *pszFileName )
{
	CFilesPackage *pRes = new CFilesPackage;
	if ( !pRes->Open( pszFileName ) )
	{
		CPtr<CFilesPackage> p = pRes;
		p = 0;
//		ASSERT(0); // throw?
		return 0;
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IFilesPackage* OpenCachedFilesPackage( const char *pszFileName )
{
	CPtr<CCachedFilesPackage> pRes = new CCachedFilesPackage;
	if ( !pRes->Open( pszFileName ) )
		return 0;
	return pRes.Extract();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool DoesFileExist( IFilesPackage *pPack, FILE_ID nFileID )
{
	if ( !pPack )
		return 0;
	return pPack->GetFileInfo( nFileID ) != 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool UpdateFilesPackage( CDataStream *pErr, const char *pszFileName, const char *pszDir )
{
	CPtr<CFilesPackage> pRes = new CFilesPackage;
	return pRes->Update( pErr, pszFileName, pszDir );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS( IFilesPackage )
