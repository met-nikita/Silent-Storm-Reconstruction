#include "StdAfx.h"
#include "HPTimer.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NHPTimer;
static double fProcFreq1 = 1;
////////////////////////////////////////////////////////////////////////////////////////////////////
double NHPTimer::GetSeconds( const NHPTimer::STime &a )
{
	return (static_cast<double>(a)) * fProcFreq1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Time counters
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline void GetCounter( int64 *pTime )
{
	__asm
	{
		rdtsc
		mov esi, pTime
		mov [esi], eax
		mov [esi+4], edx
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
double NHPTimer::GetClockRate()
{
	return 1 / fProcFreq1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void NHPTimer::GetTime( STime *pTime )
{
	GetCounter( pTime );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
double NHPTimer::GetTimePassed( STime *pTime )
{
	STime old(*pTime );
	GetTime( pTime );
	return GetSeconds( *pTime - old );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// release NHPTimer::UpdateHPTimerFrequency @0x3d4170 -- the dev tree had ONLY the one-shot InitHPTimer
// below, so fProcFreq1 was calibrated once at startup and then frozen for the whole process. Retail
// instead keeps a rolling QPC reference window and re-derives the scale forever, which is what keeps
// RDTSC->seconds honest while SpeedStep/turbo move the TSC<->wall ratio under it.
// The maths is the old InitHPTimer's, made RE-ENTRANT: g_fPassed is measured with the CURRENT
// fProcFreq1, so the new scale divides the old one back out (@0x7d424b) before inverting (@0x7d4251).
// Module statics mirror retail's globals: bInited @0x9c9858, tPrev @0x9c9850, dwStart @0x9c9838,
// qpcStart @0x9c9830, qpcFin @0x9c9848, freq @0x9c9818, fPassed @0x9c9840, fTStart @0x9c9828,
// fTFinish @0x9c9820, fProcFreq1 @0x9847d8.
static bool   g_bInited  = false;
static STime  g_tPrev    = 0;
static DWORD  g_dwStart  = 0;
static int64  g_qpcStart = 0;
static int64  g_qpcFin   = 0;
static int64  g_freq     = 0;
static double g_fPassed  = 0;
static double g_fTStart  = 0;
static double g_fTFinish = 0;
////////////////////////////////////////////////////////////////////////////////////////////////////
void NHPTimer::UpdateHPTimerFrequency()
{
	if ( g_bInited )
	{
		STime t = g_tPrev;
		GetTime( &t );                                                    // @0x7d41ab
		g_fPassed = static_cast<double>( t - g_tPrev ) * fProcFreq1;      // @0x7d41cd/@0x7d41d3
		QueryPerformanceCounter( (_LARGE_INTEGER*) &g_qpcFin );           // @0x7d41d9
		// @0x7d41e4: less than 50ms of reference window -- resample below, but do NOT recalibrate.
		DWORD dwPassed = GetTickCount() - g_dwStart;
		if ( dwPassed < 50 )
			return;
		g_fTStart  = static_cast<double>( g_qpcStart );                   // @0x7d41f0
		g_fTFinish = static_cast<double>( g_qpcFin );                     // @0x7d4200
		// @0x7d4218-@0x7d4224: the /1024 tick clock vs the QPC clock, exactly as InitHPTimer's
		// stability test did. (retail's `fild dword` + conditional 2^32 add IS the DWORD->double cast)
		float fTickTime = dwPassed * ( 1.0f / 1024.0f );
		float fPCTime   = (float)( ( g_fTFinish - g_fTStart ) / static_cast<double>( g_freq ) );
		// @0x7d423e: a sample the two clocks disagree on is thrown away, not averaged in.
		if ( fabs( fTickTime - fPCTime ) < 0.05f )
		{
			double fProcFreq = g_fPassed * static_cast<double>( g_freq ) / ( g_fTFinish - g_fTStart );
			fProcFreq1 = 1 / ( fProcFreq / fProcFreq1 );                  // @0x7d424b/@0x7d4251
		}
	}
	else
		QueryPerformanceFrequency( (_LARGE_INTEGER*) &g_freq );           // @0x7d4264

	// @0x7d4270-@0x7d428d: (re-)arm the reference window -- always, on every path.
	g_bInited = true;
	g_dwStart = GetTickCount();
	GetTime( &g_tPrev );
	QueryPerformanceCounter( (_LARGE_INTEGER*) &g_qpcStart );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// release InitHPTimer @0x3d42a0: prime the scale, then keep sampling every 100ms until
// UpdateHPTimerFrequency actually moves fProcFreq1 off its 1.0 seed (@0x7d42a5-@0x7d42dd compares it
// against the 1.0 at [0x8b47f0]). The first call only latches the QPC frequency and arms the window,
// so at least one more is always needed -- the loop is what waits out the 50ms throttle.
static void InitHPTimer()
{
	NHPTimer::UpdateHPTimerFrequency();
	while ( fProcFreq1 == 1.0 )
	{
		Sleep( 100 );
		NHPTimer::UpdateHPTimerFrequency();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// a helper object whose only job is to initialize the HP timer automatically
struct SHPTimerInit
{
	SHPTimerInit() { InitHPTimer(); }
};
static SHPTimerInit hptInit;
////////////////////////////////////////////////////////////////////////////////////////////////////
