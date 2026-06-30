#include "StdAfx.h"
#include "MapEdit.h"
#include "TemplDBCmd.h"
#include "ItemsMgr.h"
#include "TreeVDialogs.h"
#include "TemplMgr.h"
#include "..\DBFormat\DataMap.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
int AddNewTemplate( int nHintWidth, int nHintHeight )
{
	const SResTree *pTree = theApp.GetResTree( IDC_TEMPLATE_TREE );
	if ( !pTree )
		return -1;
	int nFolder = 0;
	int nTree, nItem, nVariant;
	theApp.GetActiveItem( &nTree, &nItem, &nVariant );
	if ( IDC_TEMPLATE_TREE == nTree )
		nFolder = pTree->pItemsTree->GetItemFolderID( nItem );
	//
	CAddTemplDlg dlg( pTree->pItemsTree->GetFolderPath( nFolder ) );
	dlg.m_width = nHintWidth;
	dlg.m_height = nHintHeight;
	if ( IDOK != dlg.DoModal() )
		return -1;
	int nTemplID = pTree->pItemsTree->AddItem( -1, nFolder, (LPCSTR)dlg.m_name );
	if ( nTemplID <= 0 )
		return -1;
	theTemplMgr.AddTemplate( nTemplID, dlg.m_width, dlg.m_height );
	vector<int> vars;
	pTree->pItemsTree->GetItemVariants( nTemplID, &vars );
	if ( !vars.empty() )
		return vars.front();
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetActiveTemplateVariant( int nVarID )
{
	NDb::CTemplVariant *pVar = NDb::GetTemplVariant( nVarID );
	if ( !IsValid( pVar ) || !IsValid( pVar->pTemplate ) )
		return;
	theApp.SetActiveItem( IDC_TEMPLATE_TREE, pVar->pTemplate->GetRecordID(), pVar->GetRecordID() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////