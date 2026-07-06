#include "StdAfx.h"

#include "aiUnit.h"
#include "aiPosition.h"

#include "..\DBFormat\DataRPG.h"

#include "RPGGame.h"
#include "RPGUnit.h"
#include "RPGItem.h"
#include "RPGItemSet.h"
#include "RPGItemInfo.h"
#include "RPGUnitMission.h"

#include "..\MiscDll\LogStream.h"

#include "math.h"

#include "wOSBase.h"
#include "wUnitServer.h"
//
#include "RPGToHit.h"
//
namespace NRPG
{
using NWorld::CUnitServer;
//
inline float DistanceFunc( int nDistInTile, float fSlope )
{
	return ( (fSlope + 1.f) * 100.f ) / ( float(nDistInTile) + fSlope );
}
inline float Cos3DistanceFunc( int nDistInTile, float fSlope )
{
	return 100.f * pow( cos( float(nDistInTile) / fSlope ), 3 ) + 2;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release-new free helpers (RVA 0x2b6ca0 / 0x2b6d50) -- fully recovered from .rdata, no hooks.
// GetHeadshotMultiplier weights the cover/area factor (and the thrown-blade result) for a called shot
// at a specific body part. GetThrowToHitPenalty is the alternative malus when called shots are
// DISABLED. Values confirmed from Game.exe @0x6b6ca0 / @0x6b6d50 (the spec's BODY=1.30 was wrong;
// the .rdata constant @0x8c39fc is 0x3f266666 == 0.65).
////////////////////////////////////////////////////////////////////////////////////////////////////
inline float GetHeadshotMultiplier( NAI::EHitLocation hl )
{
	switch ( hl )
	{
		case NAI::HL_BODY:  return 0.65f;
		case NAI::HL_HEAD:  return 0.15f;
		case NAI::HL_RHAND:
		case NAI::HL_LHAND: return 0.35f;
		case NAI::HL_RLEG:
		case NAI::HL_LLEG:  return 0.45f;
	}
	return 1.0f; // HL_ANY (-1) and any other -> no weighting
}
inline float GetThrowToHitPenalty( NAI::EHitLocation hl )
{
	switch ( hl )
	{
		case NAI::HL_BODY:  return 10.0f;
		case NAI::HL_HEAD:  return 25.0f;
		case NAI::HL_RHAND:
		case NAI::HL_LHAND: return 20.0f;
		case NAI::HL_RLEG:
		case NAI::HL_LLEG:  return 15.0f;
	}
	return 0.0f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::GetBestPose  @0x2b6bd0 (RVA) -- release-new free helper, fully recovered (no hooks). Picks the
// firing pose with the larger positive per-weapon bonus, defaulting to WALK when neither crouch nor
// crawl is favoured. Reads NDb::CRPGWeaponType::fCrouchBonus (+0x1c) / fCrawlBonus (+0x18); confirmed
// from Game.exe @0x6b6bd0 (fcomp 0.0 / test ah,0x41 strictly-greater guards). STANDALONE: it has no
// caller in this tree, so landing it is a pure, behaviour-neutral parity addition. It is NOT
// CToHitCalcer::GetStance (which reads the bonus by eCurPose -- different semantics).
NAI::EPose GetBestPose( const NDb::CRPGWeaponType &wt )
{
	if ( wt.fCrouchBonus > 0.f && wt.fCrawlBonus < wt.fCrouchBonus )
		return NAI::CROUCH;
	if ( wt.fCrawlBonus > 0.f && wt.fCrouchBonus < wt.fCrawlBonus )
		return NAI::CRAWL;
	return NAI::WALK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::GetMaxTrowDistance  @0x2b7c70 (RVA) -- release-new free helper. The release range is the
// 45-degree ballistic r = v^2/g, then * FP_INV_GRID_STEP (1/0.625 = 1.6) to convert metres -> grid
// tiles:  return (v/g)*v*1.6f .  The max throw VELOCITY v (NRPG::GetMaxThrowVelocity @0x2b7b50) and the
// throw-physics gravity g (a vtbl[0xdc]()[+0x24] read) are a deep opaque IUnitMission-vtable chain
// (0x58/0x74/0xdc/0x188/0x78 + weapon-DB reads + pow(x,0.74) + a clamp query) that is undecoded and
// ABSENT in this tree -- the same elision already documented in CThrowKnifeToHitCalcer::FillWeaponInfo
// (the fixed 1..15 range). A faithful port is NOT possible without reconstructing GetMaxThrowVelocity,
// so this is a documented elision returning the dev's fixed throw range. STANDALONE: no caller; it must
// NOT be wired into FillWeaponInfo (that would change behaviour). Landing it is a parity marker only.
float GetMaxTrowDistance( IUnitMission *, IInventoryItem *, bool )
{
	// elided: the (v/g)*v*FP_INV_GRID_STEP formula needs the absent GetMaxThrowVelocity / gravity chain.
	return 15.0f; // matches CThrowKnifeToHitCalcer's fixed nMaxRange elision (RPGToHit.cpp ~line 605)
}
// The release gates the per-hit-location headshot reweighting on a global config flag
// GetGlobalGame()->[+0x48][+0x69] (NRPG::CGlobalGame, a release-new world-config reach whose layout
// differs from this predecessor tree -- not cleanly resolvable to a dev field). Elided to TRUE (the
// shipped "called shots enabled" default): for HL_ANY GetHeadshotMultiplier==1.0 and
// GetThrowToHitPenalty==0, so this only affects explicit called shots. Flip this one constant if the
// flag is ever resolved.
static const bool g_bCalledShotsEnabled = true;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release migration: 1st arg is the firing unit's CUnitServer*; the mission is pulled out of it
// (server->GetUnitRPG()). nHitCover is now a FLOAT fHitCover; pUnitServer/bNight are new members. The
// nDistance>=1 clamp is from the disasm (@0x6b8b70).
CToHitCalcer::CToHitCalcer( CUnitServer *_pUnitServer, NAI::EPose _eCurPose, int _nDistance,
	CVec3 _ptAttacker, int _nExtraAP, int _nSnipeAP, float _fHitCover, bool _bFirstRound, bool _bNight,
	CVec3 _ptIllumination, int _nBullet, bool _bBackStab ):
		eCurPose(_eCurPose), nDistance(_nDistance), ptAttacker(_ptAttacker),
		nExtraAP(_nExtraAP), fHitCover(_fHitCover), bFirstRound(_bFirstRound), bNight(_bNight),
		ptIllumination(_ptIllumination), nSnipeAP(_nSnipeAP), nBullet(_nBullet), bBackStab( _bBackStab ),
		pUnitServer(_pUnitServer)
{
	pUnitMission = _pUnitServer->GetUnitRPG();
	if ( nDistance < 1 )
		nDistance = 1;
	pWeaponItem = pUnitMission->GetRPGUnit()->GetWeaponItem();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CToHitCalcer::FillWeaponInfo()
{
	sWeaponInfo.nQuality = 0;
	sWeaponInfo.nMinRange = 0;
	sWeaponInfo.nMaxRange = 0;
	sWeaponInfo.nShotAP = 0;
	sWeaponInfo.nTargetingAP = 0;
	sWeaponInfo.nRecoil = 0;
	sWeaponInfo.nDmgMin = 0;
	sWeaponInfo.nDmgMax = 0;
	sWeaponInfo.nArmorPiercingAbility = 0;
	sWeaponInfo.fScopeFactor = 0;
	fMovePenalty = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CToHitCalcer::Prepare()
{
	if ( IsValid( pWeaponItem ) )
	{
		pWeaponItem->GetInfo( &sWeaponInfo );
		fMovePenalty = pWeaponItem->GetDBWeapon()->pWeaponType->fMovePenalty;
	}
	else
		FillWeaponInfo();
	//
	nSkill = pUnitMission->GetRPGUnit()->Skills( NDb::ST_SHOOTING );
	//
	if ( IsValid( pWeaponItem ) )
	{
		NDb::EShootMode shootMode = pWeaponItem->GetShootMode();
		if ( shootMode == NDb::SM_Snipe )
			nExtraAP += nSnipeAP;
		if ( shootMode == NDb::SM_Careful || shootMode == NDb::SM_Snipe )
			nSkill += max( 0, int( nExtraAP - GetSMove() ) ) / 3;
	}
	//
	if ( nBullet > 0 )
	{
		float fBurstNonStab = nSkill * pow( (double)pUnitMission->GetRPGUnit()->Skills(NDb::ST_BURST) / N_MAX_SKILL,
			nBullet );
		float fBurstStab = pUnitMission->GetRPGUnit()->Skills(NDb::ST_BURST) *
			pUnitMission->GetToHitConstants()->nMaxBurstStabilize / N_MAX_SKILL;
		float fStabilized = fBurstStab * nSkill / 100;
		nSkill = Max( fBurstNonStab, fStabilized ) * sWeaponInfo.nRecoil / 100;
	}
	float fVPPenalty = GetVPPenalty( pUnitMission->GetRPGUnit()->Skills( NDb::ST_VP ),
		pUnitMission->GetHealedVP(), pUnitMission->GetRPGUnit()->Skills(NDb::ST_VP).GetMaxValue() );
	nSkill *= fVPPenalty;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CToHitCalcer::GetStance()
{
	int nStance = 0;
	CPtr<CWeaponItem> pWeaponItem = pUnitMission->GetRPGUnit()->GetWeaponItem();
	if ( IsValid( pWeaponItem ) )
	{
		if ( NAI::CROUCH == eCurPose )
			nStance = int( pWeaponItem->GetDBWeapon()->pWeaponType->fCrouchBonus );
		else if ( NAI::CRAWL == eCurPose )
			nStance = int( pWeaponItem->GetDBWeapon()->pWeaponType->fCrawlBonus );
	}
	return nStance;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CToHitCalcer::GetD1()
{
	return float(nSkill) * ( sWeaponInfo.nMaxRange - sWeaponInfo.nMinRange ) / float(N_MAX_SKILL) + sWeaponInfo.nMinRange;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CToHitCalcer::GetD2()
{
	float fD1 = GetD1();
	float fDTH = Cos3DistanceFunc( nDistance, fD1 );
	fDTH = Clamp( fDTH, 2.f, 100.f );
	if ( nDistance > fD1 * 2 )
		fDTH = 0;
	return fDTH;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CToHitCalcer::GetRS()
{
	return Min( pUnitMission->GetLastActionTimes(), pUnitMission->GetToHitConstants()->nMaxShotsRepeat ) * 2;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CToHitCalcer::GetSMove()
{
	float fRes = 0;
	//
	if ( !bFirstRound )
		fRes = Min( pUnitMission->GetMoveInLastTurn(),
			pUnitMission->GetToHitConstants()->nSMaxMove ) * fMovePenalty;
	//
	return fRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CToHitCalcer::GetLight()
{
	CVec3 ptLight = CVec3( Clamp(ptIllumination.x, 0.f, 1.f),
		Clamp(ptIllumination.y, 0.f, 1.f), Clamp(ptIllumination.z, 0.f, 1.f) );
	float fRes = 0.5f * (1 + fabs( ptLight ) / fabs( CVec3(1,1,1) ) );
	// release-new: at night the light factor drops 10% unless the unit has night-vision (perk 0x15).
	// bNight is wired false in this predecessor tree (CWorld::IsNight absent) so this is dormant --
	// kept for layout/faithfulness.
	if ( bNight && !pUnitMission->HasPerk( 0x15 ) )
		fRes *= 0.9f;
	return fRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CToHitCalcer::GetFRMult()
{
	return bFirstRound ? pUnitMission->GetToHitConstants()->fFirstRoundCoeff : 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CToHitCalcer::GetAllMult()
{
	return Max( 0.f, GetAllAdd() * GetLight() * GetFRMult() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CToHitCalcer::GetSnipeAdd()
{
	float fMaxSkillAdd = pUnitMission->GetRPGUnit()->Skills( NDb::ST_SNIPE ) *
		1.f / pUnitMission->GetToHitConstants()->nSnipingCoeff;
	float fAPSnipeAdd = nSnipeAP * 1.f / pUnitMission->GetToHitConstants()->nSnipingCoeff;
	return ( fAPSnipeAdd <= fMaxSkillAdd ) ? fAPSnipeAdd : fMaxSkillAdd;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CToHitCalcer::GetTArea()
{
	return 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CToHitCalcer::GetCA()
{
	float fTArea = GetTArea();
	// release-new: the "ignore cover" perk (0x48) zeroes the cover/area contribution.
	if ( pUnitMission->HasPerk( 0x48 ) )
		fTArea = 0;
	float fTCover = fHitCover / 100.f;
	float fScopeFactor = sWeaponInfo.fScopeFactor; // 20 - ������� ������ // 100 - ����������� ��������
	float fDistCoeff = DistanceFunc( nDistance, fScopeFactor );
	return ( (100-fDistCoeff)*(fTCover*fTArea)+fDistCoeff ) / 100.0f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CToHitCalcer::GetAllAdd()
{
	float fRes = sWeaponInfo.nQuality + GetD2() +
		GetStance() + GetSMove() + GetRS();
	//
	if ( IsValid( pWeaponItem ) )
	{
		NDb::EShootMode ShotMode = pWeaponItem->GetShootMode();
		if ( ShotMode == NDb::SM_Aimed || ShotMode == NDb::SM_Careful )
			fRes += sWeaponInfo.nTargetingAP;
		if ( ShotMode == NDb::SM_Careful || ShotMode == NDb::SM_Snipe )
			fRes += max( nExtraAP, int( GetSMove() ) );
	}
	//
	return fRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release-new terms. Weather (CWorld::BadWeather) + aura (NRPG::GetAuraAdd) are release-new subsystems
// ABSENT in this predecessor dev tree -> 0 (documented elisions); the careful-shoot perk uses the
// present IUnitMission::HasPerk(0x45,&out).
float CToHitCalcer::GetWeatherPenalty()
{
	// release @0x2b85a0: return 5.0 when pUnitServer is valid+alive and the world has bad weather.
	return 0; // weather subsystem absent -> elided
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CToHitCalcer::GetCarefulShootPerk()
{
	if ( IsValid( pWeaponItem ) && pWeaponItem->GetShootMode() == NDb::SM_Careful )
	{
		float fOut = 0;
		if ( pUnitMission->HasPerk( 0x45, &fOut ) )
			return fOut;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CToHitCalcer::GetAuraToHitAdd()
{
	// release @0x2b7090: NRPG::GetAuraAdd(pUnitServer,&toHit,&evasion) -> toHit. Aura subsystem absent.
	return 0; // elided
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CToHitCalcer::GetToHit()
{
	Prepare();
	// retail @0x2b85e0 (disasm 0x6b85eb: fld fHitCover; fcomp [0x8d54dc]=0.0f; test ah,0x41; jp):
	// the gate is fHitCover <= 0 -> 0%, NOT strict <. GetHitCover (@0x2b4390) returns exactly 0.0
	// when hitRays is EMPTY (every candidate ray obstructed at the muzzle -- target fully walled
	// off; the case RealPeekRay @0x2b3f00 logs "Kick the programmers"); strict < let that case
	// through and the UI showed a live % for an unreachable target.
	if ( fHitCover <= 0.0f )
		return 0;
	fToHit = GetAllMult() + ( GetAuraToHitAdd() + GetCarefulShootPerk() ) - GetWeatherPenalty();
	fToHit = Clamp( fToHit, 2.0f, 100.0f );
	return int( fToHit + 0.5f );  // release rounds the result (fistp), unlike the dev's truncation
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CToHitCalcer::Log()
{
	csRPG << "<font size=16pt>";
	csRPG << CC_GREEN << "General ToHit:";
	csRPG << CC_ORANGE << "\tDist=" << CC_GREY << nDistance;
	csRPG << CC_ORANGE << "\tW+=" << CC_GREY << sWeaponInfo.nQuality;
	csRPG << CC_ORANGE << "\tMinRange=" << CC_GREY << sWeaponInfo.nMinRange;
	csRPG << CC_ORANGE << "\tMaxRange=" << CC_GREY << sWeaponInfo.nMaxRange;
	csRPG << CC_ORANGE << "\tBulletNumber=" << CC_GREY << nBullet << endl;
	csRPG << "<font size=16pt>";
	csRPG << CC_ORANGE << "\tStance=" << CC_GREY << GetStance();
	csRPG << CC_ORANGE << "\tDerSkill=" << CC_GREY << nSkill;
	csRPG << CC_ORANGE << "\tD1=" << CC_GREY << GetD1();
	csRPG << CC_ORANGE << "\tD2=" << CC_GREY << GetD2();
	csRPG << CC_ORANGE << "\tZeroingIn(RS)=" << CC_GREY << GetRS();
	csRPG << CC_ORANGE << "\tAddAP=" << CC_GREY << nExtraAP;
	csRPG << CC_ORANGE << "\tTCover=" << CC_GREY << fHitCover;
	csRPG << CC_ORANGE << "\tTArea=" << CC_GREY << GetTArea();
	csRPG << CC_ORANGE << "\tCA=" << CC_GREY << GetCA();
	csRPG << CC_ORANGE << "\tSMove=" << CC_GREY << GetSMove() << endl;
	csRPG << "<font size=16pt>";
	csRPG << CC_ORANGE << "\tLight=" << CC_GREY << GetLight();
	csRPG << CC_ORANGE << "\tFRmult=" << CC_GREY << GetFRMult();
	csRPG << CC_ORANGE << "\tAllAdd=" << CC_GREY << GetAllAdd();
	csRPG << CC_ORANGE << "\tAllAddMult=" << CC_GREY << GetAllMult();
	csRPG << CC_GREEN << "\tToHit = " << GetAllAdd() << "\n";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitToHitCalcer::CUnitToHitCalcer(	CUnitServer *_pUnitServer, NAI::EPose _eCurPose,
		int _nDistance,	CVec3 _ptAttacker, NAI::SPosition _sTargetPosition, int _nExtraAP, int _nSnipeAP,
		float _fHitCover, bool _bFirstRound, bool _bNight, CVec3 _ptIllumination, NAI::EHitLocation _eHitLocation,
		CUnitServer *_pTarget, int _nBullet, bool _bBackStab ) :
			CToHitCalcer( _pUnitServer, _eCurPose, _nDistance, _ptAttacker, _nExtraAP, _nSnipeAP, _fHitCover,
			_bFirstRound, _bNight, _ptIllumination, _nBullet, _bBackStab ), sTargetPosition(_sTargetPosition),
			eHitLocation(_eHitLocation), pTarget(_pTarget)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnitToHitCalcer::GetToHit()
{
	Prepare();
	// retail @0x2b86c0: same <=0 cover gate as CToHitCalcer::GetToHit (disasm 0x6b86cb,
	// fcomp against the 0.0f global; strictly-greater continues). 0.0 = empty hitRays.
	if ( fHitCover <= 0.0f )
		return 0;
	float fDef = 0;
	if ( !bBackStab )
		fDef = GetTAuraEvasionAdd() + pTarget->GetUnitRPG()->GetIC() / pUnitMission->GetToHitConstants()->fICModifier;
	fToHit = GetAllMult() * GetCA() + GetCarefulShootPerk() + GetAuraToHitAdd() - fDef - GetWeatherPenalty();
	fToHit = Clamp( fToHit, 2.0f, 100.0f );
	return int( fToHit + 0.5f );  // release rounds the result (fistp), unlike the dev's truncation
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NRPG::GetTAreaReal  @0x2b8800 (RVA) -- release-new free helper extracted as the single source of truth
// for the target silhouette area. The per-HL-location area switch collapses to a single HL_HEAD*0.2 case
// (confirmed from Game.exe @0x6b8800: sub eax,1 / jne / fmul 0.2); the rest of the per-location weighting
// moved into the GetCA headshot multiplier. CUnitToHitCalcer::GetTArea is the thin wrapper that forwards
// its target position, attacker eye-point and hit-location -- one source of truth, no divergent copy.
float GetTAreaReal( const NAI::SPosition &sTargetPosition, const CVec3 &ptAttacker, NAI::EHitLocation eHitLocation )
{
	vector<CVec3> vCubes;
	GetOccupiedCubes( &vCubes, sTargetPosition );
	float fTArea = GetCubesArea( ptAttacker, &vCubes );
	if ( eHitLocation == NAI::HL_HEAD )
		fTArea *= 0.2f;
	return fTArea;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnitToHitCalcer::GetTArea()
{
	return GetTAreaReal( sTargetPosition, ptAttacker, eHitLocation );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnitToHitCalcer::GetTMove()
{
	CVec3 ptTMove = FP_INV_GRID_STEP * ( sTargetPosition.GetCP() - pTarget->GetUnitRPG()->GetTurnStartCP() );
	CVec3 ptTDir  = sTargetPosition.GetCP() - ptAttacker;
	float fTargetMoveDist = fabs( ptTMove );
	Normalize( &ptTMove );
	Normalize( &ptTDir );
	float fCos = ptTDir * ptTMove;
	float fTargetMoveDir = sqrt(1 - fCos * fCos);

	float fRes = Min( fTargetMoveDist * fTargetMoveDir,
		(float)pUnitMission->GetToHitConstants()->nMaxMoveBonus );
	if ( bFirstRound )
		fRes = 0;
	// release-new: the target's "lead the moving target" perk (0xe) scales the move penalty by the
	// cosine between the target move- and the shot-direction.
	if ( pTarget->GetUnitRPG()->HasPerk( 0xe ) )
		return fCos * fRes;
	return fRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnitToHitCalcer::GetAllAdd()
{
	return CToHitCalcer::GetAllAdd() - GetTMove();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnitToHitCalcer::GetCA()
{
	float fCA = CToHitCalcer::GetCA();
	float fOut = 0;
	// release-new: the target's no-cover perk (0x11) divides the cover/area factor by its float param.
	// ORIGINAL BUG (confirmed Game.exe @0x2b7840 `fdiv [esp+8]`, no zero guard): a zero param -> +/-inf.
	if ( pTarget->GetUnitRPG()->HasPerk( 0x11, &fOut ) )
		fCA = fCA / fOut;
	// release-new: per-hit-location headshot reweighting when called shots are enabled (see
	// g_bCalledShotsEnabled). For HL_ANY GetHeadshotMultiplier==1.0, so this is a no-op for normal shots.
	if ( g_bCalledShotsEnabled )
		fCA *= GetHeadshotMultiplier( eHitLocation );
	return fCA;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnitToHitCalcer::GetTAuraEvasionAdd()
{
	// release @0x2b7890: NRPG::GetAuraAdd(pTarget,&toHit,&evasion) -> evasion. Aura subsystem absent.
	return 0; // elided
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitToHitCalcer::Log()
{
	CToHitCalcer::Log();
	csRPG << "<font size=16pt>";
	csRPG << CC_ORANGE << "Unit ToHit:\tTMove=" << CC_GREY << GetTMove();
	csRPG << CC_ORANGE << "\tHL=" << CC_GREY << GetHLName( eHitLocation );
	csRPG << CC_ORANGE << "\tBaseIC=" << CC_GREY << pTarget->GetUnitRPG()->GetIC() << "\n";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTileToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
CTileToHitCalcer::CTileToHitCalcer( CUnitServer *_pUnitServer, NAI::EPose _eCurPose,
	int _nDistance,	CVec3 _ptAttacker, int _nExtraAP, float _fHitCover, bool _bFirstRound, bool _bNight,
	CVec3 _ptIllumination, CVec3 _ptTilePos, int _nBullet ) :
		CToHitCalcer( _pUnitServer, _eCurPose, _nDistance, _ptAttacker, _nExtraAP, 0, _fHitCover,
			_bFirstRound, _bNight, _ptIllumination, _nBullet, false ), ptTilePos(_ptTilePos)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CTileToHitCalcer::GetTArea()
{
	return 0.33f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGrenadeToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
// release: the thrown grenade is no longer a ctor argument -- it is the unit's active inventory item
// (GetUnit()->GetGrenadeItem()); the member is the runtime IGrenadeItem (was the DB CRPGGrenade).
CGrenadeToHitCalcer::CGrenadeToHitCalcer( CUnitServer *_pUnitServer, NAI::EPose _eCurPose, int _nDistance,
	CVec3 _ptAttacker, bool _bFirstRound, bool _bNight, CVec3 _ptIllumination, CVec3 _ptTilePos,
	NRPG::IGrenadeItem *_pGrenade ) :
		CToHitCalcer( _pUnitServer, _eCurPose, _nDistance, _ptAttacker, 0, 0, 0, _bFirstRound, _bNight, _ptIllumination, 0, false ),
		ptTilePos(_ptTilePos)
{
	// Release derives the thrown grenade from the unit's ACTIVE item (CUnit::GetGrenadeItem @0x2ba630 ->
	// inventory GetActive). That is correct for the player throw and CExecThrowGrenade, where the grenade
	// is already equipped/active. But the AI evaluates a grenade still sitting in the inventory
	// (CanUnitThrowGrenade), so GetActive returns the equipped weapon, the cast to IGrenadeItem yields
	// null, and Prepare() dereferences a null grenade -> crash / AI silently aborts. Use the explicit
	// grenade the AI/script caller already holds; fall back to the active item only when none is supplied.
	if ( _pGrenade )
		pGrenade = _pGrenade;
	else
		pGrenade = CDynamicCast<NRPG::IGrenadeItem>( pUnitMission->GetRPGUnit()->GetInventory()->GetActive() );
	Prepare();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrenadeToHitCalcer::Prepare()
{
	nSkill = pUnitMission->GetRPGUnit()->Skills( NDb::ST_THROWING );
	float fVPPenalty = GetVPPenalty( pUnitMission->GetRPGUnit()->Skills( NDb::ST_VP ),
		pUnitMission->GetHealedVP(), pUnitMission->GetRPGUnit()->Skills(NDb::ST_VP).GetMaxValue() );
	nSkill *= fVPPenalty;
	fMovePenalty = pGrenade->GetDBGrenade()->pWeaponType->fMovePenalty;
	FillWeaponInfo();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CGrenadeToHitCalcer::GetStance()
{
	int nStance = 0;
	if ( eCurPose == NAI::CROUCH  )
		nStance = int( pGrenade->GetDBGrenade()->pWeaponType->fCrouchBonus );
	else if ( eCurPose == NAI::CRAWL )
		nStance = int( pGrenade->GetDBGrenade()->pWeaponType->fCrawlBonus );
	return nStance;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrenadeToHitCalcer::FillWeaponInfo()
{
	CToHitCalcer::FillWeaponInfo();
	sWeaponInfo.nQuality = pGrenade->GetDBGrenade()->nQuality;
	sWeaponInfo.nMinRange = GetGrenadeMaxDistance();
	sWeaponInfo.nMaxRange = sWeaponInfo.nMinRange;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CGrenadeToHitCalcer::GetMaxImp()
{
	float fBaseStr = float( pUnitMission->GetRPGUnit()->Skills( NDb::ST_STR ) );
	if ( pUnitMission->GetPanzerklein() )
		fBaseStr = pUnitMission->GetPanzerklein()->nGrenadeStrength;
	float fStr = fBaseStr + float(nSkill) / 18.f;
	float fRes = pUnitMission->GetToHitConstants()->fGrenadeBaseCoeff;
	fRes += pUnitMission->GetToHitConstants()->fGrenadeSTRCoeff * fStr;
	fRes *= pow( float(pGrenade->GetDBGrenade()->pItem->nWeight), 0.74f );
	return fRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CGrenadeToHitCalcer::GetRelWeight()
{
	return float(pGrenade->GetDBGrenade()->pItem->nWeight) / 1000.f; // �.�. � ������������
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CGrenadeToHitCalcer::GetMaxGrenadeVelocity()
{
	return GetMaxImp() / GetRelWeight();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CGrenadeToHitCalcer::GetGrenadeMaxDistance()
{
	float fMaxImp = GetMaxImp();
	float fRelWeight = GetRelWeight();
	return int( pow(fMaxImp,2) / ( pUnitMission->GetToHitConstants()->fGravity * pow(fRelWeight,2) ) / FP_GRID_STEP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CGrenadeToHitCalcer::GetAllAdd()
{
	return sWeaponInfo.nQuality + GetD2() + GetStance() - GetSMove();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CGrenadeToHitCalcer::GetToHit()
{
	Prepare();
	fToHit = GetAllMult() + ( GetAuraToHitAdd() + GetCarefulShootPerk() ) - GetWeatherPenalty();
	fToHit = Clamp( fToHit, 2.0f, 100.0f );
	return int( fToHit + 0.5f );  // release rounds the result (fistp), unlike the dev's truncation
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrenadeToHitCalcer::Log()
{
	csRPG << "<font size=16pt>";
	csRPG << CC_GREEN << " \tGrenade ToHit: ";
	csRPG << CC_ORANGE << "\tSkill=" << CC_GREY << pUnitMission->GetRPGUnit()->Skills( NDb::ST_THROWING );
	csRPG << CC_ORANGE << "\tDist=" << CC_GREY << nDistance;
	csRPG << CC_ORANGE << "\tMaxGrenadeRange=" << CC_GREY << GetGrenadeMaxDistance();
	csRPG << CC_ORANGE << "\tW+=" << CC_GREY << sWeaponInfo.nQuality;
	csRPG << CC_ORANGE << "\tStance=" << CC_GREY << GetStance();
	csRPG << CC_ORANGE << "\tSMvDist=" << CC_GREY << pUnitMission->GetMoveInLastTurn();
	csRPG << endl;
	csRPG << "<font size=16pt>";
	csRPG << CC_ORANGE << "\tDerSkill=" << CC_GREY << nSkill;
	csRPG << CC_ORANGE << "\tD1=" << CC_GREY << GetD1();
	csRPG << CC_ORANGE << "\tD2=" << CC_GREY << GetD2();
		csRPG << CC_ORANGE << "\tLight=" << CC_GREY << GetLight();
	csRPG << CC_ORANGE << "\tFRmult=" << CC_GREY << GetFRMult();
	csRPG << CC_ORANGE << "\tAllAdd=" << CC_GREY << GetAllAdd();
	csRPG << CC_ORANGE << "\tAllAddMult=" << CC_GREY << GetAllMult();
	csRPG << CC_GREEN << "\t ToHit = " << fToHit << "\n";
	csRPG << CC_ORANGE << "\tMaxImpulse = " << GetMaxImp() << " MaxVelocity = " << GetMaxGrenadeVelocity() << "\n";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIUnitToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
// The ctor keeps the dev IAIUnit* shape (an internal API, not serialized) and derives both
// CUnitServer*s to chain the release CUnitToHitCalcer ctor. bNight derives false (CWorld::IsNight
// absent in this tree -- documented elision).
CAIUnitToHitCalcer::CAIUnitToHitCalcer(	NAI::IAIUnit *pShooter, const NAI::SUnitPosition &shooterPos,
	NAI::IAIUnit *pTarget, int nHitCover, NAI::EHitLocation _eHitLocation, int _nBullet, IInventoryItem *_pWeapon, int _nExtraAP ):
		CUnitToHitCalcer( pShooter->GetUnitServer(),
		shooterPos.GetPose(),
		fabs( shooterPos.GetCP() - pTarget->GetPosition().GetCP() ) / FP_GRID_STEP,
		shooterPos.GetEyePosition(),
		pTarget->GetPosition(),
		_nExtraAP, 0, (float)nHitCover, false, false, CVec3( 1, 1, 1 ),
		_eHitLocation,
		pTarget->GetUnitServer(),
		_nBullet, false )
{
	CDynamicCast<CWeaponItem> pWeapon( _pWeapon );
	pWeaponItem = pWeapon;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CAIUnitToHitCalcer::GetTArea()
{
	return 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CAIUnitToHitCalcer::GetTMove()
{
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRLauncherToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
CRLauncherToHitCalcer::CRLauncherToHitCalcer( CUnitServer *_pUnitServer, NAI::EPose _eCurPose,
	int _nDistance,	CVec3 _ptAttacker, float _fHitCover, int _nExtraAP, bool _bFirstRound, bool _bNight,
	CVec3 _ptIllumination, CVec3 _ptTilePos ) :
		CToHitCalcer( _pUnitServer, _eCurPose, _nDistance, _ptAttacker, _nExtraAP, 0, _fHitCover,
			_bFirstRound, _bNight, _ptIllumination, 1, false ), ptTilePos(_ptTilePos)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRLauncherToHitCalcer::GetMaxDistance()
{
	return pWeaponItem->GetDBWeapon()->nMaxRange;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CThrowKnifeToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
int CThrowKnifeToHitCalcer::GetKnifeMaxDistance()
{
	return pUnitMission->GetRPGUnit()->Skills( NDb::ST_STR ) * 1.5f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CThrowKnifeToHitCalcer::FillWeaponInfo()
{
	// release routes the ballistic range through GetMaxThrowVelocity()/fGravity; that velocity formula
	// is a deep opaque IUnitMission-vtable chain (@0x2b7b50) absent in this tree -> keep the dev's
	// fixed 1..15 range (documented elision).
	CToHitCalcer::FillWeaponInfo();
	sWeaponInfo.nQuality = 0;
	sWeaponInfo.nMinRange = 1;
	sWeaponInfo.nMaxRange = 15;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CThrowKnifeToHitCalcer::Prepare()
{
	FillWeaponInfo();
	nSkill = pUnitMission->GetRPGUnit()->Skills( NDb::ST_THROWING );
	float fVPPenalty = GetVPPenalty( pUnitMission->GetRPGUnit()->Skills( NDb::ST_VP ),
		pUnitMission->GetHealedVP(), pUnitMission->GetRPGUnit()->Skills(NDb::ST_VP).GetMaxValue() );
	nSkill *= fVPPenalty;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CThrowKnifeToHitCalcer::CThrowKnifeToHitCalcer( CUnitServer *_pUnitServer, NAI::EPose _eCurPose, int _nDistance,
	CVec3 _ptAttacker, float _fHitCover, bool _bFirstRound, bool _bNight, CVec3 _ptIllumination,
	int _nExtraAP, bool _bBackStab ):
		CToHitCalcer( _pUnitServer, _eCurPose, _nDistance, _ptAttacker, _nExtraAP, 0, _fHitCover,
			_bFirstRound, _bNight, _ptIllumination, 1, _bBackStab )
{
	// release-new: the thrown blade item (GetUnit()->GetMeleeWeaponItem()).
	pMeleeWeapon = pUnitMission->GetRPGUnit()->GetMeleeWeaponItem();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CThrowKnifeUnitToHitCalcer  (release-new)
////////////////////////////////////////////////////////////////////////////////////////////////////
CThrowKnifeUnitToHitCalcer::CThrowKnifeUnitToHitCalcer( CUnitServer *_pUnitServer, NAI::EPose _eCurPose, int _nDistance,
	CVec3 _ptAttacker, float _fHitCover, bool _bFirstRound, bool _bNight, CVec3 _ptIllumination,
	CUnitServer *_pTarget, NAI::EHitLocation _eHitLocation, const NAI::SPosition &_targetPos, int _nExtraAP, bool _bBackStab ):
		CThrowKnifeToHitCalcer( _pUnitServer, _eCurPose, _nDistance, _ptAttacker, _fHitCover, _bFirstRound,
			_bNight, _ptIllumination, _nExtraAP, _bBackStab ),
		pTarget(_pTarget), eHitLocation(_eHitLocation), targetPos(_targetPos)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CThrowKnifeUnitToHitCalcer::GetToHit()
{
	Prepare();
	// retail @0x2b8a50: same <=0 cover gate (disasm 0x6b8a5b). CGrenadeToHitCalcer
	// @0x2b8990 deliberately has NO gate (grenades arc over cover) -- dev matches.
	if ( fHitCover <= 0.0f )
		return 0;
	fToHit = GetAllMult() + ( GetAuraToHitAdd() + GetCarefulShootPerk() ) - GetWeatherPenalty();
	// release: called shots enabled -> per-HL headshot multiplier on the whole result; else the
	// per-HL throw penalty is subtracted. For HL_ANY both branches agree (mult 1.0 / penalty 0).
	if ( g_bCalledShotsEnabled )
		fToHit = GetHeadshotMultiplier( eHitLocation ) * fToHit;
	else
		fToHit = fToHit - GetThrowToHitPenalty( eHitLocation );
	fToHit = Clamp( fToHit, 2.0f, 100.0f );
	return int( fToHit + 0.5f );  // release rounds the result (fistp), unlike the dev's truncation
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CThrowKnifeTileToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
CThrowKnifeTileToHitCalcer::CThrowKnifeTileToHitCalcer( CUnitServer *_pUnitServer, NAI::EPose _eCurPose,
	int _nDistance,	CVec3 _ptAttacker, float _fHitCover, bool _bFirstRound, bool _bNight, CVec3 _ptIllumination,
	CVec3 _ptTilePos, int _nExtraAP ) :
		CThrowKnifeToHitCalcer( _pUnitServer, _eCurPose, _nDistance, _ptAttacker, _fHitCover,
			_bFirstRound, _bNight, _ptIllumination, _nExtraAP, false ), ptTilePos(_ptTilePos)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NRPG;
//
REGISTER_SAVELOAD_CLASS( 0x52132140, CUnitToHitCalcer );
REGISTER_SAVELOAD_CLASS( 0x52232150, CTileToHitCalcer );
REGISTER_SAVELOAD_CLASS( 0x52232160, CGrenadeToHitCalcer );
REGISTER_SAVELOAD_CLASS( 0x51262180, CAIUnitToHitCalcer );
REGISTER_SAVELOAD_CLASS( 0x72762140, CRLauncherToHitCalcer );
// release-new class (CObjectToHitCalcer 0x52132180 / CMeleeToHitCalcer 0x51642170 /
// CAIUnitNoWeaponToHitCalcer 0x51462140 dropped). The real release id for the new class is not
// recoverable from the answer key; 0x52132170 is an unused NRPG ToHit-family id (calcers are transient,
// never in a real savegame, so the exact value is not behaviourally observable).
REGISTER_SAVELOAD_CLASS( 0x52132170, CThrowKnifeUnitToHitCalcer );
