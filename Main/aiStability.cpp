#include "StdAfx.h"
#include "aiStability.h"
#include "aiMap.h"
#include "..\DBFormat\DataFormat.h"
#include "wTSFlags.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
const float FP_STABLE_HEIGHT = 0.15f;
bool CheckObjectStability( IAIMap *pMap, NDb::CModel *pModel, const SHMatrix &_pos, CObjectBase *pIgnore )
{
	if ( !pModel )
		return true;

	CVec3 massCenter;
	vector<SMassSphere> massSpheres;
	NAI::GetSpheres( pModel, &massSpheres, &massCenter );
	if ( massSpheres.empty() )
		return true;
	const SHMatrix &fwd = _pos;
	vector<SSphere> spheres;
	CVec3 tmp;
	for ( int i = 0; i < massSpheres.size(); ++i )
	{
		fwd.RotateHVector( &tmp, massSpheres[i].ptCenter );
		spheres.push_back( SSphere( tmp, massSpheres[i].fRadius ) );
	}
	fwd.RotateHVector( &massCenter, massCenter );	
	vector<CVec3> stable;
	for ( int i = 0; i < spheres.size(); ++i )
	{
		if ( pMap->CalcIntersection( spheres[i].ptCenter, spheres[i].fRadius, NWorld::TS_ITEM_BLOCKER, pIgnore ) ) //TERRAINS | NWorld::TS_OBJECTS
			stable.push_back( spheres[i].ptCenter );
	}
	if ( spheres.size() < 4 )
		return ( spheres.size() == stable.size() );
	if ( stable.size() <= spheres.size() / 2 )
		return false;
	return true;
	/*
	bool bStable = true;
	for ( int i = 0; i < stable.size(); ++i )
		for ( int j = i + 1; j < stable.size(); ++j )
		{
			CVec3 &p1 = stable[i];
			CVec3 &p2 = stable[j];
			float a = p2.y - p1.y;
			float b = p1.x - p2.x;
			float c = p2.x * p1.y - p2.y * p1.x;
			float fSign = 0;
			for ( int k = 0; k < stable.size(); ++k )
			{
				if ( k == i || k == j )
					continue;
				float fDist = a * stable[k].x + b * stable[k].y + c;
				if ( fabs(fDist) < 1e-6f )
					continue;
				if ( fSign == 0 )
					fSign = Sign(fDist);
				else if ( Sign(fDist) != fSign )
				{
					fSign = 0;
					break;
				}
			}
			if ( fSign != 0 )
			{
				float fDist = a * massCenter.x + b * massCenter.y + c;
				if ( fabs(fDist) < 1e-6f )
					continue;
				if ( Sign(fDist) != fSign )
				{
					bStable = false;
					break;
				}
			}
		}
	return bStable;
	*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
