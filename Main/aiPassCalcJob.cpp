#include "StdAfx.h"
#include "aiPassCalcJob.h"
#include "aiJob.h"
#include "aiGrid.h"
#include "aiColourer.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPassCalcerJob : public CAIJob
{
	OBJECT_BASIC_METHODS(CPassCalcerJob);
	ZDATA
	ZPARENT( CAIJob )
	CPtr<CLayersGroup> pGroup;
	CPtr<IAIMap> pMap;
	SPathPlace where;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIJob *)this); f.Add(3,&pGroup); f.Add(4,&pMap); f.Add(5,&where); return 0; }
	CPassCalcerJob() {}
	CPassCalcerJob( IAIMap *_pMap, CLayersGroup *_pGroup, const SPathPlace &_where ) 
		: pGroup(_pGroup), where(_where), pMap(_pMap) {}
	virtual void DoJob();
	virtual bool IsHighestPriority() { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPassCalcerJob::DoJob()
{
	ASSERT( IsValid( pMap ) && IsValid( pGroup ) );
	if ( IsValid( pMap ) && IsValid( pGroup ) )
	{
		// retail @0x488880: forced -- the recolour job added alongside would otherwise gate it off
		pGroup->RefreshSpot( where, pMap, 2, true );
		DebugTrace( "Pass calcer job is done (%d left is this group)\n", pGroup->nPassCalcJobsLeft );
	}
	else
		DebugTrace( "Pass calcer job is terminated: invalid (%d left)\n", pGroup->nPassCalcJobsLeft );

	--pGroup->nPassCalcJobsLeft;
	ASSERT(pGroup->nPassCalcJobsLeft>=0);
	Finish();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRecountColourerJob: public CAIJob
{
	OBJECT_BASIC_METHODS(CRecountColourerJob);
	ZDATA
	ZPARENT( CAIJob )
	CPtr<CLayersGroup> pGroup;
	int nCurrentLayer;
	bool bAllColoured;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIJob *)this); f.Add(3,&pGroup); f.Add(4,&nCurrentLayer); f.Add(5,&bAllColoured); return 0; }
	CRecountColourerJob( CLayersGroup *_pGroup = 0 ): nCurrentLayer(0), pGroup(_pGroup), bAllColoured(false) {}
	virtual void DoJob();
	virtual bool IsHighestPriority() { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRecountColourerJob::DoJob()
{
	if ( pGroup->nPassCalcJobsLeft > 0 )
		return;
	CNodesLayer *pLayer = 0;
	while ( !IsValid(pLayer) )
	{
		if ( nCurrentLayer >= pGroup->layers.size() )
		{
			if ( bAllColoured )
			{
				for ( int i = 0; i < pGroup->layers.size(); ++i )
				{
					CNodesLayer *pLayer = pGroup->layers[i];
					if ( IsValid(pLayer) )
					{
						int nL = pLayer->nLayer;
						CMapColourer *pC = pGroup->pNet->GetColourer(nL);
						ASSERT( pC->zonesToRecalc.empty() );
					}
				}
				Finish();
				pGroup->bHasRecolourJob = false;
				return;
			}
			else
			{
				bAllColoured = true;
				nCurrentLayer = 0;
			}
		}
		pLayer = pGroup->layers[nCurrentLayer];
		++nCurrentLayer;
	}
	int nL = pLayer->nLayer;
	CMapColourer *pC = pGroup->pNet->GetColourer(nL);
	DebugTrace( "Recolour job: layer %d in group, ", nCurrentLayer-1 );
	if ( !bAllColoured )
	{
		pC->RecalcColouring( pGroup->pNet, nL );
		DebugTrace("Recolour job half-complete\n" );
	}
	else
	{
		pC->RecalcTransitions( pGroup->pNet );
		DebugTrace("Recolour job complete\n" );
		ASSERT( pC->zonesToRecalc.empty() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void AddNewPassCalcerJob( IAIJobManager *pWhere, IAIMap *pMap, CLayersGroup *pGroup, const SPathPlace &_where )
{
	ASSERT( IsValid(pWhere) );
	ASSERT( IsValid(pMap) ); 
	if ( !IsValid(pWhere) )
		return;
	pWhere->Add( new CPassCalcerJob( pMap, pGroup, _where ) );
	++pGroup->nPassCalcJobsLeft;
	DebugTrace( "New pass calcer job added (total %d is this group)\n", pGroup->nPassCalcJobsLeft );
	if ( !pGroup->bHasRecolourJob )
	{
		pWhere->Add( new CRecountColourerJob( pGroup ) );
		DebugTrace( "New recolour job added\n" );
		pGroup->bHasRecolourJob = true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
// retail saveload ids (serialization-convergence W1; s2_scratch docs/SERIALIZATION_CONVERGENCE.md)
using namespace NAI;
REGISTER_SAVELOAD_CLASS( 0x00513130, CPassCalcerJob )
REGISTER_SAVELOAD_CLASS( 0x00513131, CRecountColourerJob )
