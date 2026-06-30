#ifndef __BASICSHARE_H_
#define __BASICSHARE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBasicShareBase
{
	int nID;
	CBasicShareBase *pNext;
protected:
	int GetID() { return nID; }
	virtual void CreateHolder( list<CObj<CObjectBase> > *pHolder ) = 0;
public:
	CBasicShareBase( int _nID );
	virtual int operator&( CStructureSaver &f ) = 0;
	//
	friend void SerializeShared( CStructureSaver *pFile );
	friend class CSharedHolder;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TKey, class TValue, class THash = hash<TKey> >
class CBasicShare: public CBasicShareBase
{
	bool bKeepData;
	typedef unordered_map< TKey, CPtr<TValue>, THash > CDataHash;
	CDataHash data;
	//
	virtual void CreateHolder( list<CObj<CObjectBase> > *pHolder )
	{
		for ( CDataHash::const_iterator i = data.begin(); i != data.end(); ++i )
			pHolder->push_back( i->second.GetPtr() );
	}
protected:
	virtual TValue* Create( const TKey &key ) { TValue *pRes = new TValue; pRes->SetKey(key); return pRes; }
public:
	CBasicShare( int nID, bool _bKeepData = true ): CBasicShareBase( nID ), bKeepData(_bKeepData) {}
	TValue* Get( const TKey &key )
	{
		CDataHash::iterator i = data.find( key );
		if ( i == data.end() )
		{
			TValue *pRes = Create( key );
			data[key] = pRes;
			return pRes;
		}
		if ( !IsValid( i->second ) )
			i->second = Create( key );
		return i->second;
	}
	int operator&( CStructureSaver &f )
	{ 
		if ( f.IsReading() && bKeepData )
		{
			CDataHash keeper( data );
			f.Add( GetID(), &data ); 
			for ( CDataHash::const_iterator i = keeper.begin(); i != keeper.end(); ++i )
			{
				CDataHash::iterator r = data.find( i->first );
				if ( r != data.end() && IsValid( i->second ) )
					*r->second = *i->second;
			}
		}
		else
			f.Add( GetID(), &data ); 
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSharedHolder
{
	list<CObj<CObjectBase> > objs;
public:
	CSharedHolder();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void SerializeShared( CStructureSaver *pFile );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif