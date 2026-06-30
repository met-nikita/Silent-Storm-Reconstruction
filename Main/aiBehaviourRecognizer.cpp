#include "stdafx.h"
//
#include "aiBehaviourRecognizer.h"
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRecognitionAttribute
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRecognitionAttribute: public IRecognitionAttribute
{
	OBJECT_BASIC_METHODS( CRecognitionAttribute );
	ZDATA
	ZEND int operator&( CStructureSaver &f ) { return 0; }
public:
	//
	CRecognitionAttribute() {}
	//
	virtual void Reset() {}
	virtual bool IsTrue() { return true; }
	virtual float GetWeight() { return 1; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRecognitionClass
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRecognitionClass: public IRecognitionClass
{
	OBJECT_BASIC_METHODS( CRecognitionClass );
	ZDATA
	vector< CObj<IRecognitionAttribute> > attributes;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&attributes); return 0; }
public:
	//
	CRecognitionClass() {}
	//
	virtual void Reset();
	virtual float GetEstimation();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRecognitionClass::Reset()
{
	for ( vector< CObj<IRecognitionAttribute> >::iterator i = attributes.begin(); i != attributes.end(); ++i )
		(*i)->Reset();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CRecognitionClass::GetEstimation()
{
	ASSERT( !attributes.empty() );
	//
	float fEstimation = 0;
	for ( vector< CObj<IRecognitionAttribute> >::iterator i = attributes.begin(); i != attributes.end(); ++i )
		fEstimation += ( (*i)->IsTrue() ? 1 : -1 ) * (*i)->GetWeight();
	//
	return fEstimation;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRecognizer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRecognizer: public IRecognizer
{
	OBJECT_BASIC_METHODS( CRecognizer );
	ZDATA
	vector< CObj<IRecognitionClass> > classes;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&classes); return 0; }
public:
	//
	CRecognizer() {}
	//
	virtual void Reset();
	virtual IRecognitionClass *Recognize();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRecognizer::Reset()
{
	for ( vector< CObj<IRecognitionClass> >::iterator i = classes.begin(); i != classes.end(); ++i )
		(*i)->Reset();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IRecognitionClass *CRecognizer::Recognize()
{
	ASSERT( !classes.empty() );
	//
	float nMaxEstimation = -0xFFFF;
	IRecognitionClass *pRes = 0;
	//
	for ( vector< CObj<IRecognitionClass> >::iterator i = classes.begin(); i != classes.end(); ++i )
	{
		int nEstimation = (*i)->GetEstimation();
		if ( nEstimation > nMaxEstimation )
		{
			nMaxEstimation = nEstimation;
			pRes = *i;
		}
	}
	//
	ASSERT( pRes != 0 );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//
REGISTER_SAVELOAD_CLASS( 0x50392110, CRecognizer );
REGISTER_SAVELOAD_CLASS( 0x50392111, CRecognitionClass );
REGISTER_SAVELOAD_CLASS( 0x50392112, CRecognitionAttribute );