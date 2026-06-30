#ifndef __LOCKS_H_
#define __LOCKS_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Locks - the release object-lock TOKEN subsystem (saveload id 0x23065400), absent from the dev tree
// (whose locking is door-specific CWindowDoor::IsLockedDoor + pathnet CPathNetwork::IsLocked, not this
// general ref-counted token). Reconstructed from the matched-release decode (oracle:
// decomp/src/s2_locks.h).
//
// A CLockObject is a ref-counted lock token whose pHandle names the object currently holding the lock.
// CLockable (a virtual-base mixin in the PDB) owns a CPtr<CLockObject> pLock; its IsLocked/Lock logic
// is reconstructed here as free functions over that CPtr (no CLockable class is defined -- it would
// collide with the unrelated GfxBuffers CLockable -- and none is needed: the logic is purely about the
// token + the holder). A token is "dead" when its CObjectBase contents-destroyed flag is set
// (nObjData & 0x80000000, exposed as CObjectBase::IsRefInvalid); a dead lock counts as no lock.
//
// Decode rvas: CLockObject ctor/$E27 @0x4a25b0, New @0x25db00, DestroyContents @0x25db30;
//   CLockable::IsLocked @0x25d9e0, CLockable::Lock @0x25da10.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLockObject: public CObjectBase
{
	OBJECT_BASIC_METHODS( CLockObject );   // New/Duplicate/MakeCopy; auto DestroyContents == @0x25db30
	                                       // (releases pHandle + re-inits, preserving nRefData/nObjData)
	ZDATA
	CPtr<CObjectBase> pHandle;             // +0x0c -- the object holding the lock
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, &pHandle ); return 0; }

public:
	CLockObject() {}
	CObjectBase* GetHandle() const { return pHandle.GetPtr(); }
	void SetHandle( CObjectBase *p ) { pHandle = p; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLockable::IsLocked @0x25d9e0: locked AGAINST pBy -- a LIVE lock held by someone ELSE. Own lock,
// no lock, and dead lock all read as "not locked".
inline bool Lockable_IsLocked( const CPtr<CLockObject> &pLock, CObjectBase *pBy )
{
	CLockObject *p = pLock.GetPtr();
	if ( p != 0 && !p->IsRefInvalid() )
		return p->GetHandle() != pBy;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLockable::Lock @0x25da10: acquire (or re-acquire) the lock for pBy. Returns the token, or null if a
// live lock is held by someone else. No lock / dead lock: a fresh CLockObject{pHandle=pBy} replaces
// the old one (the new token starts at refcount 0; the CPtr assignment bumps it and releases the old).
inline CObjectBase* Lockable_Lock( CPtr<CLockObject> &pLock, CObjectBase *pBy )
{
	CLockObject *p = pLock.GetPtr();
	if ( p == 0 || p->IsRefInvalid() )
	{
		CLockObject *pNew = new CLockObject();
		pNew->SetHandle( pBy );
		pLock = pNew;
	}
	else if ( p->GetHandle() != pBy )
		return 0;
	return pLock.GetPtr();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __LOCKS_H_
