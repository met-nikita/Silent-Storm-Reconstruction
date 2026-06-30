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
	virtual void DoJob() = 0; // ��������� ��������
	virtual bool IsIdleJob() = 0; // ������ �� ���������, �� ������ ���� ������
	virtual bool IsJobFinished() = 0; // ������ ���������
	virtual IAIJob *GetParentJob() = 0; // ������ ����������� ������
	virtual bool IsHighestPriority() = 0; // ������� true, ���� ��� ��������� ������ �� ������ ����������� �� ��������� ����
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
	virtual void Add( IAIJob *pAIJob ) = 0; // �������� ������
	virtual void Remove( IAIJob *pAIJob ) = 0; // ������� ������
	virtual void RemoveDelayed( IAIJob *pAIJob ) = 0; // ������ ����� ������� �� �����
	virtual void WaitForJob( IAIJob *pAnticipantJob, 
		IAIJob *pExpectedJob ) = 0; //  pAnticipantJob �� ����� ����������� �� ���������� pExpectedJob
	virtual void Resume( IAIJob *pAnticipantJob ) = 0; // �������� ����� ��������
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIJobManager *CreateAIJobManager();
////////////////////////////////////////////////////////////////////////////////////////////////////
}

#endif
