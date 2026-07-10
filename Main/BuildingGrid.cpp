#include "StdAfx.h"
#include "..\Misc\2DArray.h"
#include "BuildingGrid.h"
#include "BuildingInfo.h" // WALL_HEIGHT
#include "Grid.h"
#include "MELayers.h"
#include "BuildingSchema.h"

namespace NBuilding
{
const float PLANE_DIST = WALL_HEIGHT / 4; // distance between planes
const float MIN_NODE_DIST = FP_GRID_STEP; // min. distance between nodes
const float MIN_ADDITIVE_DMG = 50;
const int N_MAX_HP = 253;
const int N_INDESTRUCTIBLE = 255;
const int N_CELLAR = 254;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBuildingGrid
////////////////////////////////////////////////////////////////////////////////////////////////////
CBuildingGrid::CBuildingGrid() : seed( GetTickCount() )
{
	nDZ = 0;
	bStabilityUpdate = true;
	bOnlyCutFloorVisible = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingGrid::Setup( int _nMaxX, int _nMaxY, int _nMinFloor, int _nMaxFloor, 
	const CVec2 &ptMinXY, const SFBTransform &_pos )
{
	nBaseFloor = 0;
	pos = _pos;
	net.SetSizes( 2 + 2 * _nMaxX, 2 + 2 * _nMaxY, (_nMaxFloor - _nMinFloor + 1) * 4 + 1 );
	nDZ = -_nMinFloor * 4;
	ptBoxMin = CVec3( ptMinXY.x, ptMinXY.y, _nMinFloor * WALL_HEIGHT );
	ptBoxMax = CVec3( ptBoxMin.x + _nMaxX, ptBoxMin.y + _nMaxY, (_nMaxFloor + 1) * WALL_HEIGHT );
	nCutFloor = _nMaxFloor;
	//
	ComputeAuxValues();
	//
	net.FillEvery( 0 );
	Updated();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingGrid::GetSize( CVec3 *pptMin, CVec3 *pptMax ) const
{
	ASSERT( pptMin && pptMax );
	pptMin->x = ptBoxMin.x;
	pptMin->y = ptBoxMin.y;
	pptMin->z = ptBoxMin.z / WALL_HEIGHT;
	pptMax->x = ptBoxMax.x - ptBoxMin.x;
	pptMax->y = ptBoxMax.y - ptBoxMin.y;
	pptMax->x = ptBoxMax.z / WALL_HEIGHT - 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// FRONT, BACK, LEFT, RIGHT, TOP, BOTTOM
void CBuildingGrid::ComputeAuxValues()
{
	box[0].Set( CVec3( 0, -1, 0 ), ptBoxMin.y );
	box[1].Set( CVec3( 0, 1, 0 ), -ptBoxMax.y );
	box[2].Set( CVec3( -1, 0, 0 ), ptBoxMin.x );
	box[3].Set( CVec3( 1, 0, 0 ), -ptBoxMax.x );
	box[4].Set( CVec3( 0, 0, 1 ), -ptBoxMax.z );
	box[5].Set( CVec3( 0, 0, -1 ), ptBoxMin.z );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHECKME: inline gives linker errors, but is it really needed here?
bool CBuildingGrid::IsValidCoord( const SPoint3 &pt ) const
{
	return pt.x >= 0 && pt.y >= 0 && pt.z + nDZ >= 0 && 
		pt.x < net.GetXSize() && pt.y < net.GetYSize() && pt.z + nDZ < net.GetZSize();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHECKME: inline gives linker errors, but is it really needed here?
BYTE& CBuildingGrid::At( const SPoint3 &pt )
{
	ASSERT( IsValidCoord( pt ) );
	return net[pt.z + nDZ][pt.y][pt.x];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuildingGrid::IsDestroyed( const SPoint3 &pt ) const
{
	if ( !IsValidCoord( pt ) )
		return true;
	return const_cast<CBuildingGrid*>( this )->At( pt ) == 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingGrid::UpdatePart( const SPoint3 &pt )
{
	for ( int x = -1; x < 2; ++x )
		for ( int y = -1; y < 2; ++y )
			for ( int z = -1; z < 2; ++z )
			{
				SPoint3 pt( pt.x + x, pt.y + y, pt.z + z );
				const SPart part = Point2Part( pt );
				updatedParts[part] = true;
			}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuildingGrid::DamageSpot( const SPoint3 &pt, int nDmg, bool bAddToBrokenSpots )
{
	if ( !IsValidCoord( pt ) )
		return false;
	BYTE &node = At( pt );
	if ( 0 == node || N_INDESTRUCTIBLE == node || N_CELLAR == node )
		return false;
	node = Max( 0, int(node) - nDmg );
	if ( 0 == node )
	{
		UpdatePart( pt ); // slow
		Updated();
		if ( IsValid( pSchema ) )
			pSchema->Destroy( this, pt );
		if ( bAddToBrokenSpots )           // retail @0xc35b0: record the destroyed voxel for the destruction FX
			brokenSpots.push_back( pt );
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0xc3170: hand the accumulated destroyed-voxel list to the caller (CBuilding::RenderDestructionEffects)
// and clear the member (vector::swap on an empty pSpots), so each segment-step flush sees only the new breaks.
void CBuildingGrid::GetBrokenSpots( vector<SPoint3> *pSpots )
{
	brokenSpots.swap( *pSpots );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline int NearestPlane( float zeroPlaneZ, float z, int nDZ )
{
	return Float2Int( (z - zeroPlaneZ) / PLANE_DIST - nDZ );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline int NearestXY( float xy )
{
	return xy / MIN_NODE_DIST;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline int Power( int nPower, const CVec3 &ptCenter, float x, float y, float z )
{
	float d = ::fabs2( ptCenter.x - x, ptCenter.y - y, ptCenter.z - z );
	return nPower * (d < 1.f ? 1.f : 1.f / d);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingGrid::Explode( const CVec3 &ptEpicentre, int nPower, float fRadius )
{
	if ( nPower <= MIN_ADDITIVE_DMG )
		return;
	//
	//float fRadius = sqrt( nPower - MIN_ADDITIVE_DMG );
	float fRadius2 = sqr( fRadius / 2 );
	CVec4 ptLocal;
	pos.backward.RotateHVector( &ptLocal, ptEpicentre );
	CVec3 ptCenter( ptLocal.x, ptLocal.y, ptLocal.z );

	CVec3 ptMin, ptMax;
	if ( !Intersection( ptCenter, fRadius, &ptMin, &ptMax ) )
		return;
	const int minPl = Max( -nDZ, NearestPlane( ptBoxMin.z, ptMin.z, nDZ ) );
	const int maxPl = NearestPlane( ptBoxMin.z, ptMax.z, nDZ );
	const int minX = NearestXY( ptMin.x - ptBoxMin.x );
	const int maxX = NearestXY( ptMax.x - ptBoxMin.x );
	const int minY = NearestXY( ptMin.y - ptBoxMin.y );
	const int maxY = NearestXY( ptMax.y - ptBoxMin.y );
	const int nFloor = 0.25f * minPl;
	ptMin.z = WALL_HEIGHT * nFloor + (minPl - nFloor * 4) * 0.25f * WALL_HEIGHT;
	int iz, ix, iy;
	float x, y, z;
	for ( iz = minPl, z = ptMin.z; iz <= maxPl; z += PLANE_DIST, ++iz )
	{
		for ( iy = minY, y = ptMin.y; iy < maxY; y += MIN_NODE_DIST, ++iy )
		{
			for ( x = ptMin.x, ix = minX; ix < maxX; x += MIN_NODE_DIST, ++ix )
			{
				if ( ::fabs2( ptCenter - CVec3(x,y,z) ) <= fRadius2 )
					DamageSpot( SPoint3( ix, iy, iz ), Power( nPower, ptCenter, x, y, z ) );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuildingGrid::Intersection( const CVec3 &ptCenter, float fR, CVec3 *pPtMin, CVec3 *pPtMax )
{
	for ( int i = 0; i < 6; ++i )
	{
		float fDist = box[i].GetDistanceToPoint( ptCenter );
		if ( fDist > fR )
			return false;
	}
	//
	pPtMin->x = Max( ptBoxMin.x, ptCenter.x - fR );
	pPtMin->y = Max( ptBoxMin.y, ptCenter.y - fR );
	pPtMin->z = Max( ptBoxMin.z, ptCenter.z - fR );
	pPtMax->x = Min( ptBoxMax.x, ptCenter.x + fR );
	pPtMax->y = Min( ptBoxMax.y, ptCenter.y + fR );
	pPtMax->z = Min( ptBoxMax.z, ptCenter.z + fR );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingGrid::AddRoom( int nFloor, int nInternal, int nGlobal )
{
	rooms.push_back( SRoomMatch( nFloor, nInternal, nGlobal ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CBuildingGrid::GetRoomGlobal( int nFloor, int nInternal ) const
{
	for ( int k = 0; k < rooms.size(); ++k )
	{
		if ( rooms[k].nFloor == nFloor && rooms[k].nInternalRoom == nInternal )
			return rooms[k].nGlobalRoom;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingGrid::GetRoomLocal( int nGlobalID, int *pnFloor, int *pnInternal ) const
{
	ASSERT( pnFloor && pnInternal );

	for ( int k = 0; k < rooms.size(); ++k )
	{
		if ( rooms[k].nGlobalRoom == nGlobalID )
		{
			*pnFloor = rooms[k].nFloor;
			*pnInternal = rooms[k].nInternalRoom;
			return;
		}
	}
	*pnFloor = *pnInternal = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingGrid::Reset()
{
	if ( NeedComputeStability() )
		net.FillEvery( 0 );
	else
		//net.FillEvery( N_INDESTRUCTIBLE );
		net.FillEvery( N_MAX_HP );
	Updated();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingGrid::AddHP( const SPoint3 &pt, BYTE hp )
{
	if ( !IsValidCoord( pt ) )
		return;
	int nNewHP = At( pt ) + hp;
	//ASSERT( nNewHP <= N_MAX_HP );
	At( pt ) = Min( N_MAX_HP, nNewHP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingGrid::SetCutFloor( int nFloor ) 
{ 
	nCutFloor = nFloor; 
	Updated();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingGrid::SetIndestructible( const SPoint3 &pt )
{
	At( pt ) = N_INDESTRUCTIBLE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingGrid::SetCellar( const SPoint3 &pt )
{
	At( pt ) = N_CELLAR;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuildingGrid::IsCellar( const SPoint3 &pt )
{
	return N_CELLAR == At( pt );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingGrid::SetVisibleLayers( const vector<int> &layers )
{
	visibleLayers.clear();
	for( int i = 0; i < layers.size(); ++i )
	{
		ELayer type;
		int ind;
		GetLayerID( layers[i], &type, &ind );
		visibleLayers[MakeFragmentID( type, ind )] = true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingGrid::SetOnlyCutFloorVisible( bool bVis )
{
	bOnlyCutFloorVisible = bVis;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuildingGrid::IsLayerVisible( int nLayerID ) const
{
	if ( visibleLayers.empty() )
		return true;
	return visibleLayers.find( nLayerID ) != visibleLayers.end();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuildingGrid::IsOnlyCutFloorVisible() const
{
	return bOnlyCutFloorVisible;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingGrid::GetUpdatedParts( vector<SPart> *pParts )
{
	if ( !pParts )
	{
		ASSERT(0);
		return;
	}
	for (unordered_map<SPart, bool, SPart>::const_iterator i = updatedParts.begin(); i != updatedParts.end(); ++i )
//		if ( i->second )
		pParts->push_back( i->first );
	updatedParts.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline SPart CBuildingGrid::Point2Part( const SPoint3 &pt )
{
	const int z = pt.z - 1;
	return SPart( z < 0 ? z/4 - 1 : z/4, pt.x / 2, pt.y / 2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NBuilding;
REGISTER_SAVELOAD_CLASS( 0x02741121, CBuildingGrid );
