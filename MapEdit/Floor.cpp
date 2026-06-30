#include "StdAfx.h"
#include "Placement.h"
#include "PlacementDefs.h"
#include "Floor.h"
#include "..\Misc\BasicShare.h"
#include "..\Main\MELayers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	extern CBasicShare<int, NBuilding::CBuildInfoLoader> shareBuildings;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
extern string IToA( int n );
////////////////////////////////////////////////////////////////////////////////////////////////////
CFloorPlan::CFloorPlan()
{
	ASSERT(0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CFloorPlan::CFloorPlan( const CPlacement *_pPlacement, int _nFloor, EFloorType type, int _nLayer )
	: pPlacement( _pPlacement ), nFloor(_nFloor), nLayerID(_nLayer), ftype(type)
{
	ASSERT( pPlacement );
	nWidth  = pPlacement->GetWidth();
	nHeight = pPlacement->GetHeight();
	pBuildInfo = NGScene::shareBuildings.Get( pPlacement->GetID() );
	Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CFloorPlan::CCellInfo* CFloorPlan::GetFloor() const
{
	return &cellInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool CFloorPlan::IsValid( const SCell &cell )
{
	return cell.x >= 0 && cell.x < nWidth && cell.y >= 0 && cell.y < nHeight;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline pair<int, int>& CFloorPlan::CellAt( int x, int y )
{
	return cellMap[y * nWidth + x];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFloorPlan::AddCells( int nFloorModelID, const vector<SCell> &cells, bool bNeedUpdate )
{
	pBuildInfo.Refresh();
	NBuilding::CBuildInfo *pInfo = pBuildInfo->GetValue();
	if ( !pInfo )
		return;
	const float fFloor = FT_FLOOR_INTERMEDIATE == ftype || FT_SOLID_INTERMEDIATE == ftype ? nFloor + 0.5f : nFloor;
	NBuilding::SBuildFragment fr;
	fr.nSubBlockID = 0;
	fr.nConstructionPartID = nFloorModelID;
	fr.nFragmentID = NBuilding::MakeFragmentID( ELayer( ftype ), nLayerID );

	for ( int i = 0; i < cells.size(); ++i )
	{
		if ( !IsValid( cells[i] ) )
			continue;
		pair<int, int> &cell = CellAt( cells[i].x, cells[i].y );
		// может в этой €чейке уже есть такой элемент пола ?
		if ( cell.first == nFloorModelID )
			continue;
		// если необходимо, удал€ем старый элемент
		if ( cell.first != -1 )
		{
			const SCell &c = cellInfo[cell.first][cell.second];
			if ( bNeedUpdate )
			{
				NBuilding::SBuildFragment dfr;
				dfr.nSubBlockID = 0;
				dfr.nConstructionPartID = nFloorModelID;
				dfr.nFragmentID = fr.nFragmentID;
				dfr.ptPos = CVec3( c.x, c.y, fFloor );
				dfr.nRotationID = c.nRotationID;
				DeleteFragment( pInfo, dfr );
			}
			cellInfo[cell.first][cell.second].x = -1;
		}
		// добавл€ем €чейку
		vector<SCell> &info = cellInfo[nFloorModelID];
		cell.first  = nFloorModelID;
		cell.second = info.size();
		info.push_back( cells[i] );
		if ( bNeedUpdate )
		{
			if ( FT_ROOM != ftype )
			{
				fr.ptPos = CVec3( cells[i].x, cells[i].y, fFloor );
				fr.nRotationID = cells[i].nRotationID;
				pInfo->solidFragments.push_back( fr );
			}
		}
	}
	// «аписываем изменени€ в базу данных
	if ( bNeedUpdate )
	{
		pPlacement->Save();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFrCompare
{
public:
	const NBuilding::SBuildFragment &fr;

	CFrCompare( const NBuilding::SBuildFragment &_fr ): fr(_fr) {}
	boolean operator()( const NBuilding::SBuildFragment &a )
	{
		return fr == a;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFloorPlan::DeleteFragment( NBuilding::CBuildInfo *pInfo, const NBuilding::SBuildFragment &fr )
{
	if ( FT_ROOM == ftype )
		return true;
	vector<NBuilding::SBuildFragment>::iterator i = find_if( pInfo->solidFragments.begin(), pInfo->solidFragments.end(), CFrCompare(fr) );
	if ( i != pInfo->solidFragments.end() )
		pInfo->solidFragments.erase( i );
	else
		return false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFloorPlan::DeleteCells( const vector<SCell> &cells )
{
	pBuildInfo.Refresh();
	NBuilding::CBuildInfo *pInfo = pBuildInfo->GetValue();
	if ( !pInfo )
		return;

	NBuilding::SBuildFragment fr;
	fr.nSubBlockID = 0;
	fr.nFragmentID = NBuilding::MakeFragmentID( ELayer( ftype ), nLayerID );
	const float fFloor = FT_FLOOR_INTERMEDIATE == ftype || FT_SOLID_INTERMEDIATE == ftype ? nFloor + 0.5f : nFloor;

	for ( int i = 0; i < cells.size(); ++i )
	{
		if ( !IsValid( cells[i] ) )
			continue;
		pair<int, int> &cell = CellAt( cells[i].x, cells[i].y );
		// может €чейка уже пуста?
		if ( -1 == cell.first )
			continue;
		const SCell &c = cellInfo[cell.first][cell.second];
		fr.nConstructionPartID = cell.first;
		fr.ptPos = CVec3( c.x, c.y, fFloor );
		fr.nRotationID = c.nRotationID;
		DeleteFragment( pInfo, fr );

		cellInfo[cell.first][cell.second].x = -1;
		cell.first = -1;
	}
	// «аписываем изменени€
	if ( pPlacement )
		pPlacement->Save();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFloorPlan::CopyFrom( const CFloorPlan &plan )
{
	if ( nWidth != plan.nWidth || nHeight != plan.nHeight )
		return false;
#ifdef _DEBUG
	int nSrcInfoSize = plan.cellInfo.size();
	int nDstInfoSize = cellInfo.size();
#endif
	cellInfo = plan.cellInfo;
	cellMap = plan.cellMap;
#ifdef _DEBUG
	nSrcInfoSize = plan.cellInfo.size();
	nDstInfoSize = cellInfo.size();
#endif
	
	return	pPlacement->Save();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFloorPlan::Update()
{
	if ( !pBuildInfo.Refresh() )
		return;
	NBuilding::CBuildInfo *pInfo = pBuildInfo->GetValue();
	if ( !pInfo )
		return;
	cellMap.clear();
	cellInfo.clear();
	cellMap.resize( nWidth * nHeight );
	cellMap.assign( cellMap.size(), pair<int, int>( -1, 0 ) );
	if ( ftype < FT_ROOM )
	{
		const vector<NBuilding::SBuildFragment> &frags = pInfo->solidFragments;
		vector<SCell> cells(1);
		for ( int i = 0; i < frags.size(); ++i )
		{
			const NBuilding::SBuildFragment &fr = frags[i];
			int nLayer;
			EFloorType type;
			NBuilding::GetLayerID( fr.nFragmentID, (ELayer*)&type, &nLayer );
			if ( type != ftype || nFloor != (int)fr.ptPos.z || nLayer != nLayerID )
				continue;
			cells[0].x = fr.ptPos.x;
			cells[0].y = fr.ptPos.y;
			cells[0].nRotationID = fr.nRotationID;
			for ( int j = 0; j < NDb::N_CONSTRUCTION_MATERIALS; ++j )
				cells[0].materials[j] = fr.materials[j];
			AddCells( fr.nConstructionPartID, cells, false );
		}
		return;
	}
	const int nRIndex = nFloor - pInfo->nMinFloor;
	if ( nRIndex < 0 || nRIndex >= pInfo->roomMap.size() )
		return;
	vector<SCell> cells(1);
	const CArray2D<BYTE> &rooms = pInfo->roomMap[nRIndex];
	// по кра€м rooms добавлена полоса в 1 тайл
	for ( int j = 1; j < rooms.GetYSize(); ++j )
		for ( int i = 1; i < rooms.GetXSize(); ++i )
		{
			const int nRoomID = rooms[j][i];
			if ( nRoomID <= 0 )
				continue;
			cells[0].x = i - 1;
			cells[0].y = j - 1;
			AddCells( nRoomID, cells, false );
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
