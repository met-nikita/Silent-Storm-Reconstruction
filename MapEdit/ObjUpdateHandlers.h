#ifndef __OBJUPDATEHANDLERS_H_
#define __OBJUPDATEHANDLERS_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Variant.h"
#include "ObjectMgr.h"
#include "ObjectDB.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataObject.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataTerrain.h"
#include "..\Input\Bind.h"
#include "..\Main\iMain.h"
#include "..\Main\meLayers.h"
#include "UserSettingsSetup.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
extern CVec3 GetColor( DWORD dwColor );
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUpdateObject: public COnSetValue
{
	OBJECT_BASIC_METHODS(CUpdateObject)
public:
	virtual bool OnSetValue( int nObjectID, const string &szProperty, CVariant val ) 
	{ 
		NDb::CFinalElement *pFin = NDb::GetFinalElement( nObjectID );
		if ( pFin )
		{
			if ( szProperty == "PosX" )
				pFin->ptPos.x = val;
			else if ( szProperty == "PosY" )
				pFin->ptPos.y = val;
			else if ( szProperty == "Floor" )
				pFin->nFloor = val;
			else if ( szProperty == "DeltaZ" )
				pFin->fDZ = val;
			else if ( szProperty == "ModelID" )
				pFin->pObject = NDb::GetPlacableObject( val );
			else if ( szProperty == "Rotation" )
				pFin->fRotation = val;
			else if ( szProperty == "Lightmap" )
				pFin->bLightmap = val;
			else if ( szProperty == "OpenObject" )
				pFin->bOpen = val;
			else if ( szProperty == "ScaleX" )
				pFin->ptScale.x = val;
			else if ( szProperty == "ScaleY" )
				pFin->ptScale.y = val;
			else if ( szProperty == "ScaleZ" )
				pFin->ptScale.z = val;
			else if ( szProperty == "Power" )
				pFin->fPower = val;
			else if ( szProperty == "Radius" )
				pFin->fRadius = val;
			else if ( szProperty == "ObjStageDelta" )
				pFin->nObjStageDelta = val;
			else if ( szProperty == "ObjRadius" )
				pFin->fObjRadius = val;
			else if ( szProperty == "PointLight" )
				pFin->vLightCr = GetColor( (int)val );
			else if ( szProperty == "LightRadius" )
				pFin->fLightRadius = val;
			else if ( szProperty == "LightPosX" )
				pFin->ptLightPos.x = val;
			else if ( szProperty == "LightPosY" )
				pFin->ptLightPos.y = val;
			else if ( szProperty == "LightPosZ" )
				pFin->ptLightPos.z = val;
			else if ( szProperty == "LightFlareRadius" )
				pFin->fFlareRadius = val;
			else if ( szProperty == "LightFlareTexture" )
				pFin->pFlareTexture = NDb::GetTexture( val );
			else if ( szProperty == "FlarePosX" )
				pFin->ptFlarePos.x = val;
			else if ( szProperty == "FlarePosY" )
				pFin->ptFlarePos.y = val;
			else if ( szProperty == "FlarePosZ" )
				pFin->ptFlarePos.z = val;
			else if ( szProperty == "LightParam" )
				pFin->szLightParams = (string)val;
			else if ( szProperty == "ObjectPhase" )
				pFin->nObjectPhase = (int)val;
			else if ( szProperty == "Grenade" )
				pFin->pGrenade = NDb::GetRPGGrenade( val );
			else if ( szProperty == "Armed" )
				pFin->bArmed = val;
			// PassageObjects
		}
		GetUserSettingsSetup().SetSelectedBrushID( LID_OBJECTS, nObjectID );
		NInput::PostEvent( "update_object" );
		NMainLoop::StepApp(true, true);
		return true; 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUpdateUnit: public COnSetValue
{
	OBJECT_BASIC_METHODS(CUpdateUnit)
public:
	virtual bool OnSetValue( int nObjectID, const string &szProperty, CVariant val ) 
	{
		NDb::CUnit *pUnit = NDb::GetUnit( nObjectID );
		if ( pUnit )
		{
			if ( szProperty == "PosX" )
				pUnit->ptPos.x = val;
			else if ( szProperty == "PosY" )
				pUnit->ptPos.y = val;
			else if ( szProperty == "Floor" )
				pUnit->nFloor = val;
			else if ( szProperty == "MonsterID" )
				pUnit->pMonster = NDb::GetPers( val );
			else if ( szProperty == "Rotation" )
				pUnit->fRotation = val;
			else if ( szProperty == "ClueSlot" )
				pUnit->bClueSlot = val;
			else if ( szProperty == "ClueInventorySlot" )
				pUnit->bClueInventorySlot = val;
//			else if ( szProperty == "PlayerID" )
//				pUnit->pPlayer = NDb::GetPla;
		}
		GetUserSettingsSetup().SetSelectedBrushID( LID_UNITS, nObjectID );
		NInput::PostEvent( "update_unit" );
		NMainLoop::StepApp(true, true);
		return true; 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUpdateSubTemplate: public COnSetValue
{
	OBJECT_BASIC_METHODS(CUpdateSubTemplate)
public:
	virtual bool OnSetValue( int nObjectID, const string &szProperty, CVariant val ) 
	{
		NDb::CRectangle *pRect = NDb::GetRectangle( nObjectID );
		if ( pRect )
		{
			if ( szProperty == "Floor" )
				pRect->nFloor = val;
			else if ( szProperty == "TemplateLink" )
			{
				pRect->pTemplate = NDb::GetTemplate( val );
				if ( IsValid( pRect->pTemplate ) )
				{
					static CObjectDB db;
					pRect->fWidth = pRect->pTemplate->nWidth;
					pRect->fHeight = pRect->pTemplate->nHeight;
					db.SetValue( CVariant::VT_FLOAT, "Rects", "Width", nObjectID, pRect->fWidth );
					db.SetValue( CVariant::VT_FLOAT, "Rects", "Height", nObjectID, pRect->fHeight );
				}
			}
			else if ( szProperty == "Rotation" )
				pRect->fRotation = val;
			else if ( szProperty == "DeltaZ" )
				pRect->fDZ = val;
			if ( szProperty == "CenterX" )
				pRect->ptCenter.x = val;
			else if ( szProperty == "CenterY" )
				pRect->ptCenter.y = val;
		}
		GetUserSettingsSetup().SetSelectedBrushID( LID_SUBTEMPLATES, nObjectID );
		NInput::PostEvent( "update_subtemplate" );
		NMainLoop::StepApp(true, true);
		return true; 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUpdateTerrSpot: public COnSetValue
{
	OBJECT_BASIC_METHODS(CUpdateTerrSpot)
public:
	virtual bool OnSetValue( int nObjectID, const string &szProperty, CVariant val ) 
	{
		NDb::CRndTerrainSpot *pSpot = NDb::GetRndTerrainSpot( nObjectID );
		if ( pSpot )
		{
			if ( szProperty == "PosX" )
				pSpot->ptPos.x = val;
			else if ( szProperty == "PosY" )
				pSpot->ptPos.y = val;
			else if ( szProperty == "MaterialID" )
				pSpot->pSpot = NDb::GetSpot( val );
			else if ( szProperty == "Rotation" )
				pSpot->nRotation = val;
			else if ( szProperty == "SizeX" )
				pSpot->ptSize.x = val;
			else if ( szProperty == "SizeY" )
				pSpot->ptSize.y = val;
			else if ( szProperty == "Priority" )
				pSpot->nPriority = val;
		}
		NInput::PostEvent( "update_terrain" );
		NInput::PostEvent( "update_terrspot" );
		NMainLoop::StepApp(true, true);
		return true; 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUpdateWallSpot: public COnSetValue
{
	OBJECT_BASIC_METHODS(CUpdateWallSpot)
public:
	virtual bool OnSetValue( int nObjectID, const string &szProperty, CVariant val );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSetupWallSpot: public COnObjectSetup
{
	OBJECT_BASIC_METHODS(CSetupWallSpot)
public:
	virtual CVariant OnObjectSetup( int nObjectID, const string &szProperty );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUpdateWaypoint: public COnSetValue
{
	OBJECT_BASIC_METHODS(CUpdateWaypoint)
public:
	virtual bool OnSetValue( int nObjectID, const string &szProperty, CVariant val ) 
	{
		NDb::CWaypoint *pW = NDb::GetWaypoint( nObjectID );
		if ( pW )
		{
			if ( szProperty == "NameID" )
				pW->pName = NDb::GetWaypointName( val );
		}
		return true; 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __OBJUPDATEHANDLERS_H_