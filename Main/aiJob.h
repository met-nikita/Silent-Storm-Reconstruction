#ifndef __AIJOB_H_
#define __AIJOB_H_

namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// IAIJob
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIJob: public virtual CObjectBase   // virtual base: lets CAICombatLogic share one CObjectBase
{                                          // across its CAILogic + CAIJob bases (release diamond @0x00433340)

public:
	virtual void DoJob() = 0; // perform one iteration
	virtual bool IsIdleJob() = 0; // job not finished, but nothing to do right now
	virtual bool IsJobFinished() = 0; // job finished
	virtual IAIJob *GetParentJob() = 0; // the job that launched this one
	virtual bool IsHighestPriority() = 0; // return true if all other jobs must not run until these finish
	//
	virtual void OnJobFinished() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CIJob
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIJob: public IAIJob
{
	ZDATA
	CPtr<IAIJob> pParentJob;
	bool bJobFinished;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pParentJob); f.Add(3,&bJobFinished); return 0; }
	//
	CAIJob( IAIJob *_pParentJob = 0 ): pParentJob(_pParentJob), bJobFinished( false ) {}
	//
	virtual void DoJob() = 0;
	virtual bool IsIdleJob() { return false; }
	virtual bool IsJobFinished() { return bJobFinished; }
	virtual IAIJob *GetParentJob() { return pParentJob; }
	virtual bool IsHighestPriority() { return false; }
	//
	virtual void OnJobFinished() {}
	void Finish() { bJobFinished = true; }
	// Re-arm a finished job so it runs again. The job manager (Segment) only DoJob()s a job while
	// !IsJobFinished() and drops it once finished; a logic reused across turns must clear this when it is
	// re-Think()ed, or the manager skips it as already-finished (release CAICombatLogic::Think @0x00432c20
	// zeroes this same field).
	void ReArm() { bJobFinished = false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// IAIJobManager
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIJobManager: public CObjectBase
{
public:
	virtual void Segment() = 0;
	virtual void Add( IAIJob *pAIJob ) = 0; // add a job
	virtual void Remove( IAIJob *pAIJob ) = 0; // remove a job
	virtual void RemoveDelayed( IAIJob *pAIJob ) = 0; // job will be removed, but not immediately
	virtual void WaitForJob( IAIJob *pAnticipantJob, 
		IAIJob *pExpectedJob ) = 0; // pAnticipantJob will not run until pExpectedJob finishes
	virtual void Resume( IAIJob *pAnticipantJob ) = 0; // cancel the waiting mode
	// retail IAIJobManager vtbl+0x28 (CAIJobManager::HasPassCalcerJobs @0x58b20): is any highest-priority
	// (pass-calculator / pathfinding) job pending? CAICommander::GenerateCommand @0x353d0 gates on this so it
	// does NOT pull a command / retire a finished logic / run the round-robin while a unit's path is still
	// being computed on the job manager.
	virtual bool HasPassCalcerJobs() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIJobManager *CreateAIJobManager();
////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif
