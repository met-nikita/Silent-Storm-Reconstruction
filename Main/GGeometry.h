#ifndef __GGeometry_H_
#define __GGeometry_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "GPixelFormat.h"
#include "RectPacker.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
//const int N_LM_SIZE_LOG = 10;//9;
//const int N_LM_SIZE = 1 << N_LM_SIZE_LOG;//128;
const int N_MAX_LIGHTMAP_SIDE = 32;
const float F_MAX_LM_RESOLUTION = 20;
const int N_LM_LODS = 5;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SVertex
{
	CVec3 pos;
	CVec3 normal;
	CVec2 tex;
	CVec3 texU, texV;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_VERTEX_TEX_SIZE = 2048;
struct SUVInfo
{
	NGfx::SShortTextureUV tex;
	NGfx::SCompactVector normal, texU, texV;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SVertexWeight
{
	float fWeights[4];
	BYTE cBoneIndices[4];
	bool operator==( const SVertexWeight &v ) const { return memcmp( this, &v, sizeof(*this) ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRealVertexWeight
{
	float fWeights[4];
	BYTE nWeights[4];
	BYTE cBoneIndices[4];
	bool operator==( const SRealVertexWeight &v ) const { return memcmp( this, &v, sizeof(*this) ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline SVertexWeight GetWeight( const SRealVertexWeight &a )
{
	SVertexWeight res;
	for ( int k = 0; k < 4; ++k ) { res.fWeights[k] = a.fWeights[k]; res.cBoneIndices[k] = a.cBoneIndices[k]; }
	return res;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPolygonIndices
{
	ZDATA
	vector<WORD> indices;
	vector<WORD> polys;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&indices); f.Add(3,&polys); return 0; }
	
	void GetTriangles( vector<STriangle> *pRes ) const;
	void SetTriangles( const vector<STriangle> &tris );
	void AddTriangles( const vector<STriangle> &tris, int nBaseIndex );
	int GetTrianglesCount() const { if ( polys.empty() ) return 0;  return indices.size() - ( polys.size() - 1 ) * 2;}
	void Filter( const vector<WORD> &posIndices );
	void Clear() { indices.resize(0); polys.resize(1); polys[0] = 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectInfo : public CObjectBase
{
	OBJECT_BASIC_METHODS(CObjectInfo);
public:
	struct SLightmapInfo
	{
		WORD n1, n2, n3, n4;    // vertices indexing
		char nOrder1, nOrder2;  // first(1,2,3) | 0x4 for inverse
	};
	struct SData
	{
		ZDATA
		vector<SVertex> verts;
		SPolygonIndices geometry;
		vector<SVertexWeight> weights;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&verts); f.Add(3,&geometry); f.Add(4,&weights); return 0; }
	};
	struct SLMLOD
	{
		vector<NRectPacker::SRect> lmaps;
		CTPoint<int> lmSize;  // in pixels
	};
private:
	vector<CVec3> positions;
	vector<SUVInfo> verts;
	vector<SRealVertexWeight> weights;
	vector<SLightmapInfo> lmaps;
	vector<WORD> posIndices; // references to positions for each vert
	vector<WORD> vertRefPositions; // number of first position encounter for each vertex
	vector<WORD> lmRefVertices; // number of first vertex encounter for all lm buffer
	vector<WORD> lmRefPositions; // number of first vertex encounter for all lm buffer
	SPolygonIndices geometry;
	vector<SLMLOD> lmLODs;
	int nTris;

	void EstablishRefs();
	void MergePositions();
	void AssignGeometry( const SData &data );

public:
	CObjectInfo() : nTris(0) {}	
	void Assign( const SData &data );
	void AssignLM( const SData &data );
	void AssignFast( const SData &data );
	const vector<CVec3>& GetPositions() const { return positions; }
	const vector<SUVInfo>& GetVertices() const { return verts; }
	const vector<SRealVertexWeight>& GetWeights() const { return weights; }
	const vector<SLightmapInfo>& GetLMInfo() const { return lmaps; }
	const SLMLOD& GetLMMap( int nLOD ) const { return lmLODs[ nLOD ]; }
	const vector<WORD>& GetPositionIndices() const { return posIndices; }
	const CTPoint<int>& GetLMSize( int nLOD ) const { return lmLODs[ nLOD ].lmSize; }
	const SPolygonIndices& GetGeometry() const { return geometry; }

	void GetLMTriangles( vector<STriangle> *pRes ) const;
	void GetLMVerticesTriangles( vector<STriangle> *pRes ) const;
	void GetLMPositionTriangles( vector<STriangle> *pRes ) const;
	void GetVxVerticesTriangles( vector<STriangle> *pRes ) const;
	void GetVxPositionTriangles( vector<STriangle> *pRes ) const;
	void GetPosTriangles( vector<STriangle> *pRes ) const; // over positions[]
	int GetTrisCount() { return nTris; }
	
	bool IsEmpty() { return nTris == 0; }
	void CalcBound( SBound *pRes );
	void CalcBound( SSphere *pRes );
	friend class CSquarePacker;
};
void FilterTrinagles( vector<STriangle> *pRes, const vector<WORD> &filter );
void MergePositions( vector<WORD> *pMatches, vector<CVec3> *pPositions );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
