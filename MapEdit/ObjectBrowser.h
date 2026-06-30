#ifndef __OBJECTBROWSER_H_
#define __OBJECTBROWSER_H_
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "PropMapTypedef.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class IObjectBrowser: public CObjectBase
{
public:
	virtual void AddProperty( int nColumn, const string &szPropertyPath, IObjectBrowser *pSubProperties = 0 ) = 0;
	virtual void GetProperties( vector<CPropMap> *pProps, int nObjectID, int nVariantID = -1, const unordered_map<int, int> &propVariants = unordered_map<int, int>() ) = 0;
	virtual int  GetRootObjectTree() const = 0;

	static IObjectBrowser* Create( int nRootObjectTree );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// For example:
// 
// pb1 = Create( IDC_CONTAINERS_TREE );
// pb1->AddProperty( "ModelID\Material0" );
//
// pb2 = Create( IDC_OBJECTS_TREE );
// pb2->AddProperty( "Model0", pb1 );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __OBJECTBROWSER_H_