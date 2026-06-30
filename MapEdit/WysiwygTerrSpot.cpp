#include "StdAfx.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataTerrain.h"
#include "..\Main\BuildingInfo.h"
#include "..\Main\wInterface.h"
#include "WysiwygTerrSpot.h"
#include "..\MapEdit\TerrSpotDB.h"
#include "iWysiwyg.h"
#include "WysiwygClipboard.h"
#include "wEditor.h"
#include "WysiwygUndo.h"
#include "MEUserSettings.h"
#include "..\Main\MELayers.h"

extern SDBConnection dbConnection;
static CTerrSpotDB db;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
bool UpdateTerrSpotDB( int nRecordID, int nVariantID, const NDb::CRndTerrainSpot *pSpot );
int GetMaxSpotPriority( int nVarID );
////////////////////////////////////////////////////////////////////////////////////////////////////
CTerrSpot::CTerrSpot( NDb::CRndTerrainSpot* pSpot, ISelection *pSel, int nUserID )
	:CSpot( pSel, nUserID ), pDBSpot(pSpot)
{
	ASSERT( pSpot );
	SRand rand;
	rand.Get( 1 );
	spot.nRotation = pSpot->nRotation;
	spot.nMaterialID = pSpot->pSpot->GetRecordID();
	spot.ptNormal = CVec3( 0, 0, 1 );
	spot.ptOrigin = CVec3( pSpot->ptPos.x, pSpot->ptPos.y, 0 );
	spot.ptSize = pSpot->ptSize;
	spotStartMove = spot;

	EBrushType nBrush;
	int nFragmentID;
	GetFragmentID( nUserID, &nBrush, &nFragmentID );
	ESpotType type;
	EHandle h;
	GetSpotID( nFragmentID, &type, &nSpotIndex, &h );
	bTerrAlign = true;
	bUseStartMove = true;
	//
	pRollback = new NDb::CRndTerrainSpot;
	*pRollback = *pSpot;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void FillDBSpotValues( NDb::CRndTerrainSpot *pDBSpot, const NBuilding::SProjectedSpot &spot )
{
	pDBSpot->ptPos = CVec2( spot.ptOrigin.x, spot.ptOrigin.y );
	pDBSpot->ptSize = spot.ptSize;
	pDBSpot->nRotation = spot.nRotation;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float Dist( NDb::CRndTerrainSpot *a, NDb::CRndTerrainSpot *b )
{
	return fabs( a->ptPos - b->ptPos ) + fabs( a->ptSize - b->ptSize ) + fabs( (float)a->nRotation - b->nRotation ) + fabs( (float)a->nPriority - b->nPriority );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrSpot::EndMove( bool bCancel )
{
	CSpot::EndMove( bCancel );
	if ( bCancel || Dist( pRollback, pDBSpot ) < FP_EPSILON )
	{
		FillDBSpotValues( pDBSpot, spotStartMove );
		spot = spotStartMove;
	}
	else 
	{
		FillDBSpotValues( pDBSpot, spot );
		NMapEditor::PushUndoCmd( CreateTerrSpotUndo( CWysiwygUndo::UA_CHANGE_POS, pRollback, pDBSpot ) );
		UpdateTerrSpotDB( pDBSpot->GetRecordID(), pDBSpot->pVar->GetRecordID(), pDBSpot );
	}
	vector<NBuilding::SProjectedSpot> spots( 1, *CSpot::GetSpot() );
	spots.push_back( spotStartMove );
	pWorld->UpdateTerrainSpots( spots );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CheckDBConnection()
{
	if ( !db.HasConnection() )
		db.SetConnection( &dbConnection );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool DeleteTerrSpot( int nID )
{
	CheckDBConnection();

	char buf[512];
	sprintf( buf, "SELECT * FROM TerrainSpots WHERE ID = %d", nID );
	HRESULT hr = db.Open( buf );
	if ( FAILED(hr) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	hr = db.MoveNext();
	if ( FAILED(hr) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	hr = db.Delete();
	if ( FAILED(hr) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	NDb::CRndTerrainSpot *pSpot = NDb::GetRndTerrainSpot( nID );
	if ( IsValid( pSpot ) && IsValid( pSpot->pVar ) )
	{
		for ( int i = 0; i < pSpot->pVar->terrainSpots.size(); ++i )
		{
			NDb::CRndTerrainSpot *pS = pSpot->pVar->terrainSpots[i];
			if ( IsValid( pS ) && pS->GetRecordID() == nID )
				pSpot->pVar->terrainSpots[i] = 0;
		}
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrSpot::Delete()
{
	CObj<CWysiwygUndo> pUndo = CreateTerrSpotUndo( CWysiwygUndo::UA_DELETE, pDBSpot, 0 );
	if ( !DeleteTerrSpot( pDBSpot->GetRecordID() ) )
		return false;
	NMapEditor::PushUndoCmd( pUndo );
	vector<NBuilding::SProjectedSpot> spots( 1, *CSpot::GetSpot() );
	pWorld->UpdateTerrainSpots( spots );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrSpot::DelayedDelete()
{
	return Delete();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int AddTerrSpotDB( int nVarID, const NBuilding::SProjectedSpot &spot )
{
	NDb::CSpot *pS = NDb::GetSpot( spot.nMaterialID );
	if ( !IsValid( pS ) )
		return -1;
	CheckDBConnection();

	HRESULT hr = db.Open( "SELECT * FROM TerrainSpots WHERE ID = -1" );
	if ( FAILED(hr) )
	{
		DisplayOLEDBErrorRecords( hr );
		return -1;
	}
	db.m_nVariantID = nVarID;
	db.m_nMaterialID = spot.nMaterialID;
	db.m_ptPos = CVec2( spot.ptOrigin.x, spot.ptOrigin.y );
	db.m_ptSize = spot.ptSize;
	db.m_nRotation = spot.nRotation;
	db.m_nPriority = GetMaxSpotPriority( nVarID ) + 1;

	ELayer eType;
	int nLayer;
	NBuilding::GetLayerID( GetUserSettings().GetActiveLayerID(), &eType, &nLayer );
	db.m_nLayer = eType == LID_SPOTS ? nLayer : 0;

	hr = db.Insert( 1 );
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return -1;
	}
	if ( S_OK != db.MoveNext() )
		return -1;
	int nNewID = db.m_nID;
	hr = db.Open( "SELECT * FROM TerrainSpots WHERE ID = -1" ); // CRAP чтобы зафлушить изменения
	db.Close();
	Sleep(0); // чтобы передать упраление db'шным потокам
	
	CheckLayerExistence( NBuilding::MakeFragmentID( LID_SPOTS, db.m_nLayer ) );
	return nNewID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool UpdateTerrSpotDB( int nRecordID, int nVariantID, const NDb::CRndTerrainSpot *pSpot )
{
	if ( !pSpot )
		return false;
	CheckDBConnection();
	char buf[512];
	sprintf( buf, "SELECT * FROM TerrainSpots WHERE ID = %d", nRecordID );
	HRESULT hr = db.Open( buf );
	if ( FAILED(hr) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	hr = db.MoveNext();
	if ( FAILED(hr) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	db.m_nVariantID = nVariantID;
	db.m_nMaterialID = pSpot->pSpot->GetRecordID();
	db.m_ptPos = pSpot->ptPos;
	db.m_ptSize = pSpot->ptSize;
	db.m_nRotation = pSpot->nRotation;
	db.m_nPriority = pSpot->nPriority;
	db.m_nLayer = pSpot->nLayer;

	hr = db.SetData( 1 );
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	db.Close();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NBuilding::SProjectedSpot* CTerrSpot::GetSpot( int nUserID ) const
{
	EBrushType nBrush;
	int nFragmentID;
	GetFragmentID( nUserID, &nBrush, &nFragmentID );
	ESpotType type;
	EHandle h;
	int nSpotID;
	GetSpotID( nFragmentID, &type, &nSpotID, &h );
//	if ( nSpotID < 0 || nSpotID >= pInfo->spots.size() )
//		return 0;
	if ( BT_TEXSPOT == nBrush && ST_TERRAIN == type && nSpotIndex == nSpotID )
		return const_cast<NBuilding::SProjectedSpot*>( &spot );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrSpot::GetInfo( SSelectedInfo *pInfo )
{
	if ( !IsValid( pDBSpot ) )
		return false;
	pInfo->eBrushType = BT_TEXSPOT;
	pInfo->nBrushID = IsValid( pDBSpot->pSpot ) ? pDBSpot->pSpot->GetRecordID() : 0;
	pInfo->nRotation = pDBSpot->nRotation;
	pInfo->ptPos = CVec3( pDBSpot->ptPos, 0 );
	pInfo->nObjectID = pDBSpot->GetRecordID();
	pInfo->nFloor = 0;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrSpot::ProcessEvent( const string &str )
{
	if ( str == "topmost_spot" )
	{
		const int nVarID = pDBSpot->pVar->GetRecordID();
		*pRollback = *pDBSpot;
		pDBSpot->nPriority = GetMaxSpotPriority( nVarID ) + 1;
		NMapEditor::PushUndoCmd( CreateTerrSpotUndo( CWysiwygUndo::UA_CHANGE_POS, pRollback, pDBSpot ) );
		UpdateTerrSpotDB( pDBSpot->GetRecordID(), nVarID, pDBSpot );
		vector<NBuilding::SProjectedSpot> spots( 1, *CSpot::GetSpot() );
		pWorld->UpdateTerrainSpots( spots );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetMaxSpotPriority( int nVarID )
{
	NDb::CTemplVariant *pV = NDb::GetTemplVariant( nVarID );
	if ( !pV )
		return 0;
	int nMax = 0;
	for ( int i = 0; i < pV->terrainSpots.size(); ++i )
		if ( IsValid( pV->terrainSpots[i] ) )
			nMax = Max( nMax, pV->terrainSpots[i]->nPriority );
	return nMax;
}
/////////////////////////////////////////////////////////////////////////////////////
void CTerrSpot::OnCopy()
{
	AddTerrSpotToClipboardBuffer( spot );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrSpot::StartMove( const CVec2 &ptCursor )
{
	*pRollback = *pDBSpot;
	spotStartMove = spot;
	CSpot::StartMove( ptCursor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrSpot::TrackMove( bool bTileAlign, const CVec3 &ptMove, const CVec2 &ptCursor )
{
	vector<NBuilding::SProjectedSpot> spots;
	spots.push_back( spot );
	bool bRet = CSpot::TrackMove( bTileAlign, ptMove, ptCursor );
	FillDBSpotValues( pDBSpot, spot );
	spots.push_back( spot );
	if ( fabs( ptMove ) > FP_EPSILON )
		pWorld->UpdateTerrainSpots( spots );
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrSpot::Rotate( float fDRotation )
{
	vector<NBuilding::SProjectedSpot> spots;
	spots.push_back( spot );
	bool bRet = CSpot::Rotate( fDRotation );
	FillDBSpotValues( pDBSpot, spot );
	spots.push_back( spot );
	if ( fabs( fDRotation ) > FP_EPSILON )
		pWorld->UpdateTerrainSpots( spots );
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrSpot::IsEqual( int nID ) const 
{ 
	if ( IsValid( pDBSpot ) ) 
		return pDBSpot->GetRecordID() == nID; 
	return false; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CTerrSpot::GetSelectionID() const 
{ 
	if ( IsValid( pDBSpot ) ) 
		return pDBSpot->GetRecordID();
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrSpot::Move( const CVec3 &ptMove )
{
	*pRollback = *pDBSpot;
	pDBSpot->ptPos +=  CVec2( ptMove.x, ptMove.y );
	CSpot::Move( ptMove );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrSpot::RotateAround( float fRotation, const CVec2 &ptCenter )
{
	SBound b = GetBoundBox();
	CVec2 pt( b.s.ptCenter.x, b.s.ptCenter.y );
	CVec2 newPt = pt - ptCenter;
	RotatePt( &newPt, fRotation );
	newPt += ptCenter;
	pt = newPt - pt;

	return Rotate( fRotation ) && TrackMove( false, CVec3( pt.x, pt.y, 0 ), CVec2(0,0) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
