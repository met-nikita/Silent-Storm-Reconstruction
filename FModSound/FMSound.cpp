#include "StdAfx.h"
#include <fmod.h>
#include <fmod_errors.h>
#include <mmsystem.h>                // WAVEFORMATEX et al. -- StdAfx's windows.h is WIN32_LEAN_AND_MEAN, so dsound.h needs this first
#undef PlaySound                     // mmsystem.h #defines PlaySound -> would mangle NFMSound::PlaySound (declared in FMSound.h below)
#include <dsound.h>                  // NFMSound::GetSpeakerType: DirectSoundCreate + IDirectSound::GetSpeakerConfig
#pragma comment( lib, "dsound.lib" )  // self-contained -- no vcxproj edit (DirectSound device-config query)
#include "FMSound.h"
#include "..\Misc\HPTimer.h"

namespace NFMSound 
{
static bool bIsFMODInitialized = false;
static int nMusicVolume = 255;
static CVec3 vPrevCameraVelocity(1e38f,0,0);
static SHMatrix toCamera;
////////////////////////////////////////////////////////////////////////////////////////////////////
const float fXYScale = 2;
const float F_DISTANCE_SCALE = 0.1f;
static CVec3 ConvertPosToFMode( const CVec3 &_v )
{
	CVec4 v;
	toCamera.RotateHVector( &v, _v );
	CVec3 ptPos( v.x, v.y, v.w );
	ptPos.x *= fXYScale;
	ptPos.y *= fXYScale;
	return ptPos * F_DISTANCE_SCALE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFSoundSample
{
	FSOUND_SAMPLE *hSample;
	SFSoundSample( FSOUND_SAMPLE *_hSample = 0 ): hSample(_hSample) {}
	~SFSoundSample()
	{
		if ( hSample != 0 )
			FSOUND_Sample_Free( hSample );
	}
};
#define NOCOPIES( c ) c(const c&) {ASSERT(0); } c& operator=(const c&) {ASSERT(0); return *this;}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ��� ������ �� ����� ������� ������������ � ���������� � FMod
class CSample2D: public CObjectBase
{
	OBJECT_BASIC_METHODS(CSample2D);
	NOCOPIES(CSample2D);
	SFSoundSample sample;
public:
	CSample2D( FSOUND_SAMPLE *_hSample = 0 ): sample( _hSample ) {}
	operator FSOUND_SAMPLE*() const { return sample.hSample; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// ��� ������ �� ����� ������� ������������ � ���������� � FMod
class CSample3D: public CObjectBase
{
	OBJECT_BASIC_METHODS(CSample3D);
	NOCOPIES(CSample3D);
	SFSoundSample sample;
public:
	CSample3D( FSOUND_SAMPLE *_hSample = 0 ): sample( _hSample ) {}
	void SetMinMaxDistance( float fMin, float fMax )
	{
		FSOUND_Sample_SetMinMaxDistance( sample.hSample, fMin * F_DISTANCE_SCALE, fMax * F_DISTANCE_SCALE );
	}
	operator FSOUND_SAMPLE*() const { return sample.hSample; }
	bool IsEmpty() const { return sample.hSample == 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CChannel: public CObjectBase
{
public:
	virtual ~CChannel()
	{
		FSOUND_StopSound( nChannel );
	}
	int nChannel;
	virtual void Update( double dInterval ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSound2D: public CChannel
{
	OBJECT_BASIC_METHODS(CSound2D);
	CObj<CSample2D> pSample;
public:
	CSound2D( CSample2D *_pSample = 0 ): pSample(_pSample) {}
	//void SetPan();
	virtual void Update( double dInterval ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSound3D : public CChannel, public ISound3D
{
	OBJECT_BASIC_METHODS(CSound3D);
	CVec3 position;
	CVec3 vLastFPos, vLastVelocity; // last set 3d sound parameters
	CObj<CSample3D> pSample;
	int nVolume;
	int nLoops;
	int nCurrentLoop;
	int nLastPos;
	bool bFadeIn;
	bool bFadeOut;
	bool bFadeOut_InProgress;
	float fFadeVolume;
	float fFadeSpeed;
	int nFadeSamples;
	int nTotalLength;
public:
	CSound3D() {}
	CSound3D( CSample3D *_pSample, const CVec3 &pos, const CVec3 &_vLastFPos, int _nVolume = 255, int _nLoops = -1 )
		: pSample(_pSample), nCurrentLoop(0), nLoops(_nLoops), nVolume(_nVolume), bFadeIn(false), 
		bFadeOut(false), bFadeOut_InProgress(false), position(pos), vLastFPos(_vLastFPos), vLastVelocity(0,0,0)
	{ 
		fFadeVolume = nVolume;
		nLastPos = FSOUND_GetCurrentPosition( nChannel );
		nTotalLength = FSOUND_Sample_GetLength( *pSample );
		nFadeSamples = 0;
	}
	virtual void SetPosition( const CVec3 &pos ) { position = pos; }
	virtual void Update( double dInterval );
	CVec3 GetPosition() const { return position; }
	void SetFadeOut( bool _bFadeIn, bool _bFadeOut, int nSamples )
	{
		fFadeSpeed = (float)nVolume / nSamples;
		bFadeIn = _bFadeIn;
		if ( bFadeIn )
		{
			FSOUND_SetVolume( nChannel, 0 );
			fFadeVolume = 0;
		}
		//
		if ( !_bFadeOut || (nLoops == -1 && FSOUND_GetLoopMode( nChannel ) == FSOUND_LOOP_NORMAL ) )
			return;
		bFadeOut = true;
		nFadeSamples = nSamples;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSound3D
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CVec3 GetVelocity( const CVec3 &ptNew, const CVec3 &ptLast, double dTimeInterval )
{
	return (ptNew - ptLast) / dTimeInterval;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSound3D::Update( double dInterval )
{
	int nPos = FSOUND_GetCurrentPosition( nChannel );
	if ( nPos < nLastPos )
	{
		++nCurrentLoop;
		if ( nLoops > 1 && nCurrentLoop == nLoops )
		{
			FSOUND_StopSound( nChannel );
			return;
		}
	}
	nLastPos = nPos;

	CVec3 vFPosition = ConvertPosToFMode( position );
	CVec3 vel = GetVelocity( vFPosition, vLastFPos, dInterval );
	if ( position != vLastFPos || vLastVelocity != vel )
		FSOUND_3D_SetAttributes( nChannel, &vFPosition[0], &vel[0] );
	vLastFPos = vFPosition;
	vLastVelocity = vel;
	//
	if ( bFadeIn )
	{
		fFadeVolume = (nCurrentLoop * nTotalLength + nPos) * fFadeSpeed;
		if ( fFadeVolume >= nVolume )
		{
			fFadeVolume = nVolume;
			bFadeIn = false;
		}
		FSOUND_SetVolume( nChannel, fFadeVolume );
	}
	if ( bFadeOut )
	{
		if ( bFadeOut_InProgress )
		{
			if ( nLoops > 0 )
				fFadeVolume = Min( fFadeVolume, fFadeSpeed * (nLoops * nTotalLength - (nCurrentLoop * nTotalLength + nPos)) );
			else
				fFadeVolume = Min( fFadeVolume, fFadeSpeed * (nTotalLength - nPos) );
			if ( fFadeVolume < 0 )
			{
				fFadeVolume = 0;
				bFadeOut_InProgress = false;
				FSOUND_StopSound( nChannel );
			}
			FSOUND_SetVolume( nChannel, fFadeVolume );
		}
		else
		{
			if ( nLoops > 0 )
			{
				int nRemainSamples = nLoops * nTotalLength - (nCurrentLoop * nTotalLength + nPos);
				if ( nRemainSamples <= nFadeSamples )
					bFadeOut_InProgress = true;
			}
			else if ( FSOUND_GetLoopMode( nChannel ) == FSOUND_LOOP_OFF && nPos >= nTotalLength - nFadeSamples )
				bFadeOut_InProgress = true;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
signed char __stdcall SynchCallback( FSOUND_STREAM *stream, void *buff, int len, int param );
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStream: public CObjectBase
{
	OBJECT_NOCOPY_METHODS( CStream );
	string szFileName;
	float fFadeVolume;
	float fFadeSpeed;
	bool bFadeOut;
	bool bFadeIn;       // retail CStream+0x30: volume ramping up (SetSwitchStream / PlayStream fade-in)
	float fFadeInTime;  // retail CStream+0x34: fade-in length sec (retail Music data has FadeIn=0, so normally inert)
	bool bLoop;
	bool bSwitch;
	CPtr<CStream> pSwitch;
	bool bClose;
public:
	FSOUND_STREAM *pStream;
	int nChannel;
	bool bReset;

	CStream( string szFile = "", bool _bLoop = false ): pStream(0), bFadeOut(false), bFadeIn(false), fFadeInTime(0), fFadeVolume(0), fFadeSpeed(0), bReset(false), bLoop(_bLoop), szFileName(szFile), bSwitch(false), bClose(false), nChannel(-1) {}
	~CStream() { if ( pStream ) FSOUND_Stream_Close( pStream ); }

	string GetFileName() const { return szFileName; }
	bool IsLooped() const { return bLoop; }
	// retail CStream::PlayStream @0x3dce00: (fileName, bLoop, startMs, fadeInTime)
	void PlayStream( const char *pszFileName, bool _bLoop, int nStartMs = 0, float _fFadeInTime = 0 )
	{
		szFileName = pszFileName;
		fFadeInTime = _fFadeInTime;
		bLoop = _bLoop;
		int nFlags = FSOUND_2D;
		nFlags = bLoop ? nFlags | FSOUND_LOOP_NORMAL : nFlags;
		pStream = FSOUND_Stream_Open( pszFileName, nFlags, 0, 0 );
		ASSERT( pStream );
		if ( pStream )
		{
			// retail: a start offset past the clip end aborts playback entirely (the unplayed
			// stream is erased by the next NFMSound::Update)
			if ( FSOUND_Stream_GetLengthMs( pStream ) < nStartMs )
				return;
			FSOUND_Stream_SetSyncCallback( pStream, &SynchCallback, (int)this );
			nChannel = FSOUND_Stream_Play( 0, pStream );
			ASSERT( nChannel != -1 );
			if ( nStartMs > 0 )
				FSOUND_Stream_SetTime( pStream, nStartMs );
			FSOUND_SetPan( nChannel, FSOUND_STEREOPAN );
		}
		// retail volume/fade priming: an explicit fade-in starts silent and ramps in Update; a
		// pending SetSwitchStream fade-in (bFadeIn already set) also starts at its ramp volume
		int nVolume;
		if ( _fFadeInTime <= 0 )
			nVolume = bFadeIn ? (int)fFadeVolume : nMusicVolume;
		else
		{
			bFadeIn = true;
			fFadeVolume = 0;
			fFadeSpeed = (float)nMusicVolume / _fFadeInTime;
			nVolume = 0;
		}
		SetVolume( nVolume );
		if ( FP_EPSILON < fFadeInTime )
			fFadeSpeed = (float)nMusicVolume / fFadeInTime;   // retail's redundant recompute (tracks volume changes)
	}
	void FadeOut( float dSec )
	{
		if ( bFadeOut )
			return;
		bFadeOut = true;
		fFadeVolume = FSOUND_GetVolume( nChannel );
		fFadeSpeed = fFadeVolume / dSec;
	}
	void CancelFadeOut()
	{
		bFadeOut = false;
		SetVolume( nMusicVolume );
		if ( FP_EPSILON < fFadeInTime )
			fFadeSpeed = (float)nMusicVolume / fFadeInTime;   // retail @0x3db720
	}
	// retail CStream::Update @0x3dcae0: fade-out ramp (stopping at silence), else fade-in ramp
	void Update( double dInterval )
	{
		if ( bFadeOut )
		{
			fFadeVolume -= fFadeSpeed * dInterval;
			SetVolume( fFadeVolume );
			if ( FP_EPSILON < fFadeInTime )
				fFadeSpeed = (float)nMusicVolume / fFadeInTime;
			if ( fFadeVolume < FP_EPSILON )
			{
				FSOUND_Stream_Stop( pStream );
				FSOUND_StopSound( nChannel );
			}
		}
		else if ( bFadeIn )
		{
			fFadeVolume += fFadeSpeed * dInterval;
			if ( nMusicVolume <= fFadeVolume )
				bFadeIn = false;
			int nVolume = (int)fFadeVolume;
			if ( nVolume >= nMusicVolume )
				nVolume = nMusicVolume;
			SetVolume( nVolume );
			if ( FP_EPSILON < fFadeInTime )
				fFadeSpeed = (float)nMusicVolume / fFadeInTime;
		}
		if ( bReset )
		{
			FSOUND_Stream_SetTime( pStream, 104 );
			bReset = false;
		}
	}
	unsigned long GetTime()
	{
		if ( !pStream )
			return 0xFFFFFFFF;
		return FSOUND_Stream_GetTime( pStream );
	}
	bool Reset()
	{
		//if ( pStream ) FSOUND_Stream_Close( pStream );
		FSOUND_Stream_Stop( pStream );
		FSOUND_StopSound( nChannel );
		nChannel = FSOUND_Stream_Play( 0, pStream );
		FSOUND_Stream_SetTime( pStream, 1040 );
		bool bRet = FSOUND_Stream_SetSyncCallback( pStream, &SynchCallback, (int)this );
		return true;
		PlayStream( szFileName.c_str(), bLoop );
		return true;
		//FSOUND_SetPaused( nChannel, true );
		//bool bRet = FSOUND_Stream_SetTime( pStream, 0 );
		//FSOUND_SetPaused( nChannel, false );
		//bool bRet = FSOUND_Stream_SetPosition( pStream, 0 );
		//nChannel = FSOUND_Stream_Play( 0, pStream );
		//bRet = FSOUND_Stream_SetPosition( pStream, 0 );
		//FSOUND_SetPan( nChannel, FSOUND_STEREOPAN );
		//return bRet && nChannel != -1;
	}
	bool IsPlaying()
	{
		return (FSOUND_IsPlaying( nChannel ) || bSwitch) && !bClose;
	}
	// retail CStream::SetSwitchStream @0x3dd190: queue the replacement; a positive switch time
	// primes the NEW stream's fade-in ramp (the retail "crossfade" -- the old side is closed hard)
	void SetSwitchStream( CStream *pNew, float fFadeInSec = 0 )
	{
		bSwitch = true;
		pSwitch = pNew;
		pNew->bSwitch = true;
		if ( fFadeInSec > 0 )
		{
			pNew->fFadeVolume = 0;
			pNew->bFadeIn = true;
			pNew->fFadeSpeed = (float)nMusicVolume / fFadeInSec;
		}
		Switch(); // CRAP (retail @0x3dd190 does the same immediate switch)
	}
	void Switch()
	{
		if ( !bSwitch )
			return;
		if ( IsValid( pSwitch ) )
		{
			// retail Switch @0x3dcf60: target plays its own file from 0 with fadeInTime=0 -- a
			// fade-in primed by SetSwitchStream survives (PlayStream starts it at ramp volume)
			pSwitch->PlayStream( pSwitch->GetFileName().c_str(), pSwitch->IsLooped(), 0, 0 );
			pSwitch->bSwitch = false;
		}
		bClose = true;
		bSwitch = false;
	}
	void SetVolume( int n ) { FSOUND_SetVolumeAbsolute( nChannel, n ); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
signed char __stdcall SynchCallback( FSOUND_STREAM *stream, void *buff, int len, int param )
{
	if ( !buff )
		return false;
	string str = (char*)buff;
	int nmsec = FSOUND_Stream_GetTime( stream );
//	DebugTrace( "SynchCallback mark %s time=%dmsec\n", str.c_str(), nmsec );
	CPtr<CStream> pStream( (CStream*)param );
	if ( !IsValid(pStream) )
		return true;
//	if ( str == "END" )
//		pStream->bReset = true;
//		FSOUND_Stream_SetTime( pStream->pStream, 104 );
	pStream->Switch();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// Local variables
CDriversInfo drivers;
static NHPTimer::STime timeUpdate;
static CVec3 ptLastListenerPos;
typedef list< CMObj<CObjectBase> > CSamples;
static CSamples samples; // ???
typedef list< CMObj<CStream> > CStreamList;
static CStreamList streams;
typedef unordered_map< int, CMObj<CChannel> > CChannels;
CChannels hashChannel;
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SearchDevices()
{
	if ( FSOUND_GetVersion() < FMOD_VERSION )
	{
		OutputDebugString( "Error : You are using the wrong DLL version!\n" );
		return false;
	}
	FSOUND_SetOutput(FSOUND_OUTPUT_DSOUND);
	int nNumDrivers = FSOUND_GetNumDrivers();
	drivers.resize(nNumDrivers);
	for ( int i = 0; i < nNumDrivers; ++i )
	{
		SDriverInfo &dr = drivers[i];
		dr.sName = (const char *)FSOUND_GetDriverName(i);
		unsigned int nCaps;
		FSOUND_GetDriverCaps( i, &nCaps );
		dr.isHardware3DAccelerated = nCaps & FSOUND_CAPS_HARDWARE;
		dr.supportEAXReverb = nCaps;// & FSOUND_CAPS_EAX;
		dr.supportA3DOcclusions = false; //nCaps & FSOUND_CAPS_GEOMETRY_OCCLUSIONS;
		dr.supportA3DReflections = false; //nCaps & FSOUND_CAPS_GEOMETRY_REFLECTIONS;
		dr.supportReverb = nCaps & FSOUND_CAPS_EAX2;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool Init( const SStartInfo &info )
{
	ASSERT( info.nDriver < drivers.size() );
	FSOUND_SetDriver( info.nDriver );
	ASSERT( !( info.eOutputType == SOUND_A3D && !drivers[info.nDriver].supportA3DOcclusions ) );
/*	FSOUND_OUTPUTTYPES eOut;
	switch ( info.eOutputType )
	{
		case SOUND_NO: eOut = FSOUND_OUTPUT_NOSOUND; break;
		case SOUND_WINMM: eOut = FSOUND_OUTPUT_WINMM; break;
		case SOUND_DSOUND: eOut = FSOUND_OUTPUT_DSOUND; break;
		case SOUND_A3D: 
			if ( !drivers[info.nDriver].supportA3DOcclusions )
				eOut = FSOUND_OUTPUT_DSOUND;
			else
				eOut = FSOUND_OUTPUT_A3D; 
			break;
		default: ASSERT( 0 );
	}
	FSOUND_SetOutput( eOut );*/
	FSOUND_SetHWND( info.hWnd );

	if ( !FSOUND_Init( info.nMixrate, info.nMaxChannels, 0 ) )
	{
		OutputDebugString( "NFMSound::Start():error!\n" );
		ASSERT(0);
		return false;
	}

#ifdef _DEBUG
	OutputDebugString( "Using \"" );
	OutputDebugString( drivers[info.nDriver].sName.c_str() );
	OutputDebugString( "\" sound driver.\n" );
	if ( drivers[info.nDriver].isHardware3DAccelerated )
		OutputDebugString("- Driver supports hardware 3D sound!\n" );
	if ( drivers[info.nDriver].supportEAXReverb )
		OutputDebugString("- Driver supports EAX reverb!\n" );
	if ( drivers[info.nDriver].supportA3DOcclusions )
		OutputDebugString("- Driver supports hardware 3d geometry processing with occlusions!\n" );
	if ( drivers[info.nDriver].supportA3DReflections )
		OutputDebugString("- Driver supports hardware 3d geometry processing with reflections!\n" );
	if ( drivers[info.nDriver].supportReverb )
		OutputDebugString("- Driver supports EAX 2.0 reverb!\n" );
	
	OutputDebugString("Mixer = ");
	switch ( FSOUND_GetMixer() )
	{
		case FSOUND_MIXER_BLENDMODE:	OutputDebugString("FSOUND_MIXER_BLENDMODE\n"); break;
		case FSOUND_MIXER_MMXP5:		OutputDebugString("FSOUND_MIXER_MMXP5\n"); break;
		case FSOUND_MIXER_MMXP6:		OutputDebugString("FSOUND_MIXER_MMXP6\n"); break;
		case FSOUND_MIXER_QUALITY_FPU:	OutputDebugString("FSOUND_MIXER_QUALITY_FPU\n"); break;
		case FSOUND_MIXER_QUALITY_MMXP5:OutputDebugString("FSOUND_MIXER_QUALITY_MMXP5\n"); break;
		case FSOUND_MIXER_QUALITY_MMXP6:OutputDebugString("FSOUND_MIXER_QUALITY_MMXP6\n"); break;
	};
#endif
	NHPTimer::GetTime( &timeUpdate );
	ptLastListenerPos = CVec3(0,0,0);
	vPrevCameraVelocity = CVec3(1e38f,0,0);
	bIsFMODInitialized = true;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsInitialized()
{
	return bIsFMODInitialized;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetSoundAPI @0x3db240 -- the underlying output-device handle.  On the DSOUND output
// (FSOUND_SetOutput(FSOUND_OUTPUT_DSOUND) in SearchDevices) FSOUND_GetOutputHandle()
// returns FMOD's IDirectSound*, which CBinkVideoPlayer hands to BinkSetSoundSystem so
// the movie's audio plays through the same DirectSound device FMOD owns.  Null before
// FMOD is initialized (matches the release: bIsFMODInitialized gate, else 0).
////////////////////////////////////////////////////////////////////////////////////////////////////
void* GetSoundAPI()
{
	if ( !bIsFMODInitialized )
		return 0;
	return FSOUND_GetOutputHandle();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
T *NewSample( FSOUND_SAMPLE *hSample, T *pFake )
{
	if ( !hSample )
	{
		OutputDebugString( "Can't load sample:" );
		OutputDebugString( FMOD_ErrorString(FSOUND_GetError()) );
		OutputDebugString("\n");
		return 0;
	}
	T *smpl = new T( hSample );
	samples.push_back(smpl);
	return smpl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSample2D *LoadSample2D( const void *pData, int nLength )
{
	if ( !bIsFMODInitialized )
		return 0;
	FSOUND_SAMPLE *hSample;
	hSample = FSOUND_Sample_Load( FSOUND_UNMANAGED, (const char*)pData, FSOUND_2D | FSOUND_LOOP_OFF | FSOUND_LOADMEMORY, 0, nLength );
	FSOUND_Sample_SetMode( hSample, FSOUND_LOOP_OFF );
	return NewSample( hSample, (CSample2D*)0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSample3D *LoadSample3D( const void *pData, int nLength, float fMinDistance, float fMaxDistance, int nPriority )
{
	if ( !bIsFMODInitialized )
		return 0;
	FSOUND_SAMPLE *hSample;
	hSample = FSOUND_Sample_Load( FSOUND_UNMANAGED, (const char*)pData, FSOUND_HW3D | FSOUND_LOADMEMORY, 0, nLength );
	FSOUND_Sample_SetDefaults( hSample, -1, -1, -1, nPriority );
	CSample3D *pRes = NewSample( hSample, (CSample3D*)0 );
	pRes->SetMinMaxDistance( fMinDistance, fMaxDistance );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSample3D* GetDefault3DSound()
{
	return new CSample3D();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSound2D *PlaySound( CSample2D *pSample )
{
	if ( !bIsFMODInitialized )
		return 0;
//	ASSERT( IsValid( pSample ) );
	if ( !IsValid( pSample ) )
		return 0;
	int nChannel = FSOUND_PlaySound( FSOUND_FREE, *pSample );
	if ( nChannel == -1 )
		OutputDebugString( "Can't find free channel.\n" );
	CSound2D *pSound = new CSound2D( pSample );
	pSound->nChannel = nChannel;
	hashChannel[nChannel] = pSound;
	return pSound;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsPlaying( CSound2D *pSound )
{
	if ( IsValid( pSound ) )
		return FSOUND_IsPlaying( pSound->nChannel );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsPlaying( CSound3D *pSound )
{
	if ( IsValid( pSound ) )
		return FSOUND_IsPlaying( pSound->nChannel );
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3db4e0
void Pause( CSound2D *pSound, bool bPause )
{
	if ( IsValid( pSound ) )
		FSOUND_SetPaused( pSound->nChannel, bPause );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3db500
void Pause( CSound3D *pSound, bool bPause )
{
	if ( IsValid( pSound ) )
		FSOUND_SetPaused( pSound->nChannel, bPause );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSound3D *Play3DSound( const SPlayParams &params )
{
	if ( !bIsFMODInitialized )
		return 0;
	if ( !IsValid( params.pSample ) )
	{
		ASSERT(0);
		return 0;
	}
	if ( params.pSample->IsEmpty() )
		return 0;
	bool bLoop = params.bLoop;
	if ( params.nLoops != -1 )
		bLoop = params.nLoops > 1;
	if ( bLoop )
		FSOUND_Sample_SetMode( *params.pSample, FSOUND_LOOP_NORMAL );
	else
		FSOUND_Sample_SetMode( *params.pSample, FSOUND_LOOP_OFF );
	int nChannel = FSOUND_PlaySoundEx( FSOUND_FREE, *params.pSample, 0, true );
	if ( nChannel == -1 )
	{
		vector<int> aToDel;
		for ( CChannels::iterator i = hashChannel.begin(); i != hashChannel.end(); ++i )
		{
			if ( !IsValid( i->second ) || !FSOUND_IsPlaying( i->first ) )
			{
				FSOUND_StopSound( i->first );
				aToDel.push_back( i->first );
			}
		}
		for ( vector<int>::iterator j = aToDel.begin(); j != aToDel.end(); ++j )
			hashChannel.erase(*j);
		//
		nChannel = FSOUND_PlaySoundEx( FSOUND_FREE, *params.pSample, 0, true );
	}
	CVec3 vFPos = ConvertPosToFMode( params.position );
	if ( nChannel == -1 )
		OutputDebugString( "Can't find free channel.\n" );
	else
	{
		FSOUND_3D_SetAttributes( nChannel, &vFPos.x, 0 );
		FSOUND_SetVolume( nChannel, params.bFadeIn ? 0 : params.nVolume );
		// retail tStartTime semantics: seek nStartMs into the sample; a start past the sample end
		// means the sound already finished in world time -- keep it silent (stop the channel).
		if ( params.nStartMs > 0 && !bLoop )
		{
			int nFreq = FSOUND_GetFrequency( nChannel );
			unsigned int nOffsetSamples = (unsigned int)( (__int64)nFreq * params.nStartMs / 1000 );
			if ( nOffsetSamples >= FSOUND_Sample_GetLength( *params.pSample ) )
			{
				FSOUND_StopSound( nChannel );
				nChannel = -1;
			}
			else
				FSOUND_SetCurrentPosition( nChannel, nOffsetSamples );
		}
		if ( nChannel != -1 )
			FSOUND_SetPaused( nChannel, false );
	}
	CSound3D *pSound = new CSound3D( params.pSample, params.position, vFPos, params.nVolume, params.nLoops );
	pSound->SetFadeOut( params.bFadeIn, params.bFadeOut, params.nFadeSamples );
	pSound->nChannel = nChannel;
	hashChannel[nChannel] = pSound;
//	DebugTrace( "Playing channels: %d (max channels: %d, hw:%d)\n", FSOUND_GetChannelsPlaying(), FSOUND_GetMaxChannels(), FSOUND_GetNumHardwareChannels() );
	return pSound;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NFMSound::PlayStream @0x3db990: gates on FMOD init, a non-zero music volume and a name;
// bUseExisting adopts an already-open stream of the SAME file (cross-scene music continuity).
CStream* PlayStream( const char *pszName, bool bUseExisting, int nStartMs, bool bLoop, float fFadeInSec )
{
	if ( !bIsFMODInitialized || nMusicVolume == 0 || !pszName )
		return 0;
	if ( bUseExisting )
	{
		for ( CStreamList::iterator i = streams.begin(); i != streams.end(); ++i )
			if ( IsValid( *i ) && (*i)->GetFileName() == pszName )
				return *i;
	}
	CStream *pRes = new CStream;
	pRes->PlayStream( pszName, bLoop, nStartMs, fFadeInSec );
	streams.push_back( pRes );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NFMSound::SwitchStream @0x3dbb50: replace pOldStream with a new stream of the given file;
// an invalid old stream falls through to PlayStream (WITH the adopt-existing search, per retail).
CStream* SwitchStream( CStream *pOldStream, const char *pszNameNewStream, bool bLoop, float fFadeInSec )
{
	if ( !IsValid( pOldStream ) )
		return PlayStream( pszNameNewStream, true, 0, bLoop, fFadeInSec );
	for ( CStreamList::iterator i = streams.begin(); i != streams.end(); ++i )
	{
		if ( IsValid( *i ) && (*i).GetPtr() == pOldStream )
		{
			CStream *pNew = new CStream( pszNameNewStream, bLoop );
			streams.push_back( pNew );
			pOldStream->SetSwitchStream( pNew, fFadeInSec );
			return pNew;
		}
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void FadeOut( CStream *pStream, float fSec )
{
	for ( CStreamList::iterator i = streams.begin(); i != streams.end(); ++i )
	{
		if ( IsValid( *i ) && (*i).GetPtr() == pStream )
			pStream->FadeOut( fSec );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CancelFadeOut( CStream *pStream )
{
	for ( CStreamList::iterator i = streams.begin(); i != streams.end(); ++i )
	{
		if ( IsValid( *i ) && (*i).GetPtr() == pStream )
			pStream->CancelFadeOut();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsPlaying( CStream *pStream )
{
	for ( CStreamList::iterator i = streams.begin(); i != streams.end(); ++i )
	{
		if ( IsValid( *i ) && (*i).GetPtr() == pStream )
			return pStream->IsPlaying();
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail: current playback position ms of a live stream (NSound::CMusic captures it at save time
// so a load can resume mid-track); 0xFFFFFFFF when the stream is gone.
unsigned long GetStreamTime( CStream *pStream )
{
	for ( CStreamList::iterator i = streams.begin(); i != streams.end(); ++i )
	{
		if ( IsValid( *i ) && (*i).GetPtr() == pStream )
			return pStream->GetTime();
	}
	return 0xFFFFFFFF;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetSFXMasterVolume( int nSFX )
{
	if ( !bIsFMODInitialized )
		return;
	FSOUND_SetSFXMasterVolume( nSFX );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetMusicMasterVolume( int nMusic )
{
	if ( !bIsFMODInitialized )
		return;
	for ( CStreamList::iterator i = streams.begin(); i != streams.end(); ++i )
		(*i)->SetVolume( nMusic );
	nMusicVolume = nMusic;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetSpeakerType( ESpeakerType speaker )
{
	if ( !bIsFMODInitialized )
		return;
	switch( speaker )
	{
		case SOUND_SM_MONO: FSOUND_SetSpeakerMode( FSOUND_SPEAKERMODE_MONO ); break;
		case SOUND_SM_STEREO: FSOUND_SetSpeakerMode( FSOUND_SPEAKERMODE_STEREO ); break;
		case SOUND_SM_HEADPHONE: FSOUND_SetSpeakerMode( FSOUND_SPEAKERMODE_HEADPHONES ); break;
		case SOUND_SM_QUAD: FSOUND_SetSpeakerMode( FSOUND_SPEAKERMODE_QUAD ); break;
		case SOUND_SM_SURROUND: FSOUND_SetSpeakerMode( FSOUND_SPEAKERMODE_SURROUND ); break;
		case SOUND_SM_5DOT1: FSOUND_SetSpeakerMode( FSOUND_SPEAKERMODE_DOLBYDIGITAL ); break;
		default: ASSERT( 0 ); break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NFMSound::GetSpeakerType  @0x3dde10  (the SpeakerMode.obj compiland; absent in the dev tree). Probes the
// platform audio device for its configured speaker layout and maps it onto ESpeakerType. The retail body
// talks DirectSound DIRECTLY (this is the SpeakerMode path, NOT the FMOD path); a missing device or failed
// query falls back to SOUND_SM_STEREO. Reconstructed faithfully from the decomp @0x7dde10: DirectSoundCreate
// -> IDirectSound::GetSpeakerConfig (vtbl+0x20) -> switch on DSSPEAKER_CONFIG(cfg) (== the low byte) ->
// SOUND_SM_*. ORIGINAL BUG (confirmed @0x7dde10): the retail AddRefs the freshly-created device (the original's
// CPtr-take) then Releases exactly once per return path, so it leaks one IDirectSound reference each call --
// benign (called once at sound init); reproduced as observed. Currently UNWIRED: the dev sound config maps an
// explicit user string (Sound.cpp VarSetOutputType: "mono"/"stereo"/"headphone"/...) to SetSpeakerType; the
// release uses this getter as the OS auto-detect query (the "auto" default the dev's string map lacks). Wiring
// it would change live sound init, so deferred. dsound.lib is pulled in via the #pragma in this TU's includes.
ESpeakerType GetSpeakerType()
{
	IDirectSound *pDS = 0;
	if ( FAILED( DirectSoundCreate( 0, &pDS, 0 ) ) )
		return SOUND_SM_STEREO;                       // no device -> stereo
	if ( pDS )
		pDS->AddRef();                                // retail CPtr-take (leaks 1 ref -- see ORIGINAL BUG above)
	DWORD dwConfig = 0;
	if ( FAILED( pDS->GetSpeakerConfig( &dwConfig ) ) )
	{
		pDS->Release();
		return SOUND_SM_STEREO;                       // query failed -> stereo
	}
	ESpeakerType result;
	switch ( DSSPEAKER_CONFIG( dwConfig ) )           // DSSPEAKER_CONFIG(cfg) == LOBYTE(cfg)
	{
		case DSSPEAKER_HEADPHONE: result = SOUND_SM_HEADPHONE; break;   // 1
		case DSSPEAKER_MONO:      result = SOUND_SM_MONO;      break;   // 2
		case DSSPEAKER_QUAD:      result = SOUND_SM_QUAD;      break;   // 3
		case DSSPEAKER_SURROUND:  result = SOUND_SM_SURROUND;  break;   // 5
		case DSSPEAKER_5POINT1:   result = SOUND_SM_5DOT1;     break;   // 6
		default:                  result = SOUND_SM_STEREO;   break;    // 4 (STEREO) + DIRECTOUT/any
	}
	pDS->Release();
	return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void Update( const SListener &listener )
{
	if ( !bIsFMODInitialized )
		return;
	double dTimeInterval = NHPTimer::GetTimePassed( &timeUpdate );
	toCamera = listener.toProjective;

	CVec3 vCenter;
	CVec3 vFSoundVel(0,0,0);// = GetVelocity( ConvertPosToFMode( listener.vPosition ), ConvertPosToFMode( ptLastListenerPos ), dTimeInterval );
	
//	DebugTrace( "vel %.3f %.3f %.3f \n", vel.x, vel.z, vel.y );

	if ( vPrevCameraVelocity != vFSoundVel )//fVel)
	{
		float fPos[3] = { 0, 0, 0 };
		vPrevCameraVelocity = vFSoundVel;
		FSOUND_3D_Listener_SetAttributes( fPos,	// position
			&vFSoundVel.x,				// velocity
			0,		//-
			0,		//	forward direction
			1,		//-
			0,			//-
			1,			//	top direction
			0 );		//-
	}

	vector<int> aToDel;
	for ( CChannels::iterator i = hashChannel.begin(); i != hashChannel.end(); ++i )
	{
		if ( !IsValid( i->second ) )
			FSOUND_StopSound( i->first );
		else if ( !FSOUND_IsPlaying( i->first ) )
		{
			FSOUND_StopSound( i->first );
			aToDel.push_back( i->first );
		}
		else
		{
#ifdef _DEBUG
			int npr = FSOUND_GetPriority( i->first );
#endif
			i->second->Update( dTimeInterval );
		}
	}
	for ( CStreamList::iterator i = streams.begin(); i != streams.end(); )
	{
		if ( !IsValid( *i ) || !(*i)->IsPlaying() )
		{
			//++i;
			i = streams.erase( i );
		}
		else
		{
			(*i)->Update( dTimeInterval );
			++i;
		}
	}
	
	for ( vector<int>::iterator j = aToDel.begin(); j != aToDel.end(); ++j )
		hashChannel.erase(*j);
	
	/// ???
	EraseInvalidRefs( &samples );
	EraseInvalidRefs( &streams );
	
	FSOUND_Update();
	ptLastListenerPos = listener.vPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void Done()
{
	if ( !bIsFMODInitialized )
		return;

	hashChannel.clear();
	samples.clear();
	streams.clear();
	FSOUND_Close();
	bIsFMODInitialized = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NFMSound
using namespace NFMSound;
BASIC_REGISTER_CLASS( CSound3D )
BASIC_REGISTER_CLASS( CSound2D )
BASIC_REGISTER_CLASS( CSample3D )
BASIC_REGISTER_CLASS( CSample2D )
BASIC_REGISTER_CLASS( CStream )