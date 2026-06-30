#include "StdAfx.h"
#include "wMainMoves.h"
#include "RPGUnitInfo.h"
#include "wUnitServer.h"
#include "aiMoves.h"
#include "Grid.h"
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::EAction GetMoveActionType( const NAI::IPathNetwork *pNet, const NAI::SUnitPosition &src, 
	const NAI::SUnitPosition &dst, bool bCorpse )
{
	NAI::ETransitionType tt = NAI::GetTransitionType( pNet, src.pos.p, dst.pos.p );
	switch ( tt )
	{
		case NAI::TT_MOVE:
			return bCorpse ? NRPG::AC_MOVE_CORPSE_SIDE : NRPG::AC_MOVE_SIDE;
		case NAI::TT_MOVE_DIAGONAL:
			return bCorpse ? NRPG::AC_MOVE_CORPSE_DIAGONAL : NRPG::AC_MOVE_DIAGONAL;
		case NAI::TT_INTERGRID:
			if ( fabs( src.GetCPNoHeight() - dst.GetCPNoHeight() ) > FP_GRID_STEP )
				return bCorpse ? NRPG::AC_MOVE_CORPSE_DIAGONAL : NRPG::AC_MOVE_DIAGONAL;
			else
				return bCorpse ? NRPG::AC_MOVE_CORPSE_SIDE : NRPG::AC_MOVE_SIDE;
		case NAI::TT_SAME:
		case NAI::TT_INTERGRID_SAME:
			return NRPG::AC_NONE;
		case NAI::TT_TURN:
			return NRPG::AC_NONE;
		case NAI::TT_CLIMB_1:  return NRPG::AC_CLIMB_1;
		case NAI::TT_CLIMB_2:  return NRPG::AC_CLIMB_2;
		case NAI::TT_CLIMB_3:  return NRPG::AC_CLIMB_3;
		case NAI::TT_CLIMB_4:  return NRPG::AC_CLIMB_4;
		case NAI::TT_JUMP:     return NRPG::AC_JUMP;
		case NAI::TT_POSE:
			{
				switch ( dst.GetPose() )
				{
				case NAI::RUN:
					return NRPG::AC_POSE_RUN;
				case NAI::WALK:
					return NRPG::AC_POSE_WALK;
				case NAI::CROUCH:
					return NRPG::AC_POSE_CROUCH;
				case NAI::CRAWL:
					return NRPG::AC_POSE_CRAWL;
				default:
					ASSERT(0);
					break;
				}
				break;
			}
		case NAI::TT_LADDER_UP:
		case NAI::TT_LADDER_DOWN:
			return NRPG::AC_LADDER;
		case NAI::TT_LADDER_MOVE:
			return NRPG::AC_LADDER_MOVE;
		default:
			ASSERT( 0 );
			break;
	}
	return NRPG::AC_MOVE_SIDE;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SelectFindPathUnits( CUnit *pWho, list<CObjectBase*> *pRes )
{
	list<CPtr<CUnit> > visible;
	pWho->GetPlayer()->GetVisible( &visible );
	visible.remove( pWho );
	list<CObjectBase*> &vis = *pRes;
	for ( list<CPtr<CUnit> >::iterator i = visible.begin(); i != visible.end(); ++i )
	{
		CUnitServer *pUS = dynamic_cast<CUnitServer*>( i->GetPtr() ); 
		ASSERT( pUS );
		if ( IsValid( pUS ) )
		{
			if ( pUS->CanFight() && !pUS->IsMoving() )
				vis.push_back( *i );
			if ( pUS->IsEmptyPK() || pUS->GetWearingDBPK() )
				vis.push_back( *i );
		}
	}

	list<CPtr<CObjectBase> > traps;
	pWho->GetPlayer()->GetTrappedObjectsList( &traps );
	for ( list<CPtr<CObjectBase> >::iterator i = traps.begin(); i != traps.end(); ++i )
		vis.push_back( *i );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}