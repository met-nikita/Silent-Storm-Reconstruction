#include "StdAfx.h"
#include "ObjUpdateHandlers.h"
#include "MapEdit.h"
#include "..\Misc\BasicShare.h"
#include "..\Main\BuildingInfo.h"
/////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	extern CBasicShare<int, NBuilding::CBuildInfoLoader> shareBuildings;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NBuilding::CBuildInfo* GetBuildInfo( int nBuildingID )
{
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( nBuildingID );
	pLoader.Refresh();
	return pLoader->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NBuilding::SProjectedSpot* GetSpot( int nBuildingID, int nSpotID )
{
	NBuilding::CBuildInfo *pInfo = GetBuildInfo( nBuildingID );
	if ( !pInfo )
		return 0;
	for ( int i = 0; i < pInfo->spots.size(); ++i )
		if ( pInfo->spots[i].nID == nSpotID )
			return &pInfo->spots[i];
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline int GetResID()
{
	int nTreeID, nItemID, nVarID;
	theApp.GetActiveItem( &nTreeID, &nItemID, &nVarID );
	return nTreeID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline int GetBuildingID()
{
	int nTreeID, nItemID, nVarID;
	theApp.GetActiveItem( &nTreeID, &nItemID, &nVarID );
	return nVarID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUpdateWallSpot::OnSetValue( int nObjectID, const string &szProperty, CVariant val )
{
	if ( GetResID() != IDC_TEMPLATE_TREE )
		return CVariant();

	NBuilding::SProjectedSpot *pSpot = GetSpot( GetBuildingID(), nObjectID );
	if ( !pSpot )
		return false;

	if ( szProperty == "MaterialID" )
		pSpot->nMaterialID = val;
	else if ( szProperty == "PosX" )
	{
		if ( fabs( pSpot->ptNormal.x ) < 0.01f )
			pSpot->ptOrigin.x = val;
	}
	else if ( szProperty == "PosY" )
	{
		if ( fabs( pSpot->ptNormal.y ) < 0.01f )
			pSpot->ptOrigin.y = val;
	}
	else if ( szProperty == "PosZ" )
	{
		if ( fabs( pSpot->ptNormal.z ) < 0.01f )
			pSpot->ptOrigin.z = val;
	}
	else if ( szProperty == "Rotation" )
		pSpot->nRotation = val;
	else if ( szProperty == "SizeX" )
		pSpot->ptSize.x = val;
	else if ( szProperty == "SizeY" )
		pSpot->ptSize.y = val;
	else
	{
		ASSERT(0);
		return false;
	}
	GetUserSettingsSetup().SetSelectedBrushID( LID_WALLS, nObjectID );
	NInput::PostEvent( "update_wallspot" );
	NMainLoop::StepApp( true, true );
	NMainLoop::StepApp( true, true );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVariant CSetupWallSpot::OnObjectSetup( int nObjectID, const string &szProperty )
{
	if ( GetResID() != IDC_TEMPLATE_TREE )
		return CVariant();
	
	NBuilding::SProjectedSpot *pSpot = GetSpot( GetBuildingID(), nObjectID );
	if ( !pSpot )
		return CVariant();

	if ( szProperty == "MaterialID" )
		return pSpot->nMaterialID;
	else if ( szProperty == "PosX" )
		return pSpot->ptOrigin.x;
	else if ( szProperty == "PosY" )
		return pSpot->ptOrigin.y;
	else if ( szProperty == "PosZ" )
		return pSpot->ptOrigin.z;
	else if ( szProperty == "Rotation" )
		return pSpot->nRotation;
	else if ( szProperty == "SizeX" )
		return pSpot->ptSize.x;
	else if ( szProperty == "SizeY" )
		return pSpot->ptSize.y;
	
	ASSERT(0);
	return CVariant();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
