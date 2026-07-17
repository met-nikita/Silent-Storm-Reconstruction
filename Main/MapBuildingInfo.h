#ifndef __MapBuildingInfo_H_
#define __MapBuildingInfo_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "../DBFormat/DataMap.h"
#include "DiscretePos.h"   // NWorld::CFBTransform (retail SMapBuilding stores the placement as CObj<CFBTransform>)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBuilding
{
	class CBuildingGrid;
	class CSolidAndWallMap;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMapPosition
{
	CVec3 ptPos;
	CVec3 ptScale;
	float fRotation; // � ��������
	int nFloor;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMapBuilding
{
	struct SStorey
	{
		int nFloor; // relative to building
		int nRealFloor; // global floor
		
		SStorey() {}
		SStorey( int _nFloor, int _nRealFloor )
			: nFloor(_nFloor), nRealFloor(_nRealFloor) {}
	};
	struct SAmbientLight
	{
		CVec3 color;
		int nFloor;
		bool bLightmap;
	};

	CDBPtr<NDb::CTemplVariant> pVariant;
	SFBTransform pos;
	CObj<CFBTransform> pPos;   // retail's serialized placement (tag 3); `pos` above is this fork's flat mirror
	CObj<NBuilding::CBuildingGrid> pGrid;
	CObj<NBuilding::CSolidAndWallMap> pSWMap;
	vector<SStorey> stories;
	vector<SAmbientLight> lights;
	CVec2 ptAlignTo;

	SMapPosition mpos; // ���. � ���������, � ����� ��� ����������� ���������� ������� �������������

	const SStorey& GetStorey( int nLocalFloor ) const
	{
		for ( vector<SStorey>::const_iterator i = stories.begin(); i != stories.end(); ++i )
		{
			if ( i->nFloor == nLocalFloor )
				return *i;
		}
#ifndef _MAPEDIT
		ASSERT( 0 );
#endif
		static SStorey fake;
		return fake;
	}

	int operator&( CStructureSaver &f )
	{
		// retail SMapBuilding::operator& @0x345ce0 = {2 pVariant, 3 pPos(CObj<CFBTransform>), 4 pGrid,
		// 5 pSWMap(CObj<CSolidAndWallMap>), 6 stories(DoDataVector), 7 ptAlignTo(CVec2)}. dev was OFF BY ONE
		// from tag 1 (pVariant@1 .. stories@4) and lacked pSWMap/ptAlignTo, so pGrid read retail's pPos ref
		// (dynamic_cast -> null) -> CBuilding::Update in CWorld::CreateRestored null-derefs pGrid
		// (BuildingGrid.cpp:305). retail stores the placement as an owned CObj<CFBTransform> func-node; this
		// fork keeps the flat SFBTransform `pos`, so sync the two around the wire.
		if ( !f.IsReading() && !IsValid( pPos ) )
			pPos = new CFBTransform( pos );
		f.Add( 2, &pVariant );
		f.Add( 3, &pPos );
		f.Add( 4, &pGrid );
		f.Add( 5, &pSWMap );
		f.Add( 6, &stories );
		f.Add( 7, &ptAlignTo );
		if ( f.IsReading() && IsValid( pPos ) )
			pos = pPos->pos;
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
