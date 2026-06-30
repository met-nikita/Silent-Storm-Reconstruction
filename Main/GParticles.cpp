#include "StdAfx.h"
#include "GParticles.h"
#include "GParticleFormat.h"
#include "GGrass.h"
#include "GParticleInfo.h"
#include "..\DBFormat\DataTerrain.h"
#include "..\DBFormat\DataFormat.h"
#include "Grid.h"
#include "Interpolate.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CParticleAnimator
////////////////////////////////////////////////////////////////////////////////////////////////////
void CParticleAnimator::Recalc()
{
	if ( !IsValid( pValue ) )
		pValue = new CStandardParticleEffect;

	CDynamicCast<CStandardParticleEffect> pRealValue(pValue);
	CStandardParticleEffect &value = *pRealValue;

	value.bEnd = true;
	value.nGrassSize = 0;
	value.textures = textureIDs;

	value.pInfo = pInfo;
	value.fScale = pInstance->fScale;
	value.fEndCycle = pInstance->fEndCycle;
	value.pivot = pInstance->pivot;
//	value.bAlphaAdd = (pInstance->alpha == NDb::CParticleInstance::A_ADDITIVE);
	value.transform = pPlacement->GetValue().forward;
	value.frames.clear();
	
	STime time = pTime->GetValue();
	if ( time < stBeginTime )
	{
		value.bEnd = false;
		return;
	}
	float fTObject = (time - stBeginTime) * pInstance->fSpeed / 1000.f - pInstance->fOffset * pInstance->fSpeed;
	CParticlesInfo *pEffect = pInfo->GetValue();
	if ( !pEffect )
	{
		value.bEnd = false;
		return;
	}
	if ( fTObject < 0 )
	{
		value.bEnd = false;
		return;
	}
	vector<float> fTimes;
	if ( pInstance->fEndCycle == 0 )
	{
		if ( pInstance->nCycleCount )
		{
			if ( fTObject < pEffect->fTEnd )
			{
				fTimes.push_back( fTObject );
				value.bEnd = false;
			}
		}
		else
		{
			fTimes.push_back(0);
			value.bEnd = false;
		}
	}
	else
	{
		int nCurCycle = int( fTObject / pInstance->fEndCycle ) + 1;
		if ( pInstance->nCycleCount && nCurCycle > pInstance->nCycleCount )
			nCurCycle = pInstance->nCycleCount;
		float fT = fTObject - (nCurCycle - 1) * pInstance->fEndCycle;
		for ( int i = 0; i < nCurCycle && fT < pEffect->fTEnd; ++i )
		{
			fTimes.push_back( fT );
			fT += pInstance->fEndCycle;
		}
		if ( pInstance->nCycleCount == 0
			|| fTObject < pEffect->fTEnd + (pInstance->nCycleCount - 1) * pInstance->fEndCycle )
			value.bEnd = false;
	}

	for ( int i = 0; i < fTimes.size(); ++i )
	{
		SParticleFrame frame;
		frame.fT = fTimes[i] * pEffect->fFrameRate;
		frame.bLastCycle = (i == 0);
		value.frames.push_back( frame );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGrassAnimator
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrassAnimator::Recalc()
{
	if ( !IsValid( pValue ) )
	{
		CGrassParticleEffect *pRealValue = new CGrassParticleEffect;
		const SGrassLayer &gl = pGrassTracker->GetGrassLayer( nLayer );
		pValue = pRealValue;
		pRealValue->positions = pGrassPos->GetValue()->positions;
		pRealValue->colors.resize( pRealValue->positions.size() );
		pRealValue->waveAmps.resize( pRealValue->positions.size() );
		pRealValue->scale = pDBGrass->ptScale;
		pRealValue->fXPivot = pDBGrass->ptPivot.x;
		pRealValue->fYPivot = pDBGrass->ptPivot.y;
		pRealValue->fScaleRange = pDBGrass->fScaleRange;
		for ( int i = 0; i < pRealValue->positions.size(); ++i )
		{
			const CVec3 &pos = pRealValue->positions[i];
			float fColorX = pos.x * FP_INV_GRID_STEP * FP_GRASS_COLOR_SCALE;
			float fColorY = pos.y * FP_INV_GRID_STEP * FP_GRASS_COLOR_SCALE;
			pRealValue->colors[i].color = GetBilinear( gl.grassColor, fColorX, fColorY, NGfx::CInterpolateColor() );
		}
		pRealValue->bEnd = false;
		pRealValue->nGrassSize = pDBGrass->nSideSize;
		pRealValue->textures.resize( 1 );
		pRealValue->textures[0] = pGrassTexture;
	}
	CDynamicCast<CGrassParticleEffect> pRealValue(pValue);
	STime time = pTime->GetValue();
	pRealValue->fTEffect = time / 1000.f;
	for ( int i = 0; i < pRealValue->positions.size(); ++i )
	{
		const CVec3 &pos = pRealValue->positions[i];
		pRealValue->waveAmps[i] = pGrassTracker->GetWaveAmp( pos );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExplosionAnimator
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplosionAnimator::Recalc()
{
	if ( !IsValid( pValue ) )
		pValue = new CExplosionParticleEffect;

	CDynamicCast<CExplosionParticleEffect> pRealValue(pValue);
	CExplosionParticleEffect &value = *pRealValue;

	value.bEnd = false;
	value.nGrassSize = 0;
	value.textures = textureIDs;

	value.pInfo = pInfo;
	value.pivot = pInstance->pivot;
	value.positions.clear();
	value.fTimes.clear();
//	value.bAlphaAdd = (pInstance->alpha == NDb::CParticleInstance::A_ADDITIVE);

	CParticlesInfo *pEffect = pInfo->GetValue();
	if ( !pEffect )
		return;
	const NGScene::SParticle &part = pEffect->particles[0];
/*
	const CExplosionInfo &exp = pExplosion->GetValue();
	STime tLastBornBefore = tLastBorn;
	for ( int i = 0; i < exp.particles.size(); ++i )
	{
		const SParticleBorn &p = exp.particles[i];
		if ( p.tBorn <= tLastBornBefore )
			continue;
		particles.push_back( p );
		tLastBorn = p.tBorn;
	}

	STime time = pTime->GetValue();
	value.positions.clear();
	value.fTimes.clear();
	for ( list<SParticleBorn>::iterator i = particles.begin(); i != particles.end(); )
	{
		if ( time < i->tBorn )
		{
			++i;
			continue;
		}
		float fTime = (time - i->tBorn) * fSpeed * 0.5f * pEffect->fFrameRate / 1000.f;
		if ( fTime < part.nTStart )
		{
			++i;
			continue;
		}
		if ( fTime > part.nTEnd )
			i = particles.erase(i);
		else
		{
			value.positions.push_back( i->pos );
			value.fTimes.push_back( fTime );
			++i;
		}
	}
*/
	
	const CExplosionInfo &exp = pExplosion->GetValue();
	particles.clear();
	for ( int i = 0; i < exp.particles.size(); ++i )
	{
		const SParticleBorn &p = exp.particles[i];
		particles.push_back( p );
	}

	STime time = pTime->GetValue();
	for ( list<SParticleBorn>::iterator i = particles.begin(); i != particles.end(); )
	{
		if ( time < i->tBorn )
		{
			++i;
			continue;
		}
		float fTime = (time - i->tBorn) * pInstance->fSpeed * pEffect->fFrameRate / 1000.f;
		if ( fTime < part.nTStart )
		{
			++i;
			continue;
		}
		if ( fTime > part.nTEnd )
			i = particles.erase(i);
		else
		{
			value.positions.push_back( i->pos );
			value.fTimes.push_back( fTime );
			++i;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRainAnimator
////////////////////////////////////////////////////////////////////////////////////////////////////
// per-cell phase-noise table (file-static in the rain module; values verbatim from the binary @0x978c78)
static int nRandomTable[32] =
{
	17, 43, 456, 942, 32, 234, 865, 95, 321, 47, 909, 284, 543, 396, 193, 120,
	98, 784, 633, 10, 259, 77, 118, 66, 921, 849, 356, 80, 475, 235, 213, 826
};
void CRainAnimator::Recalc()
{
	if ( !IsValid( pValue ) )
	{
		pValue = new CRainParticleEffect;
		textureIDs.clear();
	}
	CDynamicCast<CRainParticleEffect> pRealValue( pValue );
	CRainParticleEffect &value = *pRealValue;

	unsigned long nTime = pTime->GetValue();
	if ( tStart == 0 )
		tStart = nTime;

	float fTimeFrac = (float)nTime * 0.09765625f;
	int nCycle = (int)( (nTime - tStart) >> 10 );

	float fRamp;
	if ( nCycle * FP_PI <= FP_PI )
		fRamp = 0.5f - cos( nCycle * FP_PI ) * 0.5f;
	else
		fRamp = 1.0f;
	int nThreshold = Float2Int( fRamp * 64.0f );

	const CVec3 &cam = pCamera->GetValue();
	float fD = ( cam.z - 40.0f ) * -2.0f;
	int nOriginX = Float2Int( ( cam.x - fD * 0.1f ) * 2.0f );
	int nOriginY = Float2Int( ( cam.y - fD * 0.2f ) * 2.0f );

	const int nCap = 0x19a1;
	vector<char>  faces( nCap );
	vector<CVec3> positions( nCap );
	vector<CVec3> directions( nCap );
	int nEmit = 0;

	for ( int gy = -40; gy < 41; ++gy )
	{
		int nYIdx = nOriginY + gy;
		int nXIdx = nOriginX - 40;
		for ( int col = 0; col < 81; ++col, ++nXIdx )
		{
			int nH = nRandomTable[ ( nXIdx * nXIdx + nYIdx ) & 0x1f ];
			int nPhase = ( nH + nYIdx * 13 + nXIdx * 172 ) & 0xffff;
			float v = (float)nPhase + fTimeFrac;
			unsigned int nCyc = (unsigned int)Float2Int( v * 0.005f );
			if ( (int)( nCyc & 0x3f ) >= nThreshold )
				continue;

			float t = v - (float)(int)nCyc * 200.0f;
			float z0 = 40.0f - t * 0.5f;
			int nDepth = Float2Int( ( 1.0f / 60.0f ) * ( cam.z - z0 ) );

			int nA = (int)( ( nCyc * (unsigned)-0x3b ) & 0xff ) - 0x80;	// -59 * cyc
			int nB = (int)( ( nCyc * 0x43u )           & 0xff ) - 0x80;	//  67 * cyc
			int nC = (int)( ( nCyc * (unsigned)-0x7f ) & 0xff ) - 0x80;	// -127 * cyc

			CVec3 &pos = positions[nEmit];
			pos.x = (float)nA * 0.1f * 0.0078125f + t * 0.1f + (float)nXIdx * 0.1f;
			pos.y = (float)nB * 0.1f * 0.0078125f + t * 0.2f + (float)nYIdx * 0.1f;
			pos.z = (float)nC * 0.078125f + (float)nDepth * 60.0f + z0;

			faces[nEmit] = (char)( nCyc & 3 );

			CVec3 &dir = directions[nEmit];
			int rX = nRandomTable[ ( nXIdx * nXIdx ) & 0x1f ] & 0x1f;
			int rY = nRandomTable[ ( ( nXIdx + nYIdx ) * nYIdx ) & 0x1f ] & 0x1f;
			dir.x = (float)rX * ( 1.0f / 31.0f ) * 0.2f - 0.1f - 0.2f;
			dir.y = (float)rY * ( 1.0f / 31.0f ) * 0.2f - 0.1f - 0.4f;
			dir.z = 1.0f;

			++nEmit;
		}
	}

	faces.resize( nEmit );
	positions.resize( nEmit );
	directions.resize( nEmit );

	if ( !IsValid( pFilter ) )
	{
		value.faces = faces;
		value.positions = positions;
		value.directions = directions;
	}
	else
	{
		vector<unsigned char> flags;
		pFilter->Filter( &positions, &flags );
		value.faces = faces;
		value.positions = positions;
		value.directions = directions;
		int nKept = 0;
		for ( int i = 0; i < flags.size(); ++i )
		{
			if ( flags[i] == 0 )
			{
				value.faces[nKept] = faces[i];
				value.positions[nKept] = positions[i];
				value.directions[nKept] = directions[i];
				++nKept;
			}
		}
		value.faces.resize( nKept );
		value.positions.resize( nKept );
		value.directions.resize( nKept );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x27041142, CParticleAnimator )
REGISTER_SAVELOAD_CLASS( 0x125A1140, CGrassAnimator )
REGISTER_SAVELOAD_CLASS( 0x11932170, CExplosionAnimator )
