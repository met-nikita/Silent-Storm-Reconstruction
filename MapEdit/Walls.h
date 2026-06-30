#ifndef __WALLS_H__
#define __WALLS_H__

#include "..\Misc\Geom.h"
#include "..\Main\BuildingInfo.h"
#include <map>

////////////////////////////////////////////////////////////////////////////////////////////////////
struct SWallKey
{
	CTPoint<int> pt;
	int nRotation;

	SWallKey() {}
	SWallKey( const CTPoint<int> &pos, int nRot ): pt( pos ), nRotation( nRot ) {}
	SWallKey( const CVec3 &pos, int nRot ): pt( pos.x, pos.y ), nRotation( nRot ) {}
	int  operator() ( const SWallKey &k ) const { return ((k.nRotation / 90) << 30) + (k.pt.y << 15) + k.pt.x; }
	bool operator==( const SWallKey &k ) const { return pt == k.pt && nRotation == k.nRotation; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SWall
{
	NBuilding::SBuildFragment fr;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlacement;
typedef unordered_map<SWallKey, SWall, SWallKey> CWallHash;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWallsPlan : public CObjectBase
{
	OBJECT_BASIC_METHODS( CWallsPlan );
		
  int     nID;
  int     nFloor;
	CWallHash wallHash;
	CWallHash::const_iterator itCurrent;

	CDGPtr< CPtrFuncBase<NBuilding::CBuildInfo> > pBuildInfo;
	const CPlacement *pPlacement;

  CWallsPlan( const CWallsPlan &fl ) {}
	void PlaceWall( int nModelID, int nLength, const CTPoint<int> &ptStart, const CTPoint<int> &ptEnd  );
	int  GetModelLen( int nModelID );

  friend class CPlacement;

public:
	CWallsPlan();
  CWallsPlan( const CPlacement *pPlacement, int nID, int nFloor );

	void Update();
	bool CopyFrom( const CWallsPlan &plan );

  int GetID() const { return nID; }
  int GetFloor() const { return nFloor; }
  const SWall* GetNearestWall( const CVec2 &pt, float fMaxSelectionDist ) const;

  void MoveFirst() const;
  bool MoveNext() const;
  const SWall* GetWall() const;
	static CPoint GetDirectionPt( const SWall *pWall );
	static CVec2  GetDirectionVec( const SWall *pWall );

  int  AddWall( int nWallModelID, const CTPoint<int> &ptStart, const CTPoint<int> &ptEnd );
	int  AddWall( int nWallModelID, const CVec2 &ptStart, const CVec2 &ptEnd );
  bool AddWalls( int nWallModelID, const vector<pair<CTPoint<int>, CTPoint<int> > > &points );
	void AddWall( const NBuilding::SBuildFragment &fragment );
	void FillWallFragments( NBuilding::CBuildInfo *pInfo ) const;
  bool DeleteWall( const SWall *pWall );
	bool Flip( const SWall *pWall );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CPoint CWallsPlan::GetDirectionPt( const SWall *pWall )
{
	switch( pWall->fr.nRotationID )
	{
		case SDiscretePos::TURN_0:
			return CPoint( 2, 0 );
		case SDiscretePos::TURN_90:
			return CPoint( 0, 2 );
		case SDiscretePos::TURN_180:
			return CPoint( -2, 0 );
		case SDiscretePos::TURN_270:
			return CPoint( 0, -2 );
	}
	return CPoint( 1, 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CVec2 CWallsPlan::GetDirectionVec( const SWall *pWall )
{
	switch( pWall->fr.nRotationID )
	{
		case SDiscretePos::TURN_0:
			return CVec2( 2, 0 );
		case SDiscretePos::TURN_90:
			return CVec2( 0, 2 );
		case SDiscretePos::TURN_180:
			return CVec2( -2, 0 );
		case SDiscretePos::TURN_270:
			return CVec2( 0, -2 );
	}
	return CVec2( 1, 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __WALLS_H__