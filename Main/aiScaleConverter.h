#ifndef __AISCALECONVERTER_H_
#define __AISCALECONVERTER_H_

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// ÑScaleConverterPoint
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScaleConverterPoint: public CObjectBase
{
	OBJECT_BASIC_METHODS(CScaleConverterPoint)
	ZDATA
public:
	float fPoint;
	float fValue;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&fPoint); f.Add(3,&fValue); return 0; }

	CScaleConverterPoint() {}
	CScaleConverterPoint( float _fPoint, float _fValue ): fPoint(_fPoint), fValue(_fValue) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// ÑScaleConverter 
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScaleConverter: public CObjectBase
{
	OBJECT_BASIC_METHODS(CScaleConverter)
	ZDATA
	vector< CObj<CScaleConverterPoint> > vPoints;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&vPoints); return 0; }
public:
	CScaleConverter() {}
	void AddPoint( float fPoint, float fValue );
	float Convert( float fPoint );
	void Clear();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif