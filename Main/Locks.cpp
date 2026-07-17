#include "StdAfx.h"
//
#include "Locks.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// Locks - release object-lock token registration. Reconstructed from the matched-release decode
// (decomp/src/s2_locks.h). The CLockObject token + the ILockable/CLockable mixin live in Locks.h.
// Lockable game objects wired W3 2026-07-13: NRPG::CInventoryItem (base, tag 4), NWorld::CCannon
// (base, tag 6), NWorld::CUnitServer (base, tag 31); token-holding commands: CCmdCannon/
// CCmdTakeCorpse pLock (tag 3), SItem pLockItem (tag 9). The exec-side callers (CExecCannon pCmd
// lock gate @0x3a2360, CExecCorpse pCmd) are a separate exec leg.
////////////////////////////////////////////////////////////////////////////////////////////////////
REGISTER_SAVELOAD_CLASS( 0x23065400, CLockObject )
