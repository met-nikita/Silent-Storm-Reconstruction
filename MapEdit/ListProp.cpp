#include "StdAfx.h"
#include "dbDefs.h"
#include "ListProp.h"
#include "CtrlObjectInspector.h"
#include "MapEdit.h"
#include "ItemsMgr.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
static CItemListDBCmd dbListPropery;
////////////////////////////////////////////////////////////////////////////////////////////////////
void InitParticleInstancesDB( SDBConnection *pConnection )
{
	dbListPropery.SetConnection( pConnection );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CInstancesList::CInstancesList( const string &szName, int nPropertyID )
: CListProp( szName, nPropertyID, CVariant::VT_INT, DT_RELLIST, false ), nEffectID(-1)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInstancesList::SetValue( const CVariant &value, bool bModified ) const
{
	const_cast<CInstancesList*>(this)->nEffectID = value;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInstancesList::GetValues( vector<CVariant> *pVals ) const
{
	ASSERT( pVals );
	string szTbl = GetInstancesTable();
	if ( szTbl == "" )
		return false;
	string szQuery = "SELECT ID, EffectID FROM ";
	szQuery += szTbl;
	szQuery += " WHERE EffectID = ";
	szQuery += IToA( nEffectID );
	HRESULT hr = dbListPropery.Open( szQuery );
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
	pVals->clear();
	while ( dbListPropery.MoveNext() == S_OK )
	{
		pVals->push_back( dbListPropery.m_nID );
	}
	dbListPropery.Close();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string CInstancesList::GetInstancesTable() const
{
	int nRelation = GetRelation();
	const SResTree *pTree = theApp.GetResTree( nRelation );
	if ( !pTree )
		return "";
	return pTree->pItemsTree->GetItemsTable();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInstancesList::SetParticleEffectID( const CVariant &particle, int nEffect ) const
{
	string szTbl = GetInstancesTable();
	if ( szTbl == "" )
		return false;
	string szQuery = "SELECT ID, EffectID FROM ";
	szQuery += szTbl;
	szQuery += " WHERE ID = " + (string)particle;
	HRESULT hr = dbListPropery.Open( szQuery );
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
	hr = dbListPropery.MoveNext();
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
	dbListPropery.m_nContainerID = nEffect;
	hr = dbListPropery.SetData( 1 );
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
	dbListPropery.Close();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInstancesList::AddValue( CVariant val ) const
{
	return SetParticleEffectID( val, nEffectID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInstancesList::RemoveValue( CVariant val ) const
{
	return SetParticleEffectID( val, -1 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CProp* CInstancesList::Clone() const
{
	CInstancesList *pClone = new CInstancesList;
	*pClone = *this;
	return pClone;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInstancesList::SetInfo( int nItemsTable, int nContainerItemID )
{
	info.nItemsTable = nItemsTable;
	info.nContainerItemID = nContainerItemID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInstancesList::Copy( CListProp *_pSrc )
{
	CInstancesList *pSrc = dynamic_cast<CInstancesList*>( _pSrc );
	if ( !pSrc || pSrc->GetRelation() != GetRelation() )
		return;
	int nRelation = GetRelation();
	const SResTree *pTree = theApp.GetResTree( GetRelation() );
	if ( !pTree )
		return;
	//
	vector<CVariant> vals;
	if ( !pSrc->GetValues( &vals ) )
		return;
	//
	for ( int i = 0; i < vals.size(); ++i )
	{
		const CPropMap *pSrcProps = pTree->pItemsTree->GetPropList( vals[i] );
		if ( !pSrcProps )
			continue;
		int nID = pTree->pItemsTree->AddItem( -1, 0, pTree->pItemsTree->GetItemName( vals[i] ) );
		if ( nID < 0 )
			break;
		// копируем все св-ва объекта
		const CPropMap *pDstProps = pTree->pItemsTree->GetPropList( nID );
		if ( pDstProps )
		{
			if ( pSrcProps->size() > 0 )
			{
				for ( CPropMap::const_iterator it = pSrcProps->begin(); it != pSrcProps->end(); ++it )
				{
					CPropMap::const_iterator itd = pDstProps->find( it->first );
					if ( itd == pDstProps->end() )
						continue;
					itd->second->SetValue( it->second->GetValue(), false ); // не записываем в базу каждое св-во в отдельности
				}
				// а теперь записываем всю PropMap в базу
				if ( !pTree->pItemsTree->SetItemProps( nID, pDstProps ) )
					break;
			}
			//
			pTree->pItemsTree->ReleasePropList( pDstProps );
		}
		AddValue( nID );
		pTree->pItemsTree->ReleasePropList( pSrcProps );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BASIC_REGISTER_CLASS(CListProp);