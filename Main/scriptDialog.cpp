#include "stdafx.h"
//
#include "A5Script.h"
#include "scriptCommon.h"
#include "wMain.h"
#include "wDialog.h"
#include "wUICommands.h"		// NWorld::CUICmdPlayDialog (DialogPlay returns its wait id)
#include "..\DBFormat\DataAck.h"
//
#include "scriptDialog.h"
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( DialogPlay, "sb[true]" )
	CDBPtr<NDb::CDBDialog> pDBDialog = NDb::GetDBDialogByCode( luaParams[ 0 ].s );
	if ( !IsValid( pDBDialog ) )
		return 0;
	//
	// Build the command, queue it WITH an id, and return that id so WaitForUI(DialogPlay(...)) blocks until the
	// dialog ends. CMissionDlgUI carries the id (set at dispatch) and releases it in EndDialog via
	// CCmdInterfaceEvent(nID) -- the retail CScript id-queue (NScript::luaDialogPlay @0x2e64b0, "sb[true]";
	// the bool is HeroOnLeft, see Common.l DialogPlayWithSequence).
	NWorld::CUICmdPlayDialog *pCmd = NWorld::MakePlayDialogCommand( pScript->pWorld, pDBDialog->GetRecordID(), luaParams[ 1 ].b );
	if ( !pCmd )
		return 0;
	return pScript->AddUICommandWithID( pCmd );
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_SCRIPT_COMMAND( DialogPlayAsAcks, "s" )
	CDBPtr<NDb::CDBDialog> pDBDialog = NDb::GetDBDialogByCode( luaParams[ 0 ].s );
	if ( !IsValid( pDBDialog ) )
		return 0;
	//
	NWorld::PlayDialogAsAcks( pScript->pWorld, pDBDialog->GetRecordID() );
	return 0;
END_SCRIPT_COMMAND
////////////////////////////////////////////////////////////////////////////////////////////////////
}