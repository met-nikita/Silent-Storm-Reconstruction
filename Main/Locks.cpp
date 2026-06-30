#include "StdAfx.h"
//
#include "Locks.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// Locks - release object-lock token registration. Reconstructed from the matched-release decode
// (decomp/src/s2_locks.h). The CLockObject token + the CLockable Lock/IsLocked logic (the free
// functions in Locks.h) are the standalone release subsystem; wiring lockable game objects (doors,
// containers, units) to USE it -- superseding the dev's door-specific locking -- is a follow-up.
////////////////////////////////////////////////////////////////////////////////////////////////////
REGISTER_SAVELOAD_CLASS( 0x23065400, CLockObject )
