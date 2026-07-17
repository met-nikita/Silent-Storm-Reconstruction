#include "StdAfx.h"
#include <limits>
#include "Grid.h"
#include "RPGGame.h"
#include "TerrainInfo.h"
#include "wTerrain.h"
#include "PolyUtils.h"
#include "Transform.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "GSceneUtils.h"
#include "GGrass.h"
#include "GGeometry.h"
#include "TerrainInfo.h"
#include "MapBuild.h"
#include "..\Misc\RandomGen.h"
#include "wTSFlags.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NTerrain
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Internal structures
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_TERRA_STEP = 8;
const float 
	FP_EPS					= 1e-05f,
	FP_WALL_HEIGHT	= 5.0f * FP_GRID_STEP / FP_TERRAIN_H_SCALE;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct STessPoint
{
	ZDATA
	int nIndex;
	float fNormal;
	CVec3 vPoint;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nIndex); f.Add(3,&fNormal); f.Add(4,&vPoint); return 0; }
	
	STessPoint( float _fNormal, const CVec3 &_vPoint, int _nIndex ): fNormal(_fNormal), vPoint(_vPoint), nIndex(_nIndex) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////////////////////////
//! ���������� ������ ����� � ������������ �����
float GetHeight( float fX, float fY, const STerrainInfo &sInfo )
{
	int nX = fX;
	int nY = fY;
	float fDX = fX - (float)nX;
	float fDY = fY - (float)nY;
	
	ASSERT( ( nX <= sInfo.nWidth ) && ( nY <= sInfo.nHeight ) );
	
	unsigned short nZ00 = sInfo.heightMap[nY][nX];
	unsigned short nZ01 = nZ00, nZ10 = nZ00, nZ11 = nZ00;
	if ( nX + 1 < sInfo.heightMap.GetXSize() )
		nZ01 = sInfo.heightMap[nY][nX + 1];
	if ( nY + 1 < sInfo.heightMap.GetYSize() )
		nZ10 = sInfo.heightMap[nY + 1][nX];
	if ( ( nX + 1 < sInfo.heightMap.GetXSize() ) && ( nY + 1 < sInfo.heightMap.GetYSize() ) )
		nZ11 = sInfo.heightMap[nY + 1][nX + 1];

	float fSampleX1 = ( nZ01 - nZ00 ) * fDX + nZ00;
	float fSampleX2 = ( nZ11 - nZ10 ) * fDX + nZ10;
	
	return ( fSampleX2 - fSampleX1 ) * fDY + fSampleX1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef CVec2 TTriangle[3];
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsPointInTriangle( const CVec2 &vPoint, const TTriangle &vTri )
{
	bool bInside = false;
	
	for ( int nTemp = 0; nTemp < 3; nTemp++ )
	{
		const CVec2 &vBeg = vTri[nTemp];
		const CVec2 &vEnd = vTri[( nTemp + 1 ) % 3];
		if ( ( vEnd.y <= vPoint.y ) && ( vPoint.y < vBeg.y ) && ( ( vBeg.y - vEnd.y ) * ( vPoint.x - vEnd.x) < ( vPoint.y - vEnd.y ) * ( vBeg.x - vEnd.x ) ) )
			bInside = !bInside;
		else if ( ( vBeg.y <= vPoint.y ) && ( vPoint.y < vEnd.y ) && ( ( vBeg.y - vEnd.y) * ( vPoint.x - vEnd.x ) > ( vPoint.y - vEnd.y ) * ( vBeg.x - vEnd.x ) ) )
			bInside = !bInside;
	}
	
	return bInside;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsPointInRect( const CVec2 &vPoint, const CVec2 &vLineBeg, const CVec2 &vLineEnd )
{
	const float FP_EPS = 1e-05f;
	float fMinX = Min( vLineBeg.x, vLineEnd.x );
	float fMaxX = Max( vLineBeg.x, vLineEnd.x );
	float fMinY = Min( vLineBeg.y, vLineEnd.y );
	float fMaxY = Max( vLineBeg.y, vLineEnd.y );
	if ( fMinX - vPoint.x > FP_EPS )
		return false;
	if ( fMaxX - vPoint.x < -FP_EPS )
		return false;
	if ( fMinY - vPoint.y > FP_EPS ) 
		return false;
	if ( fMaxY - vPoint.y < -FP_EPS )
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int TessellatePoly( CTerrainPart *pValue, int nIndex, const vector<CVec2> &vPolygon, int nHeight, const CTRect<int> &sRegion, const STerrainInfo &sInfo )
{
	bool bTryAgain = true;
	float fRange = -FP_EPS;

	if ( vPolygon.size() < 3 )
		return 0;
	
	for ( int nTemp = 0; nTemp < vPolygon.size(); nTemp++ )
	{
		CVec3 vVertex;
		vVertex.x = vPolygon[nTemp].x;
		vVertex.y = vPolygon[nTemp].y;
		vVertex.z = ( GetHeight( vPolygon[nTemp].x, vPolygon[nTemp].y, sInfo ) + nHeight );
		pValue->verts.push_back( vVertex );
	}	

	list<STessPoint> poly;
	for ( int i = 0; i < vPolygon.size(); i++ )
	{
		const CVec2 &vPrev2 = vPolygon[ (i-1+vPolygon.size()) % vPolygon.size() ];
		const CVec2 &vNext2 = vPolygon[ (i+1) % vPolygon.size() ];
		const CVec2 &vTemp2 = vPolygon[i];
		CVec3 vPrev( vPrev2.x, vPrev2.y, 0 );
		CVec3 vNext( vNext2.x, vNext2.y, 0 );
		CVec3 vTemp( vTemp2.x, vTemp2.y, 0 );
		CVec3 vNormal( (vPrev - vTemp) ^ (vNext - vTemp) );
		poly.push_back( STessPoint( vNormal.z, vTemp, i ) );
	}

	while( poly.size() > 2 )	
	{
		bool bRemoved = false;
		list<STessPoint>::iterator iPrev, iPrev2;
		iPrev = poly.end();
		iPrev--;
		iPrev2 = iPrev;
		iPrev2--;
		for ( list<STessPoint>::iterator i = poly.begin(); i != poly.end(); iPrev2 = iPrev, iPrev = i, ++i )
		{
			const STessPoint &prev = *iPrev2;
			const STessPoint &temp = *iPrev;
			const STessPoint &next = *i;
			
			if ( temp.fNormal > fRange )
				continue;

			TTriangle vTri = { 
				CVec2( prev.vPoint.x, prev.vPoint.y ), 
				CVec2( temp.vPoint.x, temp.vPoint.y ),
				CVec2( next.vPoint.x, next.vPoint.y ) };
			bool bHasInside = false;
			for ( int i = 0; i < vPolygon.size(); i++ )
			{
				CVec2 vTest( vPolygon[i].x, vPolygon[i].y );

				bool bGoOut = false;
				for ( int nTemp = 0; nTemp < 3; nTemp++ )
				{
					const CVec2 &vTemp = vTri[nTemp];
					const CVec2 &vNext = vTri[(nTemp + 1) % 3];

					if ( vTest == vTri[nTemp] )
						bGoOut = true;
				}
				if ( bGoOut )
					continue;
				
				bHasInside = IsPointInTriangle( vTest, vTri );
				if ( bHasInside )
					break;
			}
			if ( bHasInside )
				continue;

			pValue->faces.push_back( STriangle( nIndex + prev.nIndex, nIndex + temp.nIndex, nIndex + next.nIndex ) );
			poly.erase( iPrev );
			bRemoved = true;
			break;
		}
		if ( bRemoved )
		{
			list<STessPoint>::iterator iPrev, iPrev2;
			iPrev = poly.end();
			iPrev--;
			iPrev2 = iPrev;
			iPrev2--;
			for ( list<STessPoint>::iterator i = poly.begin(); i != poly.end(); iPrev2 = iPrev, iPrev = i, ++i )
			{
				STessPoint &temp = *iPrev;
				const STessPoint &prev = *iPrev2;
				const STessPoint &next = *i;

				CVec3 vPrev( prev.vPoint.x, prev.vPoint.y, 0 );
				CVec3 vNext( next.vPoint.x, next.vPoint.y, 0 );
				CVec3 vTemp( temp.vPoint.x, temp.vPoint.y, 0 );
				CVec3 vNormal( (vPrev - vTemp) ^ (vNext - vTemp) );
				temp.fNormal = vNormal.z;
			}
		}
		else if ( bTryAgain )
		{
			bTryAgain = false;
			fRange = 0;
		}
		else
			break;
	}

	if ( poly.size() > 2 )
	{
		DebugTrace( "Dump:\n" );
		for ( int nTemp = 0; nTemp < vPolygon.size(); nTemp++ )
			DebugTrace( "%.2f %.2f\n", vPolygon[nTemp].x, vPolygon[nTemp].y );

		DebugTrace( " after tess:\n" );
		for ( list<STessPoint>::iterator i = poly.begin(); i != poly.end(); ++i )
			DebugTrace( "%.2f %.2f\n", i->vPoint.x, i->vPoint.y );
	}
	ASSERT( poly.size() <= 2 );

	return vPolygon.size();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// 
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGroup
{
	ZDATA
	SRandomSeed sSeed;
	CTRect<int> sRegion;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sSeed); f.Add(3,&sRegion); return 0; }

	SGroup(): sSeed( 0 ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGroupInfo
{
	ZDATA
	SGroup sGroup;
	list<SMapHole> holesList;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sGroup); f.Add(3,&holesList); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPart
{
	ZDATA
	int nFloor;
	CObj<CPtrFuncBase<CTerrainPart> > pPart;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nFloor); f.Add(3,&pPart); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPartsGroup
{
	ZDATA
	SGroup sGroup;
	list<SPart> partsList;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sGroup); f.Add(3,&partsList); return 0; }
};
struct STerrainGeometry
{
	ZDATA
	list<SPart> wallsList;
	list<SPart> terrainWallsList;
	list<SPartsGroup> groupsList;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&wallsList); f.Add(3,&terrainWallsList); f.Add(4,&groupsList); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPartBuilder
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPartBuilder: public CPtrFuncBase<CTerrainPart>
{
	OBJECT_BASIC_METHODS( CPartBuilder );
private:
	ZDATA
	SGroup sGroup;
	SMapHole sHole;
	CDGPtr<CFuncBase<STerrainInfo> > pInfo;
	CDGPtr<CVersioningBase> pRegionUpdate;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sGroup); f.Add(3,&sHole); f.Add(4,&pInfo); f.Add(5,&pRegionUpdate); return 0; }

protected:
	void Recalc();
	virtual bool NeedUpdate() { return pRegionUpdate.Refresh(); }
	
public:
	CPartBuilder() {}
	CPartBuilder( const SGroup &_sGroup, const SMapHole &_sHole, CFuncBase<STerrainInfo> *_pInfo, CVersioningBase *pRegUpdate )
		: sGroup(_sGroup), sHole(_sHole), pInfo(_pInfo), pRegionUpdate(pRegUpdate) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static void MergePoints( CTerrainPart *p )
{
	vector<WORD> posIndices;
	NGScene::MergePositions( &posIndices, &p->verts );
	NGScene::FilterTrinagles( &p->faces, posIndices );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPartBuilder::Recalc()
{
	pInfo.Refresh();
	const STerrainInfo &sInfo = pInfo->GetValue();

	pValue = new CTerrainPart;

	const CTRect<int> &sRegion = sGroup.sRegion;
	int nPartXSize = sRegion.Width();
	int nPartYSize = sRegion.Height();

	if ( sHole.polygonsList.empty() )
	{
		for ( int nTempY = 0; nTempY <= nPartYSize; nTempY++ )
		{
			for ( int nTempX = 0; nTempX <= nPartXSize; nTempX++ )
			{
				pValue->verts.push_back( CVec3( nTempX + sRegion.x1, nTempY + sRegion.y1, sInfo.heightMap[nTempY + sRegion.y1][nTempX + sRegion.x1] ) );
			}
		}
		for ( int nTempY = 0; nTempY < nPartYSize; nTempY++ )
		{
			for ( int nTempX = 0; nTempX < nPartXSize; nTempX++ )
			{
				CTRect<int> rTileIndex( nTempX, nTempY * ( nPartXSize + 1 ), nTempX + 1, ( nTempY + 1 ) * ( nPartXSize + 1 ) );				
				pValue->faces.push_back( STriangle( rTileIndex.y1 + rTileIndex.x2, rTileIndex.y2 + rTileIndex.x1, rTileIndex.y1 + rTileIndex.x1 ) );
				pValue->faces.push_back( STriangle( rTileIndex.y1 + rTileIndex.x2, rTileIndex.y2 + rTileIndex.x2, rTileIndex.y2 + rTileIndex.x1 ) );
			}
		}
		return;
	}

	TPolygonsList polygonsList;
	for ( int nTempY = 0; nTempY < nPartYSize; nTempY++ )
	{
		for ( int nTempX = 0; nTempX < nPartXSize; nTempX++ )
		{
			vector<CVec2> &pointsSet = *polygonsList.insert( polygonsList.end(), vector<CVec2>());
			pointsSet.resize( 4 );
			
			CTRect<int> rTileIndex( nTempX + sRegion.x1, nTempY + sRegion.y1, nTempX + 1 + sRegion.x1, nTempY + 1 + sRegion.y1 );
			pointsSet[0] = CVec2( rTileIndex.x1, rTileIndex.y1 );
			pointsSet[1] = CVec2( rTileIndex.x2, rTileIndex.y1 );
			pointsSet[2] = CVec2( rTileIndex.x2, rTileIndex.y2 );
			pointsSet[3] = CVec2( rTileIndex.x1, rTileIndex.y2 );
		}
	}

	TPolygonsList clippedPolygonsList;
	for( TPolygonsList::const_iterator iTempPoly = polygonsList.begin(); iTempPoly != polygonsList.end(); iTempPoly++ )
	{
		TPolygonsList tempList;
		tempList.push_back( *iTempPoly );

		TPolygonsList intPolygonsList;
		ClipPolygon( tempList, sHole.polygonsList, &intPolygonsList, 0 );

		clippedPolygonsList.insert( clippedPolygonsList.end(), intPolygonsList.begin(), intPolygonsList.end() );
	}

	int nIndex = 0;
	for ( TPolygonsList::const_iterator iTempPoly = clippedPolygonsList.begin(); iTempPoly != clippedPolygonsList.end(); iTempPoly++ )
		nIndex += TessellatePoly( pValue, nIndex, *iTempPoly, sHole.nHeight, sGroup.sRegion, sInfo );

	MergePoints( pValue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTerrainHoleWallPart
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHoleWallBuilder: public CPtrFuncBase<CTerrainPart>
{
	OBJECT_BASIC_METHODS(CHoleWallBuilder);
private:
	ZDATA
	SMapWall sHole;
	CDGPtr<CFuncBase<STerrainInfo> > pInfo;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sHole); f.Add(3,&pInfo); return 0; }

protected:
	void Recalc();
	void InsertPoint( list<CVec2> *pList, const CVec2 &vPoint );

public:
	CHoleWallBuilder() {}
	CHoleWallBuilder( const SMapWall &_sHole, CFuncBase<STerrainInfo> *_pInfo ): sHole(_sHole), pInfo(_pInfo) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHoleWallBuilder::InsertPoint( list<CVec2> *pList, const CVec2 &vPoint )
{
	list<CVec2>::iterator iPoint = pList->begin();
	for ( int nTemp = 0; nTemp < pList->size() - 1; nTemp++ )
	{
		list<CVec2>::iterator iNextPoint = iPoint;
		iNextPoint++;

		const CVec2 &vInsLineBeg = *iPoint;
		const CVec2 &vInsLineEnd = *iNextPoint;
		
		if ( IsPointInRect( vPoint, vInsLineBeg, vInsLineEnd ) )
		{
			list<CVec2>::iterator iNext = iPoint;
			iNext++;
			pList->insert( iNext, vPoint );
			break;
		}
		iPoint++;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHoleWallBuilder::Recalc()
{
	pInfo.Refresh();
	const STerrainInfo &sInfo = pInfo->GetValue();

	pValue = new CTerrainPart;
	pValue->bInverseNormals = ( sHole.nHeightMax < sHole.nHeightMin ) ? false : true;

//	if ( sHole.polygonsList.size() == 0 )
//		return;

	int nIndex = 0;
	/*
	for ( TPolygonsList::iterator iPolygon = sHole.polygonsList.begin(); iPolygon != sHole.polygonsList.end(); iPolygon++ )
	{
		int nIndex = 0;
		const vector<CVec2> &polyPoints = *iPolygon;

		if ( IsPolygonInverse( polyPoints ) )
			continue;

		for ( int nTemp = 0; nTemp < polyPoints.size(); nTemp++ )
		{
		*/
			const CVec2 &vLineBeg = sHole.vBeg;//polyPoints[nTemp];
			const CVec2 &vLineEnd = sHole.vEnd;//polyPoints[( nTemp + 1 ) % polyPoints.size()];

			list<CVec2> listPoints;
			listPoints.push_back( vLineBeg );
			listPoints.push_back( vLineEnd );

			int nMinX = (int)Min( vLineBeg.x, vLineEnd.x ) - 1;
			int nMinY = (int)Min( vLineBeg.y, vLineEnd.y ) - 1;
			int nMaxX = (int)Max( vLineBeg.x, vLineEnd.x ) + 1;
			int nMaxY = (int)Max( vLineBeg.y, vLineEnd.y ) + 1;

			for ( int nTempY = nMinY; nTempY < nMaxY; nTempY++ )
			{
				CVec2 vTemp;
				ELineDispos eDisp = IntersectLines( vLineBeg, vLineEnd, CVec2( nMinX, nTempY ), CVec2( nMaxX, nTempY ), &vTemp );
				if ( eDisp == INTERSECT )
					InsertPoint( &listPoints, vTemp );
			}
			for ( int nTempX = nMinX; nTempX < nMaxX; nTempX++ )
			{
				CVec2 vTemp;
				ELineDispos eDisp = IntersectLines( vLineBeg, vLineEnd, CVec2( nTempX, nMinY ), CVec2( nTempX, nMaxY ), &vTemp );
				if ( eDisp == INTERSECT )
					InsertPoint( &listPoints, vTemp );
			}

			for ( list<CVec2>::iterator iPoint = listPoints.begin(); iPoint != listPoints.end(); iPoint++ )
			{
				pValue->verts.push_back( CVec3( iPoint->x, iPoint->y, GetHeight( iPoint->x, iPoint->y, sInfo ) + sHole.nHeightMin ) );
				pValue->verts.push_back( CVec3( iPoint->x, iPoint->y, GetHeight( iPoint->x, iPoint->y, sInfo ) + sHole.nHeightMax ) );
				nIndex += 2;
			}
			/*
		}
		*/

		for ( int nTemp = 0; nTemp < nIndex - 2; nTemp += 2 )
		{
			int nTriIndex[4] = { nTemp + 0, nTemp + 1, nTemp + 2, nTemp + 3 };

			if ( nTriIndex[1] >= nIndex ) nTriIndex[1] -= nIndex;
			if ( nTriIndex[2] >= nIndex ) nTriIndex[2] -= nIndex;
			if ( nTriIndex[3] >= nIndex ) nTriIndex[3] -= nIndex;

			/// CRAP
			int nBaseIndex = 0;
			pValue->faces.push_back( STriangle( nBaseIndex + nTriIndex[0], nBaseIndex + nTriIndex[2], nBaseIndex + nTriIndex[1] ) );
			pValue->faces.push_back( STriangle( nBaseIndex + nTriIndex[2], nBaseIndex + nTriIndex[3], nBaseIndex + nTriIndex[1] ) );
		}
		/*
		nBaseIndex += nIndex;
	}
	*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTerrainWallBuilder
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTerrainWallBuilder: public CPtrFuncBase<CTerrainPart>
{
	OBJECT_BASIC_METHODS(CTerrainWallBuilder);
public:
	enum EEdge
	{
		TOP,
		LEFT,
		RIGHT,
		BOTTOM
	};

private:
	ZDATA
	int nBegTile;
	int nEndTile;
	EEdge eEdge;
	float fHeight;
	CDGPtr<CFuncBase<STerrainInfo> > pInfo;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nBegTile); f.Add(3,&nEndTile); f.Add(4,&eEdge); f.Add(5,&fHeight); f.Add(6,&pInfo); return 0; }

protected:
	void Recalc();

public:
	CTerrainWallBuilder() {}
	CTerrainWallBuilder( EEdge eEdge, int nBegTile, int nEndTile, float fHeight, CFuncBase<STerrainInfo> *_pInfo );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CTerrainWallBuilder::CTerrainWallBuilder( EEdge _eEdge, int _nBegTile, int _nEndTile, float _fHeight, CFuncBase<STerrainInfo> *_pInfo ):
	eEdge( _eEdge ), nBegTile( _nBegTile ), nEndTile( _nEndTile ), fHeight( _fHeight ), pInfo( _pInfo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainWallBuilder::Recalc()
{
	pInfo.Refresh();
	const STerrainInfo &sInfo = pInfo->GetValue();

	pValue = new CTerrainPart;
	pValue->bInverseNormals = false;

	int nBegX = 0, nBegY = 0, nEndX = 0, nEndY = 0;
	float fShiftX = 0, fShiftY = 0;
	switch( eEdge )
	{
	case TOP:
		nBegY = nEndY = 0;
		nBegX = nBegTile;
		nEndX = nEndTile;
		fShiftX = 0;
		fShiftY = 0;
		pValue->bInverseNormals = true;
		break;
	case LEFT:
		nBegX = nEndX = 0;
		nBegY = nBegTile;
		nEndY = nEndTile;
		fShiftX = 0;
		fShiftY = 0;
		pValue->bInverseNormals = true;
		break;
	case RIGHT:
		nBegX = nEndX = sInfo.heightMap.GetXSize() - 1;
		nBegY = nBegTile;
		nEndY = nEndTile;
		fShiftX = FP_GRID_STEP;
		fShiftY = 0;
		pValue->bInverseNormals = false;
		break;
	case BOTTOM:
		nBegY = nEndY = sInfo.heightMap.GetYSize() - 1;
		nBegX = nBegTile;
		nEndX = nEndTile;
		fShiftX = 0;
		fShiftY = FP_GRID_STEP;
		pValue->bInverseNormals = false;
		break;
	}

	int nIndex = 0;
	int nDelta = Max( abs( nEndX - nBegX ), abs( nEndY - nBegY ) );
	int nDeltaX = ( nEndX - nBegX ) / nDelta;
	int nDeltaY = ( nEndY - nBegY ) / nDelta;

	int nTempX = nBegX;
	int nTempY = nBegY;
	for ( int nTemp = 0; nTemp < nDelta + 1; nTemp++ )
	{
		pValue->verts.push_back( CVec3( nTempX, nTempY, GetHeight( nTempX, nTempY, sInfo ) ) );
		pValue->verts.push_back( CVec3( nTempX, nTempY, GetHeight( nTempX, nTempY, sInfo ) + fHeight ) );

		nTempX += nDeltaX;
		nTempY += nDeltaY;
		nIndex += 2;
	}

	for ( int nTemp = 0; nTemp < nIndex - 2; nTemp += 2 )
	{
		int nTriIndex[4] = { nTemp + 0, nTemp + 1, nTemp + 2, nTemp + 3 };
		pValue->faces.push_back( STriangle( nTriIndex[0], nTriIndex[2], nTriIndex[1] ) );
		pValue->faces.push_back( STriangle( nTriIndex[2], nTriIndex[3], nTriIndex[1] ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBuilder
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBuilder: public CFuncBase<STerrainGeometry>
{
	OBJECT_BASIC_METHODS(CBuilder);
private:
	ZDATA
	int nDefaultFloor;
	vector<SGroup> groupsSet;
	list<SMapHole> holesList;
	list<SMapWall> wallsList;
	CDGPtr<CFuncBase<STerrainInfo> > pInfo;
	CPtr<CTerrainInfoHolder> pInfoHolder;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nDefaultFloor); f.Add(3,&groupsSet); f.Add(4,&holesList); f.Add(5,&wallsList); f.Add(6,&pInfo); f.Add(7,&pInfoHolder); return 0; }

protected:
	void Recalc();
	bool NeedUpdate() { return true; }
	void BuildGroups( const STerrainInfo &sInfo );
	void AddGroupHole( list<SMapHole> *pList, const TPolygonsList &polygonsList, int nHeight, bool bVisible, int nFloor );
	void BuildHolesList( const STerrainInfo &sInfo, vector<SGroupInfo> *pGroupInfoSet );
	void BuildPartsList( const vector<SGroupInfo> &groupInfoSet );
	void BuildHoleWallsList( const STerrainInfo &sInfo );
	void BuildTerrainWallsList( const STerrainInfo &sInfo );
	
public:
	CBuilder() {}
	CBuilder( CTerrainInfoHolder *_pInfo, int _nDefaultFloor, const list<SMapHole> &_holesList, const list<SMapWall> &_wallsList ): pInfo( _pInfo ), nDefaultFloor( _nDefaultFloor ), holesList( _holesList ), wallsList( _wallsList ), pInfoHolder(_pInfo) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilder::BuildGroups( const STerrainInfo &sInfo )
{
	groupsSet.clear();
	groupsSet.reserve( ( sInfo.nWidth * sInfo.nHeight ) / ( N_TERRA_STEP * N_TERRA_STEP ) );

	for ( int nTempY = 0; nTempY < sInfo.nHeight; nTempY += N_TERRA_STEP )
	{
		for ( int nTempX = 0; nTempX < sInfo.nWidth; nTempX += N_TERRA_STEP )
		{
			SGroup &sGroup = *groupsSet.insert( groupsSet.end(), SGroup());

			sGroup.sSeed = SRandomSeed( nTempY * sInfo.nWidth + nTempX );
			sGroup.sRegion = CTRect<int>( nTempX, nTempY, nTempX + N_TERRA_STEP, nTempY + N_TERRA_STEP );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilder::AddGroupHole( list<SMapHole> *pList, const TPolygonsList &polygonsList, int nHeight, bool bVisible, int nFloor )
{
	SMapHole &sHole = *pList->insert( pList->end(), SMapHole());

	sHole.nFloor = nFloor;
	sHole.nHeight = nHeight;
	sHole.bVisible = bVisible;
	sHole.polygonsList = polygonsList;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilder::BuildHolesList( const STerrainInfo &sInfo, vector<SGroupInfo> *pGroupInfoSet )
{
	pGroupInfoSet->resize( groupsSet.size() );
	for ( int nTemp = 0; nTemp < groupsSet.size(); nTemp++ )	
	{
		const SGroup &sGroup = groupsSet[nTemp];
		SGroupInfo &sGroupInfo = (*pGroupInfoSet)[nTemp];
		sGroupInfo.sGroup = sGroup;

		vector<CVec2> vRegion( 4 );
		vRegion[0] = CVec2( sGroup.sRegion.x1, sGroup.sRegion.y1 );
		vRegion[1] = CVec2( sGroup.sRegion.x2, sGroup.sRegion.y1 );
		vRegion[2] = CVec2( sGroup.sRegion.x2, sGroup.sRegion.y2 );
		vRegion[3] = CVec2( sGroup.sRegion.x1, sGroup.sRegion.y2 );

		TPolygonsList sourcePolygonsList, zeroLevelPolygonsList;
		sourcePolygonsList.push_back( vRegion );
		zeroLevelPolygonsList.push_back( vRegion );

		for ( list<SMapHole>::iterator iTempGroupHole = holesList.begin(); iTempGroupHole != holesList.end(); iTempGroupHole++ )
		{
			TPolygonsList intPolygonsList, subPolygonsList;
			ClipPolygon( iTempGroupHole->polygonsList, sourcePolygonsList, &intPolygonsList, 0 );
			ClipPolygon( zeroLevelPolygonsList, iTempGroupHole->polygonsList, 0, &subPolygonsList );

			if ( !intPolygonsList.empty() )
				AddGroupHole( &sGroupInfo.holesList, intPolygonsList, iTempGroupHole->nHeight, iTempGroupHole->bVisible, iTempGroupHole->nFloor );

			zeroLevelPolygonsList = subPolygonsList;
		}

		if ( !zeroLevelPolygonsList.empty() )
			AddGroupHole( &sGroupInfo.holesList, zeroLevelPolygonsList, 0, true, nDefaultFloor );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilder::BuildPartsList( const vector<SGroupInfo> &groupInfoSet )
{
	for ( int nTemp = 0; nTemp < groupInfoSet.size(); nTemp++ )	
	{
		SPartsGroup &sGroup = *value.groupsList.insert( value.groupsList.end(), SPartsGroup());
		const SGroupInfo &sGroupInfo = groupInfoSet[nTemp];

		sGroup.sGroup = sGroupInfo.sGroup;
		if ( !sGroupInfo.holesList.empty() )
		{
			//sGroup.sGroup = sGroupInfo.sGroup;
			for ( list<SMapHole>::const_iterator iTempHole = sGroupInfo.holesList.begin(); iTempHole != sGroupInfo.holesList.end(); iTempHole++ )
			{
				if ( !iTempHole->bVisible )
					continue;
				
				SPart &sPart = *sGroup.partsList.insert( sGroup.partsList.end(), SPart());
				sPart.nFloor = iTempHole->nFloor;
				sPart.pPart = new CPartBuilder( sGroupInfo.sGroup, *iTempHole, pInfo, pInfoHolder->GetRegionGeometry( sGroup.sGroup.sRegion ) );
			}
		}
		else
		{
			SMapHole sHole;
			sHole.nHeight = 0;
			sHole.nFloor = 0;
			sHole.bVisible = true;

			SPart &sPart = *sGroup.partsList.insert( sGroup.partsList.end(), SPart());
			sPart.nFloor = 0;
			sPart.pPart = new CPartBuilder( sGroupInfo.sGroup, sHole, pInfo, pInfoHolder->GetRegionGeometry( sGroup.sGroup.sRegion ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilder::BuildHoleWallsList( const STerrainInfo &sInfo )
{
	for ( list<SMapWall>::iterator iTempWall = wallsList.begin(); iTempWall != wallsList.end(); iTempWall++ )
	{
		SPart &sPart = *value.wallsList.insert( value.wallsList.end(), SPart());
		sPart.nFloor = 0;
		sPart.pPart = new CHoleWallBuilder( *iTempWall, pInfo );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilder::BuildTerrainWallsList( const STerrainInfo &sInfo )
{
	for ( int nTemp = 0; nTemp < sInfo.nHeight; nTemp += N_TERRA_STEP )
	{
		SPart &sPartLeft = *value.terrainWallsList.insert( value.terrainWallsList.end(), SPart());
		sPartLeft.nFloor = 0;
		sPartLeft.pPart = new CTerrainWallBuilder( CTerrainWallBuilder::LEFT, nTemp, nTemp + N_TERRA_STEP, -FP_WALL_HEIGHT, pInfo );

		SPart &sPartRight = *value.terrainWallsList.insert( value.terrainWallsList.end(), SPart());
		sPartRight.nFloor = 0;
		sPartRight.pPart = new CTerrainWallBuilder( CTerrainWallBuilder::RIGHT, nTemp + N_TERRA_STEP, nTemp, -FP_WALL_HEIGHT, pInfo );
	}

	for ( int nTemp = 0; nTemp < sInfo.nWidth; nTemp += N_TERRA_STEP )
	{
		SPart &sPartTop = *value.terrainWallsList.insert( value.terrainWallsList.end(), SPart());
		sPartTop.nFloor = 0;
		sPartTop.pPart = new CTerrainWallBuilder( CTerrainWallBuilder::TOP, nTemp + N_TERRA_STEP, nTemp, -FP_WALL_HEIGHT, pInfo );

		SPart &sPartBottom = *value.terrainWallsList.insert( value.terrainWallsList.end(), SPart());
		sPartBottom.nFloor = 0;
		sPartBottom.pPart = new CTerrainWallBuilder( CTerrainWallBuilder::BOTTOM, nTemp, nTemp + N_TERRA_STEP, -FP_WALL_HEIGHT, pInfo );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBuilder::Recalc()
{
	pInfo.Refresh();
	const STerrainInfo &sInfo = pInfo->GetValue();

	value.wallsList.clear();
	value.groupsList.clear();
	value.terrainWallsList.clear();

	BuildGroups( sInfo );

	vector<SGroupInfo> groupInfoSet;
	BuildHolesList( sInfo, &groupInfoSet );

	BuildPartsList( groupInfoSet );
	BuildHoleWallsList( sInfo );
	BuildTerrainWallsList( sInfo );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
struct STexturedPart
{
	ZDATA
	NTerrain::SPart part;
	CDBPtr<NDb::CTexture> pTex;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&part); f.Add(3,&pTex); return 0; }

	STexturedPart() {}
	STexturedPart( const NTerrain::SPart &_part, NDb::CTexture *pTexture ) : part(_part), pTex(pTexture) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTerrainRegion : public IVisObj
{
	OBJECT_NOCOPY_METHODS(CTerrainRegion);
protected:
	struct SGroupUpdate
	{
		ZDATA
		NTerrain::SPartsGroup group;
		CDGPtr<CVersioningBase> pUpdate;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&group); f.Add(3,&pUpdate); return 0; }

		SGroupUpdate() {}
		SGroupUpdate( const NTerrain::SPartsGroup &g, CVersioningBase *pVersion ): group(g), pUpdate(pVersion) {}
	};
	ZDATA
	CSyncSrcBind<IVisObj> bindGlobal;
	CPtr<CTerrainInfoHolder> pInfo;
	vector<SGroupUpdate> groups;
	vector<STexturedPart> parts;
	vector<STexturedPart> borderparts;
	bool bVisible;
	int nRegionID;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bindGlobal); f.Add(3,&pInfo); f.Add(4,&groups); f.Add(5,&parts); f.Add(6,&borderparts); f.Add(7,&bVisible); f.Add(8,&nRegionID); return 0; }

public:
	CTerrainRegion() {}
	CTerrainRegion( CSyncSrc<IVisObj> *pShow, CTerrainInfoHolder *pTerrainInfo, int _nRegionID, bool _bVisible = true )
		:pInfo(pTerrainInfo), nRegionID(_nRegionID)
	{
		bindGlobal.Link( pShow, this );
		bVisible = _bVisible;
	}

	void AddGroups( const vector<NTerrain::SPartsGroup> &_groups ) 
	{
		for ( int i = 0; i < _groups.size(); ++i )
		{
			const NTerrain::SPartsGroup &g = _groups[i];
			groups.push_back( SGroupUpdate( g, pInfo->GetRegionGeometry( g.sGroup.sRegion ) ) );
		}
	}
	void AddParts( const vector<STexturedPart> &_parts ) { parts = _parts; }
	void AddBorderParts( const vector<STexturedPart> &_parts ) { borderparts = _parts; }

	void Visit( IAIVisitor *pVisitor )
	{
		if ( !bVisible )
			return;
		int nMask = TS_PASS_BLOCKER|TS_TERRAINS|TS_VISION|TS_FRAGMENTED|TS_COVER|TS_WEAPON_BLOCKER|TS_ITEM_BLOCKER;
		for ( vector<SGroupUpdate>::const_iterator iTemp =	groups.begin(); iTemp != groups.end(); iTemp++ )
		{
			const NTerrain::SPartsGroup &sGroup = iTemp->group;
			for ( list<NTerrain::SPart>::const_iterator iTempPart = sGroup.partsList.begin(); iTempPart != sGroup.partsList.end(); iTempPart++ )
				pVisitor->AddTerrainPart( iTempPart->pPart, NRPG::GetTerrainArmor(), iTempPart->nFloor, nMask );
		}
		for ( vector<STexturedPart>::const_iterator iTempPart = parts.begin(); iTempPart != parts.end(); iTempPart++ )
			pVisitor->AddTerrainPart( iTempPart->part.pPart, NRPG::GetTerrainArmor(), iTempPart->part.nFloor, nMask );
	}

	void Visit( IRenderVisitor *pVisitor )
	{
		if ( !bVisible )
			return;
		for ( int k = 0; k < groups.size(); ++k )
		{
			const NTerrain::SPartsGroup &sGroup = groups[k].group;
			list<CObj<CPtrFuncBase<CTerrainPart> > > partsList;
			for ( list<NTerrain::SPart>::const_iterator iTempPart = sGroup.partsList.begin(); iTempPart != sGroup.partsList.end(); iTempPart++ )
				partsList.push_back( iTempPart->pPart );

			pVisitor->AddTerrainParts( sGroup.sGroup.sSeed, sGroup.sGroup.sRegion, partsList, pInfo, pInfo->GetRegionTexture( sGroup.sGroup.sRegion ), k | (nRegionID<<16) );
		}
		for ( int k = 0; k < parts.size(); ++k )
			pVisitor->AddTerrainWallPart( parts[k].part.pPart, parts[k].pTex, pInfo, k | (nRegionID<<16) | 0x10000000 );
		for ( int k = 0; k < borderparts.size(); ++k )
		{
			const STexturedPart &tp = borderparts[k];
			pVisitor->AddTerrainWallPart( tp.part.pPart, tp.pTex, pInfo, k | (nRegionID<<16) | 0x20000000 );
		}
	}

	void Update( bool _bVisible )
	{
		bool bNeedUpdate = false;
		if ( bVisible != _bVisible )
			bNeedUpdate = true;
		bVisible = _bVisible;
		for ( int i = 0; i < groups.size(); ++i )
		{
			if ( groups[i].pUpdate.Refresh() )
			{
				bNeedUpdate = true;
				for ( list<NTerrain::SPart>::iterator it = groups[i].group.partsList.begin(); it != groups[i].group.partsList.end(); ++it )
					it->pPart->Updated();
			}
		}
		if ( bNeedUpdate )
			bindGlobal.Update();
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTerrain
////////////////////////////////////////////////////////////////////////////////////////////////////
CTerrain::CTerrain( CSyncSrc<IVisObj> *pShow, CTerrainInfoHolder *pTerrainInfo, NAI::IAIMap *pAIMap, CFuncBase<STime> *_pTime, int nDefaultFloor, const list<SMapHole> &holesList, const list<SMapWall> &wallsList ):
	pInfo( pTerrainInfo ), pTime( _pTime )
{
	bindGlobal.Link( pShow, this );

	pBuilder = new NTerrain::CBuilder( pTerrainInfo, nDefaultFloor, holesList, wallsList );

	//
	CDGPtr<NTerrain::CBuilder> pTerrainBuilder( pBuilder.GetPtr() );
	pTerrainBuilder.Refresh();
	const NTerrain::STerrainGeometry &sGeometry = pTerrainBuilder->GetValue();

	int nRegionID = 0;
	for ( list<NTerrain::SPartsGroup>::const_iterator iTemp = sGeometry.groupsList.begin(); iTemp != sGeometry.groupsList.end(); iTemp++ )
	{
		vector<NTerrain::SPartsGroup> groups( 1, *iTemp );
		CTerrainRegion *pr = new CTerrainRegion( pShow, pTerrainInfo, ++nRegionID );
		pr->AddGroups( groups );
		regions.push_back( pr );
	}
	for ( list<NTerrain::SPart>::const_iterator iTempPart = sGeometry.wallsList.begin(); iTempPart != sGeometry.wallsList.end(); iTempPart++ )
	{
		vector<STexturedPart> parts( 1, STexturedPart( *iTempPart, NDb::GetTexture( 2 ) ) );
		CTerrainRegion *pr = new CTerrainRegion( pShow, pTerrainInfo, ++nRegionID );
		pr->AddParts( parts );
		regions.push_back( pr );
	}
	for ( list<NTerrain::SPart>::const_iterator iTempPart = sGeometry.terrainWallsList.begin(); iTempPart != sGeometry.terrainWallsList.end(); iTempPart++ )
	{
		vector<STexturedPart> parts( 1, STexturedPart( *iTempPart, NDb::GetTexture( 3693 ) ) );
		CTerrainRegion *pr= new CTerrainRegion( pShow, pTerrainInfo, ++nRegionID );
		pr->AddBorderParts( parts );
		regions.push_back( pr );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrain::Visit( IAIVisitor *pVisitor )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrain::Visit( IRenderVisitor *pVisitor )
{
	pVisitor->AddGrass( pInfo );
	// calc average height
	CDGPtr<CFuncBase<STerrainInfo> >pTerrainInfo( pInfo );
	pTerrainInfo.Refresh();
	const STerrainInfo &info = pTerrainInfo->GetValue();
	float fTotal = 0;
	for ( int y = 0; y < info.heightMap.GetYSize(); ++y )
	{
		for ( int x = 0; x < info.heightMap.GetXSize(); ++x )
			fTotal += info.heightMap[y][x];
	}
	int nTotal = info.heightMap.GetXSize() * info.heightMap.GetYSize();
	if ( nTotal != 0 )
		pVisitor->SetBaseFogHeight( fTotal / nTotal * FP_TERRAIN_H_SCALE );
	else
		pVisitor->SetBaseFogHeight( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrain::Update( bool bVisible /* = true  */ )
{
	for ( int i = 0; i < regions.size(); ++i )
		if ( IsValid( regions[i] ) )
			regions[i]->Update( bVisible );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NGScene::Interpolate @0x140bf0 (scalar form of the MMX per-channel RGBA lerp):
// result = clamp( round( c1 + (c2-c1)*f ), 0, 255 ) per byte. DrawExplosion always blends toward
// black (c2=0), darkening by (1-f).
static DWORD InterpolateColor( DWORD c1, DWORD c2, float f )
{
	DWORD nRes = 0;
	for ( int ch = 0; ch < 4; ++ch )
	{
		const int a = int( ( c1 >> ( ch * 8 ) ) & 0xff );
		const int b = int( ( c2 >> ( ch * 8 ) ) & 0xff );
		const int v = Clamp( Float2Int( a + ( b - a ) * f ), 0, 255 );
		nRes |= DWORD( v ) << ( ch * 8 );
	}
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CTerrain::DrawExplosion @0x38ab50 (disasm-decoded): grass-only blast feedback. The base
// terrain texture is untouched -- the visible soil scorch is the tracker's CDecal; this darkens
// the grass colour, carves the density core and prunes blades, then invalidates the region.
void CTerrain::DrawExplosion( const CVec3 &vCenter, float fRadius )
{
	if ( !IsValid( pInfo ) )
		return;
	STerrainInfo &info = pInfo->GetWritableInfo();
	const int nHX = info.heightMap.GetXSize();
	const int nHY = info.heightMap.GetYSize();
	const float fWorldToColour = 0.8f;             // colour cell = 1.25 = 2*FP_GRID_STEP
	const float fColourToWorld = 1.25f;
	const float fWorldToGrid = 1.0f / FP_GRID_STEP; // 1.6; density cell = FP_GRID_STEP
	const float fCore = fRadius * 0.4f;             // blast core (density clear + blade prune)
	const float fCore2 = fCore * fCore;
	//
	// (1) scorch the grass-colour grid toward black by 1-(d/r)^4 within the full radius
	const int cMinX = Max( 0, Float2Int( ( vCenter.x - fRadius ) * fWorldToColour ) );
	const int cMinY = Max( 0, Float2Int( ( vCenter.y - fRadius ) * fWorldToColour ) );
	const int cMaxX = Float2Int( ( vCenter.x + fRadius ) * fWorldToColour );
	const int cMaxY = Float2Int( ( vCenter.y + fRadius ) * fWorldToColour );
	for ( int nLayer = 0; nLayer < info.grass.size(); ++nLayer )
	{
		SGrassLayer &layer = info.grass[nLayer];
		const int nMaxX = Min( layer.grassColor.GetXSize() - 1, cMaxX );
		const int nMaxY = Min( layer.grassColor.GetYSize() - 1, cMaxY );
		for ( int y = cMinY; y <= nMaxY; ++y )
		{
			for ( int x = cMinX; x <= nMaxX; ++x )
			{
				const int hx = Clamp( 2 * x, 0, nHX - 1 );   // colour cell -> height index
				const int hy = Clamp( 2 * y, 0, nHY - 1 );
				const float dx = vCenter.x - x * fColourToWorld;
				const float dy = vCenter.y - y * fColourToWorld;
				const float dz = vCenter.z - float( info.heightMap[hy][hx] ) * FP_TERRAIN_H_SCALE;
				const float fDist = sqrt( dx * dx + dy * dy + dz * dz );
				if ( fDist <= fRadius )
				{
					float q = fDist / fRadius;
					q = q * q;
					layer.grassColor[y][x] = InterpolateColor( layer.grassColor[y][x], 0, 1.0f - q * q );
				}
			}
		}
	}
	//
	// (2)+(3) clear grass density + prune blades inside the r*0.4 core
	const int gMinX = Max( 0, Float2Int( ( vCenter.x - fRadius ) * fWorldToGrid ) );
	const int gMinY = Max( 0, Float2Int( ( vCenter.y - fRadius ) * fWorldToGrid ) );
	const int gMaxX = Float2Int( ( vCenter.x + fRadius ) * fWorldToGrid );
	const int gMaxY = Float2Int( ( vCenter.y + fRadius ) * fWorldToGrid );
	for ( int nLayer = 0; nLayer < info.grass.size(); ++nLayer )
	{
		SGrassLayer &layer = info.grass[nLayer];
		const int nMaxX = Min( layer.grass.GetXSize() - 1, gMaxX );
		const int nMaxY = Min( layer.grass.GetYSize() - 1, gMaxY );
		for ( int y = gMinY; y <= nMaxY; ++y )
		{
			for ( int x = gMinX; x <= nMaxX; ++x )
			{
				const int hx = Clamp( x, 0, nHX - 1 );       // density cell index == height index
				const int hy = Clamp( y, 0, nHY - 1 );
				const float dx = vCenter.x - x * FP_GRID_STEP;
				const float dy = vCenter.y - y * FP_GRID_STEP;
				const float dz = vCenter.z - float( info.heightMap[hy][hx] ) * FP_TERRAIN_H_SCALE;
				if ( dx * dx + dy * dy + dz * dz <= fCore2 )
					layer.grass[y][x] = 0;
			}
		}
		// blade prune: compact in place, dropping blades inside the core in 3D
		vector<CVec2> &blades = layer.blades;
		const int nCount = blades.size();
		int nDst = 0;
		for ( int nSrc = 0; nSrc < nCount; ++nSrc )
		{
			blades[nDst] = blades[nSrc];
			const float dx = vCenter.x - blades[nSrc].x;
			const float dy = vCenter.y - blades[nSrc].y;
			const float fD2 = dx * dx + dy * dy;
			++nDst;
			if ( fD2 <= fCore2 )
			{
				const int hx = Clamp( Float2Int( blades[nSrc].x * fWorldToGrid ), 0, nHX - 1 );
				const int hy = Clamp( Float2Int( blades[nSrc].y * fWorldToGrid ), 0, nHY - 1 );
				const float dz = float( info.heightMap[hy][hx] ) * FP_TERRAIN_H_SCALE - vCenter.z;
				if ( dz * dz + fD2 <= fCore2 )
					--nDst;   // inside the core in 3D -> drop
			}
		}
		if ( nDst < nCount )
			blades.resize( nDst );
	}
	//
	// (4) invalidate the affected grass region (density bbox grown by one cell each side)
	CTRect<int> rect;
	rect.minx = gMinX - 1;
	rect.miny = gMinY - 1;
	rect.maxx = gMaxX + 1;
	rect.maxy = gMaxY + 1;
	pInfo->UpdateRegionGrass( rect );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0x13051811, CTerrain )
using namespace NTerrain;
REGISTER_SAVELOAD_CLASS( 0x13051812, CBuilder )
REGISTER_SAVELOAD_CLASS( 0x13051813, CPartBuilder )
REGISTER_SAVELOAD_CLASS( 0x13051814, CHoleWallBuilder )
REGISTER_SAVELOAD_CLASS( 0xB3051815, CTerrainWallBuilder )
REGISTER_SAVELOAD_CLASS( 0xA2092180, CTerrainRegion )
