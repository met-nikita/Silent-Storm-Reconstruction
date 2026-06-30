#ifndef __RPGBULLET_H_
#define __RPGBULLET_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// RPGBullet.obj -- NRPG loose-ray / attack-portion helpers the shipped engine hoisted
// out of CGame into free functions, plus the two carrier PODs they introduced
// (SAttackRayInfo, SCoverInterval).
//
// This lands the carrier types + the subset of the free functions that map cleanly
// onto already-present engine calls. The equivalent CGame:: members (CalcCovers,
// ProcessMeleeAttackPortion, ProcessRangedAttackPortion, ProcessThrowingAttackPortion)
// are left in place untouched -- nothing calls these new functions yet, so the build
// stays behaviour-neutral (parity surface).
//
// DEFERRED (genuine gaps -- see RPGBullet.cpp notes):
//   * TraceLooseRay / TraceLooseRaySegment depend on NRPG::CheckBulletToHit, a
//     per-bullet hit roll (RPGToHit module) that does NOT exist as a free function in
//     the dev tree (the dev implements to-hit via the CToHitCalcer hierarchy).
//   * CanHitTarget (needs the shooter diplomacy/relation -> ally mapping) and the
//     per-ray GetHitIntersections (reads a finished CCoverInfo) are deferred.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "RPGGame.h"      // NRPG::STrailPoint, NRPG::CAttackPortion, NRPG::IAttackable, EAttackResult, CRay, CObj/CPtr, vector, NDb::CRPGArmor
#include "aiRender.h"     // NAI::CFastRenderer::SResult / SSourceInfo
#include "aiPosition.h"   // NAI::SUnitPosition / SPosition / SPathPlace
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
	class CUnitServer;
	class IWorld;
}
namespace NAI
{
	class IAIMap;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// release-new tiny POD pushed by CalcCoverIntervals: one sample of the cumulative
// armor-piercing ("AP left") profile at a given distance along the ray.
struct SCoverInterval
{
	float fEnter;	// distance where this interval begins (sentinel -1e30 before the muzzle)
	float fK;		// AP left after crossing everything up to fEnter

	SCoverInterval() {}
	SCoverInterval( float _fEnter, float _fK ): fEnter( _fEnter ), fK( _fK ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// release-new carrier for the refactored ranged-attack path. Field order matches the
// shipped 148-byte layout (RPGBullet.obj); the exact byte size differs in the dev tree
// (the dev CAttackPortion carries the later damage-modifier members), but this is
// transient runtime state (never serialized), so only field identity matters.
struct SAttackRayInfo
{
	CObj<NWorld::CUnitServer> pUS;	// shooter (null for splinter / no-shooter rays)
	NAI::SUnitPosition from;		// shooter firing position
	bool bFirstTurn;
	int nExtraAP;
	int nBullet;
	CVec3 vOrigin;
	CVec3 vDir;
	vector<STrailPoint> trailPoints;
	bool bTargetIsHit;
	CObj<CObjectBase> pIgnore;		// object the shot ignores (the shooter's own hull)
	float fMinClearDistance;
	float fMaxRange;
	CAttackPortion atk;
	CObj<CObjectBase> pTarget;

	SAttackRayInfo();
	// 12-arg ctor @0x291330
	SAttackRayInfo( const CAttackPortion &atk, const CVec3 &vOrigin, const CVec3 &vDir,
		NWorld::CUnitServer *pUS, const NAI::SUnitPosition &from, int nBullet, int nExtraAP,
		bool bTargetIsHit, float fMinClearDistance, float fMaxRange, CObjectBase *pTarget );
	// 8-arg ctor @0x291450 (no shooter / no from-position)
	SAttackRayInfo( const CAttackPortion &atk, const CVec3 &vOrigin, const CVec3 &vDir,
		bool bTargetIsHit, float fMinClearDistance, float fMaxRange, CObjectBase *pTarget );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x291f00 -- initialize *dst as a vertical "splinter" ray (points straight up +Z).
void MakeSplinter( SAttackRayInfo *dst, const CAttackPortion *atk, const CVec3 *origin, float fMaxRange );
// @0x2914f0 -- build an accidental-shot ray seeded from the unit's clear-distance / ignore object.
void MakeAccidentalShot( SAttackRayInfo *dst, NWorld::CUnitServer *pUnit, const NAI::SUnitPosition &from,
	const CAttackPortion &atk, const CVec3 &vOrigin, const CVec3 &vDir, float fMaxRange );
// @0x290740 -- is there a weapon-blocking obstacle (other than pIgnore) within fMaxDist?
bool IsObstacleRay( NAI::CFastRenderer::SResult *pList, float fMaxDist, CObjectBase *pIgnore );
// @0x290780 -- throwing attack resolution (free-fn form of CGame::ProcessThrowingAttackPortion).
EAttackResult PerformThrowingAttackPortion( NWorld::IWorld *pWorld, CAttackPortion *pA, const CVec3 &vPlace,
	IAttackable *pTarget, NDb::CRPGArmor *pArmor, int nUserID );
// @0x290f70 -- melee attack resolution (free-fn form of CGame::ProcessMeleeAttackPortion + pFilter).
void PerformMeleeAttackPortion( NWorld::IWorld *pWorld, NAI::IAIMap *pAIMap, const CAttackPortion &a,
	const CRay &ray, const vector<IAttackable*> &ignores, CObjectBase *pFilter );
// @0x290d80 -- build the cumulative AP-left profile across the cover chain.
void CalcCoverIntervals( NAI::CFastRenderer::SResult *pList, const SAttackRayInfo &ray,
	NDb::CRPGArmor *pFallbackArmor, vector<SCoverInterval> *pOut );
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NRPG
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
