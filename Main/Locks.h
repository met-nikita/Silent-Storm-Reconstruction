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
// CLockable (a virtual-base mixin in the PDB: vbptr@0, CPtr<CLockObject> pLock@4, virtual base
// ILockable@8) owns the token and implements the ILockable interface over it. The IsLocked/Lock logic
// also stays exposed as free functions (Lockable_IsLocked/Lockable_Lock) for the door-specific legacy
// paths. A token is "dead" when its CObjectBase contents-destroyed flag is set (nObjData & 0x80000000,
// exposed as CObjectBase::IsRefInvalid); a dead lock counts as no lock.
//
// NOTE the retail gfx-buffer CLockable is NGfx::CLockable (a different, namespaced class) -- the dev
// GfxBuffersInternal.h one is inside `namespace NGfx` too, so this global CLockable does NOT collide
// (the earlier note here claiming a collision was wrong; retail has both classes the same way).
//
// Retail CLockable subobject serialization (CStructureSaver::CallObjectSerialize<CLockable>
// @0x2aab70): a chunk whose tag 2 is the CPtr<CLockObject> pLock. Lock-carrying classes serialize the
// base as a numbered sub-chunk: NRPG::CInventoryItem tag 4 (@0x2aab10), NWorld::CCannon tag 6
// (@0x383970), NWorld::CUnitServer tag 31 (@0x3c6e80).
//
// Decode rvas: CLockObject ctor/$E27 @0x4a25b0, New @0x25db00, DestroyContents @0x25db30;
//   CLockable::IsLocked @0x25d9e0, CLockable::Lock @0x25da10; CLockable copy ctor @0x2a7b60
//   (plain pLock CPtr copy -- the compiler-generated copy here reproduces it).
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
// ILockable -- the retail lock interface (PDB: pure vtable, 4 bytes; a VIRTUAL base of both CLockable
// and NRPG::IInventoryItem). Vtable order decoded from the call sites: slot 0 = IsLocked (the
// CExecCannon::CanDoIt @0x3a2360 lock gate calls pLock->vtbl[0](unit)), slot +4 = Lock (CCmdCannon::
// Lock @0x3b1d20 / CCmdTakeCorpse::Lock @0x3b1c90 / SItem::LockItem @0x3b1c30 all call vtbl+4).
class ILockable
{
public:
	virtual bool IsLocked( CObjectBase *pBy ) = 0;       // retail CLockable::IsLocked @0x25d9e0
	virtual CObjectBase* Lock( CObjectBase *pBy ) = 0;   // retail CLockable::Lock @0x25da10
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLockable -- the retail lock-state mixin base of NRPG::CInventoryItem (@+0xc), NWorld::CCannon
// (@+0xa0) and NWorld::CUnitServer (@+0x17c). Serialized as a sub-chunk: tag 2 = pLock
// (CStructureSaver::CallObjectSerialize<CLockable> @0x2aab70).
class CLockable: public virtual ILockable
{
public:
	ZDATA
	CPtr<CLockObject> pLock;   // retail CLockable+0x4 -- the held lock token, or null
	ZEND int operator&( CStructureSaver &f ) { f.Add( 2, &pLock ); return 0; }
	//
	virtual bool IsLocked( CObjectBase *pBy ) { return Lockable_IsLocked( pLock, pBy ); }       // @0x25d9e0
	virtual CObjectBase* Lock( CObjectBase *pBy ) { return Lockable_Lock( pLock, pBy ); }        // @0x25da10
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __LOCKS_H_
