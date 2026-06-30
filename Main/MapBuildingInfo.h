#ifndef __MapBuildingInfo_H_
#define __MapBuildingInfo_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "../DBFormat/DataMap.h"
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
	float fRotation; // в градусах
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
	CObj<NBuilding::CBuildingGrid> pGrid;
	CObj<NBuilding::CSolidAndWallMap> pSWMap;
	vector<SStorey> stories;
	vector<SAmbientLight> lights;
	CVec2 ptAlignTo;

	SMapPosition mpos; // исп. в редакторе, а также для отложенного вычисления матрицы трансформации

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
		f.Add( 1, &pVariant );
		f.Add( 2, &pos );
		f.Add( 3, &pGrid );
		f.Add( 4, &stories );
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
