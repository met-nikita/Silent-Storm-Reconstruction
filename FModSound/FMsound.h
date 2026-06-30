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
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// fill drivers table
bool SearchDevices();	
bool Init( const SStartInfo &info );
void Done();
bool IsInitialized();
void* GetSoundAPI();   // @0x3db240 -- underlying output device handle (the IDirectSound* on the DSOUND output) so Bink can share FMOD's device; null until FMOD is initialized
// ���� ����� ��� ����� ����, � ������ ��� �� �����.
void Update( const SListener &listen );
CSample2D* LoadSample2D( const void *pData, int nLength );
CSample3D* LoadSample3D( const void *pData, int nLength, float fMinDistance = 1, float fMaxDistance = 100, int nPriority = 0 );
CSample3D* GetDefault3DSound();
CSound2D* PlaySound( CSample2D *pSample );
CSound3D* Play3DSound( const SPlayParams &params );
CStream* PlayStream( const char *pszName, bool bLoop );
CStream* SwitchStream( CStream *pOldStream, const char *pszNameNewStream, bool bLoop ); // �� ��������� ������� ������������� �� ����� ����� 
bool IsPlaying( CStream *pStream );
bool IsPlaying( CSound2D *pSound );
bool IsPlaying( CSound3D *pSound );
void FadeOut( CStream *pStream, float fSec );
void CancelFadeOut( CStream *pStream );
void SetSFXMasterVolume( int nSFX );
void SetMusicMasterVolume( int nMusic );
void SetSpeakerType( ESpeakerType speaker );
ESpeakerType GetSpeakerType();   // @0x3dde10 (SpeakerMode.obj) -- OS speaker-layout auto-detect via DirectSound
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif //__N_SOUND_FMOD__
