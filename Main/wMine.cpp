#include "StdAfx.h"
#include "wMine.h"
#include "..\Misc\RandomGen.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataGeometry.h"
#include "wMain.h"
#include "wUnitServer.h"   // complete NWorld::CUnitServer before CMinesWorld forces the CWorld TBS template instantiation
#include "Transform.h"
#include "wOSBase.h"
#include "aiMap.h"
#include "aiStability.h"
#include "scriptCallLua.h"
//
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMine
////////////////////////////////////////////////////////////////////////////////////////////////////
CMine::CMine( CWorld *_pWorld, const CVec3 &_vPlace, NDb::CRPGMine *_pMine, int _nDC, int _nFloor, CUnitServer *_pMaster, int _nAngle )
: pWorld(_pWorld), vPlace(_vPlace), pMine(_pMine), nDC(_nDC), nFloor(_nFloor), pMaster(_pMaster)
{
	SRand rnd;
	pModel = pMine->pItem->pModel->CreateModel( &rnd );
	// retail @0x37ead0: -111111 sentinel -> random facing, else degrees * pi/180
	fAngle = _nAngle == MINE_ANGLE_RANDOM ? random.GetFloat( 0, FP_2PI ) : ToRadian( (float)_nAngle );
	bindGlobal.Link( pWorld->GetUnits(), this );
	pWorld->AddMine( this );
	pMineTracker = pWorld->GetMineTracker();
	pMineTracker->AddMine( this, vPlace );
	// retail CMine::CMine @0x37ead0 tail: register with the wreckage stability grid so a mine
	// whose floor collapses under it goes boom (IStabilityTrackers::AddMine @0xa6840)
	pWorld->GetAIMap()->GetStabilityTrackers()->AddMine( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMine::~CMine()
{
	if ( IsValid(pMineTracker) )
		pMineTracker->RemoveMine( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMine::Visit( IRenderVisitor *p )
{
	SFBTransform pos;
	MakeMatrix( &pos, CVec3(1,1,1), vPlace, fAngle );
	p->AddMesh( pModel, pos, 0, nFloor, -1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMine::Visit( IAIVisitor *p )
{
	SFBTransform pos;
	MakeMatrix( &pos, CVec3(1,1,1), vPlace, fAngle );
	int nMask = ( GetMask( pModel->pGeometry->pAIGeometry, pModel->pRPGArmor ) | TS_PICK ) & ~TS_PASS_BLOCKER;
	p->AddHull( pModel->pGeometry->pAIGeometry, pos, pModel->pRPGArmor, nFloor, nMask );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x37e580: a hit just sets the mine off and reports the -1 "no damage" sentinel
// (disasm 0x77e58e: {nDmg=-1, RD_UNKNOW}). Dev returned 0.
int CMine::ProcessAttack( NWorld::IWorld *pWorld, int nUserID, NRPG::CAttackPortion *pAttack,
	const CVec3 &vDir, NDb::CRPGArmor *pArmor )
{
	GoBoom();
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CMine::GetMinePos() 
{ 
	return vPlace;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CRPGItem* CMine::DisarmMine() 
{ 
	NDb::CRPGMine *pRPGMine = pMine;
	CMObj<CMine> pHold(this);
	pHold = 0;
	return pRPGMine->pItem;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMine::GoBoom( CUnitServer *pWho )
{
	if ( !IsValid(this) )
		return;
	ASSERT( pMine->pExplosion );
	if ( pMine->pExplosion )
		pWorld->AddGrenadeExplosion( GetMinePos(), pMine->pExplosion, pMaster, 0, &sPerkModifiers );   // retail @0x37e400: credit the placer (pMaster) for the blast
	pWorld->RemoveMine( this );
	NScript::luaCallFunction( "OnMineTriggered", "p", IsValid( pWho ) ? CastToObjectBase( pWho ) : 0 );
	CMObj<CMine> pHold(this);
	pHold = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail free NWorld::GoBoom @0x37e530: detonate an IMine with no attributed shooter -- dynamic-
// cast to the concrete CMine and fire unless it is already dead/zombie. The stability trackers
// call this when the ground under a settled mine drops away (CStabilityTracker::OnChange @0xa59a0).
void GoBoom( IMine *pMine )
{
	CDynamicCast<CMine> pReal( pMine );
	if ( pReal && IsValid( pReal.GetPtr() ) )
		pReal->GoBoom( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMineTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
/*static NAI::SPathPlace GetNormalized( const NAI::SPathPlace &_place )
{
	NAI::SPathPlace place( _place.GetX(), _place.GetY(), _place.GetLayer() );
	return place;
}*/
const float F_DISCR_STEP = 0.25f;
static CVec3 GetNormalized( const CVec3 &_v )
{
	return CVec3( 
		Float2Int( _v.x / F_DISCR_STEP ) * F_DISCR_STEP,
		Float2Int( _v.y / F_DISCR_STEP ) * F_DISCR_STEP,
		Float2Int( _v.z / F_DISCR_STEP ) * F_DISCR_STEP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMineTracker::AddMine( CMine *p, const CVec3 &_vPlace )
{
	CVec3 vPlace( GetNormalized( _vPlace ) );
	for ( int nZ = -1; nZ <= 1; ++nZ )
	{
		for ( int nY = -1; nY <= 1; ++nY )
		{
			for ( int nX = -1; nX <= 1; ++nX )
			{
				CVec3 vPos( vPlace );
				vPos.x += nX * F_DISCR_STEP;
				vPos.y += nY * F_DISCR_STEP;
				vPos.z += nZ * F_DISCR_STEP;
				CMineSet &ms = mines[ vPos ];
				if ( !IsInSet( ms, p ) )
					ms.push_back( p );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMineTracker::RemoveMine( CMine *p )
{
	for ( CPlaceMinesHash::iterator i = mines.begin(); i != mines.end(); )
	{
		CMineSet &ms = i->second;
		CMineSet::iterator k = find( ms.begin(), ms.end(), p );
		if ( k != ms.end() )
			ms.erase( k );
		if ( ms.empty() )
			mines.erase( i++ );
		else
			++i;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMineTracker::GetMines( const vector<CVec3> &places, vector<CPtr<CMine> > *pRes ) const
{
	pRes->resize(0);
	for ( int k = 0; k < places.size(); ++k )
	{
		CVec3 v( GetNormalized( places[k] ) );
		CPlaceMinesHash::const_iterator i = mines.find( v );
		if ( i != mines.end() )
		{
			const CMineSet &m = i->second;
			for ( int k = 0; k < m.size(); ++k )
			{
				CMine *p = m[k];
				if ( !IsInSet( *pRes, p ) )
					pRes->push_back( p );
			}
		}
	}
	return !pRes->empty();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMinesWorld  (module wMainMine.obj) -- retail split of CWorld::AddMine/RemoveMine/GetMinesNear.
////////////////////////////////////////////////////////////////////////////////////////////////////
CMinesWorld::CMinesWorld( bool /*bRegister*/ )
{
	// retail @0x37b9b0: empty trappedObjects + an owned, freshly-created CMineTracker.
	pMineTracker = new CMineTracker;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMinesWorld::AddMine( IMine *pMine )
{
	// retail @0x37b780: dedup linear scan, then unconditional tail append.
	// v1.2 UNCHANGED (verified: true v1.2 AddMine @VA 0x77bbb0 keeps this guard; the V12_DELTA
	// "guard removed" row was a ghidriff BSIM collision with CUnitServer::MarkInterrupted @0x7721c0).
	if ( find( trappedObjects.begin(), trappedObjects.end(), pMine ) != trappedObjects.end() )
		return;
	trappedObjects.push_back( pMine );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMinesWorld::RemoveMine( IMine *pMine )
{
	// retail @0x37b6f0: unlink every matching node, then fire the situation notify.
	trappedObjects.remove( pMine );
	GetWorld()->GlobalSituationHasChanged();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMinesWorld::GetMinesNear( const CVec3 &pos, list<CPtr<IMine> > *pRes, float fRadius )
{
	// retail @0x37b840: collect live, armed mines within fRadius, pruning dead/null
	// nodes in passing; the radius test is a STRICT squared-distance compare.
	pRes->clear();
	for ( list< CPtr<IMine> >::iterator i = trappedObjects.begin(); i != trappedObjects.end(); )
	{
		IMine *pMine = *i;
		if ( IsValid(pMine) )
		{
			if ( pMine->IsMineSet() && fabs2( pos - pMine->GetMinePos() ) < sqr(fRadius) )
				pRes->push_back( pMine );
			++i;
		}
		else
			i = trappedObjects.erase( i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0x019c2120, CMine )
REGISTER_SAVELOAD_CLASS( 0x019c2170, CMineTracker )