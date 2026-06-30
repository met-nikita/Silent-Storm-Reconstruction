#ifndef __SET_H_
#define __SET_H_
//
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSet
////////////////////////////////////////////////////////////////////////////////////////////////////
template< class T >
class CSet
{
	ZDATA
	vector< CPtr<T> > members;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&members); return 0; }
	//
	CSet() {}
	//
	int GetSize() const
	{
		return members.size();
	}
	bool IsContain( T *pMember ) const
	{ 
		return find( members.begin(), members.end(), pMember ) != members.end();
	}
	void Add( T *pMember )
	{
		ASSERT( IsValid( pMember ) );
		ASSERT( !IsContain( pMember ) );
		if ( !IsValid( pMember ) || IsContain( pMember ) )
			return;
		//
		members.push_back( pMember );
	}
	void Remove( T *pMember )
	{
		ASSERT( IsValid( pMember ) );
		ASSERT( IsContain( pMember ) );
		if ( !IsValid( pMember ) || !IsContain( pMember ) )
			return;
		//
		members.erase( find( members.begin(), members.end(), pMember ) );
	}
	T *operator[]( int n ) const
	{
		ASSERT( n >= 0 && n < GetSize() );
		if (  n < 0 || n >= GetSize()  )
			return 0;
		//
		return members[ n ];
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __SET_H_