#include "StdAfx.h"
#include "UIControls.h"
#include "UIContainer.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
extern SDBConnection dbConnection;
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIContainer::Init( int _nUIContainerID )
{
	nUIContainerID = _nUIContainerID;

	CPtr<CUIControl> pDB = new CUIControl;
	pDB->SetConnection( &dbConnection );

	string szQuery = string( "SELECT * FROM " ) + UICONTROLS_TBL + " WHERE UIContainerID=" + IToA( nUIContainerID );
	HRESULT hr = pDB->Open( szQuery );
	if ( FAILED( hr ) )
		return false;
	while ( S_OK == pDB->MoveNext() )
	{
		CPtr<CUIControl> pCtrl = new CUIControl;
		pCtrl->CopyAccessor( pDB );
		pCtrl->Setup( nUIContainerID );
		controls.push_back( pCtrl );
	}
	pDB->Close();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUIControl* CUIContainer::AddControl( NDb::EUIControl type, const CRect &r )
{
	CPtr<CUIControl> pNewCtrl = new CUIControl;
	pNewCtrl->SetConnection( &dbConnection );
	string szQuery = string( "SELECT * FROM " ) + UICONTROLS_TBL + " WHERE ID=-1";
	HRESULT hr = pNewCtrl->Open( szQuery );
	if ( FAILED( hr ) )
		return false;
	//
	pNewCtrl->m_nType = type;
//	pNewCtrl->Setup( nUIContainerID );
//	pNewCtrl->SetRect( r, false );
	pNewCtrl->m_bVisible = true;
	if ( type == NDb::UI_IMAGE )
	{
		pNewCtrl->m_bTransparent = true;
		pNewCtrl->m_nColor = 0xffffffff;
	}
	pNewCtrl->Setup( nUIContainerID );
	pNewCtrl->SetRect( r, false ); 
	hr = pNewCtrl->Insert( type, true );
  if ( FAILED( hr ) )
  {
    DisplayOLEDBErrorRecords( hr );
    return false;
  }
	pNewCtrl->MoveNext(); // заполняем поле m_nID
	pNewCtrl->SetID( pNewCtrl->m_nID );
	CUIControlAccessor copy = *pNewCtrl;
	pNewCtrl->Close();
	*static_cast<CUIControlAccessor*>( pNewCtrl ) = copy;
	controls.push_back( pNewCtrl );
	return pNewCtrl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIContainer::RemoveControl( CUIControl *pCtrl )
{
	if ( !IsValid( pCtrl ) )
		return false;
	// ищем этот контрол в списке
	vector< CPtr<CUIControl> >::iterator it;
	for ( it = controls.begin(); it != controls.end(); ++it )
		if ( (*it)->GetID() == pCtrl->GetID() )
			break;
	if ( it == controls.end() )
		return false;
	//
	if ( !pCtrl->Open() )
		return false;
	if ( FAILED( pCtrl->MoveNext() ) )
		return false;
	HRESULT hr = pCtrl->Delete();
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	controls.erase( it );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
