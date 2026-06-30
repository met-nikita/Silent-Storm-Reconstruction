#include "StdAfx.h"
#include "GlobalMapView.h"
#include "..\Main\ChapterInfo.h"
#include "..\Main\GlobalInfo.h"
#include "SectorCtrl.h"
#include "MapEdit.h"
#include "ChapterSectorDlg.h"
#include "Export.h"
#include "dbDefs.h"
#include "CtrlObjectInspector.h"
#include "ItemsMgr.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
CGlobalMapView::CGlobalMapView() :nGlobalMapID(-1)
{
	bDrawStartPoint = false;
	props["Scenario"] = new CChapterProp( "Scenario", 1, CVariant::VT_INT, DT_DEC, this );
	props["Scenario"]->SetGroup( IDC_SCENARIOS_TREE );
	props["Scenario"]->SetRelation( IDC_SCENARIOS_TREE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalMapView::SetGlobalMap( int nID )
{
	if ( bModified && IsValid( pInfo ) )
	{
		UpdateGlobalMapInfo();
		SerializeGlobalMap( nGlobalMapID, pInfo );
	}
	CObj<CGlobalInfoLoader> pGlobalLoader = new CGlobalInfoLoader;
	pGlobalLoader->SetKey( nID );
	CDGPtr<CPtrFuncBase<CGlobalInfo> > pLoader = pGlobalLoader;
	pLoader.Refresh();
	pInfo = pLoader->GetValue();
	nGlobalMapID = nID;
	sectors.clear();
	CVariant background, scenario;
	if ( IsValid( pInfo ) )
	{
		for ( int i = 0; i < pInfo->sectorsSet.size(); ++i )
		{
			sectors.push_back( new CSectorCtrl( CHAPTER_SIZE_X, CHAPTER_SIZE_Y, SEGMENT_LEN, pInfo->sectorsSet[i].pointsSet ) );
			sectors.back()->nTemplateID = pInfo->sectorsSet[i].nTemplate;
			sectors.back()->nDescrID = pInfo->sectorsSet[i].nDescriptionID;
			sectors.back()->eSectorType = RANDOM;
			sectors.back()->nImageID = pInfo->sectorsSet[i].nImageID;
			sectors.back()->nImX = pInfo->sectorsSet[i].vImagePos.x;
			sectors.back()->nImY = pInfo->sectorsSet[i].vImagePos.y;
			sectors.back()->SetHitAccuracy( HIT_ACCURACY );
		}
		background = pInfo->nMapID;
		scenario = pInfo->nScenarioID;
	}
	props["Background"]->SetValue( background, false );
	props["Background"]->SetOwner( SOwner( nID, -1 ) );
	props["Scenario"]->SetValue( scenario, false );
	props["Scenario"]->SetOwner( SOwner( nID, -1 ) );
	bModified = false;
	Invalidate( FALSE );
	//theApp.SetPropMap( &props );
	nBackgroundID = background;
	const SResTree *pTree = theApp.GetResTree( IDC_GLOBALMAPS_TREE );
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nID );
	if ( !pProps )
		return;
	CPropMap::const_iterator i = pProps->find( "Background" );
	if ( i != pProps->end() )
		nBackgroundID = i->second->GetValue();
	pTree->pItemsTree->ReleasePropList( pProps );
	SetBackground();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalMapView::PostNcDestroy()
{
	if ( bModified && IsValid( pInfo ) )
	{
		UpdateGlobalMapInfo();
		SerializeGlobalMap( nGlobalMapID, pInfo );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalMapView::OnContourProps()
{
	CPoint pt = GetScrollPosition() + ptRightClick;
	int iReg, iPt;
	if ( RegionHitTest( CVec2( pt.x, pt.y ), &iReg, &iPt ) )
	{
		CSectorCtrl *pS = sectors[iReg];
		if ( pS )
		{
			CChapterSectorDlg dlg( true );

			dlg.m_nTemplteID = pS->nTemplateID;
			dlg.m_nDescrID = pS->nDescrID;
			dlg.m_nImageID = pS->nImageID;
			dlg.m_nX = pS->nImX;
			dlg.m_nY = pS->nImY;
			if ( dlg.DoModal() != IDOK )
				return;
			pS->nTemplateID = dlg.m_nTemplteID;
			pS->nDescrID = dlg.m_nDescrID;
			pS->nImageID = dlg.m_nImageID;
			pS->nImX = dlg.m_nX;
			pS->nImY = dlg.m_nY;
			bModified = true;
			Invalidate();
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGlobalMapView::UpdateGlobalMapInfo()
{
	if ( !IsValid( pInfo ) )
		return;
	//
	pInfo->sectorsSet.clear();
	for ( int i = 0; i < sectors.size(); ++i )
	{
		CSectorCtrl *p = sectors[i];
		if ( !p )
			continue;
		pInfo->sectorsSet.push_back( SGlobalSector() );
		SGlobalSector &chap = pInfo->sectorsSet.back();
		chap.pointsSet = p->GetPoints();
		chap.nTemplate = p->nTemplateID;
		chap.nDescriptionID = p->nDescrID;
		chap.nImageID = p->nImageID;
		chap.vImagePos.x = p->nImX;
		chap.vImagePos.y = p->nImY;
	}
	//
	pInfo->nMapID = props["Background"]->GetValue();
	pInfo->nScenarioID = props["Scenario"]->GetValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CGlobalMapView::SerializeGlobalMap( int nID, CGlobalInfo *pInfo )
{
	if ( !pInfo )
		return false;
	try
	{
		char szPath[MAX_PATH];
		sprintf( szPath, "%sGlobals\\%d", GetExportDstDir().c_str(), nID );
		CFileStream fp;
		fp.OpenWrite( szPath );
		{
			CStructureSaver file( fp, CStructureSaver::WRITE );
			pInfo->operator&( file ) ;
		}
		return true;
	}
	catch ( ... )
	{
		return false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
