#ifndef __GRESOURCE_H_
#define __GRESOURCE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "..\\FileIO\\FilesPackage.h"
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPartKey
{
	int nID, nPart;
	//
	SPartKey() {}
	SPartKey( int _nID, int _nPart ): nID(_nID), nPart(_nPart) {}
	bool operator==( const SPartKey &a ) const { return nID == a.nID && nPart == a.nPart; }
};
struct SPartHash
{
	int operator()( const SPartKey &k ) const { return k.nID ^ k.nPart; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TKey, class TValue>
class CResourceLoader: public CHoldedPtrFuncBase<TValue>
{
	TKey key;
protected:
	const TKey& GetKey() const { return key; }
public:
	CResourceLoader() {}
	void SetKey( const TKey &_key ) { key = _key; }
	int operator&( CStructureSaver &f ) { f.Add( 1, &key ); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFileRequest;
template <class TKey, class TValue>
class CLazyResourceLoader : public CResourceLoader<TKey,TValue>
{
	typedef CResourceLoader<TKey,TValue> TParent;
	CObj<CFileRequest> pRequest;
protected:
	virtual CFileRequest* CreateRequest() = 0;
	virtual void RecalcValue( CFileRequest *p ) = 0;
	virtual void Recalc()
	{
		if ( IsValid(pRequest) )
		{
			if ( !pRequest->IsReady() )
				return;
			RecalcValue( pRequest );
			pRequest = 0;
			ReleaseFileRequestHolder();
		}
		else
		{
			pRequest = CreateRequest();
			if ( pRequest )
				AddFileRequest( pRequest );
		}
	}
	bool NeedUpdate() { TParent::NeedUpdate(); if ( !IsValid(pValue) && IsValid(pRequest) && pRequest->IsReady() ) return true; return false; }
public:
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IPrecache : public CObjectBase
{
public:
	virtual void Update() {}
};
externA5 vector<CPtr<IPrecache> > precacheUpdateList;
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
class CResourcePrecache : public IPrecache
{
	OBJECT_NOCOPY_METHODS(CResourcePrecache);
	CDGPtr<T> pStuff;
	int operator&( CStructureSaver &f ) 
	{ 
		f.Add(2,&pStuff); 
		if ( f.IsReading() )
			precacheUpdateList.push_back( this );
		return 0; 
	}
public:
	CResourcePrecache( T *_pStuff = 0 ) : pStuff(_pStuff) { Update(); }
	~CResourcePrecache() { Update(); }
	virtual void Update() 
	{
		if ( IsValid(pStuff) ) 
		{ 
			pStuff.Refresh(); 
			pStuff->GetValue(); 
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IResource: public CObjectBase
{
public:
	virtual CDataStream* GetStream() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPackageResource: public IResource
{
	OBJECT_NOCOPY_METHODS( CPackageResource );
	CPackageStream f;
public:
	CPackageResource( IFilesPackage *pPack = 0, FILE_ID nFileID = 0 ) : f( pPack, nFileID ) {}
	virtual CDataStream* GetStream() { return &f; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFileResource: public IResource
{
	OBJECT_NOCOPY_METHODS( CFileResource );
	CFileStream f;
public:
	CFileResource() {}
	CFileResource( const char *pszResName, FILE_ID nFileID );
	virtual CDataStream* GetStream() { return &f; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// for use in CResourceOpener
class CResourceFileOpener
{
	CPtr<IResource> pf;
public:
	CResourceFileOpener( const char *pszResName, int nID );
	CResourceFileOpener( const char *pszResName, const SPartKey &key );
	static bool DoesExist( const char *pszResName, int nID );
	static bool DoesExist( const char *pszResName, const SPartKey &key );
	CDataStream* operator->() { return pf->GetStream(); }
	CDataStream* GetStream() { return pf->GetStream(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CResourceOpener
{
	CResourceFileOpener file;
	CStructureSaver saver;
public:
	CResourceOpener( const char *pszResName, int nID ): file( pszResName, nID ), saver( *file.GetStream(), CStructureSaver::READ ) {}
	CResourceOpener( const char *pszResName, const SPartKey &key ): file( pszResName, key ), saver( *file.GetStream(), CStructureSaver::READ ) {}
	CStructureSaver* operator->() { return &saver; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CResourceTracker
{
	typedef unordered_map<SPartKey, bool, SPartHash> CCheckHash;
	CCheckHash check;
	string szResourceName;

	CResourceTracker( const CResourceTracker &a ) {}
	void operator=( const CResourceTracker &a ) {}
public:
	CResourceTracker( const char *pszResourceName );
	~CResourceTracker();
	bool DoesExist( const SPartKey &k );
	bool DoesExist( int k ) { return DoesExist( SPartKey( k, 0 ) ); }
	void Clear() { check.clear(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// if for some package CFileRequest scheme is used all access to that resource should be
// through CFileRequest system
class CFileRequest : public CObjectBase
{
	OBJECT_NOCOPY_METHODS(CFileRequest);
	const char *pszResName;
	int nID;
	bool bIsReady;
	CMemoryStream data;
public:
	CFileRequest() : pszResName(0), nID(0), bIsReady(true) {}
	CFileRequest( const char *pszResName, int nID );
	CFileRequest( const char *pszResName, const SPartKey &key );
	CMemoryStream* operator->() { return &data;; }
	CMemoryStream* GetStream() { return &data; }
	void Read();
	bool IsReady() const { return bIsReady; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void AddResourceDir( const char *pszName );
void CloseAllResources();
void RunResourceLoadingThread();
void ReleaseFileRequestHolder();
void AddFileRequest( CFileRequest *pReq );
bool HasFileRequestsInFly();
void LoadPrecached();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif