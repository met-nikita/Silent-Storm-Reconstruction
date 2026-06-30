#ifndef __MEMOBJECT_H_
#define __MEMOBJECT_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMemObject: public CObjectBase
{
	OBJECT_BASIC_METHODS(CMemObject);
	vector<CVec3> resPoints;
	vector<CVec3> resNormals;
	vector<STriangle> resTris;
	bool bPolyLine;
	//
public:
	CMemObject(): bPolyLine(false) {}
	void Clear();
	void CreateCube( const CVec3 &base, const CVec3 &size, bool bTwoSided = false );
	void CreateSphere( const CVec3 &ptCenter, float fRadius, int nSubs = 2 );
	void CreateCylinder( const CVec3 &ptStart, const CVec3 &ptEnd, float fRadius, int nSubs = 2, bool bClose = false );
	void CreatePolyline( const vector<CVec3> &points );
	void CreatePolygone( const vector<CVec2> &points, float fZ );
	void CreateFlag( const CVec3 &ptBase, float fHeight, float fRadius, float fLength );
	void CreateIsoscelesColumn( const CVec3 &ptBase, float fHeight, float fBase );
	void Create( const vector<CVec3> &points, const vector<STriangle> &tris );
	void CalcBound( SSphere *pRes ) const;
	void CalcBound( SBound *pRes ) const;
	bool IsPolyLine() const { return bPolyLine; }
	const vector<CVec3>& GetPoints() const { return resPoints; }
	const vector<CVec3>& GetNormals() const { return resNormals; }
	const vector<STriangle>& GetTris() const { return resTris; }
	friend class CMemObjectBuilder;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __MEMOBJECT_H_
