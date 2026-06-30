#include "StdAfx.h"
#include "MapEdit.h"
#include "Layers.h"
#include "TreeSelItemDlg.h"
#include "dbDefs.h"
#include "ItemsMgr.h"
#include "..\Input\Bind.h"
#include "..\Main\iMain.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
CBaseLayer::CBaseLayer( int nLayerID, int nLayerInd, CString szName, int nBrushesTreeID ) 
	:CLayerCtrl( nLayerID, nLayerInd, szName )
{
	pBrushes = theApp.GetResTree( nBrushesTreeID );
	int nBrush = theApp.GetProfileInt( "Layers", (string( "Brush" ) + IToA( GetLayerID() )).c_str(), -1 );
	if ( nBrush != -1 )
		activeBrush = SBrush( pBrushes->nTreeID, nBrush );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseLayer::BrowseBrush()
{
	ASSERT( pBrushes );
	
	CTreeSelItemDlg dlg( vector<SResTree>( 1, *pBrushes ), activeBrush.nTreeID, activeBrush.nItemID, this );
	
	if ( IDOK != dlg.DoModal() )
		return;
	int nTree, nItemID;
	dlg.GetSelectedItemID( &nTree, &nItemID );
	if ( nItemID <= 0 )
		return;
	activeBrush = SBrush( pBrushes->nTreeID, nItemID );
	Reset();
	theApp.WriteProfileInt( "Layers", (string( "Brush" ) + IToA( GetLayerID() )).c_str(), nItemID );
	
	MSG msg;
	while( PeekMessage( &msg, 0, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE ) )
		;		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseLayer::Reset()
{
	ASSERT( pBrushes );
	COLORREF cr = GetSysColor( COLOR_BTNFACE );
	int nSizeX = 1;
	int nSizeY = 1;
	int fThickness = 0.1f;
	const CPropMap *pProps = pBrushes->pItemsTree->GetPropList( activeBrush.nItemID );
	if ( pProps )
	{
		CPropMap::const_iterator it = pProps->find( "UserColor" );
		CPropMap::const_iterator itx = pProps->find( "SizeX" );
		CPropMap::const_iterator ity = pProps->find( "SizeY" );
		CPropMap::const_iterator itw = pProps->find( "Thickness" );
		if ( it  != pProps->end() ) cr = int( it->second->GetValue() );
		if ( itx != pProps->end() ) nSizeX = 2 * int( itx->second->GetValue() );
		if ( ity != pProps->end() ) nSizeY = 2 * int( ity->second->GetValue() );
		pBrushes->pItemsTree->ReleasePropList( pProps );
	}
	activeBrush = SBrush( pBrushes->nTreeID, activeBrush.nItemID, nSizeX, nSizeY, fThickness, cr );
	const char *pszName = pBrushes->pItemsTree->GetItemName( activeBrush.nItemID );
	SetBrush( cr, pszName ? pszName : "..." );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBaseLayer::CanDraw() const
{
	return IsVisible() && IsActive();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTilesLayer::CTilesLayer() : CBaseLayer( LID_TILES, 0, "Texture", IDC_TERRAINTILES_TREE )
{
	bImageShift = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSpotsLayer::CSpotsLayer( int nLayerInd ) : CBaseLayer( LID_SPOTS, nLayerInd, "Spots", IDC_SPOTS_TREE )
{
	SetLayerName( "Spots layer " + IToA( nLayerInd ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSpotsLayer::OnVisible()
{
	NInput::PostEvent( "update_terrspot" );
	NMainLoop::StepApp( true, true );
	CBaseLayer::OnVisible();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSpotsLayer::BrowseBrush()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
