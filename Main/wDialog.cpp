#include "stdafx.h"
//
#include "..\DBFormat\DataAck.h"
#include "..\DBFormat\DataRPG.h"
#include "rpgUnit.h"
#include "rpgGlobal.h"
#include "rpgGame.h"
#include "rpgUnitMission.h"
#include "wMain.h"
#include "wUICommands.h"
#include "wInterface.h"
#include "wUnitServer.h"
#include "wDialog.h"
//
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool CompateDialogSeqs( const NDb::CDBDialogSeq *pLeft, const NDb::CDBDialogSeq *pRight )
{
   return pLeft->GetRecordID() < pRight->GetRecordID();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void GetDialogSequence( int nDialogID, vector< CDBPtr<NDb::CDBAckInfo> > *pSeq )
{
	ASSERT( pSeq != 0 );
	if ( pSeq == 0 )
		return;
	//
	pSeq->clear();
	vector< CDBPtr<NDb::CDBDialogSeq> > tmpRes;
	CDBTable<NDb::CDBDialogSeq> *pTable = NDatabase::GetTable<NDb::CDBDialogSeq>();
	CDBIterator<NDb::CDBDialogSeq> i(*pTable);
	while ( pTable && i.MoveNext() )
	{
		CDBPtr<NDb::CDBDialogSeq> pSeq = i.Get();
		if ( IsValid( pSeq ) && pSeq->nDialogID == nDialogID )
			tmpRes.push_back( pSeq );
	}
	//
	sort( tmpRes.begin(), tmpRes.end(), CompateDialogSeqs );
	for ( vector< CDBPtr<NDb::CDBDialogSeq> >::iterator r = tmpRes.begin(); r != tmpRes.end(); ++ r )
		pSeq->push_back( (*r)->pAckInfo.GetPtr() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static NWorld::CUnit* GetUnitForDialog( int nPersID, CWorld *pWorld, vector< CPtr<NWorld::CUnit> > *pFakeUnits )
{
	NWorld::CUnit *pUnit = 0;
	//
	pUnit = CDynamicCast<NWorld::CUnit>( pWorld->GetUnitServerByPersID( nPersID ) ).GetPtr();
	//
	if ( !IsValid( pUnit ) )
	{
		for ( vector< CPtr<NWorld::CUnit> >::const_iterator i = pFakeUnits->begin(); i != pFakeUnits->end(); ++i )
		{
			if ( (*i)->GetRPG()->GetRPGPers()->GetRecordID() == nPersID )
			{
				pUnit = *i;
				break;
			}
		}
	}
	//
	if ( !IsValid( pUnit ) )
	{
		// create fake unit
		NAI::SPathPlace p( 0, 0, 0 );
		NAI::SUnitPosition pos;
		pos.pos.p = p;
		pos.pos.SetNetwork( pWorld->GetPathNetwork() );
		NRPG::IUnitMission *pRPG = NRPG::CreateUnit( NDb::GetPers( nPersID ) );
		pUnit = new CUnitServer( pWorld, pRPG, pRPG->GetModel(), 0, pos );
		pFakeUnits->push_back( pUnit );
	}
	//
	return pUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void MakeDialogData(  CWorld *pWorld, int nDialogID, 
	vector< CPtr<NWorld::CAckEvent> > *pPhrases,	vector< CObj<NWorld::CUnit> > *pUnits )
{
	ASSERT( IsValid( pWorld ) );
	if ( !IsValid( pWorld ) )
		return;
	//
	vector< CPtr<NWorld::CUnit> > fakeUnits;
	pPhrases->clear();
	pUnits->clear();
	CPtr<NRPG::CGlobalGame> pGame = pWorld->GetGlobalGame();
	CPtr<NRPG::CUnit> pHero = pGame->GetHero();
	int nDialogHeroPersID = pGame->GetDialogHeroPersID();
	//
	vector< CDBPtr<NDb::CDBAckInfo> > sequence;
	GetDialogSequence( nDialogID, &sequence );
	//
	vector< CDBPtr<NDb::CDBAckInfo> >::const_iterator i;
	for ( i = sequence.begin(); i != sequence.end(); ++i )
	{
		int nPersID = (*i)->nRPGPersID;
		if ( nPersID == nDialogHeroPersID )
		{
			if ( !IsValid( pHero ) )
				continue;
			//
			nPersID = pHero->GetPers()->GetRecordID();
		}
		//
		NWorld::CUnit *pUnit = GetUnitForDialog( nPersID, pWorld, &fakeUnits );
		ASSERT( IsValid( pUnit ) );
		if ( IsValid( pUnit ) )
		{
			if ( find( pUnits->begin(), pUnits->end(), pUnit ) == pUnits->end() )
			{
				if ( pUnit->GetRPG() != ( NRPG::IUnitMissionInfo * )( pHero.GetPtr() ) )
					pUnits->push_back( pUnit );
				else
					pUnits->insert( pUnits->begin(), pUnit );
			}
			//
			pPhrases->push_back( new NWorld::CAckEvent( 1, pUnit, (*i).GetPtr() ) );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Build (but do NOT queue) the play-dialog command -- so a script caller can AddUICommandWithID it and
// get back a wait id for WaitForUI(DialogPlay(...)). Returns 0 if the dialog id is invalid.
NWorld::CUICmdPlayDialog* MakePlayDialogCommand( CWorld *pWorld, int nDialogID )
{
	CDBPtr<NDb::CDBDialog> pDBDialog = NDb::GetDBDialog( nDialogID );
	if ( !IsValid( pDBDialog ) )
		return 0;
	vector< CPtr<NWorld::CAckEvent> > phrases;
	vector< CObj<NWorld::CUnit> > units;
	MakeDialogData( pWorld, nDialogID, &phrases, &units );
	return new NWorld::CUICmdPlayDialog( pDBDialog->szCode, units, phrases );
}
void PlayDialog( CWorld *pWorld, int nDialogID )
{
	NWorld::CUICmdPlayDialog *pCmd = MakePlayDialogCommand( pWorld, nDialogID );
	if ( pCmd )
		pWorld->AddUICommand( pCmd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void PlayDialogAsAcks( CWorld *pWorld, int nDialogID )
{
	vector< CPtr<NWorld::CAckEvent> > phrases;
	vector< CObj<NWorld::CUnit> > units;
	MakeDialogData( pWorld, nDialogID, &phrases, &units );
	phrases.resize( min( NDb::N_ACKINFO_MAX_COUNT, phrases.size() ) );
	pWorld->AddUICommand( new NWorld::CUICmdPlayAck( phrases ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
