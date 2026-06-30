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
static void InitHPTimer()
{
	int64 freq, start, fin;
	QueryPerformanceFrequency( (_LARGE_INTEGER*) &freq );
	double fTStart, fTFinish, fPassed;
	STime t;
	for(;;)
	{
		DWORD dwStart = GetTickCount();
		GetTime( &t );
		QueryPerformanceCounter( (_LARGE_INTEGER*) &start );
		Sleep( 100 );
		fPassed = GetTimePassed( &t );
		QueryPerformanceCounter( (_LARGE_INTEGER*) &fin );
		DWORD dwFinish = GetTickCount();

		fTStart = double( start );
		fTFinish = double( fin );
		float fTickTime = ( dwFinish - dwStart ) / 1024.0f;
		float fPCTime = (float)( ( fTFinish - fTStart ) / static_cast<double>( freq ) );
		if ( fabs( fTickTime - fPCTime ) < 0.05f )
			break;
	}
	double fProcFreq = (fPassed) * (static_cast<double>( freq )) / (fTFinish-fTStart);
	fProcFreq1 = 1 / fProcFreq;
	//cout << "freq = " << fpProcFreq / 1000000 <<  "Mhz" << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// это вспомогательная структура для автоматической инициализации HP timer'а
struct SHPTimerInit
{
	SHPTimerInit() { InitHPTimer(); }
};
static SHPTimerInit hptInit;
////////////////////////////////////////////////////////////////////////////////////////////////////
