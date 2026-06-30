#ifndef __AIVOLUMECALCER_H_
#define __AIVOLUMECALCER_H_

#include "aiObject.h"
namespace NAI
{
class CGeometryInfo;
class CFileSkinPoints;
////////////////////////////////////////////////////////////////////////////////////////////////////
inline float CalcPyramidVolume( const CVec3 &v1, const CVec3 &v2, const CVec3 &v3, const CVec3 &v4 )
{
	return (1.0f/6) * (( (v2-v1) ^ (v3-v1) ) * (v4-v1));
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CalculateObjectVolume( const vector<CVec3> &points, const vector<STriangle> &tris );
float CalculateObjectVolume( const CGeometryInfo &GeometryInfo );
float CalculateObjectVolume( const CFileSkinPoints &FileSkinPoints );
float CalculateObjectVolume( const CGeometryInfo::SPiece &Piece );
////////////////////////////////////////////////////////////////////////////////////////////////////
void VolumeCalcerTest( int nID );
////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif