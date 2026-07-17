#include "StdAfx.h"
#include "DG.h"
#include "Gfx.h"
#include "GfxBuffers.h"     // NGfx::CTexture, MakeTexture, I2DBufferLock, ETextureUsage, EWrap, EAccess
#include "GPixelFormat.h"   // NGfx::SPixel8888 (::ID)
#include "GBinkPlayer.h"
#include "..\MiscDll\Commands.h"   // NGlobal::GetVar
#include "..\FileIO\Streams.h"     // CFileStream (from-memory open path)
#include "..\FModSound\FMSound.h"  // NFMSound::GetSoundAPI (Bink->DirectSound handoff)
#include <bink.h>
#pragma comment( lib, "binkw32.lib" )
////////////////////////////////////////////////////////////////////////////////////////////////////
//  dwPlayFlags / dwCopyFlags bit constants (recovered from the decomp masks).
////////////////////////////////////////////////////////////////////////////////////////////////////
#define BPF_NONMULT16_FORCE  0x00000001u   // force the slow (BINKCOPYALL) copy path
#define BPF_LOOP             0x00000002u
#define BPF_LOADTOMEMORY     0x00000004u   // preload the whole file, BinkOpen from memory
#define BPF_DIRECTSOUND      0x00000008u   // route Bink audio through DirectSound (elided: FMOD tree)
#define BPF_NOAUTOADVANCE    0x00000010u   // manual stepping (skip the BinkWait advance loop)
#define BCF_NONMULT16        0x80000000u   // OR'd into dwCopyFlags for a non-multiple-of-16 frame

#define BOF_FROMFILE         0x00182005u   // BinkOpen flags, straight from file
#define BOF_FROMMEMORY       0x04180005u   // 0x04000000 == BINKFROMMEMORY | file flags
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBinkVideoPlayer
////////////////////////////////////////////////////////////////////////////////////////////////////
CBinkVideoPlayer::CBinkVideoPlayer():                                // @0xf8180
	hBink( 0 ), bForceUpdate( false ), bStopped( false ),
	dwCopyFlags( 0 ), dwPlayFlags( 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CBinkVideoPlayer::CBinkVideoPlayer( const string &szName, DWORD dwFlags ):   // @0xf7ed0
	hBink( 0 ), bForceUpdate( false ), bStopped( false ),
	dwCopyFlags( 0 ), dwPlayFlags( dwFlags ), szFileName( szName )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CBinkVideoPlayer::~CBinkVideoPlayer()
{
	Stop();                                                          // close the movie / free the buffer
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// OpenBink @0xf7f90 -- open the movie behind this player and apply the SFX volume.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBinkVideoPlayer::OpenBink( const char *pszName )
{
	if ( ( dwPlayFlags & BPF_LOADTOMEMORY ) == 0 )
	{
		hBink = BinkOpen( pszName, BOF_FROMFILE );
	}
	else
	{
		// Preload the player's own file into `buffer`, then open from memory.
		CFileStream sFile;
		sFile.OpenRead( szFileName.c_str() );
		int nLen = sFile.GetSize();
		buffer.resize( nLen );
		if ( nLen > 0 )
			sFile.Read( &buffer[0], nLen );
		hBink = BinkOpen( buffer.empty() ? 0 : (const char*)&buffer[0], BOF_FROMMEMORY );
	}

	if ( hBink != 0 )
	{
		// Apply the global SFX volume to Bink's single audio track (0..1 -> 0..32768).
		float fVolume = NGlobal::GetVar( "sound_sfxvolume" ).GetFloat();
		BinkSetVolume( hBink, 0, Float2Int( fVolume * 32768.0f ) );
	}
	return hBink != 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Play @0xf8200 -- record the loop request, open the movie, post-process copy flags.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBinkVideoPlayer::Play( bool bLoop )
{
	if ( bLoop )
		dwPlayFlags |= BPF_LOOP;
	else
		dwPlayFlags &= ~BPF_LOOP;

	if ( ( dwPlayFlags & BPF_DIRECTSOUND ) != 0 )
	{
		void *pSoundApi = NFMSound::GetSoundAPI();
		if ( pSoundApi != 0 )
			BinkSetSoundSystem( BinkOpenDirectSound, (U32)pSoundApi );
	}

	if ( !OpenBink( szFileName.c_str() ) )
		return;

	// Non-multiple-of-16 frame size => force the slow (full) copy path.
	if ( ( hBink->Width & 0xf ) != 0 || ( hBink->Height & 0xf ) != 0 )
		dwCopyFlags |= BCF_NONMULT16;

	if ( ( dwPlayFlags & BPF_NONMULT16_FORCE ) != 0 )
		dwCopyFlags |= BCF_NONMULT16;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Recalc @0xf8280 -- advance the movie as far as real time allows, then copy the
// current frame into the player's (DG-owned) texture.
//
// The release picks a 16-bit (SPixel1555) texture when NGfx::Is16BitTextures() is set,
// else a 32-bit SPixel8888 texture.  NGfx::Is16BitTextures() now lives in the Gfx
// subsystem (Gfx.cpp, backed by NGfx::b16BitTexturesNow -- default false in this tree,
// whose render path is always 32-bit, cf. NGScene::CScreenshotTexture).  The release's
// dual path is restored faithfully: the 16-bit branch is compiled for parity (it
// instantiates CTextureLock<SPixel1555>) but is never taken at runtime; the 32-bit
// branch stays the proven, runtime-confirmed path.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBinkVideoPlayer::Recalc()
{
	if ( hBink == 0 )
		return;

	// (Re)create the destination texture when the player has none / it is dead.
	if ( !IsValid( pValue ) )
		pValue = NGfx::MakeTexture( GetNextPow2( (int)hBink->Width ), GetNextPow2( (int)hBink->Height ),
		                            1, NGfx::Is16BitTextures() ? NGfx::SPixel1555::ID : NGfx::SPixel8888::ID, NGfx::REGULAR, NGfx::CLAMP );
	if ( !IsValid( pValue ) )
		return;                          // texture create failed; bForceUpdate stays set for next frame

	// Advance the movie as far as real time allows (unless manual-stepping).
	bool bAdvanced = false;
	if ( ( dwPlayFlags & BPF_NOAUTOADVANCE ) == 0 )
	{
		while ( !bStopped && BinkWait( hBink ) == 0 )
		{
			bAdvanced = true;
			BinkDoFrame( hBink );
			BinkNextFrame( hBink );
			// Non-looping movie that reached its last frame -> stop.
			if ( ( dwPlayFlags & BPF_LOOP ) == 0 && hBink->FrameNum == hBink->Frames )
				bStopped = true;
		}
	}

	// A forced redraw decodes the current frame even when we did not advance.
	if ( bForceUpdate && !bAdvanced )
		BinkDoFrame( hBink );

	// Copy the current frame into the texture.  The release re-queries the pixel depth
	// here and ORs bit 0x8 (16-bit) / 0x5 (32-bit) into the copy flags, locking the
	// matching surface format.  b16BitTexturesNow is false in this tree, so the 16-bit
	// branch is parity-only (never taken at runtime); the 32-bit else-branch is the
	// runtime-confirmed path, left byte-for-byte unchanged.
	if ( NGfx::Is16BitTextures() )
	{
		NGfx::CTextureLock<NGfx::SPixel1555> lk( pValue, 0, NGfx::INPLACE );
		BinkCopyToBuffer( hBink, lk.GetBuffer(), lk.GetStride(), hBink->Height, 0, 0,
		                  dwCopyFlags | 8 );
	}
	else
	{
		NGfx::I2DBufferLock *pLock = pValue->Lock( 0, NGfx::INPLACE );
		if ( pLock != 0 )
		{
			BinkCopyToBuffer( hBink, pLock->GetBuffer(), pLock->GetStride(), hBink->Height, 0, 0,
			                  dwCopyFlags | 5 );
			delete pLock;
		}
	}

	Updated();                           // the frame changed -> bump the DG version
	bForceUpdate = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NeedUpdate @0xf7e40 -- true when it is time to decode/show the next frame.
// ORIGINAL BUG (confirmed @0xf7e40 disasm): hBink is passed to BinkWait with no
// null check (the open-movie test the other methods do is omitted here).  In the
// boot flow Set()+Play() run back-to-back with no render in between, so hBink is
// always open by the time the DG refresh reaches this node.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBinkVideoPlayer::NeedUpdate()
{
	return BinkWait( hBink ) == 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Stop @0xf7e90 -- close the movie, free the frame buffer.  Always returns true.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBinkVideoPlayer::Stop()
{
	if ( hBink != 0 )
	{
		BinkClose( hBink );
		hBink = 0;
		vector<BYTE> sEmpty;
		buffer.swap( sEmpty );           // free the preload buffer (release deletes _M_start)
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Pause @0xf7d80
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBinkVideoPlayer::Pause( bool bPause )
{
	if ( hBink == 0 )
		return false;
	return BinkPause( hBink, bPause ) != 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// IsPlaying @0xf7db0 -- a movie is open and has not been stopped.  (This is also the
// frame-access predicate Get/SetCurrentFrame gate on -- vtable slot +0x24.)
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBinkVideoPlayer::IsPlaying()
{
	return hBink != 0 && bStopped == false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetCurrentFrame @0xf7d30 -- the open movie's current (1-based) frame, or -1.
////////////////////////////////////////////////////////////////////////////////////////////////////
int CBinkVideoPlayer::GetCurrentFrame()
{
	if ( !IsPlaying() )
		return -1;
	return (int)hBink->FrameNum;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SetCurrentFrame @0xf7d50 -- seek (only when frame access is valid): BinkGoto,
// bump the DG version, force a redraw.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBinkVideoPlayer::SetCurrentFrame( int nFrame )
{
	if ( !IsPlaying() )
		return;
	BinkGoto( hBink, nFrame, 0 );
	Updated();                           // the decode's +0x10 seek-counter bump == nVersion++
	bForceUpdate = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetLength @0xf7dd0 -- movie duration in ms: Frames*1000 / FrameRate.
////////////////////////////////////////////////////////////////////////////////////////////////////
int CBinkVideoPlayer::GetLength()
{
	if ( hBink != 0 && hBink->FrameRate != 0 )
		return (int)( ( hBink->Frames * 1000u ) / hBink->FrameRate );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetNumFrames @0xf7e00
////////////////////////////////////////////////////////////////////////////////////////////////////
int CBinkVideoPlayer::GetNumFrames()
{
	if ( hBink != 0 )
		return (int)hBink->Frames;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetSize @0xf7e10 -- frame dimensions of the open movie, or {0,0}.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBinkVideoPlayer::GetSize( CTPoint<int> *pSize )
{
	if ( hBink == 0 )
	{
		pSize->x = 0;
		pSize->y = 0;
		return;
	}
	pSize->x = (int)hBink->Width;
	pSize->y = (int)hBink->Height;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CreateVideoPlayer @0xf8110 -- the factory the UI control uses.
////////////////////////////////////////////////////////////////////////////////////////////////////
IVideoPlayer* CreateVideoPlayer( const string &szName, int dwFlags )
{
	return new CBinkVideoPlayer( szName, (DWORD)dwFlags );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NGScene
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0xB3320170, CBinkVideoPlayer );
