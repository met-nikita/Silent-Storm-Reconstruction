#include "StdAfx.h"
#include "bspTree.h"
#include "aiVolumeCalcer.h"
#include "Grid.h"
namespace NAI
{
#undef ASSERT
#    define ASSERT( a ) if ( !(a) ) __debugbreak();
#pragma optimize("", off)
#pragma optimize("p", on)
const int N_BSP_DIVISION_COST = 3;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAngleEdge
{
	float fAngle1, fAngle2;
	bool bIgnored;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAnglePoint
{
	CVec3 vec;
	float u, v;
	float fAngle;
	bool bIgnored;
	int nIntersected;
	int nAHalf, nBHalf, nOnBound;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAddedPoint
{
	CVec3 pt;
	bool bLeft;
	bool bRight;
	SAddedPoint( const CVec3 &_pt ) : pt(_pt) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAddedEdge
{
	CVec3 pt1, pt2;
	bool bChecked;
	SAddedEdge( const CVec3& p1, const CVec3& p2 ): pt1(p1), pt2(p2), bChecked(false) {}
	SAddedEdge( const CVec3& p1, const CVec3& p2, bool b ): pt1(p1), pt2(p2), bChecked(b) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CalcTriangleSize( const CVec3 &v1, const CVec3 &v2, const CVec3 &v3 ) 
{
	CVec3 diff1 = v1 - v2;
	CVec3 diff2 = v1 - v3;
	return diff1 ^ diff2;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SBSPPoly
{
	struct SPoint
	{
		CVec3 pt;
		bool bNew;
		SPoint( const CVec3 &_pt ) : pt(_pt), bNew(false) {}
		SPoint( const CVec3 &_pt, bool _bNew ) : pt(_pt), bNew(_bNew) {}
	};
	vector<SPoint> pts;
	CVec3 norm;
	bool bTerminal;
	SBSPPoly() : bTerminal(false) {}
	unsigned int Prev( unsigned int index ) const { return index > 0 ? index - 1 : pts.size() - 1; } 
	unsigned int Succ( unsigned int index ) const { return ( index < pts.size() - 1 ) ? index + 1 : 0; } 
	void PushNewPoint( const CVec3 &pt, vector<SAddedEdge> *pAddedEdges )
	{
		if ( !pts.empty() )
		{
			bool bAdded = pts.back().bNew;
			if ( bAdded )
				pAddedEdges->push_back( SAddedEdge( pt, pts.back().pt ) );
		}
		pts.push_back( SPoint( pt, true ) );
	}

	void CheckLastEdge( vector<SAddedEdge> *pAddedEdges )
	{
		if ( pts.size() < 2 )
			return;
		if ( pts.back().bNew && pts.front().bNew )
			pAddedEdges->push_back( SAddedEdge( pts.front().pt, pts.back().pt ) );
	}
	void AssignRevised( SBSPPoly *pPoly, const SPlane &plane, vector<SAddedEdge> *pDangerousEdges, bool bLeft ) const
	{
		for ( unsigned int i = 0; i < pts.size(); ++i )
			pPoly->pts.push_back( pts[i].pt );
		for ( unsigned int i = 0; i < pts.size() - 1; ++i )
			pDangerousEdges->push_back( SAddedEdge( pts[i].pt, pts[i+1].pt, bLeft ) );
		pDangerousEdges->push_back( SAddedEdge( pts[ pts.size() - 1 ].pt, pts[0].pt, bLeft ) );
		pPoly->bTerminal = true;
	}
	bool SetPlane( SPlane *pPlane ) const
	{
		if ( fabs2( norm ) <= 0.99 || fabs2( norm ) >= 1.01 )
			return false;
		pPlane->n = norm;
		pPlane->d = - pts[0].pt * norm;
		return true;
	}
	void Divide( const SPlane &plane, SBSPPoly *pLeft, SBSPPoly *pRight, 
		vector<SAddedEdge> *pAddedEdgesL, vector<SAddedEdge> *pAddedEdgesR, vector<SAddedEdge> *pDangerousEdges ) const
	{
		pRight->bTerminal = bTerminal;
		pLeft->bTerminal = bTerminal;
		pRight->norm = pLeft->norm = norm;
		bool bNowLeft;
		bool bOnPlane = true;
		int iStart;
		//CVec3 rightDirection = plane.n ^ norm;
		for ( unsigned int i = 0; i < pts.size(); ++i )
		{
			const CVec3 &pt = pts[i].pt;
			float fWhere = pt * plane.n + plane.d;
			if ( fabs( fWhere ) >= fBSPEpsilon )
			{
				bOnPlane = false;
				iStart = i;
				bNowLeft = fWhere < 0;
				break;
			}
		}
		if ( bOnPlane )
		{ 
			if ( norm * plane.n > 0 ) 
				AssignRevised( pLeft, plane, pDangerousEdges, true );
			else
				AssignRevised( pRight, plane, pDangerousEdges, false );
			return;
		}
		int i = iStart;
		do
		{
			CVec3 ptStart = pts[i].pt;
			CVec3 ptFinish = pts[ Succ(i) ].pt;
			CVec3 dir = ptFinish - ptStart; 
			Normalize( &dir );
			float fStart = ptStart * plane.n + plane.d;
			float fFinish = ptFinish * plane.n + plane.d;
			if ( fabs( fStart ) < fBSPEpsilon && fabs( fFinish ) < fBSPEpsilon )
			{
				// edge lies entirely on the plane. check its direction against the one forming the right-handed triple
				if ( bNowLeft )
				{
					pLeft->pts.push_back( ptStart );
					pAddedEdgesL->push_back( SAddedEdge( ptFinish, ptStart ) );
				}
				else
				{
					pRight->pts.push_back( ptStart );
					pAddedEdgesR->push_back( SAddedEdge( ptFinish, ptStart ) );
				}
				i = Succ( i );
				continue;
			}
			if ( fabs( fStart ) > fBSPEpsilon )
			{
				// if the start does not lie on the plane. add it where needed and move on to the next edge
				if ( fStart > fBSPEpsilon )
					pRight->pts.push_back( ptStart );
				else 
					pLeft->pts.push_back( ptStart );
				if ( fStart * fFinish < 0 && fabs( fFinish ) > fBSPEpsilon )
				{
					CVec3 ptToAdd = ( ptStart * fFinish - ptFinish * fStart ) / ( fFinish - fStart );
					pRight->PushNewPoint( ptToAdd, pAddedEdgesR );
					pLeft->PushNewPoint( ptToAdd, pAddedEdgesL );
				}
			}
			else
			{
				// start lies on the plane
				//ptStart = ptStart - plane.n * fStart;
				if ( ( fFinish > fBSPEpsilon && bNowLeft ) || ( fFinish < -fBSPEpsilon && !bNowLeft ) )
				{
					pRight->PushNewPoint( ptStart, pAddedEdgesR );
					pLeft->PushNewPoint( ptStart, pAddedEdgesL );
				}
				else
				{
					if ( bNowLeft )
						pLeft->pts.push_back( ptStart );
					else
						pRight->pts.push_back( ptStart );
				}
			}
			if ( fFinish > fBSPEpsilon )
				bNowLeft = false;
			if ( fFinish < -fBSPEpsilon )
				bNowLeft = true;
			i = Succ( i );
		}
		while ( i != iStart );
		pLeft->CheckLastEdge( pAddedEdgesL );
		pRight->CheckLastEdge( pAddedEdgesR );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
float CalculatePolygonSquare( const SBSPPoly &poly )
{
	CVec3 vSquare = VNULL3;
	for ( unsigned int i = 1; i < poly.pts.size() - 1; ++i )
		vSquare += CalcTriangleSize( poly.pts[0].pt, poly.pts[i].pt, poly.pts[i + 1].pt );
	return fabs( vSquare ) / 2;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBSPTreeConstructor;
struct SBSPGeometry
{
	vector<SBSPPoly> polygons;
	bool CheckClosedEdge( const CVec3 &pt1, const CVec3 &pt2 ) const
	{
		int nTimes1 = 0, nTimes2 = 0;
		for ( unsigned int i = 0; i < polygons.size(); ++i )
		{
			const SBSPPoly &poly = polygons[i];
			for ( unsigned int k = 0; k < poly.pts.size(); ++k )
			{
				if ( poly.pts[k].pt == pt1 && poly.pts[ poly.Succ(k) ].pt == pt2 )
					++nTimes1;
				if ( poly.pts[k].pt == pt2 && poly.pts[ poly.Succ(k) ].pt == pt1 )
					++nTimes2;
			}
		}
		ASSERT( nTimes1 == nTimes2 );
		return( nTimes1 == nTimes2 );
	}
	bool CheckClosed() const
	{
		for ( unsigned int i = 0; i < polygons.size(); ++i )
		{
			const SBSPPoly &poly = polygons[i];
			for ( unsigned int k = 0; k < poly.pts.size(); ++k )
				if ( !CheckClosedEdge( poly.pts[k].pt, poly.pts[ poly.Succ(k) ].pt ) )
					return false;
		}
		return true;
	}
	bool CheckConsistentEdge( const CVec3 &pt1, const CVec3 &pt2, const vector<SAddedEdge> &ae, const CVec3 planeNorm ) const
	{
		int nTimes1 = 0, nTimes2 = 0;
		for ( unsigned int i = 0; i < polygons.size(); ++i )
		{
			const SBSPPoly &poly = polygons[i];
			for ( unsigned int k = 0; k < poly.pts.size(); ++k )
			{
				if ( poly.pts[k].pt == pt1 && poly.pts[ poly.Succ(k) ].pt == pt2 )
					++nTimes1;
				if ( poly.pts[k].pt == pt2 && poly.pts[ poly.Succ(k) ].pt == pt1 )
					++nTimes2;
			}
		}
		for ( unsigned int i = 0; i < ae.size(); ++i )
		{
			if ( ae[i].bChecked )
				continue;
			if ( ae[i].pt1 == pt1 && ae[i].pt2 == pt2 )
				++nTimes1;
			if ( ae[i].pt1 == pt2 && ae[i].pt2 == pt1 )
				++nTimes2;
		}
		ASSERT( nTimes1 == nTimes2 );
		return( nTimes1 == nTimes2 );
	}
	bool CheckConsistency( const vector<SAddedEdge> &ae, const CVec3 planeNorm )
	{
		for ( unsigned int i = 0; i < polygons.size(); ++i )
		{
			const SBSPPoly &poly = polygons[i];
			for ( unsigned int k = 0; k < poly.pts.size(); ++k )
				if ( !CheckConsistentEdge( poly.pts[k].pt, poly.pts[ poly.Succ(k) ].pt, ae, planeNorm ) )
					return false;
		}
		return true;
	}
	SAddedEdge *GetNext( const CVec3 &ptStart, vector<SAddedEdge> &edges, CVec3 *pCurr ) const
	{
		for ( unsigned int i = 0; i < edges.size(); ++i )
		{
			if ( edges[i].bChecked )
				continue;
			if ( edges[i].pt1 == ptStart )
			{
				*pCurr = edges[i].pt2;
				return &edges[i];
			}
		}
		return 0;
	}
	bool GetPoly( vector<SAddedEdge> &edges, SBSPPoly *pPoly ) const
	{
		pPoly->pts.clear();
		SAddedEdge *pCurrent = 0;
		CVec3 ptCurr, ptStart;
		for ( unsigned int i = 0; i < edges.size(); ++i )
		{
			if ( !edges[i].bChecked )
			{
				pCurrent = &edges[i];
				ptStart = pCurrent->pt1;
				ptCurr = pCurrent->pt2;
				break;
			}
		}
		if ( pCurrent == 0 )
			return false;
		while ( pCurrent )
		{
			pCurrent->bChecked = true;
			pPoly->pts.push_back( ptCurr );
			CVec3 ptLast( ptCurr );
			pCurrent = GetNext( ptLast, edges, &ptCurr );
		}
		ASSERT( ptStart == ptCurr ); 
		return true;
	}
	bool DeleteDangerEdge( const SAddedEdge &e, vector<SAddedEdge> *pAddedL, vector<SAddedEdge> *pAddedR ) const
	{
		vector<SAddedEdge> *pAdded = e.bChecked ? pAddedL : pAddedR;
		for ( unsigned int i = 0; i < pAdded->size(); ++i )
		{
			SAddedEdge &test = (*pAdded)[i];
			if ( e.pt1 == test.pt1 && e.pt2 == test.pt2 && (!test.bChecked) )
			{
				test.bChecked = true;
				return true;
			}
		}
		pAdded->push_back( SAddedEdge( e.pt2, e.pt1 ) );
		return false;
	}
	void Divide( const SPlane &plane, SBSPGeometry *pLeft, SBSPGeometry *pRight ) const
	{
		vector<SAddedEdge> addedEdgesL, addedEdgesR, dangerEdges;
		for ( unsigned int i = 0; i < polygons.size(); ++i )
		{
			SBSPPoly left, right;
			polygons[i].Divide( plane, &left, &right, &addedEdgesL, &addedEdgesR, &dangerEdges );
			if ( left.pts.size() > 2 )
				pLeft->polygons.push_back( left );
			else
				ASSERT(	left.pts.empty() );
			if ( right.pts.size() > 2 )
				pRight->polygons.push_back( right );
			else
				ASSERT( right.pts.empty() );
		}
		if ( pLeft->polygons.empty() || pRight->polygons.empty() )
			return;
		SBSPPoly newPoly;
		for ( unsigned int i = 0; i < dangerEdges.size(); ++i )
			DeleteDangerEdge( dangerEdges[i], &addedEdgesL, &addedEdgesR );
		//		ASSERT( pLeft->CheckConsistency( addedEdgesL, plane.n ) );
		//		ASSERT( pRight->CheckConsistency( addedEdgesR, plane.n ) );
		while ( GetPoly( addedEdgesL, &newPoly ) )
		{
			if ( newPoly.pts.size() < 3 )
				continue;
			newPoly.bTerminal = true;
			newPoly.norm = plane.n;
			pLeft->polygons.push_back( newPoly );
		}
		while ( GetPoly( addedEdgesR, &newPoly ) )
		{
			if ( newPoly.pts.size() < 3 )
				continue;
			newPoly.bTerminal = true;
			newPoly.norm = -plane.n;
			pRight->polygons.push_back( newPoly );
		}
	}

	static float CalcVolume( const vector<SBSPPoly> &polys )
	{
		if ( polys.empty() )
			return 0;
		vector<SBSPPoly> newPolys;
		unsigned int index;
		for ( index = 0; index < polys.size(); ++index )
		{
			if ( CalculatePolygonSquare( polys[ index ] ) != 0 )
				break;
		}
		if ( index == polys.size() )
			return 0;
		CVec3 ptSrc = polys[index].pts[0].pt, ptDst = polys[index].pts[1].pt;
		float fAddedVolume = 0;
		for ( unsigned int k = index; k < polys.size(); ++k )
		{
			const SBSPPoly &poly = polys[k];
			if ( CalculatePolygonSquare( poly ) == 0 )
				continue;
			bool bHas = false;
			for ( unsigned int i = 0; i < poly.pts.size(); ++i )
			{
				if ( poly.pts[i].pt == ptSrc )
				{
					const CVec3 &pt1 = poly.pts[ poly.Prev(i) ].pt;
					const CVec3 &pt2 = poly.pts[ poly.Succ(i) ].pt;
					fAddedVolume += CalcPyramidVolume( ptDst, pt1, ptSrc, pt2 );
					bHas = true;
					if ( poly.pts.size() > 3 )
					{
						newPolys.push_back( SBSPPoly() );
						SBSPPoly &newPoly = newPolys.back();
						for ( unsigned int nPt = 0; nPt < poly.pts.size(); ++nPt )
							if ( nPt != i )
								newPoly.pts.push_back( poly.pts[nPt] );
					}
					newPolys.push_back( SBSPPoly() );
					SBSPPoly &newPoly = newPolys.back();
					newPoly.pts.push_back( pt1 );
					newPoly.pts.push_back( ptDst );
					newPoly.pts.push_back( pt2 );
					break;
				}
			}
			if ( !bHas )
				newPolys.push_back( poly );
		}
		return fAddedVolume + CalcVolume( newPolys );
	}
	float CalcVolume() const
	{
		return CalcVolume( polygons );
	}

	bool IsEmpty( CBSPTreeConstructor *pCurr ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsLeafNode( const SBSPGeometry &geom )
{
	for ( unsigned int i = 0; i < geom.polygons.size(); ++i )
	{
		if ( !geom.polygons[i].bTerminal )
			return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static vector<SAnglePoint> flatPoints;
static vector<SAngleEdge> flatEdges;
////////////////////////////////////////////////////////////////////////////////////////////////////
int nBevelingPlanes; // debug
class CBSPTreeConstructor : public CObjectBase
{
	OBJECT_BASIC_METHODS(CBSPTreeConstructor);
	SPlane divider;
	int nTreeIndex;
	CObj<CBSPTreeConstructor> pRight;
	CObj<CBSPTreeConstructor> pLeft;
	CPtr<CBSPTreeConstructor> pParent;
	float EdgePrice( const CVec3 &v1, const CVec3 &v2, const SBSPGeometry &mesh, CVec3 *pVec );
	bool SetDividingEdge( const SBSPGeometry &mesh );
	void AddBevelingPlane( CBSPTreeConstructor **ppRoot, const CVec3 &norm, const CVec3 &ptOn );
	void AddBevelingInPoint( CBSPTreeConstructor **ppRoot, const SBSPGeometry &mesh, const CVec3 &pt );
	void AddBeveling( const SBSPGeometry &mesh );
	bool SelectTerminalDivider( const SBSPGeometry &mesh );
public:
	bool IsOnDivider( const CVec3 &pt ) const 
	{
		return ( fabs( pt * divider.n + divider.d ) <= fBSPEpsilon * 1.01f );
	}
	CBSPTreeConstructor() {}
	CBSPTreeConstructor( const SBSPGeometry &mesh, CBSPTreeConstructor *_pParent = 0 );
	CBSPTree *CreateBSPTree();
	CBSPTreeConstructor* GetParent() { return pParent; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline void AddProjectedPoint( const CVec3 &u, const CVec3 &v, const CVec3 &point, 
																		 const CVec3 &trueVector, int index )
{
	SAnglePoint &p = flatPoints[index];
	p.vec = trueVector;
	p.u = point * u;
	p.v = point * v;
	p.bIgnored = fabs( p.u ) < fBSPEpsilon && fabs( p.v ) < fBSPEpsilon;
	if ( !p.bIgnored )
		p.fAngle = atan2( p.v, p.u );
	else
		p.fAngle = 0;
	p.nIntersected = p.nAHalf = p.nBHalf = p.nOnBound = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline void AddProjectedEdge( const SAnglePoint &p1, const SAnglePoint &p2, int index, bool bIgnored )
{
	SAngleEdge &e = flatEdges[index];
	e.bIgnored = bIgnored || ( p1.bIgnored && p2.bIgnored );
	e.fAngle1 = p1.fAngle;
	e.fAngle2 = p2.fAngle;
	if ( !p1.bIgnored && p2.bIgnored )
		e.fAngle2 = p1.fAngle;
	if ( p1.bIgnored && !p2.bIgnored )
		e.fAngle1 = p2.fAngle;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int ProjectOnNormalPlane( const CVec3 &v1, const CVec3 &v2, const SBSPGeometry &mesh )
{
	CVec3 dir = v2 - v1;
	CVec3 u, v( 0, 0, 0 );
	if( fabs( dir.z ) > 0.001f )
		v.x = 1;
	else 
		v.z = 1;
	u = dir ^ v;
	if ( !Normalize( &u ) ) 
	{
		ASSERT(0); 
		return 0;
	}
	v = dir ^ u;
	if ( !Normalize( &v ) ) 
	{
		ASSERT(0); 
		return 0;
	}
	int index = 0;
	for ( unsigned int i = 0; i < mesh.polygons.size(); ++i )
	{
		int nPts = mesh.polygons[i].pts.size();
		bool bNotNeedEdges = mesh.polygons[i].bTerminal;
		for ( int k = 0; k < nPts; ++k )
		{
			const CVec3 &pt = mesh.polygons[i].pts[k].pt;
			AddProjectedPoint( u, v, pt - v1, pt, index );
			if ( k > 0 )
				AddProjectedEdge( flatPoints[ index ], flatPoints[ index - 1 ], index, bNotNeedEdges );
			++index;
		}
		AddProjectedEdge( flatPoints[ index - 1 ], flatPoints[ index - nPts ], index - nPts, bNotNeedEdges );
	}
	return index;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline static float LessZeroAngle( float fAngle )
{
	return ( fAngle > 0 ) ? fAngle - FP_PI : fAngle;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline static bool IsEqAng( float fA1, float fA2 )
{
	return fabs( fA1 - fA2 ) < fAngleEpsilon;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma optimize("", on)
float CBSPTreeConstructor::EdgePrice(
	const CVec3 &v1, const CVec3 &v2, const SBSPGeometry &mesh, CVec3 *pVec )
{
	if ( fabs2( v1 - v2 ) < fBSPEpsilon * fBSPEpsilon )
		return 10000;
	int nPoints = ProjectOnNormalPlane( v1, v2, mesh );
	float fLocalBest = 10000;
	CVec3 &v3 = *pVec;

	for ( SAnglePoint *it = &flatPoints[0]; it != &flatPoints[nPoints]; ++it )
		if ( it->fAngle > 0 )
			it->fAngle -= FP_PI;
	// determine the cost for each of the points
	int nIntersectEvery = 0;
	int nUnignored = 0;
	for ( int i = 0; i < nPoints; ++i )
	{
		SAngleEdge &ae = flatEdges[i];
		if ( ae.bIgnored )
			continue;
		if ( IsEqAng( ae.fAngle1, ae.fAngle2 + FP_PI ) || IsEqAng( ae.fAngle2, ae.fAngle1 + FP_PI ) ) 
		{
			++nIntersectEvery;
			continue;
		}		
		bool bSplitEdge = ( ae.fAngle1 > 0 ) != ( ae.fAngle2 > 0 );
		bool bEqEdge = IsEqAng( ae.fAngle1, ae.fAngle2 );
		bool bInverted;
		float fA1 = LessZeroAngle( ae.fAngle1 );
		float fA2 = LessZeroAngle( ae.fAngle2 );
		if ( fA1 > fA2 )
		{
			std::swap( fA1, fA2 );
			bInverted = ae.fAngle2 > 0;
		}
		else
			bInverted = ae.fAngle1 > 0;
		++nUnignored;

		if ( bSplitEdge ) // complex search, reverse order
		{
			if ( bEqEdge )
			{
				float fLowBound = fA2 + fAngleEpsilon - FP_PI;
				float fHighBound = fA1 - fAngleEpsilon + FP_PI;
				ASSERT( fA2 + fAngleEpsilon > 0 );
				ASSERT( fHighBound <= 0 );
				for ( SAnglePoint *it = &flatPoints[0]; it != &flatPoints[nPoints]; ++it )
				{
					float f = it->fAngle;
					if ( f < fLowBound || f > fHighBound )
						++(it->nOnBound);		
					else if ( bInverted )
						++(it->nBHalf);
					else 
						++(it->nAHalf);
				}
			}
			else
			{
				ASSERT( fA2 > fA1 );
				for ( SAnglePoint *it = &flatPoints[0]; it != &flatPoints[nPoints]; ++it )
				{
					float f = it->fAngle;
					if ( f < fA1 - fAngleEpsilon || f > fA2 + fAngleEpsilon )
						++(it->nIntersected);		
					else if ( bInverted )
						++(it->nBHalf);
					else 
						++(it->nAHalf);
				}
			}
		}
		else // simple search
		{
			if ( bEqEdge )
			{
				ASSERT( fA2 >= fA1 );
				for ( SAnglePoint *it = &flatPoints[0]; it != &flatPoints[nPoints]; ++it )
				{
					float f = it->fAngle;
					if ( f > fA2 - fAngleEpsilon && f < fA1 + fAngleEpsilon )
						++(it->nOnBound);
					else if ( bInverted )
					{
						if ( f <= fA2 - fAngleEpsilon )
							++(it->nAHalf);
						else 
							++(it->nBHalf);
					}
					else // !bInverted
					{
						if ( f <= fA2 - fAngleEpsilon )
							++(it->nBHalf);
						else 
							++(it->nAHalf);
					}
				}
			}
			else
			{
				ASSERT( fA2 > fA1 );
				for ( SAnglePoint *it = &flatPoints[0]; it != &flatPoints[nPoints]; ++it )
				{
					float f = it->fAngle;
					if ( bInverted )
					{
						if ( f < fA1 + fAngleEpsilon )
							++(it->nAHalf);
						else if ( f > fA2 - fAngleEpsilon )
							++(it->nBHalf);
						else 
							++(it->nIntersected);
					}
					else // !bInverted
					{
						if ( f < fA1 + fAngleEpsilon )
							++(it->nBHalf);
						else if ( f > fA2 - fAngleEpsilon )
							++(it->nAHalf);
						else 
							++(it->nIntersected);
					}
				}
			}
		}
	}
	for ( int i = 0; i < nPoints; ++i )
	{
		SAnglePoint &fp = flatPoints[i];
		if ( fp.bIgnored )
			continue;
		ASSERT( fp.nIntersected + fp.nAHalf + fp.nBHalf + fp.nOnBound == nUnignored );
		float fCost = 
			(float)(( fp.nIntersected + nIntersectEvery ) * N_BSP_DIVISION_COST + abs( fp.nAHalf - fp.nBHalf )
			+ ( fp.nAHalf + fp.nBHalf ) / 100.);
		if ( fCost >= fLocalBest )
			continue;
		fLocalBest = fCost;
		v3 = fp.vec;
		if ( fCost == 0 )
			return 0;
	}
	ASSERT( v1 != v3 && v2 != v3 );
	return fLocalBest;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBSPTreeConstructor::SetDividingEdge( const SBSPGeometry &mesh )
{
	CVec3 vv1, vv2, vv3; // vectors of best values 
	float fBestPrice = 10000;
	int nPoints = 0;
	for ( unsigned int i = 0; i < mesh.polygons.size(); ++i )
		nPoints += mesh.polygons[i].pts.size();
	if ( flatPoints.size() < nPoints + 1 )
	{
		flatPoints.resize( nPoints + 1 );
		flatEdges.resize( nPoints + 1 );
	}
	for ( unsigned int nPoly = 0; nPoly < mesh.polygons.size(); ++nPoly )
	{
		const SBSPPoly &poly = mesh.polygons[ nPoly ];
		if ( poly.bTerminal )
			continue;
		for (unsigned int k = 0; k < poly.pts.size(); ++k )
		{
			const CVec3 &v1 = poly.pts[k].pt, &v2 = poly.pts[ poly.Succ(k) ].pt;
			CVec3 v3;
			// seems like crap method of cutting half of edges
			if ( v2.x > v1.x )
				continue;
			float fEdgePrice = EdgePrice( v1, v2, mesh, &v3 ); 
			if ( fEdgePrice < fBestPrice )
			{
				fBestPrice = fEdgePrice;
				vv1 = v1; vv2 = v2; vv3 = v3;
			}
			if ( fEdgePrice == 0 )
			{
				divider.Set( vv1, vv2, vv3 );
				return true;
			}
		}
	}
	if ( fBestPrice >= 10000 )
		return false;
	divider.Set( vv1, vv2, vv3 );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int CountInsidePolys( const SBSPGeometry &mesh ) // debug
{
	int nRet = 0;
	for (unsigned int i = 0; i < mesh.polygons.size(); ++i )
		if ( !mesh.polygons[i].bTerminal )
			++nRet;
	return nRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CountDividerCost( const SBSPGeometry &mesh, const SPlane &divider, int *pNLeft, int *pNRight )
{
	SBSPGeometry left, right;
	mesh.Divide( divider, &left, &right );
	//	ASSERT( left.CheckClosed() );
	//	ASSERT( right.CheckClosed() );
	*pNLeft = CountInsidePolys( left );
	*pNRight = CountInsidePolys( right );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int CountBestDividerInDir( const SBSPGeometry &mesh, float fMin, float fMax, SPlane *pPlane )
{
	int nLeft, nRight;
	do
	{
		float fCurr = (fMin + fMax) / 2;
		pPlane->d = fCurr;
		CountDividerCost( mesh, *pPlane, &nLeft, &nRight );
		if ( nLeft > nRight )
			fMin = fCurr;
		else if ( nLeft < nRight )
			fMax = fCurr;
	} 
	while ( fMax - fMin > fBSPEpsilon && nLeft != nRight );
	return Max( nLeft, nRight );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static SPlane CountBestDivider( const SBSPGeometry &mesh, int* pBestCost )
{
	SPlane best;
	SPlane currX, currY, currZ;
	currX.n = CVec3( -1, 0, 0 );
	currY.n = CVec3( 0, -1, 0 );
	currZ.n = CVec3( 0, 0, -1 );
	float fMinX = 1e10, fMaxX = -1e10;
	float fMinY = 1e10, fMaxY = -1e10;
	float fMinZ = 1e10, fMaxZ = -1e10;
	for (unsigned int i = 0; i < mesh.polygons.size(); ++i )
	{
		const SBSPPoly &poly = mesh.polygons[i];
		for (unsigned int k = 0; k < poly.pts.size(); ++k )
		{
			const CVec3 &pt = poly.pts[k].pt;
			fMinX = Min( fMinX, pt.x );
			fMaxX = Max( fMaxX, pt.x );
			fMinY = Min( fMinY, pt.y );
			fMaxY = Max( fMaxY, pt.y );
			fMinZ = Min( fMinZ, pt.z );
			fMaxZ = Max( fMaxZ, pt.z );
		}
	}
	int nCurr = CountBestDividerInDir( mesh, fMinX, fMaxX, &currX );
	*pBestCost = nCurr;
	best = currX;
	nCurr = CountBestDividerInDir( mesh, fMinY, fMaxY, &currY );
	if ( nCurr < *pBestCost )
	{
		*pBestCost = nCurr;
		best = currY;
	}
	nCurr = CountBestDividerInDir( mesh, fMinZ, fMaxZ, &currZ );
	if ( nCurr < *pBestCost )
	{
		*pBestCost = nCurr;
		best = currZ;
	}
	return best;
}
#pragma optimize("", off)
#pragma optimize("p", on)
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBSPTreeConstructor::AddBevelingPlane( CBSPTreeConstructor **ppRoot, const CVec3 &norm, const CVec3 &ptOn )
{
	const CBSPTreeConstructor *pCurrent = this;
	while ( pCurrent->pLeft )
	{
		if ( pCurrent->divider.n == norm )
			return; 
		pCurrent = pCurrent->pLeft;
	}
	CBSPTreeConstructor *pRoot = *ppRoot;
	ASSERT( fabs2( norm ) > 0.99 && fabs2( norm ) < 1.01 );
	pRoot->divider.n = norm;
	pRoot->divider.d = - ptOn * norm;
	pRoot->pLeft = new CBSPTreeConstructor;
	pRoot->pLeft->divider.d = 1;
	pRoot->pRight = new CBSPTreeConstructor;
	pRoot->pRight->divider.d = -1;
	pRoot->pLeft->pParent = pRoot;
	pRoot->pRight->pParent = pRoot;
	*ppRoot = pRoot->pLeft;
	nBevelingPlanes += 2;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_INV_SQRT_3 = 0.5773502f;
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool HasCommonEdge( const SBSPPoly &p1, const SBSPPoly &p2, CVec3 *pCommon1, CVec3 *pCommon2 )
{
	for (unsigned int i = 0; i < p1.pts.size(); ++i )
	{
		const CVec3 &pt1 = p1.pts[ i ].pt;
		const CVec3 &pt2 = p1.pts[ p1.Succ(i) ].pt;
		for (unsigned int k = 0; k < p2.pts.size(); ++k )
		{
			if ( pt2 == p2.pts[ k ].pt && pt1 == p2.pts[ p2.Succ(k) ].pt )
			{
				*pCommon1 = pt1;
				*pCommon2 = pt2;
				return true;
			}
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBSPTreeConstructor::AddBevelingInPoint( CBSPTreeConstructor **ppRoot, const SBSPGeometry &mesh, const CVec3 &pt )
{
	CVec3 averNorm = VNULL3;
	int nHas = 0;
	bool bWasNole = false;
	for (unsigned int i = 0; i < mesh.polygons.size(); ++i )
	{
		const SBSPPoly &poly = mesh.polygons[i];
		for (unsigned int k = 0; k < poly.pts.size(); ++k )
		{
			if ( pt == poly.pts[k].pt )
			{
				CVec3 v1 = pt - poly.pts[ poly.Succ(k) ].pt;
				CVec3 v2 = pt - poly.pts[ poly.Prev(k) ].pt;
				/*float fV1 = fabs(v1), fV2 = fabs(v2);
				if ( !fV1 || !fV2 )
				{
				bWasNole = true;
				break;
				}*/
				/*float fCos = v1 * v2 / fV1 / fV2;
				ASSERT( -1.01 <= fCos && fCos <= 1.01 );
				if ( fCos < -1 )
				fCos = -1;
				if ( fCos > 1 )
				fCos = 1;
				float fMult = acos( fCos );*/
				Normalize( &v1 );
				Normalize( &v2 );
				averNorm += ( v1 + v2 );// * fMult;
				++nHas;
				break;
			}
		}
	}
	if ( !Normalize( &averNorm ) )
		return;
	bool bAdd = false;
	for (unsigned int i = 0; i < mesh.polygons.size(); ++i )
	{
		const SBSPPoly &poly = mesh.polygons[i];
		if ( poly.norm == VNULL3 )
			continue;
		for (unsigned int k = 0; k < poly.pts.size(); ++k )
		{
			if ( pt == poly.pts[k].pt )
			{
				if ( averNorm * poly.norm < 0.55f )
					bAdd = true;
				break;
			}
		}
		if ( bAdd )
			break;
	}
	if ( bAdd )
		AddBevelingPlane( ppRoot, averNorm, pt );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBSPTreeConstructor::AddBeveling( const SBSPGeometry &mesh )
{
	CBSPTreeConstructor *pRoot = this;
	for (unsigned int i = 1; i < mesh.polygons.size(); ++i )
	{
		const SBSPPoly &p1 = mesh.polygons[i];
		if ( p1.norm == VNULL3 )
			continue;
		ASSERT( fabs2(p1.norm) < 1.01 && fabs2(p1.norm) > 0.99 );
		for ( int k = 0; k < i; ++k )
		{
			const SBSPPoly &p2 = mesh.polygons[k];
			if ( p2.norm == VNULL3 )
				continue;
			if ( p1.norm * p2.norm > 0 )
				continue;
			CVec3 ptCommon1, ptCommon2;
			if ( !HasCommonEdge( p1, p2, &ptCommon1, &ptCommon2 ) )
				continue;
			CVec3 norm = p1.norm + p2.norm;
			if ( fabs2(norm) < fBSPEpsilon * fBSPEpsilon )
			{
				norm = ( ptCommon1 - ptCommon2 ) ^ ( p1.norm - p2.norm );
				// check that the result will stick outward
				float d = - norm * ptCommon1;
				for (unsigned int m = 0; m < p1.pts.size(); ++m )
				{
					if ( norm * p1.pts[m].pt + d > 0 )
					{
						norm = -norm;
						break;
					}
				}
			}
			if ( !Normalize( &norm ) )
				continue;
			AddBevelingPlane( &pRoot, norm, ptCommon1 );
		}
	}
	for (unsigned int i = 0; i < mesh.polygons.size(); ++i )
	{
		const SBSPPoly &poly = mesh.polygons[i];
		for (unsigned int k = 0; k < poly.pts.size(); ++k )
			AddBevelingInPoint( &pRoot, mesh, poly.pts[k].pt );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBSPTreeConstructor::SelectTerminalDivider( const SBSPGeometry &mesh )
{
	int nBest1 = mesh.polygons.size() - 1, nBest2 = 2 * mesh.polygons.size();
	SPlane bestDivider;
	for (unsigned int i = 0; i < mesh.polygons.size(); ++i )
	{
		if ( !mesh.polygons[i].bTerminal )
		{
			if ( !mesh.polygons[i].SetPlane( &divider ) )
				continue;
			SBSPGeometry left, right;
			mesh.Divide( divider, &left, &right );
			int nRight = CountInsidePolys( right );
			int nLeft = CountInsidePolys( left );
			int nMax = Max( nRight, nLeft );
			if ( nMax < nBest1 )
			{
				nBest1 = nMax; 
				nBest2 = nRight + nLeft;
				bestDivider = divider;
			}
			else if ( nMax == nBest1 )
			{
				if ( nRight + nLeft <= nBest2 )
				{
					nBest2 = nRight + nLeft;
					bestDivider = divider;
				}
			}
		}
	}
	if ( nBest2 == 2 * mesh.polygons.size() )
		return false;
	divider = bestDivider;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CBSPTreeConstructor::CBSPTreeConstructor( const SBSPGeometry &mesh, CBSPTreeConstructor *_pParent )
: pParent( _pParent )
{
	ASSERT( mesh.CheckClosed() );
	// depth check
	/*CBSPTreeConstructor *pCurr = pParent;
	int nDepth = 0;
	while ( pCurr )
	{
		pCurr = pCurr->pParent;
		++nDepth;
	}
	if ( nDepth > 50 )
		__debugbreak();*/

	bool bIsLeaf = IsLeafNode( mesh );
	if ( bIsLeaf )
	{
		if ( mesh.IsEmpty( pParent ) )
			divider.d = -1;
		else 
		{
			divider.d = 1;
			AddBeveling( mesh );
		}
		return;
	}
	int nTris = CountInsidePolys( mesh );
	if ( nTris > 25 )
	{
		int nBestPrice;
		divider = CountBestDivider( mesh, &nBestPrice );
		ASSERT( fabs2( divider.n ) > 0.99 && fabs2( divider.n ) < 1.01 );
		if ( nBestPrice >= nTris )
			nTris = 25;
	}
	if ( nTris <= 25 )
	{
		bool bOk;
		if ( nTris <= 5 )
			bOk = SelectTerminalDivider( mesh );
		else
			bOk = SetDividingEdge( mesh );
		if ( !bOk )
		{
			pRight = pLeft = 0;
			divider.d = -1;
			ASSERT(0);
			return;
		}
	}
	ASSERT( fabs2( divider.n ) > 0.99 && fabs2( divider.n ) < 1.01 );
	// adding tris
	SBSPGeometry left, right;
	mesh.Divide( divider, &left, &right );
	int nRight = CountInsidePolys( right ), nLeft = CountInsidePolys( left );
	int nMax = Max( nRight, nLeft );
	if ( nMax >= nTris - 1 )
	{
		SelectTerminalDivider( mesh );
		left.polygons.clear();
		right.polygons.clear();
		mesh.Divide( divider, &left, &right );
		nRight = CountInsidePolys( right );
		nLeft = CountInsidePolys( left );
		nMax = Max( nRight, nLeft );
		ASSERT( nMax <= nTris - 1 );
	}
	//	ASSERT( left.CheckClosed() );
	pLeft = new CBSPTreeConstructor( left, this );
	//	ASSERT( right.CheckClosed() );
	pRight = new CBSPTreeConstructor( right, this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CBSPTree* CBSPTreeConstructor::CreateBSPTree()
{
	CBSPTree *pTree = new CBSPTree;
	CBSPTreeConstructor *pCur = this;
	unsigned int nSpare = 0;
	do // traverse BSPtree constructor
	{
		pCur->nTreeIndex = nSpare;
		if ( nSpare >= pTree->dividers.size() )
		{
			pTree->dividers.resize( nSpare * 2 + 1 );
			pTree->rightIndices.resize( nSpare * 2 + 1 );
			pTree->leftIndices.resize( nSpare * 2 + 1 );
		}
		pTree->dividers[ nSpare ] = pCur->divider;
		pTree->rightIndices[ nSpare ] = 0;
		pTree->leftIndices[ nSpare ] = 0;
		++nSpare;
		if ( pCur->pLeft )
		{
			pCur = pCur->pLeft;
			continue;
		}
		if ( pCur->pRight )
		{
			pCur = pCur->pRight;
			continue;
		}
		while ( pCur->pParent )
		{
			if ( pCur == pCur->pParent->pLeft )
			{
				pTree->leftIndices[ pCur->pParent->nTreeIndex ] = pCur->nTreeIndex;
				pCur = pCur->pParent->pRight;
				break;
			}
			ASSERT( pCur == pCur->pParent->pRight );
			pTree->rightIndices[ pCur->pParent->nTreeIndex ] = pCur->nTreeIndex;
			pCur = pCur->pParent;
		}
	}
	while ( pCur != this );
	pTree->dividers.resize( nSpare );
	pTree->rightIndices.resize( nSpare );
	pTree->leftIndices.resize( nSpare );
	return pTree;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Diagnostics toggle for the per-tree construction trace inside CreateBSPTree (default off). The
// release shipped that OutputDebugString unconditionally, but it fires once per collision mesh --
// thousands of times on a complex level -- and with a debugger attached each call is a slow
// synchronous round-trip that dominates load time (besides spamming the output). Flip this to true
// to restore the original trace. Diagnostics only; no game-behaviour change.
static bool bLogBSPConstruction = false;
CBSPTree* CreateBSPTree( const vector<CVec3> &points, const vector<STriangle> &mesh )
{
	SBSPGeometry bspMesh;
	for (unsigned int i = 0; i < mesh.size(); ++i )
	{
		const STriangle &t = mesh[i];
		SBSPPoly poly;
		poly.pts.push_back( points[t.i1] );
		poly.pts.push_back( points[t.i2] );
		poly.pts.push_back( points[t.i3] );
		poly.bTerminal = false;
		poly.norm = (points[t.i1] - points[t.i2]) ^ (points[t.i1] - points[t.i3]);
		if ( !Normalize( &poly.norm ) )
			poly.norm = VNULL3;
		bspMesh.polygons.push_back( poly );
	}
	if ( !bspMesh.CheckClosed() )
		return 0;
	nBevelingPlanes = 0;
	CObj<CBSPTreeConstructor> pConstructor = new CBSPTreeConstructor( bspMesh );
	CBSPTree *pTree = pConstructor->CreateBSPTree();
	if ( bLogBSPConstruction )
	{
		char buf[128];
		sprintf( buf, "Created BSP tree depth %d, nodes %d for nTris = %d, beveling nodes = %d \n",
			pTree->CalcDepth(), pTree->CalcNodes(), mesh.size(), nBevelingPlanes );
		OutputDebugString( buf );
	}
	return pTree;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CBSPTree *BSPChecker()
{
	const int N_SIDES = 8;
	SBSPGeometry bspMesh;	
	SBSPPoly polyUp, polyDown, polyUpGood;
	for ( int i = 0; i < N_SIDES; ++i )
	{
		float fAng = i * FP_2PI / N_SIDES;
		float fNext = ( i + 1 ) * FP_2PI / N_SIDES;
		if ( i == N_SIDES - 1 )
			fNext = 0;
		CVec3 up1( 3 * sin(fAng), 3 * cos(fAng), 2 ), up2( 3 * sin(fNext), 3 * cos(fNext), 2 );
		CVec3 down1( 3 * sin(fAng), 3 * cos(fAng), 0 ), down2( 3 * sin(fNext), 3 * cos(fNext), 0 );
		/*if ( i + 1 > N_SIDES / 2 )
		{
		up2 = CVec3( - sin(fNext), 3 * cos(fNext), 2 );
		down2 = CVec3( - sin(fNext), 3 * cos(fNext), 0 );
		}
		if ( i > N_SIDES / 2 )
		{
		up1 = CVec3( - sin(fAng), 3 * cos(fAng), 2 );
		down1 = CVec3( - sin(fAng), 3 * cos(fAng), 0 );
		}*/
		polyUp.pts.push_back( up1 );
		polyDown.pts.push_back( down1 );
		SBSPPoly tri1, tri2;
		tri1.pts.push_back( up1 );
		tri1.pts.push_back( up2 );
		tri1.pts.push_back( down1 );
		tri2.pts.push_back( down1 );
		tri2.pts.push_back( up2 );
		tri2.pts.push_back( down2 );
		bspMesh.polygons.push_back( tri1 );
		bspMesh.polygons.push_back( tri2 );
	}
	for ( int i = 0; i < N_SIDES; ++i )
	{
		polyUpGood.pts.push_back( polyUp.pts[ N_SIDES - i - 1 ] );
	}
	bspMesh.polygons.push_back( polyUpGood );
	bspMesh.polygons.push_back( polyDown );
	for (unsigned int i = 0; i < bspMesh.polygons.size(); ++i )
	{
		SBSPPoly &poly = bspMesh.polygons[i];
		poly.bTerminal = false;
		poly.norm = (poly.pts[1].pt - poly.pts[0].pt ) ^ ( poly.pts[2].pt - poly.pts[0].pt );
		Normalize( &poly.norm );
	}

	CPtr<CBSPTreeConstructor> pConstructor = new CBSPTreeConstructor( bspMesh );
	CBSPTree *pTree = pConstructor->CreateBSPTree();
	char buf[128];
	sprintf( buf, "Created BSP tree depth %d, nodes %d for nPolygons = %d \n",
	pTree->CalcDepth(), pTree->CalcNodes(), bspMesh.polygons.size() );
	OutputDebugString( buf );
	return pTree;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SBSPGeometry::IsEmpty( CBSPTreeConstructor *_pCur ) const
{
	CBSPTreeConstructor *pCur = _pCur;
	while ( pCur )
	{
		bool bOn = true;
		for (unsigned int i = 0; i < polygons.size(); ++i )
		{
			const SBSPPoly &poly = polygons[i];
			for (unsigned int k = 0; k < poly.pts.size(); ++k )
			{
				const CVec3 &pt = poly.pts[k].pt;
				if ( !pCur->IsOnDivider( pt ) )
				{
					bOn	= false;
					break;
				}
			}
			if ( !bOn )
				break;
		}
		if ( bOn )
			return true;
		pCur = pCur->GetParent();
	}
	return false;
	//return CalcVolume() < fBSPEpsilon * fBSPEpsilon * fBSPEpsilon;
	//return polygons.empty();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma optimize("", on)
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBSPTree::CheckConsistency()
{
	for (unsigned int i = 0; i < dividers.size(); ++i )
	{
		if ( leftIndices[ i ] == 0 )
		{
			ASSERT( rightIndices[ i ] == 0 );
		}
		else
		{
			ASSERT( rightIndices[ i ] != 0 );
			ASSERT( rightIndices[ i ] > i );
			ASSERT( leftIndices[ i ] > i );
			ASSERT( leftIndices[ i ] < dividers.size() );
			ASSERT( rightIndices[ i ] < dividers.size() );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBSPTree::DoesIntersect( const CVec3 &v, float fRadius, int nIndex )
{
//	if ( nIndex == 0 )
//		CheckConsistency();
	if ( IsLeafNode( nIndex ) )
		return IsFilledNode( nIndex );
	ASSERT( leftIndices[ nIndex ] > nIndex );
	ASSERT( rightIndices[ nIndex ] > nIndex );
	bool bRet = false;
	float fWhere = dividers[ nIndex ].n * v + dividers[ nIndex ].d;
	if ( fWhere < fRadius )
		bRet = DoesIntersect( v, fRadius, leftIndices[ nIndex ] );
	if ( bRet )
		return bRet;
	if ( fWhere > - fRadius )
		bRet = DoesIntersect( v, fRadius, rightIndices[ nIndex ] );
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBSPTree::CollideCheckZeroRad( const CVec3 &v0, const CVec3 &v1, CVec3 *pImpact, SPlane *pPlane, int nIndex )
{
	if ( IsLeafNode( nIndex ) )
	{
		bool bFilled = IsFilledNode( nIndex );
		if ( bFilled )
			*pImpact = v0;
		return bFilled;
	}
	const SPlane &divider = dividers[ nIndex ];
	float fStart = v0 * divider.n + divider.d;
	float fFinish = v1 * divider.n + divider.d;
	if ( fStart > 0 && fFinish > 0 )
	{
		return CollideCheckZeroRad( v0, v1, pImpact, pPlane, rightIndices[ nIndex ] );
	}
	else if ( fStart < 0 && fFinish < 0 )
		return CollideCheckZeroRad( v0, v1, pImpact, pPlane, leftIndices[ nIndex ] );
	else
	{
		CVec3 onPlane = ( v0 * fFinish - v1 * fStart ) / ( fFinish - fStart );
		if ( CollideCheckZeroRad( v0, onPlane, pImpact, pPlane, rightIndices[ nIndex ] ) )
			return true;
		return CollideCheckZeroRad( onPlane, v1, pImpact, pPlane, leftIndices[ nIndex ] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBSPTree::CollideCheckNoImpact( const CVec3 &v0, const CVec3 &v1, float fRadius, int nIndex )
{
	if ( IsLeafNode( nIndex ) )
		return IsFilledNode( nIndex );
	const SPlane &divider = dividers[ nIndex ];
	float fStart = divider.n * v0 + divider.d;
	float fFinish = divider.n * v1 + divider.d;
	if ( fStart < - fRadius ) // start is left
	{
		if ( fFinish < - fRadius )
			return CollideCheckNoImpact( v0, v1, fRadius, leftIndices[ nIndex ] );
		else if ( fFinish > fRadius ) // left -> right
		{
			CVec3 vR = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart - fRadius );
			if ( CollideCheckNoImpact( v0, vR, fRadius, leftIndices[ nIndex ] ) )
				return true;
			CVec3 vL = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart + fRadius );
			return CollideCheckNoImpact( vL, v1, fRadius, rightIndices[ nIndex ] );
		}
		else // left -> center
		{
			if ( CollideCheckNoImpact( v0, v1, fRadius, leftIndices[ nIndex ] ) )
				return true;
			else
			{
				CVec3 vL = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart + fRadius );
				return CollideCheckNoImpact( vL, v1, fRadius, rightIndices[ nIndex ] );
			}
		}
	}
	else if ( fStart > fRadius ) // start is right
	{
		if ( fFinish > fRadius )
			return CollideCheckNoImpact( v0, v1, fRadius, rightIndices[ nIndex ] );
		else if ( fFinish < - fRadius ) // right -> left
		{
			CVec3 vL = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart + fRadius );
			if ( CollideCheckNoImpact( v0, vL, fRadius, rightIndices[ nIndex ] ) )
				return true;
			else
			{
				CVec3 vR = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart - fRadius );
				return CollideCheckNoImpact( vR, v1, fRadius, leftIndices[ nIndex ] );
			}
		}
		else // right -> center
		{
			if ( CollideCheckNoImpact( v0, v1, fRadius, rightIndices[ nIndex ] ) )
				return true;
			else
			{
				CVec3 vR = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart - fRadius );
				return CollideCheckNoImpact( vR, v1, fRadius, leftIndices[ nIndex ] );
			}
		}
	}
	else // start is center
	{
		if ( fFinish < - fRadius ) // center -> left
		{
			CVec3 vL = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart + fRadius );
			return CollideCheckNoImpact( v0, vL, fRadius, rightIndices[ nIndex ] ) 
				|| CollideCheckNoImpact( v0, v1, fRadius, leftIndices[ nIndex ] );		
		}
		else if ( fFinish > fRadius ) // center -> right
		{
			CVec3 vR = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart - fRadius );
			return CollideCheckNoImpact( v0, v1, fRadius, rightIndices[ nIndex ] ) 
				|| CollideCheckNoImpact( v0, vR, fRadius, leftIndices[ nIndex ] );		
		}
		else // center -> center
		{
			return CollideCheckNoImpact( v0, v1, fRadius, rightIndices[ nIndex ] ) 
				|| CollideCheckNoImpact( v0, v1, fRadius, leftIndices[ nIndex ] );		
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// debug: 
bool bWasPlaneSet;
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBSPTree::CollideCheckInternal( const CVec3 &v0, const CVec3 &v1, float fRadius, CVec3 *pImpact, SPlane *pPlane, 
	int nIndex, const SPlane *pSetPlane, bool bInvert )
{
	if ( IsLeafNode( nIndex ) )
	{
		bool bFilled = IsFilledNode( nIndex );
		if ( bFilled )
		{
			*pImpact = v0;			
			if ( pSetPlane )
			{
				bWasPlaneSet = true;
				*pPlane = *pSetPlane;
				if ( bInvert )
				{
					pPlane->d = - pPlane->d;
					pPlane->n = - pPlane->n;
				}
			}
			else
			{
				// crap, slow:
				for ( int i = 0; i < nIndex; ++i )
				{
					if ( rightIndices[i] == nIndex )
					{
						*pPlane = dividers[i];
						break;
					}
					if ( leftIndices[i] == nIndex )
					{
						pPlane->d = - dividers[i].d;
						pPlane->n = - dividers[i].n;
						break;
					}
				}
			}
		}	
		return bFilled;
	}
	const SPlane &divider = dividers[ nIndex ];
	float fStart = divider.n * v0 + divider.d;
	float fFinish = divider.n * v1 + divider.d;
	if ( fStart < - fRadius ) // start is left
	{
		if ( fFinish < - fRadius )
			return CollideCheckInternal( v0, v1, fRadius, pImpact, pPlane, leftIndices[ nIndex ], pSetPlane, bInvert );
		else if ( fFinish > fRadius ) // left -> right
		{
			CVec3 vR = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart - fRadius );
			if ( CollideCheckInternal( v0, vR, fRadius, pImpact, pPlane, leftIndices[ nIndex ], pSetPlane, bInvert ) )
			{
				float fImpact = *pImpact * divider.n + divider.d;
				if ( fImpact > - fRadius )
				{
					CVec3 vL = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart + fRadius );
					CollideCheckInternal( vL, CVec3(*pImpact), fRadius, pImpact, pPlane, rightIndices[ nIndex ], &divider, false );
				}
				return true;
			}
			else
			{
				CVec3 vL = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart + fRadius );
				return CollideCheckInternal( vL, v1, fRadius, pImpact, pPlane, rightIndices[ nIndex ], &divider, false );
			}
		}
		else // left -> center
		{
			if ( CollideCheckInternal( v0, v1, fRadius, pImpact, pPlane, leftIndices[ nIndex ], pSetPlane, bInvert ) )
			{
				float fImpact = *pImpact * divider.n + divider.d;
				if ( fImpact > - fRadius )
				{
					CVec3 vL = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart + fRadius );
					CollideCheckInternal( vL, CVec3(*pImpact), fRadius, pImpact, pPlane, rightIndices[ nIndex ], &divider, false );
				}
				return true;
			}
			else
			{
				CVec3 vL = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart + fRadius );
				return CollideCheckInternal( vL, v1, fRadius, pImpact, pPlane, rightIndices[ nIndex ], &divider, false );
			}
		}
	}
	else if ( fStart > fRadius ) // start is right
	{
		if ( fFinish > fRadius )
			return CollideCheckInternal( v0, v1, fRadius, pImpact, pPlane, rightIndices[ nIndex ], pSetPlane, bInvert );
		else if ( fFinish < - fRadius ) // right -> left
		{
			CVec3 vL = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart + fRadius );
			if ( CollideCheckInternal( v0, vL, fRadius, pImpact, pPlane, rightIndices[ nIndex ], pSetPlane, bInvert ) )
			{
				float fImpact = *pImpact * divider.n + divider.d;
				if ( fImpact < fRadius )
				{
					CVec3 vR = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart - fRadius );
					CollideCheckInternal( vR, CVec3(*pImpact), fRadius, pImpact, pPlane, leftIndices[ nIndex ], &divider, true );
				}
				return true;
			}
			else
			{
				CVec3 vR = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart - fRadius );
				return CollideCheckInternal( vR, v1, fRadius, pImpact, pPlane, leftIndices[ nIndex ], &divider, true );
			}
		}
		else // right -> center
		{
			if ( CollideCheckInternal( v0, v1, fRadius, pImpact, pPlane, rightIndices[ nIndex ], pSetPlane, bInvert ) )
			{
				float fImpact = *pImpact * divider.n + divider.d;
				if ( fImpact < fRadius )
				{
					CVec3 vR = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart - fRadius );
					CollideCheckInternal( vR, CVec3(*pImpact), fRadius, pImpact, pPlane, leftIndices[ nIndex ], &divider, true );
				}
				return true;
			}
			else
			{
				CVec3 vR = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart - fRadius );
				return CollideCheckInternal( vR, v1, fRadius, pImpact, pPlane, leftIndices[ nIndex ], &divider, true );
			}
		}
	}
	else // start is center
	{
		if ( fFinish < - fRadius ) // center -> left
		{
			CVec3 vL = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart + fRadius );
			if ( CollideCheckInternal( v0, vL, fRadius, pImpact, pPlane, rightIndices[ nIndex ], pSetPlane, bInvert ) )
			{
				CollideCheckInternal( v0, CVec3(*pImpact), fRadius, pImpact, pPlane, leftIndices[ nIndex ], pSetPlane, bInvert );		
				return true;
			}
			return CollideCheckInternal( v0, v1, fRadius, pImpact, pPlane, leftIndices[ nIndex ], pSetPlane, bInvert );		
		}
		else if ( fFinish > fRadius ) // center -> right
		{
			CVec3 vR = v0 + ( v1 - v0 ) / ( fStart - fFinish ) * ( fStart - fRadius );
			if ( CollideCheckInternal( v0, vR, fRadius, pImpact, pPlane, leftIndices[ nIndex ], pSetPlane, bInvert ) )
			{
				CollideCheckInternal( v0, CVec3(*pImpact), fRadius, pImpact, pPlane, rightIndices[ nIndex ], pSetPlane, bInvert );		
				return true;
			}
			return CollideCheckInternal( v0, v1, fRadius, pImpact, pPlane, rightIndices[ nIndex ], pSetPlane, bInvert );		
		}
		else // center -> center
		{
			if ( CollideCheckInternal( v0, v1, fRadius, pImpact, pPlane, leftIndices[ nIndex ], pSetPlane, bInvert ) )
			{
				//CollideCheckInternal( CVec3(*pImpact), v1, fRadius, pImpact, pPlane, rightIndices[ nIndex ], &divider, false );
				CollideCheckInternal( v0, CVec3(*pImpact), fRadius, pImpact, pPlane, rightIndices[ nIndex ], pSetPlane, bInvert );
				return true;
			}
			return CollideCheckInternal( v0, v1, fRadius, pImpact, pPlane, rightIndices[ nIndex ], pSetPlane, bInvert );		
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBSPTree::AddBound( const CVec3 &norm, float d, int nIndex, int *pSpareIndex )
{
	dividers[ nIndex ].Set( norm, d );
	int &nSpare = *pSpareIndex;
	leftIndices[ nIndex ] = nSpare;
	leftIndices[ nSpare ] = rightIndices[ nSpare ] = 0;
	dividers[ nSpare ].d = 1; // mark as filled
	++nSpare;
	rightIndices[ nIndex ] = nSpare;
	leftIndices[ nSpare ] = rightIndices[ nSpare ] = 0;
	dividers[ nSpare ].d = -1;
	++nSpare;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBSPTree::CreateBSPTerrainPart( float fMinX, float fMaxX, float fMinY, float fMaxY, 
	const vector<CVec3> &points, const vector<STriangle> &mesh, int nIndex, int *pSpareIndex )
{
	if ( fMaxX - fMinX < 0.01f || fMaxY - fMinY < 0.01f )
		return;
	if ( fMaxX - fMinX > FP_GRID_STEP + 0.01f )
	{
		float fCenter = ( fMaxX + fMinX ) / 2;
		float fRealCenter = fMaxX;
		float fBest = fRealCenter - fCenter;
		for (unsigned int i = 0; i < points.size(); ++i )
		{
			float fDist = fabs( points[i].x - fCenter );
			if ( fBest > fDist )
			{
				fBest = fDist;
				fRealCenter = points[i].x;
			}
		}
		if ( fMaxX - fRealCenter < 0.01f || fRealCenter - fMinX < 0.01f )
			return;
		dividers[ nIndex ].Set( CVec3( 1, 0, 0 ), -fRealCenter );
		int nSpare = *pSpareIndex;
		leftIndices[ nIndex ] = nSpare;
		++( *pSpareIndex );
		CreateBSPTerrainPart( fMinX, fRealCenter, fMinY, fMaxY, points, mesh, nSpare, pSpareIndex );
		nSpare = *pSpareIndex;
		rightIndices[ nIndex ] = nSpare;
		++( *pSpareIndex );
		CreateBSPTerrainPart( fRealCenter, fMaxX, fMinY, fMaxY, points, mesh, nSpare, pSpareIndex );
		return;
	}
	if ( fMaxY - fMinY > FP_GRID_STEP + 0.01f )
	{
		float fCenter = ( fMaxY + fMinY ) / 2;
		float fRealCenter = fMaxY;
		float fBest = fRealCenter - fCenter;
		for (unsigned int i = 0; i < points.size(); ++i )
		{
			float fDist = fabs( points[i].y - fCenter );
			if ( fBest > fDist )
			{
				fBest = fDist;
				fRealCenter = points[i].y;
			}
		}
		if ( fMaxY - fRealCenter < 0.01f || fRealCenter - fMinY < 0.01f )
			return;
		dividers[ nIndex ].Set( CVec3( 0, 1, 0 ), -fRealCenter );
		int nSpare = *pSpareIndex;
		leftIndices[ nIndex ] = nSpare;
		++( *pSpareIndex );
		CreateBSPTerrainPart( fMinX, fMaxX, fMinY, fRealCenter, points, mesh, nSpare, pSpareIndex );
		nSpare = *pSpareIndex;
		rightIndices[ nIndex ] = nSpare;
		++( *pSpareIndex );
		CreateBSPTerrainPart( fMinX, fMaxX, fRealCenter, fMaxY, points, mesh, nSpare, pSpareIndex );
		return;
	}
	float fMaxZ = -1e10;
	int nNow = nIndex;
	for (unsigned int i = 0; i < mesh.size(); ++i )
	{
		const CVec3 &v1 = points[ mesh[i].i1 ];
		if ( v1.x < fMinX || v1.x > fMaxX )
			continue;
		if ( v1.y < fMinY || v1.y > fMaxY )
			continue;
		const CVec3 &v2 = points[ mesh[i].i2 ];
		if ( v2.x < fMinX || v2.x > fMaxX )
			continue;
		if ( v2.y < fMinY || v2.y > fMaxY )
			continue;
		const CVec3 &v3 = points[ mesh[i].i3 ];
		if ( v3.x < fMinX || v3.x > fMaxX )
			continue;
		if ( v3.y < fMinY || v3.y > fMaxY )
			continue;
		SPlane divider; divider.Set( v1, v2, v3 );
		if ( v1.z > fMaxZ )
			fMaxZ = v1.z;
		if ( v2.z > fMaxZ )
			fMaxZ = v2.z;
		if ( v3.z > fMaxZ )
			fMaxZ = v3.z;
		AddBound( divider.n, divider.d, nNow, pSpareIndex );
		nNow = *pSpareIndex - 2;
	}
	AddBound( CVec3( 0, 0, 1 ), -fMaxZ, nNow, pSpareIndex );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CBSPTree* CreateTerrainBSPTree( const vector<CVec3> &points, const vector<STriangle> &mesh )
{
	float fMaxZ = points[0].z, fMinX = points[0].x, fMaxX = points[0].x, 
		fMinY = points[0].y, fMaxY = points[0].y;
	for (unsigned int i = 1; i < points.size(); ++i )
	{
		if ( points[i].z > fMaxZ )
			fMaxZ = points[i].z;
		if ( fMinX > points[i].x )
			fMinX = points[i].x;
		if ( fMinY > points[i].y )
			fMinY = points[i].y;
		if ( fMaxX < points[i].x )
			fMaxX = points[i].x;
		if ( fMaxY < points[i].y )
			fMaxY = points[i].y;
	}
	CBSPTree *pTree = new CBSPTree;
	int nNow = 0;
	int nSpareIndex = 1;
	pTree->dividers.resize( 1000 );
	pTree->rightIndices.resize( 1000 );
	pTree->leftIndices.resize( 1000 );
	pTree->AddBound( CVec3( -1, 0, 0 ), fMinX, nNow, &nSpareIndex );
	nNow = nSpareIndex - 2;
	pTree->AddBound( CVec3( 0, -1, 0 ), fMinY, nNow, &nSpareIndex );
	nNow = nSpareIndex - 2;
	pTree->AddBound( CVec3( 1, 0, 0 ), -fMaxX, nNow, &nSpareIndex );
	nNow = nSpareIndex - 2;
	pTree->AddBound( CVec3( 0, 1, 0 ), -fMaxY, nNow, &nSpareIndex );
	nNow = nSpareIndex - 2;
	pTree->dividers[ nNow ].Set( CVec3( 0, 0, 1 ), -fMaxZ );
	++nSpareIndex;
	pTree->rightIndices[ nNow ] = nSpareIndex;
	pTree->dividers[ nSpareIndex ].d = -1;
	++nSpareIndex;
	pTree->leftIndices[ nNow ] = nSpareIndex;
	nNow = nSpareIndex;
	++nSpareIndex;
	pTree->CreateBSPTerrainPart( fMinX, fMaxX, fMinY, fMaxY, points, mesh, nNow, &nSpareIndex );
	pTree->dividers.resize( nSpareIndex );
	pTree->rightIndices.resize( nSpareIndex );
	pTree->leftIndices.resize( nSpareIndex );
//	pTree->CheckConsistency();
#ifdef _DEBUG
	/*char buf[128];
	sprintf( buf, "Created terrain BSP tree depth %d, nodes %d for nTris = %d \n",
		pTree->CalcDepth(), pTree->CalcNodes(), mesh.size() );
	OutputDebugString( buf );
	float deltaX = ( fMaxX - fMinX ) / 10;
	float deltaY = ( fMaxY - fMinY ) / 10;
	for ( float fX = fMinX + deltaX; fX < fMaxX; fX += deltaX )
	{
		for ( float fY = fMinY + deltaY; fY < fMaxY; fY += deltaY )
		{
			bool bIntersect = pTree->CollideCheckNoImpact( CVec3( fX, fY, 10 ), CVec3( fX, fY, -1 ), 0.1f );
//			ASSERT( bIntersect );
		}
	}*/
#endif		
	return pTree;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NAI;
REGISTER_SAVELOAD_CLASS( 0x71582160, CBSPTree );