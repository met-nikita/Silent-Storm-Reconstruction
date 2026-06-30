#include "StdAfx.h"
#include "SubTemplateUndo.h"
#include "..\DBFormat\DataMap.h"
#include "..\MapEdit\RectsDBCmd.h"
#include "weInterface.h"
#include "iWysiwyg.h"

CRectPosDB db;
////////////////////////////////////////////////////////////////////////////////////////////////////
CSubTemplateUndo::CSubTemplateUndo( NDb::CRectangle *_pStart, NDb::CRectangle *_pEnd, CWysiwygUndo::EUndoAction _eAction, int nDbID )
: CDBWysiwygUndo<NDb::CRectangle>(_eAction, _pStart, _pEnd, nDbID )
{
	CObjectMgr *pMgr = GetObjectMgr( BT_SUBTEMPLATE );
	if ( pMgr )
		pMgr->MergeWith( &props, GetID() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSubTemplateUndo::SetPos( NDb::CRectangle *p, int nID )
{
	NDb::CRectangle *pDB = NDb::GetRectangle( nID );
	if ( IsValid( pDB ) )
	{
		pDB->ptCenter = p->ptCenter;
		pDB->nFloor = p->nFloor;
		pDB->fRotation = p->fRotation;
		pDB->fDZ = p->fDZ;
	}
	return db.SetPos( nID, p->ptCenter, p->fDZ, p->nFloor, p->fRotation );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSubTemplateUndo::Delete( int nID )
{
	NDb::CRectangle *p = NDb::GetRectangle( nID );
	if ( db.Delete( nID ) && IsValid( p ) && IsValid( p->pVariant ) )
	{
		for ( int i = 0; i < p->pVariant->rects.size(); ++i )
		{
			if ( IsValid( p->pVariant->rects[i] ) && p->pVariant->rects[i]->GetRecordID() == nID )
			{
				p->pVariant->rects[i] = 0;
				return true;
			}
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSubTemplateUndo::Insert( NDb::CRectangle *pR )
{
	if ( !IsValid( pR ) )
	{
		ASSERT(0);
		return false;
	}
	if ( !IsValid( pR->pVariant ) || !IsValid( pR->pTemplate ) )
		return false;
	int n = db.Insert( pR->pVariant->GetRecordID(), pR->pTemplate->GetRecordID() );
	if ( n < 0 )
		return false;
	SetID( n );
	db.SetPos( GetID(), pR->ptCenter, pR->fDZ, pR->nFloor, pR->fRotation );
	//
	Sleep(10);
	NDatabase::Refresh<NDb::CRectangle>();
	//
	CObjectMgr *pMgr = GetObjectMgr( BT_SUBTEMPLATE );
	if ( pMgr )
		pMgr->SetObjectProps( GetID(), &props );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSubTemplateUndo::Update()
{
	NWorld::IEditorWorld *pW = NMapEditor::GetEditorWorld();
	if ( pW )
		pW->UpdateSubTemplate( NWysiwyg::MakeUserID( BT_SUBTEMPLATE, abs( GetID() ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWysiwygUndo* CreateSubTemplateUndo( CWysiwygUndo::EUndoAction eAction, NDb::CRectangle *pStart, NDb::CRectangle *pEnd, int nDbID )
{
	return new CSubTemplateUndo( pStart, pEnd, eAction, nDbID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
