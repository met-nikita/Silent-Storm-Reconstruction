#include "StdAfx.h"
#include "wUnitServer.h"   // NWorld::CUnitServer, CDumbUnitServer::IsLocker (befriended), CDynamicCast, IsValid
#include "aiPosition.h"    // NAI::IsLockerUnit / IsUnitNear decls, NAI::SUnitPosition, CVec3
#include "aiLocker.h"      // NAI::IsBigLocker

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// aiPositionDebug -- locker-validity probes originally consumed by CPathNetwork::DebugCheck.
// Reconstructed from v1.1 NAI::IsLockerUnit @0x4917d0 and NAI::IsUnitNear @0x491810.
// IsLockerUnit is also used by the runtime network to prevent corpse relocking;
// IsUnitNear remains a debug probe. Neither helper changes the save format.
////////////////////////////////////////////////////////////////////////////////////////////////////
// The lock owner must be a live NWorld::CUnitServer for which CDumbUnitServer::IsLocker() holds.
// The retail null+dead-object test is the engine-standard IsValid() (non-null and not ref-invalid).
bool IsLockerUnit( CObjectBase *pUnit )
{
	CDynamicCast<NWorld::CUnitServer> pUS( pUnit );
	if ( !IsValid( pUS ) )
		return false;
	return pUS->IsLocker();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// The unit's center (SUnitPosition::GetCP) must lie within 2 grid steps -- 4 for a big locker -- of
// pt, full-3D distance, inclusive, with the retail 0.00625 epsilon: 1.25625 (@0x3fa0cccd) /
// 2.50625 (@0x40206667). IsBigLocker is keyed off the unit (the retail EDI/point aliasing is a
// decompiler artefact), so it re-probes the same unit object.
bool IsUnitNear( CObjectBase *pUnit, const CVec3 &pt )
{
	CDynamicCast<NWorld::CUnitServer> pUS( pUnit );
	if ( !IsValid( pUS ) )
		return false;
	CVec3 cp = pUS->GetPosition().GetCP();
	bool bBigLocker = IsBigLocker( pUnit );
	return fabs( pt - cp ) <= ( bBigLocker ? 2.50625f : 1.25625f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
