#include "stdafx.h"
#include "..\Misc\HPTimer.h"
#include "aiJob.h"
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const float F_AI_TIME_TO_THINK = 0.010f;
const float F_AI_MAX_OVERRUN_TIME = 0.200f;
////////////////////////////////////////////////////////////////////////////////////////////////////
// IAIJobManager
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIJobManager: public IAIJobManager
{
	typedef unordered_map< CPtr<IAIJob>, list< CPtr<IAIJob> >, SPtrHash > DependencesHash;
	//
	OBJECT_BASIC_METHODS(CAIJobManager);
	ZDATA
	float fTime;
	float fOverrunTime;
	int nCurrentJob;
	vector< CPtr<IAIJob> > jobs;
	list< CPtr<IAIJob> > jobsToRemove;
	DependencesHash Dependences;
	int nHighestPriJobs;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&fTime); f.Add(3,&fOverrunTime); f.Add(4,&nCurrentJob); f.Add(5,&jobs); f.Add(6,&jobsToRemove); f.Add(7,&Dependences); f.Add(8,&nHighestPriJobs); return 0; }
	//
	bool CanSkip();
	void Refresh();
	bool IsWaiting( IAIJob *pAIJob );
	void RemoveDependences( IAIJob *pAIJob );
public:
	//	
	CAIJobManager() {}
	CAIJobManager( float _fTime );
	// IAIJobManager
	virtual void Segment();
	virtual void Add( IAIJob *pAIJob );
	virtual void Remove( IAIJob *pAIJob );
	virtual void RemoveDelayed( IAIJob *pAIJob );
	virtual void WaitForJob( IAIJob *pAnticipantJob, IAIJob *pExpectedJob );
	virtual void Resume( IAIJob *pAnticipantJob );
	// @0x58b20: `return 0 < nHighestPriJobs;` -- any highest-priority (pass-calc / pathfinding) job pending.
	virtual bool HasPassCalcerJobs() { return nHighestPriJobs > 0; }
	virtual void DebugOutput();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIJobManager::CAIJobManager( float _fTime ) :
	fTime( _fTime ), fOverrunTime( 0 ), nCurrentJob( 0 ), nHighestPriJobs(0)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIJobManager::DebugOutput()
{
	OutputDebugString( "[AI JOB MANAGER] {\n" );
	DebugTrace( "Job manager has %d jobs\n", jobs.size() );
	DebugTrace( " %d are highest priority\n", nHighestPriJobs );
	OutputDebugString( "[AI JOB MANAGER] }\n" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIJobManager::Add( IAIJob *pAIJob )
{
	ASSERT( IsValid( pAIJob ) );
	if ( IsValid( pAIJob ) )
	{
		jobs.push_back( pAIJob );
		if ( pAIJob->IsHighestPriority() )
		{
			++nHighestPriJobs;
			ASSERT( nHighestPriJobs <= jobs.size() );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIJobManager::RemoveDependences( IAIJob *pAIJob )
{
	ASSERT( IsValid( pAIJob ) );
	Dependences.erase( pAIJob );
	//
	for ( DependencesHash::iterator i = Dependences.begin(); i != Dependences.end(); ++i )
		for ( list< CPtr<IAIJob> >::iterator j = i->second.begin(); j != i->second.end(); )
			if ( (*j) == pAIJob )
				j = i->second.erase( j );
			else
				++j;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIJobManager::Refresh()
{
	for ( vector< CPtr<IAIJob> >::iterator i = jobs.begin(); i != jobs.end(); ++i )
	{
		if ( !IsValid( *i ) && find( jobsToRemove.begin(), jobsToRemove.end(), *i ) == jobsToRemove.end() )
			jobsToRemove.push_back( *i );
	}
	// delete old tasks
	for ( list< CPtr<IAIJob> >::iterator i = jobsToRemove.begin(); i != jobsToRemove.end(); ++i )
	{
		vector< CPtr<IAIJob> >::iterator t = find( jobs.begin(), jobs.end(), *i );
		if ( t != jobs.end() )
		{
			RemoveDependences( *t );
			jobs.erase( t );
		}
	}
	// 
	jobsToRemove.clear();
	// update
	nCurrentJob = 0;
	//DebugOutput();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIJobManager::RemoveDelayed( IAIJob *pAIJob )
{
	ASSERT( IsValid( pAIJob ) );
	if ( find( jobs.begin(), jobs.end(), pAIJob ) != jobs.end() )
	{
		jobsToRemove.push_back( pAIJob );
		if ( pAIJob->IsHighestPriority() )
		{
			--nHighestPriJobs;
			ASSERT( nHighestPriJobs >= 0 );
		}
	}
	//
	for ( vector< CPtr<IAIJob> >::iterator i = jobs.begin(); i != jobs.end(); ++i )
		if ( (*i)->GetParentJob() == pAIJob )
			RemoveDelayed( *i );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIJobManager::Remove( IAIJob *pAIJob )
{
	ASSERT( IsValid( pAIJob ) );
	RemoveDelayed( pAIJob );
	Refresh();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIJobManager::CanSkip()
{
	// if no tasks
	if ( jobs.empty() )
		return true;
	// if no one wants to think :)
	for ( int k = 0; k < jobs.size(); ++k )
	{
		CPtr<IAIJob> &pAIJob = jobs[k];
		if ( !pAIJob->IsIdleJob() && !IsWaiting( pAIJob ) )
			return false;
	}
	//
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAIJobManager::IsWaiting( IAIJob *pAIJob )
{
	ASSERT( IsValid( pAIJob ) );
	if ( nHighestPriJobs > 0 && !pAIJob->IsHighestPriority() )
		return true;
	list< CPtr<IAIJob> > &ExpectedJobs = Dependences[pAIJob];
	for ( list< CPtr<IAIJob> >::iterator i = ExpectedJobs.begin(); i != ExpectedJobs.end(); ++i )
	{
		if ( !(*i)->IsJobFinished() )
			return true;
	}
	//
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIJobManager::WaitForJob( IAIJob *pAnticipantJob, IAIJob *pExpectedJob )
{
	ASSERT( IsValid( pAnticipantJob ) );
	ASSERT( IsValid( pExpectedJob ) );
	ASSERT( find( jobs.begin(), jobs.end(), pAnticipantJob ) != jobs.end() );
	ASSERT( find( jobs.begin(), jobs.end(), pExpectedJob ) != jobs.end() );
	//
	Dependences[pAnticipantJob].push_back( pExpectedJob );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIJobManager::Resume( IAIJob *pAnticipantJob )
{
	ASSERT( IsValid( pAnticipantJob ) );
	Dependences[pAnticipantJob].clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIJobManager::Segment()
{
	if ( CanSkip() )
		return;
	//
	NHPTimer::STime tTime, tTmpTime;
	NHPTimer::GetTime( &tTime );
	tTmpTime = tTime;
	//
	while ( !jobs.empty() && NHPTimer::GetTimePassed( &tTmpTime ) < ( fTime - fOverrunTime ) ) 
	{	
		tTmpTime = tTime;
		//
		CPtr<IAIJob> pAIJob = jobs[nCurrentJob];
		if ( !pAIJob->IsJobFinished() && !pAIJob->IsIdleJob() && !IsWaiting( pAIJob ) )
		{
			pAIJob->DoJob();
			if ( pAIJob->IsJobFinished() )
			{
				pAIJob->OnJobFinished();
				RemoveDelayed( pAIJob );
			}
		}
		//
		++nCurrentJob;
		if ( nCurrentJob >= jobs.size() )
			Refresh();
	}
	//
	tTmpTime = tTime;
	fOverrunTime += NHPTimer::GetTimePassed( &tTmpTime ) - fTime;
	fOverrunTime = Min( fOverrunTime, F_AI_MAX_OVERRUN_TIME );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIJobManager *CreateAIJobManager()
{
	return new CAIJobManager( F_AI_TIME_TO_THINK );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NAI;
//

REGISTER_SAVELOAD_CLASS( 0x52442120, CAIJobManager );