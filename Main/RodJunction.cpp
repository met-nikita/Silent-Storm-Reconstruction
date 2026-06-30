#include "StdAfx.h"
#include "RodJunction.h"
#include "BuildingSchema.h"
#include "..\Misc\RandomGen.h"
#include "..\DBFormat\DataRPG.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBuilding
{
const int N_RESERVE = 5;
////////////////////////////////////////////////////////////////////////////////////////////////////
CJunction::CJunction( CBuildingSchema *_pSchema, int _nID, const SRodEdge &_pt, float fW, bool bGr, bool bCellarW ) 
: nID(_nID), pSchema(_pSchema), pWaves(_pSchema->GetWaves()), ptJ( _pt.pt ), fJWeight( fW )
{
	nFlags = 0;
	if ( bGr )
		SetFlag( FLAG_GROUND );
	if ( bCellarW /*&& ptJ.z <= 0*/ )
	{
		SetFlag( FLAG_CELLARWALL );
		SetFlag( FLAG_GROUND );
	}
	if ( _pt.bFilled )
		SetFlag( FLAG_FILLED );
	stability = UNKNOWN;
	fUltimateMoment = 0;
	fUltimatePressure = 0;
	Init();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CJunction::Init()
{
	const int CONSTANT_FLAGS = FLAG_GROUND | FLAG_CELLARWALL | FLAG_FILLED | FLAG_BOTTOM;
	nFlags &= CONSTANT_FLAGS; // обнуляем неконстантные флаги
	stability = Flag( FLAG_GROUND ) ? STABLE : UNKNOWN;

	memset( moments, 0, sizeof( moments ) );
	fPressure = 0;
	fStableWeight = 0;
	nIteration = -1;
	stableJunctions.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CJunctionID CJunction::GetNeighbour( EDirection dir ) const 
{ 
	if ( pSchema->IsRodValid( neighbs[dir].nRod ) )
		return pSchema->GetRod( neighbs[dir].nRod )->GetOppositeJunction( neighbs[dir].side );
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CJunction::SetGround( bool bGround )
{
	if ( bGround )
		SetFlag( FLAG_GROUND );
	else
		ClearFlag( FLAG_GROUND );
	stability = bGround ? STABLE : UNKNOWN;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CJunction::SetBottom( bool bBottom )
{ 
	if ( bBottom )
		SetFlag( FLAG_BOTTOM );
	else
		ClearFlag( FLAG_BOTTOM );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static const int directionFlags[] =
{
	1 << XP, // +x
	1 << XM, // -x
	1 << YP, // +y
	1 << YM, // -y
	1 << UP,
	1 << DN,
	1 << NDIRECTIONS,
	1 << XPYP,
	1 << XMYP,
	1 << XMYM,
	1 << XPYM,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool CJunction::Arrow( const EDirection dir ) const 
{ 
	return Flag( (EFlags)directionFlags[dir] ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CJunction::SetArrow( const EDirection dir ) 
{ 
	SetFlag( (EFlags)directionFlags[dir] ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRodID CJunction::GetRod( EDirection dir ) const
{
	return neighbs[dir].nRod;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CJunction::AddNeighbour( const SNeighb &neighb )
{
	CRod *pNeighbRod = pSchema->GetRod( neighb.nRod );
	const CIVec3 &ptLeft = pNeighbRod->GetLeftPt();
	const CIVec3 &ptRight = pNeighbRod->GetRightPt();
	ASSERT( ptLeft == ptJ || ptRight == ptJ ); // связан ли neighb с текущим узлом?
	//
	EDirection dir = XP;
	if ( pNeighbRod->IsVert() )
	{
		if ( ptLeft == ptJ ) 
		{
			ASSERT( !pSchema->IsRodValid( neighbs[UP].nRod ) );
			dir = UP; 
			CJunction *pTopJ = pSchema->GetJunction( pNeighbRod->GetJunction( RS_RIGHT ) );
			if ( pTopJ->IsGround() )
			{
				// вышележащий узел помечен как устойчивый
				SetGround();
				if ( !pTopJ->IsCellarWall() )
					pTopJ->SetGround( false );
			}
		}
		else
		{ 
			ASSERT( !pSchema->IsRodValid( neighbs[DN].nRod ) );
			dir = DN; 
			CJunction *pDonJ = pSchema->GetJunction( pNeighbRod->GetJunction( RS_LEFT ) );
			if ( IsGround() )
			{
				// нижележащий узел должен быть помечен как устойчивый
				pDonJ->SetGround();
				if ( !IsCellarWall() )
					SetGround( false );
			}
		}
	}
	else
	{
		CIVec3 dpt = ptLeft == ptJ ? ptRight - ptLeft : ptLeft  - ptRight;
		if ( dpt.x > 0 )      { ASSERT( !pSchema->IsRodValid( neighbs[XP].nRod ) ); dir = XP; }
		else if ( dpt.x < 0 ) { ASSERT( !pSchema->IsRodValid( neighbs[XM].nRod ) ); dir = XM; }
		else if ( dpt.y > 0 ) {	ASSERT( !pSchema->IsRodValid( neighbs[YP].nRod ) ); dir = YP; }
		else if ( dpt.y < 0 ) { ASSERT( !pSchema->IsRodValid( neighbs[YM].nRod ) ); dir = YM; }
		else
			ASSERT( 0 );
	}
	neighbs[dir] = neighb;
	nNeighbs = CountLinks();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// возвращает false если не произошло изменения состояния стабильности текущего или последующих узлов
bool CJunction::Shoot( EDirection dir )
{
	ASSERT( dir >= XP && dir <= XPYM );
	if ( Arrow( dir ) )
		return false; // в этом напрвлении уже ходили
	//
	bool bRet = false;
	SetArrow( dir );
	if ( dir < UP )
	{
		if ( pSchema->IsRodValid( neighbs[dir].nRod ) )
			bRet = pSchema->GetRod( neighbs[dir].nRod )->Shoot( neighbs[dir].side, dir );
	}
	else
	{
		SNeighb neighb;
		switch ( dir )
		{
			case XPYP:
				if ( pSchema->IsRodValid( neighbs[XP].nRod ) )
					neighb = pSchema->GetJunction( pSchema->GetRod( neighbs[XP].nRod )->GetOppositeJunction( neighbs[XP].side ) )->neighbs[YP];
				else if ( pSchema->IsRodValid( neighbs[YP].nRod ) )
					neighb = pSchema->GetJunction( pSchema->GetRod( neighbs[YP].nRod )->GetOppositeJunction( neighbs[YP].side ) )->neighbs[XP];
				break;
			case XMYP:
				if ( pSchema->IsRodValid( neighbs[XM].nRod ) )
					neighb = pSchema->GetJunction( pSchema->GetRod( neighbs[XM].nRod )->GetOppositeJunction( neighbs[XM].side ) )->neighbs[YP];
				else if ( pSchema->IsRodValid( neighbs[YP].nRod ) )
					neighb = pSchema->GetJunction( pSchema->GetRod( neighbs[YP].nRod )->GetOppositeJunction( neighbs[YP].side ) )->neighbs[XM];
				break;
			case XMYM:
				if ( pSchema->IsRodValid( neighbs[XM].nRod ) )
					neighb = pSchema->GetJunction( pSchema->GetRod( neighbs[XM].nRod )->GetOppositeJunction( neighbs[XM].side ) )->neighbs[YM];
				else if ( pSchema->IsRodValid( neighbs[YM].nRod ) )
					neighb = pSchema->GetJunction( pSchema->GetRod( neighbs[YM].nRod )->GetOppositeJunction( neighbs[YM].side ) )->neighbs[XM];
				break;
			case XPYM:
				if ( pSchema->IsRodValid( neighbs[XP].nRod ) )
					neighb = pSchema->GetJunction( pSchema->GetRod( neighbs[XP].nRod )->GetOppositeJunction( neighbs[XP].side ) )->neighbs[YM];
				else if ( pSchema->IsRodValid( neighbs[YM].nRod ) )
					neighb = pSchema->GetJunction( pSchema->GetRod( neighbs[YM].nRod )->GetOppositeJunction( neighbs[YM].side ) )->neighbs[XP];
				break;
		}
		if ( pSchema->IsRodValid( neighb.nRod ) )
			bRet = pSchema->GetRod( neighb.nRod )->Shoot( neighb.side, dir );
	}
	//SetArrow( dir );
	return CheckStability() || bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// на основе массива "стрел" bArrows опрделяем стабильность узла
// false если новое значение стабильности равно старому
inline bool CJunction::CheckStability()
{
	EStability newstability = stability;

	if ( Arrow( XP ) && Arrow( XM ) )      newstability = STABLE;
	else if ( Arrow( YP ) && Arrow( YM ) ) newstability = STABLE;
	else if ( Arrow( XPYP ) && Arrow( XMYM ) ) newstability = STABLE;
	else if ( Arrow( XMYP ) && Arrow( XPYM ) ) newstability = STABLE;

	bool bRet = stability != newstability;
	stability = newstability;
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CJunction::UpdateVerticalStability()
{
	if ( STABLE == GetStability() )
		return;
	if ( !Flag( FLAG_GROUND ) /*&& nlist.size() <= 1*/ && !pSchema->IsRodValid( neighbs[DN].nRod ) )
	{
		stability = UNSTABLE;
		return;
	}
	if ( !pSchema->IsRodValid( neighbs[DN].nRod ) )
		return;
	CRod *pR = pSchema->GetRod( neighbs[DN].nRod );
	if ( STABLE == pSchema->GetJunction( pR->GetOppositeJunction( neighbs[DN].side ) )->GetStability() )
		stability = STABLE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*
CJunction::EStability CJunction::RecurseCheckStability( EDirection dir )
{
	if ( stability != UNKNOWN )
		return stability;
	if ( !Lock() )
		return UNKNOWN;
	//
	EStability s = UNKNOWN;
	if ( IsValid( neighbs[dir].pRod ) )
	{
		if ( stabilities[dir] != UNINITIALIZED )
			return stabilities[dir];
		else
		{
			s = neighbs[dir].pRod->GetOppositeJunction( neighbs[dir].side )->RecurseCheckStability( dir );
//			stabilities[dir] = s;
		}
	}

	if ( s != STABLE && IsValid( neighbs[DN].pRod ) )
	{
		s = neighbs[DN].pRod->GetOppositeJunction( neighbs[DN].side )->RecurseCheckStability( dir );
	}

	if ( s != STABLE && IsValid( neighbs[UP].pRod ) )
	{
		s = neighbs[UP].pRod->GetOppositeJunction( neighbs[UP].side )->RecurseCheckStability( dir );
	}
	//
	Unlock();
	return s;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CJunction::EStability CJunction::RecurseCheckStability()
{
	if ( UNKNOWN != stability )
		return stability;
	//
	EStability s1 = RecurseCheckStability( XP );
	EStability s2 = RecurseCheckStability( XM );
	stabilities[XP] = s1;
	stabilities[XM] = s2;
	if ( STABLE == s1 && STABLE == s2 )
	{
		stability = STABLE;
		return stability;
	}
	s1 = RecurseCheckStability( YP );
	s2 = RecurseCheckStability( YM );
	stabilities[YP] = s1;
	stabilities[YM] = s2;
	if ( STABLE == s1 && STABLE == s2 )
	{
		stability = STABLE;
		return stability;
	}
	stability = UNSTABLE;
	return stability;
}
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
CJunction::EStability CJunction::CheckFree( list<CJunction*> *pDomain, int nDepth )
{
	ASSERT( FREE != stability ); // принадлежит другой связной области - такого не может быть
	//
	if ( STABLE == stability || UNSTABLE_NOTFREE == stability )
		return STABLE;
	if ( !Lock() || IsFreeCheck() )
		return UNKNOWN;

	pDomain->push_back( this );
	SetFlag( FLAG_FREECHECK );
	EStability scheck = UNSTABLE;
	//
	for ( int i = 0; i < NDIRECTIONS; ++i )
	{
		const SNeighb &nb = neighbs[i];
		if ( pSchema->IsRodValid( nb.nRod ) && 
			STABLE == pSchema->GetJunction( pSchema->GetRod( nb.nRod )->GetOppositeJunction( nb.side ) )->CheckFree( pDomain ) ) 
			scheck = STABLE;
	}
	Unlock();
	return scheck;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float WEIGHT_EPSILON = 5e-5f;
////////////////////////////////////////////////////////////////////////////////////////////////////
static unsigned short nReminderID = 0;
static const EDirection oppositeDirs[7] = {
	XM,
	XP,
	YM,
	YP,
	DN,
	UP,
	NDIRECTIONS,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CJunction::AddMoment( float fWeight, const CIVec3 &ptMass, const EDirection from )
{
	const CIVec3 dpt = ptMass - ptJ;

	ASSERT( from < NDIRECTIONS );
	const EDirection dir = oppositeDirs[from];

	switch ( dir )
	{
		case XP:
			moments[XP] +=  dpt.x * fWeight;
			break;
		case XM:
			moments[XM] -=  dpt.x * fWeight;
			break;
		case YP:
			moments[YP] +=  dpt.y * fWeight;
			break;
		case YM:
			moments[YM] -=  dpt.y * fWeight;
			break;
	}

	fPressure += fWeight;
/*
	if ( dpt.x > 0 ) moments[XP] +=  dpt.x * fWeight;	
	else             moments[XM] += -dpt.x * fWeight;
	//
	if ( dpt.y > 0 ) moments[YP] +=  dpt.y * fWeight;	
	else             moments[YM] += -dpt.y * fWeight;
*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CJunction::ComputeStableJInfluence( CJuncList *pWaveList )
{
	if ( GetStability() != STABLE )
		return;
	vector<int> edges;
//	for ( CNeighbList::const_iterator i = nlist.begin(); i != nlist.end(); ++i )
	for ( int i = 0; i < NDIRECTIONS; ++i )
	{
		const SNeighb &nb = neighbs[i];
		if ( pSchema->IsRodValid( nb.nRod ) && pSchema->GetJunction( pSchema->GetRod( nb.nRod )->GetOppositeJunction( nb.side ) )->GetStability() == STABLE )
			continue;
		edges.push_back( i );
	}
	if ( edges.empty() )
		return;
	float fWeight = 1.0f / edges.size();
	//for ( CNeighbList::const_iterator i = edges.begin(); i != edges.end(); ++i )
	for ( int i = 0; i < edges.size(); ++i )
	{
		const SNeighb &nb = neighbs[edges[i]];
		if ( pSchema->IsRodValid( nb.nRod ) )
		{
			CRod *pRod = pSchema->GetRod( nb.nRod );
			const int nWaveID = pWaves->size();
			pWaves->push_back( SPath( this, fWeight, oppositeDirs[(EDirection)edges[i]] ) );
			pRod->AdvanceWavefront( nb.side, nWaveID, (EDirection)edges[i], -1, pWaveList );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CJunction::ComputeMoment()
{
	if ( GetStability() == STABLE )
		return;
	//
	if ( stableJunctions.empty() && GetStability() != FREE )
	{
		ASSERT( 0 );
		OutputDebugString( "Cant compute momemnt\n" );
		stability = UNKNOWN_MOMENT;
		return;
	}
	const float fScale = 1.0f / fStableWeight;
	float fCheck = 0;
	for ( vector<int>::const_iterator i = stableJunctions.begin(); i != stableJunctions.end(); ++i )
	{
		const SPath &node = (*pWaves)[*i];
		const float dw = GetWeight() * fScale * node.fWeight;
		node.pJ->AddMoment( dw, ptJ, node.from );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CJunction::HasRightAngle()
{
	/*
	if ( nlist.size() < 2 )
		return true;
	for ( CNeighbList::const_iterator i = nlist.begin(); i != nlist.end(); ++i )
	{
		for ( int j = 0; j < NDIRECTIONS; ++j )
		{
			if ( j == i->second || j == oppositeDirs[i->second] )
				continue;
			if ( IsValid( neighbs[j].pRod ) && IsValid( neighbs[j].pRod->GetOppositeJunction( neighbs[j].side ) ) )
				return true;
		}
	}
	*/
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CJunction::ProcessWavefront( SRand *pRand, int nStep, CJuncList *pWaveList )
{
	ASSERT( pRand );
	if ( !IsWaveFrontPoint() || nIteration == nStep || GetStability() == STABLE )
		return false;
	//
	stableJunctions.push_back( wavefront );
	fStableWeight += (*pWaves)[wavefront].fWeight;
	// 
	vector<int> edges;
	//for ( CNeighbList::const_iterator i = nlist.begin(); i != nlist.end(); ++i )
	for ( int i = 0; i < NDIRECTIONS; ++i )
	{
		const SNeighb &nb = neighbs[i];
		if ( !pSchema->IsRodValid( nb.nRod ) || oppositeDirs[wfrom] == i )
			continue;
		CRod *pRod = pSchema->GetRod( nb.nRod );
		if ( pSchema->GetJunction( pRod->GetOppositeJunction( nb.side ) )->IsWaveFrontPoint() )
			continue;
		if ( !pRod->Lock() )
			continue;
		pRod->Unlock();
		edges.push_back( i );
	}
	//
	if ( edges.size() < 2 )
		ClearFlag( FLAG_WAVEFRONT );
	else
		pWaveList->push_back( nID );
	//
	if ( edges.empty() )
		return false;
	const int iRnd = pRand->Get( edges.size() );
	const SNeighb &nb = neighbs[edges[iRnd]];

	return pSchema->GetRod( nb.nRod )->AdvanceWavefront( nb.side, wavefront, (EDirection)edges[iRnd], nStep, pWaveList );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CJunction::SetDestroyLimits( NDb::CRPGArmor *pArmor )
{
	if ( !pArmor )
	{
		ASSERT(0);
		fUltimatePressure = 40;
		fUltimateMoment = 140;
		return;
	}
	if ( pArmor->pMaterial->nDR < 0 )
	{
		//SetFlag( FLAG_CELLARWALL );
		SetFlag( FLAG_GROUND );
	}
	fUltimatePressure = Max( fUltimatePressure, pArmor->pMaterial->fUltimatePresure );
	fUltimateMoment = Max( fUltimateMoment, pArmor->pMaterial->fUltimateMoment );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CJunction::Destroy() 
{ 
	SetFlag( FLAG_DESTROY ); 
//	for ( CNeighbList::iterator i = nlist.begin(); i != nlist.end(); ++i )
	for ( int i = 0; i < NDIRECTIONS; ++i )
	{
		const SNeighb &nb = neighbs[i];
		if ( pSchema->IsRodValid( nb.nRod ) )
			pSchema->GetRod( nb.nRod )->Destroy();
	}
	parents.clear();
	stableJunctions.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CJunction::Reset()
{
	Init();
	//
	nNeighbs = 0;
//	for ( CNeighbList::iterator i = nlist.begin(); i != nlist.end(); )
	for ( int i = 0; i < NDIRECTIONS; ++i )
		if ( pSchema->IsRodValid( neighbs[i].nRod ) )
			++nNeighbs;
	//
	if ( nNeighbs == 0 )
		SetFlag( FLAG_DESTROY );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CJunction::AddParent( CJunctionID nJ )
{
	if ( nJ == nID )
	{
		ASSERT(0);
		return;
	}
//	ASSERT( pJ->IsFilled() );
	parents.push_back( nJ );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CJunction* CJunction::GetNearestFilled() const 
{ 
	CJunction *p = 0;
	for ( int i = 0; i < parents.size(); ++i )
		if ( pSchema->IsJunctionValid( parents[i] ) )
		{
			p = pSchema->GetJunction( parents[i] );
			if ( p->GetStability() != STABLE )
				return p;
			else
				continue;
		}
	return p;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CJunction::DestroyRod( EDirection dir )
{
	if ( pSchema->IsRodValid( neighbs[dir].nRod ) )
		pSchema->GetRod( neighbs[dir].nRod )->Destroy();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CJunction::DestroyRod( CJunction *pJ )
{
//	for ( CNeighbList::iterator i = nlist.begin(); i != nlist.end(); ++i )
	for ( int i = 0; i < NDIRECTIONS; ++i )
	{
		const SNeighb &nb = neighbs[i];
		if ( !pSchema->IsRodValid( nb.nRod ) )
			continue;
		CRod *pRod = pSchema->GetRod( nb.nRod );
		if ( pSchema->GetJunction( pRod->GetOppositeJunction( nb.side ) ) == pJ )
			pRod->Destroy();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CJunction::CountLinks() const
{
	int nCount = 0;
//	for ( CNeighbList::const_iterator i = nlist.begin(); i != nlist.end(); ++i )
	for ( int i = 0; i < NDIRECTIONS; ++i )
	{
		const SNeighb &nb = neighbs[i];
		if ( pSchema->IsRodValid( nb.nRod ) )
			++nCount;
	}
	return nCount;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CJunction::GetMoment( const CRod *pRod ) const
{
	for ( int i = 0; i < NDIRECTIONS; ++i )
	{
		const SNeighb &nb = neighbs[i];
		if ( pSchema->IsRodValid( nb.nRod ) && pRod == pSchema->GetRod( nb.nRod ) )
			return moments[i];
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CJunction::IsLinkBroken( const CRod *pRod ) const
{
	for ( int i = 0; i < NDIRECTIONS; ++i )
	{
		const SNeighb &nb = neighbs[i];
		if ( pSchema->IsRodValid( nb.nRod ) && pRod == pSchema->GetRod( nb.nRod ) )
			return IsLinkBroken( (EDirection)i );
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float NHPSCALE = 10.0f / 255.0f;
bool CJunction::IsBroken( int nHP ) const 
{ 
	const float f = (1.0f / 3.0f) * fUltimatePressure;
	//return fPressure > 2 * f + f * nHP * NHPSCALE;
	return fPressure > fUltimatePressure;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CJunction::IsLinkBroken( EDirection dir ) const 
{
	const float f = (1.0f / 3.0f) * fUltimateMoment;
	return fabs( GetMoment(dir) ) > fUltimateMoment;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CJunction::CheckVerticalValidity()
{
	if ( pSchema->IsRodValid( neighbs[UP].nRod ) && pSchema->IsRodValid( neighbs[DN].nRod ) )
	{
		if (	!pSchema->IsRodValid( neighbs[XP].nRod ) && 
					!pSchema->IsRodValid( neighbs[XM].nRod ) && 
					!pSchema->IsRodValid( neighbs[YP].nRod ) && 
					!pSchema->IsRodValid( neighbs[YM].nRod ) )
		{
			SetFlag( FLAG_DESTROY );
			pSchema->GetRod( neighbs[UP].nRod )->Destroy();
			pSchema->GetRod( neighbs[DN].nRod )->Destroy();
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
