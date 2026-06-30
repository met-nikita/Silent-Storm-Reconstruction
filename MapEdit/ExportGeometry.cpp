#include "StdAfx.h"
#include "MapEdit.h"
#include "..\DBFormat\DataFormat.h"
#include "ItemsMgr.h"
#include "..\Main\aiVolumeCalcer.h"
#include "..\Main\BuildingClip.h"
#include "..\Main\GGeometry.h"
#include "..\Main\GObjectInfo.h"
#include "Export.h"
#include "..\Main\Grid.h"
#include "..\Main\BSPtree.h"
#include "AIGeometryFormat.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLoadGeometryInfo
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPieceInfo
{
	int nPieceID;
	int x,y,z;
	CVec3 ptMin;
	CVec3 ptMax;

	SPieceInfo( int nID, int _x, int _y, int _z, const CVec3 &_ptMin, const CVec3 &_ptMax)
		:nPieceID(nID), x(_x), y(_y), z(_z), ptMin(_ptMin), ptMax(_ptMax) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CollectJunctions( vector<SPieceInfo> *pInfo, SStoredPiece *pPiece, const int nPieceID )
{
	if ( nPieceID == 0 )
		return;
	CVec3 ptMin( 1e30f, 1e30f, 1e30f );
	CVec3 ptMax( -1e30f, -1e30f, -1e30f );
	bool bProcessed = false;
	//
	bProcessed = !pPiece->verts.empty();
	for ( int j = 0; j < pPiece->verts.size(); ++j )
	{
		const CVec3 &pt = pPiece->verts[j];
		ptMin.x = Min( ptMin.x, pt.x );
		ptMin.y = Min( ptMin.y, pt.y );
		ptMin.z = Min( ptMin.z, pt.z );
		ptMax.x = Max( ptMax.x, pt.x );
		ptMax.y = Max( ptMax.y, pt.y );
		ptMax.z = Max( ptMax.z, pt.z );
	}
	//
	if ( !bProcessed )
		return;
	int x, y, z;
	NBuilding::GetPieceCoords( nPieceID, &x, &y, &z );
	pInfo->push_back( SPieceInfo( nPieceID, x, y, z, ptMin, ptMax ) );
	x--;
	y--;
	z--;
	CVec3 pt( x, y, z ); //! NBuilding::WALL_HEIGHT/4 == FP_GRID_STEP
	pt *= FP_GRID_STEP;
	const float F_TOLERANCE = 0.25f * FP_GRID_STEP;
	bool bGr  = 0 == z;
	bool bGrD = 0 == z - 1;
	vector<NAI::SJunction> &js = pPiece->juncs;
	js.clear();
	if ( ptMin.z - (pt.z - FP_GRID_STEP) < F_TOLERANCE  )
		js.push_back( NAI::SJunction( CVec3( x, y, z-1 ), bGrD ) );
	if ( ptMin.x - (pt.x - FP_GRID_STEP) < F_TOLERANCE  )
		js.push_back( NAI::SJunction( CVec3( x-1, y, z ), bGr ) );
	if ( ptMin.y - ( pt.y - FP_GRID_STEP) < F_TOLERANCE  )
		js.push_back( NAI::SJunction( CVec3( x, y-1, z ), bGr ) );
	if ( ptMax.z - (pt.z + FP_GRID_STEP) > -F_TOLERANCE  )
		js.push_back( NAI::SJunction( CVec3( x, y, z+1 ), false ) );
	if ( ptMax.x - (pt.x + FP_GRID_STEP) > -F_TOLERANCE  )
		js.push_back( NAI::SJunction( CVec3( x+1, y, z ), bGr ) );
	if ( ptMax.y - (pt.y + FP_GRID_STEP) > -F_TOLERANCE  )
		js.push_back( NAI::SJunction( CVec3( x, y+1, z ), bGr ) );
}
/*
////////////////////////////////////////////////////////////////////////////////////////////////////
bool FindJunction( const vector<NAI::SJunction> &juncs, const CVec3 &pt )
{
	for ( int i = 0; i < juncs.size(); ++i )
		if ( fabs( juncs[i].pt - pt ) < FP_EPSILON )
			return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int FindLink( int idA, SStoredPiece &a, int idB, SStoredPiece &b )
{
	int ax, ay, az;
	int bx, by, bz;
	NBuilding::GetPieceCoords( idA, &ax, &ay, &az );
	NBuilding::GetPieceCoords( idB, &bx, &by, &bz );
	--ax,--ay,--az;
	--bx,--by,--bz;
	int nDiff = abs(bx - ax) + abs(by - ay) + abs(bz - az);
	if ( 1 == nDiff )
	{
		if ( FindJunction( a.juncs, CVec3( bx, by, bz ) ) || FindJunction( b.juncs, CVec3( ax, ay, az ) ) )
			return 0;
		a.juncs.push_back( NAI::SJunction( CVec3( bx, by, bz ), bz == 0 ) );
		return 1;
	}
	else if ( 2 == nDiff )
	{
		if ( abs( bz - az ) == 2 )
		{
			int nDir = Sign( bz - az );
			if ( FindJunction( a.juncs, CVec3( ax, ay, az + nDir ) ) && FindJunction( b.juncs, CVec3( bx, by, bz - nDir ) ) )
				return 0;
			a.juncs.push_back( NAI::SJunction( CVec3( ax, ay, az + nDir ), az + nDir == 0 ) );
			b.juncs.push_back( NAI::SJunction( CVec3( bx, by, bz - nDir ), bz - nDir == 0 ) );
			return 2;
		}
	}
	else
	{
		ASSERT(0);
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInt
{
	int n;
public:
	CInt( int _n = -1 ): n(_n) {}
	operator int() const { return n; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef unordered_map<int, CInt> CInfoHash;
////////////////////////////////////////////////////////////////////////////////////////////////////
void CheckNeighbour( const SPieceInfo &piece, int nNeighb, const CInfoHash &infohash, const vector<SPieceInfo> &info, CStoredPieceMap &pieces )
{
	CInfoHash::const_iterator i = infohash.find( nNeighb );
	if ( i == infohash.end() && i->second > 0 )
		return;
	const SPieceInfo &pieceN = info[i->second];

	SBound b, bn;
	b.BoxInit( piece.ptMin, piece.ptMax );
	bn.BoxInit( pieceN.ptMin, pieceN.ptMax );

	if ( !DoesIntersect( b, bn ) )
		return;
	CStoredPieceMap::iterator ia = pieces.find( piece.nPieceID );
	CStoredPieceMap::iterator ib = pieces.find( pieceN.nPieceID );
	if ( ia != pieces.end() && ib != pieces.end() )
		FindLink( piece.nPieceID, ia->second, pieceN.nPieceID, ib->second );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void JunctionsPostprocess( const vector<SPieceInfo> &info, CStoredPieceMap &pieces )
{
	if ( info.empty() )
		return;

	CInfoHash infohash;
	for ( int i = 1; i < info.size(); ++i )
		infohash[info[i].nPieceID] = i;
	//
	for ( int i = 1; i < info.size(); ++i )
	{
		const SPieceInfo &p = info[i];

		int nPx, nPy, nPz;
		NBuilding::GetPartCoords( p.nPieceID, &nPx, &nPy, &nPz );
		int nPartHash = NBuilding::GetPartHashID( nPx, nPy, nPz );
		int nTest = nPartHash | NBuilding::GetPieceHashID( p.x, p.y, p.z + 2 );
		int nInd = infohash[nTest];
		if ( nInd > 0 )
		{
			CheckNeighbour( p, nTest, infohash, info, pieces );
		}
	}
}
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateGeometryInfo( int nID )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void GenerateBSPTrees( const vector<CVec3> &points, const NAI::CEdgesInfo &info, vector<CPtr<NAI::CBSPTree> > *pTrees )
{
	vector<vector<STriangle> > meshes;
	info.BuildClosedMeshes( &meshes );
	
	pTrees->clear();
	for ( int i = 0; i < meshes.size(); ++i )
		pTrees->push_back( NAI::CreateBSPTree( points, meshes[i] ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateAIGeometryInfo( int nID )
{
	try
	{
		NGScene::CResourceOpener file( "AIGeometries", nID );
		vector<CVec3> points;
		vector<STriangle> tris;
		vector<SMassSphere> spheres;
		CStoredPieceMap pieces;
		vector<CPtr<NAI::CBSPTree> > trees;

		file->Add( 1, &points );
		file->Add( 2, &tris );
		file->Add( N_PIECES_CHUNK, &pieces );
		file->Add( 6, &spheres );
		NAI::CEdgesInfo info;
		info.GenerateEdgeList( tris, points );
		ASSERT( info.IsClosed() );
		char buf[128];
		sprintf( buf, "Updating tree for ID = %d\n", nID );
		OutputDebugString(buf);

		GenerateBSPTrees( points, info, &trees );
		if ( !pieces.empty() )
		{
			CObj<NAI::CGeometryInfo> pValue = new NAI::CGeometryInfo;
			for ( CStoredPieceMap::const_iterator i = pieces.begin(); i != pieces.end(); ++i )
				pValue->AddPiece( i->first, i->second.verts, i->second.tris, i->second.fVolume, i->second.juncs );
			// Загружаем также обычную геометрию, по ней определяем какие стержни для схемы здания есть у кусков
			/*
			vector<CObj<NGScene::CObjectInfoPieces> > geometry;
			for ( int i = 0; i < NDb::N_MODEL_MATERIALS; ++i )
			{
				NGScene::CObjectInfoPiecesLoader *pGeomLoader = new NGScene::CObjectInfoPiecesLoader;
				pGeomLoader->SetKey( NGScene::SPartKey( nID, i ) );
				CDGPtr<CPtrFuncBase<NGScene::CObjectInfoPieces> > pGeom = pGeomLoader;
				pGeom.Refresh();
				geometry.push_back( pGeom->GetValue() );
			}
			*/
			vector<SPieceInfo> piecesinfo;
			for ( NAI::CGeometryInfo::CPieceMap::iterator it = pValue->pieces.begin(); it != pValue->pieces.end(); ++it )
			{
				const int nPieceID = it->first;
				int x, y, z;
				NBuilding::GetPieceCoords( nPieceID, &x, &y, &z );
				pieces[nPieceID].fVolume = NAI::CalculateObjectVolume( it->second );
				vector<STriangle> tris;
				it->second.edges.BuildTriangleList( &tris );
				GenerateBSPTrees( it->second.points, it->second.edges, &pieces[nPieceID].trees );
				CollectJunctions( &piecesinfo, &pieces[nPieceID], nPieceID );
			}
			//JunctionsPostprocess( piecesinfo, pieces );
		}
		try
		{
			char szPath[MAX_PATH];
			sprintf( szPath, "%sAIGeometries\\%d", GetExportDstDir().c_str(), nID );
			CFileStream fp;
			fp.OpenWrite( szPath );
			{
				CStructureSaver file( fp, CStructureSaver::WRITE );

				file.Add( 1, &points );
				file.Add( 2, &tris );
				file.Add( 4, &pieces );
				file.Add( 6, &spheres );
				file.Add( 8, &trees );
			}
		}
		catch ( ... )	{}
	}
	catch(...) {}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void AIGeometryUpdateDBData( CItemsMgr *pItems, const vector<int> &nItemIDs )
{
	for ( int i = 0; i < nItemIDs.size(); ++i )
	{
		const CPropMap *pProps = pItems->GetPropList( nItemIDs[i] );
		if ( !pProps )
			continue;
		CPropMap::const_iterator iVol = pProps->find( "Volume" );
		CPropMap::const_iterator e = pProps->end();
		if ( e == iVol )
			break;
		
		CPtr<NAI::CLoadGeometryInfo> pGInfo = new NAI::CLoadGeometryInfo;
		pGInfo->SetKey( nItemIDs[i] );
		CDGPtr<CPtrFuncBase<NAI::CGeometryInfo> > pFunc = pGInfo;
		pFunc.Refresh();
		NAI::CGeometryInfo *pInfo = pFunc->GetValue();
		if ( !pInfo )
			continue;
		float fVal = NAI::CalculateObjectVolume( *pInfo );
		iVol->second->SetValue( fVal );
		pItems->ReleasePropList( pProps );
		UpdateAIGeometryInfo( nItemIDs[i] );
	}	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
