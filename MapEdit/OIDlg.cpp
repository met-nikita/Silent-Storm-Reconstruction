// OIDlg.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "OIDlg.h"
#include "ItemsMgr.h"
#include "TreeSelItemDlg.h"
#include "..\DBFormat\DataGeometry.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// COIDlg dialog


COIDlg::COIDlg(CWnd* pParent /*=NULL*/)
	: CDialog(COIDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(COIDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	pPropMap = 0;
	nPropsTable = -1;
}


void COIDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COIDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COIDlg, CDialog)
	//{{AFX_MSG_MAP(COIDlg)
	ON_WM_PAINT()
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// COIDlg message handlers

BOOL COIDlg::OnInitDialog()
{
  CDialog::OnInitDialog();

  
  // Init object inspector
  RECT rect;

  GetClientRect( &rect );
//  GetWindowRect( &rect );
//m_wndOI.CreateEx( WS_EX_STATICEDGE, 0, 0, WS_CHILD | WS_VISIBLE, rect, this, ID_OI );
  m_wndOI.Create( 0, "ObjectInspector", WS_CHILD | WS_VISIBLE, rect, this, ID_OI );
  m_wndOI.SetWindowPos( 0, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE );
  
  return TRUE;  // return TRUE  unless you set the focus to a control
}

void COIDlg::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	// TODO: Add your message handler code here
	
	// Do not call CDialog::OnPaint() for painting messages
}

void COIDlg::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);
	
  RECT r;
  GetClientRect( &r );
  if ( ::IsWindow( m_wndOI.m_hWnd ) )
    m_wndOI.SetWindowPos( 0, 0, 0, r.right, r.bottom, SWP_NOZORDER );
}

void COIDlg::OnCancel()
{
	CWnd *pPrnt = GetParent();
	if ( pPrnt )
		pPrnt->PostMessage( WM_ME_CANCEL );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL COIDlg::PreTranslateMessage( MSG* pMsg )
{
  if ( pMsg->message == WM_USER + 1 && pPropMap )
  {
    string szName = m_wndOI.GetPropertyName( pMsg->wParam );
    CPropMap::const_iterator it = pPropMap->find( szName );
    if ( it == pPropMap->end() )
      return true;
		CProp *pProp = it->second;
		const SResTree *pRelation = theApp.GetResTree( pProp->GetRelation() );
		if ( pRelation )
		{
			theApp.DropItem( nPropsTable, pPropMap, this, pRelation->nTreeID, pMsg->lParam );
			//
			CWnd *pParent = GetParent();
			if ( pParent )
				pParent->PostMessage( WM_USER + 1, pMsg->wParam, pMsg->lParam );
		}
		else
		{
			it->second->SetValue( m_wndOI.GetPropertyValue( pMsg->wParam ) );
			UpdateProperty( pMsg->wParam );
			if ( szName == "PiecesInfo" ) // CRAP
			{
				Sleep(50);
				NDatabase::Refresh<NDb::CAIGeometry>();
			}
		}
    return true;
  }
  return CDialog::PreTranslateMessage( pMsg );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool PropCompare( const CPtr<CProp> &i1, const CPtr<CProp> &i2 )
{
	return i1->GetID() < i2->GetID();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIDlg::UpdatePropList()
{
  if ( !pPropMap )
    return;
  m_wndOI.ClearAll();
  if ( pPropMap->empty() )
    return;
	//
	vector<CPtr<CProp> > vals;
	{
		for ( CPropMap::const_iterator it = pPropMap->begin(); it != pPropMap->end(); ++it )
			vals.push_back( it->second );
		sort( vals.begin(), vals.end(), PropCompare );
	}
	//
  SOwner itemID = (*vals.begin())->GetOwnerID();
  string szGroup = string( "General (ID = {" ) + IToA( itemID.nItemID ) + "," + IToA( itemID.nVariantID ) + "} )";
	m_wndOI.SetItemID( itemID.nItemID );
  m_wndOI.SetGroup( 0, szGroup.c_str() );
	unordered_map<int, int> groups;
	//
  for ( int i = 0; i < vals.size(); ++i )
  {
		const CPtr<CProp> &pProp = vals[i];
		if ( !IsValid( pProp ) )
		{
			ASSERT(0);
			continue;
		}
    const SResTree *pRelation = theApp.GetResTree( pProp->GetRelation() );
    CVariant var;
    GroupID  gid = 0;
    DomenID  nViewType = pProp->GetViewType();
    bool     bReadOnly = pProp->IsReadOnly();

		if ( pProp->GetGroup() != CProp::PROPDEF_GROUP )
			gid = pProp->GetGroup();

		const CVariant propValue = pProp->GetValue();
		if ( pRelation && nViewType != DT_RELLIST )
    {
			if ( pRelation->pItemsTree )
				var = pRelation->pItemsTree->GetItemPath( propValue.GetType() == CVariant::VT_NULL ? -1 : propValue );
			else if ( pRelation->pTreeDlg )
				var = pRelation->pTreeDlg->GetItemPath( pProp->GetRelation(), propValue );
			else
				var = "";
      //nViewType = DT_STR;
			nViewType = DT_RELATION;
//      bReadOnly = true;
      if ( !gid || gid == pProp->GetRelation() )
			{
				gid = pProp->GetGroup();
				m_wndOI.SetGroup( gid, pRelation->szTabName, true );
			}
    }
    else
    {
      var = propValue;
    }
    string szPref = theApp.GetResSrcDir() + pProp->GetPrefix();
		if ( gid != 0 )
		{
			if ( m_wndOI.GetGroupName( gid ) == "" )
				m_wndOI.SetGroup( gid, pProp->GetName() );
		}
		++groups[gid];
		const string szName = pProp->GetName();
		const string szTip  = pProp->GetTip() == "" ? szName : pProp->GetTip();
    m_wndOI.AddPropertiesValue( pProp->GetID(), nViewType, szName, var, gid, szTip, bReadOnly, szPref.c_str(), propValue, false );
		if ( pRelation )
			m_wndOI.SetPropertyRelation( pProp->GetID(), pProp->GetRelation() );
    if ( DT_COMBO == nViewType )
    {
      const vector<string> &v = pProp->GetStrings();
      for ( int i = 0; i < v.size(); ++i )
        m_wndOI.AddPropertyString( pProp->GetID(), v[i] );
    }
		if ( DT_RELLIST == nViewType )
		{
			m_wndOI.SetListProperties( pProp->GetID(), dynamic_cast<CListProp*>( pProp.GetPtr() ) );
		}
	}
	// Сворачиваем группы, в которых большое число элементов
	for ( unordered_map<int, int>::const_iterator it = groups.begin(); it != groups.end(); ++it )
	{
		if ( it->first == 0 )
			continue;
		//if ( it->second > 9 )
		//	m_wndOI.CollapseGroup( it->first );
	}
	m_wndOI.Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIDlg::SetPropMap( int nTableID, const CPropMap *_pPropMap )
{ 
	// если изменим pPropMap до ClearAll, то можем потерять последнее измененное значение
	m_wndOI.ClearAll();
	//
	nPropsTable = nTableID;
  pPropMap = _pPropMap;
  UpdatePropList();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int COIDlg::GetActiveProp( int nGroupID )
{
  return m_wndOI.GetActiveProp( nGroupID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIDlg::UpdateProperty( int nPropID )
{
  if ( !pPropMap )
    return;

  for ( CPropMap::const_iterator it = pPropMap->begin(); it != pPropMap->end(); ++it )
  {
    if ( it->second->GetID() == nPropID )
    {
      const SResTree *pRelation = theApp.GetResTree( it->second->GetRelation() );
      CVariant var, varShadow;
      if ( pRelation )
      {
				varShadow = it->second->GetValue();
				var = pRelation->pItemsTree ? pRelation->pItemsTree->GetItemName( varShadow.GetType() == CVariant::VT_NULL ? -1 : varShadow ) : "";
      }
      else
        var = it->second->GetValue();
      m_wndOI.SetPropertiesValue( nPropID, var, varShadow );
    }
  }
	m_wndOI.Invalidate( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void COIDlg::SetActiveProp( int nPropID )
{
	m_wndOI.SetActiveProp( nPropID );
}
