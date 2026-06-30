#include "StdAfx.h"
#include "GResource.h"
#include "..\Misc\Win32Helper.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
vector<CPtr<IPrecache> > precacheUpdateList;
void LoadPrecached()
{
	for ( int k = 0; k < precacheUpdateList.size(); ++k )
	{
		IPrecache *p = precacheUpdateList[k];
		if ( IsValid(p) )
			p->Update();
	}
	precacheUpdateList.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static list<CResourceTracker*>& GetTrackers()
{
	static list<CResourceTracker*> trackers;
	return trackers;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static string szDir;
void AddResourceDir( const char *pszName )
{
	szDir = pszName;
	if ( !szDir.empty() && szDir[ szDir.length() - 1 ] != '\\' )
		szDir += "\\";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef unordered_map<string, CPtr<IFilesPackage> > CPackHash;
static CPackHash packages;
static NWin32Helper::CCriticalSection packageWork;
static IFilesPackage* GetPackage( const char *pszResName )
{
	CPackHash::iterator i = packages.find( pszResName );
	IFilesPackage *pRes;
	if ( i == packages.end() )
	{
		string szFullPath = szDir + pszResName + ".res";
		if ( strcmp( pszResName, "LRTextures" ) == 0 )
			pRes = OpenCachedFilesPackage( szFullPath.c_str() );
		else
			pRes = OpenFilesPackage( szFullPath.c_str() );
#ifndef _MAPEDIT
		ASSERT( pRes );
#endif
		packages[pszResName] = pRes;
		if ( !pRes )
			return 0;
	}
	else
		pRes = i->second;
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void WaitAllPendingLoad();
void CloseAllResources()
{
	WaitAllPendingLoad();
	NWin32Helper::CCriticalSectionLock l( packageWork );
	packages.clear();
	list<CResourceTracker*> &t = GetTrackers();
	for ( list<CResourceTracker*>::iterator i = t.begin(); i != t.end(); ++i )
		(*i)->Clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetID( const SPartKey &key )
{
	return key.nID + (key.nPart << 16);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline string GetFileResourceName( const char *pszResName, FILE_ID nFileID )
{
	// Prefix the resource dir (szDir, e.g. ".\res\") so a loose file resolves as
	// "<dir>\<ResName>\<id>" - matching the release's GetFileResourceName(dir,name,id).
	// The dev build had dropped the dir; it only ever mattered as a never-taken package-first
	// fallback, but loose-overrides-package (below) now needs the real on-disk path.
	char szBuf[1024];
	sprintf( szBuf, "%s%s\\%d", szDir.c_str(), pszResName, nFileID );
	return szBuf;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Does a LOOSE on-disk file exist for this resource? The release checks this BEFORE the package
// (loose overrides package) - that's how the shipped 1.2 patch's updated loose assets take effect.
inline bool DoesLooseFileExist( const char *pszResName, int nID )
{
	HANDLE h = CreateFile( GetFileResourceName( pszResName, nID ).c_str(),
		GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );
	if ( h == INVALID_HANDLE_VALUE )
		return false;
	CloseHandle( h );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFileResource
////////////////////////////////////////////////////////////////////////////////////////////////////
CFileResource::CFileResource( const char *pszResName, FILE_ID nFileID )
{
	f.OpenRead( GetFileResourceName( pszResName, nFileID ).c_str() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CResourceFileOpener
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool DoesPackageFileExist( const char *pszResName, int nID )
{
	IFilesPackage *pPack = GetPackage( pszResName );
	return DoesFileExist( pPack, nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool DoesPackageFileExist( const char *pszResName, const SPartKey &key )
{
	IFilesPackage *pPack = GetPackage( pszResName );
	return DoesFileExist( pPack, GetID( key ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void TypeReq( const char *pszResName, int nID )
{
	char szBuf[1024];
	sprintf( szBuf, "%s %x\n", pszResName, nID );
	OutputDebugString( szBuf );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CResourceFileOpener::CResourceFileOpener( const char *pszResName, int nID )
{
	//TypeReq( pszResName, nID );
	NWin32Helper::CCriticalSectionLock l( packageWork );
	// loose-overrides-package (matches release CResourceFileOpener)
	if ( DoesLooseFileExist( pszResName, nID ) )
		pf = new CFileResource( pszResName, nID );
	else if ( DoesPackageFileExist( pszResName, nID ) )
		pf = new CPackageResource( GetPackage( pszResName ), nID );
	else
		pf = new CFileResource( pszResName, nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CResourceFileOpener::CResourceFileOpener( const char *pszResName, const SPartKey &key )
{
	NWin32Helper::CCriticalSectionLock l( packageWork );
	const int nID = GetID( key );
	//TypeReq( pszResName, nID );
	// loose-overrides-package (matches release CResourceFileOpener)
	if ( DoesLooseFileExist( pszResName, nID ) )
		pf = new CFileResource( pszResName, nID );
	else if ( DoesPackageFileExist( pszResName, nID ) )
		pf = new CPackageResource( GetPackage( pszResName ), nID );
	else
		pf = new CFileResource( pszResName, nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CResourceFileOpener::DoesExist( const char *pszResName, int nID )
{
	NWin32Helper::CCriticalSectionLock l( packageWork );
	if ( DoesPackageFileExist( pszResName, nID ) )
		return true;
#ifdef _MAPEDIT
	HANDLE h = CreateFile( GetFileResourceName( pszResName, nID ).c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );
	CloseHandle( h );
	return h != INVALID_HANDLE_VALUE;
//	CFileStream file;
//	return file.TryOpenRead( GetFileResourceName( pszResName, nID ).c_str() );
#else
	return false;
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CResourceFileOpener::DoesExist( const char *pszResName, const SPartKey &key )
{
	NWin32Helper::CCriticalSectionLock l( packageWork );
	if ( DoesPackageFileExist( pszResName, key ) )
		return true;
#ifdef _MAPEDIT
	string szName = GetFileResourceName( pszResName, GetID( key ) ).c_str();
	HANDLE h = CreateFile( szName.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );
	CloseHandle( h );
	return h != INVALID_HANDLE_VALUE;
	//CFileStream file;
	//return file.TryOpenRead( GetFileResourceName( pszResName, GetID( key ) ).c_str() );
#else
	return false;
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CResourceTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
CResourceTracker::CResourceTracker( const char *pszResourceName ): szResourceName(pszResourceName)
{
	GetTrackers().push_back( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CResourceTracker::~CResourceTracker()
{
	GetTrackers().remove( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CResourceTracker::DoesExist( const SPartKey &k )
{
	bool bRes;
	CCheckHash::iterator i = check.find( k );
	if ( i == check.end() )
	{
		bRes = CResourceFileOpener::DoesExist( szResourceName.c_str(), k );
		check[k] = bRes;
	}
	else
		bRes = i->second;
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFileRequest
////////////////////////////////////////////////////////////////////////////////////////////////////
CFileRequest::CFileRequest( const char *_pszResName, int _nID ) 
	: pszResName(_pszResName), nID(_nID), bIsReady(false)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CFileRequest::CFileRequest( const char *_pszResName, const SPartKey &_key )
	: pszResName(_pszResName), nID( GetID(_key) ), bIsReady(false)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static NWin32Helper::CCriticalSection readResource;
static bool bIsFileReading = false;
void CFileRequest::Read()
{
	ASSERT(!bIsReady);
	if ( bIsReady )
		return;
	NWin32Helper::CCriticalSectionLock l( readResource );
	NWin32Helper::CCriticalSectionLock lp( packageWork );
	bIsFileReading = true;
	//OutputDebugString( "request " );
	//TypeReq( pszResName, nID );
	try
	{
		// loose-overrides-package (matches release: loose on-disk file wins over the .res package)
		if ( DoesLooseFileExist( pszResName, nID ) )
		{
			CFileStream f;
			f.OpenRead( GetFileResourceName( pszResName, nID ).c_str() );
			f.ReadTo( data, f.GetSize() );
		}
		else if ( DoesPackageFileExist( pszResName, nID ) )
		{
			CPackageStream f( GetPackage( pszResName ), nID );
			f.ReadTo( data, f.GetSize() );
		}
		data.Seek(0);
	}
	catch (...) 
	{
	}
	bIsFileReading = false;
	bIsReady = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Resource loading thread
////////////////////////////////////////////////////////////////////////////////////////////////////
static NWin32Helper::CCriticalSection reqQueue, pendingCheck;
static NWin32Helper::CEvent newRequest;
static HANDLE hLoaderThread;
static list<CPtr<CFileRequest> > holdRequests;
static list<CFileRequest*> requests;
////////////////////////////////////////////////////////////////////////////////////////////////////
static void WaitAllPendingLoad()
{
	for(;;)
	{
		{
			NWin32Helper::CCriticalSectionLock lp( pendingCheck );
			NWin32Helper::CCriticalSectionLock l( reqQueue );
			if ( requests.empty() )
				return;
		}
		Sleep(0);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static DWORD WINAPI LoaderThread( void* )
{
	for (;;)
	{
		newRequest.Wait();
		newRequest.Reset();
		// process new requests
		CFileRequest *pRes;
		for(;;)
		{
			NWin32Helper::CCriticalSectionLock lp( pendingCheck );
			{
				NWin32Helper::CCriticalSectionLock l( reqQueue );
				if ( requests.empty() )
					break;
				pRes = requests.front();
				requests.pop_front();
			}
			if ( pRes == 0 )
				return 0;
			if ( !IsValid(pRes) )
				continue;
			pRes->Read();
		}
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void AddFileRequest( CFileRequest *pReq )
{
	if ( pReq->IsReady() )
		return;
	//pReq->Read();
	//return;
	NWin32Helper::CCriticalSectionLock l( reqQueue );
	holdRequests.push_front( pReq );
	requests.push_front( pReq );
	newRequest.Set();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool HasFileRequestsInFly()
{
	NWin32Helper::CCriticalSectionLock l( reqQueue );
	return bIsFileReading || !requests.empty();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ReleaseFileRequestHolder()
{
	NWin32Helper::CCriticalSectionLock l( reqQueue );
	if ( requests.empty() )
		holdRequests.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void RunResourceLoadingThread()
{
	DWORD dwThread;
	hLoaderThread = CreateThread( 0, 102400, LoaderThread, 0, 0, &dwThread );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SKillLoaderThread
{
	~SKillLoaderThread()
	{
		if ( hLoaderThread )
		{
			{
				NWin32Helper::CCriticalSectionLock l( reqQueue );
				newRequest.Set();
				requests.push_front( 0 );
			}
			WaitForSingleObject( hLoaderThread, INFINITE );
		}
	}
} killLoaderThread;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
