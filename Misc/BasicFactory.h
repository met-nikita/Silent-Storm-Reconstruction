#ifndef __BASICFACTORY_H_
#define __BASICFACTORY_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SDefaultPtrHash
{
	int operator()( const void *pData ) const { return (int)pData; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// factory is using RTTI
// objects should inherit T and T must have at least 1 virtual function
template <class T>
class CClassFactory
{
public:
	typedef const type_info *VFT;
private:
	typedef T* (*newFunc)();
	typedef std::unordered_map<int, newFunc> CTypeNewHash;                // typeID->newFunc()
	typedef std::unordered_map<VFT, int, SDefaultPtrHash> CTypeIndexHash; // vftable->typeID

	CTypeIndexHash typeIndex;
	CTypeNewHash typeInfo;

	void RegisterTypeBase( int nTypeID, newFunc func, VFT vft );
	static VFT GetObjectType( T *pObject ) { return &typeid(*pObject); }
	int VFT2TypeID( VFT t )
	{
		CTypeIndexHash::const_iterator i = typeIndex.find( t );
		if ( i != typeIndex.end() )
			return i->second;
		for ( i = typeIndex.begin(); i != typeIndex.end(); ++i )
		{
			if ( *i->first == *t )
			{
				typeIndex[t] = i->second;
				return i->second;
			}
		}
		return -1;
	}
public:
	template < class TT >
		void RegisterType( int nTypeID, newFunc func, TT* ) { RegisterTypeBase( nTypeID, func, &typeid(TT) ); }
	void RegisterTypeSafe( int nTypeID, newFunc func ) 
	{
		CPtr<T> pObj = func();
		VFT vft = GetObjectType( pObj );
		RegisterTypeBase( nTypeID, func, vft ); 
	}
	T* CreateObject( int nTypeID ) { newFunc f = typeInfo[ nTypeID ]; if ( f ) return f(); return 0; }
	int GetObjectTypeID( T *pObject ) { return VFT2TypeID( GetObjectType( pObject ) ); }
	template<class TT>
		int GetTypeID( TT *p = 0 ) { return VFT2TypeID( &typeid(TT) ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
void CClassFactory<T>::RegisterTypeBase( int nTypeID, newFunc func, VFT vft )
{
	ASSERT( typeInfo.find( nTypeID ) == typeInfo.end() );
	ASSERT( typeIndex.find( vft ) == typeIndex.end() );
	typeIndex[ vft ] = nTypeID;
	typeInfo[ nTypeID ] = func;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// macro for registering CFundament derivatives
#define REGISTER_CLASS( factory, N, name ) factory.RegisterType( N, name##::New##name, (name*)0 );
#define REGISTER_TEMPL_CLASS( factory, N, name, className ) factory.RegisterType( N, name##::New##className, (name*)0 );
#define REGISTER_CLASS_NM( factory, N, name, nmspace ) factory.RegisterType( N, nmspace::name##::New##name, (nmspace::name*)0 );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __BASICFACTORY_H_
