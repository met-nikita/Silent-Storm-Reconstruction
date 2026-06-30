#ifndef __FLOOR_H__
#define __FLOOR_H__

#include "..\Main\BuildingInfo.h"
class CPlacement;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFloorPlan : public CObjectBase
{
	OBJECT_BASIC_METHODS( CFloorPlan );
public:
	struct SCell
	{
		int nRotationID;
		NBuilding::SRawMixedMaterial materials[NDb::N_CONSTRUCTION_MATERIALS];
		short x;			// координты положительные. Если х < 0  значит ячейка удалена
		short y;
	};
	typedef unordered_map<int, vector<SCell> > CCellInfo;		// <nFloorModelID, <> >
	
private:
	typedef vector<pair<int, int> > CCellMap;						// <nFloorModelID, индекс в массиве vector<SCell> >
	CCellInfo		cellInfo;
	CCellMap 		cellMap;
	int					nFloor;								// этаж
	EFloorType  ftype;									// тип этажа
	int					nLayerID;								// на одном этаже может быть несколько слоев перекрытий
	int					nWidth;
	int					nHeight;
	const CPlacement *pPlacement;
	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pBuildInfo;

	bool IsValid( const SCell &cell );
	pair<int, int>& CellAt( int x, int y );
	bool DeleteFragment( NBuilding::CBuildInfo *pInfo, const NBuilding::SBuildFragment &fr );

public:
	CFloorPlan();
	CFloorPlan( const CPlacement *pPlacement, int nFloor, EFloorType type, int nLayer );

	void Update();
	bool CopyFrom( const CFloorPlan &plan );
	void AddCells( int nFloorModelID, const vector<SCell> &cells, bool bNeedUpdate = true );
	void DeleteCells( const vector<SCell> &cells );
	const CCellInfo* GetFloor() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool operator != ( const CFloorPlan::SCell &c1, const CFloorPlan::SCell &c2 )
{
	return c1.x != c2.x || c1.y != c2.y;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __FLOOR_H__