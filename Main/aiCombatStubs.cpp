#include "StdAfx.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// Phase-7 supersede - placeholder definitions + cast registration for the release-only inventory/area
// types the CAICombatLogic substrate references by pointer but that are NOT yet reconstructed:
//
//   CAIFireArmsWeaponBase  - the firearms-weapon base the reload action's SInfo points at (dev tree has
//                            the concrete CAIFireArmsWeapon; the release base is not yet ported).
//
// (CAIMeleeWeapon / CAIThrowingWeapon / CAIFirstAid were here too, now reconstructed as real wrappers in
// aiWeapon.h/.cpp.)
//
// The substrate only ever forward-declares these and holds CPtr<T> to them; because the CPtr<>/CObj<>
// machinery instantiates CastTo{ObjectBase,UserObject}Impl<T> for serialization/ref-counting, every TU
// that touches such a CPtr needs those cast helpers DEFINED somewhere. BASIC_REGISTER_CLASS provides
// them (it requires only a complete CObjectBase-derived T). These minimal stubs satisfy the link; the
// real reconstructed types (with members) replace them in the build-settle. The GetInfoInner bodies
// that would populate these pointers are themselves still ASSERT/bCanDo=false stubs, so nothing creates
// a real instance yet.
////////////////////////////////////////////////////////////////////////////////////////////////////
// (CAIFireArmsWeaponBase stub RETIRED -- the real base class now lives in aiWeapon.h with the retail
//  member set + operator& tags 2-5, and its saveload registration 0x52642100 in aiWeapon.cpp;
//  serialization-convergence Wave 2.)
