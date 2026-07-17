#ifndef __HPTIMER_H_
#define __HPTIMER_H_
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NHPTimer
{
	typedef int64 STime;
	// release NHPTimer::UpdateHPTimerFrequency @0x3d4170 -- re-derive the RDTSC->seconds scale from a
	// QueryPerformanceCounter reference window. Self-throttled: a call inside 50ms of the window start
	// only samples and returns. Retail's main loop (WinMain @0x9810, @0x40a70b right after StepApp)
	// calls this EVERY FRAME, so the scale tracks the CPU as SpeedStep/turbo move the TSC<->wall ratio.
	void UpdateHPTimerFrequency();
	double GetSeconds( const STime &a );
	// �������� ������� �����
	void GetTime( STime *pTime );
	// �������� �����, ��������� � �������, ����������� � *pTime, ��� ���� � *pTime ����� �������� ������� �����
	double GetTimePassed( STime *pTime );
	// �������� ������� ����������
	double GetClockRate();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif