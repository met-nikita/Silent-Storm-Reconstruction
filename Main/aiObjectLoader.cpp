#include "StdAfx.h"
#include "aiObjectLoader.h"
#include "MemObject.h"
#include "..\Misc\BasicShare.h"
#include "aiObject.h"
#include "GFileSkin.h"
#include "BSPTree.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static CVec3 CalcMassCenter( vector<SMassSphere> &spheres )
{
	float fMassSum = 0;
	CVec3 massCenter = VNULL3;
	for ( int i = 0; i < spheres.size(); ++i )
	{
		if ( spheres[i].fMass <= 0 )
			spheres[i].fMass = 1;
		fMassSum += spheres[i].fMass;
		massCenter += spheres[i].fMass * spheres[i].ptCenter;
	}
	massCenter /= fMassSum;
	return massCenter;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLoadGeometryInfo
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SStoredPiece
{
	vector<STriangle> tris;
	vector<CVec3> verts;
	float fVolume;
	vector<SJunction> juncs;
	vector<CPtr<CPrecalcSpheres> > precalc;	// was vector<CPtr<CBSPTree> > trees (chunk 12 -> 13)

	int operator&( CStructureSaver &f )
	{
		f.Add( 1, &verts );
		f.Add( 2, &tris );
		f.Add( 10, &fVolume );
		f.Add( 11, &juncs );
		f.Add( 13, &precalc );	// release SStoredPiece::operator& @00483dd0 chunk '\r'
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef unordered_map<int, SStoredPiece > CStoredPieceMap;
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLoadGeometryInfo::Recalc()
{
	pValue = new CGeometryInfo;
	try
	{
		NGScene::CResourceOpener file( "AIGeometries", GetKey() );
		vector<CVec3> points;
		vector<STriangle> tris;
		vector<CPtr<CPrecalcSpheres> > precalc;

		CStoredPieceMap pieces;
		file->Add( 1, &points );
		file->Add( 2, &tris );
		file->Add( 4, &pieces );
		file->Add( 6, &pValue->spheres );
		file->Add( 9, &precalc );	// release Recalc @00481820 chunk '\t' (was 8 = CBSPTree)

		if ( pieces.empty() )
		{
			pValue->AddPiece( 0, points, tris, 0, vector<SJunction>(), true, precalc );
		}
		else
		{
			for ( CStoredPieceMap::const_iterator i = pieces.begin(); i != pieces.end(); ++i )
			{
				pValue->AddPiece( i->first, i->second.verts, i->second.tris, i->second.fVolume, i->second.juncs, true, i->second.precalc );
			}
		}
		pValue->CalcBound();
		pValue->massCenter = CalcMassCenter( pValue->spheres );
	}
	catch(...)
	{
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLoadTwoBSPTrees
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLoadTwoBSPTrees::Recalc()
{
	pValue = new CTwoBSPTrees;
	try
	{
		NGScene::CResourceOpener file( "AIBSPTrees", GetKey() );
		// release CLoadTwoBSPTrees::Recalc @00481b60: chunk 3 = open, chunk 4 = closed (was 1/2)
		file->Add( 3, &pValue->treesOpen );
		file->Add( 4, &pValue->treesClosed );
		// release re-inits each loaded sphere's bound from ptMin/ptMax
		for ( unsigned int s = 0; s < pValue->treesOpen.size(); ++s )
			for ( CPrecalcPieces::iterator it = pValue->treesOpen[s].begin(); it != pValue->treesOpen[s].end(); ++it )
				it->second->bound.BoxInit( it->second->ptMin, it->second->ptMax );
		for ( unsigned int s = 0; s < pValue->treesClosed.size(); ++s )
			for ( CPrecalcPieces::iterator it = pValue->treesClosed[s].begin(); it != pValue->treesClosed[s].end(); ++it )
				it->second->bound.BoxInit( it->second->ptMin, it->second->ptMax );
	}
	catch(...)
	{
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMemGeometryInfo
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMergePoints
{
	vector<CVec3> points;
	vector<STriangle> tris;

	int GetPointIndex( const CVec3 &a )
	{
		for ( int k = 0; k < points.size(); ++k )
		{
			if ( points[k] == a )
				return k;
		}
		points.push_back( a );
		return points.size() - 1;
	}
	void AddTriangle( const CVec3 &a, const CVec3 &b, const CVec3 &c )
	{
		tris.push_back( STriangle( GetPointIndex(a), GetPointIndex(b), GetPointIndex(c) ) );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMemGeometryInfo::Recalc()
{
	pValue = new CGeometryInfo;
	SMergePoints p;
	const vector<CVec3> &points = pMemObject->GetPoints();
	const vector<STriangle> &tris = pMemObject->GetTris();
	for ( int k = 0; k < tris.size(); ++k )
		p.AddTriangle( points[tris[k].i1], points[tris[k].i2], points[tris[k].i3] );
	pValue->AddPiece( -1, p.points, p.tris, 0 );
	pValue->CalcBound();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFileSkinPointsLoad
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SStoredSkin
{
	vector<CVec3> points;
	vector<STriangle> tris;
	vector<NGScene::SLoadVertexWeight> weights;

	int operator&( CStructureSaver &f )
	{ 
		f.Add( 1, &points );
		f.Add( 2, &tris );
		f.Add( 3, &weights );
		return 0;
	}
};
typedef unordered_map<int, SStoredSkin> CBodypartsStoredHash;
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFileSkinPointsLoad::Recalc()
{
	pValue = new CFileSkinPoints;

	NGScene::CResourceOpener file( "AIGeometries", GetKey() );
	CBodypartsStoredHash data;
	file->Add( 4, &data );
	file->Add( 6, &pValue->spheres );
	pValue->massCenter = CalcMassCenter( pValue->spheres );
	for ( CBodypartsStoredHash::const_iterator i = data.begin(); i != data.end(); ++i )
	{
		const SStoredSkin &src = i->second;
		CFileSkinPoints::SBodypart &r = pValue->parts[i->first];
		r.points = src.points;
		r.edges.GenerateEdgeList( src.tris, src.points );
		NGScene::ConvertWeights( &r.weights, src.weights, src.points.size() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSkinner
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSkinner::Recalc()
{
	const CFileSkinPoints &src = *pSkin->GetValue();
	if ( !IsValid( pValue ) )
	{
		pValue = new CGeometryInfo;
		for ( CFileSkinPoints::CBodypartsHash::const_iterator i = src.parts.begin(); i != src.parts.end(); ++i )
		{
			CGeometryInfo::SPiece &dst = pValue->pieces[i->first];
			const CFileSkinPoints::SBodypart &src = i->second;
			dst.edges = src.edges;
			ASSERT( src.edges.IsClosed() );
			dst.points.resize( src.points.size() );
		}
	}
	//pValue->points.resize( nVertices );
	typedef CVec3 SVertex;
	using NGScene::SVertexWeight;
	
	for ( CFileSkinPoints::CBodypartsHash::const_iterator i = src.parts.begin(); i != src.parts.end(); ++i )
	{
		CGeometryInfo::SPiece &dst = pValue->pieces[i->first];
		const CFileSkinPoints::SBodypart &src = i->second;
		ASSERT( src.points.size() == dst.points.size() );
		int nVertices = src.points.size();
		SVertex *pRes = &dst.points[0];
		const SVertex *pMesh = &src.points[0];
		const SVertexWeight *pWeight = &src.weights[0];
		const NGScene::SSkeletonMatrices &blends = pAnimation->GetValue();
		memset( pRes, 0, sizeof(SVertex) * nVertices );
		CVec3 p;
		for ( int i = 0; i < nVertices; ++i )
		{
			int j = 0;
			while ( pWeight->fWeights[j] && j < 4 )
			{
				blends[ pWeight->cBoneIndices[j] ].RotateHVector( &p, *pMesh );
				*pRes += pWeight->fWeights[j] * p;
				++j;
			}
			pRes++;
			pMesh++;
			pWeight++;
		}
	}
	pValue->CalcBound();
	if ( bDoPrecalc )
		pValue->PrecalcCollideInfo();
	else if ( !setTrees.empty() )
	{
		pValue->SetCollideInfo( setTrees );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NAI;
REGISTER_SAVELOAD_CLASS( 0x012c1160, CLoadGeometryInfo )
REGISTER_SAVELOAD_CLASS( 0x012c1161, CMemGeometryInfo )
REGISTER_SAVELOAD_CLASS( 0x014c1110, CFileSkinPointsLoad )
REGISTER_SAVELOAD_CLASS( 0x014c1111, CSkinner )
REGISTER_SAVELOAD_CLASS( 0x73102120, CTwoBSPTrees )
REGISTER_SAVELOAD_CLASS( 0x73102121, CLoadTwoBSPTrees )
REGISTER_SAVELOAD_CLASS( 0x72813140, CPrecalcSpheres )	// retail saveload id (from initCPrecalcSpheres0x72813140)
