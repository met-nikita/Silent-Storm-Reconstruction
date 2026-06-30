#include "StdAfx.h"
#include "dbDefs.h"
#include "ObjectMgr.h"
#include "ObjUpdateHandlers.h"
#include "CtrlObjectInspector.h"
#include "iWysiwyg.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef unordered_map<int, CObj<CObjectMgr> > CObjectMgrMap;
static CObjectMgrMap objectMgrMap;
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetupObjects()
{
	{
		CObjectMgr *pMgr = new CObjectMgr( "Units" );
		CUpdateUnit *pHandler = new CUpdateUnit;
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "MonsterID", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddProperty( CVariant::VT_STR, DT_STR, "Name", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "PosX", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "PosY", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "Floor", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Rotation", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddProperty( CVariant::VT_BOOL, DT_BOOL, "ClueSlot", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddProperty( CVariant::VT_BOOL, DT_BOOL, "ClueInventorySlot", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "Player", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "RelativeLevel", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "Group", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddProperty( CVariant::VT_STR, DT_COMBO, "Pose", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddProperty( CVariant::VT_STR, DT_COMBO, "Logic", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "RoamingRadius", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddProperty( CVariant::VT_BOOL, DT_BOOL, "FearUseToHit", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddProperty( CVariant::VT_BOOL, DT_BOOL, "FearUseToHit", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "GuardAnimation", pHandler, IDC_RPG_PERS_TREE );
		pMgr->AddString( "Pose", "Stand" );
		pMgr->AddString( "Pose", "Crouch" );
		pMgr->AddString( "Pose", "Crawl" );
		pMgr->AddString( "Logic", "Guard" );
		pMgr->AddString( "Logic", "Sentry" );
		pMgr->AddString( "Logic", "Roaming" );
		pMgr->AddString( "Logic", "Fear" );
		pMgr->SetRelation( "MonsterID", IDC_RPG_PERS_TREE );
		pMgr->SetRelation( "Group", IDC_UNITGROUPS_TREE );
		pMgr->SetRelation( "GuardAnimation", IDC_ANIMATIONS_TREE );
		objectMgrMap[BT_UNIT] = pMgr;
	}
	{
		CObjectMgr *pMgr = new CObjectMgr( "FinalElements" );
		CUpdateObject *pHandler = new CUpdateObject;
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "ModelID", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_STR, DT_STR, "Name", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PosX", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PosY", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "Floor", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "DeltaZ", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Rotation", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Lightmap", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_COMBO, "ObjectPhase", pHandler, IDC_PLACABLE_TREE );
		pMgr->SetRelation( "ModelID", IDC_PLACABLE_TREE );
		pMgr->AddString( "ObjectPhase", "0" );
		pMgr->AddString( "ObjectPhase", "1" );
		pMgr->AddString( "ObjectPhase", "2" );
		pMgr->AddString( "ObjectPhase", "3" );
		//
		const int PLIGHT_GRP = 10212;
		pMgr->AddProperty( CVariant::VT_INT, DT_COLOR, "PointLight", pHandler, PLIGHT_GRP );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "LightPosX", pHandler, PLIGHT_GRP );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "LightPosY", pHandler, PLIGHT_GRP );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "LightPosZ", pHandler, PLIGHT_GRP );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "LightRadius", pHandler, PLIGHT_GRP );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "LightFlareRadius", pHandler, PLIGHT_GRP );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "LightFlareTexture", pHandler, PLIGHT_GRP );
		pMgr->AddProperty( CVariant::VT_STR, DT_COMBO, "LightParam", pHandler, PLIGHT_GRP );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "FlarePosX", pHandler, PLIGHT_GRP );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "FlarePosY", pHandler, PLIGHT_GRP );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "FlarePosZ", pHandler, PLIGHT_GRP );
		pMgr->SetRelation( "LightFlareTexture", IDC_TEXTURES_TREE );
		pMgr->AddString( "LightParam", "Night" );
		pMgr->AddString( "LightParam", "Day" );
		pMgr->AddString( "LightParam", "Always" );

		objectMgrMap[BT_OBJECT] = pMgr;
	}
	{
		CObjectMgr *pMgr = new CObjectMgr( "FinalElements" );
		CUpdateObject *pHandler = new CUpdateObject;
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "ScaleX", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "ScaleY", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "ScaleZ", pHandler, IDC_PLACABLE_TREE );
		objectMgrMap[BT_SCALABLEOBJECT] = pMgr;
	}
	{
		CObjectMgr *pMgr = new CObjectMgr( "FinalElements" );
		CUpdateObject *pHandler = new CUpdateObject;
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Power", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Radius", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "ObjStageDelta", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "ObjRadius", pHandler, IDC_PLACABLE_TREE );
		objectMgrMap[BT_EXPLOSION] = pMgr;
	}
	{
		CObjectMgr *pMgr = new CObjectMgr( "FinalElements" );
		CUpdateObject *pHandler = new CUpdateObject;
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "PassageZoneID", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "PassageObjectID", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "APRadius", pHandler, IDC_PLACABLE_TREE );
		objectMgrMap[BT_PASSAGEOBJECT] = pMgr;
	}
	{
		CObjectMgr *pMgr = new CObjectMgr( "FinalElements" );
		CUpdateObject *pHandler = new CUpdateObject;
		pMgr->AddProperty( CVariant::VT_BOOL, DT_BOOL, "OpenObject", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "Grenade", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Power", pHandler, IDC_PLACABLE_TREE );
		pMgr->SetRelation( "Grenade", IDC_RPG_GRENADES_TREE );
		objectMgrMap[BT_WINDOWDOOR] = pMgr;
	}
	{
		CObjectMgr *pMgr = new CObjectMgr( "FinalElements" );
		CUpdateObject *pHandler = new CUpdateObject;
		pMgr->AddProperty( CVariant::VT_BOOL, DT_BOOL, "Armed", pHandler, IDC_PLACABLE_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Power", pHandler, IDC_PLACABLE_TREE );
		objectMgrMap[BT_MINE] = pMgr;
	}
	{
		CObjectMgr *pMgr = new CObjectMgr( "Rects" );
		CUpdateSubTemplate *pHandler = new CUpdateSubTemplate;
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "TemplateLink", pHandler, IDC_TEMPLATE_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "Floor", pHandler, IDC_TEMPLATE_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "DeltaZ", pHandler, IDC_TEMPLATE_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "Rotation", pHandler, IDC_TEMPLATE_TREE );
		pMgr->AddProperty( CVariant::VT_STR, DT_PARAMS, "Params", pHandler, IDC_TEMPLATE_TREE );
		pMgr->SetRelation( "TemplateLink", IDC_TEMPLATE_TREE );
		objectMgrMap[BT_SUBTEMPLATE] = pMgr;
	}
	{
		CObjectMgr *pMgr = new CObjectMgr( "TerrainSpots" );
		CUpdateTerrSpot *pHandler = new CUpdateTerrSpot;
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "MaterialID", pHandler, IDC_SPOTS_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PosX", pHandler, IDC_SPOTS_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PosY", pHandler, IDC_SPOTS_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "Rotation", pHandler, IDC_SPOTS_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SizeX", pHandler, IDC_SPOTS_TREE );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SizeY", pHandler, IDC_SPOTS_TREE );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "Priority", pHandler, IDC_SPOTS_TREE );
		pMgr->SetRelation( "MaterialID", IDC_SPOTS_TREE );
		objectMgrMap[BT_TEXSPOT] = pMgr;
	}
	{
		CObjectMgr *pMgr = new CObjectMgr( "" );
		CUpdateWallSpot *pHandler = new CUpdateWallSpot;
		CSetupWallSpot *pSetup = new CSetupWallSpot;
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "MaterialID", pHandler, IDC_MATERIALS_TREE, CVariant(), false, pSetup );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PosX", pHandler, IDC_MATERIALS_TREE, CVariant(), false, pSetup );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PosY", pHandler, IDC_MATERIALS_TREE, CVariant(), false, pSetup );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "PosZ", pHandler, IDC_MATERIALS_TREE, CVariant(), false, pSetup );
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "Rotation", pHandler, IDC_MATERIALS_TREE, CVariant(), false, pSetup );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SizeX", pHandler, IDC_MATERIALS_TREE, CVariant(), false, pSetup );
		pMgr->AddProperty( CVariant::VT_FLOAT, DT_DEC, "SizeY", pHandler, IDC_MATERIALS_TREE, CVariant(), false, pSetup );
		pMgr->SetRelation( "MaterialID", IDC_MATERIALS_TREE );
		objectMgrMap[BT_WALLSPOT] = pMgr;
	}
	{
		CObjectMgr *pMgr = new CObjectMgr( "Waypoints" );
		CUpdateWaypoint *pHandler = new CUpdateWaypoint;
		pMgr->AddProperty( CVariant::VT_INT, DT_DEC, "NameID", pHandler, IDC_WAYPOINTS_TREE );
		pMgr->SetRelation( "NameID", IDC_WAYPOINTNAMES_TREE );
		objectMgrMap[BT_WAYPOINT] = pMgr;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectMgr* GetObjectMgr( EBrushType type )
{
	CObjectMgrMap::iterator i = objectMgrMap.find( type );
	if ( i == objectMgrMap.end() )
		return 0;
	return i->second;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
