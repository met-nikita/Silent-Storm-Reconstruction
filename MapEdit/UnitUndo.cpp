#include "StdAfx.h"
#include "UnitUndo.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataRPG.h"
#include "..\MapEdit\UnitDB.h"
#include "weInterface.h"
#include "iWysiwyg.h"

CUnitPosDB db;
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitUndo::CUnitUndo( NDb::CUnit *pStart, NDb::CUnit *pEnd, CWysiwygUndo::EUndoAction eAction, int nDbID )
: CDBWysiwygUndo<NDb::CUnit>( eAction, pStart, pEnd, nDbID )
{
	CObjectMgr *pMgr = GetObjectMgr( BT_UNIT );
	if ( pMgr )
		pMgr->MergeWith( &props, GetID() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitUndo::SetPos( NDb::CUnit *p, int nID )
{
	NDb::CUnit *pDB = NDb::GetUnit( nID );
	if ( IsValid( pDB ) )
	{
		pDB->ptPos = p->ptPos;
		pDB->nFloor = p->nFloor;
		pDB->fRotation = p->fRotation;
	}
	return db.SetPos( nID, p->ptPos.x, p->ptPos.y, p->nFloor, p->fRotation );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitUndo::Delete( int nID )
{
	NDb::CUnit *p = NDb::GetUnit( nID );
	if ( db.Delete( nID ) && IsValid( p ) && IsValid( p->pVariant ) )
	{
		for ( int i = 0; i < p->pVariant->pUnits.size(); ++i )
		{
			if ( IsValid( p->pVariant->pUnits[i] ) && p->pVariant->pUnits[i]->GetRecordID() == nID )
			{
				p->pVariant->pUnits[i] = 0;
				p->pVariant = 0;
				return true;
			}
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitUndo::Insert( NDb::CUnit *pU )
{
	if ( !IsValid( pU ) )
	{
		ASSERT(0);
		return false;
	}
	if ( !IsValid( pU->pVariant ) || !IsValid( pU->pMonster ) )
		return false;
	int n = db.Insert( pU->pVariant->GetRecordID(), pU->pMonster->GetRecordID() );
	if ( n < 0 )
		return false;
	SetID( n );
	db.SetPos( GetID(), pU->ptPos.x, pU->ptPos.y, pU->nFloor, pU->fRotation );
	//
	Sleep(10);
	NDatabase::Refresh<NDb::CUnit>();
	//
	CObjectMgr *pMgr = GetObjectMgr( BT_UNIT );
	if ( pMgr )
		pMgr->SetObjectProps( GetID(), &props );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitUndo::Update()
{
	NWorld::IEditorWorld *pW = NMapEditor::GetEditorWorld();
	if ( pW )
		pW->UpdateUnit( abs( GetID() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWysiwygUndo* CreateUnitUndo( CWysiwygUndo::EUndoAction eAction, NDb::CUnit *pStart, NDb::CUnit *pEnd, int nDbID )
{
	return new CUnitUndo( pStart, pEnd, eAction, nDbID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
