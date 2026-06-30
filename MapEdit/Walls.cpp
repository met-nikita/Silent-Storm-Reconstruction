#include "StdAfx.h"
#include "TemplMgr.h"
#include "Placement.h"
#include "PlacementDefs.h"
#include "Walls.h"
#include "MapEdit.h"
#include "ItemsMgr.h"
#include "dbDefs.h"
#include <limits>
#include "..\Main\BuildingClip.h"
#include "..\Misc\BasicShare.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	extern CBasicShare<int, NBuilding::CBuildInfoLoader> shareBuildings;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWallsPlan::CWallsPlan()
{
	ASSERT(0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWallsPlan::CWallsPlan( const CPlacement *_pPlacement, int _nID, int _nFloor ) 
 : pPlacement( _pPlacement ), nID( _nID ), nFloor( _nFloor )
{
	pBuildInfo = NGScene::shareBuildings.Get( nID );
	Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Сброс указателя на текущий стену
// для установки на первую стенку вызвать MoveNext()
void CWallsPlan::MoveFirst() const
{
	const_cast<CWallHash::const_iterator&>( itCurrent ) = wallHash.end();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Продвижение указателя на текущую стенку в следующую позицию
// возвращает "false", если достигнут конец списка
bool CWallsPlan::MoveNext() const
{
	if ( wallHash.empty() )
		return false;
	if ( itCurrent == wallHash.end() )
	{
		const_cast<CWallHash::const_iterator&>( itCurrent ) = wallHash.begin();
		return true;
	}
	++const_cast<CWallHash::const_iterator&>( itCurrent );
	if ( itCurrent == wallHash.end() )
		return false;
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает временный указатель на текущую стенку 
// ( при вызове некоторых функций класса он становится недействительным )
// 0, если ошибка
const SWall* CWallsPlan::GetWall() const
{
	if ( wallHash.empty() || itCurrent == wallHash.end() )
		return 0;
	return &itCurrent->second;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвращает указатель на стенку, ближайшую у точке pt
// 0, если ошибка
const SWall* CWallsPlan::GetNearestWall( const CVec2 &pt, float fMaxSelectionDist ) const
{
  CWallHash::const_iterator iMin = wallHash.end();
  float fMin = 1e10;

  for( CWallHash::const_iterator it = wallHash.begin(); it != wallHash.end(); ++it )
  {
    const SWall &w = it->second;
		CVec2 ptDir = GetDirectionVec( &w );
		CVec2 ptVec = pt - CVec2( w.fr.ptPos.x, w.fr.ptPos.y );
	  float fModDist = fabs2( ptDir );
		ASSERT( fModDist > FP_EPSILON2 );

    float t = ptDir * ptVec / fModDist;
    float d;
    if ( t > 0 && t < 1  )
    {
      d = fabs( t * ptDir - ptVec );
    }
    else
    {
      d = Min( fabs( ptVec ), fabs( ptVec - ptDir ) );
    }
    if ( d < fMin )
    {
      fMin = d;
      iMin = it;
    }
  }
  if ( fMin > fMaxSelectionDist )
    return 0;
  if ( wallHash.end() != iMin )
    return &iMin->second;
  return 0;
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
// Удалить стенку
// В базе данных стенка удаляется после UpdateModified()
// если стенка с nWallID не найдена или другая ошибка, возвр. false
bool CWallsPlan::DeleteWall( const SWall *pWall )
{
	if ( !pWall )
		return false;
	CWallHash::iterator it = wallHash.find( SWallKey( pWall->fr.ptPos, pWall->fr.nRotationID ) );

  if ( wallHash.end() == it )
    return false;
	pBuildInfo.Refresh();
	NBuilding::CBuildInfo *pInfo = pBuildInfo->GetValue();
	if ( !pInfo )
		return false;
	vector<NBuilding::SBuildFragment>::iterator i = find_if( pInfo->wallFragments.begin(), pInfo->wallFragments.end(), CFrCompare( pWall->fr ) );
	if ( i != pInfo->wallFragments.end() )
		pInfo->wallFragments.erase( i );
	else
		return false;
  wallHash.erase( it );
  MoveFirst();
	return pPlacement->Save();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWallsPlan::Flip( const SWall *pW )
{
	if ( !pW )
		return false;
	SWall w = *pW; // сохраняем копию, т.к. оригинал будет удален
	DeleteWall( pW );
	CVec2 ptDir = GetDirectionVec( &w );
	w.fr.ptPos += CVec3( ptDir.x, ptDir.y, 0 );
	switch ( w.fr.nRotationID )
	{
		case SDiscretePos::TURN_0:   w.fr.nRotationID = SDiscretePos::TURN_180; break;
		case SDiscretePos::TURN_90:  w.fr.nRotationID = SDiscretePos::TURN_270; break;
		case SDiscretePos::TURN_180: w.fr.nRotationID = SDiscretePos::TURN_0;   break;
		case SDiscretePos::TURN_270: w.fr.nRotationID = SDiscretePos::TURN_90;  break;
	}
	AddWall( w.fr );
	return pPlacement->Save();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWallsPlan::CopyFrom( const CWallsPlan &plan )
{
	CWallsPlan *pPlan = const_cast<CWallsPlan*>( &plan );

	pPlan->MoveFirst();
	while ( pPlan->MoveNext() )
	{
		const SWall *pWall = plan.GetWall();
		wallHash[SWallKey( pWall->fr.ptPos, pWall->fr.nRotationID )] = *pWall;
	}
	return true;		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Дефолтная ориентация - вдоль оси Х, внутренняя сторона смотрит в сторону +У 
// ( start.y == end.y && end.x < start.x )
static void WallPosition( int nLength, const CTPoint<int> &ptStart, const CTPoint<int> &ptEnd, CVec3 *ptDelta, int *pnRotation )
{
	*pnRotation = 0;
	*ptDelta = CVec3( 0, 0, 0 );
	if ( ptStart.y == ptEnd.y )
	{
		if ( ptStart.x < ptEnd.x )
		{
			*pnRotation = SDiscretePos::TURN_180;
			ptDelta->x = nLength;
		}
		return;
	}
	if ( ptStart.y > ptEnd.y )
	{
		*pnRotation = SDiscretePos::TURN_90;
	}
	else
	{
		*pnRotation = SDiscretePos::TURN_270;
		ptDelta->y = nLength;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsPlan::PlaceWall( int nModelID, int nLength, const CTPoint<int> &ptiStart, const CTPoint<int> &ptiEnd  )
{
	CTPoint<int> wallVec = ptiEnd - ptiStart;
	const int nTiles = Float2Int( fabs( wallVec ) ) / nLength;
	CVec3 ptStart( ptiStart.x, ptiStart.y, GetFloor() );
	CVec3 ptEnd( ptiEnd.x, ptiEnd.y, GetFloor() );
	CVec3 ptDir = ptEnd - ptStart;
	ptDir /= nTiles;
	// проверяем чтобы у блоков было правильное смещение относительно начала координат
	if ( ptDir.x < 0 || ptDir.y < 0 )
	{
		swap( ptStart, ptEnd );
		ptDir.x = fabs( ptDir.x );
		ptDir.y = fabs( ptDir.y );
	}
	//
	int   nRotationID;
	CVec3 ptDelta;
	WallPosition( nLength, ptiStart, ptiEnd, &ptDelta, &nRotationID );
	ptDelta += ptStart;
	nLength /= 2; //переводим в строительные тайлы
	pBuildInfo.Refresh();
	NBuilding::CBuildInfo *pInfo = pBuildInfo->GetValue();
	if ( !pInfo )
		return;
	for ( int j = 0; j < nTiles; ++j )
	{
		SWall wall;
		
		wall.fr.nConstructionPartID = nModelID;
		wall.fr.nRotationID = nRotationID;
		CVec3 ptSPos = ptDelta + j * ptDir;
		CVec2 pd = GetDirectionVec( &wall );
		CVec3 ptNormDir( pd.x, pd.y, 0 );
		for ( int k = 0; k < nLength; ++k )
		{
			wall.fr.ptPos = ptSPos + k * ptNormDir;
			wall.fr.nSubBlockID = NBuilding::GetPartHashID( 1 + k, 1, 1 );
			wallHash[SWallKey( wall.fr.ptPos, wall.fr.nRotationID )] = wall;
			pInfo->wallFragments.push_back( wall.fr );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsPlan::AddWall( const NBuilding::SBuildFragment &fr )
{
	SWall &wall = wallHash[SWallKey( fr.ptPos, fr.nRotationID )];
	wall.fr = fr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CWallsPlan::AddWall( int nWallModelID, const CVec2 &ptStart, const CVec2 &ptEnd )
{
	return AddWall( nWallModelID, CTPoint<int>( ptStart.x, ptStart.y ), CTPoint<int>( ptEnd.x, ptEnd.y ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CWallsPlan::AddWall( int nWallModelID, const CTPoint<int> &ptStart, const CTPoint<int> &ptEnd )
{
	const int len = 2 * GetModelLen( nWallModelID );
	PlaceWall( nWallModelID, len, ptStart, ptEnd );
	if ( pPlacement )
		pPlacement->Save();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWallsPlan::AddWalls( int nWallModelID, const vector<pair<CTPoint<int>, CTPoint<int> > > &points )
{
	const int len = 2 * GetModelLen( nWallModelID );
  for ( int i = 0; i < points.size(); ++i )
		PlaceWall( nWallModelID, len, points[i].first, points[i].second );
	if ( pPlacement )
		pPlacement->Save();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsPlan::FillWallFragments( NBuilding::CBuildInfo *pInfo ) const
{
	for ( CWallHash::const_iterator it = wallHash.begin(); it != wallHash.end(); ++it )
		pInfo->wallFragments.push_back( it->second.fr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CWallsPlan::GetModelLen( int nModelID )
{
	const SResTree *pTree = theApp.GetResTree( IDC_CONSTRUCTIONPARTS_TREE );
	if ( !pTree )
		return -1;
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nModelID );
	if ( !pProps )
		return -1;
	CPropMap::const_iterator it = pProps->find( "SizeX" );
	if ( it == pProps->end() )
		return -1;
	int len = it->second->GetValue();
	pTree->pItemsTree->ReleasePropList( pProps );
	return len;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsPlan::Update()
{
	if ( !pBuildInfo.Refresh() )
		return;
	NBuilding::CBuildInfo *pInfo = pBuildInfo->GetValue();
	if ( !pInfo )
		return;
	wallHash.clear();
	MoveFirst();
	const vector<NBuilding::SBuildFragment> &frags = pInfo->wallFragments;
	for ( int i = 0; i < frags.size(); ++i )
		if ( nFloor == int( frags[i].ptPos.z ) )
			AddWall( frags[i] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
