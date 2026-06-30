#include "StdAfx.h"
#include "Rod.h"
#include "RodJunction.h"
#include "BuildingSchema.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBuilding
{
////////////////////////////////////////////////////////////////////////////////////////////////////
CRod::CRod( CBuildingSchema *_pSchema, CJunctionID nLJ, CJunctionID nRJ ) 
	: pSchema(_pSchema), nLJunction( nLJ ), nRJunction( nRJ )
{
	ASSERT( pSchema );
	ASSERT( pSchema->IsJunctionValid( nLJ ) && pSchema->IsJunctionValid( nRJ ) );
	CJunction *pLJ = pSchema->GetJunction( nLJ );
	CJunction *pRJ = pSchema->GetJunction( nRJ );
	ASSERT( pLJ->ptJ != pRJ->ptJ ); // нулевой длины ?
	ASSERT( pLJ->ptJ.x == pRJ->ptJ.x || pLJ->ptJ.y == pRJ->ptJ.y ); // диагональный ?
	bVert = pLJ->ptJ.x == pRJ->ptJ.x && pLJ->ptJ.y == pRJ->ptJ.y;
	if ( IsVert() )
	{
		ASSERT( pLJ->ptJ.z < pRJ->ptJ.z );
	}
	bLock = 0;
	bDestroy = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CIVec3 CRod::GetLeftPt() const { return pSchema->GetJunction( nLJunction )->ptJ; }
////////////////////////////////////////////////////////////////////////////////////////////////////
CIVec3 CRod::GetRightPt() const { return pSchema->GetJunction( nRJunction )->ptJ; }
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRod::Shoot( ERodSide from, EDirection dir )
{
	return pSchema->GetJunction( GetOppositeJunction( from ) )->Shoot( dir );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRod::AddWeight( float fWeight )
{
	ASSERT( pSchema->IsJunctionValid( nLJunction ) );
	pSchema->GetJunction( nLJunction )->AddWeight( 0.5f * fWeight );
	ASSERT( pSchema->IsJunctionValid( nRJunction ) );
	pSchema->GetJunction( nRJunction )->AddWeight( 0.5f * fWeight );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRod::AdvanceWavefront( ERodSide side, const int nWaveID, EDirection from, int nStep, CJuncList *pWaveList )
{
	if ( !Lock() )
		return false;
	const CJunctionID nJ = GetOppositeJunction( side );
	ASSERT( pSchema->IsJunctionValid( nJ ) );
	CJunction *pJ = pSchema->GetJunction( nJ );
	pJ->SetFlag( CJunction::FLAG_WAVEFRONT );
	pJ->wavefront  = nWaveID;
	pJ->wfrom = from;
	pJ->nIteration = nStep;
	pWaveList->push_back( nJ );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRod::Destroy() 
{ 
	bDestroy = true;
	if ( pSchema->IsJunctionValid( nLJunction ) )
		pSchema->GetJunction( nLJunction )->CheckVerticalValidity();
	if ( pSchema->IsJunctionValid( nRJunction ) )
		pSchema->GetJunction( nRJunction )->CheckVerticalValidity();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
