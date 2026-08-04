#include "StdAfx.h"
#include "BuildingSchema.h"
#include "BuildingGrid.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBuilding
{
////////////////////////////////////////////////////////////////////////////////////////////////////
CBuildingSchema::CBuildingSchema()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingSchema::Reserve( int nJuncs, int nRods )
{
	juncs.reserve( nJuncs );
	rods.reserve( nRods );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// at a point x,y on the ground there can be only one node at a time
void CBuildingSchema::CheckGroundHash( CJunction *pJ )
{
	ASSERT( pJ );
	if ( !pJ->IsGround() )
		return;
	CTPoint<int> pt( pJ->ptJ.x, pJ->ptJ.y );
	CJunctionID &ref = groundhash[pt];
	CJunctionID nOld = ref;
	if ( IsJunctionValid( nOld ) )
	{
		CJunction *pOld = GetJunction( nOld );
		if ( pOld->IsCellarWall() || pOld->ptJ.z < pJ->ptJ.z )
		{
			if ( !pJ->IsCellarWall() )
				pJ->SetGround( false );
			return;
		}
		pOld->SetGround( false );
		pOld->SetBottom( false );
	}
	pJ->SetBottom( true );
	ref = pJ->GetID();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRod* CBuildingSchema::AddRod( NDb::CRPGArmor *pArmor, SRodEdge ptLeft, SRodEdge ptRight, bool bGround )
{
#ifdef _DEBUG
	if ( ptLeft == ptRight || (ptLeft.pt.x != ptRight.pt.x && ptRight.pt.y != ptLeft.pt.y && ptLeft.pt.z != ptRight.pt.z) 
		|| fabs2( ptLeft.pt - ptRight.pt ) > 1 )
	{
		ASSERT( 0 ); // only horizontal or vertical rods
		return 0;
	}
#endif
	//
	if ( ptRight.pt.z < ptLeft.pt.z ) // ordering the edges of the rod (the left edge is lower than the right, etc.)
		swap( ptLeft, ptRight );
	else if ( ptRight.pt.y < ptLeft.pt.y )
		swap( ptLeft, ptRight );
	else if ( ptRight.pt.x < ptLeft.pt.x )
		swap( ptLeft, ptRight );
	//
	CJunctionID &nLJ = junchash[ptLeft.pt];
	CJunctionID &nRJ = junchash[ptRight.pt];
	//const float fWeight2 = 0.5f * fWeight;
	if ( IsJunctionValid( nLJ ) && IsJunctionValid( nRJ ) )
	{
		CJunction *pLJ = GetJunction( nLJ );
		CJunction *pRJ = GetJunction( nRJ );
		EDirection dir;
		CIVec3 ptDir = pRJ->ptJ - pLJ->ptJ;
		if ( ptDir.x > 0 )
			dir = XP;
		else if ( ptDir.x < 0 )
			dir = XM;
		else if ( ptDir.y > 0 )
			dir = YP;
		else if ( ptDir.y < 0 )
			dir = YM;
		else if ( ptDir.z > 0 )
			dir = UP;
		else
			dir = DN;
		if ( IsRodValid( pLJ->GetRod( dir ) ) )
		{
			CRod *pR = GetRod( pLJ->GetRod( dir ) );
			// this rod already exists
			//pLJ->AddWeight( ptLeft.fWeight );
			//pRJ->AddWeight( ptRight.fWeight );
			if ( ptLeft.bFilled && !pLJ->IsFilled() )
				pLJ->SetFilled();
			if ( ptRight.bFilled && !pRJ->IsFilled() )
				pRJ->SetFilled();
			pLJ->SetDestroyLimits( pArmor );
			pRJ->SetDestroyLimits( pArmor );
			return pR;
		}
	}
	//
	if ( !IsJunctionValid( nLJ ) )
	{
		nLJ = juncs.size(); // remember the new junction in the hash
		CJunction &lj = *juncs.insert( juncs.end(), CJunction( this, nLJ, ptLeft, 0, bGround, ptLeft.bCellar ) );   // retail @0xc9b60: e1.bCellar
		CheckGroundHash( &lj );
		lj.AddWeight( ptLeft.fWeight );
		ASSERT( lj.GetID() < juncs.size() );
	}
	if ( !IsJunctionValid( nRJ ) )
	{
		nRJ = juncs.size(); // remember the new junction in the hash
		CJunction &rj = *juncs.insert( juncs.end(), CJunction( this, nRJ, ptRight, 0, bGround && ptLeft.pt.z == ptRight.pt.z, ptRight.bCellar ) );   // retail: e2.bCellar
		CheckGroundHash( &rj );
		rj.AddWeight( ptRight.fWeight );
		ASSERT( rj.GetID() < juncs.size() );
	}
	//
	CJunction *pLJ = GetJunction( nLJ );
	CJunction *pRJ = GetJunction( nRJ );
	pLJ->SetDestroyLimits( pArmor );
	pRJ->SetDestroyLimits( pArmor );
	const int nRodID = rods.size();
	CRod &rod = *rods.insert( rods.end(), CRod( this, pLJ->GetID(), pRJ->GetID() ) );
	//
	pLJ->AddNeighbour( SNeighb( nRodID, RS_LEFT ) );
	pRJ->AddNeighbour( SNeighb( nRodID, RS_RIGHT ) );
	return &rod;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingSchema::AddNode( NDb::CRPGArmor *pArmor, const CVec3 &pt, bool bCellarWall, float fWeight, const vector<SJunction> &points, bool bFilled )
{
	fWeight *= 0.1f;
	for ( int i = 0; i < points.size(); ++i )
	{
		// retail @0xc9f80: the node end carries the caller's cellar bit, the far end its OWN per-SJunction bit
		CRod *p = AddRod( pArmor, SRodEdge( pt, bFilled, fWeight, bCellarWall ), SRodEdge( points[i].pt, false, 0, points[i].bCellar ), points[i].bGround );
		if ( p )
		{
			const CIVec3 ptCenter(pt.x, pt.y, pt.z);
			// 
			CJunction *pJL = GetJunction( p->GetJunction( RS_LEFT ) );
			CJunction *pJR = GetJunction( p->GetJunction( RS_RIGHT ) );
			if ( pJL->IsFilled() )
			{
//				ASSERT( ptCenter == pJL->ptJ );
//				ASSERT( !pJR->IsFilled() );
				pJR->AddParent( pJL->GetID() );
			}
			else
			{
//				ASSERT( ptCenter == pJR->ptJ );
//				ASSERT( pJR->IsFilled() );
//				ASSERT( !pJL->IsFilled() );
				pJL->AddParent( pJR->GetID() );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingSchema::Destroy(	CBuildingGrid *pGrid, const SPoint3 &pt )
{
	CJunctionID &nID = junchash[CIVec3(pt.x, pt.y, pt.z)];
	if ( !IsJunctionValid( nID ) )
		return;
	CJunction *pJ = GetJunction( nID );
	pJ->Destroy();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuildingSchema::Destroy( CBuildingGrid *pGrid, CJunction *pJ, int nDepth, bool bEffects )
{
	if ( !pJ || nDepth > 5 )
		return false;
	if ( pJ->IsGround() || pJ->IsCellarWall() )
		return false;
	SPoint3 pt( pJ->ptJ.x, pJ->ptJ.y, pJ->ptJ.z );
	bool bRet1 = pGrid->DamageSpot( pt, 255, bEffects );   // retail @0xc8f40: DamageSpot(pt, 0xff=DESTROY_LIM, bEffects)
	if ( pJ->IsFilled() )
	{
		if ( pGrid->IsDestroyed( pt ) )
			pJ->Destroy();
		return bRet1;
	}
	if ( pJ->CountLinks() == 0 )
		return bRet1;
	CJunction *pNearestJF = pJ->GetNearestFilled();
	if ( pNearestJF )
	{
		SPoint3 ptNear( pNearestJF->ptJ.x, pNearestJF->ptJ.y, pNearestJF->ptJ.z );
		bool bRet2 = pGrid->DamageSpot( ptNear, 255, bEffects );
		if ( pNearestJF->IsFilled() )
		{
			if ( pGrid->IsDestroyed( ptNear ) )
				pNearestJF->Destroy();
			pNearestJF->DestroyRod( pJ );
			return bRet2;
		}
		else
		{
			ASSERT(0);
		}
	}
	ASSERT(0);
	//
	/*
	for ( int i = XP; i < NDIRECTIONS; ++i )
	{
		const CJunction *pNJ = pJ->GetNeighbour( EDirection(i) );
		if ( !pNJ )
			continue;
		bool bDS = nDepth < 2 ? pNJ->GetStability() != CJunction::STABLE : true;
		if ( bDS && pNJ->IsFilled() && !pGrid->IsDestroyed( SPoint3( pNJ->ptJ.x, pNJ->ptJ.y, pNJ->ptJ.z ) ) )
		{
			pGrid->DamageSpot( SPoint3( pNJ->ptJ.x, pNJ->ptJ.y, pNJ->ptJ.z ) );
			return true;
		}
	}
	// coudn't destroy any node
	for ( int i = XP; i < NDIRECTIONS; ++i )
	{
		CJunction *pNJ = pJ->GetNeighbour( EDirection(i) );
		if ( Destroy( pGrid, pNJ, nDepth + 1 ) )
			return true;
	}
	ASSERT( 0 ); // cant destroy construction piece
	*/
	//OutputDebugString( "Cant destroy grid node\n" );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuildingSchema::Recalc( CBuildingGrid *pGrid )
{
	waves.clear();
	CheckArtifacts();
	ComputeStability();
//	ComputeStabilityR();
	FindFree();
	ComputeMoments();
	
	if ( !pGrid )
		return false;
	bool bModified = false;
	for ( vector<CJunction>::iterator pJ = juncs.begin(); pJ != juncs.end(); ++pJ )
	{
		CJunction &jun = *pJ;
		if ( pJ->GetStability() == CJunction::FREE )
		{
			// retail Recalc @0xc9670: FREE-cluster removals are SILENT (bEffects=false)
			if ( Destroy( pGrid, &jun, 0, false ) )
				bModified = true;
		}
		else if ( pJ->GetStability() == CJunction::STABLE )
		{
			// retail: ROUND(SQRT(cellHP)) -- round-to-nearest fistp, not truncation
			int nHP = Float2Int( sqrt( (float)pGrid->GetHP( SPoint3( pJ->ptJ.x, pJ->ptJ.y, pJ->ptJ.z ) ) ) );
			if ( pJ->IsBroken( nHP ) )
			{
				// broken-STABLE junction: FX-worthy break (retail bEffects=true)
				if ( Destroy( pGrid, &jun, 0, true ) )
				{
					bModified = true;
					continue;
				}
			}
			for ( int i = XP; i < UP; ++i )
			{
				if ( pJ->IsLinkBroken( EDirection( i ) ) )
				{
					int nJID = pJ->GetNeighbour( EDirection( i ) );
					if ( IsJunctionValid( nJID ) )
					{
						CJunction *pNJ = GetJunction( nJID );
						// broken link of a stable junction: FX-worthy (retail bEffects=true)
						if ( Destroy( pGrid, pNJ, 0, true ) )
						{
							bModified = true;
							pJ->DestroyRod( EDirection(i) );
						}
					}
				}
			}
		}
	}
	if ( bModified )
		pGrid->Updated();
	//FindBorders();
	//for ( list<CRod*>::iterator i = rods.begin(); i != rods.end(); ++i )
	//	Process( *i, F_RODWEIGHT, true );
	return bModified;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingSchema::Clear()
{
	junchash.clear();
	groundhash.clear();
	juncs.clear();
	rods.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class JuncsSort
{
	CBuildingSchema *pSchema;
public:
	JuncsSort( CBuildingSchema *p ): pSchema(p) {}
	bool operator()( const int j1, const int &j2 )
	{
		return pSchema->GetJunction( j1 )->ptJ.z < pSchema->GetJunction( j2 )->ptJ.z;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingSchema::Start()
{
	sortedJuncs.resize( juncs.size() );
	for ( int i = 0; i < juncs.size(); ++i )
		sortedJuncs[i] = i;
	sort( sortedJuncs.begin(), sortedJuncs.end(), JuncsSort(this) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingSchema::ComputeStability()
{
	int i, j;

	for ( i = 0; i < sortedJuncs.size(); )
	{
		if ( !IsJunctionValid( sortedJuncs[i] ) )
		{
			++i;
			continue;
		}
		CJunction *pI = GetJunction( sortedJuncs[i] );
		const int nz = pI->ptJ.z;
		// initialize layer state according to previous layer
		for ( j = i; j < sortedJuncs.size(); ++j )
		{
			if ( !IsJunctionValid( sortedJuncs[j] ) )
				continue;
			CJunction *pJ = GetJunction( sortedJuncs[j] );
			if ( pJ->ptJ.z != nz )
				break;
			pJ->UpdateVerticalStability();
		}
		bool bChanges = true;
		int  nIterations = 0;
		// continue iteration while changes happen
		while ( bChanges )
		{
			bChanges = false;
			for ( j = i; j < juncs.size(); ++j )
			{
				if ( !IsJunctionValid( sortedJuncs[j] ) )
					continue;
				CJunction *pJ = GetJunction( sortedJuncs[j] );
				if ( pJ->ptJ.z != nz )
					break;
				if ( pJ->GetStability() == CJunction::STABLE )
				{
					bChanges = pJ->Shoot( XP ) || bChanges;
					bChanges = pJ->Shoot( XM ) || bChanges;
					bChanges = pJ->Shoot( YP ) || bChanges;
					bChanges = pJ->Shoot( YM ) || bChanges;
					//
					bChanges = pJ->Shoot( XPYP ) || bChanges;
					bChanges = pJ->Shoot( XMYP ) || bChanges;
					bChanges = pJ->Shoot( XMYM ) || bChanges;
					bChanges = pJ->Shoot( XPYM ) || bChanges;
				}
			}
			++nIterations;
		}
		i = j;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingSchema::ComputeStabilityR()
{
	/*
	list< CObj<CJunction> >::iterator i, j;

	for ( i = juncs.begin(); i != juncs.end(); ++i )
	{
		const nz = (*i)->ptJ.z;
		for ( j = i; j != juncs.end(); ++j )
		{
			CJunction *pJ = *j;
			if ( pJ->ptJ.z != nz )
				break;
			pJ->RecurseCheckStability();
		}
	}
	*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingSchema::FindFree()
{
	for ( vector<CJunction>::iterator pJ = juncs.begin(); pJ != juncs.end(); ++pJ )
	{
		//ASSERT( pJ->GetStability() != CJunction::UNKNOWN ); // node stability is not calculated yet
//		if ( !pJ->IsFilled() && !pJ->HasRightAngle() )
//		{
			// If the node doesn't match the actual building block and doesn't have 90 degree rods, then throw it away.
//			pJ->SetStability( CJunction::FREE );
//			continue;
//		}
		if ( pJ->GetStability() == CJunction::UNSTABLE || pJ->GetStability() == CJunction::UNKNOWN )
		{
			list<CJunction*> domain;
			CJunction::EStability s = pJ->CheckFree( &domain );
			s = CJunction::STABLE == s ? CJunction::UNSTABLE_NOTFREE : CJunction::FREE;
			for ( list<CJunction*>::iterator j = domain.begin(); j != domain.end(); ++j )
			{
				CJunction *p = (*j);
				if ( CJunction::UNSTABLE == p->GetStability() || CJunction::UNKNOWN == p->GetStability() )
					p->SetStability( s );
#ifdef _DEBUG
				else
				{
					int l = domain.size();
				}
#endif
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingSchema::ComputeMoments()
{
	CJuncList waveList1, waveList2;
	for ( vector<CJunction>::iterator i = juncs.begin(); i != juncs.end(); ++i )
	{
		//if ( (*i)->GetWeight() < FP_EPSILON )
		//	continue;
		i->ComputeStableJInfluence( &waveList1 );
	}
	//
	SRand rand( SRandomSeed( 1 ) );
	//SRand rand;
	bool bIter = true;
	int nStep = 0;
	CJuncList *pMasterList = &waveList1;
	CJuncList *pNextList = &waveList2;
	waveList2.reserve( waveList1.size() );
	while ( bIter ) 
	{
		bIter = false;
		/*
		for ( list< CObj<CJunction> >::iterator i = juncs.begin(); i != juncs.end(); ++i )
		{
			//if ( (*i)->GetWeight() < FP_EPSILON )
			//	continue;
			bIter = (*i)->ProcessWavefront( &rand, nStep ) || bIter;
		}
		++nStep;
		*/
		for ( CJuncList::iterator i = pMasterList->begin(); i != pMasterList->end(); ++i )
		{
			//if ( (*i)->GetWeight() < FP_EPSILON )
			//	continue;
			bIter = GetJunction( *i )->ProcessWavefront( &rand, nStep, pNextList ) || bIter;
		}
		++nStep;
		pMasterList->clear();
		swap( pNextList, pMasterList );
	}
	//
	for ( vector<CJunction>::iterator i = juncs.begin(); i != juncs.end(); ++i )
	{
		//if ( (*i)->GetWeight() < FP_EPSILON )
		//	continue;
		if ( !i->IsDestroyed() )
			i->ComputeMoment();
	}
	//
#ifdef _DEBUG
	static int nComputeCount = 0;
	static int nTotalDiscrepancy = 0;
	++nComputeCount;
	float fRealWeight = 0, fComputedWeight = 0;
	for ( vector<CJunction>::iterator i = juncs.begin(); i != juncs.end(); ++i )
	{
		if ( CJunction::UNSTABLE_NOTFREE == i->GetStability() )
			fRealWeight += i->GetWeight();
		fComputedWeight += i->GetPressure();
	}
	float fdw = fRealWeight - fComputedWeight;
	if ( fdw > 1 )
	{
		int percent = 100 * (fdw / fRealWeight);
		nTotalDiscrepancy += percent;
		char buf[64];
		sprintf( buf, "CBuildingSchema: Weight discrepancy=%d%%\taverage=%.4g%%\n", percent, (float)nTotalDiscrepancy / nComputeCount );
		OutputDebugString( buf );
/*
		if ( percent > 20 )
		{
			CString str;
			str.Format( "%d\n", nSeed );
			OutputDebugString( str );
			Sleep( 1000 );			
		}
*/
	}
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingSchema::Reset()
{
	for ( vector<CJunction>::iterator i = juncs.begin(); i != juncs.end(); ++i )
	{
		if ( i->IsDestroyed() )
		{
			//i = juncs.erase( i );
		}
		else
		{
			i->Reset();
			//++i;
		}
	}
	//
	for ( vector<CRod>::iterator i = rods.begin(); i != rods.end(); ++i )
	{
		if ( i->IsDestroyed() )
		{
		//	i = rods.erase( i );
		}
		else
		{
			i->Reset();
	//		++i;
		}
	}
	//for ( list< CObj<CJunction> >::const_iterator i = juncs.begin(); i != juncs.end(); ++i )
//		(*i)->Reset();
	//
	}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuildingSchema::CheckArtifacts()
{
//	for ( vector<CJunction>::iterator i = juncs.begin(); i != juncs.end(); ++i )
//		i->CheckVerticalValidity();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NBuilding;
BASIC_REGISTER_CLASS(CBuildingSchema);
