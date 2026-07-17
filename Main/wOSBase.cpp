#include "StdAfx.h"

#include "wOSBase.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataAnimation.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\DBFormat\DataSound.h"
#include "..\DBFormat\DataRPG.h"
#include "RPGObject.h"
#include "Transform.h"
#include "aiStability.h"
#include "aiInterval.h"
#include "wMain.h"
#include "wMisc.h"
#include "GAnimation.h"
#include "Grid.h"
#include "GSceneUtils.h"
#include "..\MiscDll\LogStream.h"

namespace NWorld
{
//////////////////////////`//////////////////////////////////////////////////////////////////////////
static bool IsOccluder( NDb::CModel *pModel )
{
	for ( int k = 0; k < NDb::N_MODEL_MATERIALS; ++k )
	{
		if ( pModel->pMaterials[k] == 0 )
			continue;
		NDb::CMaterial::EAlpha alpha = pModel->pMaterials[k]->alpha;
		if ( alpha == NDb::CMaterial::A_OPAQUE || alpha == NDb::CMaterial::A_SELF_ILLUM )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjectServerBase
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail ctor @0x3862e0: takes the element's NDb::ETimeOfDay and stores it into eLightActivity (@+104)
CObjectServerBase::CObjectServerBase( CWorld *_pWorld, const SObjectPlace &_pos, bool _bLightMap,
	NDb::CObject *_pDbO, NRPG::IObject *_pRPG, const vector<int> &_vCreateFlags, ETimeOfDay _eTimeOfDay, bool _bBorder )
	: pWorld(_pWorld), position(_pos), bLightMap(_bLightMap), vCreateFlags(_vCreateFlags), bBorder( _bBorder )
{
	pRPG = _pRPG;
	pDbObject = _pDbO;
	nDestroyStage = 0;
	eLightActivity = _eTimeOfDay;
	tStageChange = 0;
	tLastSound = pWorld->GetTime()->GetValue();
	bindGlobal.Link( pWorld->GetActive(), this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CObjectServerBase::GetDecalID()
{
	if ( !pDbObject )
		return 0;
	if ( pDbObject->bKeepDecals )
		return 0;
	return nDestroyStage;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CDebrisMaterial* CObjectServerBase::GetDebrisMaterial() const
{
	return pDbObject->pDebrisMaterial;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CObjectServerBase::IsTargetable() const
{
	// NULL-record hardening (same policy as GetDecalID above): retail @0x383b20 derefs unguarded,
	// but retail's data guarantees a live traced object always carries its record -- this tree can
	// produce record-less object servers (default-ctor save path / degenerate map objects), and the
	// cursor trace crashed on one (TraceCursor -> IsTargetable). No record == not a pick target.
	if ( !pDbObject )
		return false;
	return pDbObject->bTargetable;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CObjectServerBase::NeedSegment() const
{
	NDb::CContainerModel *pCont = pDbObject->pModels[nDestroyStage];
	if ( IsValid( pCont ) && pCont->eSoundType == NDb::ST_RANDOM && pCont->pSound )
		return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CObjectServerBase::CreateTransform( SFBTransform *pRes )
{
	if ( fabs2( position.ptScale ) == 0 )
		return false;
	SFBTransform &rv = *pRes;
	MakeMatrix( &rv, CVec3(1,1,1), position.ptPos, position.fAngle );
	rv = rv * MakeTransform( VNULL3, position.ptScale );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CObjectServerBase::GetCheckStabilityParams @0x383fe0: the (model, matrix) pair the
// stability checks/registration run on. False (no params) when the current destroy stage has no
// live model, the RPG object is dead, or no placement transform exists.
bool CObjectServerBase::GetCheckStabilityParams( NDb::CModel **ppModel, SHMatrix *pMatrix )
{
	NDb::CContainerModel *pCont = pDbObject->pModels[ pRPG->GetDestroyStage() ];
	if ( !pCont || !IsValid( pCont->pModel ) )
		return false;
	*ppModel = pCont->pModel;
	if ( pRPG->IsDead() )
		return false;
	SFBTransform rv;
	if ( !CreateTransform( &rv ) )
		return false;
	*pMatrix = rv.forward;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3850c0: an unstable object KILLS itself (its destruction spawns the debris); false
// tells the stability tracker to drop it from the tracked list.
bool CObjectServerBase::CheckStability()
{
	NDb::CModel *pModel;
	SHMatrix m;
	if ( !GetCheckStabilityParams( &pModel, &m ) )
		return true;
	if ( !NAI::CheckObjectStability( pCurrentWorld->GetAIMap(), pModel, m, this ) )
	{
		Kill( VNULL3 );
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CObjectServerBase::RegisterForStability @0x384060: register this static object's resting
// bound with the wreckage stability grid (only objects with live AI params qualify).
void CObjectServerBase::RegisterForStability( NAI::IStabilityTrackers *pTrackers )
{
	NDb::CModel *pModel;
	SHMatrix m;
	if ( GetCheckStabilityParams( &pModel, &m ) )
		pTrackers->AddObject( this, pModel, m );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x384100 -- play the CURRENT destroy stage's container-model destroy sound (CContainerModel::
// pDestroySound, +0x7c) at the object's position. Throttled: silent when the PREVIOUS stage change was
// less than 100 ticks ago (retail |now - tStageChange| > 99), so a rapid multi-stage cascade doesn't stack
// booms; callers re-stamp tStageChange AFTER calling this (cf. SetDestroyStage / ProcessAttack). Retail
// quirk kept faithfully: the loop walks the DB child chain but always reads THIS object's
// pModels[GetDestroyStage()] -- an N-child chain plays the same sound N times.
void CObjectServerBase::MakeDestroySound()
{
	int nSinceStageChange = int( GetWorld()->GetAimTime()->GetValue() - tStageChange );
	if ( abs( nSinceStageChange ) <= 99 )
		return;
	for ( NDb::CObject *pO = pDbObject; pO; pO = pO->pChild )
	{
		NDb::CContainerModel *pCont = pDbObject->pModels[ pRPG->GetDestroyStage() ];
		if ( pCont && IsValid( pCont->pDestroySound ) )
		{
			static SRand rnd;
			NDb::CSoundVariant *pSVar = pCont->pDestroySound->GetSound( &rnd );
			if ( IsValid( pSVar ) && IsValid( pSVar->pSound ) )
				pWorld->MakeSound( position.ptPos, pSVar->pSound );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectServerBase::SetDestroyStage( int nStage )
{
	// retail @0x384d40: the side effects run ONLY when the destroy stage actually changes, and tStageChange is
	// re-stamped so GetTimeSinceLastStageChange()'s 300-tick throttle re-arms. CheckItemsBreakGlass relies on that
	// throttle; the aim clock is constant within one physics step, so without the stamp a breakable pane spanning
	// many triangles would re-bump on every contact and jump straight to its max stage in a single frame. Stamp
	// with GetWorld()->GetAimTime()->GetValue() -- the SAME clock GetTimeSinceLastStageChange compares against
	// (cf. Kill / ProcessAttack). Retail also plays the new stage's destroy sound (MakeDestroySound @0x384100,
	// BEFORE the re-stamp so the throttle compares against the PREVIOUS change) and bumps the world's action
	// counter -- this is what makes a SCRIPTED stage change audible (e.g. the GFirst bank-safe blast, zone
	// script 95: ObjectSetDestroyStage(safe,1) -- the stage-1 effect was visible but the boom never played).
	int nPrevStage = pRPG->GetDestroyStage();
	pRPG->SetDestroyStage( nStage );
	if ( nPrevStage != pRPG->GetDestroyStage() )
	{
		MakeDestroySound();
		tStageChange = GetWorld()->GetAimTime()->GetValue();
		// retail @0x384d40 (disasm-verified): the bumped action counter is held in a SCOPED AddRef/Release
		// pair (nObjData+1 ... CObjectBase::ReleaseObj on function exit) -- the lasting effect is only the
		// 10-segment action LAG; an otherwise-unowned counter DIES at scope end. The previous bare call
		// leaked an unowned CActionCounter (refcount never reaches zero) that pinned the world's weak
		// pActiveCount forever -> IsAction() stuck true with no performing unit -> the TBS pump never
		// fetched another command (the GFirst first-enemy-turn hang; the safe's scripted
		// ObjectSetDestroyStage runs exactly this line).
		CObj<CActionCounter> pStageAction = GetWorld()->GetActiveCounter( 10 );
		bindGlobal.Update();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CObjectServerBase::GetHP()
{
	return pRPG->GetHP();   // delegates to the RPG-side object (CObject::nVP)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CObjectServerBase::GetDestroyStage()
{
	return pRPG->GetDestroyStage();   // RPG object is the source of truth for the destroy stage
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// wOSBase.obj @0x383c00 -- ticks elapsed since this object last changed destroy stage, measured on the
// same clock that stamps tStageChange. The retail reads pWorld->GetWorldTime() (IWorld vtbl slot 5);
// in the dev tree that override is protected (CDebrisController) and tStageChange is stamped from
// GetAimTime()->GetValue() (see SetDestroyStage / ProcessAttack / Kill), so use the same accessible clock.
int CObjectServerBase::GetTimeSinceLastStageChange()
{
	return int( GetWorld()->GetAimTime()->GetValue() - tStageChange );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// wCheckGlassGrenade.obj @0x347bd0 -- grenade-vs-breakable-glass collider gate. Returns true to keep the
// hit solid (the normal case); for armor flagged bBreakableByGrenade on a live CObjectServerBase it
// returns false (let the blast pass through the glass) and, no more than once per 300 ticks, advances the
// object's destroy stage. ORIGINAL behaviour (confirmed @0x347bd0): any null / dead / non-castable lookup
// falls back to "solid" (return true). The two release `& 0x80000000` guards are the loaded-and-alive
// checks reproduced here via IsValid().
bool CheckItemsBreakGlass( const NAI::SSourceInfo *pSrc )
{
	if ( pSrc == 0 )
		return true;
	NDb::CRPGArmor *pArmor = pSrc->pArmor.GetPtr();
	if ( !IsValid( pArmor ) )                       // armor must be loaded and not being deleted
		return true;
	if ( !pArmor->bBreakableByGrenade )
		return true;
	CObjectServerBase *pObj = dynamic_cast<CObjectServerBase*>( pSrc->pUserData.GetPtr() );
	if ( !IsValid( pObj ) )                         // catcher must dynamic_cast to a live object-server
		return true;
	if ( pObj->GetTimeSinceLastStageChange() > 300 )
		pObj->SetDestroyStage( pObj->GetDestroyStage() + 1 );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x384ea0 -- re-light the object: re-sync its global vis binding so the renderer re-evaluates
// the object's appearance for the (changed) world time-of-day. The retail first walks the model's node
// light list and only re-syncs when a node carries a non-trivial light; that per-node-light filter is an
// OPTIMISATION (it only skips the re-sync for unlit objects), so it is elided here -- the bindGlobal.Update()
// re-sync is the dev's established object-refresh idiom (cf. SetDestroyStage / SetPosition).
void CObjectServerBase::UpdateLight()
{
	bindGlobal.Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail BeAddedToVisitiors [sic] (release-added; oracle src/s2_nscript_scriptobject.h -- the object-
// pocket pair calls it with false on pocketing and true on restore): (dis)connect the object's global
// vis binding, so a pocketed object stops being visited by the render/AI/sound sync consumers.
void CObjectServerBase::BeAddedToVisitiors( bool bAdd )
{
	if ( bAdd )
		bindGlobal.Link( pWorld->GetActive(), this );
	else
		bindGlobal.Unlink();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectServerBase::Kill( const CVec3 &ptDir )
{
	//pCurrentWorld->AddActionLocator( new CActionLocator( CActionLocator::TYPE_DIE, this, position.ptPos ), 1000 );
	/*
	pCurrentWorld->GenerateDebris( GetDebrisMaterial(), position.ptPos, ptDir, 5 );
	pCurrentWorld->ActivateDebris( SSphere( position.ptPos, 5 ), pCurrentWorld->GetAIMap(), pCurrentWorld->GetTime() ); // CRAP
	pCurrentWorld->KillObject( this );
	*/
	int nPrevStage = pRPG->GetDestroyStage();
	pRPG->Kill();
	if ( nPrevStage != pRPG->GetDestroyStage() )
	{
		tStageChange = pCurrentWorld->GetAimTime()->GetValue();
		bindGlobal.Update();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectServerBase::SetPosition( const SObjectPlace &pos )
{
	position = pos;
	bindGlobal.Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x372170 (IObject vtable slot +0x10) -- pure setter: arm the grenade this object will detonate on
// a LATER ProcessAttack (path A). The CDBPtr assignment is exactly the AddRef(new)/ReleaseRef(old) retail does by hand.
void CObjectServerBase::AttachExplosion( NDb::CRPGGrenade *pGrenade )
{
	pAttachedGrenade = pGrenade;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x385130: forwards the incoming world to the NRPG side -- the seam that carries pWorld
// down to CObject::ProcessAttack @0x2ad3f0. AddGrenadeExplosion below goes to this->pWorld
// (member 72, disasm `mov ecx,[esi+0x40]`), so the param must not shadow it.
int CObjectServerBase::ProcessAttack( NWorld::IWorld *_pWorld, int nUserID, NRPG::CAttackPortion *pAttack,
	const CVec3 &vDir, NDb::CRPGArmor *pArmor )
{
	if ( !IsValid( this ) )
		return false;
	CDynamicCast<NRPG::IAttackable> pAtk( pRPG );
	ASSERT( pAtk );
	int nPrevStage = pRPG->GetDestroyStage();
	int nRes = pAtk->ProcessAttack( _pWorld, nUserID, pAttack, vDir, pArmor );
	if ( nPrevStage != pRPG->GetDestroyStage() )
	{
		// retail @0x785xxx stage-change block: MakeDestroySound @0x384100 (the throttled helper -- a burst of
		// hits landing within 100 ticks plays ONE boom), then re-stamp the clock + counter + vis re-sync.
		MakeDestroySound();
		/*
		if ( pRPG->IsDead() )
			Kill( ptDir );
		else
		{
			pCurrentWorld->GenerateDebris( GetDebrisMaterial(), position.ptPos, VNULL3, 1 );
			//pCurrentWorld->AttachMiscObject( new C3DSound( ptPlace, NDb::GetSound(8) ) );
			tStageChange = pCurrentWorld->GetAimTime()->GetValue();
			bindGlobal.Update();
		}
		*/
		tStageChange = pCurrentWorld->GetAimTime()->GetValue();
		// retail stage-change block: scoped counter hold, same as SetDestroyStage @0x384d40 -- see the
		// comment there (a bare GetActiveCounter leaks an unowned counter and pins IsAction() forever).
		CObj<CActionCounter> pStageAction = pCurrentWorld->GetActiveCounter( 10 );
		bindGlobal.Update();
	}
	// (A) retail @0x785242 -- detonate a grenade already ARMED on this object. NOTE this is deliberately
	// OUTSIDE the stage-change guard above: retail's `je` on an unchanged stage skips only the sound/counter/
	// bind block and falls straight through to here, so A fires on the first ProcessAttack that finds a grenade
	// attached, gated only by IsValid( pAttachedGrenade ). The object is its own igniter -- CastToObjectBase( this )
	// gives the 10x self-damage (cf. CWindowDoor::GoBoom, wObject.cpp:282) that finishes it and chains to neighbours.
	// Retail's 5-arg producer passes SPerkMineModifiers{1,1,0} = the no-perk default the 4-arg overload seeds
	// inside CVoxelExplTracker, so it is omitted here.
	if ( IsValid( pAttachedGrenade ) )
	{
		pWorld->AddGrenadeExplosion( position.ptPos, pAttachedGrenade, 0, CastToObjectBase( this ) );
		pAttachedGrenade = 0;   // retail 0x785299 -- clear the member after detonation (ReleaseRef via CDBPtr)
	}
	// (B) retail @0x7852a7 -- the destroy stage just changed and nothing is armed yet: ARM from the new stage's
	// container model so a SUBSEQUENT ProcessAttack (path A) detonates it. Gated (faithful to retail's
	// cmp ebp,stage / cmp [member],0) on a fresh stage change AND a raw-null member -- distinct from path A's IsValid.
	if ( nPrevStage != pRPG->GetDestroyStage() && !pAttachedGrenade )
	{
		NDb::CContainerModel *pCont = pDbObject->pModels[pRPG->GetDestroyStage()];
		if ( pCont && pCont->pAttachedGrenade )
			AttachExplosion( pCont->pAttachedGrenade );
	}
	return nRes;
/*	else if ( CDynamicCast<CWObject> pObj(pTarget) )
	{
		CObjectServer *pObjectServer = GetObject( pObj );
		if ( pObjectServer->IsValid() )
		{
			if ( pObjectServer->GetAttackable()->IsDead() )
			{
				pCurrentWorld->AddActionLocator( new CActionLocator( CActionLocator::TYPE_DIE, pObj, pObj->aiPos.pos.GetCP() ), 1000 );
				KillObject( pObjectServer, ptDir );
			}
			//else if ( pObjectServer->IsTargetable() )
			//{
				// TEMPORARILY //
				//KillObject( pObjectServer, NRPG::GetDir(ray) );
				//Explode( pObj->position.forward.GetTranslation(), 1000 );
			//}
			else
				pCurrentWorld->Add3DSound( NDb::GetSound(8),
					new NGScene::CCFBTransform( MakeTransform( ptPlace, 0 ) ), 0 );
		}
	}*/
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectServerBase::AddLights( IRenderVisitor *p, NDb::CContainerModel *pCont, const SFBTransform &rv )
{
	if ( !pCont )
		return;
	// retail @0x383d50: light emission is gated by the object's time-of-day activity window --
	// active iff the object is ANYTIME, OR the world is ANYTIME, OR they match.
	ETimeOfDay tod = pWorld->GetTimeOfDay();
	if ( !( eLightActivity == TOD_ANYTIME || tod == TOD_ANYTIME || tod == eLightActivity ) )
		return;
	if ( pCont->ptPLightCr != VNULL3 )
	{
		SFBTransform m;
		CVec3 ptOrigin;
		rv.forward.RotateHVector( &ptOrigin, pCont->ptPLightPos );
		p->AddPointLight( pCont->ptPLightCr, ptOrigin, pCont->fPLightRadius, bLightMap );
		if ( pCont->fPFlareRadius > 0 )
		{
			CVec3 ptFlareOrigin;
			rv.forward.RotateHVector( &ptFlareOrigin, pCont->ptPLightFlarePos );
			p->AddFlare( new NGScene::CCVec3(ptFlareOrigin), pCont->fPFlareRadius, pCont->pPFlareTexture, position.nFloor );
		}
	}
	if ( pCont->ptSLightCr != VNULL3 )
	{
		CVec3 ptOrigin;
		rv.forward.RotateHVector( &ptOrigin, pCont->ptSLightPos );
		p->AddSpotLight( pCont->ptSLightCr, ptOrigin, pCont->ptSLightDir, pCont->fSLightFOV, 
			pCont->fSLightRadius, pCont->pSLightMask, false );//pCont->bLightmapOnly );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectServerBase::AddEffects( IRenderVisitor *p, NDb::CContainerModel *pCont, const SFBTransform &rv )
{
	if ( pCont->pEffect )
	{
		SFBTransform tr;
		tr = rv * MakeTransform( pCont->ptEffectPos, CVec3(1,1,1 ) );
		p->AddParticleEffect( tStageChange, pCont->pEffect, position.nFloor, tr );
		for ( int i = 0; i < pCont->pEffect->instances.size(); ++i )
		{
			NDb::CParticle *pParticle = pCont->pEffect->instances[i]->pParticle;
			if ( IsValid( pParticle ) )
				p->AddOccluder( pParticle->pAIGeometry, rv, position.nFloor );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectServerBase::AddObject( IRenderVisitor *p, NDb::CObject *pO, const SFBTransform &rv )
{
	NDb::CContainerModel *pCont = pO->pModels[nDestroyStage];
	if ( pCont )
	{
		NDb::CModel *pModel = pCont->pModel;
		if ( pModel )
		{
			p->AddMesh( pModel, rv, 0, position.nFloor, GetDecalID() );
			if ( pModel->pGeometry && IsOccluder( pModel ) && ( pModel->pRPGArmor == 0 || pModel->pRPGArmor->pMaterial || pModel->pRPGArmor->pMaterial->fTransparency < 0.01f ) )
			{
				NDb::CGeometry *pGeom = pModel->pGeometry;
				if ( pGeom->pAIGeometry )
					p->AddOccluder( pGeom->pAIGeometry, rv, position.nFloor );
				if ( pGeom->pAIGeometry2 )
					p->AddOccluder( pGeom->pAIGeometry2, rv, position.nFloor );
			}
		}
		AddLights( p, pCont, rv );
		AddEffects( p, pCont, rv );
	}
	for ( int k = nDestroyStage + 1; k < NDb::N_DESTROY_STAGES; ++k )
	{
		if ( NDb::CContainerModel *pCont = pO->pModels[k] )
			p->LoadGeometry( pCont->pModel );
	}
	if ( IsValid( pO->pChild ) )
		AddObject( p, pO->pChild, rv );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectServerBase::Visit( IRenderVisitor *p )
{
	nDestroyStage = pRPG->GetDestroyStage();
	SFBTransform rv;
	if ( !CreateTransform( &rv ) )
		return;
	AddObject( p, pDbObject, rv );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x783c30: TS_COVER rides TR_DAMAGE (TR_COVER is never read); fully transparent
// armor (fTransparency == 1) suppresses TS_VISION entirely; TR_LADDER -> TS_LADDER_PART.
int GetMask( NDb::CAIGeometry *pAIGeometry, NDb::CRPGArmor *pArmor )
{
	if ( !pAIGeometry )
		return TS_VIRTUAL;
	int nMask = ( pAIGeometry->traficability & NDb::TR_DAMAGE ) ? ( TS_FRAGMENTED|TS_COVER ) : TS_VIRTUAL;
	if ( pAIGeometry->traficability & NDb::TR_VISION )
	{
		float fTransparency = 0;
		if ( pArmor && pArmor->pMaterial )
			fTransparency = pArmor->pMaterial->fTransparency;
		if ( fTransparency != 1.0f )
		{
			nMask |= TS_VISION;
			if ( fTransparency > 0 )
				nMask |= TS_VISION_SOLID;
		}
	}
	if ( pAIGeometry->traficability & NDb::TR_PASS )
		nMask |= TS_PASS_BLOCKER|TS_WEAPON_BLOCKER;
	if ( pAIGeometry->traficability & NDb::TR_ITEM_BLOCKER )
		nMask |= TS_ITEM_BLOCKER;
	if ( pAIGeometry->traficability & NDb::TR_LADDER )
		nMask |= TS_LADDER_PART;
	return nMask;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectServerBase::AddEffects( IAIVisitor *p, NDb::CContainerModel *pCont, const SFBTransform &rv )
{
	if ( !pCont )
		return;
	NDb::CEffect *pEffect = 0;
	if ( IsValid( pCont->pEffect ) )
		pEffect = pCont->pEffect;
	if ( IsValid( pEffect ) )
	{
		for ( int i = 0; i < pEffect->instances.size(); ++i )
		{
			NDb::CParticle *pParticle = pEffect->instances[i]->pParticle;
			if ( IsValid( pParticle ) && IsValid( pParticle->pAIGeometry ) )
			{
				int nMask = GetMask( pParticle->pAIGeometry, pParticle->pRPGArmor );
				p->AddHull( pParticle->pAIGeometry, rv, pParticle->pRPGArmor, position.nFloor, nMask );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectServerBase::PrecacheEffects( IAIVisitor *p, NDb::CObject *pO )
{
	for ( int k = nDestroyStage + 1; k < NDb::N_DESTROY_STAGES; ++k )
	{
		if ( NDb::CContainerModel *pCont = pO->pModels[k] )
		{
			// effects
			NDb::CEffect *pEffect = pCont->pEffect;
			if ( IsValid( pEffect ) )
			{
				for ( int i = 0; i < pEffect->instances.size(); ++i )
				{
					NDb::CParticle *pParticle = pEffect->instances[i]->pParticle;
					if ( IsValid( pParticle ) && IsValid( pParticle->pAIGeometry ) )
						p->LoadGeometry( pParticle->pAIGeometry );
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectServerBase::AddObjectHull( IAIVisitor *pVisitor, 
	NDb::CAIGeometry *pGeometry, const SFBTransform &rv, NDb::CRPGArmor *pArmor, int nFloor )
{
	if ( IsValid( pGeometry ) )
	{
		int nMask = GetMask( pGeometry, pArmor );
		if ( !bBorder )
			nMask |= TS_PICK;
		pVisitor->AddHull( pGeometry, rv, pArmor, nFloor, nMask );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectServerBase::AddObject( IAIVisitor *p, NDb::CObject *pO, const SFBTransform &rv )
{
	NDb::CContainerModel *pCont = pO->pModels[nDestroyStage];
	if ( pCont )
	{
		NDb::CModel *pModel = pCont->pModel;
		if ( pModel && pModel->pGeometry )
		{
			NDb::CGeometry *pGeom = pModel->pGeometry;
			AddObjectHull( p, pGeom->pAIGeometry, rv, pModel->pRPGArmor, position.nFloor );
			AddObjectHull( p, pGeom->pAIGeometry2, rv, pModel->pRPGArmor, position.nFloor );
		}
		AddEffects( p, pCont, rv );
	}
	for ( int k = nDestroyStage + 1; k < NDb::N_DESTROY_STAGES; ++k )
	{
		if ( NDb::CContainerModel *pCont = pO->pModels[k] )
		{
			// geometry
			NDb::CModel *pModel = pCont->pModel;
			if ( pModel && pModel->pGeometry && pModel->pGeometry )
			{
				p->LoadGeometry( pModel->pGeometry->pAIGeometry );
				p->LoadGeometry( pModel->pGeometry->pAIGeometry2 );
			}
		}
	}
	PrecacheEffects( p, pO );
	if ( IsValid( pO->pChild ) )
		AddObject( p, pO->pChild, rv );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectServerBase::Visit( IAIVisitor *p )
{
	nDestroyStage = pRPG->GetDestroyStage();
	SFBTransform rv;
	if ( !CreateTransform( &rv ) )
		return;
	AddObject( p, pDbObject, rv );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectServerBase::AddObject( ISoundVisitor *p, NDb::CObject *pO, const SFBTransform &rv )
{
	static SRand rnd;
	NDb::CContainerModel *pCont = pO->pModels[nDestroyStage];
	if ( pCont && pCont->eSoundType == NDb::ST_PERMANENT && pCont->pSound )
	{
		CVec3 ptOrigin;
		rv.forward.RotateHVector( &ptOrigin, pCont->ptSoundPos );
		NDb::CSoundVariant *pSVar = pCont->pSound->GetSound( &rnd );
		if ( IsValid( pSVar ) )
			p->Add3DSound( 0, pSVar->pSound, new NGScene::CCVec3( ptOrigin ) );
	}
	if ( pCont && pCont->pSoundEffect )
	{
		CVec3 ptOrigin;
		rv.forward.RotateHVector( &ptOrigin, pCont->ptSoundPos );
		STime t = pWorld->GetTime()->GetValue();
		p->AddEffect( t, pCont->pSoundEffect, new NGScene::CCVec3( ptOrigin ), vCreateFlags );
	}
	if ( IsValid( pO->pChild ) )
		AddObject( p, pO->pChild, rv );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectServerBase::Visit( ISoundVisitor *p )
{
	SFBTransform rv;
	if ( !CreateTransform( &rv ) )
		return;
	AddObject( p, pDbObject, rv );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CObjectServerBase::Segment()
{
	NDb::CContainerModel *pCont = pDbObject->pModels[nDestroyStage];
	if ( pCont && pCont->eSoundType == NDb::ST_RANDOM && pCont->pSound )
	{
		static SRand rnd;
		STime t = pWorld->GetTime()->GetValue();
		float fInterval = float( t - tLastSound ) / 1000.0f;
		float fAvrgInterval = Max( 1.0f, pCont->fSoundAvgInterval );
		if ( fInterval > rnd.GetFloat( 0.5f * fAvrgInterval, 20.0f * fAvrgInterval ) )
		{
			char buf[256];
			sprintf( buf, "Random sound interval=%f, current=%f\n", pCont->fSoundAvgInterval, fInterval );
			//OutputDebugString( buf );
			NDb::CSoundVariant *pSVar = pCont->pSound->GetSound( &rnd, vCreateFlags );
			if ( IsValid( pSVar ) && IsValid( pWorld ) )
			{
				CVec3 ptOrigin;
				SFBTransform rv;
				if ( CreateTransform( &rv ) )
				{
					rv.forward.RotateHVector( &ptOrigin, pCont->ptSoundPos );
					pWorld->MakeSound( ptOrigin, pSVar->pSound );
				}
			}
			tLastSound = t;
		}
	}
	return !NeedSegment();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CObjectServerBase::GetDBObjectID() const
{ 
	return pDbObject->nParentID; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAnimObjectServerBase
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail ctor @0x386510: eTimeOfDay appended after vCreateFlags, forwarded to the base ctor
CAnimObjectServerBase::CAnimObjectServerBase( CWorld *pWorld, const SObjectPlace &pos, bool bLightMap,
	NDb::CObject *pO, NRPG::IObject *pRPG, CFuncBase<STime> *_pTime, const vector<int> &vCreateFlags, ETimeOfDay eTimeOfDay ) :
	CObjectServerBase( pWorld, pos, bLightMap, pO, pRPG, vCreateFlags, eTimeOfDay ), pTime(_pTime)
{
	NDb::CContainerModel *pCont = pDbObject->pModels[nDestroyStage];
	ASSERT(pCont);
	ASSERT( IsValid( pCont->pModel ) );
	ASSERT( IsValid( pCont->pModel->pSkeleton ) );
	pSkeleton = pCont->pModel->pSkeleton;
	pAnimator = new NAnimation::CSkeletonAnimator( pSkeleton );
	pAnimator->pTime = pTime;
	pState = new NAnimation::CSkeletonState;
	pState->pAnimator = pAnimator;
	pState->pTime = _pTime;
	pState->state.nAnimFlagsPoseWeapon  = 0;
	pState->state.nAnimFlagsClassSex  = 0;
	pState->state.pos = pos.ptPos;
	pState->state.fAngle = pos.fAngle;
	pState->state.cIdleBannedFlags = 0;

	STime t = pTime->GetValue();
	CPtr<NAnimation::CAnimation> pAnim = pAnimator->CreateAnimation(
		pSkeleton->GetAnimation( NDb::CAnimation::POSE, 0 ), t );
	if ( pAnim )
	{
		pAnim->SetStand( t, pos.ptPos, pos.fAngle );
		pAnimator->AddAnimator( t, pAnim );
		pAnimator->AddMemorizer( t );
	}
	IdleOn();
	PrecacheAnimations();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimObjectServerBase::IdleOn()
{
	pState->state.cIdleBannedFlags &= ~NAnimation::E_INTERNAL_IDLE_OFF;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimObjectServerBase::IdleOff()
{
	pState->state.cIdleBannedFlags |= NAnimation::E_INTERNAL_IDLE_OFF;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimObjectServerBase::GetApproachPts( vector<CVec3> *pRes ) const
{
	pRes->clear();
	char pszName[16];
	for ( int i = 1; i < 10; ++i )
	{
		sprintf( pszName, "Unit%d", i );
		int nIndex = pAnimator->GetBoneIndex( pszName );
		if ( nIndex < 0 )
			break;
		CDGPtr<CFuncBase<NAnimation::SSkeletonPose> > pToUpdate = pAnimator;
		pToUpdate.Refresh();

		NAnimation::SBonePose bone;
		//pAnimator->GetCurrentBonePos( nIndex, &bone );
		if ( CDynamicCast<ICannon>( this ) )
			pAnimator->GetCurrentBonePos( nIndex, &bone );
		else
		{
			pAnimator->GetDefaultBonePos( nIndex, &bone );
			CQuat rot( position.fAngle, CVec3(0,0,1) );
			bone.pos = rot.Rotate(bone.pos) + position.ptPos;
			bone.rot = rot * bone.rot;
		}
		pRes->push_back( bone.pos );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimObjectServerBase::GetApproaches( vector<NAI::SPathPlace> *pRes, NAI::IPathNetwork *pNet ) const
{
	pRes->clear();
	char pszName[16];
	vector<NAI::SPathPlace> tmp;
	for ( int i = 1; i < 10; ++i )
	{
		sprintf( pszName, "Unit%d", i );
		int nIndex = pAnimator->GetBoneIndex( pszName );
		if ( nIndex < 0 )
			break;
		CDGPtr<CFuncBase<NAnimation::SSkeletonPose> > pToUpdate = pAnimator;
		pToUpdate.Refresh();

		NAnimation::SBonePose bone;
		//pAnimator->GetCurrentBonePos( nIndex, &bone );
		if ( CDynamicCast<ICannon>( this ) )
			pAnimator->GetCurrentBonePos( nIndex, &bone );
		else
		{
			pAnimator->GetDefaultBonePos( nIndex, &bone );
			CQuat rot( position.fAngle, CVec3(0,0,1) );
			bone.pos = rot.Rotate(bone.pos) + position.ptPos;
			bone.rot = rot * bone.rot;
		}

		CVec3 axis = bone.rot.GetXAxis();
		float fAngle = atan2( axis.y, axis.x );
		vector<NAI::SPathPlace> places;
		pNet->GetNearPlaces( SSphere( bone.pos, FP_GRID_STEP * 3 ), &places );
		float fMin;
		int nMin = -1;
		NAI::SPathPlace res;
		for ( int k = 0; k < places.size(); ++k )
		{
			NAI::SPathPlace p;
			p = places[k];
			p.SetPose( NAI::CM_STAND );
			p.SetDirection( pNet->GetClosestDir( p.GetLayer(), fAngle ) );
			if ( !pNet->IsNativePassable( p ) )
			{
				p.SetPose( NAI::CM_CROUCH );
				if ( !pNet->IsNativePassable( p ) )
					continue;
			}
			NAI::SPosition pos;
			pos.SetNetwork( pNet );
			pos.p = p;
			CVec3 vDiff = bone.pos - pos.GetCP();
			float fDist = vDiff.x * vDiff.x + vDiff.y * vDiff.y;
			float fVertDist = vDiff.z;
			if ( fVertDist < -0.25f )
				continue; 
			if ( nMin < 0 || fDist < fMin )
			{
				fMin = fDist;
				nMin = k;
				res = p;	
				if ( fVertDist < 0.75f )
					res.SetPose( NAI::CM_CROUCH );
			}
		}
		if ( nMin >= 0 )
			tmp.push_back( res );
	}
	for ( int i = 0; i < tmp.size(); ++i )
	{
		pRes->push_back( tmp[i] );
		NAI::SPathPlace p = tmp[i];
		if ( p.GetPose() == NAI::CM_CROUCH )
			continue;
		p.SetPose( NAI::CM_CROUCH );
		if ( pNet->IsNativePassable( p ) )
			pRes->push_back( p );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimObjectServerBase::AddEffects( IRenderVisitor *p, NDb::CContainerModel *pCont, const SFBTransform &rv )
{
	if ( pCont->pEffect )
	{
		SFBTransform tr;
		tr = rv * MakeTransform( pCont->ptEffectPos, CVec3(1,1,1 ) );
		p->AddParticleEffect( tStageChange, pCont->pEffect, position.nFloor, new NGScene::CCFBTransform(tr), pAnimator );
		for ( int i = 0; i < pCont->pEffect->instances.size(); ++i )
		{
			NDb::CParticle *pParticle = pCont->pEffect->instances[i]->pParticle;
			if ( IsValid( pParticle ) )
				p->AddOccluder( pParticle->pAIGeometry, rv, position.nFloor );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimObjectServerBase::Visit( IRenderVisitor *p )
{
	nDestroyStage = pRPG->GetDestroyStage();
	SFBTransform rv;
	if ( !CreateTransform( &rv ) )
		return;

	NDb::CContainerModel *pCont = pDbObject->pModels[nDestroyStage];
	if ( pCont )
	{
		NDb::CModel *pModel = pCont->pModel;
		if ( pModel )
		{
			if ( pModel->pSkeleton == pSkeleton )
			{
				vector<IRenderVisitor::SBoundMesh> boundMeshes;
				p->AddMesh( pModel, pAnimator, pState, boundMeshes, 0, position.nFloor, 0, GetDecalID() );
				PlayAnimation();
				if ( pModel->pGeometry && IsOccluder( pModel ) && pModel->pRPGArmor && pModel->pRPGArmor->pMaterial->fTransparency < 0.01f )
				{
					NDb::CGeometry *pGeom = pModel->pGeometry;
					if ( pGeom->pAIGeometry )
						p->AddOccluder( pGeom->pAIGeometry, pModel->pSkeleton, pAnimator, position.nFloor );
					if ( pGeom->pAIGeometry2 )
						p->AddOccluder( pGeom->pAIGeometry2, pModel->pSkeleton, pAnimator, position.nFloor );
				}
			}
			else
			{
				p->AddMesh( pModel, rv, 0, position.nFloor, GetDecalID() );
			}
		}
		AddLights( p, pCont, rv );
		AddEffects( p, pCont, rv );
	}
	for ( int k = nDestroyStage + 1; k < NDb::N_DESTROY_STAGES; ++k )
	{
		if ( NDb::CContainerModel *pCont = pDbObject->pModels[k] )
			p->LoadGeometry( pCont->pModel );
	}
	
	if ( IsValid( pDbObject->pChild ) )
		AddObject( p, pDbObject->pChild, rv );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimObjectServerBase::PrecacheAIGeom( IAIVisitor *p )
{
	for ( int k = nDestroyStage + 1; k < NDb::N_DESTROY_STAGES; ++k )
	{
		if ( NDb::CContainerModel *pCont = pDbObject->pModels[k] )
		{
			NDb::CModel *pModel = pCont->pModel;
			if ( !pModel )
				continue;
			NDb::CGeometry *pGeometry = pModel->pGeometry;
			if ( !pGeometry )
				continue;
			if ( pModel->pSkeleton == pSkeleton )
				p->LoadSkinGeometry( pModel->pGeometry->pAIGeometry, pSkeleton );
			else
				p->LoadGeometry( pModel->pGeometry->pAIGeometry );
			if ( IsOccluder( pModel ) )
			{
				p->LoadSkinGeometry( pGeometry->pAIGeometry, pModel->pSkeleton );
				p->LoadSkinGeometry( pGeometry->pAIGeometry2, pModel->pSkeleton );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimObjectServerBase::Visit( IAIVisitor *p )
{
	nDestroyStage = pRPG->GetDestroyStage();
	SFBTransform rv;
	if ( !CreateTransform( &rv ) )
		return;

	NDb::CContainerModel *pCont = pDbObject->pModels[nDestroyStage];
	if ( pCont )
	{
		NDb::CModel *pModel = pCont->pModel;
		if ( pModel && pModel->pGeometry )
			AddObjectHull( p, pModel->pGeometry->pAIGeometry, rv, pModel->pRPGArmor, position.nFloor );
		CObjectServerBase::AddEffects( p, pCont, rv );
	}
	PrecacheAIGeom( p );
	PrecacheEffects( p, pDbObject );
	
	if ( IsValid( pDbObject->pChild ) )
		AddObject( p, pDbObject->pChild, rv );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAnimObjectServerBase::Segment()
{
	if ( IsValid(pAction) )
	{
		STime tCur = pTime->GetValue();
		if ( tCur > tEnd )
		{
			pAction = 0;
			IdleOn();
		}
		else
			return false;
	}
	return CObjectServerBase::Segment();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimObjectServerBase::SetPosition( const SObjectPlace &pos )
{
	CObjectServerBase::SetPosition( pos );
	PlayAnimation( true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimObjectServerBase::PlayCustomAnimation( NAnimation::CAnimation *pAnimation, bool bInstantly )
{
	ASSERT( IsValid( pAnimation ) );
	if ( !IsValid( pAnimation ) )
		return;
	//
	IdleOff();
	STime t = pTime->GetValue();
	if ( bInstantly )
		pAnimation->SetInterval( t, t + 1 );
	pAnimation->SetStand( t, position.ptPos, position.fAngle );
	pAnimator->AddAnimator( t, pAnimation );
	pAnimator->AddMemorizer( t + pAnimation->GetTime() );
	tEnd = t + pAnimation->GetTime();
	pAction = pWorld->GetActiveCounter( 10 );
	pWorld->RegisterObjectForSegment( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimObjectServerBase::PlayDBAnimation( int nDBAnimationID )
{
	CPtr<NAnimation::CAnimation> pAnimation = 
		pAnimator->CreateAnimation( NDb::GetDBAnimation( nDBAnimationID ) , pTime->GetValue() );
	if ( IsValid( pAnimation ) )
		PlayCustomAnimation( pAnimation );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimObjectServerBase::PrecacheAnimations()
{
	destroyAnimations.push_back( NAnimation::PrecacheAnimation( pSkeleton->GetAnimation( NDb::CAnimation::DESTRUCT_1, 0 ) ) );
	destroyAnimations.push_back( NAnimation::PrecacheAnimation( pSkeleton->GetAnimation( NDb::CAnimation::DESTRUCT_2, 0 ) ) );
	destroyAnimations.push_back( NAnimation::PrecacheAnimation( pSkeleton->GetAnimation( NDb::CAnimation::DESTRUCT_3, 0 ) ) );
	destroyAnimations.push_back( NAnimation::PrecacheAnimation( pSkeleton->GetAnimation( NDb::CAnimation::DESTRUCT_4, 0 ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimObjectServerBase::PlayAnimation( bool bInstantly )
{
	NDb::CAnimation::EType type;
	if ( bInstantly )
	{
		type = NDb::CAnimation::POSE;
	}
	else
	{
		switch ( nDestroyStage )
		{
			case 1: type = NDb::CAnimation::DESTRUCT_1; break;
			case 2: type = NDb::CAnimation::DESTRUCT_2; break;
			case 3: type = NDb::CAnimation::DESTRUCT_3; break;
			case 4: type = NDb::CAnimation::DESTRUCT_4; break;
			default: return;
		}
	}
	//
	CPtr<NAnimation::CAnimation> pAnim = pAnimator->CreateAnimation(
		pSkeleton->GetAnimation( type, 0 ), pTime->GetValue() );
	PlayCustomAnimation( pAnim, bInstantly );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAnimObjectServerBase::IsPerformingAction()
{
	return IsValid( pAction );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimObjectServerBase::CancelAction()
{
	tEnd = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
