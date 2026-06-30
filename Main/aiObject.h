#ifndef __AIOBJECT_H_
#define __AIOBJECT_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "../DBFormat/DataRPG.h"
#include "../Misc/2Darray.h"
#include "aiPMConst.h"		// NAI::F_TEST_SPHERE_RADIUS (unit collision radius, 0.31f)
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SEdge
{
	WORD wStart, wFinish;
	//
	SEdge() {}
	SEdge( WORD _wS, WORD _wF ): wStart(_wS), wFinish(_wF) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEdgesInfo
{
	WORD InsertEdge( WORD i1, WORD i2, const vector<CVec3> &pts );
public:
	vector<SEdge> edges;
	vector<STriangle> mesh;
	bool bClosed;
	//
	CEdgesInfo() { bClosed = true; }
	void BuildTriangleList( vector<STriangle> *pRes ) const;
	void BuildClosedMeshes( vector<vector<STriangle> > *pMeshes ) const;
	void GenerateEdgeList( const vector<STriangle> &tris, const vector<CVec3> &pts );
	bool IsClosed() const; // checks geometry
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SJunction
{
	CVec3 pt;
	bool  bGround;

	SJunction() {}
	SJunction( const CVec3 &_pt, bool _bGr = false ): pt(_pt), bGround(_bGr) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBSPTree;
class CPrecalcSpheres;	// release replaced the per-piece CBSPTree collision with CPrecalcSpheres
typedef unordered_map<int, CPtr<CBSPTree> > CBSPPieces;
typedef unordered_map<int, CPtr<CPrecalcSpheres> > CPrecalcPieces;
class CGeometryInfo: public CObjectBase
{
	OBJECT_BASIC_METHODS(CGeometryInfo);
public:
	struct SPiece
	{
		vector<CVec3> points;
		CEdgesInfo edges;
		float fVolume;
		vector<SJunction> juncs;
		vector<CPtr<CPrecalcSpheres> > precalc;	// was vector<CPtr<CBSPTree> > trees
	};
	typedef std::unordered_map<int, SPiece> CPieceMap;

	SBound bound;
	vector<SMassSphere> spheres;
	CVec3 massCenter;
	CPieceMap pieces;
	//
	SPiece* GetPiece( int nPieceID );
	void AddPiece( int nPieceID, const vector<CVec3> &_points, const vector<STriangle> &_tris,
		float fVolume, vector<SJunction> juncs = vector<SJunction>(), bool _bClosed = true,
		vector<CPtr<CPrecalcSpheres> > precalc = vector<CPtr<CPrecalcSpheres> >() );
	void CalcBound();
	void PrecalcCollideInfo( bool bTerrain = false );	// release rename of CalcBSPTrees
	void SetCollideInfo( const CPrecalcPieces &precalc );	// release rename of SetBSPTrees
	bool HasPiece( int nID ) const { return pieces.find( nID ) != pieces.end(); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPrecalcSpheres - release-added precomputed sphere-collision grid for a geometry piece. The shipped
// AIGeometries .res packages embed these objects (saveload type id 0x72813140). The dev AI doesn't
// use them, but must be able to deserialize them so CStructureSaver::Start's object-table phase
// doesn't fail on an unregistered type. Layout + operator& reconstructed verbatim from the release
// Game.exe (NAI::CPrecalcSpheres @0x42b1c0: Do2DArrayData<int> isCollided + raw bound/ptMin/ptMax).
////////////////////////////////////////////////////////////////////////////////////////////////////
// Precalc voxel-grid collision constants (release NAI globals). F_PRECALC_STEP = 0.1 is the grid cell
// size; the release indexes the grid with the literal reciprocal 10.0f (which is NOT exactly
// 1.0f/0.1f in float, so keep both as distinct literals to match the binary).
const float F_PRECALC_STEP = 0.1f;
const float F_INV_PRECALC_STEP = 10.0f;
// F_TEST_SPHERE_RADIUS (= 0.31f, the unit collision radius and the bake test-sphere radius, release
// .rdata @0x008c1b60) lives in aiPMConst.h; it's used by Generate and the collider's bound padding.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPrecalcSpheres : public CObjectBase
{
	OBJECT_BASIC_METHODS(CPrecalcSpheres);
public:
	CArray2D<int> isCollided;	// +0x0c  (Do2DArrayData)
	SBound        bound;		// +0x1c  (raw 28 bytes)
	CVec3         ptMin;		// +0x38  (raw 12 bytes)
	CVec3         ptMax;		// +0x44  (raw 12 bytes)

	int operator&( CStructureSaver &f )
	{
		f.Add( 2, &isCollided );
		f.Add( 3, &bound );
		f.Add( 4, &ptMin );
		f.Add( 5, &ptMax );
		return 0;
	}
	// Collision queries against the baked voxel grid (release @0068cc80 / @0068cd20). isCollided[y][x]
	// is a 32-bit Z-bitmask; cell = Float2Int( (p - ptMin) * F_INV_PRECALC_STEP - 0.5 ).
	bool IsSphereCollided( const CVec3 &p ) const;
	bool IsMovingSphereCollided( const CVec3 &p1, const CVec3 &p2 ) const;
	// Bake the voxel grid for the nZBlock-th 32-slab Z block (release @0068cef0). Returns true if the
	// geometry extends past this block (caller should generate the next one).
	bool Generate( const vector<CVec3> &points, const vector<STriangle> &tris, int nZBlock );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Build the full stack of CPrecalcSpheres Z-blocks for a mesh (release NAI::GeneratePrecalcSpheres
// @0068d3f0). Used as the collider's runtime fallback when a hull has no precomputed spheres.
void GeneratePrecalcSpheres( vector<CPtr<CPrecalcSpheres> > *pRes, const vector<CVec3> &points,
	const vector<STriangle> &tris );
////////////////////////////////////////////////////////////////////////////////////////////////////
//! structure describing object for different tracers
struct SSourceInfo;
struct SConvexHull
{
	const vector<CVec3> &points;
	const CEdgesInfo &tris;
	const SFBTransform &trans;
	const SSourceInfo &src;
	int nUserID;
	const vector<CPtr<CPrecalcSpheres> > precalc;	// was vector<CPtr<CBSPTree> > trees
	//
	SConvexHull( const vector<CVec3> &_points, const CEdgesInfo &_tris, const SFBTransform &_trans,
		SSourceInfo &_src, int _nUserID, const vector<CPtr<CPrecalcSpheres> > _precalc )
		: points(_points), tris(_tris), trans(_trans), src(_src), nUserID(_nUserID), precalc(_precalc) {}
	int operator&( CStructureSaver &f ) { ASSERT(0&&"This struct could not be serialized!"); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//! group of entities; entity terrain is represented with several SConvexHull
struct SHullSet
{
	vector<SConvexHull> objects, terrain;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif