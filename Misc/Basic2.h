#ifndef __BASIC2_H_
#define __BASIC2_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStructureSaver;
////////////////////////////////////////////////////////////////////////////////////////////////////
// single thread version
// base classes for the object reference counting system to simplify memory
// management
// There are different pointer templates - simple pointers - CPtr with added reference and
// pointers with ownership - CObj/CMObj. After a CObj is destroyed, the object it
// referenced is cleared and marked as invalid. CMObj operates similarly, organizing
// concurrent object ownership (used when an object can be destroyed due to
// external circumstances)
// Limitations - sometimes it may be necessary to use BASIC_REGISTER_CLASS() in the .cpp file
// - when using pointers to forward-declared classes
// You cannot override operator new, as objects will be deleted using the standard
// delete operator (due to delete this)
////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma warning(disable:4250)
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectBase
{
private:
#if defined(_DEBUG) && !defined(FAST_DEBUG)
	void AddRef() { if ( nRefData == 0 ) strncpy( szObjectName, GetTypeName(), 12 ); ++nRefData; }
	void AddObj( int nRef ) { if ( nObjData == 0 ) strncpy( szObjectName, GetTypeName(), 12 ); nObjData += nRef; }
#else
	void AddRef() { ++nRefData; }
	void AddObj( int nRef ) { nObjData += nRef; }
#endif
	void DecRef() { --nRefData; }
	void DecObj( int nRef ) { nObjData -= nRef; }
	void ReleaseRef();
	void ReleaseObj( int nRef, int nMask );
	static void DestroyDelayed();
protected:
	const char* GetTypeName();
#if defined(_DEBUG) && !defined(FAST_DEBUG)
	char szObjectName[12];
#endif
	int nObjData;
	int nRefData;
	// function should clear contents of object, easy to implement via consequent calls to
	// destructor and constructor, this function should not be called directly, use Clear()
	virtual void DestroyContents() = 0;
	virtual CObjectBase* MakeCopy() const { ASSERT( 0 ); return 0; }
	virtual ~CObjectBase() {}
public:
	CObjectBase(): nObjData(0), nRefData(0) {}
	// do not copy refcount when copy object
	CObjectBase( const CObjectBase &a ): nObjData(0), nRefData(0) {}
	CObjectBase& operator=( const CObjectBase &a ) { return *this; }
	//
	int IsRefInvalid() const { return (nObjData & 0x80000000); }
	// reset data in class to default values, saves RefCount from destruction
	void Clear() { AddRef(); DestroyContents(); DecRef(); }
	// for serialization purposes
	virtual int operator&( CStructureSaver &f ) { return 0; }
	//
	// due to absense of template friend classes in vc these classes are needed
	struct SRefO
	{
		void AddRef( CObjectBase *pObj ) {  pObj->AddObj( 1 ); }
		void DecRef( CObjectBase *pObj ) {  pObj->DecObj( 1 ); }
		void Release( CObjectBase *pObj ) { pObj->ReleaseObj( 1, 0x000fffff ); }
	};
	struct SRefM
	{
		void AddRef( CObjectBase *pObj ) {  pObj->AddObj( 0x100000 ); }
		void DecRef( CObjectBase *pObj ) {  pObj->DecObj( 0x100000 ); }
		void Release( CObjectBase *pObj ) { pObj->ReleaseObj( 0x100000,0x7ff00000 ); }
	};
	struct SRef
	{
		void AddRef( CObjectBase *pObj ) {  pObj->AddRef(); }
		void DecRef( CObjectBase *pObj ) {  pObj->DecRef(); }
		void Release( CObjectBase *pObj ) { pObj->ReleaseRef(); }
	};
	friend struct CObjectBase::SRef;
	friend struct CObjectBase::SRefO;
	friend struct CObjectBase::SRefM;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// macro that helps to make CFundament derivative to be serializable
// and makes sure that class will not be copied or destroyed via obj references
////////////////////////////////////////////////////////////////////////////////////////////////////
// macro that helps to create neccessary members for proper operation of refcount system
// if class needs special destructor, use CFundament
#define OBJECT_BASIC_METHODS(classname)                                              \
	public:                                                                            \
		static CObjectBase* New##classname() { return new classname(); }                 \
		classname* Duplicate() const { return dynamic_cast<classname*>(MakeCopy()); }    \
protected:                                                                           \
		CObjectBase* MakeCopy() const { return new classname(*this); }                   \
		virtual void DestroyContents() { classname::~classname(); int nHoldRefs = nRefData, nHoldObjs = nObjData; ::new(this) classname; nRefData += nHoldRefs; nObjData += nHoldObjs; }\
		virtual ~classname() {}                                                          \
	private:
#define OBJECT_NOCOPY_METHODS(classname)                                             \
	public:                                                                            \
		static CObjectBase* New##classname() { return new classname(); }                 \
	protected:                                                                         \
		virtual void DestroyContents() { classname::~classname(); int nHoldRefs = nRefData, nHoldObjs = nObjData; ::new(this) classname; nRefData += nHoldRefs; nObjData += nHoldObjs; }\
	private:
#define BASIC_REGISTER_CLASS(classname) \
template<> CObjectBase* CastToObjectBaseImpl<classname >( classname *p, void* ) { return p; }  \
template<> classname* CastToUserObjectImpl<classname >( CObjectBase *p, classname*, void* ) { return dynamic_cast<classname*>( p ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TUserObj> CObjectBase* CastToObjectBaseImpl( TUserObj *p, void* );
template<class TUserObj> CObjectBase* CastToObjectBaseImpl( TUserObj *p, CObjectBase* ) { return p; }
template<class TUserObj> TUserObj* CastToUserObjectImpl( CObjectBase *p, TUserObj*, void * );
template<class TUserObj> TUserObj* CastToUserObjectImpl( CObjectBase *p, TUserObj*, CObjectBase* ) { return dynamic_cast<TUserObj*>( p ); }
template<class TUserObj> inline CObjectBase* CastToObjectBase( TUserObj *p ) { return CastToObjectBaseImpl( p, p ); }
template<class TUserObj> inline TUserObj* CastToUserObject( CObjectBase *p, TUserObj *pu ) { return CastToUserObjectImpl( p, pu, pu ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
// TObject - base object for reference counting, TUserObj - user object name
// TRef - struct with AddRef/DecRef/Release methods for refcounting to use
template< class TUserObj, class TRef>
class CPtrBase
{
private:
	TUserObj *ptr;
	//
	inline void AddRef( TUserObj *_ptr ) { TRef p; if ( _ptr ) p.AddRef( CastToObjectBase(_ptr) ); }
	inline void DecRef( TUserObj *_ptr ) { TRef p; if ( _ptr ) p.DecRef( CastToObjectBase(_ptr) ); }
	inline void Release( TUserObj *_ptr ) { TRef p; if ( _ptr ) p.Release( CastToObjectBase(_ptr) ); }
protected:
	inline void SetObject( TUserObj *_ptr ) { TUserObj *pOld = ptr; ptr = _ptr; AddRef( ptr ); Release( pOld ); }
	inline TUserObj* Get() const { return ptr; }
public:
	inline CPtrBase(): ptr( 0 ) {}
	inline CPtrBase( TUserObj *_ptr ): ptr( _ptr ) { AddRef( ptr ); }
	inline CPtrBase( const CPtrBase &a ): ptr( a.ptr ) { AddRef( ptr ); }
	inline ~CPtrBase() { Release( ptr ); }
	//
	inline void Set( TUserObj *_ptr ) { SetObject( _ptr ); }
	inline TUserObj* Extract() { TUserObj *pRes = ptr; DecRef(ptr); ptr = 0; return pRes; }
	//
	// assignment operators
	inline CPtrBase& operator=( TUserObj *_ptr ) { Set( _ptr ); return *this; }
	inline CPtrBase& operator=( const CPtrBase &a ) { Set( a.ptr ); return *this; }
	// access
	inline TUserObj* operator->() const { return ptr; }
	inline operator TUserObj*() const { return ptr; }
	inline TUserObj* GetPtr() const { return ptr; }
	inline CObjectBase* GetBarePtr() const { return CastToObjectBase(ptr); }
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> inline bool IsValid( T *p ) { return p != 0 && !CastToObjectBase(p)->IsRefInvalid(); }
template<class T, class TRef> inline bool IsValid( const CPtrBase< T, TRef > &p ) { return p.GetPtr() && !p.GetBarePtr()->IsRefInvalid(); }
////////////////////////////////////////////////////////////////////////////////////////////////////
#define BASIC_PTR_DECLARE( TPtrName, TRef )                                                 \
template<class T>                                                                           \
class TPtrName: public CPtrBase< T, TRef >                                                  \
{                                                                                           \
	typedef CPtrBase< T, TRef > CBase;                                                        \
public:                                                                                     \
	typedef T CDestType;                                                                      \
	inline TPtrName() {}                                                                      \
	inline TPtrName( T *_ptr ): CBase( _ptr ) {}                                              \
	inline TPtrName( const TPtrName &a ): CBase( a ) {}                                       \
	inline TPtrName& operator=( T *_ptr ) { Set( _ptr ); return *this; }                      \
	inline TPtrName& operator=( const TPtrName &a ) { SetObject( a.Get() ); return *this; }   \
	inline bool operator< ( const TPtrName &a ) const { return Get() < a.Get(); }             \
	inline bool operator> ( const TPtrName &a ) const { return Get() > a.Get(); }             \
	inline bool operator<=( const TPtrName &a ) const { return Get() <= a.Get(); }            \
	inline bool operator>=( const TPtrName &a ) const { return Get() >= a.Get(); }            \
	inline int operator&( CStructureSaver &f ) { return (*(CBase*)this) & (f); }              \
};
#ifdef STUPID_VISUAL_ASSIST
template<class T> class CPtr {};
template<class T> class CObj {};
template<class T> class CMObj {};
#endif
//
BASIC_PTR_DECLARE( CPtr, CObjectBase::SRef );
BASIC_PTR_DECLARE( CObj, CObjectBase::SRefO );
BASIC_PTR_DECLARE( CMObj, CObjectBase::SRefM );
// misuse guard
template<class T> inline bool IsValid( CObj<T> *p ) { return p->YouHaveMadeMistake(); }
template<class T> inline bool IsValid( CPtr<T> *p ) { return p->YouHaveMadeMistake(); }
template<class T> inline bool IsValid( CMObj<T> *p ) { return p->YouHaveMadeMistake(); }
////////////////////////////////////////////////////////////////////////////////////////////////////
// functor for STL tests
struct SPtrTest
{
	CObjectBase *pTest;
	SPtrTest( CObjectBase *_pTest ): pTest(_pTest) {}
	template <class T,class T1> 
		inline bool operator()( const CPtrBase<T,T1> &a ) const { return a == pTest; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPtrHash
{
	template <class T,class T1> 
		inline int operator()( const CPtrBase<T,T1> &a ) const { return (int)a.GetBarePtr(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// walks container of pointers and erases references on invalid entries
template<class TContainer>
inline bool EraseInvalidRefs( TContainer *pData )
{
	bool bRes = false;
	for ( TContainer::iterator i = pData->begin(); i != pData->end(); )
	{
		if ( IsValid( *i ) )
			++i;
		else
		{
			i = pData->erase( i );
			bRes = true;
		}
	}
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// class for convinient handling of framework`s contexts
class CFWContext
{
	void **pFrameworkPtr;
public:
	template<class T>
		CFWContext( T **pF, T *pData ): pFrameworkPtr((void**)pF) { *pF = pData; }
	~CFWContext() { *pFrameworkPtr = 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// strips a leading const off a source type so const pointers can take the offset-correct
// direct-dynamic_cast path below (used only by CDynamicCast)
template<class TT> struct SDynCastStripConst           { typedef TT Type; };
template<class TT> struct SDynCastStripConst<const TT> { typedef TT Type; };
////////////////////////////////////////////////////////////////////////////////////////////////////
// Engine-wide cross/down-cast helper, used everywhere as CDynamicCast<T>( somePtr ).
//
// Why this is not simply dynamic_cast<T*>( (CObjectBase*)p ) (the original one-liner):
//   That reinterpreted EVERY source pointer as CObjectBase* before the dynamic_cast. For a source
//   that derives from CObjectBase (single inheritance -- the engine's rule) the reinterpret is a
//   harmless offset-0 upcast, so the cast worked. But for a *pure interface* source -- one that does
//   NOT derive from CObjectBase (NWorld::IExecMove, NWorld::IGetApproaches, ...) -- (CObjectBase*)p
//   mislabels the interface subobject pointer, and modern MSVC's __RTDynamicCast rejects the
//   srcType/offset mismatch and returns NULL (VC7.1 tolerated it). That silently broke real
//   cross-casts and produced hard-to-find AVs (re-path crash, open-door-on-path crash, ...).
//
// Root fix: when the source type is complete and polymorphic, dynamic_cast directly on the REAL
// source pointer -- this is offset-correct for BOTH CObjectBase-derived sources and pure interfaces.
// The direct overload is SFINAE-selected via its decltype return type, so it silently drops out for
// forward-declared / incomplete / non-polymorphic sources; those fall to the legacy (CObjectBase*)
// reinterpret, which is exactly the case where the relationship can't be seen at the call site and
// where the single-inheritance-from-CObjectBase rule makes the offset-0 reinterpret valid (this is
// also the only case the original ever handled for incomplete types). Source const is stripped first
// so const interface pointers also take the correct direct path. No call site needs to change.
template<class T>
class CDynamicCast
{
	T *ptr;
	// complete + polymorphic source: dynamic_cast on the true pointer (offset-correct cross-cast)
	template<class TT>
		static auto DoCast( TT *p, int ) -> decltype( dynamic_cast<T*>(p) ) { return dynamic_cast<T*>(p); }
	// forward-declared / incomplete / non-polymorphic source: legacy reinterpret to the CObjectBase root
	template<class TT>
		static T* DoCast( TT *p, long ) { return dynamic_cast<T*>( (CObjectBase*)p ); }
public:
	template<class TT>
		inline CDynamicCast( TT *_ptr ) { ptr = DoCast( const_cast<typename SDynCastStripConst<TT>::Type*>(_ptr), 0 ); }
	template<class T1, class T2>
		inline CDynamicCast( const CPtrBase<T1,T2> &_ptr ) { ptr = dynamic_cast<T*>(_ptr.GetBarePtr()); }
	inline operator T*() const { return ptr; }
	inline T* operator->() const { return ptr; }
	inline T* GetPtr() const { return ptr; }
};
template <class T>
inline bool IsValid( const CDynamicCast<T> &p ) { return IsValid( p.GetPtr() ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif