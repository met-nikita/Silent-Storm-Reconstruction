#ifndef _FMOD_H_
#define _FMOD_H_
//
// Minimal clean-room FMOD 3 interface for the Silent Storm engine.
//

#ifdef __cplusplus
extern "C" {
#endif

#define FMOD_VERSION 3.70f

// Opaque handles -- the engine only ever passes these around as pointers.
typedef struct FSOUND_SAMPLE  FSOUND_SAMPLE;
typedef struct FSOUND_STREAM  FSOUND_STREAM;
typedef struct FSOUND_DSPUNIT FSOUND_DSPUNIT;

// FSOUND_MODE bit flags (FSOUND_Sample_Load / FSOUND_Stream_Open / *_SetMode).
#define FSOUND_LOOP_OFF      0x00000001
#define FSOUND_LOOP_NORMAL   0x00000002
#define FSOUND_HW3D          0x00001000
#define FSOUND_2D            0x00002000
#define FSOUND_LOADMEMORY    0x00008000

// Special channel / sample-slot indices.
#define FSOUND_FREE          (-1)
#define FSOUND_UNMANAGED     (-2)
#define FSOUND_STEREOPAN     (-1)

// Driver capability bits (FSOUND_GetDriverCaps).
#define FSOUND_CAPS_HARDWARE 0x00000001
#define FSOUND_CAPS_EAX2     0x00000002
#define FSOUND_CAPS_EAX3     0x00000010

enum FSOUND_OUTPUTTYPES
{
	FSOUND_OUTPUT_NOSOUND,
	FSOUND_OUTPUT_WINMM,
	FSOUND_OUTPUT_DSOUND,
	FSOUND_OUTPUT_A3D
};

enum FSOUND_MIXERTYPES
{
	FSOUND_MIXER_AUTODETECT,
	FSOUND_MIXER_BLENDMODE,
	FSOUND_MIXER_MMXP5,
	FSOUND_MIXER_MMXP6,
	FSOUND_MIXER_QUALITY_AUTODETECT,
	FSOUND_MIXER_QUALITY_FPU,
	FSOUND_MIXER_QUALITY_MMXP5,
	FSOUND_MIXER_QUALITY_MMXP6
};

enum FSOUND_SPEAKERMODES
{
	FSOUND_SPEAKERMODE_DOLBYDIGITAL,
	FSOUND_SPEAKERMODE_HEADPHONES,
	FSOUND_SPEAKERMODE_MONO,
	FSOUND_SPEAKERMODE_QUAD,
	FSOUND_SPEAKERMODE_STEREO,
	FSOUND_SPEAKERMODE_SURROUND
};

typedef signed char (__stdcall *FSOUND_STREAMCALLBACK)( FSOUND_STREAM *stream, void *buff, int len, int param );

// ---- system ----
signed char    __stdcall FSOUND_SetOutput( int outputtype );
signed char    __stdcall FSOUND_SetDriver( int driver );
signed char    __stdcall FSOUND_SetHWND( void *hwnd );
signed char    __stdcall FSOUND_Init( int mixrate, int maxsoftwarechannels, unsigned int flags );
void           __stdcall FSOUND_Close( void );
void           __stdcall FSOUND_Update( void );

int            __stdcall FSOUND_GetNumDrivers( void );
const char *   __stdcall FSOUND_GetDriverName( int id );
signed char    __stdcall FSOUND_GetDriverCaps( int id, unsigned int *caps );
float          __stdcall FSOUND_GetVersion( void );
int            __stdcall FSOUND_GetMixer( void );
void *         __stdcall FSOUND_GetOutputHandle( void );
int            __stdcall FSOUND_GetError( void );

void           __stdcall FSOUND_SetSFXMasterVolume( int volume );
signed char    __stdcall FSOUND_SetSpeakerMode( unsigned int speakermode );

// ---- samples ----
FSOUND_SAMPLE *__stdcall FSOUND_Sample_Load( int index, const char *name_or_data, unsigned int mode, int offset, int length );
void           __stdcall FSOUND_Sample_Free( FSOUND_SAMPLE *sptr );
unsigned int   __stdcall FSOUND_Sample_GetLength( FSOUND_SAMPLE *sptr );
signed char    __stdcall FSOUND_Sample_SetMode( FSOUND_SAMPLE *sptr, unsigned int mode );
signed char    __stdcall FSOUND_Sample_SetDefaults( FSOUND_SAMPLE *sptr, int deffreq, int defvol, int defpan, int defpri );
signed char    __stdcall FSOUND_Sample_SetMinMaxDistance( FSOUND_SAMPLE *sptr, float min, float max );

// ---- channels ----
int            __stdcall FSOUND_PlaySound( int channel, FSOUND_SAMPLE *sptr );
int            __stdcall FSOUND_PlaySoundEx( int channel, FSOUND_SAMPLE *sptr, FSOUND_DSPUNIT *dsp, signed char startpaused );
signed char    __stdcall FSOUND_StopSound( int channel );
signed char    __stdcall FSOUND_IsPlaying( int channel );
signed char    __stdcall FSOUND_SetVolume( int channel, int vol );
signed char    __stdcall FSOUND_SetVolumeAbsolute( int channel, int vol );
int            __stdcall FSOUND_GetVolume( int channel );
signed char    __stdcall FSOUND_SetPaused( int channel, signed char paused );
signed char    __stdcall FSOUND_SetPan( int channel, int pan );
unsigned int   __stdcall FSOUND_GetCurrentPosition( int channel );
unsigned int   __stdcall FSOUND_GetLoopMode( int channel );
int            __stdcall FSOUND_GetPriority( int channel );

// ---- 3D ----
signed char    __stdcall FSOUND_3D_SetAttributes( int channel, const float *pos, const float *vel );
void           __stdcall FSOUND_3D_Listener_SetAttributes( const float *pos, const float *vel, float fx, float fy, float fz, float tx, float ty, float tz );

// ---- streams ----
FSOUND_STREAM *__stdcall FSOUND_Stream_Open( const char *name_or_data, unsigned int mode, int offset, int length );
signed char    __stdcall FSOUND_Stream_Close( FSOUND_STREAM *stream );
int            __stdcall FSOUND_Stream_Play( int channel, FSOUND_STREAM *stream );
signed char    __stdcall FSOUND_Stream_Stop( FSOUND_STREAM *stream );
signed char    __stdcall FSOUND_Stream_SetTime( FSOUND_STREAM *stream, int ms );
int            __stdcall FSOUND_Stream_GetTime( FSOUND_STREAM *stream );
signed char    __stdcall FSOUND_Stream_SetPosition( FSOUND_STREAM *stream, unsigned int position );
signed char    __stdcall FSOUND_Stream_SetSyncCallback( FSOUND_STREAM *stream, FSOUND_STREAMCALLBACK callback, int userdata );

#ifdef __cplusplus
}
#endif

#endif
