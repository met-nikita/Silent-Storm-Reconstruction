#ifndef _BINK_H_
#define _BINK_H_
//
// Minimal clean-room Bink interface for the Silent Storm engine (intro/outro and
// in-game movie playback, NGScene::CBinkVideoPlayer).
//

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int U32;
typedef int          S32;

// The Bink movie handle. binkw32.dll allocates and fills it; the engine follows
// the returned pointer and reads only the leading frame-geometry fields below, so
// their offsets must match the runtime. Trailing runtime state is intentionally
// omitted -- the engine never sizes or allocates a BINK itself.
struct BINK
{
	U32 Width;          // 0x00  frame width  (1-based)
	U32 Height;         // 0x04  frame height (1-based)
	U32 Frames;         // 0x08  total frame count
	U32 FrameNum;       // 0x0C  current frame (1-based)
	U32 LastFrameNum;   // 0x10
	U32 FrameRate;      // 0x14  frames-per-second numerator
	U32 FrameRateDiv;   // 0x18  frames-per-second divisor
};
typedef struct BINK *HBINK;

// Sound-system installer signature -- the engine passes BinkOpenDirectSound here.
typedef S32 (__stdcall *BINKSNDSYSOPEN)( U32 param );

HBINK __stdcall BinkOpen( const char *name, U32 flags );
void  __stdcall BinkClose( HBINK bnk );
S32   __stdcall BinkWait( HBINK bnk );
S32   __stdcall BinkDoFrame( HBINK bnk );
void  __stdcall BinkNextFrame( HBINK bnk );
void  __stdcall BinkGoto( HBINK bnk, U32 framenum, S32 flags );
S32   __stdcall BinkPause( HBINK bnk, S32 pause );
void  __stdcall BinkSetVolume( HBINK bnk, U32 trackid, S32 volume );
S32   __stdcall BinkCopyToBuffer( HBINK bnk, void *dest, S32 destpitch, U32 destheight, U32 destx, U32 desty, U32 flags );
S32   __stdcall BinkOpenDirectSound( U32 param );
S32   __stdcall BinkSetSoundSystem( BINKSNDSYSOPEN open, U32 param );

#ifdef __cplusplus
}
#endif

#endif
