#include "StdAfx.h"
#include "WysiwygRoute.h"
#include "..\Main\aiWaypoint.h"
#include "WysiwygSelectionImpl.h"
#include "weInterface.h"
#include "MESerialize.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataAI.h"
#include "..\Misc\BasicShare.h"
#include "WysiwygUnitSel.h"
#include "WysiwygWaypointSel.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
extern CBasicShare<int, NAI::CUnitAIInfoLoader> shareUnits;
extern CBasicShare<int, NAI::CUnitGroupAIInfoLoader> shareUnitGroups;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWysiwygRoute: public IWysiwygRoute
{
	OBJECT_BASIC_METHODS(CWysiwygRoute)
	CPtr<CSelection> pSelection;

	int GetSelectedUnitID() const;
	int GetUnitGroup( int nUnitID ) const;
	NAI::CUnitAIInfo* GetUnitRoute( int nUnitID ) const;

public:
	CWysiwygRoute() {}
	CWysiwygRoute( CSelection *pSel ): pSelection(pSel) { ASSERT( pSelection ); }

	virtual void ShowRoute();
	virtual void SetRoute();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygRoute::ShowRoute()
{
	int nUnitID = GetSelectedUnitID();
	const NAI::CUnitAIInfo* pInfo = GetUnitRoute( nUnitID );
	if ( !pInfo || pInfo->routes.empty() )
		return;
	const NAI::SRoute &route = pInfo->routes.front();
	NDb::CTemplVariant *pVar = NDb::GetTemplVariant( pSelection->GetWorld()->GetWorldID() );
	if ( !IsValid( pVar ) )
		return;
	//
	SForceSelection sel;
	sel.nWorldID = pSelection->GetWorld()->GetWorldID();
	sel.unitIDs.push_back( nUnitID );
	for ( int i = 0; i < route.waypoints.size(); ++i )
	{
		int nName = route.waypoints[i];
		for ( int i = 0; i < pVar->waypoints.size(); ++i )
		{
			NDb::CWaypoint *pW = pVar->waypoints[i];
			if ( IsValid( pW ) && IsValid( pW->pName ) && pW->pName->GetRecordID() == nName )
			{
				sel.waypointIDs.push_back( pW->GetRecordID() );
				break;
			}
		}
	}
	//
	pSelection->Select( sel );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygRoute::SetRoute()
{
	int nUnitID = GetSelectedUnitID();
	CPtr<NAI::CUnitAIInfo> pInfo = GetUnitRoute( nUnitID );
	if ( !IsValid( pInfo ) )
		return;
	pInfo->routes.resize( 1 );
	vector<int> &waypoints = pInfo->routes.front().waypoints;
	waypoints.clear();
	//
	const vector <CPtr<CSelectedObj> > &selection = pSelection->GetSelection();
	for (int i = 0; i < selection.size(); ++i)
	{
		CDynamicCast<CWaypoint> pW(selection[i]);
		if (pW)
		{
			NDb::CWaypoint* pdbW = NDb::GetWaypoint(pW->GetSelectionID());
			if (IsValid(pdbW) && IsValid(pdbW->pName))
				waypoints.push_back(pdbW->pName->GetRecordID());
		}
	}
	//
	int nGroupID = GetUnitGroup( nUnitID );
	if ( nGroupID > 0 )
		SerializeUnitGroupAIInfo( pInfo, nGroupID );
	else
		SerializeUnitAIInfo( pInfo, nUnitID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CWysiwygRoute::GetSelectedUnitID() const
{
	const vector <CPtr<CSelectedObj> > &selection = pSelection->GetSelection();
	for (int i = 0; i < selection.size(); ++i)
	{
		CDynamicCast<CUnitSel> pU(selection[i]);
		if (pU)
			return pU->GetSelectionID();
	}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CWysiwygRoute::GetUnitGroup( int nUnitID ) const
{
	NDb::CUnit *pU = NDb::GetUnit( nUnitID );
	if ( !IsValid( pU ) || !IsValid( pU->pGroup ) )
		return -1;
	return pU->pGroup->GetRecordID();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NAI::CUnitAIInfo* CWysiwygRoute::GetUnitRoute( int nUnitID ) const
{
	if ( nUnitID < 0 )
		return 0;
	int nGroupID = GetUnitGroup( nUnitID );
	//
	CDGPtr<CPtrFuncBase<NAI::CUnitAIInfo> > pLoader;
	if ( nGroupID > 0 )
		pLoader = shareUnitGroups.Get( nGroupID );
	else
		pLoader = shareUnits.Get( nUnitID );
	pLoader.Refresh();
	NAI::CUnitAIInfo *p = pLoader->GetValue();
	if ( !p )
		p = new NAI::CUnitAIInfo;
	return p;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IWysiwygRoute* CreateWysiwygRoute( ISelection *pSelection )
{
	CDynamicCast<CSelection> pSel(pSelection);
	if (pSel)
		return new CWysiwygRoute( pSel );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
