#include "StdAfx.h"

#include "aiScaleConverter.h"

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// ÑScaleConverter 
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScaleConverter::AddPoint( float fPoint, float fValue )
{
	vPoints.push_back( new CScaleConverterPoint( fPoint, fValue ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CScaleConverter::Convert( float fPoint )
{
	if ( vPoints.empty() )
		return 0;

	vector< CObj<CScaleConverterPoint> >::iterator i = vPoints.begin();
	vector< CObj<CScaleConverterPoint> >::iterator j = vPoints.begin(); ++j;
	for ( ; j != vPoints.end() ; )
	{
		if ( fPoint >= (*i)->fPoint && fPoint <= (*j)->fPoint )
		{
			float fCoof = ( (*j)->fValue - (*i)->fValue ) / ( (*j)->fPoint - (*i)->fPoint );
			return (*i)->fValue + ( fPoint - (*i)->fPoint ) * fCoof;
		}
		++i; ++j;
	}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScaleConverter::Clear()
{
	vPoints.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x52822135, CScaleConverterPoint );
REGISTER_SAVELOAD_CLASS( 0x52822136, CScaleConverter );