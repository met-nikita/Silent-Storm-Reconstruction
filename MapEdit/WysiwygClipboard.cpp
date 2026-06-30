#include "StdAfx.h"
#include "WysiwygClipboard.h"
#include "..\Main\BuildingInfo.h"
#include "..\Misc\BasicShare.h"
#include "..\MapEdit\FinDBCmd.h"
#include "..\MapEdit\RectsDBCmd.h"
#include "..\MapEdit\UnitDB.h"
#include "..\MapEdit\ObjectMgr.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataObject.h"
#include "..\DBFormat\DataRPG.h"
#include "WysiwygUndo.h"
#include "MEUserSettings.h"
#include "..\Input\Bind.h"
#include "iWysiwyg.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	extern CBasicShare<int, NBuilding::CBuildInfoLoader> shareBuildings;
}
namespace NWysiwyg
{
	extern int AddTerrSpotDB( int nVarID, const NBuilding::SProjectedSpot &spot );
}
namespace NGfx
{
	HWND GetHWND();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static const char szCF_NAME[] = "A5_WYSIWYG_DATA";
static int CF_WYSIWYGDATA = 0;
const int FRAGMENT_SIZE = sizeof( NBuilding::SBuildFragment );
static SRand rnd;
////////////////////////////////////////////////////////////////////////////////////////////////////
static struct SClpBuffer
{
	ZDATA
	int nActiveFloor;
	int nSelectionMinFloor;
	vector<NBuilding::SBuildFragment> frags;
	vector<int> objects; // nFinalElementID
	vector<NBuilding::SProjectedSpot> terrSpots;
	vector<NBuilding::SProjectedSpot> wallSpots;
	vector<NBuilding::SLadder> ladders;
	vector<int> units; // nDBUnitID
	vector<int> subtemplates; // nDBRectID
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nActiveFloor); f.Add(3,&nSelectionMinFloor); f.Add(4,&frags); f.Add(5,&objects); f.Add(6,&terrSpots); f.Add(7,&wallSpots); f.Add(8,&ladders); f.Add(9,&units); f.Add(10,&subtemplates); return 0; }

	void Clear()
	{
		frags.clear();
		objects.clear();
		terrSpots.clear();
		wallSpots.clear();
		ladders.clear();
		units.clear();
		subtemplates.clear();
		nSelectionMinFloor = 10;
	}
	SClpBuffer(): nActiveFloor(0), nSelectionMinFloor(10) {}
} clpBuffer;
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetClipboardData( int nActiveFloor )
{
	if ( !::OpenClipboard( NGfx::GetHWND() ) )
    return;
  // Remove the current Clipboard contents
	if( !::EmptyClipboard() )
	{
		DWORD dwError = GetLastError();
		::CloseClipboard();
    return;
	}
	if ( 0 == CF_WYSIWYGDATA )	
	{
		CF_WYSIWYGDATA = ::RegisterClipboardFormat( szCF_NAME );
		if ( 0 == CF_WYSIWYGDATA )
		{
			::CloseClipboard();
			return;
		}
	}
	//
	CMemoryStream memstream;
	{
		clpBuffer.nActiveFloor = nActiveFloor;
		CStructureSaver saver( memstream, CStructureSaver::WRITE );
		saver.Add( 1, &clpBuffer );
	}
	//
	HGLOBAL hGlobal = ::GlobalAlloc( GMEM_SHARE|GMEM_FIXED, memstream.GetSize() );
	void *pClp = (void*)::GlobalLock( hGlobal );
	if ( !pClp )
	{
		::CloseClipboard();
		return;
	}
	//
	memcpy( pClp, memstream.GetBuffer(), memstream.GetSize() );
	//Put reference to global memory into clipboard
	::GlobalUnlock( hGlobal );

  // For the appropriate data formats...
	if ( ::SetClipboardData( CF_WYSIWYGDATA, hGlobal ) == NULL )
  {
		::CloseClipboard();
    return;
  }
	clpBuffer.Clear();
  //
	::CloseClipboard();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
extern void FixRoomMapSize( NBuilding::CBuildInfo *pInfo );
extern void FixCellarSize( NBuilding::CBuildInfo *pInfo, int nX, int nY );
////////////////////////////////////////////////////////////////////////////////////////////////////
static void InsertFragments( SForceSelection *pSel, NBuilding::CBuildInfo *pInfo, const vector<NBuilding::SBuildFragment> &frags, int nWorldID, int nDFloor )
{
	ASSERT( pInfo && pSel );
	NDb::CTemplVariant *pVar = NDb::GetTemplVariant( nWorldID );
	if ( !pVar || !IsValid( pVar->pTemplate ) )
		return;
	if ( !frags.empty() )
	{
		pInfo->nMaxX = pVar->pTemplate->nWidth;
		pInfo->nMaxY = pVar->pTemplate->nHeight;
	}
	FixRoomMapSize( pInfo );
	FixCellarSize( pInfo, pVar->pTemplate->nWidth, pVar->pTemplate->nHeight );
	//
	for ( int i = 0; i < frags.size(); ++i )
	{
		NBuilding::SBuildFragment fr = frags[i];
		NDb::CTConstructionPart *pTCP = NDb::GetTConstructionPart( fr.nConstructionPartID );
		if ( !pTCP )
			continue;
		CPtr<NDb::CConstructionPart> pCP = pTCP->CreateConstructionPart( &rnd );
		if ( !IsValid( pCP ) )
			continue;
		fr.nID = pInfo->CreateNextFragmentID();
		fr.ptPos.z += nDFloor;
		if ( pCP->nSizeY == 0 )
		{
			pSel->fragsUserIDs.push_back( pInfo->wallFragments.size() | 0x40000000 );
			pInfo->wallFragments.push_back( fr );
		}
		else
		{
			pSel->fragsUserIDs.push_back( pInfo->solidFragments.size() );
			pInfo->solidFragments.push_back( fr );
			CheckLayerExistence( fr.nFragmentID );
		}
		NMapEditor::PushUndoCmd( CreateFragmentSelUndo( CWysiwygUndo::UA_INSERT, nWorldID, &fr, 0 ) );
		int nf = fr.ptPos.z + 0.5f;
		pInfo->nMaxFloor = Max( pInfo->nMaxFloor, nf );
		pInfo->nMinFloor = Min( pInfo->nMinFloor, nf );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void InsertObjects( SForceSelection *pSel, const vector<int> &objects, int nWorldID, int nDFloor )
{
	ASSERT( pSel );
	static CFinPosDB db;
	NDb::CTemplVariant *pVar = NDb::GetTemplVariant( nWorldID );
	if ( !pVar || !IsValid( pVar->pTemplate ) )
		return;
	float nWidth = pVar->pTemplate->nWidth;
	float nHeight = pVar->pTemplate->nHeight;
	CObjectMgr *pMgr = GetObjectMgr( BT_OBJECT );
	CObjectMgr *pSMgr = GetObjectMgr( (EBrushType)BT_SCALABLEOBJECT );
	CObjectMgr *pEMgr = GetObjectMgr( (EBrushType)BT_EXPLOSION );
	CObjectMgr *pPMgr = GetObjectMgr( (EBrushType)BT_PASSAGEOBJECT );
	CObjectMgr *pWDMgr = GetObjectMgr( (EBrushType)BT_WINDOWDOOR );
	CObjectMgr *pMNMgr = GetObjectMgr( (EBrushType)BT_MINE );
	//
	for ( int i = 0; i < objects.size(); ++i )
	{
		const int nSrcID = objects[i];
		NDb::CFinalElement *pF = NDb::GetFinalElement( nSrcID );
		if ( !pF )
			continue;
		int nID = db.Insert( nWorldID, pF->pObject->GetRecordID() );
		if ( nID > 0 )
		{
			CPropMap props;
			if ( pMgr )	 pMgr->MergeWith( &props, nSrcID );
			if ( pSMgr ) pSMgr->MergeWith( &props, nSrcID );
			if ( pEMgr ) pEMgr->MergeWith( &props, nSrcID );
			if ( pPMgr ) pPMgr->MergeWith( &props, nSrcID );
			if ( pWDMgr ) pWDMgr->MergeWith( &props, nSrcID );
			if ( pMNMgr ) pMNMgr->MergeWith( &props, nSrcID );

//			pF->ptPos.x = Clamp( pF->ptPos.x, 0.0f, nWidth ); // выравнивается вместе со всем селекшеном 
//			pF->ptPos.y = Clamp( pF->ptPos.y, 0.0f, nHeight );
			db.SetPos( nID, pF->ptPos, pF->fDZ, pF->nFloor + nDFloor, pF->fRotation );

			if ( pMgr )	pMgr->SetObjectProps( nID, &props );
			if ( pSMgr ) pSMgr->SetObjectProps( nID, &props );
			if ( pEMgr ) pEMgr->SetObjectProps( nID, &props );
			if ( pPMgr ) pPMgr->SetObjectProps( nID, &props );
			if ( pWDMgr ) pWDMgr->SetObjectProps( nID, &props );
			if ( pMNMgr ) pMNMgr->SetObjectProps( nID, &props );

/*
			db.SetScale( nID, pF->ptScale.x, pF->ptScale.y, pF->ptScale.z );
			db.SetOpen( nID, pF->bOpen );
			db.SetLightmap( nID, pF->bLightmap );
			CVec3 cr = 255 * pF->vLightCr;
			int nColor = (BYTE( cr.x ) << 16) + (BYTE( cr.y ) << 8) + BYTE( cr.z );
			int nFlareID = IsValid( pF->pFlareTexture ) ?  pF->pFlareTexture->GetRecordID() : 0;
			db.SetLight( nID, nColor, pF->ptLightPos, pF->fLightRadius, pF->fFlareRadius, nFlareID );
*/
			pSel->objectIDs.push_back( nID );
			NMapEditor::PushUndoCmd( CreateObjSelectionUndo( CWysiwygUndo::UA_INSERT, pF, 0, nID ) );
		}
	}
	if ( !objects.empty() )
	{
		//Sleep( 10 );
		//NDatabase::Refresh<NDb::CFinalElement>();
		for ( int i = 0; i < pSel->objectIDs.size(); ++i )
			NDb::GetFinalElement( pSel->objectIDs[i] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void InsertTerrSpots( SForceSelection *pSel, const vector<NBuilding::SProjectedSpot> &spots, int nWorldID )
{
	for ( int i = 0; i < spots.size(); ++i )
	{
		int nID = NWysiwyg::AddTerrSpotDB( nWorldID, spots[i] );
		if ( nID > 0 )
			pSel->terrSpotsIDs.push_back( nID );
	}
	if ( !spots.empty() )
	{
		Sleep( 30 );
		NDatabase::Refresh<NDb::CRndTerrainSpot>();
		for ( int i = 0; i < pSel->terrSpotsIDs.size(); ++i )
			NMapEditor::PushUndoCmd( CreateTerrSpotUndo( CWysiwygUndo::UA_INSERT, NDb::GetRndTerrainSpot( pSel->terrSpotsIDs[i] ), 0 ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void InsertWallSpots( SForceSelection *pSel, const vector<NBuilding::SProjectedSpot> &spots, int nWorldID, int nDFloor )
{
	for ( int i = 0; i < spots.size(); ++i )
	{
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void InsertSubTemplates( SForceSelection *pSel, const vector<int> &subtemplates, int nWorldID, int nDFloor )
{
	static CRectPosDB db;
	for ( int i = 0; i < subtemplates.size(); ++i )
	{
		NDb::CRectangle *p = NDb::GetRectangle( subtemplates[i] );
		if ( !IsValid( p ) || !IsValid( p->pTemplate ) )
			continue;
		int nID = db.Insert( nWorldID, p->pTemplate->GetRecordID() );
		if ( nID > 0 )
		{
			pSel->subtemplateIDs.push_back( nID );
			db.SetPos( nID, p->ptCenter, p->fDZ, p->nFloor + nDFloor, p->fRotation );
			NMapEditor::PushUndoCmd( CreateSubTemplateUndo( CWysiwygUndo::UA_INSERT, p, 0, nID ) );
		}
	}
	if ( !subtemplates.empty() )
	{
		Sleep( 50 );
		NDatabase::Refresh<NDb::CRectangle>();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void InsertUnits( SForceSelection *pSel, const vector<int> &units, int nWorldID, int nDFloor )
{
	static CUnitPosDB db;
	for ( int i = 0; i < units.size(); ++i )
	{
		NDb::CUnit *p = NDb::GetUnit( units[i] );
		if ( !IsValid( p ) || !IsValid( p->pMonster ) )
			continue;
		int nID = db.Insert( nWorldID, p->pMonster->GetRecordID() );
		if ( nID > 0 )
		{
			pSel->unitIDs.push_back( nID );
			db.SetPos( nID, p->ptPos.x, p->ptPos.y, p->nFloor + nDFloor, p->fRotation );
			NMapEditor::PushUndoCmd( CreateUnitUndo( CWysiwygUndo::UA_INSERT, p, 0, nID ) );
		}
	}
	if ( !units.empty() )
	{
		Sleep( 10 );
		NDatabase::Refresh<NDb::CUnit>();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void PasteClipboard( SForceSelection *pSelectionBuf, int *pnSelectionMinFloor, int nWorldID, int nActiveFloor )
{
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pLoader = NGScene::shareBuildings.Get( nWorldID );
	pLoader.Refresh();
	NBuilding::CBuildInfo *pInfo = pLoader->GetValue();
	if ( !pInfo )
		return;
	//
	if ( !::IsClipboardFormatAvailable(CF_WYSIWYGDATA) )
      return; 
	if ( !::OpenClipboard( NGfx::GetHWND() ) )
      return; 

	HANDLE hglb = ::GetClipboardData(CF_WYSIWYGDATA);
  if (hglb != NULL) 
  { 
		int nSize = ::GlobalSize( hglb );
		void *pClp = (void*)::GlobalLock( hglb );

		CFixedMemStream memstream( pClp, nSize );
		CStructureSaver loader( memstream, CStructureSaver::READ );
		SClpBuffer buf;

		loader.Add( 1, &buf );
		int nDFloor = nActiveFloor - buf.nActiveFloor;
		*pnSelectionMinFloor = buf.nSelectionMinFloor;

		NMapEditor::BeginUndoList();
			InsertFragments( pSelectionBuf, pInfo, buf.frags, nWorldID, nDFloor );
			InsertObjects( pSelectionBuf, buf.objects, nWorldID, nDFloor );
			InsertTerrSpots( pSelectionBuf, buf.terrSpots, nWorldID );
			InsertWallSpots( pSelectionBuf, buf.wallSpots, nWorldID, nDFloor );
			InsertSubTemplates( pSelectionBuf, buf.subtemplates, nWorldID, nDFloor );
			InsertUnits( pSelectionBuf, buf.units, nWorldID, nDFloor );
		NMapEditor::EndUndoList();

		pSelectionBuf->nWorldID = nWorldID;
		::GlobalUnlock( hglb );
	}
	::CloseClipboard();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void AddFragmentToClipboardBuffer( const NBuilding::SBuildFragment &fr )
{
	clpBuffer.frags.push_back( fr );
	clpBuffer.nSelectionMinFloor = Min( clpBuffer.nSelectionMinFloor, (int)floor( fr.ptPos.z ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void AddObjectToClipboardBuffer( int nObjectID )
{
	clpBuffer.objects.push_back( nObjectID );
	NDb::CFinalElement *pF = NDb::GetFinalElement( nObjectID );
	if ( !pF )
		return;
	clpBuffer.nSelectionMinFloor = Min( clpBuffer.nSelectionMinFloor, pF->nFloor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void AddTerrSpotToClipboardBuffer( const NBuilding::SProjectedSpot &spot )
{
	clpBuffer.terrSpots.push_back( spot );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void AddLadderToClipboardBuffer( const NBuilding::SLadder &ladder )
{
	clpBuffer.ladders.push_back( ladder );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void AddUnitToClipboardBuffer( int nDBUnitID )
{
	clpBuffer.units.push_back( nDBUnitID );
	NDb::CUnit *pU = NDb::GetUnit( nDBUnitID );
	if ( !pU )
		return;
	clpBuffer.nSelectionMinFloor = Min( clpBuffer.nSelectionMinFloor, pU->nFloor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void AddSubTemplateToClipboardBuffer( int nDBRectID )
{
	clpBuffer.subtemplates.push_back( nDBRectID );
	NDb::CRectangle *pR = NDb::GetRectangle( nDBRectID );
	if ( !pR )
		return;
	clpBuffer.nSelectionMinFloor = Min( clpBuffer.nSelectionMinFloor, pR->nFloor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CheckLayerExistence( int nLayerID )
{
	vector<int> layers;
	GetUserSettings().GetVisibleLayers( &layers );
	if ( find( layers.begin(), layers.end(), nLayerID ) == layers.end() )
		NInput::PostEvent( "update_layers" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////