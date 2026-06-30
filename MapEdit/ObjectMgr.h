#ifndef __OBJECTMGR_H_
#define __OBJECTMGR_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "PropMap.h"
#include "PropMapTypedef.h"
#include "Variant.h"

enum EBrushType;
////////////////////////////////////////////////////////////////////////////////////////////////////
class COnSetValue: public CObjectBase
{
public:
	virtual bool OnSetValue( int nObjectID, const string &szProperty, CVariant val ) { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class COnObjectSetup: public CObjectBase
{
public:
	virtual CVariant OnObjectSetup( int nObjectID, const string &szProperty ) { return CVariant(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectMgr: public CObjectBase
{
	OBJECT_BASIC_METHODS(CObjectMgr)
	string szTable;
	CPropMap pattern;
	static int nSafePropID;
public:
	CObjectMgr() {}
	CObjectMgr( const string &szTable );

	void AddProperty( int type, int viewType, const char *szPropName, COnSetValue *pOnSetVal = 0, int nGroup = CProp::PROPDEF_GROUP, const CVariant &defVal = CVariant(), 
		int bReadOnly = false, COnObjectSetup *pOnObjSetup = 0 );

	void SetRelation( const char *szPropName, int nTableID );
	void AddString( const char *szPropName, const char *pszString );
	void MergeWith( CPropMap *pProps, int nObjectID ) const;
	void SetObjectProps( int nDstObject, const CPropMap *pSrcProps ) const;

	static void Intersect( CPropMap *pDstProps, const CPropMap *pSrcProps );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const int BT_SCALABLEOBJECT = 0xBBB;
const int BT_PASSAGEOBJECT = 0xABAB;
const int BT_EXPLOSION = 0xEEE;
const int BT_WINDOWDOOR = 0xEEF;
const int BT_MINE = 0xEF0;
//const int BT_LIGHT = 0xEEF;
CObjectMgr* GetObjectMgr( EBrushType type );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __OBJECTMGR_H_