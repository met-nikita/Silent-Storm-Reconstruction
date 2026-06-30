#include "StdAfx.h"
//
#include "wUnitServer.h"      // NWorld::CUnitServer (complete)
#include "wOSBase.h"          // NWorld::CObjectServerBase (complete)
//
#include "wPocket.h"
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPocket - release "between-maps pocket" bodies. Reconstructed from the matched-release decode
// (decomp/src/s2_pocket.h). The unit-pocket half is the release form of the dev
// CWorld::PlaceUnitInPocket/RemoveUnitFromPocket/IsUnitInPocket (wMain.cpp); the object-pocket half is
// release-new (the dev had no object pocket). Each Place silently skips null/zombie/duplicate entries
// (the release guard @0x00387e50/@0x00387f00; the dev asserted -- omitted here to match release behavior).
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPocket::IsUnitInPocket( CUnitServer *pUnit ) const                          // @0x00387d10
{
	return IsSmthInPocket( pUnit, unitPocket );                                   // -> @0x00387cb0
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPocket::IsObjectInPocket( CObjectServerBase *pObject ) const                // @0x00387d40
{
	return IsSmthInPocket( pObject, objectPocket );                               // -> @0x00387ce0
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPocket::PlaceUnitInPocket( CUnitServer *pUnit )                             // @0x00387fb0
{
	PlaceSmthInPocket( pUnit, true, unitPocket );                                 // master entry -> @0x00387e50
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPocket::PlaceObjectInPocket( CObjectServerBase *pObject )                   // @0x00387fd0
{
	PlaceSmthInPocket( pObject, false, objectPocket );                            // non-master entry -> @0x00387f00
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPocket::RemoveUnitFromPocket( CUnitServer *pUnit )                          // @0x00387e10
{
	RemoveSmthFromPocket( pUnit, unitPocket );                                    // -> @0x00387d70
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPocket::RemoveObjectFromPocket( CObjectServerBase *pObject )                // @0x00387e30
{
	RemoveSmthFromPocket( pObject, objectPocket );                                // -> @0x00387dc0
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NWorld;
//
REGISTER_SAVELOAD_CLASS( 0xA0523090, CPocket )
