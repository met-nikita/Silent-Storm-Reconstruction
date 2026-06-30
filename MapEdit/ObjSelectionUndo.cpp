#include "StdAfx.h"
#include "ObjSelectionUndo.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataObject.h"
#include "..\MapEdit\FinDBCmd.h"
#include "weInterface.h"
#include "iWysiwyg.h"

CFinPosDB db;
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjSelectionUndo::CObjSelectionUndo( NDb::CFinalElement *_pStart, NDb::CFinalElement *_pEnd, CWysiwygUndo::EUndoAction _eAction, int nDbID )
	: CDBWysiwygUndo<NDb::CFinalElement>(_eAction, _pStart, _pEnd, nDbID )
{
	CObjectMgr *pMgr = GetObjectMgr( BT_OBJECT );
	CObjectMgr *pSMgr = GetObjectMgr( (EBrushType)BT_SCALABLEOBJECT );
	CObjectMgr *pEMgr = GetObjectMgr( (EBrushType)BT_EXPLOSION );
	CObjectMgr *pPMgr = GetObjectMgr( (EBrushType)BT_PASSAGEOBJECT );
	CObjectMgr *pWDMgr = GetObjectMgr( (EBrushType)BT_WINDOWDOOR );
	CObjectMgr *pMNMgr = GetObjectMgr( (EBrushType)BT_MINE );
	if ( pMgr )	 pMgr->MergeWith( &props, GetID() );
	if ( pSMgr ) pSMgr->MergeWith( &props, GetID() );
	if ( pEMgr ) pEMgr->MergeWith( &props, GetID() );
	if ( pPMgr ) pPMgr->MergeWith( &props, GetID() );
	if ( pWDMgr ) pWDMgr->MergeWith( &props, GetID() );
	if ( pMNMgr ) pMNMgr->MergeWith( &props, GetID() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CObjSelectionUndo::SetPos( NDb::CFinalElement *p, int nID )
{
	NDb::CFinalElement *pDB = NDb::GetFinalElement( nID );
	if ( IsValid( pDB ) )
	{
		pDB->ptPos = p->ptPos;
		pDB->fDZ = p->fDZ;
		pDB->nFloor = p->nFloor;
		pDB->fRotation = p->fRotation;
	}
	return db.SetPos( nID, p->ptPos, p->fDZ, p->nFloor, p->fRotation );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CObjSelectionUndo::Delete( int nID )
{
	NDb::CFinalElement *pFin = NDb::GetFinalElement( nID );
	if ( db.Delete( nID ) && IsValid( pFin ) && IsValid( pFin->pVariant ) )
	{
		vector< CPtr<NDb::CFinalElement> > &fins = pFin->pVariant->pFinalElements;
		for ( int i = 0; i < fins.size(); ++i )
			if ( IsValid( fins[i] ) && fins[i]->GetRecordID() == nID )
			{
				fins[i] = 0;
				pFin->pVariant = 0;
				return true;
			}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CObjSelectionUndo::Insert( NDb::CFinalElement *pFin )
{
	if ( !IsValid( pFin ) )
	{
		ASSERT(0);
		return false;
	}
	if ( !IsValid( pFin->pVariant ) || !IsValid( pFin->pObject ) )
		return false;
	int n = db.Insert( pFin->pVariant->GetRecordID(), pFin->pObject->GetRecordID() );
	if ( n < 0 )
		return false;
	SetID( n );
	db.SetPos( GetID(), pFin->ptPos, pFin->fDZ, pFin->nFloor, pFin->fRotation );
	db.SetOpen( GetID(), pFin->bOpen );
	db.SetLightmap( GetID(), pFin->bLightmap );
	db.SetScale( GetID(), pFin->ptScale.x, pFin->ptScale.y, pFin->ptScale.z );
	Sleep(10);
	NDatabase::Refresh<NDb::CFinalElement>();
	//
	CObjectMgr *pMgr = GetObjectMgr( BT_OBJECT );
	CObjectMgr *pSMgr = GetObjectMgr( (EBrushType)BT_SCALABLEOBJECT );
	CObjectMgr *pEMgr = GetObjectMgr( (EBrushType)BT_EXPLOSION );
	CObjectMgr *pPMgr = GetObjectMgr( (EBrushType)BT_PASSAGEOBJECT );
	CObjectMgr *pWDMgr = GetObjectMgr( (EBrushType)BT_WINDOWDOOR );
	CObjectMgr *pMNMgr = GetObjectMgr( (EBrushType)BT_MINE );
	if ( pMgr )	pMgr->SetObjectProps( GetID(), &props );
	if ( pSMgr ) pSMgr->SetObjectProps( GetID(), &props );
	if ( pEMgr ) pEMgr->SetObjectProps( GetID(), &props );
	if ( pPMgr ) pPMgr->SetObjectProps( GetID(), &props );
	if ( pWDMgr ) pWDMgr->SetObjectProps( GetID(), &props );
	if ( pMNMgr ) pMNMgr->SetObjectProps( GetID(), &props );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjSelectionUndo::Update()
{
	NWorld::IEditorWorld *pW = NMapEditor::GetEditorWorld();
	if ( pW )
		pW->UpdateObject( abs( GetID() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWysiwygUndo* CreateObjSelectionUndo( CWysiwygUndo::EUndoAction eAction, NDb::CFinalElement *pStart, NDb::CFinalElement *pEnd, int nDbID )
{
	return new CObjSelectionUndo( pStart, pEnd, eAction, nDbID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
