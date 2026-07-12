#include "StdAfx.h"
#include "aiMap.h"
#include "wExplTracker.h"
#include "RPGAttackMech.h"
#include "wMain.h"
#include "../dbformat/DataFormat.h"
#include "../dbformat/DataRPG.h"
#include "wUnitServer.h"
#include "wAckBase.h"
#include "RPGUnitMission.h"
#include "aiVoxelRender.h"
#include "wTSFlags.h"
#include "..\Misc\HPTimer.h"
#include "wDecal.h"
#include "..\Misc\EventsBase.h"   // NGlobal::ThrowEvent
#include "eventUnit.h"            // NWorld::CEventOnGrenadeExplosion (AI grenade-perception event)

namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x354b00: apply the thrower's explosive-perk modifiers to one blast CAttackPortion. The AE modifier
// scales the min/max damage (and combines with the structure modifier into fStructDmgModifier); the human-critical
// perk forces always-human-critical. With the neutral default {1,1,false} this is a no-op, so non-perk throwers and
// thrower-less blasts are unchanged.
void FillAttackModifiers( NRPG::CAttackPortion *pAttack, const SPerkMineModifiers &mods )
{
	pAttack->bAlwaysHumanCritical = mods.bAlwaysHumanCritical;
	pAttack->fStructDmgModifier   = mods.fAEDmgModifier * mods.fStructureDmgModifier;
	pAttack->nDmgMin = Float2Int( pAttack->nDmgMin * mods.fAEDmgModifier );
	pAttack->nDmgMax = Float2Int( pAttack->nDmgMax * mods.fAEDmgModifier );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release NWorld::nBreakCalcs -- the per-segment explosion work budget that paces the destruction ripple.
// Every 16^3 explosion-cube voxel trace bumps it (release CExplosionCube::Recalc @0x355080 tail; this
// build's equivalent trace is the CExplCube ctor's TraceVoxelGrid). CWorld::Segment resets it once per
// world segment (release ResetBreakExplCalcs @0x3549f0, invoked at the top of CExplosionMaster::Segment
// @0x3571d0). CVoxelExplTracker::Segment reads it: once >1 cube got traced this segment, the ring's
// damage pass -- and every later blast's stepping -- is postponed to the NEXT segment.
////////////////////////////////////////////////////////////////////////////////////////////////////
static int nBreakCalcs = 0;
void ResetBreakExplCalcs()
{
	nBreakCalcs = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Engineer-grenade blast scaling -- everything derives from d = nEngSkill - nSkillReq (retail
// CVoxelExpl ctor @0x3562c0 / tracker MakeDamage @0x3566d0; raw idiv, the DB guarantees nDeltaWave != 0):
//   rings  = d / nDeltaWave + nStartNWave
//   radius = d * fDeltaRadius * 0.025 + fWaveRadius
static int GetEngGrenadeWaves( NDb::CRPGEngGrenade *pEngGrenade, int nEngSkill )
{
	int nD = nEngSkill - pEngGrenade->nSkillReq;
	return nD / pEngGrenade->nDeltaWave + pEngGrenade->nStartNWave;
}
static float GetEngGrenadeRadius( NDb::CRPGEngGrenade *pEngGrenade, int nEngSkill )
{
	int nD = nEngSkill - pEngGrenade->nSkillReq;
	return nD * pEngGrenade->fDeltaRadius * 0.025f + pEngGrenade->fWaveRadius;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExplCube
////////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned short N_INDEX_OBJECT = 0xFFFF;
////////////////////////////////////////////////////////////////////////////////////////////////////
CExplCube::CExplCube( CVec3 _ptCenter, 
	NAI::IAIMap *_pAIMap, CVoxelExpl *_pExplosion ):
	ptCenter( _ptCenter ), pAIMap( _pAIMap ), pExplosion( _pExplosion ), 
	bFinished( false ), nFront( 1 ), nFrontSize(0), nEmpty( 1 )
{
	renderer.Init( ptCenter, F_REAL_CUBE_SIZE, N_REAL_CUBE_SIZE, &pExplosion->objects, &pExplosion->nObjectsEnd );
	pAIMap->TraceVoxelGrid( &renderer, NWorld::TS_FRAGMENTED );
	++nBreakCalcs;   // release CExplosionCube::Recalc @0x355080: each cube voxel-trace spends one unit of the per-segment budget
	pExplosion->damageInfo.resize( pExplosion->nObjectsEnd, CVoxelExpl::SObjectDamageInfo() );
	voxels.resize( N_REAL_CUBE_SIZE * N_REAL_CUBE_SIZE * N_REAL_CUBE_SIZE + 1 );
	neighborCubes.resize( 6 );
	neighborCubesCoords.resize( 6 );
	neighborCubesCoords[0] = ptCenter + CVec3( +F_CUBE_SIZE, 0, 0 );
	neighborCubesCoords[1] = ptCenter + CVec3( -F_CUBE_SIZE, 0, 0 );
	neighborCubesCoords[2] = ptCenter + CVec3( 0, 0, +F_CUBE_SIZE );
	neighborCubesCoords[3] = ptCenter + CVec3( 0, 0, -F_CUBE_SIZE );
	neighborCubesCoords[4] = ptCenter + CVec3( 0, +F_CUBE_SIZE, 0 );
	neighborCubesCoords[5] = ptCenter + CVec3( 0, -F_CUBE_SIZE, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CExplCube::GetDirection( int nX1, int nY1, int nZ1, int nX2, int nY2, int nZ2 )
{
	CVec3 ptDir = ( nX2 - nX1 ) * CVec3( 1, 0, 0 ) + 
		( nY2 - nY1 ) * CVec3( 0, 1, 0 ) + ( nZ2 - nZ1 ) * CVec3( 0, 0, 1 );
	Normalize( &ptDir );
	return ptDir;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CExplCube::GetVoxelCenter( int nX, int nY, int nZ )
{
	ASSERT( nX >=0 && nX < N_REAL_CUBE_SIZE );
	ASSERT( nY >=0 && nY < N_REAL_CUBE_SIZE );
	ASSERT( nZ >=0 && nZ < N_REAL_CUBE_SIZE );
	//
	CVec3 ptCoords;
	ptCoords.x = nX * F_VOXEL_SIZE + ptCenter.x - F_REAL_CUBE_SIZE * 0.5f;
	ptCoords.y = nY * F_VOXEL_SIZE + ptCenter.y - F_REAL_CUBE_SIZE * 0.5f;
	ptCoords.z = nZ * F_VOXEL_SIZE + ptCenter.z - F_REAL_CUBE_SIZE * 0.5f;
	return ptCoords;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExplCube::IsInCube( CVec3 ptCoords )
{
	float fHalfCube = F_CUBE_SIZE / 2.f;
	CVec3 ptHalfCube = CVec3( fHalfCube, fHalfCube, fHalfCube );
	CVec3 ptFirst = ptCenter - ptHalfCube;
	CVec3 ptSecond = ptCenter + ptHalfCube;
	return ( ( ptCoords.x >= ptFirst.x && ptCoords.x <= ptSecond.x ) &&
		( ptCoords.y >= ptFirst.y && ptCoords.y <= ptSecond.y ) &&
		( ptCoords.z >= ptFirst.z && ptCoords.z <= ptSecond.z ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CExplCube *CExplCube::GetNeighborCube( ENeighbourCube cube )
{
	CPtr<CExplCube> &pCube = neighborCubes[ (int)cube ];
	if ( !IsValid( pCube ) )
		pCube = pExplosion->GetExplCube( neighborCubesCoords[ (int)cube ] );
	//
	return pCube;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplCube::SubProcessBoundaryVoxel( CExplCube *pNeighborCube, 
	int nNX, int nNY, int nNZ, bool *bBoundaryVoxel )
{
	*bBoundaryVoxel = true;
	NAI::SExplVoxel &voxel = pNeighborCube->renderer.voxels[nNX][nNY][nNZ];
	if ( voxel.nIndex == 0 && voxel.nObject < NAI::N_VOXEL_TERRAIN )
		pNeighborCube->Front( nNX, nNY, nNZ );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExplCube::ProcessBoundaryVoxel( int nX, int nY, int nZ )
{
	bool bBoundaryVoxel = false;
	if ( renderer.voxels[nX][nY][nZ].nObject < NAI::N_VOXEL_TERRAIN )
	{
		if ( nZ == 0 )
			SubProcessBoundaryVoxel( GetNeighborCube( NC_UNDER ), nX, nY, N_CUBE_SIZE, &bBoundaryVoxel );
		else if ( nZ == N_REAL_CUBE_SIZE - 1 )
			SubProcessBoundaryVoxel( GetNeighborCube( NC_UPPER ), nX, nY, 1, &bBoundaryVoxel );
		else if ( nX == 0 )
			SubProcessBoundaryVoxel( GetNeighborCube( NC_RIGHT ), N_CUBE_SIZE, nY, nZ, &bBoundaryVoxel );
		else if ( nX == N_REAL_CUBE_SIZE - 1 )
			SubProcessBoundaryVoxel( GetNeighborCube( NC_LEFT ), 1, nY, nZ, &bBoundaryVoxel );
		else if ( nY == 0 )
			SubProcessBoundaryVoxel( GetNeighborCube( NC_NEAR ), nX, N_CUBE_SIZE, nZ, &bBoundaryVoxel );
		else if ( nY == N_REAL_CUBE_SIZE - 1 )
			SubProcessBoundaryVoxel( GetNeighborCube( NC_DISTANT ), nX, 1, nZ, &bBoundaryVoxel );
	}
	return bBoundaryVoxel;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplCube::ExpandFront( int nPX, int nPY, int nPZ, int nX, int nY, int nZ )
{
	int nData = renderer.voxels[nX][nY][nZ].nObject;
	if ( nData >= NAI::N_VOXEL_TERRAIN )
	{
		renderer.voxels[nX][nY][nZ].nIndex = N_INDEX_OBJECT;
		if ( nData > NAI::N_VOXEL_TERRAIN )
		{
			//NAI::CExplVoxelRenderer::SExplObject &object = pExplosion->objects[nData];
			CVoxelExpl::SObjectDamageInfo &object = pExplosion->damageInfo[ nData ];
			if ( object.nVolume == 0 )
			{
				object.nVolume = pExplosion->nVolume;
				object.rDir.ptDir = GetDirection( nPX, nPY, nPZ, nX, nY, nZ );
				object.rDir.ptOrigin = GetVoxelCenter( nPX, nPY, nPZ );
			}
		}
	}
	//
	bool bBoundaryVoxel = ProcessBoundaryVoxel( nX, nY, nZ );
	if ( nData < NAI::N_VOXEL_TERRAIN )
	{
		++pExplosion->nVolume;
		if ( !bBoundaryVoxel )
			Wave( nX, nY, nZ );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplCube::SubProcessNeighborVoxels( int nPX, int nPY, int nPZ, int nX, int nY, int nZ )
{
	if ( ( nX >= 0 && nX < N_REAL_CUBE_SIZE ) &&
			( nY >= 0 && nY < N_REAL_CUBE_SIZE ) &&
			( nZ >= 0 && nZ < N_REAL_CUBE_SIZE ) &&
			renderer.voxels[nX][nY][nZ].nIndex == 0 )
				ExpandFront( nPX, nPY, nPZ, nX, nY, nZ );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplCube::ProcessNeighborVoxels( int nX, int nY, int nZ )
{
	SubProcessNeighborVoxels( nX, nY, nZ, nX - 1, nY, nZ );
	SubProcessNeighborVoxels( nX, nY, nZ, nX + 1, nY, nZ );
	SubProcessNeighborVoxels( nX, nY, nZ, nX, nY - 1, nZ );
	SubProcessNeighborVoxels( nX, nY, nZ, nX, nY + 1, nZ );
	SubProcessNeighborVoxels( nX, nY, nZ, nX, nY, nZ - 1 );
	SubProcessNeighborVoxels( nX, nY, nZ, nX, nY, nZ + 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplCube::Wave( int nX, int nY, int nZ )
{
	voxels[nEmpty] = SExplVoxelCoords( nX, nY, nZ );
	renderer.voxels[nX][nY][nZ].nIndex = nEmpty;
	++nEmpty;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CExplCube::IsEmpty( int nX, int nY, int nZ )
{
	NAI::SExplVoxel &voxel = renderer.voxels[nX][nY][nZ];
	return voxel.nIndex == 0 && voxel.nObject < NAI::N_VOXEL_TERRAIN;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplCube::Front( int nX, int nY, int nZ )
{
	Wave( nX, nY, nZ );
	++nFrontSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplCube::MakeStep()
{
	if ( !bFinished )
	{
		for ( int n = nFront; n < nFront + nFrontSize; ++n )
		{
			const SExplVoxelCoords &coords = voxels[n];
			ProcessNeighborVoxels( coords.nX, coords.nY, coords.nZ );
		}
		//
		nFront = nFront + nFrontSize;
		nFrontSize = nEmpty - nFront;
		//
		bFinished = ( nFrontSize == 0 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVoxelExpl
////////////////////////////////////////////////////////////////////////////////////////////////////
CVoxelExpl::CVoxelExpl( CVec3 _ptCenter, int _nWave, NDb::CRPGGrenade *_pGrenade,
	CUnitServer *_pThrower, NAI::IAIMap *_pAIMap, CVoxelExplTracker* _pTracker, CObjectBase *_pIgnitionObject,
	NDb::CRPGEngGrenade *_pEngGrenade, int _nEngSkill ):
	ptCenter( _ptCenter ), pAIMap( _pAIMap ), pGrenade( _pGrenade ), pThrower( _pThrower ),
	pTracker( _pTracker ), nObjectsDestroyed( 0 ), nWave( _nWave ), nObjectsEnd( 0 ),
	tOverrun( 0 ), nCurrentCube( 0 ), bFinished( false ), nEnemyUnitsKilled( 0 ), pIgnitionObject( _pIgnitionObject ),
	pEngGrenade( _pEngGrenade ), nEngSkill( _nEngSkill )
{
	ASSERT( IsValid( pAIMap ) );
	//
	if ( IsValid( pGrenade ) )
	{
		// retail CVoxelExpl ctor @0x3562c0 (disasm 0x7563e5: cmp [grenade+0x14],1; jne multi-wave):
		// a SINGLE-wave grenade takes GetVolume(fWaveRadius) directly -- the 0.66/(nWaveNumber-1)
		// radius ramp only runs for multi-wave grenades. The unguarded division made every
		// WaveNumber==1 blast (small gas bottles etc.) compute a NaN -> INT_MIN budget: vol stayed
		// 0, so those blasts damaged ONLY the pre-seeded source object and contributed ZERO area
		// damage (an nMaxVolume of INT_MIN in the wave bookkeeping).
		if ( pGrenade->nWaveNumber == 1 )
			nMaxVolume = GetVolume( pGrenade->fWaveRadius );
		else
		{
			// dev nWave is 1-based: fA*nWave + (0.33 - fA) == 0.66/(w-1)*(nWave-1) + 0.33 == the
			// retail 0-based coeff 0.66/(nWaveNumber-1)*nCurrentWave + 0.33 -- keep as is.
			float fA = 0.66 / ( pGrenade->nWaveNumber - 1 );
			float fB = 0.33 - fA;
			nMaxVolume = GetVolume( pGrenade->fWaveRadius * ( fA * nWave + fB ) );
		}
	}
	else if ( IsValid( pEngGrenade ) )
	{
		// retail @0x3562c0 eng branch: skill-scaled radius/ring count, then the same 0.33 + 0.66/(waves-1)
		// per-ring radius ramp as the regular record (single-ring blasts take the radius directly).
		float fRadius = GetEngGrenadeRadius( pEngGrenade, nEngSkill );
		int nWaves = GetEngGrenadeWaves( pEngGrenade, nEngSkill );
		if ( nWaves == 1 )
			nMaxVolume = GetVolume( fRadius );
		else
		{
			float fA = 0.66 / ( nWaves - 1 );
			float fB = 0.33 - fA;
			nMaxVolume = GetVolume( fRadius * ( fA * nWave + fB ) );
		}
	}
	else
		nMaxVolume = GetVolume( 5 ); // for the AI Viewer (== the retail literal 0x3e6a fallback)
	//
	ExplodeWave();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVoxelExpl::Segment()
{
	const float F_EXPLOSION_TIME = 0.01f;
	//
	NHPTimer::STime tTime, tTmpTime;
	NHPTimer::GetTime( &tTime );
	//
	bool bComplete = false;
	tTmpTime = tTime;
	while ( !bComplete && nVolume < nMaxVolume && ( NHPTimer::GetTimePassed( &tTmpTime ) + tOverrun ) < F_EXPLOSION_TIME )
	{
		if ( !cubes.empty() )
		{
			cubes[nCurrentCube]->MakeStep();
			++nCurrentCube;
		}
		//
		if ( nCurrentCube == cubes.size() )
		{
			nCurrentCube = 0;
			//
			for ( list< CObj<CExplCube> >::iterator i = cubesToAdd.begin(); i != cubesToAdd.end(); ++i )
				cubes.push_back( *i );
			cubesToAdd.clear();
			//
			bComplete = true;
			for ( vector< CObj<CExplCube> >::iterator i = cubes.begin(); i != cubes.end(); ++i )
				if ( !(*i)->IsFinished() )
				{
					bComplete = false;
					break;
				}
		}
		//
		tTmpTime = tTime;
	}
	//
	tTmpTime = tTime;
	tOverrun += NHPTimer::GetTimePassed( &tTmpTime ) - F_EXPLOSION_TIME;
	//
	bFinished = bComplete || nVolume >= nMaxVolume;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release CVoxelExpl::MakeDamage @0x356580: the ring's damage is NOT applied while the wave is stepping --
// the tracker applies it in a separate per-segment damage pass (release CExplosionMaster::Segment @0x3571d0
// pass 2 -> CVoxelExplTracker::MakeDamage @0x3566d0), one segment AFTER a big ring finished computing.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVoxelExpl::MakeDamage()
{
	// retail @0x356580: the gate accepts EITHER live record (regular or engineer grenade)
	if ( bFinished && ( IsValid( pGrenade ) || IsValid( pEngGrenade ) ) )
	{
		ApplyWaveDamage();
		CheckWaveResults();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVoxelExpl::ExplodeWave()
{
	CPtr<CExplCube> pCube = GetExplCube( ptCenter + CVec3( 0, 0, + 1 / 3.f * F_CUBE_SIZE ) );
	bool bSeeded = false;
	// retail CVoxelExpl::StartWave @0x355830, PATH 1 -- an OBJECT-ORIGIN blast (gas tank / fuel barrel /
	// trapped door) seeds the wavefront from the SOURCE OBJECT's OWN voxels (every voxel of the ignition
	// object within a +/-7-voxel box around the blast centre), and pre-marks the source in damageInfo
	// (nVolume=1, blast direction straight up) so ApplyWaveDamage's `di.nVolume > 0` gate passes and the
	// source object is consumed by its own blast via the 10x ignition multiplier. The previous column-only
	// seeding started the wave deep INSIDE the source's voxels: object voxels don't propagate the front, so
	// most of the blast was absorbed by the tank's own hull (structures around it barely damaged -- retail
	// levels half a shed), and -- because the source was first touched while nVolume==0 -- the tank itself
	// NEVER took wave damage, never advanced a destroy stage, and never played its destroy sound / burning
	// model swap (grenades 39 "HeavyObjectExplosion" carry NO effect/sound records of their own, so the
	// destroy sound + visible destruction ARE the retail explosion's audio-visual for those objects).
	int nMatched = 0, nSeeds = 0;   // ignition entries matched / hull voxels seeded (the fallback sweep below keys on these)
	if ( IsValid( pIgnitionObject ) )
	{
		// local voxel coords of the blast centre inside the (freshly traced) seed cube
		const float fHalf = F_REAL_CUBE_SIZE * 0.5f;
		const int nCX = int( ( ptCenter.x - ( pCube->ptCenter.x - fHalf ) ) / F_VOXEL_SIZE );
		const int nCY = int( ( ptCenter.y - ( pCube->ptCenter.y - fHalf ) ) / F_VOXEL_SIZE );
		const int nCZ = int( ( ptCenter.z - ( pCube->ptCenter.z - fHalf ) ) / F_VOXEL_SIZE );
		for ( NAI::CExplVoxelRenderer::CObjectsHash::const_iterator i = objects.begin(); i != objects.end(); ++i )
		{
			const NAI::CExplVoxelRenderer::SExplObject &o = i->second;
			if ( o.bTerrain || !IsValid( o.pUserData ) || o.pUserData.GetPtr() != pIgnitionObject.GetPtr() )
				continue;
			if ( o.nObjectID <= NAI::N_VOXEL_TERRAIN || o.nObjectID >= (int)damageInfo.size() )
				continue;
			++nMatched;
			// retail: damageInfo[ignition].nVolume = 1, direction (0,0,1) -- the ignition damage record
			SObjectDamageInfo &di = damageInfo[ o.nObjectID ];
			if ( di.nVolume == 0 )
			{
				di.nVolume = 1;
				di.rDir.ptDir = CVec3( 0, 0, 1 );
				di.rDir.ptOrigin = ptCenter;
			}
			// seed every voxel of the source object inside the +/-7 box (retail's scan bounds), clamped to
			// the cube interior (the outermost layer is the neighbour-cube overlap border)
			for ( int nZ = Max( 1, nCZ - 7 ); nZ <= Min( N_REAL_CUBE_SIZE - 2, nCZ + 7 ); ++nZ )
				for ( int nY = Max( 1, nCY - 7 ); nY <= Min( N_REAL_CUBE_SIZE - 2, nCY + 7 ); ++nY )
					for ( int nX = Max( 1, nCX - 7 ); nX <= Min( N_REAL_CUBE_SIZE - 2, nCX + 7 ); ++nX )
					{
						NAI::SExplVoxel &voxel = pCube->renderer.voxels[nX][nY][nZ];
						if ( voxel.nIndex == 0 && (int)voxel.nObject == o.nObjectID )
						{
							pCube->Front( nX, nY, nZ );
							++nSeeds;
							bSeeded = true;
						}
					}
		}
		// Robustness vs retail: retail's +/-7 scan runs in the UNCLAMPED global voxel space (@0x355830 bounds
		// 1<c<0x3ff), but this build clamps to the single seed cube -- which is raised F_CUBE_SIZE/3 above the
		// blast centre, so a source hull hugging the cube's bottom/side border can be clipped out of the box.
		// If the source registered in the trace but the box found none of its voxels, sweep the whole cube
		// interior for them before giving up on PATH 1.
		if ( nMatched > 0 && nSeeds == 0 )
		{
			for ( NAI::CExplVoxelRenderer::CObjectsHash::const_iterator i = objects.begin(); i != objects.end(); ++i )
			{
				const NAI::CExplVoxelRenderer::SExplObject &o = i->second;
				if ( o.bTerrain || !IsValid( o.pUserData ) || o.pUserData.GetPtr() != pIgnitionObject.GetPtr() )
					continue;
				if ( o.nObjectID <= NAI::N_VOXEL_TERRAIN || o.nObjectID >= (int)damageInfo.size() )
					continue;
				for ( int nZ = 1; nZ <= N_REAL_CUBE_SIZE - 2; ++nZ )
					for ( int nY = 1; nY <= N_REAL_CUBE_SIZE - 2; ++nY )
						for ( int nX = 1; nX <= N_REAL_CUBE_SIZE - 2; ++nX )
						{
							NAI::SExplVoxel &voxel = pCube->renderer.voxels[nX][nY][nZ];
							if ( voxel.nIndex == 0 && (int)voxel.nObject == o.nObjectID )
							{
								pCube->Front( nX, nY, nZ );
								++nSeeds;
								bSeeded = true;
							}
						}
			}
		}
	}
	// retail StartWave PATH 2 (fallback, also used when the box scan found no source voxel): a thrown/aerial
	// blast seeds a small central column. Kept from 8eca71e: seed empty OR object voxels (skip only bare
	// terrain) so a blast resting against geometry still starts.
	if ( !bSeeded )
	{
		int nXY = N_CUBE_SIZE / 2;
		int nBaseZ = N_CUBE_SIZE / 2.f - N_CUBE_SIZE / 3.0f;//F_CUBE_SIZE / ( F_VOXEL_SIZE * 3.f );
		for ( int nDZ = 0; nDZ < 3; ++nDZ )
		{
			for ( int nDY = 0; nDY < 2; ++nDY )
			{
				for ( int nDX = 0; nDX < 2; ++nDX )
				{
					int nX = nXY + nDX;
					int nY = nXY + nDY;
					int nZ = nBaseZ + nDZ;
					NAI::SExplVoxel &seedVoxel = pCube->renderer.voxels[nX][nY][nZ];
					if ( seedVoxel.nIndex == 0 && seedVoxel.nObject != NAI::N_VOXEL_TERRAIN )
						pCube->Front( nX, nY, nZ );
				}
			}
		}
	}
	//
	nVolume = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVoxelExpl::ApplyWaveDamage()
{
	// retail @0x355ba0 record branch: every record-sourced input of the wave attack comes off whichever
	// record is live. The eng blast scales with d = nEngSkill - nSkillReq: damage d*fDamageModifier +
	// fStartWaveDamage (min == max), nK (d/30+1)*nAPAModifier (same /30 as ExplodeFragments), ring count
	// d/nDeltaWave + nStartNWave; crits/structure-coeff are the raw eng record fields.
	const bool bGrenadeRec = IsValid( pGrenade );
	float fWaveDmgMin, fWaveDmgMax;
	int nAPA, nCritProb, nCritDiff, nWavesTotal;
	float fStructDamageCoeff;
	if ( bGrenadeRec )
	{
		fWaveDmgMin = pGrenade->fWaveDmgMin;
		fWaveDmgMax = pGrenade->fWaveDmgMax;
		nAPA = pGrenade->nFragmentAPA;
		nCritProb = pGrenade->nCriticalProbability;
		nCritDiff = pGrenade->nCriticalDifficulty;
		nWavesTotal = pGrenade->nWaveNumber;
		fStructDamageCoeff = pGrenade->fStructureDamageCoeff;
	}
	else
	{
		int nD = nEngSkill - pEngGrenade->nSkillReq;
		fWaveDmgMin = fWaveDmgMax = nD * pEngGrenade->fDamageModifier + pEngGrenade->fStartWaveDamage;
		nAPA = ( nD / 30 + 1 ) * pEngGrenade->nAPAModifier;
		nCritProb = pEngGrenade->nCriticalProbability;
		nCritDiff = pEngGrenade->nCriticalDifficulty;
		nWavesTotal = GetEngGrenadeWaves( pEngGrenade, nEngSkill );
		fStructDamageCoeff = pEngGrenade->fStructureDamageCoeff;
	}
	for ( NAI::CExplVoxelRenderer::CObjectsHash::const_iterator i = objects.begin(); i != objects.end(); ++i )
	{
		const NAI::CExplVoxelRenderer::SExplObject &o = i->second;
		SObjectDamageInfo &di = damageInfo[ o.nObjectID ];
		// the blast's SOURCE object (retail nIgnitionObjectIdx compare @0x355ba0 == pointer identity here)
		const bool bIsSource = IsValid( pIgnitionObject ) && IsValid( o.pUserData ) &&
			o.pUserData.GetPtr() == pIgnitionObject.GetPtr();
		if ( !di.bPutDecal )
		{
			di.bPutDecal = true;
			CDynamicCast<NWorld::CUnitServer> pUS( o.pUserData );
			if ( !pUS && IsValid( o.pUserData ) )
			{
				CDynamicCast<NWorld::IBuilding> pB(o.pUserData);
				if (pB)
					pTracker->drawDecals[ pB->GetSceneHandle() ];
				else
					pTracker->drawDecals[ o.pUserData ];
			}
		}
		if ( !o.bTerrain && di.nVolume > 0 && IsValid( o.pUserData ) )
		{
			CDynamicCast<NRPG::IAttackable> pAtt(o.pUserData);
			if (pAtt)
			{
				float fCoeff = ( F_WAVE_ATTENUATION_COEFF - 1 ) / float( nMaxVolume ) * float( di.nVolume ) + 1;
				float fDamageMin = fWaveDmgMin;// * fCoeff;
				float fDamageMax = fWaveDmgMax;// * fCoeff;
				// retail @0x355ba0: the igniting object (a trapped barrel/door that set off THIS blast) takes 10x wave
				// damage to itself, so it is reliably consumed by its own explosion (o.nObjectID==nIgnitionObjectIdx ==
				// pointer identity here -- CWindowDoor's voxel pUserData IS its own CObjectBase, not a building proxy).
				if ( bIsSource )
				{
					fDamageMin *= F_IGNITION_OBJECT_DAMAGE_MULT;
					fDamageMax *= F_IGNITION_OBJECT_DAMAGE_MULT;
				}
				//
				list<int> &userIDs = pTracker->damagedObjects[o.pUserData];
				if ( find( userIDs.begin(), userIDs.end(), o.nUserID ) == userIDs.end() )
				{
					userIDs.push_back( o.nUserID );
					//
					CDynamicCast<NWorld::CUnitServer> pUS(o.pUserData);
					if (pUS)
					{
						// damage to Units
						if ( fDamageMax > 0 && !pUS->GetUnitRPG()->IsDead() && IsValid( pTracker ) )
						{
							if ( find( pTracker->damagedUnits.begin(), pTracker->damagedUnits.end(), pUS.GetPtr() ) ==
								pTracker->damagedUnits.end() )
							{
								pTracker->damagedUnits.push_back( pUS.GetPtr() );
								NRPG::IUnitMission* pRPG = 0;
								if ( IsValid( pThrower ) )
									pRPG = pThrower->GetUnitRPG();
								NRPG::CAttackPortion att( nAPA, 0, 0.0f, fDamageMin, fDamageMax,
									nCritProb * fCoeff, nCritDiff * fCoeff, pRPG, 0, fCoeff );   // retail @0x355ba0: fPushCoeff=0 (blast push = AddImpulse only)
								att.bNoBlowUp = false;   // retail @0x355ba0: the blast-wave unit portion CAN gib (byte store portion+0x39 = 0)
								if ( IsValid( pTracker ) )
									FillAttackModifiers( &att, pTracker->sMineModifiers );
								pAtt->ProcessAttack( o.nUserID, &att, o.pArmor );
								if ( ( !IsValid( pThrower ) || pUS->GetPlayer() != pThrower->GetPlayer() ) && ( !IsValid(pUS) || pUS->GetUnitRPG()->IsDead() ) )
									++nEnemyUnitsKilled;
							}
						}
						// retail @0x355ba0: on the FIRST wave the blast pushes the body along di.rDir whether or
						// not it was (still) attackable -- this is what ragdolls corpses/unconscious near a blast
						// (AddImpulse @0x3c0370 self-gates on downed/uncarried). Dev nWave is 1-based (see the
						// structure-branch note below): retail nCurrentWave==0 <=> nWave==1.
						if ( fDamageMax > 0 && nWave == 1 && IsValid( pUS ) )
							pUS->AddImpulse( di.rDir );
					}
					else
					{
						// damage to Structure
						// retail @0x355ba0 scales by (nWaveNumber - nCurrentWave + 1) with a 0-BASED wave counter;
						// the dev nWave is 1-based (the tracker pre-increments before spawning the ring), so convert
						// nCurrentWave == nWave-1. The previous "- nWave + 1" sat one wave ahead, costing every ring
						// one step of the structure multiplier (first ring N instead of N+1).
						fCoeff *= ( nWavesTotal - ( nWave - 1 ) + 1 ) * fStructDamageCoeff;
						NRPG::IUnitMission* pRPG = 0;
						if ( IsValid( pThrower ) )
							pRPG = pThrower->GetUnitRPG();
						NRPG::CAttackPortion att( nAPA, 0, 0.0f, fDamageMin, fDamageMax, 0, 0, pRPG, 0, fCoeff );   // retail ApplyWaveDamage @0x355ba0: fPushCoeff=0; bNoBlowUp stays TRUE on the structure portion
						att.rTtrajectory.ptDir = di.rDir.ptDir;
						att.rTtrajectory.ptOrigin = di.rDir.ptOrigin;
						att.atkType = NRPG::AT_BLAST_WAVE;
						if ( IsValid( pTracker ) )
							FillAttackModifiers( &att, pTracker->sMineModifiers );
						ASSERT( IsValid( o.pArmor ) );
						if ( IsValid( o.pArmor ) )
						{
							if ( pAtt->ProcessAttack( o.nUserID, &att, o.pArmor ) )
								di.bDestroyed = true;
						}
					}
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVoxelExpl::CheckWaveResults()
{
	for ( NAI::CExplVoxelRenderer::CObjectsHash::const_iterator i = objects.begin(); i != objects.end(); ++i )
	{
		const NAI::CExplVoxelRenderer::SExplObject &o = i->second;
		const SObjectDamageInfo &di = damageInfo[ o.nObjectID ];
		if ( !o.bTerrain && di.nVolume > 0 )
		{
			CDynamicCast<NWorld::CUnitServer> pUS(o.pUserData);
			if ( !IsValid( pUS ) )
				if ( di.bDestroyed )
					++nObjectsDestroyed;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CExplCube *CVoxelExpl::GetExplCube( CVec3 ptCoords )
{
	for ( vector< CObj<CExplCube> >::iterator i = cubes.begin(); i != cubes.end(); ++i )
		if ( (*i)->IsInCube( ptCoords ) )
			return *i;
	//
	CExplCube *pCube = new CExplCube( ptCoords, pAIMap, this );
	cubesToAdd.push_back( pCube );
	return pCube;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CVoxelExpl::GetVolume( float fRadius )
{
	float fRealRadius = fRadius * FP_GRID_STEP / F_VOXEL_SIZE;
	return 4.f / 3.f * PI * fRealRadius * fRealRadius * fRealRadius;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVoxelExplTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
CVoxelExplTracker::CVoxelExplTracker( CVec3 _ptCenter,
	NDb::CRPGGrenade *_pGrenade, CUnitServer *_pThrower, CWorld *_pWorld, CObjectBase *_pIgnitionObject, const SPerkMineModifiers *_pMods,
	NDb::CRPGEngGrenade *_pEngGrenade, int _nEngSkill ):
	ptCenter( _ptCenter ), pGrenade( _pGrenade ), pThrower( _pThrower ),
	pWorld( _pWorld ), nWave( 0 ), nEnemyUnitsKilled( 0 ), nObjectsDestroyed( 0 ), pIgnitionObject( _pIgnitionObject ),
	pEngGrenade( _pEngGrenade ), nEngSkill( _nEngSkill ),
	nLag( 2 )   // release @0x3571d0: after enqueuing a blast the master parks in S_WAIT (nLag=2) before the first ring spawns
{
	// A pre-placed source (a mine / trapped door) supplies the PLACER's perk modifiers explicitly -- the placer may be
	// gone by detonation, so they are stored on the mine (CMine::sPerkModifiers) and passed in here, not re-derived.
	if ( _pMods )
		sMineModifiers = *_pMods;
	// The a5dll derives a LIVE thrower's (thrown-grenade) modifiers HERE in the ctor rather than at the call site
	// (retail @0x356ff0 receives an already-Filled struct from AddGrenadeExplosion); functionally equivalent for
	// grenades. A live thrower overrides any passed mods BEFORE ExplodeFragments (which runs here) so the fragment
	// burst gets the perk bonus too. No a5dll caller passes BOTH a live thrower and explicit mods, so no double-apply.
	if ( IsValid( pThrower ) )
		sMineModifiers.Fill( pThrower->GetRPG()->GetRPGUnit() );
	if ( IsValid( pGrenade ) || IsValid( pEngGrenade ) )   // retail @0x356ff0: EITHER live record sprays fragments
		ExplodeFragments();
	//
	pAction = pWorld->GetActiveCounter( 30 );
	drawDecals[ pWorld->GetTerrainInfo() ];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail paces each blast as a slow radial ripple -- one destruction ring every ~4 world segments -- via
// CExplosionMaster::Segment @0x3571d0 (this build has no master object; its single-tracker state machine is
// reproduced here on the tracker itself):
//   S_WAIT   after a ring's damage lands, wait nLag=2 segments before the next ring;
//   step     spawn the next CVoxelExpl ring and drain it (release do{}while(MakeSingleStep @0x3565c0));
//   budget   if this segment traced >1 explosion cube (nBreakCalcs>1) return BEFORE the damage pass --
//            a big ring's destruction lands on the NEXT segment (release @0x3571d0 early return);
//   damage   release CVoxelExplTracker::MakeDamage @0x3566d0 -- apply the finished ring's wave damage,
//            accumulate the kill/destruction tallies, drop the ring (pExpl=0), then park in S_WAIT.
bool CVoxelExplTracker::Segment()
{
	// ---- S_WAIT: inter-ring lag countdown (release @0x3571d0: state==S_WAIT -> nLag-- and return) ----
	if ( nLag > 0 )
	{
		--nLag;
		return false;
	}
	// ---- budget spent by an earlier blast this segment: the master would already have returned ----
	if ( nBreakCalcs > 1 )
		return false;
	//
	// total ring count per record (release CVoxelExplTracker::MakeDamage @0x3566d0): regular = nWaveNumber,
	// engineer = (nEngSkill - nSkillReq) / nDeltaWave + nStartNWave; neither record -> nothing to spawn.
	const int nWavesTotal = IsValid( pGrenade ) ? pGrenade->nWaveNumber
		: ( IsValid( pEngGrenade ) ? GetEngGrenadeWaves( pEngGrenade, nEngSkill ) : 0 );
	//
	// ---- step pass: (re)spawn the next ring, then drain it (release MakeSingleStep @0x3565c0 spawns a
	// fresh CVoxelExpl whenever the previous one was dropped, iterating it in the same pass) ----
	if ( !IsValid( pExpl ) && nWave < nWavesTotal )
	{
		++nWave;
		pExpl = new CVoxelExpl( ptCenter, nWave, pGrenade, pThrower, pWorld->GetAIMap(), this, pIgnitionObject.GetPtr(), pEngGrenade, nEngSkill );
	}
	if ( IsValid( pExpl ) && !pExpl->IsFinished() )
		pExpl->Segment();
	//
	// ---- budget check: >1 cube traced -> damage lands NEXT segment (release @0x3571d0 early return) ----
	if ( nBreakCalcs > 1 )
		return false;
	//
	// ---- damage pass (release CVoxelExplTracker::MakeDamage @0x3566d0): apply the finished ring's damage,
	// THEN accumulate its tallies (the old accumulate-at-spawn read a freshly-zeroed ring and lost them all,
	// so OnGrenadeExplosion always reported 0), drop the ring, and park in S_WAIT for the next one ----
	if ( IsValid( pExpl ) && pExpl->IsFinished() )
	{
		pExpl->MakeDamage();
		nObjectsDestroyed += pExpl->nObjectsDestroyed;
		nEnemyUnitsKilled += pExpl->nEnemyUnitsKilled;
		pExpl = 0;
		if ( nWave < nWavesTotal )
			nLag = 2;   // release @0x3571d0: state=S_WAIT; nLag=2
	}
	//
	bool bDone = !IsValid( pExpl ) && nWave >= nWavesTotal;
	if ( bDone )
	{
		vector<CObjectBase*> targets;
		for ( CDecalsHash::iterator i = drawDecals.begin(); i != drawDecals.end(); ++i )
			targets.push_back( i->first );
		// release MakeDamage @0x3566d0: the scorch-decal base radius comes off whichever record is live
		// (regular fDecalRadius @+0x70, engineer fDecalRadius @+0x74); neither record -> radius 0.
		float fDecalRadius = 0;
		if ( IsValid( pGrenade ) )
			fDecalRadius = pGrenade->fDecalRadius;
		else if ( IsValid( pEngGrenade ) )
			fDecalRadius = pEngGrenade->fDecalRadius;
		if ( !targets.empty() && IsValid(pWorld) )
			new CDecal( pWorld, ptCenter + CVec3(0,0,0.3f), fDecalRadius * random.GetFloat( 0.8f, 1.2f), NDb::GetMaterial( 3264 ), targets );
	}
	if ( bDone && pThrower )
	{
		pWorld->GetGlobalAck()->OnGrenadeExplosion( pThrower,
			nEnemyUnitsKilled, nObjectsDestroyed );
		// retail CVoxelExplTracker::MakeDamage @0x3566d0: alert AI units within ~30m of the blast to the thrower
		// (an enemy thrower -> possibleEnemy; a friendly one -> ally-needs-help). OnGrenade measures thrower->unit.
		NGlobal::ThrowEvent( NWorld::CEventOnGrenadeExplosion( pThrower, ptCenter ) );
	}
	//
	return bDone;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x356a80 (disasm-decoded 2026-07-11): ONE portion off whichever record is live, stamped
// atkType=AT_FRAGMENT + bNoBlowUp=false (fragment kills CAN gib), fPushCoeff=0. Up to (count-20)
// fragments are AIMED -- each at a random hit location (0..5) of a random unit within the fragment
// range (GetUnitsNear; the picked unit is then removed so nobody soaks two aimed splinters) -- and
// the remainder (~20) fly in rejection-sampled uniform sphere directions (GetRandomSphereVector
// @0x354b60). The eng record scales with d = nEngSkill - nSkillReq: count = nEngSkill/5,
// nK = (d/30+1)*nAPAModifier, dmg = ROUND(fFragDmgModifier*10)..ROUND(fFragDmgModifier*40).
// No lower clamp on the aimed count (retail 0x756cd1..): a record with count<20 goes negative and
// the random loop fires (count - nTargeted) rays -- benign for the 0-damage records that hit it.
// KNOWN DIVERGENCE: retail caps every splinter at fFragmentRange*FP_GRID_STEP inside its bullet
// tracer (SAttackRayInfo.fMaxRange); this build's ProcessRangedAttackPortion has no per-attack
// range cap, so splinters fly to the first obstacle (docs/NEXT_SESSION_ITEMS_FOLLOWUPS.md #14).
void CVoxelExplTracker::ExplodeFragments()
{
	NRPG::IUnitMission* pRPG = 0;
	if ( pThrower )
		pRPG = pThrower->GetUnitRPG();

	int nAPA, nDmgMin, nDmgMax, nCritProb, nCritDiff, nFragments;
	float fRange;
	if ( IsValid( pGrenade ) )
	{
		nAPA = pGrenade->nFragmentAPA;
		nDmgMin = pGrenade->nFragmentDmgMin;
		nDmgMax = pGrenade->nFragmentDmgMax;
		nCritProb = pGrenade->nCriticalProbability;
		nCritDiff = pGrenade->nCriticalDifficulty;
		nFragments = pGrenade->nFragmentNumber;
		fRange = pGrenade->fFragmentRange * FP_GRID_STEP;
	}
	else if ( IsValid( pEngGrenade ) )
	{
		int nD = nEngSkill - pEngGrenade->nSkillReq;
		nAPA = ( nD / 30 + 1 ) * pEngGrenade->nAPAModifier;
		nDmgMin = Float2Int( pEngGrenade->fFragDmgModifier * 10.0f );
		nDmgMax = Float2Int( pEngGrenade->fFragDmgModifier * 40.0f );
		nCritProb = pEngGrenade->nCriticalProbability;
		nCritDiff = pEngGrenade->nCriticalDifficulty;
		nFragments = nEngSkill / 5;
		fRange = pEngGrenade->fFragmentRange * FP_GRID_STEP;
	}
	else
		return;

	NRPG::CAttackPortion att( nAPA, 1, 0.0f, nDmgMin, nDmgMax, nCritProb, nCritDiff, pRPG );   // fPushCoeff=0 (retail 3rd arg)
	att.atkType = NRPG::AT_FRAGMENT;   // retail 0x756bf3: mov ebx,3 -> portion+0x24
	att.bNoBlowUp = false;             // retail 0x756bfc: byte portion+0x39 = 0
	FillAttackModifiers( &att, sMineModifiers );   // CVoxelExplTracker holds the modifiers directly

	CRay ray;
	CVec3 &v = ray.ptDir;
	ray.ptOrigin = ptCenter;
	vector< NRPG::IAttackable * > ignores;
	CDynamicCast<NRPG::IAttackable> pIgnore( pIgnitionObject.GetPtr() );
	if ( pIgnore )
		ignores.push_back( pIgnore.GetPtr() );   // retail rayInfo.pIgnore = pIgnitionObject: the trapped source doesn't soak its own splinters

	// aimed fragments: min(units-in-range, count-20), one random hit location per picked unit
	list< CPtr<CUnitServer> > units;
	pWorld->GetUnitsNear( ptCenter, &units, fRange );
	int nTargeted = Min( (int)units.size(), nFragments - 20 );
	for ( int i = 0; i < nTargeted; ++i )
	{
		int nIdx = units.empty() ? 0 : (int)random.Get( (unsigned int)units.size() );   // retail: raw isaac % size
		list< CPtr<CUnitServer> >::iterator it = units.begin();
		for ( int k = 0; k < nIdx; ++k )
			++it;
		if ( IsValid( *it ) )
		{
			CVec3 pt;
			pWorld->GetAIMap()->GetUnitHLPos( &pt, CastToObjectBase( it->GetPtr() ), (int)random.Get( 6 ) );   // retail: isaac % 6
			v = pt - ptCenter;
			Normalize( &v );   // self-guarded against a zero delta, matching the retail fabs2 != 0 gate
			pWorld->PerformRangedAttack( att, ray, ignores, pWorld->GetTime()->GetValue(), 0, 0 );
		}
		units.erase( it );
	}
	// the rest fly in uniformly random directions (retail GetRandomSphereVector @0x354b60:
	// rejection-sample the unit ball, reject near-zero, then normalize)
	for ( int nRandom = nFragments - nTargeted; nRandom > 0; --nRandom )
	{
		float fN2;
		do
		{
			v.x = random.GetFloat( -1, 1 );
			v.y = random.GetFloat( -1, 1 );
			v.z = random.GetFloat( -1, 1 );
			fN2 = fabs2( v );
		} while ( fN2 > 1.0f || fN2 <= 0.001f );
		Normalize( &v );
		pWorld->PerformRangedAttack( att, ray, ignores, pWorld->GetTime()->GetValue(), 0, 0 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NWorld;
//
REGISTER_SAVELOAD_CLASS( 0x51682140, CVoxelExpl )
REGISTER_SAVELOAD_CLASS( 0x52382160, CExplCube )
REGISTER_SAVELOAD_CLASS( 0x52782130, CVoxelExplTracker )