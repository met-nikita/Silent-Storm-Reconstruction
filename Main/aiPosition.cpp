#include "StdAfx.h"
#include "aiPosition.h"
#include "aiGrid.h"
#include "Grid.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
int nMoveShift[][2] = { {1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1} };
////////////////////////////////////////////////////////////////////////////////////////////////////
// SPosition
////////////////////////////////////////////////////////////////////////////////////////////////////
SPosition::SPosition( const SPathPlace &_p, IPathNetwork *_pNet ) : p(_p)
{
	pNet = dynamic_cast<CPathNetwork*>( _pNet );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SPosition::IsValid() const
{
	return p.GetLayer() < pNet->GetNumLayers();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec2 SPosition::GetCPNoHeight() const
{
	return pNet->GetCPNoHeight( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 SPosition::GetCP() const
{
	return pNet->GetCP( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float SPosition::GetDirection() const
{
	return pNet->GetDirection( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int SPosition::GetFloor() const
{
	return pNet->GetFloor( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SPosition::SetNetwork( IPathNetwork *_pNet )
{
	pNet = dynamic_cast<CPathNetwork*>( _pNet );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int SPosition::operator&( CStructureSaver &f )
{
	f.Add( 1, &p );
	f.Add( 2, &pNet );
	return 0;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// MakeFlyPos -- see aiPosition.h. Retail @0x3d0e0 worker: the network snaps *pOut onto layer 0 near
// the world point, then the altitude (pt.z - groundZ) is quantised into nLayer (8 bits, 0..255),
// nFinal is set (the "3D" flag CPathNetwork::GetCP decodes), and the pose is forced to WALK.
// nIntegral/nMoving/nDirection are preserved from *pOut's PRIOR content (mask 0x3c01) -- callers that
// want retail's load-time fly waypoints must pass a default-constructed pos (retail @0x496000).
bool MakeFlyPos( const CVec3 &pt, IPathNetwork *pNet, SPosition *pOut )
{
	if ( !pNet->SetOnLayer( pOut, 0, pt ) )
		return false;
	// retail @0x3d0e0 rounds toward zero (fistp with RC=truncate)
	int nAlt = (int)( ( pt.z - pOut->GetCP().z ) * ( 1.0f / F_3D_STEP ) );
	if ( nAlt > 0xff )
		nAlt = 0xff;
	else if ( nAlt < 0 )
		nAlt = 0;
	unsigned short hi = (unsigned short)( pOut->p.GetData() >> 16 );
	hi = (unsigned short)( ( ( ( 0xc100 | ( nAlt & 0xff ) ) << 1 ) | ( hi & 0x3c01 ) ) & 0xffff );
	pOut->p = SPathPlace( ( pOut->p.GetData() & 0xffff ) | ( hi << 16 ) );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3d180 wrapper
bool MakeFlyPos( SPosition *pSrc, SPosition *pOut )
{
	CVec3 cp = pSrc->GetCP();					// capture before SetOnLayer mutates *pOut (may alias *pSrc)
	return MakeFlyPos( cp, pSrc->GetNetwork(), pOut );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SObjectPosition
////////////////////////////////////////////////////////////////////////////////////////////////////
int SObjectPosition::operator&( CStructureSaver &f )
{
	f.Add( 1, &pos );
	f.Add( 2, &nFloor );
	return 0;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SUnitPosition
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SUnitPosition::IsValid() const
{
	return pos.IsValid();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float SUnitPosition::GetHeight() const
{
	switch( GetPose() )
	{
		case CRAWL:
			return 0.4f;
		case CROUCH:
			return 1.0f;
		case WALK:
		case RUN:
			return 1.9f;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float SUnitPosition::GetHLHeight( EHitLocation eHL ) const
{
	switch( GetPose() )
	{
		case CRAWL:
			return 0.2f;
		case CROUCH:
			if ( eHL == HL_HEAD )
				return 1.0f;
			else
				return 0.4f;
		case WALK:
		case RUN:
			switch ( eHL )
			{
				case HL_HEAD:
					return 1.6f;
				case HL_BODY:
				case HL_LHAND:
				case HL_RHAND:
					return 1.0f;
				default:
					return 0.4f;
			}
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 SUnitPosition::GetCenter() const
{
	CVec3 vAdd = VNULL3;
	switch( GetPose() )
	{
		case CRAWL:
			vAdd.z = 0.25f;
			break;
		case CROUCH:
			vAdd.z = 0.75f;
			break;
		case WALK:
		case RUN:
			vAdd.z = 1.3f;
			break;
	}
	return GetCP() + vAdd;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 SUnitPosition::GetEyePosition() const
{
	CVec3 vAdd = VNULL3;
	switch( GetPose() )
	{
		case CRAWL:
		{	// retail @0x914c0 LAY arm: eye pushed forward along the facing (Jan03 had z-only 0.3)
			float fDir = GetDirection();        // CPathNetwork::GetDirection @0x3dec0
			vAdd.x = cos( fDir ) * 0.625f;      // 0.625f @0x8b42c4
			vAdd.y = sin( fDir ) * 0.625f;
			vAdd.z = 0.4f;                      // 0x3ecccccd
			break;
		}
		case CROUCH:                            // CM_CROUCH + CM_INACTIVE both land here (retail entries 1&3)
			vAdd.z = 1.1f;
			break;
		case WALK:                              // retail ignores bRun (jumptable entries 2/3 identical)
		case RUN:
			vAdd.z = 1.56f;
			break;
	}
	return GetCP() + vAdd;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SUnitPosition::SetPose( EPose pose ) 
{
	switch( pose )
	{
		case CRAWL:
			pos.p.SetPose( CM_LAY );
			bRun = false;
			break;
		case CROUCH:
			pos.p.SetPose( CM_CROUCH );
			bRun = false;
			break;
		case WALK:
			pos.p.SetPose( CM_STAND );
			bRun = false;
			break;
		case RUN:
			pos.p.SetPose( CM_STAND );
			bRun = true;
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EPose SUnitPosition::GetPose() const
{
	switch ( pos.p.GetPose() )
	{
		case CM_LAY:
			return CRAWL;
		case CM_CROUCH:
		case CM_INACTIVE:
			return CROUCH;
		default:
			if ( bRun )
				return RUN;
			else
				return WALK;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int SUnitPosition::operator&( CStructureSaver &f )
{
	f.Add( 2, &pos );
	//f.Add( 3, &dir );
	f.Add( 4, &bRun );
	return 0;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_CLOSE_HEIGHT_1 = 0.65f;
const float F_CLOSE_HEIGHT_2 = 1.5f;
EBlowHeight GetBlowHeight( const SUnitPosition &attackerPos, const CVec3 &ptTarget )
{
	float fHeightDiff = ptTarget.z - attackerPos.GetCP().z;
	if ( fHeightDiff < F_CLOSE_HEIGHT_1 )
		return BH_BOTTOM;
	if ( fHeightDiff > F_CLOSE_HEIGHT_2 )
		return attackerPos.GetPose() != NAI::CROUCH ? BH_TOP : BH_MIDDLE;
	return BH_MIDDLE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IPathNetwork* CreateNodesNetwork( IAIMap *pMap, IAIJobManager *pJobManager )
{
	return new CPathNetwork( pMap, pJobManager );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////





















