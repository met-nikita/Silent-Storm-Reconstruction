#ifndef __Sync_H_
#define __Sync_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class CSyncSrc;
template<class T> class CSyncDst;
template<class T>
class CSyncSrcBind
{
	CSyncSrcBind( const CSyncSrcBind &a ) { ASSERT( 0 ); }
	CSyncSrcBind& operator=( const CSyncSrcBind &a ) { ASSERT( 0 ); return *this; }
	ZDATA
	CPtr<CSyncSrc<T> > pSync;
	int nID;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pSync); f.Add(3,&nID); return 0; }
	CSyncSrcBind() {}
	~CSyncSrcBind() { Unlink(); }
	void Link( CSyncSrc<T> *p, T *pObject ) { Unlink(); pSync = p; if ( IsValid( pSync ) ) nID = p->Add( pObject ); }
	void Unlink() { if ( IsValid( pSync ) ) pSync->Remove( nID ); pSync = 0; }
	void Update() { if ( IsValid( pSync ) ) pSync->Update( nID ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
class CSyncSrc: public CObjectBase
{
	struct SObject
	{
		ZDATA
		CPtr<T> pObject;
		int nNext, nPrev, nVersion;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pObject); f.Add(3,&nNext); f.Add(4,&nPrev); f.Add(5,&nVersion); return 0; }

		SObject(): nNext(0), nPrev(0) {}
		explicit SObject( T *_pObject ): pObject(_pObject) {}
	};
	ZDATA
	vector<int> freeIDs;
	vector<SObject> objects;
	int nVersion;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&freeIDs); f.Add(3,&objects); f.Add(4,&nVersion); return 0; }
private:
	bool Has( T *p ) { for ( int k = 0; k < objects.size(); ++k ) if ( objects[k].pObject == p ) return true; return false; }
	void EraseLink( int nID )
	{
		const SObject &o = objects[nID];
		objects[o.nPrev].nNext = o.nNext;
		objects[o.nNext].nPrev = o.nPrev;
	}
	void AddHead( int nID )
	{
		SObject &f = objects[0];
		SObject &o = objects[nID];
		o.nNext = f.nNext;
		o.nPrev = 0;
		objects[f.nNext].nPrev = nID;
		f.nNext = nID;
		o.nVersion = ++nVersion;
	}
	void MoveToHead( int nID )
	{
		EraseLink( nID );
		AddHead( nID );
	}
protected:
	int Add( T *p ) 
	{
		ASSERT( !Has(p) );
		ASSERT( p );
		int nID;
		if ( freeIDs.empty() )
		{
			nID = objects.size();
			objects.push_back( SObject(p) );
			AddHead( nID );
		}
		else
		{
			nID = freeIDs.back();
			freeIDs.pop_back();
			objects[nID].pObject = p;
			MoveToHead( nID );
		}
		return nID;
	}
	void Remove( int nID ) 
	{
		objects[nID].pObject = 0;
		freeIDs.push_back( nID );
		MoveToHead( nID );
	}
	void Update( int nID )
	{
		MoveToHead( nID );
	}
	virtual void Refresh() {}
public:
	CSyncSrc(): nVersion(1) { objects.resize(1); }
	friend class CSyncDst<T>;
	friend class CSyncSrcBind<T>;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
class CSyncDst
{
	ZDATA
	CObj<CSyncSrc<T> > pSource;
	int nVersion;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pSource); f.Add(3,&nVersion); return 0; }
protected:
	CSyncSrc<T>* GetSource() const { return pSource; }
	virtual void VisitObject( int nID, T *p ) = 0;
public:
	void Sync()
	{
		pSource->Refresh();
		const CSyncSrc<T> &src = *pSource;
		for ( int nID = src.objects[0].nNext; nID != 0; nID = src.objects[nID].nNext )
		{
			const CSyncSrc<T>::SObject &o = src.objects[nID];
			if ( o.nVersion <= nVersion )
				break;
			VisitObject( nID, o.pObject );
		}
		nVersion = src.nVersion;
	}
	CSyncDst( CSyncSrc<T> *_pSrc = 0 ): pSource(_pSrc), nVersion(-1) {}
	virtual void SetNewSource( CSyncSrc<T> *_pSrc ) { pSource = _pSrc; nVersion = -1; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T,class TFinal>
class COrdinarySyncDst: public CSyncDst<T>
{
	typedef vector<vector<CObj<CObjectBase> > > CObjVector;
	typedef CSyncDst<T> TSyncParent;
	vector<CObj<CObjectBase> > *pStuff;
	T *pCurrentObject;
	ZDATA_(TSyncParent)
	CObjVector view;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TSyncParent*)this); f.Add(2,&view); return 0; }
private:
	void VisitObject( int nID, T *pObject )
	{
		if ( view.size() < nID + 1 )
			view.resize( nID + 1 );
		view[nID].resize(0);
		if ( pObject )
		{
			pStuff = &view[nID];
			pStuff->resize(0);
			pCurrentObject = pObject;
			pObject->Visit( static_cast<TFinal*>(this) );
			PostVisit( nID, pObject );
			pStuff = 0;
			pCurrentObject = 0;
		}
	}
protected:
	virtual void PostVisit( int nID, T *pObject ) {}
	void RegisterBase( CObjectBase *p ) { ASSERT( pStuff ); pStuff->push_back( p ); }
	template<class TR>
		TR* Register( TR *p ) { RegisterBase( CastToObjectBase(p) ); return p; }
	T* GetCurrentSrcObject() const { return pCurrentObject; }
	const vector<CObj<CObjectBase> >& GetObjects( int nID ) const { return view[nID]; }
	const vector<CObj<CObjectBase> >& GetCurrentObjects() const { ASSERT(pStuff); return *pStuff; }
public:
	COrdinarySyncDst( CSyncSrc<T> *p = 0 ): CSyncDst<T>(p), pStuff(0) {}
	virtual void SetNewSource( CSyncSrc<T> *_pSrc ) { TSyncParent::SetNewSource(_pSrc); view.clear(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
class CSetSyncSrc: public CSyncSrc<T>
{
	OBJECT_BASIC_METHODS( CSetSyncSrc );
	typedef CSyncSrc<T> TParent;
	typedef unordered_map<CPtr<T>, int, SPtrHash> CStuffHash;
	ZDATA_(TParent)
	CStuffHash stuff;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TParent*)this); f.Add(2,&stuff); return 0; }
public:
	void Set( const vector<T*> &newSet )
	{
		CStuffHash t = stuff;
		for ( int k = 0; k < newSet.size(); ++k )
		{
			T *p = newSet[k];
			CStuffHash::iterator i = t.find( p );
			if ( i != t.end() )
				t.erase( i );
			else
				stuff[p] = Add( p );
		}
		for ( CStuffHash::iterator i = t.begin(); i != t.end(); ++i )
		{
			CStuffHash::iterator k = stuff.find( i->first );
			ASSERT( k != stuff.end() );
			stuff.erase( k );
			Remove( i->second );
		}
	}
	void Set( T *p ) 
	{
		CStuffHash::iterator k = stuff.find( i->first );
		if ( k == stuff.end() )
			stuff[p] = Add( p );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// utility class for CBoolSyncSrc<T>
template<class T, class TUplink>
class CBoolSyncDstUtil: public CSyncDst<T>
{
	typedef CSyncDst<T> TParent;
	ZDATA_(TParent)
	CPtr<TUplink> pRes; // uplink
	int nMask;
	vector<CPtr<T> > track;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TParent*)this); f.Add(2,&pRes); f.Add(3,&nMask); f.Add(4,&track); return 0; }
private:
	
	void VisitObject( int nID, T *pObject )
	{
		if ( track.size() < nID + 1 )
			track.resize( nID + 1 );
		if ( track[nID] != pObject )
		{
			if ( track[nID] )
				pRes->BoolSwitch( track[nID], nMask );
			if ( pObject )
				pRes->BoolSwitch( pObject, nMask );
			track[nID] = pObject;
		}
		else
		{
			if ( pObject )
				pRes->BoolUpdate( pObject );
		}
	}
public:
	CBoolSyncDstUtil() {}
	CBoolSyncDstUtil( CSyncSrc<T> *pSrc, TUplink *p, int _nMask ): CSyncDst<T>(pSrc), pRes(p), nMask(_nMask) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T, class TFunc>
class CBoolSyncSrc: public CSyncSrc<T>
{
	OBJECT_BASIC_METHODS( CBoolSyncSrc )
	typedef CBoolSyncSrc<T,TFunc> TThis;
	struct SObjectInfo
	{
		int nMask;
		int nTrackID;
	};
	typedef unordered_map<CPtr<T>, SObjectInfo, SPtrHash> CObjectsHash;
	typedef CSyncSrc<T> TParent;
	ZDATA_(TParent)
	CObjectsHash objects;
	CBoolSyncDstUtil<T,TThis> syncA, syncB;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TParent*)this); f.Add(2,&objects); f.Add(3,&syncA); f.Add(4,&syncB); return 0; }
	
	void BoolSwitch( T *pObject, int nMask )
	{
		CObjectsHash::iterator i = objects.find( pObject );
		if ( i == objects.end() )
		{
			SObjectInfo &r = objects[pObject];
			r.nMask = nMask;
			if ( TFunc::GetResult( r.nMask ) )
				r.nTrackID = Add( pObject );
			else
				r.nTrackID = -1;
		}
		else
		{
			i->second.nMask ^= nMask;
			if ( TFunc::GetResult( i->second.nMask ) )
			{
				if ( i->second.nTrackID == -1 )
					i->second.nTrackID = Add( pObject );
			}
			else
			{
				if ( i->second.nTrackID >= 0 )
				{
					Remove( i->second.nTrackID );
					i->second.nTrackID = -1;
				}
			}
			if ( i->second.nMask == 0 )
			{
				ASSERT( i->second.nTrackID == -1 );
				objects.erase( i );
			}
		}
	}
	void BoolUpdate( T *pObject )
	{
		CObjectsHash::iterator i = objects.find( pObject );
		ASSERT( i != objects.end() );
		if ( i->second.nTrackID >= 0 )
			Update( i->second.nTrackID );
	}
	virtual void Refresh()
	{
		syncA.Sync();
		syncB.Sync();
	}
public:
	CBoolSyncSrc() {}
	CBoolSyncSrc( CSyncSrc<T> *pA, CSyncSrc<T> *pB ): 
		syncA( pA, this, 1 ), 
		syncB( pB, this, 2 ) {}

	friend class CBoolSyncDstUtil<T,TThis>;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnionFunc
{
public:
	static bool GetResult( int nMask ) { return nMask != 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CIntersectionFunc
{
public:
	static bool GetResult( int nMask ) { return nMask == 3; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CBoolSyncSrc<IVisObj,CSubtractFunc> (BoolSwitch @0x2d2d70: `nMask == 1`): in source A (bit 1)
// and NOT in source B (bit 2). CRenderGame::UpdateVisible @0x2cee50 uses it in the no-viewer
// (show-all / cinematic) branch: GetUnits() MINUS the world's heard-marker set (GetAllSoundStuff).
class CSubtractFunc
{
public:
	static bool GetResult( int nMask ) { return nMask == 1; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
