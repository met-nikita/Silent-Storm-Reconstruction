#ifndef __AICOMBATLOG_H_
#define __AICOMBATLOG_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release CAICombatLogic substrate - the combat action log container CAILog (structural port, Approach A).
//
// CAILog is the release's concrete combat log: an ordered list<CObj<IAILogRecord>> of action records
// (the existing dev AILog.h records - IAILogRecord/CAILogRecord/CAILogPosition/CAILogShot/...). A logic
// appends records with operator<< (which commits them), and the commander harvests the resulting world
// commands with GetWorldCommands. This REUSES the dev AILog.h records rather than re-declaring them;
// CreateAILog() replaces the predecessor's IAILogContainer/CreateAILogContainer.
//
// Reconstructed from the release Game.exe - reconstruction/exports/cailog.c. CAILog size 0x10:
// [CObjectBase 0x0..0xb][records list head @0xc]. operator<< appends + record->Commit() (vtbl+0x10);
// GetWorldCommands fans out to record->GetCommands() (vtbl+0x14, named GetWorldCommands in the release).
//
// WIP for the substrate port - NOT yet in Main.vcxproj.
// BUILD-SETTLE RECONCILIATION: the release record command-producer takes list<CPtr<NWorld::CCmd>>; the
// dev IAILogRecord::GetCommands takes list<CPtr<NWorld::CCommand>>. Confirm CCmd vs CCommand and align.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "AILog.h"          // IAILogRecord, CAILogRecord + the ~20 dev records (reused)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld { class CCommand; }
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAILog - ordered list of committed action records; the release combat log container.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAILog: public CObjectBase
{
	OBJECT_BASIC_METHODS( CAILog );
	ZDATA
	list< CObj<IAILogRecord> > records;     // +0x0c  the committed records, in order
public:
	ZEND int operator&( CStructureSaver &f );   // tag2 records
	//
	CAILog() {}
	//
	void operator<<( IAILogRecord *pRecord );   // append + commit the record
	// Append the world command(s) of every record, in order (release name; delegates to GetCommands).
	void GetWorldCommands( list< CPtr<NWorld::CCommand> > *pOut );
	void Clear();
	bool IsEmpty();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAILog* CreateAILog();      // @0x0045b660
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __AICOMBATLOG_H_
