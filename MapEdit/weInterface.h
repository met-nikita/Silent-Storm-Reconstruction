#ifndef __WEINTERFACE_H_
#define __WEINTERFACE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\Main\wInterface.h"
#include "..\Main\wInterfaceVisitors.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CFinalElement;
	class CRectangle;
	class CUnit;
	class CModel;
}
namespace NAI
{
	class IAIMap;
}
namespace NBuilding
{
	struct SProjectedSpot;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class IEditorWorld: public IWorld
{
public:
	virtual int GetWorldID() const = 0;
	virtual SFBTransform GetTerrainTransform( float x, float y ) = 0;
	virtual SFBTransform GetTerrainTransform( const SFBTransform &posObj ) = 0;

	virtual const STerrainInfo& GetTerrainInfo() = 0;
	virtual bool GetTerrAlign() const = 0;

	virtual void InvalidateTerrainGeometryRect( const CTRect<float> &sRect ) = 0;
	virtual void InvalidateTerrainTextureRect( const CTRect<float> &sRect ) = 0;
	virtual void InvalidateTerrainGrassRect( const CTRect<float> &sRect ) = 0;
	virtual void UpdateTerrainSpots( const vector<NBuilding::SProjectedSpot> &spots ) = 0;
	virtual void UpdateObject( int nDBObjectID ) = 0;
	virtual void UpdateSubTemplate( int nUserID ) = 0;
	virtual void UpdateUnit( int nDBUnitID ) = 0;
	virtual void UpdateWallSpot( int nSpotID ) = 0;
	virtual void UpdateWaypoints() = 0;
	virtual void UpdateBuilding() = 0;
	virtual void UpdateFakeSolids() = 0;
	virtual void OpenObject( int nDBObjectID ) = 0;
	virtual void ResetBuilding() = 0;
	virtual int  GetMaxFloor() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IEditorObject: public IVisObj
{
public:
	static IEditorObject* Create( IEditorWorld *pWorld, NDb::CFinalElement *pFin, int nCutFloor, CFuncBase<STime> *pTime );

	virtual NDb::CFinalElement* GetDBElement() const = 0;
	virtual NDb::CModel* GetModel() const = 0;
	virtual void Update( int nCutFloor ) = 0;
	virtual void UpdateCutFloor( int nCutFloor ) = 0;
	virtual void ToggleOpen() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IEditorSubTemplate: public IVisObj
{
public:
	static IEditorSubTemplate* Create( IEditorWorld *pWorld, NDb::CRectangle *pRect, int nCutFloor );

	virtual int GetUserID() const = 0;
	virtual NDb::CRectangle* GetDBRect() const = 0;
	virtual void Update( int nCutFloor, vector<CTRect<float> > *pUpdatedRects ) = 0;
	virtual bool GetOccupiedRect( CTRect<float> *pRes ) const = 0;
	virtual void ShowInfo( bool bShow ) = 0;
	virtual int  GetMaxFloor() const = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class IEditorUnit: public CUnit
{
public:
	static IEditorUnit* Create( IEditorWorld *pWorld, NDb::CUnit *pUnit, const SFBTransform &pos );

	virtual NDb::CUnit* GetDBUnit() = 0;
	virtual void Update() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __WEINTERFACE_H_
