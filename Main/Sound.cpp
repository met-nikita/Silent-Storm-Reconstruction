#include "StdAfx.h"
#include "Sound.h"
#include "SoundFormat.h"
#include "Transform.h"
#include "..\DBFormat\DataSound.h"
#include "..\FModSound\FMSound.h"
#include "..\Misc\BasicShare.h"
#include "..\Misc\HPTimer.h"
#include "..\Misc\RandomGen.h"
#include "..\MiscDll\Commands.h"
#include "..\MiscDll\LogStream.h"
#include "SoundEffect.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NSound
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static HWND hWnd;
CBasicShare<int, CFileSample2D> share2DSamples(127);
CBasicShare<int, CFileSample3D> share3DSamples(128);
// retail SetMusicStopTime/SetMusicStartTime @0x304ae0/@0x304b80 fallback window (0x1d4c0 ms) when
// no valid music record is bound.
const int N_MUSIC_FALLBACK_MS = 120000;
// last-resort fade when a record carries no FadeOut ms (dev db without the column); retail data
// is 20000ms and lives on NDb::CMusic::nFadeOut.
const float N_MUSIC_FADE_SEC = 4.0f;
////////////////////////////////////////////////////////////////////////////////////////////////////
// dev stand-in for retail CSoundScene.pTime (the CFuncBase<unsigned long> data-graph time node the
// scene ctor @0x3058c0 receives): a monotonic module ms clock the music deadlines are computed on.
static __int64 GetMusicTimeMs()
{
	static bool bInit = false;
	static NHPTimer::STime tLast;
	static double dAccumMs = 0;
	if ( !bInit )
	{
		NHPTimer::GetTime( &tLast );
		bInit = true;
	}
	dAccumMs += NHPTimer::GetTimePassed( &tLast ) * 1000.0;
	return (__int64)dAccumMs;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSound: public CObjectBase
{
	OBJECT_BASIC_METHODS(CSound);
public:
	ZDATA
	CObj<NFMSound::CSound3D> pSound;
	CDGPtr< CFuncBase<CVec3> > pPos;
	CDGPtr< CPtrFuncBase<NFMSound::CSample3D> > pSample;
	CDBPtr<NDb::CSound> pDBSample;
	// retail CSound+0x2c: how far (ms) INTO the sample playback starts (the render mixer's
	// max(0, now - tStart - 50) delay). A sound whose offset is past the sample end never plays --
	// that is what stops an OLD sound replaying when its emitter (re)enters the visible set.
	int nStartMs = 0;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pSound); f.Add(3,&pPos); f.Add(4,&pSample); f.Add(5,&pDBSample); f.Add(6,&nStartMs); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSound2D: public ISound2D
{
	OBJECT_BASIC_METHODS(CSound2D);
public:
	ZDATA
	CObj<NFMSound::CSound2D> pSound;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pSound); return 0; }
	virtual bool IsPlaying()
	{
		return NFMSound::IsPlaying( pSound );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMusic: public CObjectBase
{
	OBJECT_BASIC_METHODS(CMusic);
public:
	CPtr<NFMSound::CStream> pStream;	// runtime only -- retail never serializes the stream
	ZDATA
	CDBPtr<NDb::CMusic> pMusic;
	bool bLoop = true;
	unsigned int tCurrent = 0xFFFFFFFF;	// play position ms captured at save; 0xFFFFFFFF = nothing to resume
	ZEND int operator&( CStructureSaver &f )
	{
		// retail NSound::CMusic::operator& @0x307000 (byte-exact tags): 2=pMusic (CDBPtr),
		// 3=bLoop, 4=tCurrent. tCurrent is captured LIVE from the playing stream right before the
		// chunk write so a load can resume mid-track (retail calls GetStreamTime the same way; on
		// read the chunk overwrites the captured value).
		if ( IsValid( pStream ) && NFMSound::IsPlaying( pStream ) )
			tCurrent = NFMSound::GetStreamTime( pStream );
		else
			tCurrent = 0xFFFFFFFF;
		f.Add(2,&pMusic); f.Add(3,&bLoop); f.Add(4,&tCurrent);
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// The CURRENT music track, module-global so it survives interface switches (retail parity: the
// retail stream layer is global and a scene's death never closes the playing track -- switching
// main menu -> campaign -> hero menu keeps ONE stream running). A new CSoundScene whose ambient is
// the SAME music record adopts this instead of reopening the file from zero; a scene wanting a
// DIFFERENT track goes through SwitchStream so the old one is properly closed (no overlay).
// Runtime-only, never serialized (matches the per-scene pMusic, which is outside ZDATA too).
static CObj<CMusic> theCurrentMusic;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail CSoundScene music machine (Sound.obj), rebuilt to the retail shape over the dev framework:
//   * per-scene pAmbient/pCombat CTMusic pools + edge-triggered eCurrent/eNextMusicType,
//   * a data-driven play/fade/silence cycle: SetMusicStopTime (@0x304ae0, now + PlayTime +
//     rnd(RndPlayTime)), FadeOutMusic (@0x304c20, fade over the record's FadeOut ms),
//     SetMusicStartTime (@0x304b80, now + Silence + rnd(RndSilence)), then the next track of the
//     pending type (Draw @0x3052d0 stage 5),
//   * cross-scene continuity through NFMSound::PlayStream's adopt-same-file mode (retail @0x3db990)
//     -- the confirmed menu-music and mission->base ambient carry-over -- while a surviving FOREIGN
//     track is closed through SwitchStream when this scene starts its own music.
class CSoundScene: public ISoundScene
{
	OBJECT_BASIC_METHODS(CSoundScene);
private:
	list< CPtr<CSound> > sounds;
	list< CPtr<CSound2D> > sounds2D;
	list< CPtr<CSoundEffect> > effects;
	bool bSilence;			// runtime (retail leaves it out of the serializer too)
	bool bRearmStopTime;	// dev: deadlines from a save belong to another session's clock -> re-arm
	ZDATA
	CObj<CMusic> pMusic;
	CDBPtr<NDb::CTMusic> pAmbient;
	CDBPtr<NDb::CTMusic> pCombat;
	NDb::EMusicType eCurrentMusicType;
	NDb::EMusicType eNextMusicType;
	__int64 tStartMusic;
	__int64 tStopMusic;
	SRand rand;
public:
	ZEND int operator&( CStructureSaver &f )
	{
		// retail CSoundScene::operator& @0x307440 tag layout: 2=tStartMusic(8), 4=pTime (the DG
		// time node -- dev has none, the chunk is skipped), 5=pMusic (CObj), 6=pAmbient,
		// 7=pCombat, 8=eCurrentMusicType, 9=eNextMusicType, 10=tStopMusic(8), 11=rand(4).
		// Tags 6/7 are CDBPtr<CTMusic> pool ids, not already-picked CMusic record ids.
		f.Add(2,&tStartMusic);
		f.Add(5,&pMusic);
		f.Add(6,&pAmbient);
		f.Add(7,&pCombat);
		f.Add(8,&eCurrentMusicType);
		f.Add(9,&eNextMusicType);
		f.Add(10,&tStopMusic);
		f.Add(11,&rand.seed.nSeed);
		if ( f.IsReading() )
		{
			// the loaded deadlines were computed on another session's music clock: let a restored
			// track start/resume immediately and re-arm its stop window from data on first Draw
			tStartMusic = 0;
			bRearmStopTime = true;
		}
		return 0;
	}

public:
	CSoundScene( NDb::CTMusic *_pAmbient = 0, NDb::CTMusic *_pCombat = 0 );

	virtual CSound* Add3DSound( NDb::CSound *pSample, CFuncBase<CVec3> *pPos, STime tStart );
	virtual CSound2D* Add2DSound( NDb::CSound *pSample );
	virtual CSoundEffect* AddEffect( NDb::CSoundEffect *pEff, STime stBeginTime, CFuncBase<STime> *pTime, CFuncBase<CVec3> *pPos, const vector<int> &flags );

	virtual void SetMusic( NDb::EMusicType eType );
	virtual void FadeOutMusic();
	virtual void Draw( CTransformStack *pTS );

	// retail @0x304d80 (ISoundScene vtbl+0x28): freeze/resume every live channel -- effects, 3D
	// sounds, 2D sounds, in that order. The music stream is NOT touched (retail menus keep music).
	virtual void Pause( bool bPause )
	{
		for ( list< CPtr<CSoundEffect> >::iterator i = effects.begin(); i != effects.end(); ++i )
			if ( IsValid( *i ) )
				(*i)->Pause( bPause );
		for ( list< CPtr<CSound> >::iterator i = sounds.begin(); i != sounds.end(); ++i )
			if ( IsValid( *i ) )
				NFMSound::Pause( (*i)->pSound, bPause );
		for ( list< CPtr<CSound2D> >::iterator i = sounds2D.begin(); i != sounds2D.end(); ++i )
			if ( IsValid( *i ) )
				NFMSound::Pause( (*i)->pSound, bPause );
	}

private:
	void SetMusicStopTime();	// retail @0x304ae0
	void SetMusicStartTime();	// retail @0x304b80
	void DoFadeOutMusic();		// retail FadeOutMusic @0x304c20 (the vtbl+0x20 wind-down virtual)
	bool StartMusic( NDb::CMusic *pTrack, int nStartMs = 0 );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CSoundScene::CSoundScene( NDb::CTMusic *_pAmbient, NDb::CTMusic *_pCombat ):
	bSilence(true), bRearmStopTime(false), pAmbient(_pAmbient), pCombat(_pCombat),
	eCurrentMusicType(NDb::MT_AMBIENT), eNextMusicType(NDb::MT_AMBIENT),
	tStartMusic(0), tStopMusic(0)
{
	// retail ctor @0x3058c0: stores both the ambient and combat CTMusic pools; bSilence=true,
	// both types MT_AMBIENT,
	// tStopMusic=0 and tStartMusic seeded to "now" -- a fresh scene launches its ambient on the
	// very first Draw.
	tStartMusic = GetMusicTimeMs();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSound* CSoundScene::Add3DSound( NDb::CSound *pSample, CFuncBase<CVec3> *pPos, STime tStart )
{
	if ( !pSample || !pPos )
	{
		ASSERT( pSample && pPos );
		return 0;
	}
	CSound *pSound = new CSound;
	pSound->pDBSample = pSample;
	pSound->pSample = share3DSamples.Get( pSample->GetRecordID() );
	pSound->pPos = pPos;
	pSound->pPos.Refresh();
	pSound->nStartMs = (int)tStart;	// retail @0x305bf0: the 3rd arg is the start OFFSET (ms), stored at CSound+0x2c
	if ( NFMSound::IsInitialized() )
		pSound->pSample.Refresh();
	sounds.push_back( pSound );

	return pSound;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSound2D* CSoundScene::Add2DSound( NDb::CSound *pSample )
{
	if ( !pSample )
	{
		ASSERT( pSample );
		return 0;
	}
	CDGPtr< CPtrFuncBase<NFMSound::CSample2D> > pSam = share2DSamples.Get( pSample->GetRecordID() );
	pSam.Refresh();
	CSound2D *p = new CSound2D;
	p->pSound = NFMSound::PlaySound( pSam->GetValue() );
	sounds2D.push_back( p );
	return p;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail SetMusicStopTime @0x304ae0: schedule the end of the current track's play window --
// now + PlayTime + rnd(RndPlayTime) ms from its record; 120000ms fallback without a valid record.
void CSoundScene::SetMusicStopTime()
{
	__int64 now = GetMusicTimeMs();
	NDb::CMusic *pInfo = 0;
	if ( IsValid( pMusic ) && IsValid( pMusic->pMusic ) )
		pInfo = pMusic->pMusic;
	if ( pInfo )
		tStopMusic = now + pInfo->nPlayTime + rand.Get( pInfo->nRndPlayTime );
	else
		tStopMusic = now + N_MUSIC_FALLBACK_MS;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail SetMusicStartTime @0x304b80: schedule the end of the between-tracks silence --
// now + Silence + rnd(RndSilence) ms from the record that just finished; 120000ms fallback.
void CSoundScene::SetMusicStartTime()
{
	__int64 now = GetMusicTimeMs();
	NDb::CMusic *pInfo = 0;
	if ( IsValid( pMusic ) && IsValid( pMusic->pMusic ) )
		pInfo = pMusic->pMusic;
	if ( pInfo )
		tStartMusic = now + pInfo->nSilence + rand.Get( pInfo->nRndSilence );
	else
		tStartMusic = now + N_MUSIC_FALLBACK_MS;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail FadeOutMusic @0x304c20 (the internal vtbl+0x20 virtual SetMusic(MT_AMBIENT) and the Draw
// stop-window both call): fade the bound stream over the RECORD's FadeOut ms (retail data: 20s) --
// data-driven, not hard-coded.
void CSoundScene::DoFadeOutMusic()
{
	if ( !IsValid( pMusic ) )
	{
		// dev keep: adopt an ORPHANED cross-scene stream so a combat track that outlived its
		// origin scene (in-game-menu load, restart.sav resume, zone re-enter) can be wound down.
		if ( IsValid( theCurrentMusic ) && IsValid( theCurrentMusic->pStream )
			&& NFMSound::IsPlaying( theCurrentMusic->pStream ) )
			pMusic = theCurrentMusic;
		else
			return;
	}
	if ( !IsValid( pMusic->pStream ) )
		return;
	float fFadeSec = N_MUSIC_FADE_SEC;
	if ( IsValid( pMusic->pMusic ) && pMusic->pMusic->nFadeOut > 0 )
		fFadeSec = pMusic->pMusic->nFadeOut * 0.001f;
	NFMSound::FadeOut( pMusic->pStream, fFadeSec );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// launch pTrack as the scene's current music. Cross-scene continuity: a surviving stream of the
// SAME file is adopted (continues seamlessly) through PlayStream's adopt mode; a surviving FOREIGN
// track is closed through SwitchStream (retail switch, fading the new track in over its FadeIn ms).
bool CSoundScene::StartMusic( NDb::CMusic *pTrack, int nStartMs )
{
	CMusic *pM = new CMusic;
	pM->pMusic = pTrack;
	pM->bLoop = true;	// retail: music streams always loop; the data-driven stop window ends them
	float fFadeInSec = 0;
	if ( pTrack->nFadeIn > 0 )
		fFadeInSec = pTrack->nFadeIn * 0.001f;
	NFMSound::CStream *pOld = 0;
	bool bSameFile = false;
	if ( IsValid( theCurrentMusic ) && IsValid( theCurrentMusic->pStream )
		&& NFMSound::IsPlaying( theCurrentMusic->pStream ) )
	{
		pOld = theCurrentMusic->pStream.GetPtr();
		bSameFile = IsValid( theCurrentMusic->pMusic )
			&& theCurrentMusic->pMusic->szFileName == pTrack->szFileName;
	}
	if ( pOld && !bSameFile )
		pM->pStream = NFMSound::SwitchStream( pOld, pTrack->szFileName.c_str(), true, fFadeInSec );
	else
		pM->pStream = NFMSound::PlayStream( pTrack->szFileName.c_str(), true, nStartMs, true, fFadeInSec );
	if ( !IsValid( pM->pStream ) )
		return false;
	pMusic = pM;
	theCurrentMusic = pM;
	bSilence = false;
	SetMusicStopTime();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail SetMusic(EMusicType) @0x304db0 -- the edge-triggered type machine UpdateSound drives.
void CSoundScene::SetMusic( NDb::EMusicType eType )
{
	if ( eType == eNextMusicType )
		return;
	eNextMusicType = eType;
	if ( eType == NDb::MT_AMBIENT )
	{
		// retail: the ambient edge just winds the current track down (vtbl+0x20); Draw's silence
		// machine brings the ambient back after the record's Silence window
		DoFadeOutMusic();
		return;
	}
	if ( eType != NDb::MT_COMBAT )
		return;
	if ( IsValid( pMusic ) && eCurrentMusicType == NDb::MT_COMBAT )
	{
		// combat re-detected while the combat track was fading out -- just cancel the fade
		NFMSound::CancelFadeOut( pMusic->pStream );
		return;
	}
	NDb::CMusic *pTrack = 0;
	if ( IsValid( pCombat ) )
		pTrack = pCombat->GetMusic( &rand );
	if ( !pTrack )
		return;
	if ( StartMusic( pTrack ) )
		eCurrentMusicType = NDb::MT_COMBAT;
	else
		SetMusicStopTime();	// retail arms the stop window even when the switch failed
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::FadeOutMusic()
{
	// retail CMission::UpdateSound @0x1feb50 calls SetMusic(MT_AMBIENT) when calm; the dev mission
	// calls FadeOutMusic() -- map it onto the retail ambient EDGE so eNextMusicType flips back to
	// ambient (otherwise the silence machine would relaunch COMBAT after the fade).
	SetMusic( NDb::MT_AMBIENT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::Draw( CTransformStack *pTS )
{
	for ( list< CPtr<CSound> >::iterator it = sounds.begin(); it != sounds.end(); )
	{
		CSound *pSound = *it;
		if ( !IsValid( pSound ) || ( !IsValid( pSound->pSound ) && !pSound->pSample ) )
		{
			it = sounds.erase( it );
			continue;
		}
		if ( NFMSound::IsInitialized() )
		{
			pSound->pPos.Refresh();
			CVec3 ptPos = pSound->pPos->GetValue();
			if ( pSound->pSound )
			{
				CDynamicCast<NFMSound::ISound3D> p3D( pSound->pSound );
				p3D->SetPosition( ptPos );
			}
			else
			{
				ASSERT( IsValid( pSound->pSample ) );
				pSound->pSample.Refresh();
				NFMSound::CSample3D *pData = pSound->pSample->GetValue();
				if ( pData )
				{
					NFMSound::SPlayParams p;
					p.pSample = pData;
					p.bLoop = pSound->pDBSample->bLoop || (pSound->pDBSample->nEndingSamples > 0);
					p.position = pSound->pPos->GetValue();
					p.nStartMs = pSound->nStartMs;	// retail Draw @0x3052d0: SPlayParams.tStartTime = CSound+0x2c
					pSound->pSound = NFMSound::Play3DSound( p );
				}
			}
		}
		//
		++it;
	}
	for ( list< CPtr<CSound2D> >::iterator it = sounds2D.begin(); it != sounds2D.end(); )
	{
		CSound2D *pSound = *it;
		if ( !pSound || !IsValid( pSound->pSound ) )
		{
			it = sounds2D.erase( it );
			continue;
		}
		++it;
	}
	//
	for ( list< CPtr<CSoundEffect> >::iterator i = effects.begin(); i != effects.end(); )
		if ( !IsValid( *i ) || (*i)->Update() )
			i = effects.erase( i );
		else
			++i;
	//
	if ( NFMSound::IsInitialized() )
	{
		NFMSound::SListener listener;
		CVec4 vC;
		pTS->Get().backward.RotateHVector( &vC, CVec4(0,0,1,0) );
		listener.vPosition = CVec3( vC.x/vC.w, vC.y/vC.w, vC.z/vC.w );
		listener.toProjective = pTS->Get().forward;

		NFMSound::Update( listener );
	}
	//
	// ---- retail music state machine (Draw @0x3052d0 stage 5) ----
	//
	// (a) resume a restored track: a loaded pMusic carries its record and a saved play position
	// but no live stream (retail: PlayStream WITHOUT the adopt search, seeking to tCurrent).
	if ( IsValid( pMusic ) && IsValid( pMusic->pMusic ) && !IsValid( pMusic->pStream )
		&& pMusic->tCurrent != 0xFFFFFFFF )
	{
		pMusic->pStream = NFMSound::PlayStream( pMusic->pMusic->szFileName.c_str(), false,
			(int)pMusic->tCurrent, pMusic->bLoop, 0 );
		eNextMusicType = eCurrentMusicType;	// retail: re-arm the pending type to the restored one
		if ( IsValid( pMusic->pStream ) )
			theCurrentMusic = pMusic;
		else
			pMusic = 0;	// retail: a failed restart drops the music
	}
	__int64 now = GetMusicTimeMs();
	if ( IsValid( pMusic ) && NFMSound::IsPlaying( pMusic->pStream ) )
	{
		bSilence = false;
		if ( bRearmStopTime )
		{
			// dev: the loaded stop deadline was computed on another session's clock
			SetMusicStopTime();
			bRearmStopTime = false;
		}
		if ( now > tStopMusic )
			DoFadeOutMusic();	// retail: play window over -> wind the track down (vtbl+0x20)
	}
	else
	{
		bRearmStopTime = false;
		if ( !bSilence )
		{
			// first silent frame: arm the between-tracks silence window from the finished record
			bSilence = true;
			SetMusicStartTime();
		}
		else if ( now >= tStartMusic )
		{
			// silence over: choose a weighted track from the pending type's CTMusic pool.
			NDb::CTMusic *pPool = 0;
			NDb::CMusic *pTrack = 0;
			if ( eNextMusicType == NDb::MT_AMBIENT )
			{
				if ( IsValid( pAmbient ) )
					pPool = pAmbient;
			}
			else
			{
				if ( IsValid( pCombat ) )
					pPool = pCombat;
			}
			if ( pPool )
				pTrack = pPool->GetMusic( &rand );
			eCurrentMusicType = eNextMusicType;	// retail publishes the type even without a track
			if ( pTrack )
			{
				// retail @0x705c9b: if Draw arrives after the exact deadline, begin that far
				// into the looping stream instead of restarting it from sample zero.
				__int64 nStartMs = now - tStartMusic;
				StartMusic( pTrack, nStartMs > 0 ? (int)nStartMs : 0 );
			}
			else if ( IsValid( theCurrentMusic ) && IsValid( theCurrentMusic->pStream )
				&& NFMSound::IsPlaying( theCurrentMusic->pStream ) )
			{
				// dev keep: this scene has NO track of its own (the chapter/global map scenes are
				// created with CreateSoundScene(0)) but a foreign stream survived the switch --
				// wind it down instead of letting it loop forever (the "menu music leaks into the
				// chapter map" / "combat stuck after campaign start" bugs). FadeOut no-ops while a
				// fade is already running, so repeating it per frame is harmless.
				// CROSS-SCENE FADE = the DEFAULT 4s, NOT the record's own nFadeOut: the retail
				// combat records carry FadeOut=20000ms -- that 20s relax is the IN-SCENE
				// combat->calm transition (CMission::UpdateSound's MT_AMBIENT edge).
				// NOTE (2026-07-05): the campaign bases are NOT in this recordless branch -- the
				// retail Steam game.db gives them ambient POOLS (variant 5246 -> MusicTemplates 124
				// = ambient07, 5376 -> 93 = ambient02/09; the earlier "NULL music" reading came
				// from the lagging SQL mirror). Their StartMusic path closes a surviving foreign
				// stream through SwitchStream instead.
				NFMSound::FadeOut( theCurrentMusic->pStream, N_MUSIC_FADE_SEC );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSoundEffect* CSoundScene::AddEffect( NDb::CSoundEffect *pEff, STime stBeginTime, CFuncBase<STime> *pTime, CFuncBase<CVec3> *pPos, const vector<int> &flags )
{
	if ( !IsValid( pEff ) )
		return 0;
	CSoundEffect *p = new CSoundEffect( pEff, stBeginTime, pTime, pPos, flags );
	effects.push_back( p );
	return p;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool InitSound( HWND _hWnd )
{
	hWnd = _hWnd;
	return NFMSound::SearchDevices();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool SetMode( bool bInitSound )
{
	if ( !bInitSound )
		return true;

	NFMSound::SStartInfo info;
	info.hWnd = hWnd;
	return NFMSound::Init( info );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool SetModeFromConfig()
{
	NGlobal::CValue sValue;

	bool bInitSound = false;
	sValue = NGlobal::GetVar( "sound_mode", 1.0f );
	if ( sValue.GetFloat() != 0 )
		bInitSound = true;

	if ( !SetMode( bInitSound ) )
		return false;

	NFMSound::SetSFXMasterVolume( NGlobal::GetVar( "sound_sfxvolume" ).GetFloat() * 0xFF );
	NFMSound::SetMusicMasterVolume( NGlobal::GetVar( "sound_musicvolume" ).GetFloat() * 0xFF );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
ISoundScene* CreateSoundScene( NDb::CTMusic *pAmbient, NDb::CTMusic *pCombat )
{
	return new CSoundScene( pAmbient, pCombat );	// retail @0x305ad0 (two-slot factory)
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void DoneSound()
{
	NFMSound::Done();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Commands/Vars
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandSoundUpdate( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	DoneSound();
	SetModeFromConfig();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarSetSfxVolume( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	NFMSound::SetSFXMasterVolume( sValue.GetFloat() * 0xFF );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void VarSetMusicVolume( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	NFMSound::SetMusicMasterVolume( sValue.GetFloat() * 0xFF );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// (v1.2 deleted the whole sound_outputmode mechanism -- registration, VarSetOutputType handler and
// the SetModeFromConfig output-mode stage.)
////////////////////////////////////////////////////////////////////////////////////////////////////
// v1.2 SoundInit @0x706ae0: sound_mode is registered UNSAVED, both volumes default 0.75 (saved),
// and the v1.1 sound_outputmode registration is dropped entirely (its SetModeFromConfig stage is
// gone too -- already ported above).
START_REGISTER(Sound)
	REGISTER_CMD( "sound_update", CommandSoundUpdate )
	////
	REGISTER_VAR( "sound_mode", 0, 1.0f, false )
	REGISTER_VAR( "sound_sfxvolume", VarSetSfxVolume, 0.75f, true )
	REGISTER_VAR( "sound_musicvolume", VarSetMusicVolume, 0.75f, true )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NSound;
BASIC_REGISTER_CLASS( ISoundScene )
REGISTER_SAVELOAD_CLASS( 0x03081147, CSound )
REGISTER_SAVELOAD_CLASS( 0xa1063160, CMusic )		// retail classreg id for NSound::CMusic (pMusic is serialized in the scene's tag 5)
REGISTER_SAVELOAD_CLASS( 0x02881171, CSoundScene )
// retail saveload ids (serialization-convergence W1; s2_scratch docs/SERIALIZATION_CONVERGENCE.md)
REGISTER_SAVELOAD_CLASS( 0xA1863130, CSound2D )
