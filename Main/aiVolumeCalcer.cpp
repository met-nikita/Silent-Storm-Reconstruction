#include "StdAfx.h"

#include "aiMap.h"
#include "gGeometry.h"
#include "aiObject.h"
#include "aiObjectLoader.h"

#include "aiVolumeCalcer.h"

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CConnectedPoints
////////////////////////////////////////////////////////////////////////////////////////////////////
/*class CConnectedPoints:public CObjectBase
{
	OBJECT_BASIC_METHODS(CConnectedPoints);
public:
	ZDATA
	vector<WORD> Points; // точки
	unordered_map< WORD, CVec3 > PointsCoords; // points coordinates
	unordered_map< WORD, vector<WORD> > Connectivities; // links between points
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&Points); f.Add(3,&PointsCoords); f.Add(4,&Connectivities); return 0; }
	//
	CConnectedPoints() {}
	//
	void DebugOutput( const char *szTitle );
	void AddPoint( WORD n );
	void RemovePoint( WORD n ); // deletes the point and the edges containing it
	void RemoveIncorrectPoints( WORD k );
	void AddPointCoords( WORD n, CVec3 pt );
	void AddConnectivity( WORD n1, WORD n2 );
	void RemoveConnectivity( WORD n1, WORD n2 );
	bool IsNeighbourPoints( WORD n1, WORD n2 );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CConnectedPoints
////////////////////////////////////////////////////////////////////////////////////////////////////
void CConnectedPoints::DebugOutput( const char *szTitle )
{
	char szStr[128];
	sprintf( szStr, "%s :\n", szTitle );
	OutputDebugString( szStr );
	for ( int n = 0; n < Points.size(); ++n )
	{
		sprintf( szStr, " Point <%d> ( %+4.2f, %+4.2f, %+4.2f ) : ", Points[n], 
			PointsCoords[ Points[n] ].x, PointsCoords[ Points[n] ].y, PointsCoords[ Points[n] ].z );
		OutputDebugString( szStr );

		for ( vector<WORD>::iterator i = Connectivities[ Points[n] ].begin();  i != Connectivities[ Points[n] ].end(); ++i )
		{
			sprintf( szStr, " <%d> ", *i );
			OutputDebugString( szStr );
		}

		OutputDebugString( "\n" );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CConnectedPoints::AddPointCoords( WORD n, CVec3 pt )
{
	PointsCoords[n] = pt;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CConnectedPoints::AddPoint( WORD n )
{
	if ( find( Points.begin(), Points.end(), n ) == Points.end() )
		Points.push_back( n );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CConnectedPoints::RemovePoint( WORD n )
{
	Points.erase( find( Points.begin(), Points.end(), n ) );
	for ( vector<WORD>::iterator i = Connectivities[n].begin(); i != Connectivities[n].end(); ++i )
		Connectivities[*i].erase( find( Connectivities[*i].begin(), Connectivities[*i].end(), n ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CConnectedPoints::AddConnectivity( WORD n1, WORD n2 )
{
	if ( find( Connectivities[n1].begin(), Connectivities[n1].end(), n2 ) == Connectivities[n1].end() )
		Connectivities[n1].push_back( n2 );
	if ( find( Connectivities[n2].begin(), Connectivities[n2].end(), n1 ) == Connectivities[n2].end() )
		Connectivities[n2].push_back( n1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CConnectedPoints::RemoveConnectivity( WORD n1, WORD n2 )
{
	Connectivities[n1].erase( find( Connectivities[n1].begin(), 
		Connectivities[n1].end(), n2 ) );
	Connectivities[n2].erase( find( Connectivities[n2].begin(), 
		Connectivities[n2].end(), n1 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CConnectedPoints::IsNeighbourPoints( WORD n1, WORD n2 )
{
	return find( Connectivities[n1].begin(), Connectivities[n1].end(), n2 ) != Connectivities[n1].end();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CConnectedPoints::RemoveIncorrectPoints( WORD k )
{
	for ( vector<WORD>::iterator i = Points.begin(); i != Points.end(); )
	{
		if ( Connectivities[*i].size() < k )
		{
			RemovePoint( *i );
			i = Points.begin();
		}
		else 
			++i;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTriangulater
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPolygonPath
{
	vector<WORD> Front;
	vector<WORD> AddToFront;
	//
	SPolygonPath() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTriangulater: public CConnectedPoints
{
	OBJECT_BASIC_METHODS(CTriangulater);
	ZDATA
	ZPARENT(CConnectedPoints)
	CVec3 vNormal;
	WORD nSpecialPoint; // точка в которую "переводим" вершину пирамиды
	vector<WORD> VisitedPoints;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CConnectedPoints*)this); f.Add(3,&vNormal); f.Add(4,&nSpecialPoint); f.Add(5,&VisitedPoints); return 0; }
	//
	void EraseInnerEdges( CConnectedPoints *pOwner );
	void BifurcatePath( vector<SPolygonPath> *pPaths, int n1, int n2 );
	int GetNextPoint( int n );
	int GetTriangleSign( WORD _n1, WORD _n2, WORD _n3 );
	bool IsEdgeConsistsInOnePolygon( WORD _n1, WORD _n2 );
public:
	//
	CTriangulater(): vNormal( CVec3( 0, 0, 0 ) ) {}
	//
	void SortPoints( CConnectedPoints *pOwner );
	void SetSpecialPoint( WORD _nSpecialPoint ) { nSpecialPoint = _nSpecialPoint; }
	void CalculateNormal( CVec3 ptTop );
	void GetTriangulation( list<STriangle> *pRes, list<int> *pSign );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTriangulater::CalculateNormal( CVec3 ptTop )
{
	vNormal = CVec3( 0, 0, 0 );
	//
	for ( vector<WORD>::iterator i = Points.begin(); i != Points.end(); ++i )
		vNormal += PointsCoords[ *i ];
	//
	vNormal /= Points.size();
	vNormal = ptTop - vNormal;
	Normalize( &vNormal );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CTriangulater::GetTriangleSign( WORD _n1, WORD _n2, WORD _n3 )
{
	// распологаем точки в пор€дке их обхода в многоугольнике
	int n[3];
	int k = 0;
	n[0] = -1; n[1] = -1; n[2] = -1;
	for ( vector<WORD>::iterator i = Points.begin(); i != Points.end(); ++i )
	{
		if ( *i == _n1 )
			n[k] = _n1;
		if ( *i == _n2 )
			n[k] = _n2;
		if ( *i == _n3 )
			n[k] = _n3;
		if ( n[k] != -1 )
			++k;
	}
	//
	CVec3 vSign = ( PointsCoords[n[1]] - PointsCoords[n[0]] ) ^ ( PointsCoords[n[2]] - PointsCoords[n[1]] );
	float fRes = vNormal * vSign;
	if ( fRes > 0 )
		return 1;
	else
		return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CTriangulater::GetNextPoint( int n )
{
	vector<WORD>::iterator i = find( Points.begin(), Points.end(), n );
	++i;
	if ( i == Points.end() )
		i = Points.begin();
	return *i;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTriangulater::BifurcatePath( vector<SPolygonPath> *pPaths, int n1, int n2 )
{
	int k = 0;
	pPaths->resize( Connectivities[n2].size() - 1 );
	for ( vector<WORD>::iterator c = Connectivities[n2].begin(); c != Connectivities[n2].end(); ++c )
	{
		if ( *c != n1 )
		{
			(*pPaths)[k].Front.push_back( *c );
			VisitedPoints.push_back( *c );
			VisitedPoints.push_back( n2 );
			++k;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTriangulater::IsEdgeConsistsInOnePolygon( WORD _n1, WORD _n2 )
{
	int nRes = 0;
	bool bEnd = false;
	bool bFirst = true;
	vector<SPolygonPath> Paths;
	//
	VisitedPoints.clear();
	BifurcatePath( &Paths, _n1, _n2 );
	//
	while ( !bEnd )
	{
		bEnd = true;
		for ( vector<SPolygonPath>::iterator i = Paths.begin(); i != Paths.end(); ++i )
		{
			if ( !(*i).Front.empty() )
			{
				for ( vector<WORD>::iterator f = (*i).Front.begin(); f != (*i).Front.end(); ++f )
				{
					for ( vector<WORD>::iterator c = Connectivities[*f].begin(); c != Connectivities[*f].end(); ++c  )
					{
						if ( *c == _n1 )
						{
							++nRes;
							(*i).Front.clear();
							(*i).AddToFront.clear();
							break;
						}
						else if ( find( VisitedPoints.begin(), VisitedPoints.end(), *c ) == VisitedPoints.end() )
							(*i).AddToFront.push_back( *c );
					}
					//
					if ( (*i).Front.empty() )
						break;
					//
					VisitedPoints.push_back( *f );
				}
				//
				(*i).Front.clear();
				for ( vector<WORD>::iterator f = (*i).AddToFront.begin(); f != (*i).AddToFront.end(); ++f )
					(*i).Front.push_back( *f );
				(*i).AddToFront.clear();
			}
			//
			bEnd = bEnd & (*i).Front.empty();
		}
	}
	//
	return nRes == 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTriangulater::EraseInnerEdges( CConnectedPoints *pOwner )
{
	vector<WORD> ToRemove;
	for ( vector<WORD>::iterator p = Points.begin(); p != Points.end(); ++p )
	{
		for ( vector<WORD>::iterator c = Connectivities[*p].begin(); c != Connectivities[*p].end(); ++c )
		{
			if ( !IsEdgeConsistsInOnePolygon( *p, *c ) )
				ToRemove.push_back( *c );
		}
		//
		for ( vector<WORD>::iterator t = ToRemove.begin(); t != ToRemove.end(); ++t )
		{
			RemoveConnectivity( *p, *t );
			pOwner->RemoveConnectivity( *p, *t );
		}
		ToRemove.clear();
	}
	//
	for ( vector<WORD>::iterator p = Points.begin(); p != Points.end(); ++p )
		if ( Connectivities[*p].size() < 2 )
			ToRemove.push_back( *p );
	//
	for ( vector<WORD>::iterator t = ToRemove.begin(); t != ToRemove.end(); ++t )
		RemovePoint( *t );
	ToRemove.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTriangulater::SortPoints( CConnectedPoints *pOwner )
{
	// удал€ем некорректные точки
	vector<WORD> PassedPoints;
	EraseInnerEdges( pOwner );
	if ( Points.size() < 3 )
		return;
	//
	vector<WORD> TmpPoints;
	int nPrev = Points[0];
	int nCurrent = Points[1];
	float fClockwise = 0;
	float fAntiClockwise = 0;
	//
	TmpPoints.push_back( nPrev );
	PassedPoints.push_back( nPrev );
	TmpPoints.push_back( nCurrent );
	// сравнение текущей точки с первой не примен€етс€, т.к. встречаютс€ незамкнутые контуры
	while ( find( PassedPoints.begin(), PassedPoints.end(), nCurrent ) == PassedPoints.end() )
	{
		// ищем следующую точку
		int nNext;
		for ( vector<WORD>::iterator i = Connectivities[nCurrent].begin(); i != Connectivities[nCurrent].end(); ++i )
			if ( *i != nPrev )
			{
				nNext = *i;
				break;
			}
		//
		if ( find( PassedPoints.begin(), PassedPoints.end(), nNext ) == PassedPoints.end() )
			TmpPoints.push_back( nNext );
		PassedPoints.push_back( nCurrent );
		nPrev = nCurrent;
		nCurrent = nNext;
	}
	//
	Points.clear();
	for ( vector<WORD>::iterator i = TmpPoints.begin(); i != TmpPoints.end(); ++i )
		Points.push_back( *i );
	PassedPoints.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTriangulater::GetTriangulation( list<STriangle> *pRes, list<int> *pSign )
{
	//DebugOutput("Triangulation");
	//
	// ищем все треугольники со специальной точкой 
	vector<WORD>::iterator i;
	vector<WORD>::iterator j;
	for ( i = Connectivities[nSpecialPoint].begin(); i != Connectivities[nSpecialPoint].end(); ++i )
	{
		for ( j = Connectivities[*i].begin(); j != Connectivities[*i].end(); ++j )
		{
			if ( *j != nSpecialPoint && *i > *j )
				if ( find( Connectivities[*j].begin(), Connectivities[*j].end(), nSpecialPoint ) != 
					Connectivities[*j].end() )
				{
					pRes->push_back( STriangle( nSpecialPoint, *i, *j ) );
					pSign->push_back( GetTriangleSign( nSpecialPoint, *i, *j ) );
				}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SPyramid
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPyramid
{
	ZDATA
	WORD i1, i2, i3, i4;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&i1); f.Add(3,&i2); f.Add(4,&i3); f.Add(5,&i4); return 0; }
	//
	SPyramid() {}
	SPyramid( WORD _i1, WORD _i2, WORD _i3, WORD _i4 ):
		i1(_i1), i2(_i2), i3(_i3), i4(_i4) {}
	//
	float GetVolume( CVec3 pt1, CVec3 pt2, CVec3 pt3, CVec3 pt4 );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
float SPyramid::GetVolume( CVec3 pt1, CVec3 pt2, CVec3 pt3, CVec3 pt4 )
{
	// находим площадь основани€
	CVec3 vA = pt4 - pt2;
	float fA = fabs( vA );
	CVec3 vC = pt4 - pt3;
	Normalize( &vA );
	float fA1 = vA * vC;
	float fC = fabs( vC );
	float fH = sqrt( fC*fC - fA1 * fA1 );
	float fS = fA * fH / 2;
	// нормаль к основанию пирамиды
	CVec3	vB( pt3 - pt2 );
	CVec3 vN = vA ^ vB;
  Normalize( &vN );
	// высота пирамиды
	float fPH = fabs( ( pt2 - pt1 ) * vN );
	// объем пирамиды
	return fS * fPH / 3.f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVolumeCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CVolumeCalcer: public CConnectedPoints
{
	OBJECT_BASIC_METHODS(CVolumeCalcer);
	ZDATA
	ZPARENT(CConnectedPoints);
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CConnectedPoints*)this); return 0; }
public:
	//
	CVolumeCalcer() {}
	//
	float CalculateVolume();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
float CVolumeCalcer::CalculateVolume()
{
	//DEBUG{
	char szStr[128];
	//DEBUG}

	float fRes = 0;
	DebugOutput( "Object" );
	//
	while ( Points.size() > 4 )
	{
		// выбираем точку и находим дл€ нее все соседние точки и их триангул€цию
		list<int> Signs;
		list<STriangle> Triangles;
		int n = Points.back();
		CPtr<CTriangulater> pTriangulater = new CTriangulater();
		//
		for ( vector<WORD>::const_iterator i = Connectivities[n].begin(); i != Connectivities[n].end(); ++i )
		{
			pTriangulater->AddPoint( *i );
			pTriangulater->AddPointCoords( *i, PointsCoords[*i] );
			for ( vector<WORD>::const_iterator j = Connectivities[*i].begin(); j != Connectivities[*i].end(); ++j )
			{
				if ( find( Connectivities[n].begin(), Connectivities[n].end(), *j ) != Connectivities[n].end() )
					pTriangulater->AddConnectivity( *i, *j );
			}
		}
		//
		pTriangulater->CalculateNormal( PointsCoords[n] );
		//pTriangulater->DebugOutput( "Triangulation ( pre.sort )" );
		pTriangulater->SortPoints( this );
		//pTriangulater->DebugOutput( "Triangulation ( after.sort )" );
		// переносим св€зи вершины в другую точку
		int k = Connectivities[n].front();
//
//		int nCount = 0;
//		for ( vector<WORD>::const_iterator j = pTriangulater->Points.begin(); j != pTriangulater->Points.end(); ++j )
//		{
//			if ( pTriangulater->Connectivities[*j].size() > nCount )
//			{
//				k = *j;
//				nCount = pTriangulater->Connectivities[*j].size();
//			}
//		}

		pTriangulater->SetSpecialPoint( k );
		for ( vector<WORD>::const_iterator j = Connectivities[n].begin(); j != Connectivities[n].end(); ++j )
			if ( *j != k )
				if ( find( Connectivities[n].begin(), Connectivities[n].end(), *j ) != Connectivities[n].end() )
				{
					AddConnectivity( k, *j );
					pTriangulater->AddConnectivity( k, *j );
				}
		//
		pTriangulater->GetTriangulation( &Triangles, &Signs );
		// составл€ем пирамиды из найденных треугольников
		list<STriangle>::const_iterator t = Triangles.begin();
		list<int>::const_iterator s = Signs.begin();
		//DEBUG{
		sprintf( szStr, " Triangles for point <%d> : ", n );
		OutputDebugString( szStr );
		//DEBUG}
		float fTmpRes = 0;
		for ( ; t != Triangles.end(); ++t, ++s )
		{
			// считаем объем
			SPyramid p;
			fTmpRes += p.GetVolume( PointsCoords[n], 
				PointsCoords[(*t).i1], PointsCoords[(*t).i2], PointsCoords[(*t).i3] ) * (*s);
			//DEBUG{
			sprintf( szStr, " <%d %d %d> ", (*t).i1, (*t).i2, (*t).i3 );
			OutputDebugString( szStr );
			//DEBUG}
		}
		//
		OutputDebugString( "\n" );
		RemovePoint( n );
		RemoveIncorrectPoints( 3 );
		DebugOutput( "Object" );
		fRes += fabs( fTmpRes ); // суммарный объем всегда положительный
	}
	//
	if ( Points.size() == 4 )
	{
		// считаем объем
		SPyramid p;
		fRes += p.GetVolume( PointsCoords[Points[0]], 
			PointsCoords[Points[1]], PointsCoords[Points[2]], PointsCoords[Points[3]] );
	}
	//
	return fRes;
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
float CalculateObjectVolume( const vector<CVec3> &points, const vector<STriangle> &tris )
{
	// filter wrong tris
	vector<STriangle> fTris;
	for ( int k = 0; k < tris.size(); ++k )
	{
		const STriangle &t = tris[k];
		if ( t.i1 == t.i2 || t.i2 == t.i3 || t.i3 == t.i1 )
			continue;
		fTris.push_back( t );
	}

	if ( fTris.empty() )
		return 0;
	WORD nSrc = fTris[0].i1, nDst = fTris[0].i2;
	float fAddedVolume = 0;
	vector<STriangle> newTris;
	for ( int k = 0; k < fTris.size(); ++k )
	{
		const STriangle &t = fTris[k];
		if ( t.i1 == nSrc )
		{
			fAddedVolume += CalcPyramidVolume( points[nDst], points[t.i1], points[t.i2], points[t.i3] );
			newTris.push_back( STriangle( nDst, t.i2, t.i3 ) );
		}
		else if ( t.i2 == nSrc )
		{
			fAddedVolume += CalcPyramidVolume( points[nDst], points[t.i1], points[t.i2], points[t.i3] );
			newTris.push_back( STriangle( t.i1, nDst, t.i3 ) );
		}
		else if ( t.i3 == nSrc )
		{
			fAddedVolume += CalcPyramidVolume( points[nDst], points[t.i1], points[t.i2], points[t.i3] );
			newTris.push_back( STriangle( t.i1, t.i2, nDst ) );
		}
		else 
			newTris.push_back( STriangle( t.i1, t.i2, t.i3 ) );
	}
	return fAddedVolume + CalculateObjectVolume( points, newTris );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CalculateObjectVolume( const vector<CVec3> &points, const CEdgesInfo &edges )
{
	/*CPtr<CVolumeCalcer> pCalcer = new CVolumeCalcer();
	//
	WORD n = 0;
	for ( vector<CVec3>::const_iterator i = points.begin(); i != points.end(); ++i, ++n )
		pCalcer->AddPointCoords( n, *i );*/
	//
	vector<STriangle> triangles;
	edges.BuildTriangleList( &triangles );
	return CalculateObjectVolume( points, triangles );
/*	for ( vector<STriangle>::iterator i = Triangles.begin(); i != Triangles.end(); ++i )
	{
		pCalcer->AddPoint( (*i).i1 );
		pCalcer->AddPoint( (*i).i2 );
		pCalcer->AddPoint( (*i).i3 );
		pCalcer->AddConnectivity( (*i).i1, (*i).i2 );
		pCalcer->AddConnectivity( (*i).i2, (*i).i3 );
		pCalcer->AddConnectivity( (*i).i3, (*i).i1 );
	}
	//
	return pCalcer->CalculateVolume();*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CalculateObjectVolume( const CGeometryInfo::SPiece &Piece )
{
	return CalculateObjectVolume( Piece.points, Piece.edges );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CalculateObjectVolume( const CGeometryInfo &GeometryInfo )
{
	float fRes = 0;
	unordered_map<int, CGeometryInfo::SPiece>::const_iterator i;
	for ( i = GeometryInfo.pieces.begin(); i != GeometryInfo.pieces.end(); ++i )
	{
		float fTmpRes = CalculateObjectVolume( i->second.points, i->second.edges );
		fRes += fTmpRes;
	}

		//fRes += CalculateObjectVolume( i->second.points, i->second.edges );
	return fRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CalculateObjectVolume( const CFileSkinPoints &FileSkinPoints )
{
	float fRes = 0;
	unordered_map<int, CFileSkinPoints::SBodypart>::const_iterator i;
	for ( i = FileSkinPoints.parts.begin(); i != FileSkinPoints.parts.end(); ++i )
		fRes += CalculateObjectVolume( i->second.points, i->second.edges );
	return fRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void VolumeCalcerTest( int nID )
{
	return;

	CPtr<NAI::CLoadGeometryInfo> pGInfo = new NAI::CLoadGeometryInfo;
	pGInfo->SetKey( nID );
	CDGPtr<CPtrFuncBase<NAI::CGeometryInfo> > pFunc = pGInfo;
	pFunc.Refresh();
	NAI::CGeometryInfo *pInfo = pFunc->GetValue();
	float fVal = NAI::CalculateObjectVolume( *pInfo );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NAI;
//REGISTER_SAVELOAD_CLASS( 0x51452110, CVolumeCalcer );
//REGISTER_SAVELOAD_CLASS( 0x51452111, CTriangulater );