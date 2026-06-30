#include "StdAfx.h"
#include "Sound.h"
#include "SoundFormat.h"
#include "Transform.h"
#include "..\DBFormat\DataSound.h"
#include "..\FModSound\FMSound.h"
#include "..\Misc\BasicShare.h"
#include "..\Misc\HPTimer.h"
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
const int N_MAX_SILENCE = 120;
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
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pSound); f.Add(3,&pPos); f.Add(4,&pSample); f.Add(5,&pDBSample); return 0; }
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
	ZDATA
	CPtr<NFMSound::CStream> pStream;
	CDBPtr<NDb::CMusic> pMusic;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pStream); f.Add(3,&pMusic); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSoundScene: public ISoundScene
{
	OBJECT_BASIC_METHODS(CSoundScene);
private:
	CObj<CMusic> pMusic;
	list< CPtr<CSound> > sounds;
	list< CPtr<CSound2D> > sounds2D;
	list< CPtr<CSoundEffect> > effects;
	ZDATA
	bool bSilence;
	NHPTimer::STime tStartSilence;
	CDBPtr<NDb::CMusic> pAmbient;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bSilence); f.Add(3,&tStartSilence); f.Add(4,&pAmbient); return 0; }

public:
	CSoundScene( NDb::CMusic *_pAmbient = 0 );

	virtual CSound* Add3DSound( NDb::CSound *pSample, CFuncBase<CVec3> *pPos, STime tStart );
	virtual CSound2D* Add2DSound( NDb::CSound *pSample );
	virtual CSoundEffect* AddEffect( NDb::CSoundEffect *pEff, STime stBeginTime, CFuncBase<STime> *pTime, CFuncBase<CVec3> *pPos, const vector<int> &flags );

	virtual void SetMusic( NDb::CMusic *pMusic );
	virtual void FadeOutMusic();

	virtual void Draw( CTransformStack *pTS );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CSoundScene::CSoundScene( NDb::CMusic *_pAmbient ): 
	bSilence(true), pAmbient(_pAmbient)
{
	NHPTimer::GetTime(&tStartSilence);
	tStartSilence -= N_MAX_SILENCE * NHPTimer::GetClockRate();
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
void CSoundScene::SetMusic( NDb::CMusic *pDBMusic )
{
	if ( !IsValid( pDBMusic ) )
		return;
	if ( IsValid( pMusic ) && IsValid( pMusic->pMusic ) && IsValid( pMusic->pStream ) 
		//&& (pMusic->pMusic->GetRecordID() == pDBMusic->GetRecordID() || pMusic->pMusic->eType == NDb::MT_COMBAT ) )
		&& (pMusic->pMusic->eType == pDBMusic->eType || pMusic->pMusic->eType == NDb::MT_COMBAT ) )
	{
		NFMSound::CancelFadeOut( pMusic->pStream );
		return;
	}
	CMusic *pM = new CMusic;
	pM->pMusic  = pDBMusic;
	bool bLoop = !(pDBMusic->eType == NDb::MT_AMBIENT);
	pM->pStream = NFMSound::SwitchStream( IsValid( pMusic ) ? pMusic->pStream : 0, pDBMusic->szFileName.c_str(), bLoop );
	pMusic = pM;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundScene::FadeOutMusic()
{
	if ( IsValid( pMusic ) && pMusic->pMusic->eType != NDb::MT_AMBIENT )
		NFMSound::FadeOut( pMusic->pStream, 30 );
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
					p.bLoop = pSound->pDBSample->bLoop;
					p.position = pSound->pPos->GetValue();
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
	if ( !IsValid( pMusic ) || !NFMSound::IsPlaying( pMusic->pStream ) )
	{
		if ( !bSilence )
		{
			bSilence = true;
			NHPTimer::GetTime( &tStartSilence );
		}
		else
		{
			NHPTimer::STime t = tStartSilence;
			double dt = NHPTimer::GetTimePassed( &t );
			if ( IsValid( pAmbient ) && dt > N_MAX_SILENCE )
			{
				CMusic *pM = new CMusic;
				pM->pMusic = pAmbient;
				bool bLoop = !(pAmbient->eType == NDb::MT_AMBIENT);
				pM->pStream = NFMSound::PlayStream( pAmbient->szFileName.c_str(), bLoop );
				pMusic = pM;
				bSilence = false;
			}
		}
	}
	else
		bSilence = false;
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
ISoundScene* CreateSoundScene( NDb::CMusic *pAmbient )
{
	return new CSoundScene( pAmbient );
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
static void VarSetOutputType( const string &szID, const NGlobal::CValue &sValue, void *pContext )
{
	NFMSound::ESpeakerType eType = NFMSound::SOUND_SM_MONO;

	if ( sValue.GetString() == L"mono" )
		eType = NFMSound::SOUND_SM_MONO;
	else if ( sValue.GetString() == L"stereo" )
		eType = NFMSound::SOUND_SM_STEREO;
	else if ( sValue.GetString() == L"headphone" )
		eType = NFMSound::SOUND_SM_HEADPHONE;
	else if ( sValue.GetString() == L"surround" )
		eType = NFMSound::SOUND_SM_SURROUND;
	else if ( sValue.GetString() == L"quad" )
		eType = NFMSound::SOUND_SM_QUAD;
	else if ( sValue.GetString() == L"5dot1" )
		eType = NFMSound::SOUND_SM_5DOT1;

	NFMSound::SetSpeakerType( eType );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(Sound)
	REGISTER_CMD( "sound_update", CommandSoundUpdate )
	////
	REGISTER_VAR( "sound_mode", 0, 1.0f, true )
	REGISTER_VAR( "sound_sfxvolume", VarSetSfxVolume, 1.0f, true )
	REGISTER_VAR( "sound_musicvolume", VarSetMusicVolume, 1.0f, true )
	REGISTER_VAR( "sound_outputmode", VarSetOutputType, NGlobal::CValue( L"mono" ), true )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NSound;
BASIC_REGISTER_CLASS( ISoundScene )
REGISTER_SAVELOAD_CLASS( 0x03081147, CSound )
REGISTER_SAVELOAD_CLASS( 0x02881171, CSoundScene )
