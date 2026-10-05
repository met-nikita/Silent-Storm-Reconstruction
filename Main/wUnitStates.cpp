#include "StdAfx.h"
#include "wUnitMove.h"
#include "wUnitServer.h"
#include "wUnitExec.h"
#include "RPGUnitMission.h"
#include "wMain.h"
#include "RPGItemSet.h" // CRAP
#include "wUnitAttack.h"
#include "wObject.h"
#include "..\DBFormat\DataRPG.h"
#include "..\misc\RandomGen.h"
#include "..\MiscDll\LogStream.h"
#include "wAckBase.h"
#include "RPGCritical.h"
#include "time.h"
#include "RPGGame.h"
#include "aiPosition.h"
#include "RPGGlobal.h"
#include "wUnitCommands.h"
#include "wUnitAttackExec.h"	// CExecNotHeroWantsToTalk (corpse-carrier talk dispatch mirror)
#include "rpgCheatConstants.h"
#include "RPGMedals.h"

#include "wUnitStates.h"

namespace NWorld
{
//
// criticalsBan's ctor calls GetObjectID(new CCmdPath()...) -> pSSClasses, the save/load class
// registry that static-init registrars build. Constructing it eagerly as a global is link-order
// dependent and crashes if it runs before those registrars (pSSClasses still null). Build it on
// first use instead -- order-independent, same single instance, same behavior.
static CCriticalsBan &GetCriticalsBan() { static CCriticalsBan criticalsBan; return criticalsBan; }
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitState
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitState::FilterCriticals()
{
	ASSERT( pUS );
	pUS->GetUnitRPG()->EnableCriticals();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitState::ProcessCritical( NDb::ECritical eCA )
{
	if ( eCA == NDb::C_DEATH )
		pUS->KillUnit( VNULL3 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitState::IsCriticalsFailCommand( CCmd *pCmd, EUnitCommandResult *pResult )
{
	EUnitCommandResult res = *pResult;
	*pResult = UCR_CRITICALS_BAN;
	//
	ASSERT( IsValid( pCmd ) );
	if ( !IsValid( pCmd ) )
		return true;
	//
	const list<NDb::ECritical> &criticals = GetCriticalsBan().GetCommandBans( pUS, pCmd );
	for ( list<NDb::ECritical>::const_iterator i = criticals.begin(); i != criticals.end(); ++i )
		if ( pUS->GetUnitRPG()->HasCritical( *i ) )
		{
			// Retail v1.2 0x7cb133..0x7cb149: PK-type command bans
			// have their own feedback, rather than the ordinary injury message.
			if ( *i >= NDb::C_PANZERKLEIN_AXIS && *i <= NDb::C_PANZERKLEIN_TERRORS_HWG )
				*pResult = UCR_PK_BAN;
			return true;
		}
	//
	if ( pUS->IsWearingPK() ) // has Panzerklein critical - special bans
	{
		// Retail v1.2 0x7cafff: all of the pose/path restrictions
		// below report PK feedback. Successful probes restore res at the end.
		*pResult = UCR_PK_BAN;
		CDynamicCast<CCmdPath> pPath(pCmd);
		if (pPath)
		{
			NAI::SPathPlace ptCurr = pUS->GetPosition().pos.p;
			NAI::SPathPlace ptDst = pPath->ptDst.p;
			//if ( pPath->bStrafe )
			//	return true;
			if ( ( ptCurr.GetPose() == NAI::CM_LAY || ptCurr.GetPose() == NAI::CM_INACTIVE ) && ( pPath->eParams & NAI::PF_USE_POSE ) )
				return true;
			int nCrouches = 0;
			if ( ptCurr.GetPose() == NAI::CM_CROUCH )
				++nCrouches;
			if ( ptDst.GetPose() == NAI::CM_CROUCH && ( pPath->eParams & NAI::PF_USE_POSE ) )
				++nCrouches;
			if ( nCrouches )
			{
				if ( ptDst.GetX() != ptCurr.GetX() || ptDst.GetY() != ptCurr.GetY() || 
						ptDst.GetLayer() != ptCurr.GetLayer() )
					return true;
				if ( ptDst.GetDirection() != ptCurr.GetDirection() && nCrouches == 1 )
					return true;
			}
		}
		CDynamicCast<CCmdWishPose> pPose(pCmd);
		if (pPose)
		{
			if ( pPose->pose == NAI::CRAWL || pPose->pose == NAI::RUN )
				return true;
		}
		CDynamicCast<CCmdOpenClose> pOpenClose(pCmd);
		if (pOpenClose)
		{
			if ( pUS->GetPosition().GetPose() == NAI::CROUCH )
				return true;
		}
	}
	//
	*pResult = res;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitStateNormal
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitStateNormal::IsInactive() const
{
	return pUS->GetPosition().pos.p.GetPose() == NAI::CM_INACTIVE;		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const NDb::CPanzerklein *CUnitStateNormal::GetPK() const // returns 0 if no PK present
{
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CCommandExecute* CUnitStateNormal::CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult )
{
	*pResult = UCR_UNAVAILABLE;

	if ( IsCriticalsFailCommand( pCmd, pResult ) )
		return 0;

	if ( IsInactive() )
	{
		CDynamicCast<CCmdReload> pReload(pCmd);
		if (pReload)
			return 0;
		CDynamicCast<CCmdSetActiveItem> pSAI(pCmd);
		if (pSAI)
			return 0;
		CDynamicCast<CCmdMoveInventoryItem> pMII(pCmd);
		if (pMII)
			return 0;
	}
	else
	{
		CDynamicCast<CCmdDropCorpse> pDropCorpse(pCmd);
		if (pDropCorpse)
			return 0;
		CDynamicCast<CCmdExitCannon> pExitCannon(pCmd);
		if (pExitCannon)
			return 0;
	}

	CDynamicCast<CCmdSnipeAttack> pSnipe(pCmd);
	if (pSnipe)
		return 0;

	return NWorld::CreateExecutor( pUS, pCmd, pResult );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateNormal::ProcessCritical( NDb::ECritical eCA )
{
	CCommandExecute *p = 0;
	switch ( eCA )
	{
		case NDb::C_LOST_WEAPON:
			p = CreateLostWeapon( pUS, false );
			break;
		case NDb::C_IDLE_HAND:
			p = CreateLostWeapon( pUS, true );
			break;
		case NDb::C_ACCIDENTAL_SHOT:
			p = CreateAccidentalShot( pUS );
			break;
		case NDb::C_DAMAGE_WEAPON:
			break;
		case NDb::C_BLIND:
			break;
		case NDb::C_STUN:
			pUS->SetState( new CUnitStateStun( pUS ) );
			break;
		default:
			CUnitState::ProcessCritical( eCA );
			break;
	}

	if ( p )
		pUS->RunCriticalExecutor( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateNormal::FilterCriticals()
{
	pUS->GetUnitRPG()->EnableCriticals();

	if ( IsInactive() )
	{
		pUS->GetUnitRPG()->DisableCritical( NDb::C_STUN, NRPG::CS_REROLL );
		pUS->GetUnitRPG()->DisableCritical( NDb::C_MOTIONLESS, NRPG::CS_REROLL );
		pUS->GetUnitRPG()->DisableCritical( NDb::C_ACCIDENTAL_SHOT, NRPG::CS_REROLL );
		pUS->GetUnitRPG()->DisableCritical( NDb::C_DAMAGE_WEAPON, NRPG::CS_REROLL );
		pUS->GetUnitRPG()->DisableCritical( NDb::C_LOST_WEAPON, NRPG::CS_REROLL );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitStateSniping
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitStateSniping::CUnitStateSniping( CUnitServer *_pUS, CUnitServer *_pTarget, int _nBaseAP ):
 CUnitState(_pUS), pTarget(_pTarget), nBaseAP(_nBaseAP), nSnipeAP(0), hitLocation(NAI::HL_BODY)
{
	TargetPosition = pTarget->GetPosition();
	InitialTargetPosition = pTarget->GetPosition();
	pUS->GetUnitRPG()->SaveAP( NRPG::SSnipeAP( nSnipeAP, pTarget->GetUnitRPG() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitStateSniping::CheckTarget()
{
	bool bRes = true;
	//
	bRes = bRes && pTarget->CanFight();
	// check target visibility
	bRes = bRes && pUS->GetWorld()->GetGame()->CheckVisibility( pUS, pTarget, true );
	// check the angle by which the target has moved
	CVec3 ptInitialDir = ( InitialTargetPosition.GetCenter() - pUS->GetPosition().GetCenter() );
	CVec3 ptCurrentDir = ( pTarget->GetPosition().GetCenter() - pUS->GetPosition().GetCenter() );
	Normalize( &ptInitialDir );
	Normalize( &ptCurrentDir );
	float fAngle = acos( ptInitialDir * ptCurrentDir );
	bRes = bRes && (fAngle <= PI/8);
	//
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateSniping::CollectSnipeAP( int _nAP )
{ 
	NRPG::SUnitInfo info;
	pUS->GetUnitRPG()->GetInfo( NAI::WALK, &info );
	if ( info.nAP > 0 )
	{
		pUS->GetUnitRPG()->SpendAP( _nAP );
		nSnipeAP += _nAP;
		pUS->GetUnitRPG()->SaveAP( NRPG::SSnipeAP( nSnipeAP, pTarget->GetUnitRPG() ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateSniping::Segment()
{
	if ( TargetPosition.GetCenter() != pTarget->GetPosition().GetCenter() )
	{
		if ( !CheckTarget() )
			CancelSnipe();
		else
		{
//			pUS->Do( new CCmdSetCommand( pUS, new CCmdSnipeAim( pTarget ) ) );
//			pUS->Do( new CCmdSetCommand( pUS, new CCmdContinue() ) );
			TargetPosition = pTarget->GetPosition();
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CCommandExecute* CUnitStateSniping::CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult ) 
{
	CObj<CCmd> pHold = pCmd;
	if ( IsCriticalsFailCommand( pCmd, pResult ) )
		return 0;

	*pResult = UCR_OK;
	CDynamicCast<CCmdCollectSnipeAP> pCollect(pCmd);
	if (pCollect)
		return NWorld::CreateExecutor( pUS, pCmd, pResult );
	else {
		CDynamicCast<CCmdSnipeAttack> pSnipeAttack(pCmd);
		if (pSnipeAttack)
			return NWorld::CreateExecutor(pUS, new CCmdShootObject(pTarget, 0, hitLocation), pResult);
		else {
			CDynamicCast<CCmdShootObject> pShootObject(pCmd);
			if (pShootObject)
			{
				// Retail v1.2 0x7c8be2..0x7c8c96: a targetless action-bar probe
				// means the held snipe target, not an unavailable attack. Resolve
				// HL_ANY to the stored aim point and keep prepare/extra-AP flags.
				if ( !IsValid( pShootObject->pTarget ) || pShootObject->pTarget.GetPtr() == static_cast<CObjectBase*>(pTarget.GetPtr()) )
				{
					NAI::EHitLocation eHL = pShootObject->eHL == NAI::HL_ANY ? hitLocation : pShootObject->eHL;
					CPtr<CCmdShootObject> pShoot = new CCmdShootObject( pTarget, pShootObject->nExtraAttackAP, eHL );
					pShoot->bOnlyPrepareToShoot = pShootObject->bOnlyPrepareToShoot;
					return NWorld::CreateExecutor( pUS, pShoot, pResult );
				}
			}
		}
	}

	*pResult = UCR_UNAVAILABLE;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateSniping::ProcessCritical( NDb::ECritical eCA ) 
{
	pUS->PostponeCritical( eCA );
	CancelSnipe();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateSniping::CancelSnipe()
{
	pUS->GetUnitRPG()->SaveAP( NRPG::SSnipeAP( 0, 0 ) );
	// reset state back to normal
	pUS->SetState( new CUnitStateNormal( pUS ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitStateUsingCannon
////////////////////////////////////////////////////////////////////////////////////////////////////
CCommandExecute* CUnitStateUsingCannon::CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult ) 
{ 
	if ( IsCriticalsFailCommand( pCmd, pResult ) )
		return 0;

	*pResult = UCR_OK;
	CDynamicCast<CCmdShootTile> pShootTile(pCmd);
	if (pShootTile)
		return NWorld::CreateExecutor( pUS, pCmd, pResult );
	CDynamicCast<CCmdShootObject> pShootObject(pCmd);
	if (pShootObject)
		return NWorld::CreateExecutor( pUS, pCmd, pResult );
	CDynamicCast<CCmdReload> pReload(pCmd);
	if (pReload)
		return NWorld::CreateExecutor( pUS, pCmd, pResult );
	// Retail v1.2 0x7c8e67: mounted units may change the cannon's fire mode.
	CDynamicCast<CCmdShootMode> pShootMode(pCmd);
	if (pShootMode)
		return NWorld::CreateExecutor( pUS, pCmd, pResult );
	// Spending a level-up point changes the RPG sheet, not the mounted gun.
	// The manual save "stational weapons" has an available point while the
	// selected unit is mounted; rejecting this command makes its perk buttons
	// appear clickable but leaves the point unchanged.
	CDynamicCast<CCmdTakePerk> pTakePerk(pCmd);
	if (pTakePerk)
		return NWorld::CreateExecutor( pUS, pCmd, pResult );
	CDynamicCast<CCmdExitCannon> pExitCannon(pCmd);
	if (pExitCannon)
	{
		pExitCannon->pCannon = pCannon;
		return NWorld::CreateExecutor( pUS, pExitCannon, pResult );
	}

	*pResult = UCR_UNAVAILABLE;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateUsingCannon::ProcessCritical( NDb::ECritical eCA ) 
{
	EUnitCommandResult eResult;
	switch ( eCA )
	{
		case NDb::C_BLIND:
		case NDb::C_DEAF:
		case NDb::C_AP_REDUCTION:
		case NDb::C_ENCUMBRANCE:
		case NDb::C_DAMAGE_WEAPON:
		case NDb::C_WEAPONSKILL_REDUCTION:
			break;
		case NDb::C_ACCIDENTAL_SHOT:
			pUS->RunCriticalExecutor( CreateAccidentalShot( pUS ) );
			break;
		default:
			pUS->PostponeCritical( eCA );
			pUS->RunCriticalExecutor( NWorld::CreateExecutor( pUS, new CCmdExitCannon( pCannon ), &eResult ) );
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateUsingCannon::OnActionFinish()
{
	EUnitCommandResult eResult;
	// check if cannon is destroyed or unusable
	if ( !IsValid( pCannon ) || pCannon->IsBroken() )
		pUS->RunCriticalExecutor( NWorld::CreateExecutor( pUS, new CCmdExitCannon( pCannon ), &eResult ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateUsingCannon::OnDeath()
{
	pUS->GetUnitRPG()->SetCannonItem(0);
	pCannon->SetCurrentUnit(0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitStateCorpseCarrier
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitStateCorpseCarrier::CUnitStateCorpseCarrier( CUnitServer *_pUS, CUnitServer *_pDead ): 
	CUnitState(_pUS), pDeadUnit(_pDead)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CCommandExecute* CUnitStateCorpseCarrier::CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult ) 
{ 
	if ( IsCriticalsFailCommand( pCmd, pResult ) )
		return 0;

	*pResult = UCR_OK;
	CDynamicCast<CCmdPath> pPath(pCmd);
	// v1.1 0x7c8d1a / v1.2 0x7c90ea: carriers can turn without dropping the body.
	CDynamicCast<CCmdLook> pLook(pCmd);
	// v1.2 0x7c9141..0x7c9158: taking a perk is also allowed while carrying.
	CDynamicCast<CCmdTakePerk> pTakePerk(pCmd);
	CDynamicCast<CCmdUsePassage> pPassage(pCmd);
	CDynamicCast<CCmdTeleport> pTeleport(pCmd);
	CDynamicCast<CCmdPlayAnimation> pAnimation(pCmd);
	CDynamicCast<CCmdTalk> pTalk(pCmd);
	CDynamicCast<CCmdNotHeroWantsToTalk> pNotHeroTalk(pCmd);
	// Retail v1.2 0x7c90a8..0x7c9192 forwards all eight admitted commands.
	if (pPath || pPassage || pLook || pTeleport || pAnimation || pTakePerk || pTalk || pNotHeroTalk)
		return NWorld::CreateExecutor( pUS, pCmd, pResult );
	CDynamicCast<CCmdWishPose> pWishPose(pCmd);
	if (pWishPose)
	{
		if (pWishPose->pose == NAI::WALK)
			return NWorld::CreateExecutor( pUS, pCmd, pResult );
		else
			return 0;
	}
	CDynamicCast<CCmdDropCorpse> pDropCorpse(pCmd);
	if (pDropCorpse)
	{
		CDynamicCast<CUnitServer> pCorpse(pDropCorpse->pCorpse);
		if (pCorpse)
		{
			if ( pCorpse != pDeadUnit )
			{
				*pResult = UCR_INVALID_COMMAND;
				return 0;
			}
		}

		pDropCorpse->pCorpse = pDeadUnit;
		return NWorld::CreateExecutor( pUS, pDropCorpse, pResult );
	}
	*pResult = UCR_UNAVAILABLE;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateCorpseCarrier::OnStateStarted()
{
	CPtr<NRPG::CGlobalPlayer> pGlobalPlayer = pUS->GetPlayer()->GetGlobalPlayer();
	if ( IsValid( pGlobalPlayer ) )
	{
		NRPG::CUnit *pCarrier = pUS->GetUnitRPG()->GetRPGUnit();
		NRPG::CUnit *pCorpse = pDeadUnit->GetUnitRPG()->GetRPGUnit();
		//
		if ( !pDeadUnit->IsDead() )
		{
			if ( pUS->GetPlayer() == pDeadUnit->GetPlayer() )
				pGlobalPlayer->RescueUnit( pCarrier, pCorpse );
			else
				pGlobalPlayer->CaptureUnit( pCarrier,	pCorpse );
		}
		else
			pGlobalPlayer->TakeUnitCorpse( pCarrier, pCorpse );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateCorpseCarrier::OnStateFinished()
{
	CPtr<NRPG::CGlobalPlayer> pGlobalPlayer = pUS->GetPlayer()->GetGlobalPlayer();
	if ( IsValid( pGlobalPlayer ) )
		pGlobalPlayer->FreeUnit( pUS->GetUnitRPG()->GetRPGUnit() );
	pDeadUnit->animator.BeDropped( pDeadUnit );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateCorpseCarrier::ProcessCritical( NDb::ECritical eCA )
{
	switch ( eCA )
	{
		case NDb::C_BLIND:
		case NDb::C_DEAF:
		case NDb::C_AP_REDUCTION:
		case NDb::C_WEAPONSKILL_REDUCTION:
			break;
		default:
			pUS->PostponeCritical( eCA );
			pUS->SetState( new CUnitStateNormal( pUS ) );
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitStateHealer
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitStateHealer::CUnitStateHealer(): bNewSegment(false), fKitCapacity(0), nCriticalHealAPRequired(0)
{
	healCriticalInfo.eCritical = NDb::C_NONE;
	healCriticalInfo.eCl = NDb::CL_ANY;
	healCriticalInfo.nDC = 0;
}
CUnitStateHealer::CUnitStateHealer( CUnitServer *_pUS, CUnitServer *_pTarget ):
	CUnitStateHealer()
{
	pUS = _pUS;
	pTarget = _pTarget;
	ASSERT( IsValid( pUS ) );
	ASSERT( IsValid( pTarget ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateHealer::DoHealing( int nUnitAP )
{
	ASSERT( IsValid( pTarget ) );
	if ( !IsValid( pTarget ) || ( fKitCapacity <= 0 && nCriticalHealAPRequired <= 0 ) )
	{
		pUS->SetState( new CUnitStateNormal( pUS ) );
		return;
	}

	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	NRPG::CUnit *pRPGUnit = pRPG->GetRPGUnit();
	NRPG::IFirstAidItem *pItem = pRPGUnit->GetFirstAidItem();
	if ( !pItem )
	{
		pUS->SetState( new CUnitStateNormal( pUS ) );
		return;
	}
	// Paid treatment now needs at least 1 AP. Wait for the next turn rather
	// than treating an exhausted AP budget as a finished/failed treatment.
	if ( nUnitAP <= 0 )
		return;
	int nRequiredAP = 0;
	NRPG::SFirstAid fa;
	if ( pItem->GetDBFirstAid()->effect == NDb::FAE_REPAIR_PK )
	{
		CUnitServer *pPK = pTarget->GetWearingPK();
		if ( !IsValid(pPK) && pTarget->IsEmptyPK() )
			pPK = pTarget;
		if ( IsValid(pPK) && pRPGUnit->CreateFirstAid( &fa, nUnitAP, fKitCapacity, pItem,
			pPK->GetUnitRPG()->GetRPGUnit(), &nRequiredAP ) )
		{
			fKitCapacity -= fa.fdVP;
			pPK->GetUnitRPG()->GetRPGUnit()->Skills(NDb::ST_VP).Modify( int(fa.fdVP) );
			pUS->SpendAP( nRequiredAP );
		}
		else
			fKitCapacity = 0;
	}
	else if ( nCriticalHealAPRequired > 0 )
	{
		nRequiredAP = Min( nCriticalHealAPRequired, nUnitAP );
		nCriticalHealAPRequired -= nRequiredAP;
		pUS->SpendAP( nRequiredAP );
		if ( nCriticalHealAPRequired <= 0 )
		{
			float fHealed = pTarget->GetUnitRPG()->HealCritical( healCriticalInfo );
			pRPGUnit->AddMedalPoints( pUS->GetWorld()->GetGlobalGame(), NRPG::MPC_HEALED_CRITICAL, fHealed );
		}
	}
	else if ( pRPGUnit->CreateFirstAid( &fa, nUnitAP, fKitCapacity, pItem, pTarget->GetUnitRPG()->GetRPGUnit(), &nRequiredAP ) )
	{
		fKitCapacity -= fa.fdVP;
		float fHealed = pTarget->GetUnitRPG()->HealVP( fa );
		if ( fHealed > 0 )
			pRPGUnit->AddMedalPoints( pUS->GetWorld()->GetGlobalGame(), NRPG::MPC_HEALED_WOUND, fHealed );
		pUS->SpendAP( nRequiredAP );
		if ( !pRPGUnit->CreateFirstAid( &fa, nUnitAP, fKitCapacity, pItem, pTarget->GetUnitRPG()->GetRPGUnit(), &nRequiredAP ) )
			fKitCapacity = 0;
	}
	else
		fKitCapacity = 0;

	if ( fKitCapacity <= 0 && nCriticalHealAPRequired <= 0 )
		pUS->SetState( new CUnitStateNormal( pUS ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateHealer::SetCriticalHealAPRequired()
{
	NRPG::CUnit *pHealer = pUS->GetUnitRPG()->GetRPGUnit();
	float fAP = healCriticalInfo.nDC * 50.f / Max( 1, int(pHealer->Skills(NDb::ST_MEDICINE)) );
	float fPerk;
	if ( pHealer->HasPerk(62, &fPerk) && fPerk > 0 )
		fAP /= fPerk;
	nCriticalHealAPRequired = Max( 1, Float2Int(fAP) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateHealer::SayAck( bool bRepair )
{
	NRPG::SUnitInfo Info = {};
	pTarget->GetUnitRPG()->GetInfo( NAI::WALK, &Info );
	bool bGood;
	if ( nCriticalHealAPRequired != 0 )
		bGood = !pTarget->GetUnitRPG()->HasCritical( healCriticalInfo.eCritical );
	else if ( bRepair )
	{
		if ( !Info.bPKInfo || !sTargetInfo.bPKInfo )
			return;
		bGood = (Info.nMaxPKHP - Info.nPKHP) * 2 < sTargetInfo.nMaxPKHP - sTargetInfo.nPKHP;
	}
	else
		bGood = (Info.nMaxHP - Info.nHP - Info.nHealedHP) * 2 < sTargetInfo.nMaxHP - sTargetInfo.nHP - sTargetInfo.nHealedHP;
	if ( !bGood )
		pUS->GetWorld()->GetGlobalAck()->OnCannotFinishHeal( pUS, pTarget );
	else
		pUS->GetWorld()->GetGlobalAck()->OnHealFinished( pUS, pTarget );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateHealer::OnFinishTimeOrTurn( bool bRealTime )
{
	CPtr<CObjectBase> pHold(this);
	if ( bNewSegment )
	{
		int nTotalUnitAP = pUS->GetUnitRPG()->GetRPGUnit()->Skills(NDb::ST_AP).GetMaxValue();
		int nUnitAP = bRealTime ? Float2Int( nTotalUnitAP * 0.5f ) : pUS->GetAP();
		DoHealing( nUnitAP );
	}
	bNewSegment = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateHealer::OnStateStarted()
{
	CPtr<CObjectBase> pHold(this);
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	pTarget->GetUnitRPG()->ApplyCritical( NRPG::SCritical( NDb::CL_ANY, NDb::C_PATIENT ) );
	//
	NRPG::IFirstAidItem *pItem = pRPG->GetRPGUnit()->GetFirstAidItem();
	if ( !pItem )
	{
		pUS->SetState( new CUnitStateNormal( pUS ) );
		return;
	}
	fKitCapacity = 0;
	nCriticalHealAPRequired = 0;
	NRPG::IUnitMission *pPatient = pTarget->GetUnitRPG();
	NRPG::CUnit *pHealer = pRPG->GetRPGUnit();
	NRPG::SUnitInfo info = {};
	pPatient->GetInfo( NAI::WALK, &info );
	sTargetInfo.bPKInfo = info.bPKInfo;
	sTargetInfo.nHP = info.nHP;
	sTargetInfo.nHealedHP = info.nHealedHP;
	sTargetInfo.nMaxHP = info.nMaxHP;
	sTargetInfo.nPKHP = info.nPKHP;
	sTargetInfo.nMaxPKHP = info.nMaxPKHP;
	sTargetInfo.nAP = info.nAP;
	sTargetInfo.nMaxAP = info.nMaxAP;
	sTargetInfo.nSightDistance = info.nSightDistance;
	sTargetInfo.bUnitInfo = info.bUnitInfo;
	bool bStopBleeding = pHealer->HasPerk(59);
	float fDurationMult = 1.f, fPerk;
	if ( pHealer->HasPerk(64, &fPerk) )
		fDurationMult = fPerk;
	NDb::CRPGFirstAid *pFirstAid = pItem->GetDBFirstAid();
	switch ( pFirstAid->effect )
	{
		case NDb::FAE_NORMAL:
			if ( pHealer->CanHeal( pPatient->GetRPGUnit(), pItem ) )
				fKitCapacity = pFirstAid->nTotalHealVP;
			else
			{
				healCriticalInfo = pPatient->TryHealCritical( pHealer->GetFirstAidDC(pItem, pPatient->GetRPGUnit()), bStopBleeding );
				if ( healCriticalInfo.eCritical != NDb::C_NONE )
					SetCriticalHealAPRequired();
			}
			break;
		case NDb::FAE_CRITICAL_ONLY:
			healCriticalInfo = pPatient->TryHealCritical( pHealer->GetFirstAidDC(pItem, pPatient->GetRPGUnit()), bStopBleeding );
			// Retail tests the location field in this branch (0x7c9c52).
			if ( int(healCriticalInfo.eCl) != int(NDb::C_NONE) )
				SetCriticalHealAPRequired();
			break;
		case NDb::FAE_CRITICAL_FIRST:
			healCriticalInfo = pPatient->TryHealCritical( pHealer->GetFirstAidDC(pItem, pPatient->GetRPGUnit()), bStopBleeding );
			if ( healCriticalInfo.eCritical != NDb::C_NONE )
				SetCriticalHealAPRequired();
			else
				fKitCapacity = pFirstAid->nTotalHealVP;
			break;
		case NDb::FAE_TEMP_REMOVE_PENALTIES:
			pPatient->SuspendCriticals( int(pFirstAid->nDuration * fDurationMult) );
			pPatient->AddPostponedModifier( &pPatient->GetRPGUnit()->Skills(NDb::ST_VP),
				NRPG::SSkillModifyInfo(1.f - pFirstAid->fPower * .01f, 0), int(pFirstAid->nDuration * fDurationMult) );
			break;
		case NDb::FAE_BOOST_VP:
		{
			float fMult = 1.0f, fPerk;
			if ( pRPG->GetRPGUnit()->HasPerk( 0x40, &fPerk ) )   // 0x7c9721 (healer's capacity perk)
				fMult = fPerk;
			pTarget->GetUnitRPG()->AddVPBoost( pFirstAid->fPower, int( pFirstAid->nDuration * fMult ) );  // 0x7c9966: AddVPBoost(fPower, trunc(nDuration*fMult))
			pTarget->SyncConscious();                            // 0x7c996f
			if ( pTarget == pUS && !pUS->CanFight() )            // 0x7c997a: self-heal knocked self out
				return;                                          // goto release: skip the fKitCapacity/SetState(Normal)/breath tail
			break;
		}
		case NDb::FAE_TEMP_STOP_BLEEDING:
			pPatient->AddBleedingStopper( Float2Int(pFirstAid->fPower) );
			break;
		case NDb::FAE_REMOVE_BLEEDING:
		{
			float fHealed = pPatient->HealCriticals( -100, true );
			if ( fHealed > 0 )
				pHealer->AddMedalPoints( pUS->GetWorld()->GetGlobalGame(), NRPG::MPC_HEALED_CRITICAL, fHealed );
			break;
		}
		case NDb::FAE_REPAIR_PK:
			if ( IsValid(pTarget->GetWearingPK()) || pTarget->IsEmptyPK() )
				fKitCapacity = pFirstAid->nTotalHealVP;
			break;
		default:
			ASSERT(0);
			break;
	}
	if ( fKitCapacity <= 0 && nCriticalHealAPRequired <= 0 )
	{
		pUS->SetState( new CUnitStateNormal( pUS ) );
		return;
	}

	if ( !pUS->GetWorld()->IsRealTime() )
	{
		DoHealing( pUS->GetAP() );
		bNewSegment = false;
	}
	if ( IsValid( pTarget ) )
		pTarget->animator.SetBreathOnlyIdle( true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateHealer::OnStateFinished()
{
	NRPG::IUnitMission *pRPG = pUS->GetUnitRPG();
	NRPG::IInventory *pInventory = pRPG->GetInventory();
	CDynamicCast<NRPG::CFirstAidItem> pFA(pRPG->GetInventory()->GetActive());
	bool bRepair = pFA && pFA->GetDBFirstAid()->effect == NDb::FAE_REPAIR_PK;
	if (pFA)
	{
		pFA->SpendPotion();
		if ( pFA->IsEmpty() )
		{
			// healing finished and the medkits are used up, remove the item
			CObj<NRPG::IInventoryItem> pErase = pInventory->TakeOff( (NDb::ESlot)pInventory->GetActiveSlot() );
			pUS->animator.SetActiveItem( false );
			// Retail v1.2 0x7ca7cf: a spent medkit also restores the unarmed animation set.
			pUS->animator.SetWeaponAnimation( NDb::WT_DEFAULT );
			pUS->Update();
		}
	}
	if ( bRepair )
		pUS->SetHandEffect( 0, 0 );
	pUS->animator.FinishHealing( pUS->GetPosition(), bRepair,
		IsValid(pFA) && pFA->GetDBFirstAid()->GetRecordID() == 26 ? 0x1185 : 0x1184 );
	pTarget->GetUnitRPG()->RemoveCritical( NDb::C_PATIENT );
	if ( pTarget->CanFight() )
		SayAck( bRepair );
	//
	pTarget->animator.SetBreathOnlyIdle( pUS->GetWorld()->IsRealTime() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateHealer::OnUnitDied( CUnitServer *pUnit )
{
	if ( pUnit == pTarget )
		pUS->SetState( new CUnitStateNormal( pUS ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CCommandExecute* CUnitStateHealer::CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult ) 
{
	if ( !IsCriticalsFailCommand( pCmd, pResult ) )
	{
		CDynamicCast<CCmdCancel> pCancel(pCmd);
		if (pCancel)
			return NWorld::CreateExecutor( pUS, pCmd, pResult );
	}
	//
	*pResult = UCR_UNAVAILABLE;
	return 0; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateHealer::ProcessCritical( NDb::ECritical eCA ) 
{
	switch ( eCA )
	{
		case NDb::C_DEAF:
		case NDb::C_AP_REDUCTION:
		case NDb::C_MOTIONLESS:
		case NDb::C_ENCUMBRANCE:
		case NDb::C_WEAPONSKILL_REDUCTION:
			break;
		case NDb::C_ACCIDENTAL_SHOT:
			// deal small damage ( 4 d4 )
			for ( int i = 0; i < 4; ++i)
				pTarget->GetUnitRPG()->MakeDirectDamage( random.Get( 1, 4 ) );
			pUS->SetState( new CUnitStateNormal( pUS ) );
			break;
		default:
			pUS->PostponeCritical( eCA );
			pUS->SetState( new CUnitStateNormal( pUS ) );
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitStateEngineering
////////////////////////////////////////////////////////////////////////////////////////////////////
CCommandExecute* CUnitStateEngineering::CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult )
{
	return NWorld::CreateExecutor( pUS, pCmd, pResult );
	//*pResult = UCR_UNAVAILABLE;
	//return 0; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateEngineering::ProcessCritical( NDb::ECritical eCA ) 
{
	switch ( eCA )
	{
		case NDb::C_DEAF:
		case NDb::C_AP_REDUCTION:
		case NDb::C_MOTIONLESS:
		case NDb::C_ENCUMBRANCE:
		case NDb::C_WEAPONSKILL_REDUCTION:
			break;
		case NDb::C_ACCIDENTAL_SHOT:
			// detonate the trap
			// switch to normal state
			pUS->SetState( new CUnitStateNormal( pUS ) );			
			break;
		default:
			pUS->PostponeCritical( eCA );
			pUS->SetState( new CUnitStateNormal( pUS ) );			
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitStateStun
////////////////////////////////////////////////////////////////////////////////////////////////////
CCommandExecute* CUnitStateStun::CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult ) 
{ 
	*pResult = UCR_UNAVAILABLE;
	return 0; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateStun::ProcessCritical( NDb::ECritical eCA ) 
{
	CUnitState::ProcessCritical( eCA );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateStun::Segment()
{
	if ( !pUS->GetUnitRPG()->HasCritical( NDb::C_STUN ) )
		pUS->SetState( new CUnitStateNormal( pUS ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitStateDeath
////////////////////////////////////////////////////////////////////////////////////////////////////
CCommandExecute* CUnitStateDeath::CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult ) 
{ 
	*pResult = UCR_UNAVAILABLE;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateDeath::ProcessCritical( NDb::ECritical eCA ) 
{
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateDeath::FilterCriticals()
{
	pUS->GetUnitRPG()->DisableCriticals();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateDeath::OnStateStarted()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateDeath::OnStateFinished()
{
	ASSERT( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitStateUnconscious
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitStateUnconscious::CUnitStateUnconscious( CUnitServer *_pUS ):
	CUnitState(_pUS)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CCommandExecute* CUnitStateUnconscious::CreateExecutor( CCmd *pCmd, EUnitCommandResult *pResult )
{
	*pResult = UCR_UNAVAILABLE;
	return 0; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateUnconscious::ProcessCritical( NDb::ECritical eCA )
{
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateUnconscious::FilterCriticals()
{
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateUnconscious::OnStateStarted()
{
	pUS->GetUnitRPG()->GetRPGUnit()->SetUnconscious( true );
	//
	vector< CPtr<CPlayer> > players;
	pUS->GetWorld()->GetPlayersList( &players );
	for ( vector< CPtr<CPlayer> >::iterator i = players.begin(); i != players.end(); ++i )
		(*i)->OnUnitDied( pUS );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitStateUnconscious::OnStateFinished()
{
	pUS->GetUnitRPG()->GetRPGUnit()->SetUnconscious( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitStateInPocket
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitStateInPocket::CUnitStateInPocket( CUnitServer *_pUS ): CUnitState( _pUS ) 
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3c9fb0: pocket the unit (master hold) + drop it from the world unit list (vtbl+0xdc =
// RemoveUnit), then flag it OUT of the visitor set. That last store -- bNotAddedToVisitors = true at
// CDumbUnitServer+0x12c (the decomp's `*(undefined1 *)(pUS + 300) = 1`) -- is the UNIT-side analogue
// of the objects' BeAddedToVisitiors(false); without it a pocketed unit stays bound to the vis sync.
void CUnitStateInPocket::OnStateStarted()
{
	CPtr<NWorld::CWorld> pWorld = pUS->GetWorld();
	if ( !pWorld->GetPocket()->IsUnitInPocket( pUS ) )
	{
		pWorld->GetPocket()->PlaceUnitInPocket( pUS );
		pWorld->RemoveUnit( pUS );
		pUS->MarkNotAddedToVisitors();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3ca060: the exact mirror -- re-add (vtbl+0xe0), settle onto a passable place, refresh
// vision (vtbl+0x104, bForce=false), unpocket, then clear bNotAddedToVisitors.
void CUnitStateInPocket::OnStateFinished()
{
	CPtr<NWorld::CWorld> pWorld = pUS->GetWorld();
	if ( pWorld->GetPocket()->IsUnitInPocket( pUS ) )
	{
		pWorld->AddUnit( pUS );
		pUS->PlaceOnPassablePlace();
		pWorld->UpdateVisible();
		pWorld->GetPocket()->RemoveUnitFromPocket( pUS );
		pUS->ClearNotAddedToVisitors();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCriticalsBan
////////////////////////////////////////////////////////////////////////////////////////////////////
static const int CR_DEFAULT_PARAM = -1;
//
int CCriticalsBan::GetObjectID( CObjectBase *pObject )
{
	ASSERT( IsValid( pObject ) );
	if ( !IsValid( pObject ) )
		return 0;
	//
	CPtr<CObjectBase> pHolder = pObject;
	return pSSClasses->GetObjectTypeID( pObject );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CCriticalsBan::GetParam( CUnitServer *pUS, CCmd *pCmd )
{
	EActionType type = GetActionType( pUS );
	CDynamicCast<CCmdShootObject> pShootObject(pCmd);
	if (pShootObject)
	{
		if ( type == AT_MELEE )
			return type;
		else
			return CR_DEFAULT_PARAM;
	}
	else {
		CDynamicCast<CCmdShootTile> pShootTile(pCmd);
		if (pShootTile)
		{
			if (type == AT_MELEE || type == AT_GRENADE)
				return type;
			else
				return CR_DEFAULT_PARAM;
		}
		else {
			CDynamicCast<CCmdMoveInventoryItem> pMII(pCmd);
			if (pMII)
			{
				// moving an item out of the hand is always allowed
				if (pMII->GetSource().eType != SItem::HAND)
					return CR_DEFAULT_PARAM;
				else
					return CR_DEFAULT_PARAM - 1;
			}
			else
				return CR_DEFAULT_PARAM;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int CR_LAST = 0xFFFF;
//
void CCriticalsBan::AddCommandBans( int nCommandID, int nParam, ... )
{
	va_list params;
	va_start( params, nParam );
	int nCritical = 0;
	while( nCritical != CR_LAST )
	{
		nCritical = va_arg( params, int);
		if ( nCritical != CR_LAST )
			commandsBans[nCommandID][nParam].push_back( (NDb::ECritical)nCritical );
	}
	va_end( params );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CCriticalsBan::CCriticalsBan()
{
#define C_ANY_PANZERKLEIN \
	NDb::C_PANZERKLEIN_AXIS,\
	NDb::C_PANZERKLEIN_ALLIES,\
	NDb::C_PANZERKLEIN_TERRORS,\
	NDb::C_PANZERKLEIN_BROKEN,\
	NDb::C_PANZERKLEIN_AXIS_SOLDIER,\
	NDb::C_PANZERKLEIN_AXIS_ENGINEER,\
	NDb::C_PANZERKLEIN_ALLIES_SCOUT,\
	NDb::C_PANZERKLEIN_ALLIES_SNIPER,\
	NDb::C_PANZERKLEIN_TERRORS_MEDIC,\
	NDb::C_PANZERKLEIN_TERRORS_HWG
	//
	AddCommandBans( GetObjectID( new CCmdPath() ), CR_DEFAULT_PARAM,
		NDb::C_MOTIONLESS, NDb::C_PATIENT, NDb::C_PANZERKLEIN_BROKEN, CR_LAST );
	AddCommandBans( GetObjectID( new CCmdShootObject() ), AT_MELEE,
		NDb::C_BLIND, NDb::C_MOTIONLESS, NDb::C_IDLE_HAND, NDb::C_PATIENT, NDb::C_PANZERKLEIN_BROKEN, CR_LAST );
	AddCommandBans( GetObjectID( new CCmdShootObject() ), CR_DEFAULT_PARAM,
		NDb::C_BLIND, NDb::C_IDLE_HAND, NDb::C_PANZERKLEIN_BROKEN, CR_LAST );
	AddCommandBans( GetObjectID( new CCmdShootTile() ), AT_MELEE,
		NDb::C_MOTIONLESS, NDb::C_IDLE_HAND, NDb::C_PATIENT, NDb::C_PANZERKLEIN_BROKEN, CR_LAST );
	AddCommandBans( GetObjectID( new CCmdShootTile() ), AT_GRENADE,	
		NDb::C_IDLE_HAND, NDb::C_PANZERKLEIN_BROKEN, CR_LAST );
	AddCommandBans( GetObjectID( new CCmdShootTile() ), CR_DEFAULT_PARAM,	
		NDb::C_IDLE_HAND, NDb::C_PANZERKLEIN_BROKEN, CR_LAST );
	AddCommandBans( GetObjectID( new CCmdHeal() ), CR_DEFAULT_PARAM,	
		NDb::C_IDLE_HAND, NDb::C_BLIND, C_ANY_PANZERKLEIN, CR_LAST );
	AddCommandBans( GetObjectID( new CCmdOpenClose() ), CR_DEFAULT_PARAM,	
		NDb::C_PATIENT, NDb::C_PANZERKLEIN_BROKEN, CR_LAST );
	AddCommandBans( GetObjectID( new CCmdCannon() ), CR_DEFAULT_PARAM,	
		NDb::C_IDLE_HAND, NDb::C_BLIND, C_ANY_PANZERKLEIN, CR_LAST );
	AddCommandBans( GetObjectID( new CCmdTakeCorpse() ), CR_DEFAULT_PARAM,	
		NDb::C_IDLE_HAND, NDb::C_BLIND, NDb::C_MOTIONLESS, NDb::C_ENCUMBRANCE, NDb::C_PATIENT, C_ANY_PANZERKLEIN, CR_LAST );
	AddCommandBans( GetObjectID( new CCmdReload() ), CR_DEFAULT_PARAM,	
		NDb::C_IDLE_HAND, NDb::C_BLIND, NDb::C_PANZERKLEIN_BROKEN, CR_LAST );
	AddCommandBans( GetObjectID( new CCmdMoveInventoryItem() ), CR_DEFAULT_PARAM,	
		NDb::C_PATIENT, NDb::C_IDLE_HAND, NDb::C_BLIND, NDb::C_PANZERKLEIN_BROKEN, CR_LAST );
	AddCommandBans( GetObjectID( new CCmdSetActiveItem() ), CR_DEFAULT_PARAM,	
		NDb::C_IDLE_HAND, NDb::C_BLIND, NDb::C_PANZERKLEIN_BROKEN, CR_LAST );
	AddCommandBans( GetObjectID( new CCmdUsePassage() ), CR_DEFAULT_PARAM,	
		NDb::C_MOTIONLESS, NDb::C_PANZERKLEIN_BROKEN, CR_LAST );
	AddCommandBans( GetObjectID( new CCmdSetGrenadeOnObject() ), CR_DEFAULT_PARAM,	
		NDb::C_IDLE_HAND, NDb::C_BLIND, NDb::C_PATIENT, C_ANY_PANZERKLEIN, CR_LAST );
	AddCommandBans( GetObjectID( new CCmdUntrapObject() ), CR_DEFAULT_PARAM,	
		NDb::C_IDLE_HAND, NDb::C_BLIND, NDb::C_PATIENT, C_ANY_PANZERKLEIN, CR_LAST );
	AddCommandBans( GetObjectID( new CCmdSetMineOnTile() ), CR_DEFAULT_PARAM,	
		NDb::C_IDLE_HAND, NDb::C_BLIND, NDb::C_PATIENT, C_ANY_PANZERKLEIN, CR_LAST );
#undef C_ANY_PANZERKLEIN
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const list<NDb::ECritical> &CCriticalsBan::GetCommandBans( CUnitServer *pUS, CCmd *pCmd )
{
	ASSERT( IsValid( pCmd ) );
	return commandsBans[ GetObjectID( pCmd ) ][ GetParam( pUS, pCmd ) ];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NWorld;
BASIC_REGISTER_CLASS( CUnitState )
REGISTER_SAVELOAD_CLASS( 0x01822160, CUnitStateNormal )
REGISTER_SAVELOAD_CLASS( 0x01822161, CUnitStateSniping )
REGISTER_SAVELOAD_CLASS( 0x01822162, CUnitStateUsingCannon )
REGISTER_SAVELOAD_CLASS( 0x01822163, CUnitStateCorpseCarrier )
REGISTER_SAVELOAD_CLASS( 0x01822164, CUnitStateHealer )
REGISTER_SAVELOAD_CLASS( 0x01822165, CUnitStateEngineering )
REGISTER_SAVELOAD_CLASS( 0x01822166, CUnitStateDeath )
REGISTER_SAVELOAD_CLASS( 0x01822167, CUnitStateStun )
REGISTER_SAVELOAD_CLASS( 0x52322110, CUnitStateUnconscious )
REGISTER_SAVELOAD_CLASS( 0x52822170, CUnitStateInPocket )
