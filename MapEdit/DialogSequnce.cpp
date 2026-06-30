#include "StdAfx.h"
#include "dbDefs.h"
#include "MapEdit.h"
#include "DialogSequnce.h"
#include "ItemsMgr.h"
#include "ExportAcks.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDialogSeqDB::GetSequence( vector<SDialogSeqEntry> *pSequnce, int nDialogID )
{
	if ( !pSequnce )
	{
		ASSERT(0);
		return;
	}
	string szQuery = "SELECT * FROM DialogSeqs WHERE DialogID=";
	szQuery += IToA( nDialogID );

	HRESULT hr = Open( szQuery );
	if ( FAILED(hr) )
		return;
	const SResTree* pTree = theApp.GetResTree( IDC_ACKINFOS );
	const SResTree* pStrTree = theApp.GetResTree( IDC_STRINGS_TREE );
	if ( !pTree )
		return;
	while ( MoveNext() == S_OK )
	{
		const CPropMap *pProps = pTree->pItemsTree->GetPropList( m_nAckInfoID );
		if ( !pProps )
			continue;
		CPropMap::const_iterator i = pProps->find( "StringID" );
		CPropMap::const_iterator ii = pProps->find( "IntonationID" );
		CPropMap::const_iterator ip = pProps->find( "WhoID" );
		if ( i == pProps->end() || ii == pProps->end() || ip == pProps->end() )
			return;

		SDialogSeqEntry e;
		e.nSeqID = m_nID;
		e.szIntonation = (string)ii->second->GetValue();
		e.nPersID = ip->second->GetValue();
		e.nAckInfoID = m_nAckInfoID;

		const CPropMap *pStrProps = pStrTree->pItemsTree->GetPropList( i->second->GetValue() );
		if ( pStrProps )
		{
			CPropMap::const_iterator istr = pStrProps->find( "String" );
			e.szText = (string)istr->second->GetValue();
			pStrTree->pItemsTree->ReleasePropList( pStrProps );
		}
		pTree->pItemsTree->ReleasePropList( pProps );
		pSequnce->push_back( e );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDialogSeqDB::DeleteEntry( SAckOperation *pRes, int nSeqID )
{
	string szQuery = "SELECT * FROM DialogSeqs WHERE ID=";
	szQuery += IToA( nSeqID );

	HRESULT hr = Open( szQuery );
	if ( FAILED(hr) )
		return false;
	hr = MoveNext();
	if ( FAILED( hr ) )
	{
//		DisplayOLEDBErrorRecords( hr );
		return false;
	}

	static CAckInfoDB aidb;
	if ( !aidb.OpenInfo( m_nAckInfoID ) )
		return false;
	pRes->szIntonation = aidb.m_szIntonation;
	pRes->szText = aidb.GetInfoString();
	pRes->b6Voices = true;
	aidb.GetInfoSound( pRes->szSndFiles );
	aidb.OpenInfo( m_nAckInfoID );
	aidb.DeleteInfo( m_nAckInfoID );

	if ( FAILED( Open( szQuery ) ) )
		return false;
	Delete();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CDialogSeqDB::InsertEntry( int nDialogID, int nAckInfoID )
{
	string szQuery = "SELECT * FROM DialogSeqs WHERE ID=-1";

	HRESULT hr = Open( szQuery );
	if ( FAILED(hr) )
		return -1;
	m_nDialogID = nDialogID;
	m_nAckInfoID = nAckInfoID;
	hr = Insert( 1 );
	if ( FAILED( hr ) )
		return -1;
	if ( MoveNext() == S_OK )
		return m_nID;
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
