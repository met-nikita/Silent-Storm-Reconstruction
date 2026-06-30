#include "StdAfx.h"
#include "MapEdit.h"
#include "dbDefs.h"
#include "WaypointDB.h"
#include "..\Main\aiWaypoint.h"
#include "MESerialize.h"
#include "..\Main\Grid.h"
#include "MEUserSettings.h"
#include "..\Misc\BasicShare.h"
#include "ItemsMgr.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
extern CBasicShare<int, NAI::CWaypointLoader> shareWaypoints;
////////////////////////////////////////////////////////////////////////////////////////////////////
int AddWaypoint2DB( int nWaypointNameID, int nVarID )
{
	const SResTree *pNames = theApp.GetResTree( IDC_WAYPOINTNAMES_TREE );
	const SResTree *pTree = theApp.GetResTree( IDC_WAYPOINTS_TREE );
	if ( !pNames || !pTree )
		return -1;
	int nID = pTree->pItemsTree->AddItem( -1, 0, pNames->pItemsTree->GetItemPath( nWaypointNameID ) );
	if ( nID <= 0 )
		return -1;
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nID );
	if ( pProps )
	{
		CPropMap::const_iterator i = pProps->find( "VariantID" );
		CPropMap::const_iterator in = pProps->find( "NameID" );
		if ( i != pProps->end() )
			i->second->SetValue( nVarID );
		if ( in != pProps->end() )
			in->second->SetValue( nWaypointNameID );
		pTree->pItemsTree->ReleasePropList( pProps );
	}
	//
	CObj<NAI::CWaypoint> pWP = new NAI::CWaypoint;
	SerializeWaypoint( pWP, nID );
	return nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SetWaypointPos( int nWaypointID, const CVec2 &ptPos, int nFloor )
{
	CDGPtr< CPtrFuncBase<NAI::CWaypoint> > pWPLoader = shareWaypoints.Get( nWaypointID );
	if ( !IsValid( pWPLoader ) )
		return false;
	pWPLoader.Refresh();
	NAI::CWaypoint *pWP = pWPLoader->GetValue();
	if ( !pWP )
		return false;
	pWP->ptPos.x = ptPos.x * FP_INV_GRID_STEP;
	pWP->ptPos.y = ptPos.y * FP_INV_GRID_STEP;
	pWP->nFloor = GetUserSettings().GetActiveFloor();
	return SerializeWaypoint( pWP, nWaypointID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////