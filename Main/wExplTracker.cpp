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
#include "wTerrain.h"             // CTerrain::DrawExplosion (blast grass scorch)
#include "wMisc.h"                // CreateDGrassEvent

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
// release NWorld::nBreakCalcs -- the per-segment explosion work budget that paces the destruction
// ripple. Every 16^3 explosion-cube voxel trace bumps it (release CExplosionCube::Recalc @0x355080
// tail). CExplosionMaster::Segment @0x3571d0 resets it at its top (release ResetBreakExplCalcs
// @0x3549f0) and bails between blasts once >1 cube got traced; CVoxelExpl::MakeSingleIteration
// throttles to one front voxel per call while it stays >1.
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
// CVoxelExpl -- the retail GLOBAL-voxel wavefront (W5: replaces the dev per-blast CExplCube machine)
////////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned short N_INDEX_OBJECT = 0xFFFF;
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::GetDirection @0x354a40: normalized direction from voxel 1 to voxel 2
CVec3 CVoxelExpl::GetDirection( int nX1, int nY1, int nZ1, int nX2, int nY2, int nZ2 )
{
	CVec3 ptDir = ( nX2 - nX1 ) * CVec3( 1, 0, 0 ) +
		( nY2 - nY1 ) * CVec3( 0, 1, 0 ) + ( nZ2 - nZ1 ) * CVec3( 0, 0, 1 );
	Normalize( &ptDir );
	return ptDir;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail ctor @0x3562c0: volume budget off whichever record is live, then the ring's OWN
// C3DLookupTable over the shared space, then StartWave (the ignition object passes through --
// retail stores only its SPACE id, nIgnitionObjectIdx).
CVoxelExpl::CVoxelExpl( CWorld *_pWorld, CVec3 _ptCenter, CObjectBase *_pIgnitionObject, int _nCurrentWave,
	NDb::CRPGGrenade *_pGrenade, CUnitServer *_pThrower, CVoxelExplTracker *_pTracker,
	CExplosionSpace *_pSpace, NDb::CRPGEngGrenade *_pEngGrenade, int _nEngSkill ):
	bFinished( false ), ptCenter( _ptCenter ), pThrower( _pThrower ), pGrenade( _pGrenade ),
	pSpace( _pSpace ), pTracker( _pTracker ), nCurrentWave( _nCurrentWave ),
	nObjectsDestroyed( 0 ), nEnemyUnitsKilled( 0 ), nNewFrontSize( 0 ),
	pEngGrenade( _pEngGrenade ), nEngSkill( _nEngSkill ), pWorld( _pWorld ), nIgnitionObjectIdx( -1 )
{
	if ( IsValid( pGrenade ) )
	{
		// retail @0x3562c0 (disasm 0x7563e5: cmp [grenade+0x14],1; jne multi-wave): a SINGLE-wave
		// grenade takes GetVolume(fWaveRadius) directly -- the 0.66/(nWaveNumber-1) radius ramp only
		// runs for multi-wave grenades (nCurrentWave is the retail 0-BASED ring index).
		if ( pGrenade->nWaveNumber == 1 )
			nMaxVolume = GetVolume( pGrenade->fWaveRadius );
		else
			nMaxVolume = GetVolume( pGrenade->fWaveRadius *
				( 0.66f / ( pGrenade->nWaveNumber - 1 ) * nCurrentWave + 0.33f ) );
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
			nMaxVolume = GetVolume( fRadius * ( 0.66f / ( nWaves - 1 ) * nCurrentWave + 0.33f ) );
	}
	else
		nMaxVolume = 0x3e6a;   // retail literal fallback (the AI viewer's record-less blast)
	//
	// retail: the ring builds its own lookup grid over the shared space (registers in its flush list)
	pLookup = new C3DLookupTable( pSpace );
	//
	StartWave( _pIgnitionObject );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CVoxelExpl::MakeSingleIteration @0x355610: advance the flood fill by one wave (or, once
// nBreakCalcs>1, by one voxel per call). Walks `front` from the resume cursor, flooding each voxel's
// six neighbours into `newFront`; then trims newFront to its logical count, swaps the double
// buffers, clears the swapped-out buffer and bumps the wave counter. bFinished when nothing new was
// claimed or the volume budget is spent.
void CVoxelExpl::MakeSingleIteration()
{
	// (1) fresh pass over `front`: reset the running count and pre-grow newFront so the flood can
	// write up to six neighbours per front voxel by INDEX (retail resizes to capacity, then -- if
	// still smaller than front.size()*6 -- to front.size()*12 + 10; the surplus is trimmed below).
	if ( nCurrentFrontElement == 0 )
	{
		nNewFrontSize = 0;
		newFront.resize( newFront.capacity() );
		if ( (int)newFront.size() < (int)front.size() * 6 )
			newFront.resize( front.size() * 12 + 10 );
	}
	// (2) walk `front` from the resume cursor
	int i = nCurrentFrontElement;
	if ( i < (int)front.size() )
	{
		do
		{
			const SExplVoxelCoords coords = front[i];
			ProcessNeighborVoxels( coords.nX, coords.nY, coords.nZ );
			++i;
			// (3) throttle: >1 cube traced this segment -> one voxel per call; persist the cursor
			if ( nBreakCalcs > 1 )
			{
				nCurrentFrontElement = i;
				return;
			}
		} while ( i < (int)front.size() );
	}
	// (4) done when nothing new was claimed this wave, or the volume budget is spent
	bFinished = ( nNewFrontSize == 0 ) || ( nMaxVolume <= nVolume );
	// (5) trim newFront to its logical count, swap the double buffers, drop the stale one
	newFront.resize( nNewFrontSize );
	front.swap( newFront );
	newFront.clear();
	//
	++nIteration;
	nCurrentFrontElement = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x355590: flood the six face-neighbours of (nX,nY,nZ)
void CVoxelExpl::ProcessNeighborVoxels( int nX, int nY, int nZ )
{
	SubProcessNeighborVoxels( nX, nY, nZ, nX - 1, nY, nZ );
	SubProcessNeighborVoxels( nX, nY, nZ, nX + 1, nY, nZ );
	SubProcessNeighborVoxels( nX, nY, nZ, nX, nY - 1, nZ );
	SubProcessNeighborVoxels( nX, nY, nZ, nX, nY + 1, nZ );
	SubProcessNeighborVoxels( nX, nY, nZ, nX, nY, nZ - 1 );
	SubProcessNeighborVoxels( nX, nY, nZ, nX, nY, nZ + 1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x355480 (raw disasm 0x755480, decoded 2026-07-14): resolve the neighbour voxel through
// the lookup table. An unvisited EMPTY voxel is claimed into newFront (cell stamped with the wave
// number); an unvisited OBJECT/terrain voxel is stamped 0xFFFF, and a real object gets its first-
// touch damageInfo entry: nVolume-at-touch, blast direction parent->voxel, impact origin = the
// CLAIMED voxel's own centre (disasm 0x755513..3d: GetCenter(pSpace, nX, nY, nZ, &rDir.ptOrigin)).
void CVoxelExpl::SubProcessNeighborVoxels( int nPX, int nPY, int nPZ, int nX, int nY, int nZ )
{
	unsigned short nObj;
	unsigned short *pCell;
	pLookup->GetObject( nX, nY, nZ, &nObj, &pCell );
	if ( *pCell != 0 )
		return;   // already visited by this ring
	if ( nObj == 0 )
	{
		// empty voxel: claim it (the buffer was pre-grown by MakeSingleIteration -- indexed write)
		newFront[ nNewFrontSize ] = SExplVoxelCoords( nX, nY, nZ );
		++nNewFrontSize;
		++nVolume;
		*pCell = (unsigned short)nIteration;
	}
	else
	{
		*pCell = N_INDEX_OBJECT;   // touched object/terrain voxel
		if ( nObj > NAI::N_VOXEL_TERRAIN )
		{
			SObjectDamageInfo &di = damageInfo[ (int)nObj ];
			if ( di.nVolume == 0 )
			{
				di.nVolume = nVolume;
				di.rDir.ptDir = GetDirection( nPX, nPY, nPZ, nX, nY, nZ );
				pSpace->GetCenter( nX, nY, nZ, &di.rDir.ptOrigin );
			}
		}
	}
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
// retail CVoxelExpl::StartWave @0x355830 -- seed the wavefront.
// PATH 1 (an OBJECT-ORIGIN blast: gas tank / fuel barrel / trapped door): resolve the centre voxel
// (which rasterises its cube and registers the local objects in the SPACE's hash), find the
// ignition object's blast-object id, pre-mark it in damageInfo (nVolume=1, direction straight up,
// nIgnitionObjectIdx remembered -- ApplyWaveDamage's source test + 10x self-damage key on it), then
// scan the +/-7 GLOBAL-voxel box around the centre (bounds 0<x,y<0x3ff, 0<z<0x7f): every voxel of
// the source object seeds `front`, its index cell marked 1.
// PATH 2 (fallback, also when the box scan found no source voxel): a 3-voxel vertical column at the
// centre, seeded UNCONDITIONALLY (retail gates only the 0x7f upper z bound).
void CVoxelExpl::StartWave( CObjectBase *pIgnitionObject )
{
	SExplVoxelCoords sCoords;
	pSpace->GetCoord( &ptCenter, &sCoords );
	const int nCX = sCoords.nX, nCY = sCoords.nY, nCZ = sCoords.nZ;
	unsigned short nObj;
	unsigned short *pCell;
	int nSeeds = 0;
	if ( IsValid( pIgnitionObject ) )
	{
		// prime the centre cube: the resolve rasterises it, registering its objects in the space hash
		pLookup->GetObject( nCX, nCY, nCZ, &nObj, &pCell );
		// the ignition object's blast-object id (retail keeps the LAST matching hash entry)
		unsigned short nIgnition = 0;
		for ( NAI::CExplVoxelRenderer::CObjectsHash::const_iterator i = pSpace->objects.begin(); i != pSpace->objects.end(); ++i )
			if ( i->first.pUser.GetPtr() == pIgnitionObject )
				nIgnition = (unsigned short)i->second.nObjectID;
		if ( nIgnition != 0 )
		{
			// retail: damageInfo[ignition] = { nVolume=1, dir (0,0,1) } + remember the source id
			SObjectDamageInfo &di = damageInfo[ (int)nIgnition ];
			di.nVolume = 1;
			di.rDir.ptDir = CVec3( 0, 0, 1 );
			nIgnitionObjectIdx = nIgnition;
			// the +/-7 box scan in GLOBAL voxel space
			for ( int nDZ = -7; nDZ < 8; ++nDZ )
			{
				const int nZ = nCZ + nDZ;
				if ( nZ <= 0 || nZ >= 0x7f )
					continue;
				for ( int nDY = -7; nDY < 8; ++nDY )
				{
					const int nY = nCY + nDY;
					if ( nY <= 0 || nY >= 0x3ff )
						continue;
					for ( int nDX = -7; nDX < 8; ++nDX )
					{
						const int nX = nCX + nDX;
						if ( nX <= 0 || nX >= 0x3ff )
							continue;
						pLookup->GetObject( nX, nY, nZ, &nObj, &pCell );
						if ( nObj == nIgnition )
						{
							front.push_back( SExplVoxelCoords( nX, nY, nZ ) );
							++nSeeds;
							*pCell = 1;
						}
					}
				}
			}
		}
	}
	if ( nSeeds == 0 )
	{
		// PATH 2: the central 3-voxel column (z upper bound only, matching retail)
		for ( int nDZ = 0; nDZ < 3; ++nDZ )
		{
			if ( nCZ + nDZ >= 0x7f )
				continue;
			front.push_back( SExplVoxelCoords( nCX, nCY, nCZ + nDZ ) );
			pLookup->GetObject( nCX, nCY, nCZ + nDZ, &nObj, &pCell );
			*pCell = 1;
		}
	}
	//
	nVolume = 0;
	nIteration = 2;
	nCurrentFrontElement = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CVoxelExpl::ApplyWaveDamage()
{
	// retail @0x355ba0 top guard: the whole damage pass requires a live world
	if ( !IsValid( pWorld ) )
		return;
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
	// retail: iterate the SPACE's blast-object registry; only objects the wave actually touched have a
	// damageInfo entry (retail's inlined bucket-walk find -- no entry, no processing).
	for ( NAI::CExplVoxelRenderer::CObjectsHash::const_iterator i = pSpace->objects.begin(); i != pSpace->objects.end(); ++i )
	{
		const NAI::CExplVoxelRenderer::SExplObject &o = i->second;
		CDamageInfoHash::iterator itDi = damageInfo.find( o.nObjectID );
		if ( itDi == damageInfo.end() )
			continue;
		SObjectDamageInfo &di = itDi->second;
		// the blast's SOURCE object (retail @0x355ba0: o.nObjectID == nIgnitionObjectIdx -- id compare,
		// NOT pointer identity)
		const bool bIsSource = ( o.nObjectID == nIgnitionObjectIdx );
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
								pAtt->ProcessAttack( pWorld, o.nUserID, &att, di.rDir.ptDir, o.pArmor );
								if ( ( !IsValid( pThrower ) || pUS->GetPlayer() != pThrower->GetPlayer() ) && ( !IsValid(pUS) || pUS->GetUnitRPG()->IsDead() ) )
									++nEnemyUnitsKilled;
							}
						}
						// retail @0x355ba0: on the FIRST ring (nCurrentWave == 0, retail 0-based) the blast pushes
						// the body along di.rDir whether or not it was (still) attackable -- this is what ragdolls
						// corpses/unconscious near a blast (AddImpulse @0x3c0370 self-gates on downed/uncarried).
						if ( fDamageMax > 0 && nCurrentWave == 0 && IsValid( pUS ) )
							pUS->AddImpulse( di.rDir );
					}
					else
					{
						// damage to Structure
						// retail @0x355ba0 scales by (nWaveNumber - nCurrentWave + 1) with the 0-BASED wave counter
						fCoeff *= ( nWavesTotal - nCurrentWave + 1 ) * fStructDamageCoeff;
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
							if ( pAtt->ProcessAttack( pWorld, o.nUserID, &att, di.rDir.ptDir, o.pArmor ) )
								di.bDestroyed = true;
						}
					}
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3553a0: tally destroyed non-unit objects. NB retail reads damageInfo through operator[]
// (a missed object INSERTS a default zero entry) -- kept 1:1.
void CVoxelExpl::CheckWaveResults()
{
	for ( NAI::CExplVoxelRenderer::CObjectsHash::const_iterator i = pSpace->objects.begin(); i != pSpace->objects.end(); ++i )
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
// retail CVoxelExpl::GetFront @0x354e10: world-space cell centres of the live front (AI viewer)
void CVoxelExpl::GetFront( vector<CVec3> *pRes )
{
	pRes->resize( front.size() );
	for ( int i = 0; i < (int)front.size(); ++i )
	{
		(*pRes)[i].x = ( front[i].nX + 0.5f ) * F_VOXEL_SIZE;
		(*pRes)[i].y = ( front[i].nY + 0.5f ) * F_VOXEL_SIZE;
		(*pRes)[i].z = ( front[i].nZ + 0.5f ) * F_VOXEL_SIZE;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CVoxelExpl::GetTouchedObjects @0x354f10: append the world-space centre of every voxel the
// blast touched (the 0xFFFF index-cell sentinel) across all occupied cubes of the lookup grid.
void CVoxelExpl::GetTouchedObjects( vector<CVec3> *pRes )
{
	vector<C3DLookupTable::SIndexCubeInfo> cubes;
	pLookup->GetAllCubes( &cubes );
	for ( int c = 0; c < (int)cubes.size(); ++c )
	{
		const C3DLookupTable::SIndexCubeInfo &info = cubes[c];
		for ( int nZ = 0; nZ < N_CUBE_SIZE; ++nZ )
			for ( int nY = 0; nY < N_CUBE_SIZE; ++nY )
				for ( int nX = 0; nX < N_CUBE_SIZE; ++nX )
					if ( info.p->data[ ( nZ * N_CUBE_SIZE + nY ) * N_CUBE_SIZE + nX ] == N_INDEX_OBJECT )
						pRes->push_back( CVec3(
							( info.x * N_CUBE_SIZE + nX + 0.5f ) * F_VOXEL_SIZE,
							( info.y * N_CUBE_SIZE + nY + 0.5f ) * F_VOXEL_SIZE,
							( info.z * N_CUBE_SIZE + nZ + 0.5f ) * F_VOXEL_SIZE ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail GetVolume (inlined @0x3562c0 as radius*3.125 == FP_GRID_STEP/F_VOXEL_SIZE, then
// ROUND(4/3 * pi * r^3) -- Float2Int is the same fistp round-to-nearest)
int CVoxelExpl::GetVolume( float fRadius )
{
	float fRealRadius = fRadius * FP_GRID_STEP / F_VOXEL_SIZE;
	return Float2Int( 4.f / 3.f * PI * fRealRadius * fRealRadius * fRealRadius );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVoxelExplTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail unified ctor @0x356ff0: adopt the space/records/ignition object, copy the ALREADY-FILLED
// modifiers struct (a pre-placed source passes the PLACER's stored mods; a live thrower's perks are
// Filled by CWorld::AddGrenadeExplosion -- W5 moved the Fill there, retail shape), spray the
// fragment burst, grab the action counter and pre-register the terrain as a decal target.
CVoxelExplTracker::CVoxelExplTracker( CExplosionSpace *_pSpace, CVec3 _ptCenter, CObjectBase *_pIgnitionObject,
	NDb::CRPGGrenade *_pGrenade, CUnitServer *_pThrower, const SPerkMineModifiers &_modifiers,
	CWorld *_pWorld, NDb::CRPGEngGrenade *_pEngGrenade, int _nEngSkill ):
	nWave( 0 ), ptCenter( _ptCenter ), pGrenade( _pGrenade ), pThrower( _pThrower ),
	nEnemyUnitsKilled( 0 ), nObjectsDestroyed( 0 ), pWorld( _pWorld ),
	sMineModifiers( _modifiers ), bIsFinished( false ), pSpace( _pSpace ),
	pIgnitionObject( _pIgnitionObject ), pEngGrenade( _pEngGrenade ), nEngSkill( _nEngSkill )
{
	if ( IsValid( pGrenade ) || IsValid( pEngGrenade ) )   // retail @0x356ff0: EITHER live record sprays fragments
		ExplodeFragments();
	//
	pAction = pWorld->GetActiveCounter( 30 );
	drawDecals[ pWorld->GetTerrainInfo() ];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CVoxelExplTracker::MakeSingleStep @0x3565c0: (re)spawn the current ring when it was
// dropped, then advance it one iteration. Returns true while the ring still steps (the master
// drains it with do{}while); false once the ring settled (or the blast is finished).
bool CVoxelExplTracker::MakeSingleStep()
{
	if ( bIsFinished )
		return false;
	if ( !IsValid( pExpl ) )
		pExpl = new CVoxelExpl( pWorld, ptCenter, pIgnitionObject, nWave, pGrenade, pThrower,
			this, pSpace, pEngGrenade, nEngSkill );
	if ( IsValid( pExpl ) && !pExpl->IsFinished() )
	{
		pExpl->MakeSingleIteration();
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CVoxelExplTracker::MakeDamage @0x3566d0: apply the settled ring's damage, accumulate its
// tallies, advance nWave; the LAST ring finishes the blast (scorch decal + grass + ack + AI event)
// and the ring is dropped either way (pExpl = 0).
void CVoxelExplTracker::MakeDamage()
{
	if ( IsValid( pExpl ) )
	{
		// retail inlines CVoxelExpl::MakeDamage @0x356580 (bFinished + either-record gate)
		pExpl->MakeDamage();
		nObjectsDestroyed += pExpl->nObjectsDestroyed;
		nEnemyUnitsKilled += pExpl->nEnemyUnitsKilled;
		++nWave;
		// total ring count per record: regular = nWaveNumber, engineer = (nEngSkill - nSkillReq) /
		// nDeltaWave + nStartNWave; neither record -> 0 (finish immediately).
		const int nWavesTotal = IsValid( pGrenade ) ? pGrenade->nWaveNumber
			: ( IsValid( pEngGrenade ) ? GetEngGrenadeWaves( pEngGrenade, nEngSkill ) : 0 );
		bIsFinished = ( nWavesTotal <= nWave );
		if ( bIsFinished )
		{
			vector<CObjectBase*> targets;
			for ( CDecalsHash::iterator i = drawDecals.begin(); i != drawDecals.end(); ++i )
				targets.push_back( i->first );
			// retail @0x3566d0: the scorch-decal base radius comes off whichever record is live
			// (regular fDecalRadius @+0x70, engineer fDecalRadius @+0x74); neither record -> radius 0.
			float fDecalRadius = 0;
			if ( IsValid( pGrenade ) )
				fDecalRadius = pGrenade->fDecalRadius;
			else if ( IsValid( pEngGrenade ) )
				fDecalRadius = pEngGrenade->fDecalRadius;
			// retail rolls the [0.8,1.2) factor ONCE and the SAME randomized radius feeds the decal, the
			// terrain grass scorch AND the grass-event square (disasm @0x756836: single ISAAC roll in ebx).
			const float fRadius = fDecalRadius * random.GetFloat( 0.8f, 1.2f );
			if ( !targets.empty() && IsValid(pWorld) )
				new CDecal( pWorld, ptCenter + CVec3(0,0,0.3f), fRadius, NDb::GetMaterial( 3264 ), targets );
			if ( IsValid( pWorld ) )
			{
				// retail @0x75690c: darken/clear the grass around the blast (base texture scorch is the CDecal)
				if ( pWorld->GetTerrain() )
					pWorld->GetTerrain()->DrawExplosion( ptCenter, fRadius );
				// retail @0x756930: a (2n+1)^2 square of disturbed-grass events around the centre,
				// n = fistp(fRadius); z = the blast-centre z (not terrain-sampled).
				const int n = Float2Int( fRadius );
				for ( int y = -n; y <= n; ++y )
					for ( int x = -n; x <= n; ++x )
						pWorld->AttachMiscObject( CreateDGrassEvent( ptCenter + CVec3( (float)x, (float)y, 0 ) ) );
			}
			if ( pThrower )
			{
				pWorld->GetGlobalAck()->OnGrenadeExplosion( pThrower,
					nEnemyUnitsKilled, nObjectsDestroyed );
				// retail @0x3566d0: alert AI units within ~30m of the blast to the thrower (an enemy
				// thrower -> possibleEnemy; a friendly one -> ally-needs-help). OnGrenade measures thrower->unit.
				NGlobal::ThrowEvent( NWorld::CEventOnGrenadeExplosion( pThrower, ptCenter ) );
			}
		}
	}
	pExpl = 0;   // retail tail: the ring is dropped unconditionally
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
// Every splinter is capped at fFragmentRange*FP_GRID_STEP like retail (SAttackRayInfo.fMaxRange;
// TraceLooseRaySegment @0x292010 gates fEnter/fExit on it) via PerformRangedAttack's fMaxRange.
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
			pWorld->PerformRangedAttack( att, ray, ignores, pWorld->GetTime()->GetValue(), 0, 0, fRange );
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
		pWorld->PerformRangedAttack( att, ray, ignores, pWorld->GetTime()->GetValue(), 0, 0, fRange );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ============================== retail wExplTracker voxel subsystem ==============================
// (W4 serialization-convergence -- see the header banner in wExplTracker.h)
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// SFlushCacheList
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x359460: push_back holds exactly one reference on the cache (retail builds a temporary
// CPtr, copies it into the fresh node, then destroys the temporary -- net +1, same as push_back).
void SFlushCacheList::RegisterCache( IFlushCache *pCache )
{
	caches.push_back( CPtr<IFlushCache>( pCache ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x358a90: one pass over the registry. DEAD entries (null, or the CObjectBase dead flag --
// exactly what !IsValid tests) are erased (the node's CPtr releases its ref); LIVE ones get their
// DoFlush virtual (retail IFlushCache vtbl+0x10). The iterator is advanced/captured before acting,
// like retail captures node->next, so a re-entrant DoFlush cannot derail the walk.
void SFlushCacheList::Flush()
{
	for ( list< CPtr<IFlushCache> >::iterator it = caches.begin(); it != caches.end(); )
	{
		if ( !IsValid( *it ) )
			it = caches.erase( it );
		else
		{
			(*it)->DoFlush();
			++it;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// C3DLookupTable
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x35a0d0 (space-bound ctor): keep the space alive, register in its deferred-flush list
// (so the space's next SyncWithAIMap calls this->DoFlush), then allocate the 64x64x8 grid --
// 0x8000 null CObj<CIndexCube> slots (nXYSize = 0x1000).
C3DLookupTable::C3DLookupTable( CExplosionSpace *_pSpace ):
	nGCx( 0x7fffffff ), nGCy( 0 ), nGCz( 0 ), pSpace( _pSpace )
{
	if ( _pSpace )
		_pSpace->cacheList.RegisterCache( this );
	grid.SetSizes( 0x40, 0x40, 8 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3583d0: clear *pRes unconditionally, then walk the grid z (outer) -> y -> x with linear
// index nXYSize*z + nXSize*y + x and append {x,y,z,cube} for every non-null cell.
void C3DLookupTable::GetAllCubes( vector<SIndexCubeInfo> *pRes )
{
	pRes->clear();
	for ( int z = 0; z < grid.GetZSize(); ++z )
		for ( int y = 0; y < grid.GetYSize(); ++y )
			for ( int x = 0; x < grid.GetXSize(); ++x )
			{
				CIndexCube *pCube = grid[z][y][x];
				if ( pCube )
				{
					SIndexCubeInfo info;
					info.x = x;
					info.y = y;
					info.z = z;
					info.p = pCube;
					pRes->push_back( info );
				}
			}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail C3DLookupTable::FlushCache @0x3587b0 (the IFlushCache::DoFlush override): invalidate the
// one-cube resolve cache -- re-arm the nGCx sentinel and release both cached cubes. The grid itself
// stays intact; only the cache drops.
void C3DLookupTable::DoFlush()
{
	nGCx = 0x7fffffff;
	nGCy = 0;
	nGCz = 0;
	pExplCube = 0;
	pIndexCube = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail C3DLookupTable::GetObject @0x358d60 (W5): resolve one GLOBAL voxel. Cube grid coords =
// voxel>>4; the one-cube resolve cache (nGCx/nGCy/nGCz) short-circuits repeated lookups in the same
// cube; a miss FetchCube-s the (rasterised) explosion cube and lazily creates the 16^3 CIndexCube
// in the grid slot. Local index = ((z&15)*16 + (y&15))*16 + (x&15) -- matching the X<->Z-swapped
// rvoxels layout CExplosionCube::Recalc writes.
void C3DLookupTable::GetObject( int nX, int nY, int nZ, unsigned short *pObj, unsigned short **ppCell )
{
	const int nGX = nX >> 4, nGY = nY >> 4, nGZ = nZ >> 4;
	if ( nGCx != nGX || nGCy != nGY || nGCz != nGZ )
	{
		pExplCube = pSpace->FetchCube( nGX, nGY, nGZ );
		CIndexCube *pIC = grid[nGZ][nGY][nGX];
		if ( !pIC )
		{
			// lazy 16^3 index cube, zero-filled (retail: new CIndexCube(0x10, 0) into the slot)
			pIC = new CIndexCube( 0x10, 0 );
			grid[nGZ][nGY][nGX] = pIC;
		}
		pIndexCube = pIC;
		nGCx = nGX;
		nGCy = nGY;
		nGCz = nGZ;
	}
	const int nIdx = ( ( nZ & 15 ) * 16 + ( nY & 15 ) ) * 16 + ( nX & 15 );
	*pObj = pExplCube->rvoxels[ nIdx ].nObject;
	*ppCell = &pIndexCube->data[ nIdx ];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExplosionCube (the retail AI-grid cube -- NOT the dev per-blast CExplCube above)
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x358240: store centre/deltas/space/aimap, then register this cube as a region tracker on
// the AI map: bound = { centre = vCenter, radius = sqrt(3)*F_CUBE_SIZE (the cube half-diagonal),
// half box = (F_CUBE_SIZE, F_CUBE_SIZE, F_CUBE_SIZE) }, mask TS_FRAGMENTED (0x10),
// bInformOnDoorFlip = true (disasm: pAIMap vtbl+0x40(this, &bound, 0x10, 1)).
CExplosionCube::CExplosionCube( CExplosionSpace *_pSpace, NAI::IAIMap *_pAIMap, const CVec3 &_vCenter,
	int _nDeltaX, int _nDeltaY, int _nDeltaZ ):
	vCenter( _vCenter ), pAIMap( _pAIMap ),
	nDeltaX( _nDeltaX ), nDeltaY( _nDeltaY ), nDeltaZ( _nDeltaZ ), pSpace( _pSpace ),
	bCalced( false ), bNeedRecalc( false )
{
	SBound bound;
	bound.s.ptCenter = vCenter;
	bound.s.fRadius = sqrt( F_CUBE_SIZE * F_CUBE_SIZE * 3.0f );
	bound.ptHalfBox = CVec3( F_CUBE_SIZE, F_CUBE_SIZE, F_CUBE_SIZE );
	pAIMap->AddTracker( this, bound, TS_FRAGMENTED, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x355080: rebuild rvoxels from the AI map when not yet calced.
//   1. a TRANSIENT 16^3 CExplVoxelRenderer over vCenter, seeded with the SPACE's object registry
//      (decomp: Init(&renderer, &vCenter, F_CUBE_SIZE, 0x10, &pSpace->objects, &pSpace->nObjectsEnd));
//   2. the AI map rasterises its solid hulls into it (vtbl+0x20 == TraceVoxelGrid, mask 0x10);
//   3. copy the renderer grid into the flat rvoxels with the fixed axis RE-INDEX (renderer index
//      256*j + 16*y + z -> rvoxels index 256*z + 16*y + j, i.e. the renderer's X<->Z axes swap),
//      forcing any voxel on a WORLD grid face to terrain (=1): global X = nDeltaX+j in {0,0x3ff},
//      global Y = nDeltaY+y in {0,0x3ff}, global Z = nDeltaZ+z in {0,0x7f};
//   4. bump the per-segment work budget, latch bCalced.
void CExplosionCube::Recalc()
{
	if ( bCalced )
		return;
	const int N = N_CUBE_SIZE;   // 0x10
	NAI::CExplVoxelRenderer renderer;
	renderer.Init( vCenter, F_CUBE_SIZE, N, &pSpace->objects, &pSpace->nObjectsEnd );
	pAIMap->TraceVoxelGrid( &renderer, TS_FRAGMENTED );
	//
	rvoxels.resize( N * N * N );   // 0x1000
	for ( int z = 0; z < N; ++z )
	{
		const int nGZ = nDeltaZ + z;
		for ( int y = 0; y < N; ++y )
		{
			const int nGY = nDeltaY + y;
			for ( int j = 0; j < N; ++j )   // j walks the renderer's Z axis == the world X axis
			{
				const int nGX = nDeltaX + j;
				unsigned short nVal = renderer.voxels[j][y][z].nObject;
				if ( nGX == 0 || nGX == 0x3ff || nGY == 0 || nGY == 0x3ff || nGZ == 0 || nGZ == 0x7f )
					nVal = 1;   // world-boundary voxels are forced to terrain
				rvoxels[ N * N * z + N * y + j ].nObject = nVal;
			}
		}
	}
	//
	++nBreakCalcs;   // retail tail: nBreakCalcs = nBreakCalcs + 1
	bCalced = true;
	bNeedRecalc = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExplosionSpace
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x35a960: keep the AI map, allocate the 64x64x8 cube grid (0x8000 null CObj slots,
// nXYSize = 0x1000), empty registries, nObjectsEnd = 0.
CExplosionSpace::CExplosionSpace( NAI::IAIMap *_pAIMap ):
	pAIMap( _pAIMap ), nObjectsEnd( 0 )
{
	grid.SetSizes( 0x40, 0x40, 8 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x358c60: one reconciliation of the voxel grid with the AI map -- drain the deferred-flush
// registry (which drops every registered lookup table's resolve cache), then re-arm every grid cube
// the AI map changed: bNeedRecalc set (+0x39) => clear bCalced (+0x38) so the next resolve
// re-rasterises it. NB retail clears ONLY bCalced -- bNeedRecalc stays set until the Recalc.
void CExplosionSpace::SyncWithAIMap()
{
	cacheList.Flush();
	for ( int z = 0; z < grid.GetZSize(); ++z )
		for ( int y = 0; y < grid.GetYSize(); ++y )
			for ( int x = 0; x < grid.GetXSize(); ++x )
			{
				CExplosionCube *pCube = grid[z][y][x];
				if ( pCube && pCube->bNeedRecalc )
					pCube->bCalced = false;
			}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CExplosionSpace::FetchCube @0x358ae0 (W5): bounds-checked cube resolve. A null grid slot
// lazily builds the cube (centre = (g+0.5)*F_CUBE_SIZE, voxel deltas = g<<4 -- it registers itself
// as an AI-map region tracker in its ctor), then Recalc (self-gated on bCalced) rasterises it.
CExplosionCube* CExplosionSpace::FetchCube( int nX, int nY, int nZ )
{
	if ( nX < 0 || nX >= grid.GetXSize() )
		return 0;
	if ( nY < 0 || nY >= grid.GetYSize() )
		return 0;
	if ( nZ < 0 || nZ >= grid.GetZSize() )
		return 0;
	CExplosionCube *pCube = grid[nZ][nY][nX];
	if ( !pCube )
	{
		const float fHalf = F_CUBE_SIZE * 0.5f;
		CVec3 vCentre( nX * F_CUBE_SIZE + fHalf, nY * F_CUBE_SIZE + fHalf, nZ * F_CUBE_SIZE + fHalf );
		pCube = new CExplosionCube( this, pAIMap, vCentre, nX << 4, nY << 4, nZ << 4 );
		grid[nZ][nY][nX] = pCube;
	}
	pCube->Recalc();   // retail: every resolve re-arms the cube (Recalc self-gates on bCalced)
	return pCube;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CExplosionSpace::GetCoord @0x357840 (W5): world position -> GLOBAL voxel coords.
// voxel = ROUND(coord / F_VOXEL_SIZE - 0.5), clamped to [1,0x3fe] for x/y and [1,0x7e] for z.
void CExplosionSpace::GetCoord( const CVec3 *pPos, SExplVoxelCoords *pRes )
{
	int nX = Float2Int( pPos->x / F_VOXEL_SIZE - 0.5f );
	if ( nX < 1 )
		nX = 1;
	else if ( nX > 0x3fe )
		nX = 0x3fe;
	int nY = Float2Int( pPos->y / F_VOXEL_SIZE - 0.5f );
	if ( nY < 1 )
		nY = 1;
	else if ( nY > 0x3fe )
		nY = 0x3fe;
	int nZ = Float2Int( pPos->z / F_VOXEL_SIZE - 0.5f );
	if ( nZ < 1 )
		nZ = 1;
	else if ( nZ > 0x7e )
		nZ = 0x7e;
	pRes->nX = (unsigned short)nX;
	pRes->nY = (unsigned short)nY;
	pRes->nZ = (unsigned short)nZ;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CExplosionSpace::GetCenter @0x357790 (W5): GLOBAL voxel coords -> world-space cell centre.
void CExplosionSpace::GetCenter( int nX, int nY, int nZ, CVec3 *pRes )
{
	pRes->x = ( nX + 0.5f ) * F_VOXEL_SIZE;
	pRes->y = ( nY + 0.5f ) * F_VOXEL_SIZE;
	pRes->z = ( nZ + 0.5f ) * F_VOXEL_SIZE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NWorld::CreateExplosionSpace @0x357730: the space factory (null on OOM).
CExplosionSpace* CreateExplosionSpace( NAI::IAIMap *pAIMap )
{
	return new CExplosionSpace( pAIMap );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExplosionMaster
////////////////////////////////////////////////////////////////////////////////////////////////////
// shared head of both AddExplosion overloads (retail @0x357520/@0x357620): an idle master goes
// S_ACTIVE and builds the shared space over the world's AI map
// (decomp: pIAIMap = pWorld->vtbl+0x20() == GetAIMap; CreateExplosionSpace @0x357730).
void CExplosionMaster::BeginIfIdle()
{
	if ( state == S_IDLE )
	{
		state = S_ACTIVE;
		pSpace = CreateExplosionSpace( pWorld->GetAIMap() );   // retail @0x357730
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x357520 (IExplosionMaster vtbl+0x14): enqueue an ordinary-grenade blast.
void CExplosionMaster::AddExplosion( const CVec3 &vCenter, CObjectBase *pIgnitionObject, NDb::CRPGGrenade *pGrenade,
	CUnitServer *pThrower, const SPerkMineModifiers &modifiers )
{
	BeginIfIdle();
	toBeStarted.push_back( SStartInfo( vCenter, pIgnitionObject, pGrenade, pThrower, modifiers ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x357620 (IExplosionMaster vtbl+0x10): enqueue an engineer-grenade blast.
void CExplosionMaster::AddExplosion( const CVec3 &vCenter, CObjectBase *pIgnitionObject, NDb::CRPGEngGrenade *pEngGrenade,
	CUnitServer *pThrower, const SPerkMineModifiers &modifiers, int nEngSkill )
{
	BeginIfIdle();
	toBeStarted.push_back( SStartInfo( vCenter, pIgnitionObject, pEngGrenade, pThrower, modifiers, nEngSkill ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3571d0 -- the per-segment driver (W5: the literal retail loop; the W4-interim
// tracker-side state machine is gone):
//   nBreakCalcs = 0;                          // reset the per-segment work budget
//   S_WAIT:   nLag-- and return while > 0; on expiry -> S_ACTIVE + pSpace->SyncWithAIMap();
//   S_ACTIVE: (1) step every in-flight blast to a stop (do{}while(MakeSingleStep @0x3565c0)),
//                 bailing for THIS segment once nBreakCalcs > 1;
//             (2) MakeDamage (@0x3566d0) every blast that has NOT finished;
//             (3) compact out every finished blast;
//             (4) drain toBeStarted into fresh CVoxelExplTracker blasts;
//             (5) settle: all done -> S_IDLE + drop the space; else S_WAIT with nLag = 2.
void CExplosionMaster::Segment()
{
	ResetBreakExplCalcs();   // retail top: nBreakCalcs = 0 (release ResetBreakExplCalcs @0x3549f0)
	//
	// ---- S_WAIT: inter-ring lag countdown; on expiry reconcile the grid with the AI map ----
	if ( state == S_WAIT )
	{
		const int nPrevLag = nLag--;
		if ( nPrevLag > 0 )
			return;
		state = S_ACTIVE;
		pSpace->SyncWithAIMap();   // retail: unguarded (S_WAIT implies a live space)
	}
	if ( state != S_ACTIVE )
		return;
	//
	// ---- (1) step every blast to a stop; per-segment budget bails between blasts ----
	for ( int i = 0; i < (int)explosions.size(); ++i )
	{
		while ( explosions[i]->MakeSingleStep() ) {}   // retail do{}while drain
		if ( nBreakCalcs > 1 )
			return;   // budget spent -> resume next segment (state stays S_ACTIVE)
	}
	//
	// ---- (2) damage pass: every NOT-finished blast lands its settled ring ----
	for ( int i = 0; i < (int)explosions.size(); ++i )
	{
		CVoxelExplTracker *pTracker = explosions[i];
		if ( !pTracker->bIsFinished )
			pTracker->MakeDamage();
	}
	//
	// ---- (3) compact out the finished blasts (retail: erase-then-advance -- the element moved
	// into the erased slot is not rechecked this pass; it goes next segment) ----
	for ( int i = 0; i < (int)explosions.size(); ++i )
	{
		if ( explosions[i]->bIsFinished )
			explosions.erase( explosions.begin() + i );   // the CObj releases the finished blast
	}
	//
	// ---- (4) drain the start queue: retail spawns CVoxelExplTracker(pSpace, vCenter,
	// pIgnitionObject, pGrenade, pThrower, modifiers, pWorld, pEngGrenade, nEngSkill) ----
	CDynamicCast<CWorld> pW( pWorld.GetPtr() );
	for ( list<SStartInfo>::iterator i = toBeStarted.begin(); i != toBeStarted.end(); ++i )
	{
		if ( pW )
			explosions.push_back( CObj<CVoxelExplTracker>( new CVoxelExplTracker( pSpace, i->vCenter,
				i->pIgnitionObject.GetPtr(), i->pGrenade, i->pThrower, i->modifiers, pW,
				i->pEngGrenade, i->nEngSkill ) ) );
	}
	toBeStarted.clear();
	//
	// ---- (5) settle the state machine ----
	if ( explosions.empty() )
	{
		state = S_IDLE;
		pSpace = 0;   // release the shared space
	}
	else
	{
		state = S_WAIT;
		nLag = 2;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x355b40: the master factory (new CExplosionMaster(pWorld); null on OOM).
IExplosionMaster* CreateExplosionMaster( IWorld *pWorld )
{
	return new CExplosionMaster( pWorld );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NWorld;
//
REGISTER_SAVELOAD_CLASS( 0x51682140, CVoxelExpl )
// (dev CExplCube 0x52382160 REMOVED -- W5 serialization-convergence: the id is ABSENT from retail;
// the wavefront now runs over the shared CExplosionSpace/C3DLookupTable voxel grid like retail)
REGISTER_SAVELOAD_CLASS( 0x52782130, CVoxelExplTracker )
// ---- retail wExplTracker voxel subsystem (W4 serialization-convergence; ids == retail) ----
REGISTER_SAVELOAD_CLASS( 0x02443140, CExplosionCube )
REGISTER_SAVELOAD_CLASS( 0x02443141, CExplosionSpace )
REGISTER_SAVELOAD_CLASS( 0x02443142, CIndexCube )
REGISTER_SAVELOAD_CLASS( 0x02443143, C3DLookupTable )
REGISTER_SAVELOAD_CLASS( 0x02443144, CExplosionMaster )