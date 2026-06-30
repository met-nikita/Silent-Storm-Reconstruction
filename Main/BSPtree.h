#ifndef __BSPTree_H_
#define __BSPTree_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//#define _BSP_DEBUG
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
const float fBSPEpsilon = 0.01f;
const float fBSPEpsilonCubed = fBSPEpsilon * fBSPEpsilon * fBSPEpsilon;
const float fAngleEpsilon = 0.1f;//10 * fBSPEpsilon;
externA5 bool bWasPlaneSet;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBSPTree : public CObjectBase
{
	OBJECT_BASIC_METHODS(CBSPTree);
	friend class CBSPTreeConstructor;

	// here and later we assume that divider.n is already normalized i.e. fabs(divider.n)=1
	vector<SPlane> dividers;
	vector<int> rightIndices;
	vector<int> leftIndices;
	int operator&( CStructureSaver &f )
	{
		f.Add( 1, &dividers );
		f.Add( 2, &rightIndices );
		f.Add( 3, &leftIndices );
		return 0;
	}

	bool IsLeafNode( int nIndex ) const { return rightIndices[ nIndex ] == 0 && leftIndices[ nIndex ] == 0; }
	bool IsFilledNode( int nIndex ) const
	{ 
		ASSERT( IsLeafNode( nIndex ) );
		return dividers[ nIndex ].d > 0;
	}
	bool CollideCheckInternal( const CVec3 &v0, const CVec3 &v1, float fRadius, CVec3 *pImpact, SPlane *pPlane, 
		int nLeafIndex, const SPlane *pSetPlane, bool bInvert );
public:
	// next functions are used only when constructing a tree
	void AddBound( const CVec3 &norm, float d, int nIndex, int *pSpareIndex );
	void CreateBSPTerrainPart( float fMinX, float fMaxX, float fMinY, float fMaxY, 
	const vector<CVec3> &points, const vector<STriangle> &mesh, int nIndex, int *pSpareIndex );
	friend CBSPTree* CreateTerrainBSPTree( const vector<CVec3> &points, const vector<STriangle> &mesh );
	void CheckConsistency();
	CBSPTree() {}
	// sphere intersection test
	bool DoesIntersect( const CVec3 &v, float fRadius, int nLeafIndex = 0 );
	// moving sphere intersection tests
	bool CollideCheck( const CVec3 &v0, const CVec3 &v1, float fRadius, CVec3 *pImpact, SPlane *pPlane )
	{
		//*pPlane = dividers[0];
		bWasPlaneSet = false;
		return CollideCheckInternal( v0, v1, fRadius, pImpact, pPlane, 0, 0, false );
	}
	bool CollideCheckNoImpact( const CVec3 &v0, const CVec3 &v1, float fRadius, int nLeafIndex = 0 );
	bool CollideCheckZeroRad( const CVec3 &v0, const CVec3 &v1, CVec3 *pImpact, SPlane *pPlane, int nLeafIndex = 0 );

	int CalcNodes() const
	{
		return dividers.size();
	}

	int CalcDepth( int nIndex = 0 ) const
	{
		int nRet = 0;
		if ( rightIndices[ nIndex ] )
			nRet = CalcDepth( rightIndices[ nIndex ] ) + 1;
		if ( leftIndices[ nIndex ] )
			nRet = Max( nRet, CalcDepth( leftIndices[ nIndex ] ) + 1 );
		return nRet;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CBSPTree* CreateBSPTree( const vector<CVec3> &points, const vector<STriangle> &mesh );
CBSPTree* CreateTerrainBSPTree( const vector<CVec3> &points, const vector<STriangle> &mesh );
CBSPTree* BSPChecker();
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif