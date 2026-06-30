#ifndef __aiMoves_H_
#define __aiMoves_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
namespace NRPG
{
enum EAction;
}
namespace NAI
{
struct SMove;
struct SPathPlace;
class IPathNetwork;
enum ETransitionType;

////////////////////////////////////////////////////////////////////////////////////////////////////
ETransitionType GetTransitionType( const IPathNetwork *pNet, const SPathPlace &src, const SPathPlace &dst );
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetNonStandartMoves() is different than GetMoves() in such aspects:
// 1) All stand poses with different directions and moving/nonmoving only are identical, so
//	1a) turn moves when we stand are not returned - done
//	1b) moves are returned for all directions
//	1c) moves are returned with final direction = 0, and with "not moving" flag
// 2) Maybe there will be some kinda difference with crouch poses too.
void GetNonStandartMoves( 
		IPathNetwork *_pNet, const SPathPlace &src, bool bCheckSuicide, bool bMoveOnly, vector<SMove> *pRes,
		int *pMovesCount, vector<char> *pDynLocks );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
