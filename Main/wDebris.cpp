#include "StdAfx.h"
#include "wDebris.h"
#include "wInterface.h"
#include "GAnimParticles.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataGeometry.h"
#include "aiMap.h"
#include "aiStability.h"
#include "GAnimation.h"
#include "RPGItemInfo.h"
#include "Transform.h"
#include "..\Misc\RandomGen.h"
#include "RPGAttackMech.h"
#include "GSceneUtils.h"
#include "GView.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDFrozenItem
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail ctor @0x34aaf0 (ret 0x20 = 7 semantic args + the vbase flag): the trailing bool is stored to
// +0x88 (bIsTemporaryVisible) at @0x74ac69 `mov [ebx+0x88], dl` -- dl = arg 7.
CDFrozenItem::CDFrozenItem( CSyncSrc<IVisObj> *pShow, const SItemRenderInfo &_model, const SHMatrix &_m,
	int _nFloor, NRPG::IInventoryItem *_pItem, CDebrisControllerTrash *_pTrash, bool _bIsTemporaryVisible )
	: model(_model), m(_m), nFloor(_nFloor), pInvItem(_pItem), pTrash( _pTrash ), bIsTemporaryVisible( _bIsTemporaryVisible )
{
	bindGlobal.Link( pShow, this );
	//
	if ( IsValid( model.pModel ) )
		nMaxVP = model.pModel->GetMaxVP();
	else
		nMaxVP = 1;
	//
	nVP = nMaxVP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CDFrozenItem::GetVisiblePos @0x349e50 -- the LOS probe points of an on-ground item:
//   pRes->resize(1); (*pRes)[0] = m.GetTranslation();          // fallback: the matrix translation
//   NAI::GetSpheres( model.pModel, &spheres );                 // the model's mass spheres (model space)
//   if ( !spheres.empty() ) { pRes->resize( spheres.size() );
//       for (i) (*pRes)[i] = m * spheres[i].ptCenter; }        // centres pushed through the world matrix
// so a rifle lying flat probes one point per collision sphere along its length, not just the origin.
// (Oracle: s2_scratch/src/s2_cdfrozenitem.h `CDFrozenItem_GetVisiblePos`.)
void CDFrozenItem::GetVisiblePos( vector<CVec3> *pRes ) const
{
	pRes->resize( 1 );
	(*pRes)[0] = m.GetTranslation();
	vector<SMassSphere> spheres;
	CVec3 massCenter;
	NAI::GetSpheres( model.pModel, &spheres, &massCenter );
	if ( !spheres.empty() )
	{
		pRes->resize( spheres.size() );
		for ( int i = 0; i < (int)spheres.size(); ++i )
			m.RotateHVector( &(*pRes)[i], spheres[i].ptCenter );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x34a4b0: vDir unread, single-shot (nAccumulated = 0).
int CDFrozenItem::ProcessAttack( NWorld::IWorld *pWorld, int nUserID, NRPG::CAttackPortion *pAttack,
	const CVec3 &vDir, NDb::CRPGArmor *pArmor )
{
	if ( IsValid( pArmor ) && nVP > 0 )
	{
		int nDmg = pAttack->CalcStructDmg( pWorld, pArmor, 0 );
		nVP = Max( 0, nVP - nDmg );
		if ( nVP == 0 )
			pTrash->itemsToRemove.push_back( this );
		return nDmg;
	}
	//
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDFrozenItem::Visit( IRenderVisitor *p )
{
	SFBTransform pos;
	pos.forward = m;
	pos.backward.HomogeneousInverse( m );
	if ( model.pModel )
		p->AddMesh( model.pModel, pos, 0, nFloor, -1 );
	else
		p->AddHead( model.pUnit, new NGScene::CCFBTransform( pos ), NGScene::SRoomInfo( nFloor ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetMask( NDb::CAIGeometry *pAIGeometry, NDb::CRPGArmor *pArmor );
void CDFrozenItem::Visit( IAIVisitor *p )
{
	NDb::CModel *pModel = model.pModel;
	if ( !pModel )
		return;
	SFBTransform pos;
	pos.forward = m;
	pos.backward.HomogeneousInverse( m );

	int nAddMask = 0;
	if ( IsValid( pInvItem ) )
		nAddMask |= TS_PICK;
	int nMask = GetMask( pModel->pGeometry->pAIGeometry, pModel->pRPGArmor ) | nAddMask;
	p->AddHull( pModel->pGeometry->pAIGeometry, pos, pModel->pRPGArmor, nFloor, nMask );
	// TS_VIRTUAL|TS_PICK|TS_FRAGMENTED|TS_VISION|TS_COVER|TS_PASS_BLOCKER
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDItem
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail ctor @0x34acc0: the trailing pVisibilityParent (CDItem+0x2c, save tag 7) is the fog-gate
// carrier -- Segment @0x34b4b0 reads it back at physics-settle to keep an unseen unit's dropped
// item hidden in its frozen form too.
CDItem::CDItem( CSyncSrc<IVisObj> *pShow, const SItemRenderInfo &_model, CFuncBase<NAnimation::SSkeletonPose> *_pAnim,
	int _nFloor, NRPG::IInventoryItem *_pItem, CObjectBase *_pVisibilityParent )
	: model(_model), pAnimation(_pAnim), nFloor(_nFloor), pInvItem(_pItem), pVisibilityParent(_pVisibilityParent)
{
	bindGlobal.Link( pShow, this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CDItem::GetPos @0x34a030: DGPtr-style refresh of the skeleton animation (frame-stamp check +
// Process), then the root bone's position -- the item's CURRENT physics position. Used by
// FilterVisibleItems<CDItem> @0x34bd40 as the gather-range test point.
CVec3 CDItem::GetPos() const
{
	CDGPtr< CFuncBase<NAnimation::SSkeletonPose> > pPos = GetAnimation();
	pPos.Refresh();
	return pPos->GetValue()[0].pos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDItem::Visit( IRenderVisitor *p )
{
	if ( model.pModel )
		p->AddItemMesh( model.pModel, pAnimation, nFloor );
	else
		p->AddItemHead( model.pUnit, pAnimation, nFloor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDebrisController
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDebrisController::Add( CDItem *pItem, NAnimation::CASphereSet *pAnim )
{
	STrackItem d;
	d.pItem = pItem;
	d.pAnim = pAnim;
	items.push_back( d );
	showItems.push_back( pItem );
	InitAction();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDebrisController::InnerSegment( list<STrackItem> *pRes )
{
	for ( list<STrackItem>::iterator i = items.begin(); i != items.end(); )
	{
		i->pAnim->Calc( GetWorldTime() );
		//
		if ( i->pAnim->HasStopped() )
		{
			list<STrackItem>::iterator k = i++;
			k->pItem->SetFloor( k->pAnim->GetFloor() );
			pRes->splice( pRes->end(), items, k );
		}
		else
			++i;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
struct SRangeTest
{
	const SSphere &s;
	float fR2;
	list<CObj<T> > &res;
	SRangeTest( const SSphere &_sp, list<CObj<T> > &_res ): s(_sp), res(_res) { fR2 = sqr( s.fRadius ); }
	bool operator()( T *p ) const 
	{
		if ( fabs2( s.ptCenter - p->GetPos() ) < fR2 )
		{
			res.push_back( p );
			return true;
		}
		return false;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDebrisController::GetInSphere( const SSphere &sphere, list< CObj<CDFrozenItem> > *pRes )
{
	SRangeTest<CDFrozenItem> testFI( sphere, *pRes );
	for ( list<CObj<CDFrozenItem> >::iterator i = showFrozenItems.begin(); i != showFrozenItems.end(); )
	{
		if ( testFI( *i ) )
			i = showFrozenItems.erase( i );
		else
			++i;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x749c20 (release CDebrisController::GetFrozenItem; the GetWorldItem @0x774cd0 thunk jmps here) -- the
// on-ground world item carrying `pInvItem`: the first showFrozenItems entry whose GetInvItem() matches.
CDFrozenItem* CDebrisController::GetFrozenItem( NRPG::IInventoryItem *pInvItem )
{
	for ( list< CObj<CDFrozenItem> >::iterator i = showFrozenItems.begin(); i != showFrozenItems.end(); ++i )
	{
		CDFrozenItem *p = i->GetPtr();
		if ( p != 0 && p->GetInvItem() == pInvItem )
			return p;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release luaFindItem @0x2e7170 (sig "n"): the first on-ground item whose RPG db-record id matches
// nDbID. Retail enumerates via the WHOLE-LEVEL region query over visibleItems (GetVisibleItems,
// radius 0xFFFF) and RTTI-filters frozen items -- scan the same source so the two builds cannot
// diverge (showFrozenItems and visibleItems differ on removal bookkeeping); the predicate
// GetInvItem()->GetDBItem()->GetRecordID() == nDbID is byte-identical to retail's [rec+0xc] test.
CDFrozenItem* CDebrisController::FindFrozenItem( int nDbID )
{
	SSphere sphere;
	sphere.ptCenter = VNULL3;
	sphere.fRadius = 65535.0f;
	list<IVisible*> items;
	GetVisibleItems( sphere, &items );
	for ( list<IVisible*>::iterator i = items.begin(); i != items.end(); ++i )
	{
		CDFrozenItem *p = dynamic_cast<CDFrozenItem*>( *i );
		if ( p != 0 && p->GetInvItem() != 0 && p->GetInvItem()->GetDBItem()->GetRecordID() == nDbID )
			return p;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDebrisController::AddDebris( const SItemRenderInfo &_model, NAI::IAIMap *pAIMap, const CVec3 &ptCenter, const CQuat &q,
	const CVec3 &velocity, CFuncBase<STime> *pTime, bool bFallFromBody, CObjectBase *pVisibilityParent,
	NRPG::IInventoryItem *pItem, int nFloor )
{
	if ( _model.pModel )
	{
		NDb::CAIGeometry *pAIGeom = _model.pModel->pGeometry->pAIGeometry;
		if ( !pAIGeom )
			DebugTrace( "Geometry %d has no AI geometry\n", _model.pModel->pGeometry->GetRecordID() );
	}
	vector<SMassSphere> spheres;
	CVec3 massCenter;
	NAI::GetSpheres( _model.pModel, &spheres, &massCenter );
	CVec3 boundCenter( 0, 0, 0 ), boundSize( 0.5f, 0.5f, 0.5f );
	if ( _model.pModel )
	{
		boundCenter = _model.pModel->pGeometry->boundCenter;
		boundSize = _model.pModel->pGeometry->boundSize;
	}
	// retail @0x74aef9: the sphere set itself starts on floor -2 (push -2), no ignore object;
	// Step reassigns nFloor from the hit surfaces as it tumbles
	NAnimation::CASphereSet *pSphere = new NAnimation::CASphereSet( spheres, boundCenter, boundSize, 0, -2 );
	pSphere->pMap = pAIMap;
	pSphere->pTime = pTime;
	// retail @0x74af85: phase = PH_THROW_OUT + bFallFromBody; pItem drives the phys-case RTTI
	pSphere->Init( pTime->GetValue(), ptCenter, q, velocity, false,
		NAnimation::EPhysCase( NAnimation::PH_THROW_OUT + bFallFromBody ), pItem );
	NAnimation::CSkeletonAnimator *pAnimator = new NAnimation::CSkeletonAnimator( 0 );
	pAnimator->pTime = pTime;
	pAnimator->AddAnimator( pTime->GetValue(), pSphere );

	// retail AddDebris @0x34ade0 selects the render sync by the CObjectBase* visibility-parent alone
	// (@0x74b00f `test ebp,ebp`), SEPARATE from the item: every drop off a body threads the unit as
	// parent (fog-gated list); knife/blast debris (no parent) stays on the always-on list. The parent
	// is stored on the CDItem so the gate SURVIVES physics-settle: Segment @0x34b4b0 reads it back
	// and forwards `parent != 0` to AddFrozenItem @0x34b100. The CDItem floor is the caller's nFloor.
	CDItem *pI = new CDItem( pVisibilityParent ? GetVisibleShowList() : GetShowList(), _model, pAnimator, nFloor, pItem, pVisibilityParent );
	// retail @0x74b04a: a fog-gated flying item is ALSO published into visibleDynamicItems so
	// CUnitServer::UpdateVisible's dynamic-items loop (@0x7c5341) can LOS-reveal it in flight;
	// the publish gate is `pVisibilityParent != 0` alone (`test ebp,ebp`).
	if ( pVisibilityParent )
		visibleDynamicItems.push_back( pI );
	Add( pI, pSphere );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDebrisController::ActivateDebris( const SSphere &sphere, NAI::IAIMap *pAIMap, 
	CFuncBase<STime> *pTime )
{
	bc.Add( sphere.ptCenter, sphere.fRadius );
	list<CObj<CDFrozenItem> > affected;
	GetInSphere( sphere, &affected );
	for ( list<CObj<CDFrozenItem> >::iterator i = affected.begin(); i != affected.end(); ++i )
	{
		CDFrozenItem *p = *i;
		CVec3 pos = p->GetPos();
		CVec3 vel = pos - sphere.ptCenter;
		float fDist = fabs( vel );
		Normalize( &vel ); vel += CVec3(0,0,1);
		vel *= Max( 0.0f, 1 - sqr( fDist / sphere.fRadius ) );
		// retail ActivateDebris @0x34a640 tail (@0x74a7c6): bFallFromBody=false, the frozen item's
		// own CObjectBase rides as the visibility parent (fog gate + dynamic vision candidate),
		// and the relaunched debris keeps the item's own floor (item->GetFloor() pushed as nFloor).
		AddDebris( p->GetModel(), pAIMap, pos, QNULL, vel, pTime, false, p, p->GetInvItem(), p->GetFloor() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail ActivateDebris @0x34a240 (IDFrozenItem overload): the stability trackers found the
// support under this ONE settled item gone -- unlink it from the frozen list and relaunch it as
// dynamic debris (zero velocity, orientation from the item's own matrix; the item rides as its own
// visibility parent so the fog gate carries over, same as the sphere overload).
void CDebrisController::ActivateDebris( CDFrozenItem *pItem, NAI::IAIMap *pAIMap, CFuncBase<STime> *pTime )
{
	if ( !pItem )
		return;
	// retail holds a ref across the unlink (RTDynamicCast ref + trailing ReleaseObj @0x34a240) --
	// showFrozenItems is the OWNER list, so the erase below would otherwise destroy the item
	CObj<CDFrozenItem> pHold( pItem );
	for ( list<CObj<CDFrozenItem> >::iterator i = showFrozenItems.begin(); i != showFrozenItems.end(); ++i )
	{
		if ( i->GetPtr() == pItem )
		{
			showFrozenItems.erase( i );
			break;
		}
	}
	CQuat q;
	q.FromEulerMatrix( pItem->GetMatrix() );
	// retail @0x74a34c: bFallFromBody=false, parent=the item itself, nFloor=item->GetFloor()
	AddDebris( pItem->GetModel(), pAIMap, pItem->GetPos(), q, VNULL3, pTime, false, pItem, pItem->GetInvItem(), pItem->GetFloor() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CDFrozenItem* CDebrisController::AddFrozenItem( NAI::IAIMap *pMap, const SHMatrix &m, NRPG::IInventoryItem *pInvItem, const SItemRenderInfo &_model, int nFloor, bool bTemporaryVisible, bool bVisibleGated )
{
	// retail @0x34b100: `(pInvItem == 0 && !bVisibleGated) ? GetShowList() : GetVisibleShowList()`,
	// and the visibleItems (vision-candidate) publish runs when `pInvItem != 0 || bVisibleGated`.
	// bVisibleGated covers the item-less uniform cap of an UNSEEN unit: its frozen form must sit on
	// the fog-gated list AND be discoverable by CUnitServer::UpdateVisible's GetVisibleItems query,
	// so gaining LOS reveals it -- the bare `pInvItem != 0` test parked it on the always-on list.
	// bTemporaryVisible threads straight into the CDFrozenItem ctor (@0x74b15b, ctor arg 7).
	bool bIsVisibleItem = pInvItem != 0 || bVisibleGated;
	CDFrozenItem *pWorldItem = new CDFrozenItem(
		bIsVisibleItem ? GetVisibleShowList() : GetShowList(),
		_model, m, nFloor, pInvItem, pTrash, bTemporaryVisible );
	if ( bIsVisibleItem )
	{
		visibleItems.push_back( pWorldItem );
		// retail @0x34b100 tail (same branch): register the settled item with the wreckage
		// stability grid so collapsing support re-drops it (IStabilityTrackers::AddDebris @0xa6770)
		if ( pMap )
			pMap->GetStabilityTrackers()->AddDebris( pWorldItem );
	}
	showFrozenItems.push_back( pWorldItem );
	// retail @0x34b100 tail: a CLUE item's frozen form is additionally tracked on clueItems
	// (save tag 8) -- gated by the RTDynamicCast<NRPG::IClueItem> on the inventory item.
	if ( CDynamicCast<NRPG::IClueItem>( pInvItem ) )
		clueItems.push_back( pWorldItem );
	return pWorldItem;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CDebrisController::GetClueObjects @0x34a870 (CWorld::GetClueItems @0x361520 forwards):
// walk clueItems -- ERASE dead entries in place (null/zombie), append the live ones to *pRes.
void CDebrisController::GetClueObjects( list<CPtr<CObjectBase> > *pRes )
{
	if ( !pRes )
		return;
	for ( list<CPtr<CDFrozenItem> >::iterator i = clueItems.begin(); i != clueItems.end(); )
	{
		CDFrozenItem *pItem = i->GetPtr();
		if ( !IsValid( pItem ) )
			i = clueItems.erase( i );   // prune the dead clue node (retail unlink + release)
		else
		{
			pRes->push_back( CPtr<CObjectBase>( pItem ) );
			++i;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail public overload @0x34b340: (pMap, pos, rot, pInvItem, bool bTemporaryVisible, int nFloor);
// @0x74b45d forwards bTemporaryVisible as the inner overload's arg 6 and hardwires bVisibleGated 0.
CDFrozenItem* CDebrisController::AddFrozenItem( NAI::IAIMap *pMap, const CVec3 &pos, const CQuat &rot, NRPG::IInventoryItem *pInvItem, bool bTemporaryVisible, int nFloor )
{
	CPtr<NRPG::IInventoryItem> pHold( pInvItem );
	if ( !IsValid( pInvItem->GetDBItem()->pModel ) )
	{
		ASSERT( 0 );
		return 0;
	}
	SRand rnd;
	SHMatrix m;
	MakeMatrix( &m, pos, rot );
	CDFrozenItem *pWorldItem = AddFrozenItem( pMap, m, pInvItem, pInvItem->GetDBItem()->pModel->CreateModel( &rnd ), nFloor, bTemporaryVisible );
	return pWorldItem;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDebrisController::RemoveFrozenItem( NRPG::IInventoryItem *pInvItem )
{
	list<CObj<CDFrozenItem> >::iterator it;
	for ( it = showFrozenItems.begin(); it != showFrozenItems.end(); ++it )
	{
		if ( (*it)->GetInvItem() == pInvItem )
		{
			showFrozenItems.erase( it );
			break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDebrisController::GetVisibleItems( const SSphere &sphere, list<IVisible*> *pRes )
{
	// retail FilterVisibleItems<CDFrozenItem> @0x34bc40: dead entries erased; the range test point is
	// the item's GetPos() (the IItem-base vtbl+4 @0x34c290, m.GetTranslation()) -- the multi-point
	// GetVisiblePos is only for the LOS rays, not the gather.
	for ( list<CPtr<CDFrozenItem> >::iterator i = visibleItems.begin(); i != visibleItems.end(); )
	{
		CDFrozenItem *p = *i;
		if ( IsValid(p) )
		{
			if ( fabs2( p->GetPos() - sphere.ptCenter ) <= sqr( sphere.fRadius ) )
				pRes->push_back( p );
			++i;
		}
		else
			i = visibleItems.erase( i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail GetVisibleDynamicItems @0x34a420 -> FilterVisibleItems<CDItem> @0x34bd40: the same
// erase-dead + range filter over the in-flight vision candidates; the test point is the item's
// CURRENT physics position (CDItem::GetPos @0x34a030, the refreshed root-bone pos).
void CDebrisController::GetVisibleDynamicItems( const SSphere &sphere, list<IVisible*> *pRes )
{
	for ( list<CPtr<CDItem> >::iterator i = visibleDynamicItems.begin(); i != visibleDynamicItems.end(); )
	{
		CDItem *p = *i;
		if ( IsValid(p) )
		{
			if ( fabs2( p->GetPos() - sphere.ptCenter ) <= sqr( sphere.fRadius ) )
				pRes->push_back( p );
			++i;
		}
		else
			i = visibleDynamicItems.erase( i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDebrisController::Segment( NAI::IAIMap *pMap, SSphere *pInvalidate )
{
	bool bRes = false;
	if ( HasDynamicItems() )
	{
		list<STrackItem> stopped;
		InnerSegment( &stopped );
		for ( list<STrackItem>::iterator i = stopped.begin(); i != stopped.end(); ++i )
		{
			CDItem *pI = i->pItem;
			CDGPtr< CFuncBase<NAnimation::SSkeletonPose> > pPos = pI->GetAnimation();
			pPos.Refresh();
			SHMatrix pos;
			const NAnimation::SBonePose &bone = pPos->GetValue()[0];
			MakeMatrix( &pos, bone.pos, bone.rot );
			// retail @0x34b4b0 settle (disasm @0x74b5eb): bVisibleGated = (pI->GetVisibilityParent() != 0,
			// read via IVisible vtbl+8) -- the launch-time fog gate carried on the flying CDItem decides
			// the frozen item's show list + vision candidacy; bTemporaryVisible = 0 (push 0 @0x74b5f0).
			AddFrozenItem( pMap, pos, pI->GetInvItem(), pI->GetModel(), pI->GetFloor(), false, pI->GetVisibilityParent() != 0 );
			showItems.remove( pI );
			bc.Add( bone.pos, 1 );
		}
	}
	if ( !HasDynamicItems() )
	{
		pDebrisAction = 0;
		if ( !bc.IsEmpty() )
		{
			bc.Make( pInvalidate );
			bc.Clear();
			bRes = true;
		}
	}
	//
	for ( list< CPtr<CDFrozenItem> >::iterator i = pTrash->itemsToRemove.begin();
		i != pTrash->itemsToRemove.end(); ++i )
	{
		CDFrozenItem *pItem = *i;
		CPtr<NRPG::IInventoryItem> pInventoryItem = pItem->GetInvItem();
		if ( !IsValid( pInventoryItem ) )
			continue;
		//
		CDBPtr<NDb::CTEffect> pTEffect = pInventoryItem->GetDBItem()->pDestructionEffect;
		if ( IsValid( pTEffect ) )
		{
			SRand rand;
			CQuat rot = CQuat( random.GetFloat( 0, 10000 ), CVec3( 0, 0, 1 ) );
			CDBPtr<NDb::CEffect> pEffect = pTEffect->GetEffect( &rand );
			if ( IsValid( pEffect ) )
				CreateParticle( (*i)->GetPos(), rot, pEffect, pItem->GetFloor() );
		}
		//
		//OnFrozenItemDestroyed( (*i)->GetInvItem()->GetDBItem()->pSuccessor->GetRecordID() );
		OnFrozenItemDestroyed( (*i)->GetInvItem()->GetDBItem()->GetRecordID() );
		RemoveFrozenItem( (*i)->GetInvItem() );
	}
	pTrash->itemsToRemove.clear();
	//
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NWorld;
//REGISTER_SAVELOAD_CLASS( 0x12781170, CDebrisController );
//REGISTER_SAVELOAD_CLASS( 0x018C1120, CRealDebrisPiece )
REGISTER_SAVELOAD_CLASS( 0x009b1140, CDFrozenItem )
REGISTER_SAVELOAD_CLASS( 0x130A1191, CDItem )
REGISTER_SAVELOAD_CLASS( 0x50692130, CDebrisControllerTrash )