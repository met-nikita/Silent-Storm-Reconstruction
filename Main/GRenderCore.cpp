#include "StdAfx.h"
#include "GRenderCore.h"
#include "GfxBuffers.h"
#include "Transform.h"
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSceneFragments
////////////////////////////////////////////////////////////////////////////////////////////////////
CSceneFragments::CSceneFragments() : nSceneTris(0)//, pRejected(0), nRejectedUsed(0) 
{
	fragments.push_back( fragmentInfos.Alloc() );
	ASSERT( fragments.size() == 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSceneFragments::AddGeometry( CObjectBase *pHandle, SRenderGeometryInfo *pGeometry, const SBound &_bv )
{
	SRenderStaticInfo *pRes = staticInfos.Alloc();
	pRes->pHandle = pHandle;
	pRes->bv = _bv;
	statics.push_back( pRes );
	geometries.push_back( pGeometry );
	return geometries.size() - 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSceneFragments::AddElement( int _nGeometryIndex, const CPartFlags &_parts, IMaterial *pMaterial, 
	NGfx::CTexture *pLightmap, const SDynamicAmbientInfo *pLM )
{
	if ( _parts.IsEmpty() )
		return;
	// search for suitable fragment
	SRenderFragmentKey k;
	k.pMat = pMaterial;
	k.pLightmap = pLightmap;
	k.pLM = pLM;
	CFragmentHash::iterator i = fragmentHash.find( k );
	SRenderFragmentInfo *pFragment;
	if ( i == fragmentHash.end() )
	{
		fragmentHash[k] = fragments.size();
		pFragment = fragmentInfos.Alloc();
		pFragment->pMaterial = pMaterial;
		pFragment->pLightmap = pLightmap;
		pFragment->pLM = pLM;
		fragments.push_back( pFragment );
	}
	else
		pFragment = fragments[ i->second ];
	for ( int i = 0; i < _parts.GetBlocksNumber(); ++i )
	{
		int nFlags = _parts.GetBlock( i );
		if ( nFlags )
			pFragment->elements.push_back( SRenderFragmentInfo::SElement( _nGeometryIndex, i, nFlags ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSceneFragments::AddLitParticles( IVBCombiner *pCombiner, CFuncBase<vector<NGfx::STriangleList> > *pTris, int nPart, const SBound &_bv )
{
	SRenderGeometryInfo *pGeom = geometryInfos.Alloc();
	pGeom->pTriLists[TLT_POSITION] = pTris;
	pGeom->pTriLists[TLT_GEOM] = pTris;
	pGeom->pVertices = pCombiner;
	pGeom->pVertices.Refresh();
	int nGeom = AddGeometry( 0, pGeom, _bv );
	int nBlock = nPart / 32, nShift = nPart & 31;
	fragments[0]->elements.push_back( SRenderFragmentInfo::SElement( nGeom, nBlock, 1<<nShift ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSceneFragments::SetLitParticlesMaterial( IMaterial *p )
{
	ASSERT( fragments[0]->pMaterial == 0 || fragments[0]->pMaterial == p );
	fragments[0]->pMaterial = p;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// not exact realisation
bool CSceneFragments::HasSelectedFragments() const
{
	if ( fragments.size() == 1 && fragments[0]->elements.empty() )
		return false;
	if ( filterFragment.size() )
	{
		bool bAllAreFiltered = true;
		for ( int k = 0; k < fragments.size(); ++k )
		{
			if ( filterFragment[k] == 0 && !fragments[k]->elements.empty() )
			{
				bAllAreFiltered = false;
				break;
			}
		}
		if ( bAllAreFiltered )
			return false;
	}
	if ( filterGeometry.size() )
	{
		bool bAllAreFiltered = true;
		for ( int k = 0; k < filterGeometry.size(); ++k )
		{
			if ( filterGeometry[k] != FST_REJECT )
			{
				bAllAreFiltered = false;
				break;
			}
		}
		if ( bAllAreFiltered )
			return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
inline EFragmentsSplit GetIntersectLevel( const SBound &bound, const SBound &test )
{
	CVec3 ptDif = bound.s.ptCenter - test.s.ptCenter;
	// fast outside
	float fDif2 = fabs2(ptDif);
	if ( fDif2 > sqr( bound.s.fRadius + test.s.fRadius ) )
		return FST_REJECT;
	// fast inside, fDif + test.s.fRadius < bound.s.fRadius
	float fRDif = bound.s.fRadius - test.s.fRadius; 
	if ( fDif2 < fRDif * fabs( fRDif ) )
		return FST_ACCEPT;
	if ( 
		fabs(ptDif.x) + test.ptHalfBox.x < bound.ptHalfBox.x && 
		fabs(ptDif.y) + test.ptHalfBox.y < bound.ptHalfBox.y && 
		fabs(ptDif.z) + test.ptHalfBox.z < bound.ptHalfBox.z )
		return FST_ACCEPT;
	return FST_SPLIT;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline EFragmentsSplit GetIntersectLevel( const SSphere &bound, const SSphere &test )
{
	CVec3 ptDif = bound.ptCenter - test.ptCenter;
	// fast outside
	float fDif2 = fabs2(ptDif);
	if ( fDif2 > sqr( bound.fRadius + test.fRadius ) )
		return FST_REJECT;
	// fast inside, fDif + test.s.fRadius < bound.s.fRadius
	float fRDif = bound.fRadius - test.fRadius; 
	if ( fDif2 < fRDif * fabs( fRDif ) )
		return FST_ACCEPT;
	return FST_SPLIT;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool DoesIntersect( const SSphere &s, const SBound &bv )
{
	CVec3 v( bv.s.ptCenter - s.ptCenter );
	return
		fabs(v.x) - s.fRadius < bv.ptHalfBox.x &&
		fabs(v.y) - s.fRadius < bv.ptHalfBox.y &&
		fabs(v.z) - s.fRadius < bv.ptHalfBox.z;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool DoesIntersect( const SSphere &a, const SSphere &b )
{
	CVec3 ptDif = a.ptCenter - b.ptCenter;
	return fabs2(ptDif) < sqr( a.fRadius + b.fRadius );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Filter ops
////////////////////////////////////////////////////////////////////////////////////////////////////
EFragmentsSplit SBoundIntersectFilter::operator()( SRenderStaticInfo *pStatic, SRenderGeometryInfo *pGeom, CPartFlags *pRes ) const
{
	EFragmentsSplit res = GetIntersectLevel( bv, pStatic->bv );
	if ( res == FST_SPLIT )
	{
		const vector<SSphere> &bounds = pGeom->pVertices->GetBounds();
		for ( int k = 0; k < bounds.size(); ++k )
		{
			if ( !pRes->IsSet( k ) )
				continue;
			if ( !DoesIntersect( bounds[k], bv ) )
				pRes->Reset( k );
		}
	}
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EFragmentsSplit SFrustrumFilter::operator()( SRenderStaticInfo *pStatic, SRenderGeometryInfo *pGeom, CPartFlags *pRes ) const
{
	if ( !pTS->PushClipHint( pStatic->bv ) )
		return FST_REJECT;
	if ( pTS->IsFullGet() )
	{
		pTS->PopClipHint();
		return FST_ACCEPT;
	}
	const vector<SSphere> &bounds = pGeom->pVertices->GetBounds();
	for ( int k = 0; k < bounds.size(); ++k )
	{
		if ( !pRes->IsSet( k ) )
			continue;
		if ( !pTS->IsIn( bounds[k] ) )
			pRes->Reset( k );
	}
	pTS->PopClipHint();
	return FST_SPLIT;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EFragmentsSplit SSphereFilter::operator()( SRenderStaticInfo *pStatic, SRenderGeometryInfo *pGeom, CPartFlags *pRes ) const
{
	EFragmentsSplit res = GetIntersectLevel( sph, pStatic->bv.s );
	if ( res == FST_SPLIT )
	{
		const vector<SSphere> &bounds = pGeom->pVertices->GetBounds();
		for ( int k = 0; k < bounds.size(); ++k )
		{
			if ( !pRes->IsSet( k ) )
				continue;
			if ( !DoesIntersect( bounds[k], sph ) )
				pRes->Reset( k );
		}
	}
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SSphereAndIgnoredFilter
////////////////////////////////////////////////////////////////////////////////////////////////////
/*EFragmentsSplit SSphereAndIgnoredFilter::operator()( SRenderFragmentInfo *pF, const SSelectFragments &selector ) const
{
	ECullAcceptLevel l = GetIntersectLevel( sph, pF->pGeometry->pVertices->GetBound().s );
	if ( l == REJECT )
		return FST_REJECT;
	CFilterPartsHash::const_iterator ignored = ignoreList.find( pF->pStatic->pHandle );
	if ( ( l == FULL_GET || pF->bSkipPerPartTests ) && ignored == ignoreList.end() )
		return FST_ACCEPT;
	IVBCombiner *pVB = pF->pStatic->pGeometry->pVertices;
	const vector<SSphere> &partBVs = pVB->GetBounds();
	CPartFlags accepted, rejected;
	accepted.Clear();
	rejected.Clear();
	if ( ignored == ignoreList.end() )
	{
		for ( int k = 0; k < pVB->GetPartsNum(); ++k )
		{
			if ( pF->parts.IsSet( k ) )
			{
				if ( DoesIntersect( sph, partBVs[k] ) )
					accepted.Set( k );
				else
					rejected.Set( k );
			}
		}
	}
	else
	{
		CPartFlags check = pF->parts & (~ignored->second);
		rejected = pF->parts & ignored->second;
		if ( !check.IsEmpty() )
		{
			for ( int k = 0; k < pVB->GetPartsNum(); ++k )
			{
				if ( check.IsSet( k ) )
				{
					if ( DoesIntersect( sph, partBVs[k] ) )
						accepted.Set( k );
					else
						rejected.Set( k );
				}
			}
		}
	}
	return selector.Split( *pF, accepted, rejected );
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void MakeSingleOp( CRenderCmdList *pRes, CSceneFragments &src, bool bTakeLitParticles, ERenderOperation op,
	CRenderCmdList::UParameter _p1,
	CRenderCmdList::UParameter _p2,
	CRenderCmdList::UParameter _p3,
	int nStencilOp )
{
	const vector<SRenderFragmentInfo*> &fragments = src.GetFragments();
	for ( int k = bTakeLitParticles ? 0 : 1; k < fragments.size(); ++k )
	{
		if ( src.IsFilteredFragment( k ) )
			continue;
		const SRenderFragmentInfo &f = *fragments[k];
		SOpGenContext fi( &pRes->ops, &f );
		if ( f.pMaterial )
		{
			f.pMaterial->AddATOperations( &fi );
			if ( fi.HasAddedOps() )//bAlphaTest )
			{
				fi.AddOperation( op, 100, nStencilOp|DPM_EQUAL, 0, _p1, _p2, _p3 );
				continue;
			}
		}
		fi.AddOperation( op, 100, nStencilOp, 0, _p1, _p2, _p3 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void AddFinalOps( CRenderCmdList *pRes, CSceneFragments &src, ERenderPath rm, 
	ERenderOperation op, CRenderCmdList::UParameter _p1 )
{
	//fi.pRes = this;
	const vector<SRenderFragmentInfo*> &fragments = src.GetFragments();
	for ( int k = 1; k < fragments.size(); ++k )
	{
		if ( src.IsFilteredFragment( k ) )
			continue;
		const SRenderFragmentInfo &f = *fragments[k];
		SOpGenContext fi( &pRes->ops, &f );
		if ( op != RO_NOP )
			fi.AddOperation( op, 50, ABM_SMART|DPM_EQUAL, 0, _p1 );
		f.pMaterial->AddOperations( &fi, rm );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SplitOps( CRenderCmdList *pLower, CRenderCmdList *pSrc, int nHighPass )
{
	int nDest = 0;
	for ( int k = 0; k < pSrc->ops.size(); ++k )
	{
		if ( pSrc->ops[k].nPass < nHighPass )
			pLower->ops.push_back( pSrc->ops[k] );
		else
		{
			if ( nDest != k )
				pLower->ops[ nDest++ ] = pSrc->ops[k];
			else
				++nDest;
		}
	}
	pSrc->ops.resize( nDest );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGScene;
BASIC_REGISTER_CLASS( ILight )
