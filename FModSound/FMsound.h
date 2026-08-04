#ifndef __N_SOUND_FMOD__
#define __N_SOUND_FMOD__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\Misc\Geom.h"

struct FSOUND_SAMPLE;
namespace NFMSound
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Generic drivers/modes info
struct SDriverInfo
{
public:
	string sName;
	bool isHardware3DAccelerated; // This driver supports hardware accelerated 3d sound.
	bool supportEAXReverb;        // This driver supports EAX reverb
	bool supportA3DOcclusions;    // This driver supports (A3D) geometry occlusions
	bool supportA3DReflections;   // This driver supports (A3D) geometry reflections
	bool supportReverb;           // This driver supports EAX2/A3D3 reverb  
};
typedef vector<SDriverInfo> CDriversInfo;
externA5 CDriversInfo drivers;			// [0] is default driver

////////////////////////////////////////////////////////////////////////////////////////////////////
enum ESpeakerType
{
	SOUND_SM_MONO,
	SOUND_SM_STEREO,
	SOUND_SM_HEADPHONE,
	SOUND_SM_QUAD,
	SOUND_SM_SURROUND,
	SOUND_SM_5DOT1
};
enum EOutputType
{
	SOUND_NO,
	SOUND_WINMM,
	SOUND_DSOUND,
	SOUND_A3D
};
struct SStartInfo
{
	int nMaxChannels;	// Number of additional software channels, hardware channels are always in use
	int nMixrate;
	EOutputType eOutputType;
	int nDriver;
	HWND hWnd;	// ������� ���� ����������, ����� ������� ���� ���� ��� ������ ������

	SStartInfo() : nMaxChannels(32), nMixrate(44100), eOutputType(SOUND_DSOUND), nDriver(0), hWnd(0) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SListener
{
//	CVec3 position;
//	CVec3 forward;
//	CVec3 top;
	SHMatrix toProjective;
	CVec3 vPosition;
	
	SListener() {}// position.Set(0,0,0); forward.Set(0,0,1.f); top.Set(0,1.f,0); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// ��� ������ �� ����� ������� ������������ � ���������� � FMod
class CSample2D;
class CSample3D;
class CSound3D;
class CSound2D;
class CStream;
////////////////////////////////////////////////////////////////////////////////////////////////////
class ISound3D
{
public:
	virtual void SetPosition( const CVec3 &pos ) = 0;
};
struct SPlayParams
{
	CSample3D *pSample;
	CVec3 position;
	bool bLoop;
	int  nVolume;
	int  nLoops;
	bool bFadeIn;
	bool bFadeOut;
	int  nFadeSamples;
	// start OFFSET into the sample, ms (retail SPlayParams.tStartTime): the mixer hands sounds a
	// world-time delay so a sound (re)submitted late plays its remaining TAIL -- or nothing at all
	// when the offset is past the sample end (e.g. an old death grunt on corpse reveal).
	int  nStartMs;

	SPlayParams()
	{
		pSample = 0;
		bLoop = false;
		nVolume = 255;
		position = VNULL3;
		nLoops = -1;
		bFadeIn = false;
		bFadeOut = false;
		nFadeSamples = 0;
		nStartMs = 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// fill drivers table
bool SearchDevices();	
bool Init( const SStartInfo &info );
void Done();
bool IsInitialized();
void* GetSoundAPI();   // @0x3db240 -- underlying output device handle (the IDirectSound* on the DSOUND output) so Bink can share FMOD's device; null until FMOD is initialized
//     ,     .
void Update( const SListener &listen );
CSample2D* LoadSample2D( const void *pData, int nLength );
CSample3D* LoadSample3D( const void *pData, int nLength, float fMinDistance, float fMaxDistance, int nPriority, int nStartSamples = 0, int nEndingSamples = 0 );
CSample3D* GetDefault3DSound();
CSound2D* PlaySound( CSample2D *pSample );
CSound3D* Play3DSound( const SPlayParams &params );
// retail @0x3db990: PlayStream( name, bUseExisting, nStartMs, bLoop, fFadeInSec ).
// bUseExisting = adopt an already-open stream of the SAME file instead of reopening -- this is
// the engine's cross-scene music continuity (menu screens, mission ambient carrying into the
// base). nStartMs seeks into the clip (save/load resume); fFadeInSec ramps the volume from 0.
CStream* PlayStream( const char *pszName, bool bUseExisting, int nStartMs, bool bLoop, float fFadeInSec );
// retail @0x3dbb50: the new stream replaces pOldStream (which is closed); fFadeInSec ramps the
// NEW stream in over the switch (retail SetSwitchStream @0x3dd190 fade-in priming).
CStream* SwitchStream( CStream *pOldStream, const char *pszNameNewStream, bool bLoop, float fFadeInSec ); // �� ��������� ������� ������������� �� ����� �����
bool IsPlaying( CStream *pStream );
unsigned long GetStreamTime( CStream *pStream );   // current play position ms, 0xFFFFFFFF if not playing (retail NSound::CMusic save capture)
bool IsPlaying( CSound2D *pSound );
bool IsPlaying( CSound3D *pSound );
// retail @0x3db4e0 / @0x3db500: freeze/resume the live FMOD channel (the menu-over-mission pause)
void Pause( CSound2D *pSound, bool bPause );
void Pause( CSound3D *pSound, bool bPause );
void FadeOut( CStream *pStream, float fSec );
void CancelFadeOut( CStream *pStream );
void SetSFXMasterVolume( int nSFX );
void SetMusicMasterVolume( int nMusic );
void SetSpeakerType( ESpeakerType speaker );
ESpeakerType GetSpeakerType();   // @0x3dde10 (SpeakerMode.obj) -- OS speaker-layout auto-detect via DirectSound
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif //__N_SOUND_FMOD__
