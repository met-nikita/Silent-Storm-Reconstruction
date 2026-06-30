#include "StdAfx.h"
#include "TerrSpotUndo.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataTerrain.h"
#include "..\Main\BuildingInfo.h"
#include "weInterface.h"

namespace NWysiwyg
{
	extern bool UpdateTerrSpotDB( int nRecordID, int nVariantID, const NDb::CRndTerrainSpot *pSpot );
	extern int AddTerrSpotDB( int nVariantID, const NBuilding::SProjectedSpot &spot );
	extern bool DeleteTerrSpot( int nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CTerrSpotUndo::CTerrSpotUndo( NDb::CRndTerrainSpot *_pStart, NDb::CRndTerrainSpot *_pEnd, CWysiwygUndo::EUndoAction _eAction, int nDbID )
: CDBWysiwygUndo<NDb::CRndTerrainSpot>(_eAction, _pStart, _pEnd, nDbID )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static NBuilding::SProjectedSpot DBSpot2ProjectedSpot( NDb::CRndTerrainSpot *p )
{
	NBuilding::SProjectedSpot s;

	s.ptOrigin = CVec3( p->ptPos, 0 );
	s.ptNormal = CVec3( 0, 0, 1 );
	s.ptSize   = p->ptSize;
	s.nRotation= p->nRotation;
	s.nMaterialID = IsValid( p->pSpot ) ? p->pSpot->GetRecordID() : 0;
	return s;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrSpotUndo::SetPos( NDb::CRndTerrainSpot *p, int nID )
{
	NDb::CRndTerrainSpot *pDB = NDb::GetRndTerrainSpot( nID );
	if ( !IsValid( pDB ) || !IsValid( pDB->pVar ) )
		return true;
	pDB->ptPos = p->ptPos;
	pDB->ptSize = p->ptSize;
	pDB->nRotation = p->nRotation;
	pDB->nPriority = p->nPriority;

	return NWysiwyg::UpdateTerrSpotDB( nID, pDB->pVar->GetRecordID(), p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrSpotUndo::Insert( NDb::CRndTerrainSpot *p )
{
	if ( !IsValid( p ) )
	{
		ASSERT(0);
		return false;
	}
	if ( !IsValid( p->pVar ) )
		return false;
	int n = NWysiwyg::AddTerrSpotDB( p->pVar->GetRecordID(), DBSpot2ProjectedSpot( p ) );
	if ( n < 0 )
		return false;
	SetID( n );
	Refresh<NDb::CRndTerrainSpot>( n );
	SetPos( p, GetID() );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrSpotUndo::Update()
{
	NWorld::IEditorWorld *pW = NMapEditor::GetEditorWorld();
	if ( pW )
	{
		vector<NBuilding::SProjectedSpot> spots;
		if ( IsValid( pStart ) )
			spots.push_back( DBSpot2ProjectedSpot( pStart ) );
		if ( IsValid( pEnd ) )
			spots.push_back( DBSpot2ProjectedSpot( pEnd ) );
		pW->UpdateTerrainSpots( spots );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrSpotUndo::Delete( int nID )
{
	return NWysiwyg::DeleteTerrSpot( nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWysiwygUndo* CreateTerrSpotUndo( CWysiwygUndo::EUndoAction eAction, NDb::CRndTerrainSpot *pStart, NDb::CRndTerrainSpot *pEnd, int nDbID )
{
	return new CTerrSpotUndo( pStart, pEnd, eAction, nDbID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
