#include "StdAfx.h"
#include "DG.h"
#include "Time.h"
#include "..\DBFormat\DataSound.h"
#include "..\FModSound\FMSound.h"
#include "SoundEffect.h"
#include "SoundFormat.h"
#include "..\Misc\BasicShare.h"

namespace NSound
{
extern CBasicShare<int, CFileSample3D> share3DSamples;
////////////////////////////////////////////////////////////////////////////////////////////////////
CSoundInstance::CSoundInstance( NDb::CSoundInstance *_pInstance, STime t, CFuncBase<STime> *_pTime, CFuncBase<CVec3> *pPos, const vector<int> &_flags )
	: pInstance(_pInstance), stBeginTime(t), pTime(_pTime), pPlacement(pPos), flags(_flags) 
{
	value = false;
	stBeginTime += pInstance->nStartTime;
	tLastSound = stBeginTime - 1000 * pInstance->fSoundAvgInterval;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundInstance::Recalc()
{
	STime time = pTime->GetValue();
	if ( time < stBeginTime )
		return;
	if (  IsValid( pSound )  )
	{
		if ( pInstance->eSoundType != NDb::ST_RANDOM )
			value = !NFMSound::IsPlaying( pSound );
		return;
	}
	//
	if ( !IsValid( pInstance->pSound ) )
	{
		ASSERT(0);
		value = true;
		return;
	}
	//
	if ( pInstance->eSoundType == NDb::ST_RANDOM )
	{
		static SRand rnd;
		STime t = pTime->GetValue();
		float fInterval = float( t - tLastSound ) / 1000.0f;
		float fAvrgInterval = Max( 1.0f, pInstance->fSoundAvgInterval );
		if ( fInterval < rnd.GetFloat( 0.5f * fAvrgInterval, 20.0f * fAvrgInterval ) )
			return;
		//char buf[256];
		//sprintf( buf, "Random sound interval=%f, current=%f\n", pInstance->fSoundAvgInterval, fInterval );
		//OutputDebugString( buf );
	}
	//
	static SRand rand;
	NDb::CSoundVariant* pSVar = pInstance->pSound->GetSound( &rand, flags );
	if ( !pSVar || !pSVar->pSound )
	{
		ASSERT(0);
		value = true;
		return;
	}
	CDGPtr< CPtrFuncBase<NFMSound::CSample3D> > pSam = share3DSamples.Get( pSVar->pSound->GetRecordID() );
	pSam.Refresh();
	NFMSound::CSample3D *pData = pSam->GetValue();
	if ( !pData )
	{
	//	ASSERT(0);
	}
	else
	{
		NFMSound::SPlayParams p;
		p.pSample = pData;
		p.bLoop = pSVar->pSound->bLoop;
		p.nVolume = pInstance->nVolume;
		p.position = pPlacement->GetValue();
		p.nLoops = pInstance->nCycleCount;
		p.bFadeIn = pInstance->bFadeIn;
		p.bFadeOut = pInstance->bFadeOut;
		p.nFadeSamples = pInstance->nFadeSamples;
		pSound = NFMSound::Play3DSound( p );
		tLastSound = pTime->GetValue();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSoundEffect::CSoundEffect( NDb::CSoundEffect *pEff, STime stBeginTime, CFuncBase<STime> *pTime, CFuncBase<CVec3> *pPos, const vector<int> &flags )
{
	for ( int i = 0; i < pEff->instances.size(); ++i )
		if ( IsValid( pEff->instances[i] ) )
			instances.push_back( new CSoundInstance( pEff->instances[i], stBeginTime, pTime, pPos, flags ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSoundEffect::Update()
{
	for ( vector<CDGPtr<CFuncBase<bool> > >::iterator i = instances.begin(); i != instances.end(); )
	{
		if ( !IsValid( *i ) || ((*i).Refresh(), (*i)->GetValue()) )
			i = instances.erase( i );
		else
			++i;
	}
	return instances.empty();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NSound;
BASIC_REGISTER_CLASS(CSoundEffect)