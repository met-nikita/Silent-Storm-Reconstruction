#include "StdAfx.h"
#include "wDebris.h"
#include "wInterface.h"
#include "GAnimParticles.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataGeometry.h"
#include "aiMap.h"
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
CDFrozenItem::CDFrozenItem( CSyncSrc<IVisObj> *pShow, const SItemRenderInfo &_model, const SHMatrix &_m,
	int _nFloor, NRPG::IInventoryItem *_pItem, CDebrisControllerTrash *_pTrash )
	: model(_model), m(_m), nFloor(_nFloor), pInvItem(_pItem), pTrash( _pTrash )
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
int CDFrozenItem::ProcessAttack( int nUserID, NRPG::CAttackPortion *pAttack, NDb::CRPGArmor *pArmor )
{
	if ( IsValid( pArmor ) && nVP > 0 )
	{
		int nDmg = pAttack->CalcStructDmg(pArmor);
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
const int N_VISIBLE_FLOOR = -5;
void CDebrisController::AddDebris( const SItemRenderInfo &_model, NAI::IAIMap *pAIMap, const CVec3 &ptCenter, const CQuat &q,
	const CVec3 &velocity, CFuncBase<STime> *pTime, NRPG::IInventoryItem *_pItem, CObjectBase *pVisibilityParent )
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
	NAnimation::CASphereSet *pSphere = new NAnimation::CASphereSet( N_VISIBLE_FLOOR );
	pSphere->pMap = pAIMap;
	pSphere->pTime = pTime;
	pSphere->InitSpheres( spheres );
	if ( _model.pModel )
		pSphere->InitBound( _model.pModel->pGeometry->boundCenter, _model.pModel->pGeometry->boundSize );
	else
		pSphere->InitBound( CVec3(0,0,0), CVec3(0.5f,0.5f,0.5f) );
	pSphere->Init( pTime->GetValue(), ptCenter, q, velocity, false );
	NAnimation::CSkeletonAnimator *pAnimator = new NAnimation::CSkeletonAnimator( 0 );
	pAnimator->pTime = pTime;
	pAnimator->AddAnimator( pTime->GetValue(), pSphere );

	// retail AddDebris @0x34ade0 selects the render sync by the CObjectBase* visibility-parent, SEPARATE
	// from the item: dropped inventory items (death loot) AND the item-less uniform cap bind to the
	// fog-of-war-gated GetVisibleShowList(); explosion/grenade debris (no item, no parent) stays on the
	// always-on list. The parent is stored on the CDItem so the gate SURVIVES physics-settle: retail
	// Segment @0x34b4b0 reads it back (IVisible vtbl+8) and forwards `parent != 0` to AddFrozenItem
	// @0x34b100 -- without it a fog-gated cap became unconditionally visible the moment it settled.
	CDItem *pI = new CDItem( ( _pItem || pVisibilityParent ) ? GetVisibleShowList() : GetShowList(), _model, pAnimator, N_VISIBLE_FLOOR, _pItem, pVisibilityParent ); // CRAP nFloor
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
		AddDebris( p->GetModel(), pAIMap, pos, QNULL, vel, pTime, p->GetInvItem() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CDFrozenItem* CDebrisController::AddFrozenItem( const SHMatrix &m, NRPG::IInventoryItem *pInvItem, const SItemRenderInfo &_model, int nFloor, bool bVisibleGated )
{
	// retail @0x34b100: `(pInvItem == 0 && !bVisibleGated) ? GetShowList() : GetVisibleShowList()`,
	// and the visibleItems (vision-candidate) publish runs when `pInvItem != 0 || bVisibleGated`.
	// bVisibleGated covers the item-less uniform cap of an UNSEEN unit: its frozen form must sit on
	// the fog-gated list AND be discoverable by CUnitServer::UpdateVisible's GetVisibleItems query,
	// so gaining LOS reveals it -- the bare `pInvItem != 0` test parked it on the always-on list.
	bool bIsVisibleItem = pInvItem != 0 || bVisibleGated;
	CDFrozenItem *pWorldItem = new CDFrozenItem(
		bIsVisibleItem ? GetVisibleShowList() : GetShowList(),
		_model, m, nFloor, pInvItem, pTrash );
	if ( bIsVisibleItem )
		visibleItems.push_back( pWorldItem );
	showFrozenItems.push_back( pWorldItem );
	return pWorldItem;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CDFrozenItem* CDebrisController::AddFrozenItem( const CVec3 &pos, const CQuat &rot, NRPG::IInventoryItem *pInvItem, int nFloor )
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
	CDFrozenItem *pWorldItem = AddFrozenItem( m, pInvItem, pInvItem->GetDBItem()->pModel->CreateModel( &rnd ), nFloor );
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
	for ( list<CPtr<CDFrozenItem> >::iterator i = visibleItems.begin(); i != visibleItems.end(); )
	{
		CDFrozenItem *p = *i;
		if ( IsValid(p) )
		{
			if ( fabs2( p->GetVisiblePos() - sphere.ptCenter ) <= sqr( sphere.fRadius ) )
				pRes->push_back( p );
			++i;
		}
		else
			i = visibleItems.erase( i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDebrisController::Segment( SSphere *pInvalidate )
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
			// retail @0x34b4b0 settle: bVisibleGated = (pI->GetVisibilityParent() != 0) -- the launch-time
			// fog gate carried on the flying CDItem decides the frozen item's show list + vision candidacy.
			AddFrozenItem( pos, pI->GetInvItem(), pI->GetModel(), pI->GetFloor(), pI->GetVisibilityParent() != 0 );
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