#include "StdAfx.h"
#include "MapEdit.h"
#include "PlacableItemsDlg.h"
#include "dbDefs.h"
#include "PlacableDB.h"
#include "ItemsMgr.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataObject.h"
#include "..\DBFormat\DataRPG.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
CPlacableItemsDlg::CPlacableItemsDlg( const vector<SResTree> &vResTrees )
:CTreeSelItemDlg( vResTrees )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlacableItemsDlg::MakeItemID( int *pnTree, int *pnID, int nPlacableID )
{
	NDb::CPlacableObject *p = NDb::GetPlacableObject( nPlacableID );
	if ( !p )
		return false;
	if ( IsValid( p->pObject ) )
	{
		*pnTree = IDC_OBJECTS_TREE;
		*pnID = p->pObject->GetRecordID();
	}
	else if ( IsValid( p->pRPGItem ) )
	{
		*pnTree = IDC_RPG_ITEMS_TREE;
		*pnID = p->pRPGItem->GetRecordID();
	}
	else
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlacableItemsDlg::SetSelectedItemID( int nPlaceTreeID, int nPlaceID )
{
	ASSERT( nPlaceTreeID == IDC_PLACABLE_TREE );
	NDb::CPlacableObject *p = NDb::GetPlacableObject( nPlaceID );
	if ( !p )
		return;
	if ( IsValid( p->pObject ) )
		CTreeSelItemDlg::SetSelectedItemID( IDC_OBJECTS_TREE, p->pObject->GetRecordID() );
	else if ( IsValid( p->pRPGItem ) )
		CTreeSelItemDlg::SetSelectedItemID( IDC_RPG_ITEMS_TREE, p->pRPGItem->GetRecordID() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPlacableItemsDlg::GetSelectedItemID( int *pnTree, int *pnID )
{
	CTreeSelItemDlg::GetSelectedItemID( pnTree, pnID );
	static CPlaceDB db;
	switch ( *pnTree )
	{
		case IDC_OBJECTS_TREE:
			*pnID = db.GetPlaceID( "ObjectTemplates", *pnID );
			break;
		case IDC_RPG_ITEMS_TREE:
			*pnID = db.GetPlaceID( "RPGItems", *pnID );
			break;
		default:
			ASSERT(0);
			break;
	}
	*pnTree = IDC_PLACABLE_TREE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string CPlacableItemsDlg::GetItemPath( int nPlacableTreeID, int nItemID )
{
	ASSERT( nPlacableTreeID == IDC_PLACABLE_TREE );

	int nTree, nID;
	if ( !MakeItemID( &nTree, &nID, nItemID ) )
		return "";
	const SResTree *pTree = theApp.GetResTree( nTree );
	if ( !pTree )
		return "";
	return pTree->pItemsTree->GetItemPath( nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
