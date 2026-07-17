#ifndef __AICHOOSEPLACE_H_
#define __AICHOOSEPLACE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release CAICombatLogic substrate - the place-chooser job (structural port, Approach A).
//
// CAIChoosePlaceJob is the CAIJob that, for each registered CAIAction, walks its bound place source's
// candidate places and selects the best one (per the subclass IsPlaceBetter policy). CAICombatLogic
// runs it as a sub-job; MakeDecision then reads the chosen place per action via GetPlaceForAction.
//
// Reconstructed from the release Game.exe - reconstruction/exports/{placesource.c, vtable_placesource.txt}.
// IAIJob vtable (verified): slot0 DoJob, slot6 Reset, slot7 AddAction, slot8 GetPlaceForAction,
// slot9 IsPlaceBetter [PURE]. CAIChoosePlaceForAttackJob::IsPlaceBetter @0x004762b0;
// CreateAIChoosePlaceForAttackJob @0x00475610 (new CAIChoosePlaceForAttackJob, size 0x48).
//
// WIP for the substrate port - NOT yet in Main.vcxproj. See reconstruction/integration-plan.md.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "aiJob.h"         // CAIJob, IAIJob
#include "aiActionBase.h"  // SPlaceWithAP (complete), CAIAction
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
class CAIAction;
class IAIActionPlaceSource;
////////////////////////////////////////////////////////////////////////////////////////////////////
// IAIChoosePlaceJob - abstract chooser interface (a CAIJob).
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIChoosePlaceJob: public CAIJob
{
public:
	IAIChoosePlaceJob( IAIJob *_pParentJob = 0 ): CAIJob( _pParentJob ) {}
	// retail @0x475ea0: {2 CAIJob} -- the release serializes the CAIJob base through this extra
	// interface level, so the concrete jobs' wire is 2(ChoosePlaceJob).2(IAIChoosePlaceJob).2(CAIJob).
	int operator&( CStructureSaver &f );   // defined in the .cpp
	//
	virtual void Reset() = 0;                                                  // slot6
	virtual void AddAction( CAIAction *pAction, IAIActionPlaceSource *pSrc ) = 0; // slot7
	virtual bool GetPlaceForAction( CAIAction *pAction, SPlaceWithAP *pPlace ) = 0; // slot8
	// True if `pCandidate` is a better place for `pAction` than `pBest` (subclass policy).
	virtual bool IsPlaceBetter( CAIAction *pAction,
		const SPlaceWithAP *pCandidate, const SPlaceWithAP *pBest ) = 0;         // slot9 [PURE]
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIChoosePlaceJob - the concrete chooser (size 0x48). Implements DoJob/Reset/AddAction/
// GetPlaceForAction; leaves IsPlaceBetter pure for the attack/retreat subclasses.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIChoosePlaceJob: public IAIChoosePlaceJob
{
	ZDATA
	ZPARENT( CAIJob );
	// Per-action working state: its bound place source, the prepared candidate places, and the scan cursor.
	struct SPlaceSourceInfo
	{
		ZDATA
		CPtr<IAIActionPlaceSource> pSource;
		vector<SPlaceWithAP>       places;
		bool                       bPlacesPrepared;
		int                        nBestPlace;
		int                        nCurrentPlace;
		ZEND int operator&( CStructureSaver &f );
		SPlaceSourceInfo(): bPlacesPrepared( false ), nBestPlace( -1 ), nCurrentPlace( 0 ) {}
	};
	//
	int  nCurrentAction;
	vector< CPtr<CAIAction> > actions;
	unordered_map< CPtr<CAIAction>, SPlaceSourceInfo, SPtrHash > info;
	int  nAPToReserve;
	ZEND
public:
	int operator&( CStructureSaver &f );   // public: the Attack/Retreat subclasses serialize (CAIChoosePlaceJob*)this
	//
	CAIChoosePlaceJob() {}
	CAIChoosePlaceJob( IAIJob *_pParentJob, int _nAPToReserve );
	//
	virtual void DoJob();                                            // slot0
	// Completion travels through the inherited CAIJob bJobFinished byte, exactly like the release:
	// DoJob Finish()es once every action's places are scanned (@0x004753f0 sets the +0xc byte), Reset
	// ReArm()s it (@0x00475210 clears it), and the inherited CAIJob::IsJobFinished reports it to the
	// job manager. The byte is serialized inside the CAIJob chunk (wire tag 2.2.2.3 of the standalone
	// jobs), so a mid-scan chooser resumes after load. (The former dev-only bChoosingFinished mirror
	// duplicated this state AND leaked into the wire as a bogus 1-byte tag 6 -- removed.)
	virtual void Reset();                                            // slot6
	virtual void AddAction( CAIAction *pAction, IAIActionPlaceSource *pSrc ); // slot7
	virtual bool GetPlaceForAction( CAIAction *pAction, SPlaceWithAP *pPlace ); // slot8
	// IsPlaceBetter remains pure (slot9).
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIChoosePlaceForAttackJob - attack place-comparison policy.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIChoosePlaceForAttackJob: public CAIChoosePlaceJob
{
	OBJECT_BASIC_METHODS( CAIChoosePlaceForAttackJob );
	ZDATA
	ZPARENT( CAIChoosePlaceJob );
	ZEND int operator&( CStructureSaver &f );
public:
	CAIChoosePlaceForAttackJob() {}
	CAIChoosePlaceForAttackJob( IAIJob *_pParentJob, int _nAPToReserve );
	virtual bool IsPlaceBetter( CAIAction *pAction,
		const SPlaceWithAP *pCandidate, const SPlaceWithAP *pBest );             // @0x004762b0
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIChoosePlaceForRetreatJob - retreat place-comparison policy.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIChoosePlaceForRetreatJob: public CAIChoosePlaceJob
{
	OBJECT_BASIC_METHODS( CAIChoosePlaceForRetreatJob );
	ZDATA
	ZPARENT( CAIChoosePlaceJob );
	ZEND int operator&( CStructureSaver &f );
public:
	CAIChoosePlaceForRetreatJob() {}
	CAIChoosePlaceForRetreatJob( IAIJob *_pParentJob, int _nAPToReserve );
	virtual bool IsPlaceBetter( CAIAction *pAction,
		const SPlaceWithAP *pCandidate, const SPlaceWithAP *pBest );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IAIChoosePlaceJob* CreateAIChoosePlaceForAttackJob( IAIJob *pParentJob, int nAPToReserve );  // @0x00475610
IAIChoosePlaceJob* CreateAIChoosePlaceForRetreatJob( IAIJob *pParentJob );                   // @0x00475680
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __AICHOOSEPLACE_H_
