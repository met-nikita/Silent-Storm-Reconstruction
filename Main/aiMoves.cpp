#include "StdAfx.h"
#include "aiMoves.h"
#include "aiGrid.h"
#include "Grid.h"
#include "rpgUnitInfo.h"
namespace NAI
{
//
////////////////////////////////////////////////////////////////////////////////////////////////////
static EMoveType GetClimbMoveTypeByHeight( float fHDiff )
{
	if ( fHDiff < 1.0f )
		return MT_CLIMB_1;
	if ( fHDiff < 1.5f )
		return MT_CLIMB_2;
	if ( fHDiff < 2.0f )
		return MT_CLIMB_3;
	return MT_CLIMB_4;
}
static ETransitionType GetClimbTransTypeByHeight( float fHDiff )
{
	if ( fHDiff < 1.0f )
		return TT_CLIMB_1;
	if ( fHDiff < 1.5f )
		return TT_CLIMB_2;
	if ( fHDiff < 2.0f )
		return TT_CLIMB_3;
	return TT_CLIMB_4;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static ETransitionType GetLadderTransitionType( 
	const vector<CObj<CNodesLayer> > &layers, const SPathPlace &p1, const SPathPlace &p2 )
{
	int nLadder = p1.GetX();
	CNodesLayer *pLayer = layers[p1.GetLayer()];
	if ( p2.IsIntegral() )
	{
		const CNodesLayer::SLadder &ladder = pLayer->ladders[ nLadder ];
		if ( p1.GetLadderStep() == 0 ) // lowest point
		{
			const SPathPlace &low = ladder.placeOnBottom;
			if ( p2.GetLayer() == low.GetLayer() && 
					p2.GetX() == low.GetX() && 
					p2.GetY() == low.GetY() )
					return TT_LADDER_DOWN;
			return TT_NO_WAY;
		}
		if ( p1.GetLadderStep() == ladder.GetHeight() - 4 ) // upper point
		{
			const SPathPlace &top = ladder.placeOnTop;
			if ( p2.GetLayer() == top.GetLayer() && 
					p2.GetX() == top.GetX() && 
					p2.GetY() == top.GetY() )
					return TT_LADDER_UP;
		}
		return TT_NO_WAY;
	}
	// ���������, � �� �� ��� ���� ��������...
	if ( p1.GetX() != p2.GetX() )
		return TT_NO_WAY;
	if ( p1.GetLayer() != p2.GetLayer() )
		return TT_NO_WAY;
	// ����� ��, � �� ������� �� �� ������ ���������?
	if ( p1.GetLadderStep() == p2.GetLadderStep() + 1 )
		return TT_LADDER_MOVE;
	if ( p1.GetLadderStep() == p2.GetLadderStep() - 1 )
		return TT_LADDER_MOVE;
	if ( p1.GetLadderStep() == p2.GetLadderStep() )
		return TT_SAME;
	return TT_NO_WAY;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool HasSame( CPathNetwork *pNet, const SPathPlace &src, int nLayer )
{
	CNodesLayer *pSrcL = pNet->GetLayer( src.GetLayer() );
	CNodesLayer *pDstL = pNet->GetLayer( nLayer );
	if ( pSrcL->pGroup != pDstL->pGroup )
		return false;
	CNodesLayer::STile &t1 = pSrcL->tiles[ src.GetY() ][ src.GetX() ];
	CNodesLayer::STile &t2 = pDstL->tiles[ src.GetY() ][ src.GetX() ];
	return ( t1.nHeight == t2.nHeight && t1.nDisplacement == t2.nDisplacement );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SetOnBaseLayer( CPathNetwork *pNet, SPathPlace *pSrc )
{
	CNodesLayer *pSrcL = pNet->GetLayer( pSrc->GetLayer() );
	if ( !pSrcL )
		return;
	int nFloor = pSrcL->nFloor + Max( 0, - pSrcL->pGroup->nFirstFloor );
	CNodesLayer *pBaseL = pSrcL->pGroup->layers[ nFloor * N_MAX_LAYERS_PER_FLOOR ];
	if ( !pBaseL )
		return;
	pSrc->SetOnLayer( pBaseL->nLayer, pSrc->GetX(), pSrc->GetY(), pSrc->IsIntegral() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsSame( CPathNetwork *pNet, const SPathPlace &src, const SPathPlace &dst )
{
	if ( src.GetX() != dst.GetX() )
		return false;
	if ( src.GetY() != dst.GetY() )
		return false;
	CNodesLayer *pSrcL = pNet->GetLayer( src.GetLayer() );
	CNodesLayer *pDstL = pNet->GetLayer( dst.GetLayer() );
	if ( pSrcL->pGroup != pDstL->pGroup )
		return false;
	CNodesLayer::STile &t1 = pSrcL->tiles[ src.GetY() ][ src.GetX() ];
	CNodesLayer::STile &t2 = pDstL->tiles[ src.GetY() ][ src.GetX() ];
	return ( t1.nHeight == t2.nHeight && t1.nDisplacement == t2.nDisplacement );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
ETransitionType GetTransitionType( const IPathNetwork *_pNet, const SPathPlace &src, const SPathPlace &dst )
{
	// Release (GetTransitionType @0x77df0) early-outs when either endpoint is a "final" place
	// (SPathPlace::nFinal, bit 0x200) BEFORE any layer access. A final place carries no grid cell --
	// its nX/nY/nLayer are the -1 sentinel from the default ctor -- so indexing layers[GetLayer()]
	// (GetLayer()==0xff) reads past the vector and dereferences a garbage CNodesLayer::pGroup => crash
	// (observed in combat pathfinding: pSrcL->pGroup == 0x390036). Two final endpoints at the same
	// cell/pose are TT_SAME, or TT_TURN when only the facing differs; otherwise there is no transition.
	if ( src.IsFinal() || dst.IsFinal() )
	{
		if ( src.GetX() == dst.GetX() && src.GetY() == dst.GetY() &&
			 src.GetLayer() == dst.GetLayer() && src.GetPose() == dst.GetPose() )
			return ( src.GetDirection() != dst.GetDirection() ) ? TT_TURN : TT_SAME;
		return TT_NO_WAY;
	}
	CDynamicCast<CPathNetwork> pNet( _pNet );
	const vector<CObj<CNodesLayer> > &layers = pNet->GetLayers();
	IAIMap *pMap = pNet->GetAIMap();
	CNodesLayer *pLayer = layers[src.GetLayer()];
	pLayer->pGroup->RefreshSpot( src, pMap, 2 );
	if ( !src.IsIntegral() )
		return GetLadderTransitionType( layers, src, dst );
	if ( !dst.IsIntegral() )
		return GetLadderTransitionType( layers, dst, src );

	if ( src.GetLayer() != dst.GetLayer() )
	{
		SPathPlace s( src.GetX(), src.GetY(), src.GetLayer() );
		SPathPlace d( dst.GetX(), dst.GetY(), dst.GetLayer() );
		CNodesLayer *pDstLayer = layers[dst.GetLayer()];
		if ( HasSame( pNet, s, d.GetLayer() ) )
		{
			SPathPlace test( src );
			test.SetOnLayer( dst.GetLayer(), src.GetX(), src.GetY(), src.IsIntegral() );
			test.SetPose( src.GetPose() );
			if ( test == d )
				return TT_INTERGRID_SAME;
			ETransitionType tt = GetTransitionType( _pNet, test, dst );
			if ( tt != TT_NO_WAY )
				return tt;
		}
		if ( HasSame( pNet, d, s.GetLayer() ) )
		{
			SPathPlace test( dst );
			test.SetOnLayer( src.GetLayer(), dst.GetX(), dst.GetY(), dst.IsIntegral() );
			test.SetPose( dst.GetPose() );
			if ( test == s )
				return TT_INTERGRID_SAME;
			ETransitionType tt = GetTransitionType( _pNet, src, test );
			if ( tt != TT_NO_WAY )
				return tt;
		}
		// maybe it is intergrid?
		SetOnBaseLayer( pNet, &s );
		pLayer = layers[s.GetLayer()];
		SetOnBaseLayer( pNet, &d );
		pDstLayer = layers[d.GetLayer()];
		CNodesLayer::CTransitionsHash::const_iterator k = pLayer->transitions.find( s );
		CNodesLayer::CTransitionsHash::const_iterator kDst = pDstLayer->transitions.find( d );
		if ( k != pLayer->transitions.end() )
		{
			//ASSERT( kDst != pDstLayer->transitions.end() );
			const CNodesLayer::STransitionSet &trSet = k->second;
			if ( kDst != pDstLayer->transitions.end() )
			{
				const CNodesLayer::STransitionSet &dstSet = kDst->second;
				for ( list<CNodesLayer::SLink>::const_iterator l = trSet.links.begin(); l != trSet.links.end(); ++l )
				{
					if ( l->dst == d || IsSame( pNet, d, l->dst ) )//l->dst == d )
						return TT_INTERGRID;
				}
			}
			else
			{
				for ( list<CNodesLayer::SLink>::const_iterator l = trSet.links.begin(); l != trSet.links.end(); ++l )
				{
					if ( l->dst == d )
						return TT_INTERGRID;
				}
			}
		}
		return TT_NO_WAY;
	}
	if ( abs(src.GetX() - dst.GetX()) > 1 || abs(src.GetY() - dst.GetY()) > 1 )
		return TT_NO_WAY;
	if ( src.GetPose() == CM_INACTIVE && src.GetDirection() != dst.GetDirection() )
		return TT_NO_WAY;
	if ( src.GetX() == dst.GetX() && src.GetY() == dst.GetY() )
	{
		if ( src.GetPose() != dst.GetPose() )
			return TT_POSE;
		if ( src.GetDirection() != dst.GetDirection() )
			return TT_TURN;
		return TT_SAME;
	}
	int i = pNet->GetDir( src, dst );

	CNodesLayer::STile &t = pLayer->tiles[src.GetY()][src.GetX()];
	if ( t.nMoveHC & (1<<i) )
	{
		CNodesLayer *pLayer1 = layers[dst.GetLayer()];
		pLayer->pGroup->RefreshSpot( dst, pMap, 1 );
		CNodesLayer::STile &tDst = pLayer1->tiles[dst.GetY()][dst.GetX()];
		float fHDiff = GetFHeight(tDst.nHeight) - GetFHeight(t.nHeight);
		if ( fHDiff > 0 )
			return GetClimbTransTypeByHeight( fHDiff );
		else
			// @0x77df0 -- a downward high-clearance step is a JUMP; if the unit FACES away from the move
			// direction (i = GetDir), it is a JUMP_BACK (dropping down backwards). TT_JUMP_BACK == TT_JUMP+1.
			return (ETransitionType)( TT_JUMP + ( src.GetDirection() != i ) );
	}
	if ( (t.nMoveStand | t.nMoveCrouch | t.nMoveLay) & (1<<i) )
	{
		if ( src.GetX() == dst.GetX() || src.GetY() == dst.GetY() )
			return TT_MOVE;
		return TT_MOVE_DIAGONAL;
	}
	return TT_NO_WAY;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void DebugForEach( const SPathPlace	&src, const SMove &m, CPathNetwork *pNet )
{
	float fH1 = pNet->GetCP( src ).z;
	float fH2 = pNet->GetCP( m.dest ).z;
	if ( fH1 < fH2 - 3 || fH1 > fH2 + 3 )
		__debugbreak();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline static bool IsGoodPt( const NAI::SPathPlace &p, CPathNetwork *pNet )
{
	return !pNet->IsLocked( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetNonStandartMoves() is different than GetMoves() in such aspects:
// 1) All stand poses with different directions and moving/nonmoving only are identical, so
//	1a) turn moves when we stand are not returned
//	1b) moves if source pose == CM_STAND are returned for all directions
//	1c) moves with dest pose == CM_STAND are returned with final direction = 0, and with "not moving" flag
// 2) All crouch and inactive poses with different directions and moving/nonmoving are identical
void GetNonStandartMoves( 
		IPathNetwork *_pNet, const SPathPlace &src, bool bCheckSuicide, bool bMoveOnly, 
		vector<SMove> *pRes, int *pMovesCount, vector<char> *pDynLocks )
{
	CDynamicCast<CPathNetwork> pNet( _pNet );
	const vector<CObj<CNodesLayer> > &layers = pNet->GetLayers();
	vector<char> &dynLocks = *pDynLocks;
	IAIMap *pMap = pNet->GetAIMap();
	SMove m;
	int pResPos = 0;
	vector<SMove> &res = *pRes;

	ASSERT( src.GetLayer() < layers.size() );
	int nX = src.GetX(), nY = src.GetY();
	CNodesLayer *pLayer = layers[ src.GetLayer() ];
	if ( src.IsIntegral() )
		pLayer->pGroup->RefreshSpot( src, pMap, 2 );
	if ( !src.IsIntegral() )  // we are on ladder
	{
		//OutputDebugString("[ LADDER ]�� �� ��������...\n");
		CNodesLayer::SLadder &ladder = pLayer->ladders[ nX ];
		int nLastStep = ladder.GetHeight() - 4;
		if ( nY < nLastStep )
		{
			// move up
			if ( ladder.pointPassable[ nY + 1 ] && ladder.nLocks[ nY + 1 ] == 0 )
			{
				m.type = MT_LADDER_MOVE;
				m.dest = src;
				m.dest.SetY( src.GetY() + 1 );
				if ( IsGoodPt( m.dest, pNet ) )
				{
					res[ pResPos ] = m;
					dynLocks[ pResPos ] = 0;
					++pResPos;
				}
			}
		}
		else
		{
			// �������� �������� ������
			//OutputDebugString("[ LADDER ]Can try leave ladder from up\n");
			CNodesLayer *pLeaveLayer = pNet->GetLayer( ladder.placeOnTop.GetLayer() );
			CNodesLayer::STile &t = pLeaveLayer->tiles[ladder.placeOnTop.GetY()][ladder.placeOnTop.GetX()];
			if ( ( t.nPassable & CP_INACTIVE ) && t.nLocks == 0 )
			{
				//OutputDebugString("[ LADDER ]Can leave ladder from up\n");
				m.type = MT_LADDER_UP;
				m.dest = ladder.placeOnTop;
				m.dest.SetPose( CM_INACTIVE );
				m.dest.SetDirection( src.GetDirection() );
				m.dest.SetMoving( 0 );
				if ( IsGoodPt( m.dest, pNet ) )
				{
					res[ pResPos ] = m;
					dynLocks[ pResPos ] = t.nDynLocks;
					++pResPos;
				}
			}
		}
		if ( nY > 0 )
		{
			// move down
			if ( ladder.pointPassable[ nY - 1 ] && ladder.nLocks[ nY - 1 ] == 0 )
			{
				m.type = MT_LADDER_MOVE;
				m.dest = src;
				m.dest.SetY( src.GetY() - 1 );
				if ( IsGoodPt( m.dest, pNet ) )
				{
					res[ pResPos ] = m; 
					dynLocks[ pResPos ] = 0;
					++pResPos;
				}
			}
		}
		else
		{
			// �������� �������� �����
			CNodesLayer *pLeaveLayer = pNet->GetLayer( ladder.placeOnBottom.GetLayer() );
			CNodesLayer::STile &t = pLeaveLayer->tiles[ladder.placeOnBottom.GetY()][ladder.placeOnBottom.GetX()];
			if ( ( t.nPassable & CP_STAND ) && t.nLocks == 0 )
			{
				m.type = MT_LADDER_DOWN;
				m.dest = ladder.placeOnBottom;
				m.dest.SetPose( CM_STAND );
				m.dest.SetDirection( 0 );
				m.dest.SetMoving( 0 );
				if ( IsGoodPt( m.dest, pNet ) )
				{
					res[ pResPos ] = m;
					dynLocks[ pResPos ] = t.nDynLocks;
					++pResPos;
				}
			}
		}
		*pMovesCount = pResPos;
		return;
	}

	CNodesLayer::STile &t = pLayer->tiles[nY][nX];
	// moves
	int nDirFrom = src.GetDirection(), nDirTo = (src.GetDirection() + 1) & 7;
	
	switch( src.GetPose() )
	{
		case CM_LAY:
			if ( src.IsMoving() )
			{
				nDirFrom = (src.GetDirection() + 7) & 7;
				nDirTo = (src.GetDirection() + 2) & 7;
			}
			for ( int i = nDirFrom; i != nDirTo; i = (i+1) & 7 )
			{
				if ( t.nMoveLay & (1<<i) )
				{
					int nX1 = nX + nMoveShift[i][0];
					int nY1 = nY + nMoveShift[i][1];
					SPathPlace layPlace( nX1, nY1, src.GetLayer(), i, CM_LAY, 0 );
					if ( pNet->IsPassable( layPlace ) )
					{
						m.dest = layPlace;
						m.type = (i&1) ? MT_MOVE_CRAWL_DIAG : MT_MOVE_CRAWL;
						if ( IsGoodPt( m.dest, pNet ) )
						{
							res[ pResPos ] = m;
							dynLocks[ pResPos ] = t.nDynLocks;
							++pResPos;
						}
					}
				}
			}
			break;

		case CM_CROUCH:
			if ( !bMoveOnly )
			{
				// climbing
				for ( int i = 0; i < 8; ++i )
				{
					// approach to edge for jumping
					if ( (i & 1) == 0 && (t.nMoveCrouch & (1<<i)) ) // non-diagonal
					{
						bool bCanMoveToInactive = false;
						int nX1 = nX + nMoveShift[i][0];
						int nY1 = nY + nMoveShift[i][1];
						CNodesLayer::STile &t1 = pLayer->tiles[nY1][nX1];
						float fFirstHDiff = GetFHeight(t.nHeight) - GetFHeight(t1.nHeight);
						if ( ( t1.nMoveHC & (1<<i) ) && ( fabs( fFirstHDiff ) < F_MIN_CLIMB_HEIGHT ) )
							bCanMoveToInactive = true;
						// find ladder
						SPathPlace search( nX1, nY1, src.GetLayer()	);
						if ( pLayer->ladderEntrances.find( search ) != pLayer->ladderEntrances.end() )
						{
							const CNodesLayer::SLadderTransition &tr = pLayer->ladderEntrances[ search ];
							if ( tr.bUpper )
							{
								int nL = tr.nLayerGroup;
								CNodesLayer *pLadderLayer = pNet->GetLayer( nL );
								CNodesLayer::SLadder &ladder = pLadderLayer->ladders[ tr.nLadder ];
								if ( ladder.pointPassable[ 0 ] && ladder.nLocks[ 0 ] == 0 )
								{
									bCanMoveToInactive = true;
								}
							}
						}
						if ( bCanMoveToInactive )
						{
							int nX2 = nX1 + nMoveShift[i][0];
							int nY2 = nY1 + nMoveShift[i][1];
							CNodesLayer::STile &t2 = pLayer->tiles[nY2][nX2];
							float fHDiff = GetFHeight(t2.nHeight) - GetFHeight(t1.nHeight);
							if ( fHDiff < 0 )
							{
								m.dest = SPathPlace( nX1, nY1, src.GetLayer(), i, CM_INACTIVE, 0 );
								m.type = MT_MOVE_CROUCH;
								if ( IsGoodPt( m.dest, pNet ) )
								{
									res[ pResPos ] = m;
									dynLocks[ pResPos ] = t.nDynLocks;
									++pResPos;
								}
							}
						}
					}
				}
			}
			for ( int i = 0; i < 8; ++i )
			{
				if ( t.nMoveCrouch & (1<<i) )
				{
					int nX1 = nX + nMoveShift[i][0];
					int nY1 = nY + nMoveShift[i][1];
					CNodesLayer::STile &t1 = pLayer->tiles[nY1][nX1];
					if ( t1.nPassable & CP_CROUCH )
					{
						m.dest = SPathPlace( nX1, nY1, src.GetLayer(), 0, CM_CROUCH, 0 );
						m.type = (i&1) ? MT_MOVE_CROUCH_DIAG : MT_MOVE_CROUCH;
						if ( IsGoodPt( m.dest, pNet ) )
						{
							res[ pResPos ] = m;
							dynLocks[ pResPos ] = t.nDynLocks;
							++pResPos;
						}
					}
				}
			}
			break;

		case CM_STAND:
			if ( !bMoveOnly )
			{
				// climbing -- retail @0x78300 SIMPLIFIED the Jan03 nested variant: the climb onto a
				// special point is emitted whenever the point is higher by (0, F_MAX_CLIMB_HEIGHT]
				// and CP_INACTIVE. Jan03 additionally demanded a usable continuation BEYOND the edge
				// (t1.nMoveCrouch/t1.nMoveHC toward t2 + t2 pose bits), which rejected legitimate
				// climb spots (edges whose far tile doesn't cooperate) and with it the whole
				// climb-over / climb-down chain through them. Retail decomp is authoritative.
				for ( int i = 0; i < 8; ++i )
				{
					if ( ( t.nMoveHC & (1<<i) ) == 0 )
						continue;
					int nX1 = nX + nMoveShift[i][0];
					int nY1 = nY + nMoveShift[i][1];
					CNodesLayer::STile &t1 = pLayer->tiles[nY1][nX1];
					float fHDiff = GetFHeight(t1.nHeight) - GetFHeight(t.nHeight);
					if ( fHDiff > 0 && fHDiff <= F_MAX_CLIMB_HEIGHT && (t1.nPassable & CP_INACTIVE) )
					{
						m.dest = SPathPlace( nX1, nY1, src.GetLayer(), i, CM_INACTIVE, 0 );
						m.type = GetClimbMoveTypeByHeight( fHDiff );
						if ( IsGoodPt( m.dest, pNet ) )
						{
							res[ pResPos ] = m;
							dynLocks[ pResPos ] = t.nDynLocks;
							//DebugForEach( src, m, pNet );
							++pResPos;
						}
					}
				}
			}
			for ( int i = 0; i < 8; ++i )
			{
				if ( t.nMoveStand & (1<<i) )
				{
					m.dest = SPathPlace( nX + nMoveShift[i][0], nY + nMoveShift[i][1], src.GetLayer(), 0, src.GetPose(), 0 );
					m.type = (i&1) ? MT_MOVE_STAND_DIAG : MT_MOVE_STAND;
					if ( IsGoodPt( m.dest, pNet ) )
					{
						res[ pResPos ] = m;
						dynLocks[ pResPos ] = t.nDynLocks;
						//DebugForEach( src, m, pNet );
						++pResPos;
					}
				}
			}
			break;

		case CM_INACTIVE:
			if ( !bMoveOnly )
			{
				int i = src.GetDirection();
				if ( t.nMoveCrouch & (1<<i) )
				{
					int nX1 = nX + nMoveShift[i][0];
					int nY1 = nY + nMoveShift[i][1];
					CNodesLayer::STile &t1 = pLayer->tiles[nY1][nX1];
					if ( t1.nPassable & CP_CROUCH )
					{
						//int i2 = ( i + 4 ) & 7;
						//if ( ( t1.nMoveLay & (1<<i2) ) == 0 )
						//	__debugbreak();
						m.dest = SPathPlace( nX1, nY1, src.GetLayer(), 0, CM_CROUCH, 0 );
						m.type = MT_MOVE_CROUCH;
						if ( IsGoodPt( m.dest, pNet ) )
						{
							res[ pResPos ] = m;
							dynLocks[ pResPos ] = t.nDynLocks;
							++pResPos;
						}
					}
				}
				else if ( t.nMoveHC & (1<<i) )
				{
					int nX1 = nX + nMoveShift[i][0];
					int nY1 = nY + nMoveShift[i][1];
					CNodesLayer::STile &t1 = pLayer->tiles[nY1][nX1];
					float fHDiff = GetFHeight(t1.nHeight) - GetFHeight(t.nHeight);
					if ( fHDiff < 0 && (t1.nPassable & CP_CROUCH) )
					{
						if ( bCheckSuicide || fHDiff > -F_MAX_CLIMB_HEIGHT )
						{
							m.dest = SPathPlace( nX1, nY1, src.GetLayer(), 0, CM_CROUCH, 0 );
							m.type = MT_JUMP;
							if ( IsGoodPt( m.dest, pNet ) )
							{
								res[ pResPos ] = m;
								dynLocks[ pResPos ] = t.nDynLocks;
								++pResPos;
							}
						}
					}
				}
				// retail @0x78300 ADDITION (NAI::TryToJump @0x77d00 via the back direction): from a
				// special climb point also try the jump-down BEHIND the facing -- a unit that climbed
				// onto an edge (CM_INACTIVE faces the climb direction) descends the far side by
				// jumping back. Jan03 only ever jumped forward, so the wave could mount an edge from
				// the high side yet never come down the low side (climb-down over a ledge).
				{
					int nBack = ( i + 4 ) & 7;
					if ( t.nMoveHC & (1<<nBack) )
					{
						int nX1 = nX + nMoveShift[nBack][0];
						int nY1 = nY + nMoveShift[nBack][1];
						CNodesLayer::STile &t1 = pLayer->tiles[nY1][nX1];
						float fHDiff = GetFHeight(t1.nHeight) - GetFHeight(t.nHeight);
						if ( fHDiff < 0 && (t1.nPassable & CP_CROUCH) )
						{
							if ( bCheckSuicide || fHDiff > -F_MAX_CLIMB_HEIGHT )
							{
								m.dest = SPathPlace( nX1, nY1, src.GetLayer(), 0, CM_CROUCH, 0 );
								m.type = MT_JUMP;
								if ( IsGoodPt( m.dest, pNet ) )
								{
									res[ pResPos ] = m;
									dynLocks[ pResPos ] = t.nDynLocks;
									++pResPos;
								}
							}
						}
					}
				}
				SPathPlace search( src.GetX(), src.GetY(), src.GetLayer() );
				CNodesLayer::CLadderHash::iterator where = pLayer->ladderEntrances.find( search );
				if ( where != pLayer->ladderEntrances.end() ) // there is a ladder
				{
					if ( where->second.bUpper ) 
					{
						//OutputDebugString("[ LADDER ] Can try go on ladder from up\n");
						int nL = where->second.nLayerGroup;
						CNodesLayer *pLadderLayer = pNet->GetLayer( nL );
						CNodesLayer::SLadder &ladder = pLadderLayer->ladders[ where->second.nLadder ];
						int nHeight = ladder.GetHeight() - 4;
						if ( ladder.pointPassable[ nHeight ] && ladder.nLocks[ nHeight ] == 0 && ladder.eDir * 2 == src.GetDirection() )
						{
							//OutputDebugString("[ LADDER ] Can go on ladder from up\n");
							m.dest.SetOnLayer( nL, where->second.nLadder, nHeight, 0 );
							int nDirection = ( ladder.eDir * 2 + 4 ) & 7;
							m.dest.SetDirection( nDirection );
							m.dest.SetPose( CM_STAND );
							m.type = MT_LADDER_UP;
							if ( IsGoodPt( m.dest, pNet ) )
							{
								res[ pResPos ] = m;
								dynLocks[ pResPos ] = t.nDynLocks;
								++pResPos;
							}
						}
					}
				}
			}
			break;
	}
	if ( src.GetPose() != CM_INACTIVE )
	{
		//go on the ladder
		if ( src.GetPose() == CM_STAND && !bMoveOnly )
		{
				SPathPlace search( src.GetX(), src.GetY(), src.GetLayer() );
				CNodesLayer::CLadderHash::iterator where = pLayer->ladderEntrances.find( search );
				if ( where != pLayer->ladderEntrances.end() ) // there is a ladder
				{
					if ( !where->second.bUpper ) 
					{
						//OutputDebugString("[ LADDER ]Can try go on ladder\n");
						int nL = where->second.nLayerGroup;
						CNodesLayer *pLadderLayer = pNet->GetLayer( nL );
						CNodesLayer::SLadder &ladder = pLayer->ladders[ where->second.nLadder ];
						if ( ladder.pointPassable[ 0 ] && ( ladder.nLocks[ 0 ] == 0 ) )
						{
							//OutputDebugString("[ LADDER ]Can go on ladder\n");
							m.dest.SetOnLayer( nL, where->second.nLadder, 0, 0 );
							int nDirection = ( ladder.eDir * 2 + 4 ) & 7;
							m.dest.SetDirection( nDirection );
							m.dest.SetPose( CM_STAND );
							m.type = MT_LADDER_DOWN;
							if ( IsGoodPt( m.dest, pNet ) )
							{
								res[ pResPos ] = m;
								dynLocks[ pResPos ] = t.nDynLocks;
								++pResPos;
							}
							//else
								//OutputDebugString("[ LADDER ]�� ��� ������-�� ���� ��������\n");
						}
					}
				}
		}
		// rotate, lay
		if ( src.GetPose() == CM_LAY ) 
		{
			/*int dir = 0;
			for ( dir = 0; dir < 8; ++dir )
			{
				if ( dir == src.GetDirection() )
					continue;
				SPathPlace layPlace( src.GetX(), src.GetY(), src.GetLayer(), dir, src.GetPose(), 0 );
				if ( pNet->IsPassable( layPlace ) )
				{
					m.dest = layPlace;
					m.type = MT_TURN;
					res[ pResPos ] = m;
					dynLocks[ pResPos ] = t.nDynLocks;
					++pResPos;
				}
			}*/

			// rotate left
			int leftdir = (src.GetDirection() + 1) & 7;
			SPathPlace layPlaceLeft( src.GetX(), src.GetY(), src.GetLayer(), leftdir, src.GetPose(), 0 );
			while ( pNet->IsPassable( layPlaceLeft ) )
			{
				m.dest = layPlaceLeft;
				m.type = MT_TURN;
				res[ pResPos ] = m;
				dynLocks[ pResPos ] = t.nDynLocks;
				++pResPos;
				leftdir = ( leftdir + 1 ) & 7;
				if ( leftdir == src.GetDirection() )
					break;
				layPlaceLeft.SetDirection( leftdir );
			}
			// rotate right
			int rightdir = src.GetDirection() - 1;
			if ( rightdir == -1 )
				rightdir = 7;
			SPathPlace layPlaceRight( src.GetX(), src.GetY(), src.GetLayer(), leftdir, src.GetPose(), 0 );
			while ( pNet->IsPassable( layPlaceRight ) )
			{
				m.dest = layPlaceRight;
				m.type = MT_TURN;
				res[ pResPos ] = m; 
				dynLocks[ pResPos ] = t.nDynLocks;
				++pResPos;
				rightdir--;
				if ( rightdir == -1 )
					rightdir = 7;
				if ( leftdir == rightdir )
					break;
				layPlaceRight.SetDirection( rightdir );
			}
		}
		if ( !bMoveOnly )
		{
			// stand and crouch -> lay
			if ( src.GetPose() == CM_STAND || src.GetPose() == CM_CROUCH )
			{
				if ( src.GetPose() == CM_STAND )
					m.type = MT_POSE_WALK_CRAWL;
				else 
					m.type = MT_POSE_CROUCH_CRAWL;
				for ( int i = 0; i < 8; ++i )
					if ( t.nPassable & (CP_LAY1 << (i & 3)) )
					{
						m.dest = SPathPlace( src.GetX(), src.GetY(), src.GetLayer(), i, CM_LAY, 0 );
						if ( pNet->IsPassable( m.dest ) )
						{
					 		res[ pResPos ] = m;
							dynLocks[ pResPos ] = 0;
							++pResPos;
						}
					}
			}
			// stand -> crouch
			if ( src.GetPose() == CM_STAND )
			{
				if ( t.nPassable & CP_CROUCH )
					{
						m.dest = SPathPlace( src.GetX(), src.GetY(), src.GetLayer(), 0, CM_CROUCH, 0 );
						m.type = MT_POSE_WALK_CROUCH;
						res[ pResPos ] = m; 
						dynLocks[ pResPos ] = 0;
						++pResPos;
					}
			}
			// crouch -> stand 
			if ( src.GetPose() == CM_CROUCH )
			{
				if ( t.nPassable & CP_STAND )
					{
						m.dest = SPathPlace( src.GetX(), src.GetY(), src.GetLayer(), 0, CM_STAND, 0 );
						m.type = MT_POSE_WALK_CROUCH;
						res[ pResPos ] = m; 
						dynLocks[ pResPos ] = 0;
						++pResPos;
					}
			}
			// lay -> stand
			if ( src.GetPose() == CM_LAY && (t.nPassable & CP_STAND) )
			{
				m.dest = SPathPlace( src.GetX(), src.GetY(), src.GetLayer(), 0, CM_STAND, 0 );
				m.type = MT_POSE_WALK_CRAWL;
				res[ pResPos ] = m; 
				dynLocks[ pResPos ] = 0;
				++pResPos;
			}
			// lay -> crouch
			if ( src.GetPose() == CM_LAY && (t.nPassable & CP_CROUCH) )
			{
				m.dest = SPathPlace( src.GetX(), src.GetY(), src.GetLayer(), 0, CM_CROUCH, 0 );
				m.type = MT_POSE_CROUCH_CRAWL;
				res[ pResPos ] = m; 
				dynLocks[ pResPos ] = 0;
				++pResPos;
			}
		}
		// find transitions to other layers
		if ( src.GetPose() == CM_STAND || src.GetPose() == CM_CROUCH || src.GetPose() == CM_LAY )
		{
			CLayersGroup *pGroup = pLayer->pGroup;
			int nFloor = pLayer->nFloor; 
			if ( t.nFlags & TF_HAS_SAME )
			{
				for ( int nL = 0; nL < N_MAX_FLOORS * N_MAX_LAYERS_PER_FLOOR; ++nL )
				{
					CNodesLayer *pNew = pGroup->layers[ nL ];
					if ( !pNew )
						continue;
					if ( pNew == pLayer )
						continue;
					CNodesLayer::STile &tDst = pNew->tiles[ src.GetY() ][ src.GetX() ];
					if ( tDst.nHeight == t.nHeight && tDst.nDisplacement == t.nDisplacement )				
					{
						if ( src.GetPose() != CM_LAY )
							m.dest = SPathPlace( src.GetX(), src.GetY(), pNew->nLayer, 0, src.GetPose(), 0 );
						else
							m.dest = SPathPlace( src.GetX(), src.GetY(), pNew->nLayer, src.GetDirection(), src.GetPose(), src.IsMoving() );
						m.type = MT_ZERO;
						res[ pResPos ] = m;
						dynLocks[ pResPos ] = 0;
						++pResPos;
					}
				}
			}
			SPathPlace search( src.GetX(), src.GetY(), src.GetLayer() );
			if ( ( t.nFlags & TF_HAS_INTERGRID ) == 0 )
			{
#ifdef _DEBUG
			CNodesLayer::CTransitionsHash::const_iterator mustBeEnd = pLayer->transitions.find( search );
			ASSERT( mustBeEnd == pLayer->transitions.end() );
#endif
				*pMovesCount = pResPos;
				return;
			}
			CNodesLayer::CTransitionsHash::const_iterator k = pLayer->transitions.find( search );
			if ( k == pLayer->transitions.end() )
			{
				ASSERT(0);
			}
			else
			{
				const CNodesLayer::STransitionSet &trSet = k->second;
				for ( list<CNodesLayer::SLink>::const_iterator l = trSet.links.begin(); l != trSet.links.end(); ++l )
				{
					const SPathPlace &dst = l->dst;
					CNodesLayer *pLayer1 = layers[ dst.GetLayer() ];
					//pLayer1->RefreshSpot( dst, pMap, 2 );
					CNodesLayer::STile &t1 = pLayer1->tiles[dst.GetY()][dst.GetX()];
					float fDist = fabs( pNet->GetCPNoHeight(src) - pNet->GetCPNoHeight(dst) );
					bool bCan = false;
					EMoveType type;
					switch ( src.GetPose() )
					{
						case CM_STAND:
							bCan = (( t1.nPassable & CP_STAND ) != 0);
							type = (fDist > FP_GRID_STEP) ? MT_MOVE_STAND_DIAG : MT_MOVE_STAND;
							break;
						case CM_CROUCH:
							bCan = (( t1.nPassable & CP_CROUCH ) != 0);
							type = (fDist > FP_GRID_STEP) ? MT_MOVE_CROUCH_DIAG : MT_MOVE_CROUCH;
							break;
					}
					if ( fDist < 0.02f ) 
						type = MT_ZERO;
					if ( src.GetPose() != CM_LAY )
					{
						if ( bCan )
						{
							m.dest = SPathPlace( l->dst.GetX(), l->dst.GetY(), l->dst.GetLayer(), 0, src.GetPose(), 0 );
							m.type = type;
							if ( IsGoodPt( m.dest, pNet ) )
							{
								res[ pResPos ] = m;
								dynLocks[ pResPos ] = 0;
								++pResPos;
							}
						}
					}
					else // lay
					{
						int nDirAfter = pNet->GetClosestDir( dst, src );
						bCan = (( t1.nPassable & (CP_LAY1 << (nDirAfter & 3)) ) != 0);
						if ( bCan )
						{
							type = (fDist > FP_GRID_STEP) ? MT_MOVE_CRAWL_DIAG : MT_MOVE_CRAWL;
							int nDirBefore = pNet->GetClosestDir( src, dst );
							if ( fDist < 0.01 )
							{
								nDirBefore = src.GetDirection();
								float fAbsoluteDir = pNet->GetDirection( src );
								nDirAfter = pNet->GetClosestDir( dst.GetLayer(), fAbsoluteDir );
							}
							for ( int i = nDirFrom; i != nDirTo; i = (i+1) & 7 )
							{
								if ( i == nDirBefore )
								{
									m.dest = SPathPlace( l->dst.GetX(), l->dst.GetY(), l->dst.GetLayer(), ( nDirAfter + 4 ) & 7, src.GetPose(), 1 );
									m.type = type;
									if ( IsGoodPt( m.dest, pNet ) )
									{
										res[ pResPos ] = m;
										dynLocks[ pResPos ] = 0;
										++pResPos;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	*pMovesCount = pResPos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
