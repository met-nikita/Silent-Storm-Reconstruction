#ifndef __MESERIALIZE_H_
#define __MESERIALIZE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
	class CWaypoint;
	class CUnitAIInfo;
}
namespace NBuilding
{
	class CBuildInfo;
}
class CMETerrainInfo;
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SerializeBuilding( NBuilding::CBuildInfo *pInfo, int nBuildingID );
bool SerializeWaypoint( NAI::CWaypoint *pW, int nWaypointID );
bool SerializeUnitAIInfo( NAI::CUnitAIInfo *pU, int nUnitID );
bool SerializeTerrain( CMETerrainInfo *pTerr, int nTerrID );
bool SerializeUnitGroupAIInfo( NAI::CUnitAIInfo *pU, int nGroupID );
bool DeleteWaypoint( int nID );
bool DeleteBuilding( int nID );
bool DeleteUnitAIInfo( int nID );
bool DeleteTerrain( int nID );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __MESERIALIZE_H_