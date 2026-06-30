#include "StdAfx.h"
#include "MapEdit.h"
#include "LayerList.h"
#include "..\Main\Buildinginfo.h"
#include "iWysiwyg.h"
#include "..\Main\iMapEditor.h"
#include "MEParams.h"
#include "WaypointDlg.h"
#include "GameFrm.h"
#include "GameView.h"
#include "CameraInfoDlg.h"
#include "dbDefs.h"
#include "ItemsMgr.h"
#include "WysiwygSelection.h"
#include "WysiwygUndo.h"
#include "CtrlObjectInspector.h"
#include "FragmentDialog.h"
#include "FinDBCmd.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataObject.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataCamera.h"
#include "..\Input\Bind.h"
#include "WallSpotDlg.h"
#include "LadderDlg.h"
#include "RectsDBCmd.h"
#include "UnitDB.h"
#include "ObjectMgr.h"
#include "RouteDlg.h"
#include "ChildView.h"
#include "TerrainProp.h"
#include "MainFrm.h"
#include "MaterialEditPage.h"
#include "ObjBrowserDescription.h"
#include "ObjBrowserContainer.h"

extern ERPGItemCamera geRPGInventoryCamera;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLayerList dialog
CLayerList::CLayerList() : invisLayer( -1, 0 )
{
	//{{AFX_DATA_INIT(CLayerList)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	eMode = EM_SELECT;
	ePushedMode = EM_SELECT;
	eMoveMode = MM_XY;
	eActiveMaterial = MSET_FIRST;
	nRotationID = 0;
	bCanEditAnyLayer = true;
	nSpotMaterialID = -1;
	bDirty = false;
	nBrushSize = 0;
	bGridVisible = true;
	bWireSubTemplate = false;
	pPlacementProps = 0;
	ptSelectionCenter = VNULL3;

	for ( int s = 0; s < MSET_SIZE; ++s )
		for ( int m = 0; m < 4; ++m )
		{
			int nMID = theApp.GetProfileInt( "Materials", GetMSetString( (EMaterialSet)s, m ).c_str(), -100 );
			if ( nMID != -100 )
			{
				ASSERT( materials[s].size() == m );
				materials[s].push_back( NBuilding::SRawMaterialApply() );
				materials[s].back().nTMaterialID = nMID;
			}
		}
}

CLayerList::~CLayerList()
{
	for ( list<CLayerCtrl*>::iterator it = layers.begin(); it != layers.end(); ++it )
		if ( (*it)->m_hWnd != NULL )
			(*it)->DestroyWindow();
	internalWnd.DestroyWindow();
	invisLayer.DestroyWindow();
}

CLayers::iterator CLayerList::begin() { return layers.begin(); }
CLayers::iterator CLayerList::end() { return layers.end(); }

BEGIN_MESSAGE_MAP(CLayerList, SECControlBar)
//{{AFX_MSG_MAP(CLayerList)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \fn void CLayerList::AddLayer( CLayerCtrl *pLayer )
Если окно для объекта pLayer еще не создано, то оно создается.
Поэтому в момент вызова объект LayerList должен быть уже создан и иметь корректный HWND.
\param pLayer указатель на добавляемый слой CLayerCtrl
*/
void CLayerList::AddLayer( CLayerCtrl *pLayer )
{
	if ( !pLayer )
		return;
	if ( !IsWindow( pLayer->m_hWnd ) )
		pLayer->Create( IDD_LAYER, &internalWnd );
	else
		pLayer->SetParent( &internalWnd );
	layers.push_back( pLayer );
	SetupScrolls();
	pLayer->Reset();
	pLayer->Activate( false );
	pLayer->ShowWindow( SW_SHOW );
	int bShow = theApp.GetProfileInt( "Layers", IToA( pLayer->GetLayerID() ).c_str(), -10 );
	if ( bShow != -10 )
		pLayer->SetVisible( bShow );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::DeleteAllLayers()
{
	for ( list<CLayerCtrl*>::iterator it = layers.begin(); it != layers.end(); ++it )
		(*it)->ShowWindow( SW_HIDE );
	layers.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::DeleteLayer( int nLayerID )
{
	CLayers::iterator it = FindLayer( nLayerID );
	if ( layers.end() != it )
	{
		(*it)->ShowWindow( SW_HIDE );
		layers.erase( it );
	}
	SetupScrolls();
	CWnd *pPrnt = GetParent();
	if ( pPrnt )
		pPrnt->PostMessage( WM_USER_NOTIFY );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CLayers::iterator CLayerList::FindLayer( int nLayerID )
{
	for ( list<CLayerCtrl*>::iterator it = layers.begin(); it != layers.end(); ++it )
		if ( (*it)->GetLayerID() == nLayerID )
			return it;
	return layers.end();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CLayerCtrl* CLayerList::GetLayer( int nLayerID )
{
	CLayers::iterator i = FindLayer( nLayerID );
	if ( i != layers.end() )
		return (*i);
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLayerList::HasLayer( CLayerCtrl *pLayer )
{
	for ( list<CLayerCtrl*>::iterator it = layers.begin(); it != layers.end(); ++it )
		if ( *it == pLayer )
			return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLayerList::HasLayer( int nLayerID )
{
	for ( list<CLayerCtrl*>::iterator it = layers.begin(); it != layers.end(); ++it )
		if ( (*it)->GetLayerID() == nLayerID )
			return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CLayerList::IsLayerVisible( int nLayerID )
{
	CLayers::iterator i = FindLayer( nLayerID );
	if ( i != layers.end() )
		return (*i)->IsVisible();
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::ArrangeLayers()
{
	CRect r, rbar;
	GetInsideRect( r );
	int nItemWidth = r.Width();
	internalWnd.MoveWindow( &r );
	int nStartY = rlistbar.Height();
	nStartY -= GetScrollPos( SB_VERT );
	listBar.MoveWindow( 0, -GetScrollPos( SB_VERT ), nItemWidth, rlistbar.Height() );
	int i = 0;
	for ( list<CLayerCtrl*>::const_iterator it = layers.begin(); it != layers.end(); ++it, ++i )
	{
		(*it)->MoveWindow( 0, i * nItemHeight + nStartY, nItemWidth, nItemHeight );
		(*it)->SetMiddle();
	}
	if ( !layers.empty() )
	{
		layers.front()->SetFirst();
		layers.back()->SetLast();
	}
	//
	Invalidate( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetupScrolls()
{
	//
	//	if ( r.Width() < nItemWidth )
	//		ShowScrollBar( SB_HORZ );
	//	else
	//		ShowScrollBar( SB_HORZ, FALSE );
	/*
	if ( r.Height() < nItemHeight * layers.size() )
	ShowScrollBar( SB_VERT );
	else
	ShowScrollBar( SB_VERT, FALSE );		
	*/
	CRect r, rbar;
	GetInsideRect( r );
	int nTotalH = nItemHeight * layers.size() + rlistbar.Height();
	if ( r.Height() < nTotalH )
	{
		SCROLLINFO info;

		EnableScrollBarCtrl( SB_VERT );
		info.fMask = SIF_PAGE|SIF_RANGE;
		info.nMin = 0;
		info.nPage = r.Height();
		info.nMax = nTotalH + 1;
		if ( !SetScrollInfo( SB_VERT, &info ) )
			SetScrollRange( SB_VERT, 0, r.Height() + 1 );
		if ( GetScrollPos( SB_VERT ) > r.Height() + 1 )
			SetScrollPos( SB_VERT, 0 );
	}
	else
	{
		EnableScrollBarCtrl( SB_VERT, false );
		SetScrollPos( SB_VERT, 0, false );
	}
	ArrangeLayers();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::Paint( ITemplateView *pView )
{
	if ( bDirty )
	{
		int nTree, nItem, nVar;
		theApp.GetActiveItem( &nTree, &nItem, &nVar );
		theApp.SetActiveVariant( nVar );
		return;
	}
	for ( list<CLayerCtrl*>::reverse_iterator it = layers.rbegin(); it != layers.rend(); ++it )
	{
		if ( !(*it)->IsVisible() )
			continue;
		(*it)->Paint( pView );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CLayerCtrl* CLayerList::GetActiveLayer() const
{
	for ( list<CLayerCtrl*>::const_iterator it = layers.begin(); it != layers.end(); ++it )
		if ( (*it)->IsActive()  )
			return *it;
		return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::ClearSelection()
{
	for ( list<CLayerCtrl*>::iterator it = layers.begin(); it != layers.end(); ++it )
		(*it)->ClearSelection();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::GetLayerOrder( vector< pair<int, bool> > *pOrder )
{
	if ( !pOrder )
		return;
	pOrder->clear();
	for ( list<CLayerCtrl*>::const_iterator it = layers.begin(); it != layers.end(); ++it )
		pOrder->push_back( pair<int, bool>( (*it)->GetLayerID(), (*it)->IsVisible() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetLayerOrder( const vector< pair<int, bool> > &order )
{
	CLayers newlist;
	
	for ( int i = 0; i < order.size(); ++i )
	{
		CLayers::iterator it = FindLayer( order[i].first );
		if ( layers.end() != it )
		{
			(*it)->SetVisible( order[i].second );
			newlist.push_back( *it );
			layers.erase( it );
		}
	}
	newlist.insert( newlist.end(), layers.begin(), layers.end() );
	layers = newlist;
	ArrangeLayers();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::Activate( int nLayerID, bool bActivate )
{
	CLayers::iterator it = FindLayer( nLayerID );
	if ( layers.end() != it )
	{
		for ( list<CLayerCtrl*>::const_iterator i = layers.begin(); i != layers.end(); ++i )
			(*i)->Activate( false );
		(*it)->Activate( bActivate );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLayerList message handlers
int CLayerList::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECControlBar::OnCreate(lpCreateStruct) == -1)
		return -1;
	internalWnd.Create( 0, 0, WS_VISIBLE | WS_CHILD, CRect( 0, 0, 0, 0 ), this, 123 );
	listBar.Create( IDD_LAYER_LIST, &internalWnd );
	listBar.GetClientRect( &rlistbar );
	invisLayer.Create( IDD_LAYER, this );
	invisLayer.ShowWindow( SW_HIDE );
	CRect r;
	invisLayer.GetClientRect( &r );
	nItemHeight = r.Height();
	nItemWidth = r.Width();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::OnSize(UINT nType, int cx, int cy) 
{
	SECControlBar::OnSize(nType, cx, cy);
	
	SetupScrolls();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CLayerList::PreTranslateMessage(MSG* pMsg) 
{
	bool bRet = false;
	WPARAM wParam = 0;

	switch ( pMsg->message )
	{
		case WM_USER_SELECT:
			{
				for ( list<CLayerCtrl*>::const_iterator it = layers.begin(); it != layers.end(); ++it )
				{
					int nID = (*it)->GetLayerID();
					if ( nID == pMsg->wParam )
					{
						(*it)->Activate();
					}
					else
						(*it)->Activate( false );
				}
				for ( int i = 0; i < notifyWnds.size(); ++i )
					notifyWnds[i]->PostMessage( WM_USER_SELECT, pMsg->wParam );
			}
			return TRUE;
		case WM_USER_LAYERUP:
			{
				int nID = pMsg->wParam;
				if ( -1 == nID )
				{
					CLayerCtrl *pLr = GetActiveLayer();
					if ( pLr )
						nID = pLr->GetLayerID();
					else
						return true;
				}
				CLayers::iterator it = FindLayer( nID );
				if ( layers.end() == it || layers.begin() == it )
					return true;
				CLayers::iterator iprev = it;
				--iprev;  // можно сделать, т.к. it != begin()
				swap( *it, *iprev );
				ArrangeLayers();
			}
			bRet = true;
			break;
		case WM_USER_LAYERDOWN:
			{
				int nID = pMsg->wParam;
				if ( -1 == nID )
				{
					CLayerCtrl *pLr = GetActiveLayer();
					if ( pLr )
						nID = pLr->GetLayerID();
					else
						return true;
				}
				CLayers::iterator it = FindLayer( nID );
				if ( layers.end() == it )
					return true;
				CLayers::iterator inext = it;
				++inext;
				if ( inext != layers.end() )
				{
					swap( *it, *inext );
					ArrangeLayers();
				}
			}
			bRet = true;
			break;
		case WM_USER_NOTIFY:
			wParam = pMsg->wParam;
			bRet = true;
			break;
		case WM_USER_EXPORTTERR:
			for ( int i = 0; i < notifyWnds.size(); ++i )
				notifyWnds[i]->PostMessage( WM_USER_EXPORTTERR, pMsg->wParam );
			return true;
		case WM_USER_LINK:
			for ( int i = 0; i < notifyWnds.size(); ++i )
				notifyWnds[i]->PostMessage( WM_USER_LINK, pMsg->wParam );
			return true;
		case WM_USER_FLOORLINK:
			for ( int i = 0; i < notifyWnds.size(); ++i )
				notifyWnds[i]->PostMessage( WM_USER_FLOORLINK );
			return true;
	}
	if ( bRet )
	{
		SendNotify( wParam );
		return true;
	}
	return SECControlBar::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::AddNotifyWnd( CWnd *pWnd )
{
	if ( pWnd )
		notifyWnds.push_back( pWnd );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SendNotify( WPARAM wParam )
{
	for ( int i = 0; i < notifyWnds.size(); ++i )
		notifyWnds[i]->PostMessage( WM_USER_NOTIFY, wParam );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
EEditMode CLayerList::GetMode() const
{
	return eMode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CLayerList::GetActiveLayerID() const
{
	CLayerCtrl *pLayer = GetActiveLayer();
	if ( !pLayer )
		return LID_FLOORS;
	return pLayer->GetLayerID();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CLayerList::GetActiveFloor() const
{
	float fFloor = theApp.GetActiveFloor();
	ELayer nType;
	int nInd;
	NBuilding::GetLayerID( GetActiveLayerID(), &nType, &nInd );
	if ( nType == LID_FLOORS_INTERM || nType == LID_SOLIDS_INTERM )
		fFloor += 0.5f;
	return fFloor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CLayerList::GetActiveRotationID() const
{
	return nRotationID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CLayerList::GetSelectedBrushID( int nLayerID ) const
{
	CLayerCtrl *p = const_cast<CLayerList*>( this )->GetLayer( nLayerID );
	if ( !p )
	{
		unordered_map<int, int>::const_iterator i = lid2brush.find( nLayerID );
		if ( i != lid2brush.end() )
			return i->second;
		return -1;
	}
	return p->GetBrushID();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EMaterialSet CLayerList::GetActiveMaterialSet()
{
	return eActiveMaterial;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLayerList::GetActiveMaterial( vector<NBuilding::SRawMaterialApply> *pMaterials ) const
{
	ASSERT( pMaterials );
	*pMaterials = materials[eActiveMaterial];
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLayerList::CanEditAnyLayer() const
{
	return bCanEditAnyLayer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::GetVisibleLayers( vector<int> *pLayers ) const
{
	ASSERT( pLayers );
	pLayers->clear();
	for ( CLayers::const_iterator i = layers.begin(); i != layers.end(); ++i )
		if ( (*i)->IsVisible() )
			pLayers->push_back( (*i)->GetLayerID() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLayerList::IsCameraReset() const
{
	return theApp.IsCameraReset();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CItemsMgr* CLayerList::GetResourceManager( int nResourceTypeID ) const
{
	const SResTree* pTree = theApp.GetResTree( nResourceTypeID );
	if ( !pTree )
		return 0;
	return pTree->pItemsTree;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CLayerList::GetBrushSize() const
{
	return nBrushSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLayerList::IsGridVisible() const
{
	return bGridVisible;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CLayerList::GetSubTemplateDepth() const
{
	return theApp.GetTemplateMaxDepth();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLayerList::IsWireSubTemplate() const
{
	return bWireSubTemplate;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#define DW_BITS( dw ) ( *reinterpret_cast<float*>( &(dw) ) )
////////////////////////////////////////////////////////////////////////////////////////////////////
float CLayerList::GetParam( int nParamID ) const
{
	unordered_map<int, float>::const_iterator i = meparams.find( nParamID );
	if ( i != meparams.end() )
		return i->second;
	//
	const int nDefault = 0xcbcbcbcb;
	int nVal = theApp.GetProfileInt( "Params", GetMEParamName( nParamID ).c_str(), nDefault );
	float fVal = nDefault == nVal ? GetMEParamDefValue( nParamID ) : DW_BITS( nVal );
	const_cast<unordered_map<int, float>& >(meparams)[nParamID] = fVal;
	return fVal;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CLayerList::GetSelectionCenter() const
{
	return ptSelectionCenter;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetMode( EEditMode mode )
{
	eMode = mode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::PushMode( EEditMode mode )
{
	ePushedMode = eMode;
	eMode = mode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::PopMode()
{
	eMode = ePushedMode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetActiveLayerID( int nID )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetActiveFloor( float fFloor )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetActiveRotationID( int nID )
{
	nRotationID = nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetSelectedBrushID( int nLayerID, int nID )
{
	lid2brush[nLayerID] = nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetMaterial( EMaterialSet set, const vector<NBuilding::SRawMaterialApply> &_materials )
{
	materials[set] = _materials;
	((CMainFrame*)theApp.GetMainWnd())->GetWysiwygBar()->m_pMaterial->SetNames();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetActiveMaterialSet( EMaterialSet set )
{
	eActiveMaterial = set;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::GetMaterial( EMaterialSet set, vector<NBuilding::SRawMaterialApply> *pMaterials ) const
{
	*pMaterials = materials[set];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetCanEditAnyLayer( bool bCan )
{
	bCanEditAnyLayer = bCan;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetVisibleLayers( const vector<int> &layers )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetSpotMaterial( int nMaterialID )
{
	nSpotMaterialID = nMaterialID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::ResetLinkedFloors()
{
	listBar.m_floor_1 = FALSE;
	listBar.m_floor_2 = FALSE;
	listBar.m_floor0 = FALSE;
	listBar.m_floor1 = FALSE;
	listBar.m_floor2 = FALSE;
	listBar.m_floor3 = FALSE;
	listBar.m_floor4 = FALSE;
	listBar.UpdateData( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::GetLinkedFloors( vector<int> *pFloors )
{
	ASSERT( pFloors );

	listBar.UpdateData();
	pFloors->clear();
	if ( listBar.m_floor_1 )
		pFloors->push_back( -1 );
	if ( listBar.m_floor_2 )
		pFloors->push_back( -2 );
	if ( listBar.m_floor0 )
		pFloors->push_back( 0 );
	if ( listBar.m_floor1 )
		pFloors->push_back( 1 );
	if ( listBar.m_floor2 )
		pFloors->push_back( 2 );
	if ( listBar.m_floor3 )
		pFloors->push_back( 3 );
	if ( listBar.m_floor4 )
		pFloors->push_back( 4 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetLinkedFloors( const vector<int> &floors )
{
	ResetLinkedFloors();
	for ( int i = 0; i < floors.size(); ++i )
	{
		switch ( floors[i] )
		{
			case -2:
				listBar.m_floor_2 = TRUE;
				break;
			case -1:
				listBar.m_floor_1 = TRUE;
				break;
			case 0:
				listBar.m_floor0 = TRUE;
				break;
			case 1:
				listBar.m_floor1 = TRUE;
				break;
			case 2:
				listBar.m_floor2 = TRUE;
				break;
			case 3:
				listBar.m_floor3 = TRUE;
				break;
			case 4:
				listBar.m_floor4 = TRUE;
				break;
		}
	}
	listBar.UpdateData( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjProp : public CProp
{
OBJECT_BASIC_METHODS(CObjProp);
private:
	CVariant value;
	EBrushType eBType;
	int nObjectID;
	SMessage msg;
  
public:
	CObjProp() {ASSERT(0);}
  CObjProp( const string &szName, int nID, int nType, int nViewType, CVariant defValue = CVariant(), bool bReadOnly = true )
		: CProp( szName, nID, nType, nViewType, bReadOnly ) {}
  
  const CVariant& GetValue() const { return value; }
	const CVariant GetDefValue() const { return value; }
	CProp* Clone() const { return 0; }
	void SetObject( EBrushType eBrush, int _nObjectID ) { eBType = eBrush; nObjectID = _nObjectID; }
  void SetValue( const CVariant &val, bool bModified = true ) const 
	{
		const_cast<CVariant&>( value ) = val; 
		if ( !bModified )
			return;
		switch ( eBType )
		{
			case BT_WALLSPOT:
				break;
			default:
				break;
		}
		CVec3 pt = GetUserSettings().GetSelectionCenter();
		if ( GetName() == "SelectionX" )
		{
			pt.x = value;
			NInput::PostEvent( "move_selection" );
		}
		else if ( GetName() == "SelectionY" )
		{
			pt.y = value;
			NInput::PostEvent( "move_selection" );
		}
		else if ( GetName() == "SelectionZ" )
		{
			pt.z = value;
			NInput::PostEvent( "move_selection" );
		}
		GetUserSettingsSetup().SetSelectionCenter( pt );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSetTemplateCmd: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CSetTemplateCmd);
	int nTemplateID;
public:
	CSetTemplateCmd( int nTemplate = -1 ):nTemplateID(nTemplate) {}
	virtual void Exec()
	{
		theApp.SetActiveItem( IDC_TEMPLATE_TREE, nTemplateID );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPopInterfaceCmd: public NMainLoop::CInterfaceCommand
{
	OBJECT_NOCOPY_METHODS(CPopInterfaceCmd);
public:
	virtual void Exec()	{	PopInterface();	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool AddObjMgr( EBrushType eMgr, CPropMap *pProps, int nObjectID )
{
	CObjectMgr *pMgr = GetObjectMgr( eMgr );
	if ( !pMgr )
		return false;
	pMgr->MergeWith( pProps, nObjectID );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SendMessage( SMessage msg )
{
	switch ( msg.msg )
	{
		case MSG_EDIT:
			switch ( msg.brush )
			{
				case BT_WAYPOINT:
				{
					CWaypointDlg dlg;

					dlg.SetWaypoint( msg.data );
					dlg.DoModal();
					theApp.GetGameView()->SetFocus();
					theApp.GetGameFrame()->Invalidate();
					break;
				}
				case BT_GEOMETRY:
				{
					int nTree, nItem, nVariant;
					theApp.GetActiveItem( &nTree, &nItem, &nVariant );
					if ( nTree != IDC_TEMPLATE_TREE )
						break;
					NBuilding::SBuildFragment *pF = (NBuilding::SBuildFragment*)msg.data;
					NDb::CTConstructionPart *pTCP = NDb::GetTConstructionPart( pF->nConstructionPartID );
					if ( !pTCP )
						break;
					SRand rand;
					CPtr<NDb::CConstructionPart> pCP = pTCP->CreateConstructionPart( &rand );
					if ( !IsValid( pCP->pObject ) )
						break;
					CFragmentDlg dlg( nVariant, msg.data );

					dlg.DoModal();
					theApp.GetGameView()->SetFocus();
					theApp.GetGameFrame()->Invalidate();
					break;
				}
				case BT_OBJECT:
				{
					NDb::CFinalElement *pFin = NDb::GetFinalElement( msg.data );
					if ( !pFin )
						return;
					CObjectDlg dlg( pFin );
					dlg.DoModal();
					theApp.GetGameView()->SetFocus();
					theApp.GetGameFrame()->Invalidate();
					break;
				}
				case BT_WALLSPOT:
				{
					NBuilding::SProjectedSpot *pSpot = (NBuilding::SProjectedSpot*)msg.data;
					CWallSpotDlg dlg( pSpot );

					dlg.DoModal();
					theApp.GetGameFrame()->SetFocus();
					theApp.GetGameFrame()->Invalidate();
					break;
				}
				case BT_LADDER:
				{
					NBuilding::SLadder *p = (NBuilding::SLadder*)msg.data;
					CLadderDlg dlg( p );

					dlg.DoModal();
					break;
				}
				case BT_SUBTEMPLATE:
					if ( IDOK != ::MessageBox( theApp.GetMainWnd()->m_hWnd, "Do you really want to load the selected map ?", "Editor", MB_OKCANCEL | MB_ICONQUESTION ) )
						break;
					NMainLoop::Command( new CPopInterfaceCmd );
					NMainLoop::Command( new CSetTemplateCmd( msg.data ) );
					break;
				case BT_UNIT:
				{
					NDb::CUnit *pUnit = NDb::GetUnit( msg.data );
					if ( IsValid( pUnit ) )
					{
						CRouteDlg dlg( msg.data );
						dlg.DoModal();
					}
					break;
				}
			}
			break;
		case MSG_SELECT:
			{
				static CPropMap	staticprops;
				staticprops.clear();
				CObjProp *prop = 0;
				int nTree, nItem, nVar;
				theApp.GetActiveItem( &nTree, &nItem, &nVar );
				SOwner owner( -1, -1 );
				if ( IDC_TEMPLATE_TREE == nTree )
				{
					owner = SOwner( nItem, nVar );
					const SResTree *pTree = theApp.GetResTree( IDC_TEMPLATE_TREE );
					if ( pTree )
					{
						if ( pPlacementProps )
							pTree->pItemsTree->ReleasePropList( pPlacementProps );
						pPlacementProps = const_cast<CPropMap*>( pTree->pItemsTree->GetPropList( nItem, nVar ) );
					}
				}

				//CPropMap &props = staticprops;
				//
				if ( msg.data != 0 )
				{
					const vector<NWysiwyg::SSelectedInfo> *pInfoVec = (vector<NWysiwyg::SSelectedInfo>*)msg.data;

					for ( int i = 0; i < pInfoVec->size(); ++i )
					{
						int nGroup = -1;
						bool bCommonProps = true;
						CPropMap props;
						const NWysiwyg::SSelectedInfo *pInfo = &(*pInfoVec)[i];
						switch ( pInfo->eBrushType )
						{
							case BT_GEOMETRY:
								prop = new CObjProp( "ConstructionPartID", 20, CVariant::VT_INT, DT_DEC );
								prop->SetOwner( owner );
								prop->SetValue( pInfo->nBrushID, false );
								prop->SetGroup(  IDC_CONSTRUCTIONPARTS_TREE );
								prop->SetRelation( IDC_CONSTRUCTIONPARTS_TREE );
								props["ConstructionPartID"] = prop;
								nGroup = IDC_CONSTRUCTIONPARTS_TREE;
								break;
							case BT_OBJECT:
								{
									CObjectMgr *pMgr = GetObjectMgr( BT_OBJECT );
									if ( pMgr )
										pMgr->MergeWith( &props, pInfo->nObjectID );
									//
									NDb::CFinalElement *pFin = NDb::GetFinalElement( pInfo->nObjectID );
									// NDb::CObject
									if ( pFin && IsValid( pFin->pObject ) && IsValid( pFin->pObject->pObject ) )
									{
										SRand rnd;
										CPtr<NDb::CObject> pO = pFin->pObject->pObject->CreateObject( &rnd );

										if ( IsValid( pO ) && IsValid( pO->pModels[0] ) && IsValid( pO->pModels[0]->pModel ) 
											&& !IsValid( pO->pModels[0]->pModel->pSkeleton ) )
											AddObjMgr( (EBrushType)BT_SCALABLEOBJECT, &props, pInfo->nObjectID );

										if ( IsValid( pFin->pObject->pObject->pPassage ) )
											AddObjMgr( (EBrushType)BT_PASSAGEOBJECT, &props, pInfo->nObjectID );

										if ( IsValid( pFin->pObject->pObject->pDoor ) )
											AddObjMgr( (EBrushType)BT_WINDOWDOOR, &props, pInfo->nObjectID );
									}
									// NDb::CRPGItem
									if ( pFin && IsValid( pFin->pObject ) && IsValid( pFin->pObject->pRPGItem ) )
									{
										NDb::CRPGItem *pI = pFin->pObject->pRPGItem;
										if ( 189 == pI->GetRecordID() )
											AddObjMgr( (EBrushType)BT_EXPLOSION, &props, pInfo->nObjectID );
										else {
											CDynamicCast<NDb::CRPGMine> p(pI->pSuccessor);
											if (p)
												AddObjMgr((EBrushType)BT_MINE, &props, pInfo->nObjectID);
										}
									}
									bCommonProps = false;
									break;
								}
							case BT_LADDER:
								prop = new CObjProp( "Ladder", 20, CVariant::VT_INT, DT_DEC, CVariant(), true );
								prop->SetOwner( owner );
								prop->SetGroup( nGroup );
								prop->SetObject( pInfo->eBrushType, pInfo->nObjectID );
								props["Ladder"] = prop;
								bCommonProps = true;
								break;
							case BT_TERRHOLE:
								GetHoleProperties( &props, nItem, nVar, pInfo->nObjectID );
								bCommonProps = false;
								break;
							default:
							{
								if ( AddObjMgr( pInfo->eBrushType, &props, pInfo->nObjectID ) )
									bCommonProps = false;
								break;
							}
						}
						if ( bCommonProps )
						{
							prop = new CObjProp( "Floor", 30, CVariant::VT_INT, DT_DEC, CVariant(), true );
							prop->SetOwner( owner );
							prop->SetValue( pInfo->nFloor, false );
							prop->SetGroup( nGroup );
							prop->SetObject( pInfo->eBrushType, pInfo->nObjectID );
							props["Floor"] = prop;
							prop = new CObjProp( "Rotation", 31, CVariant::VT_INT, DT_DEC, CVariant(), false );
							prop->SetOwner( owner );
							prop->SetValue( pInfo->nRotation, false );
							prop->SetGroup( nGroup );
							prop->SetObject( pInfo->eBrushType, pInfo->nObjectID );
							props["Rotation"] = prop;
						}
						//
						CObjectMgr::Intersect( &staticprops, &props );
					}
					//
					if ( IDC_TEMPLATE_TREE == nTree )
					{
						prop = new CObjProp( "MapID", 1, CVariant::VT_INT, DT_DEC, true );
						prop->SetOwner( owner );
						prop->SetValue( nVar, false );
						staticprops["MapID"] =  prop;

						CVec3 pt = GetUserSettings().GetSelectionCenter();
						prop = new CObjProp( "SelectionX", 2, CVariant::VT_FLOAT, DT_DEC, CVariant(), false );
						prop->SetOwner( owner );
						prop->SetValue( pt.x, false );
						staticprops["SelectionX"] =  prop;
						prop = new CObjProp( "SelectionY", 3, CVariant::VT_FLOAT, DT_DEC, CVariant(), false );
						prop->SetOwner( owner );
						prop->SetValue( pt.y, false );
						staticprops["SelectionY"] =  prop;
						prop = new CObjProp( "SelectionZ", 4, CVariant::VT_FLOAT, DT_DEC, CVariant(), false );
						prop->SetOwner( owner );
						prop->SetValue( pt.z, false );
						staticprops["SelectionZ"] =  prop;
					}
					theApp.SetPropMap( &staticprops );
				}
				else
				{
					theApp.SetPropMap( pPlacementProps );
				}
			}
			break;
		case MSG_UPDATE_OBJECTS:
			{
				bDirty = true;
				CLayerCtrl *pL = GetLayer( NBuilding::MakeFragmentID( LID_RECT, 0 ) );
				if ( pL )
					pL->Reset();
			}
			break;
		case MSG_UPDATE_LAYERLIST:
			theApp.GetTemplateView()->SetupLayerList();
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::Setup()
{
	bDirty = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void WriteCamera( int nTree, int nItem, const ICamera::SCameraPos &pos, float fFOV )
{
	const SResTree *pTree = theApp.GetResTree( nTree );
	if ( !pTree )
		return;
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nItem );
	if ( pProps )
	{
		string szPref;
		if ( nTree == IDC_RPG_ITEMS_TREE )
		{
			switch ( geRPGInventoryCamera )
			{
			case CAM_SLOT:
				szPref = "Slot";
				break;
			case CAM_AMMO:
				szPref = "Ammo";
				break;
			}
			szPref += "Camera";
		}
		if ( nTree == IDC_RPG_PERS_TREE )
		{
			if ( GetUserSettings().GetParam( ME_PERS_FACEGENCAMERA ) )
				szPref += "FaceGenCamera";
			else
				szPref += "Camera";
		}
		CPropMap::const_iterator iax = pProps->find( szPref + "AnchorX" );
		CPropMap::const_iterator iay = pProps->find( szPref + "AnchorY" );
		CPropMap::const_iterator iaz = pProps->find( szPref + "AnchorZ" );
		CPropMap::const_iterator id = pProps->find( szPref + "Distance" );
		CPropMap::const_iterator iy = pProps->find( szPref + "Yaw" );
		CPropMap::const_iterator ip = pProps->find( szPref + "Pitch" );
		CPropMap::const_iterator ir = pProps->find( szPref + "Roll" );
		CPropMap::const_iterator ifov = pProps->find( szPref + "FOV" );
		CPropMap::const_iterator e = pProps->end();
		if ( iax == e || iay == e || iaz == e || id == e || iy == e || ip == e || ir == e || ifov == e )
			return;
		iax->second->SetValue( pos.ptAnchor.x );
		iay->second->SetValue( pos.ptAnchor.y );
		iaz->second->SetValue( pos.ptAnchor.z );
		id->second->SetValue( pos.fRod );
		iy->second->SetValue( pos.fYaw );
		ip->second->SetValue( pos.fPitch );
		ir->second->SetValue( pos.fRoll );
		ifov->second->SetValue( fFOV );
		pTree->pItemsTree->ReleasePropList( pProps );
	}
	if ( nTree == IDC_CAMERAS_TREE )
		Refresh<NDb::CDBCamera>( nItem );
	else
		theApp.SetActiveItem( nTree, nItem, -1, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetCameraInfo( const ICamera::SCameraPos &pos, float fFOV )
{
	char str[1024];

	sprintf( str, "Anchor: \t\t(%.2f  %.2f  %.2f)\r\nDistance: \t%.2f\t\r\nYaw: \t\t%.2f\r\nPitch: \t\t%.2f\r\n", 
		pos.ptAnchor.x, pos.ptAnchor.y, pos.ptAnchor.z, pos.fRod, pos.fYaw, pos.fPitch );

	CCameraInfoDlg dlg;
	dlg.m_szInfo = str;
//	dlg.DoModal();
	
	int nTree, nItem, nVar;
	theApp.GetActiveItem( &nTree, &nItem, &nVar );

	if ( nTree != IDC_RPG_ITEMS_TREE && nTree != IDC_RPG_PERS_TREE && nTree != IDC_TEMPLATE_TREE )
		return;

	if ( nTree == IDC_TEMPLATE_TREE )
	{
		nTree = IDC_CAMERAS_TREE;
		nItem = GetUserSettings().GetSelectedBrushID( LID_CAMERAS );
	}
	WriteCamera( nTree, nItem, pos, fFOV );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetDBCameraInfo( const ICamera::SCameraPos &pos, float fFOV )
{
	int nItem = GetUserSettings().GetSelectedBrushID( LID_CAMERAS );
	WriteCamera( IDC_CAMERAS_TREE, nItem, pos, fFOV );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetBrushSize( int nSize )
{
	nBrushSize = nSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetGridVisible( bool bVisible )
{
	bGridVisible = bVisible;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetWireSubTemplates( bool bWire )
{
	bWireSubTemplate = bWire;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetParam( int nParamID, float fValue )
{
	meparams[nParamID] = fValue;
	theApp.WriteProfileInt( "Params", GetMEParamName( nParamID ).c_str(), FP_BITS( fValue ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetSelectionCenter( const CVec3 &ptCenter )
{
	ptSelectionCenter = ptCenter;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::ShowPropertyBrowser( const int nObjectID )
{
	IObjectBrowser *p1 = GetBrowser( OB_OBJECTS_MODEL0 );
	IObjectBrowser *p2 = GetBrowser( OB_OBJECTS_MODEL1 );
	IObjectBrowser *p3 = GetBrowser( OB_OBJECTS_MODEL2 );
	IObjectBrowser *p4 = GetBrowser( OB_OBJECTS_MODEL3 );
	IObjectBrowser *p5 = GetBrowser( OB_OBJECTS_MODEL4 );
	if ( p1 && p2 && p3 && p4 && p5 )
	{
		vector< CPtr<IObjectBrowser> > v;
		v.push_back( p1 );
		v.push_back( p2 );
		v.push_back( p3 );
		v.push_back( p4 );
		v.push_back( p5 );
		CObjBrowserContainer dlg( v );
		dlg.SetObject( nObjectID, -1 );
		dlg.DoModal();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
EMoveMode CLayerList::GetMoveMode() const
{
	return eMoveMode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::SetMoveMode( EMoveMode mode )
{
	eMoveMode = mode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateScroll( CWnd *pWnd, int nBar, UINT nSBCode, UINT nPos )
{
	SCROLLINFO info;
	int line = 5;

	if ( !pWnd->GetScrollInfo( nBar, &info ) )
		return;

	switch( nSBCode )
	{
	case SB_LINEDOWN:
		pWnd->SetScrollPos( nBar, info.nPos + line );
		break;
	case SB_LINEUP:
		pWnd->SetScrollPos( nBar, info.nPos - line );
		break;
	case SB_PAGEDOWN:
		pWnd->SetScrollPos( nBar, info.nPos + info.nPage );
		break;
	case SB_PAGEUP:
		pWnd->SetScrollPos( nBar, info.nPos - info.nPage );
		break;
	case SB_BOTTOM:
		// SetScrollPos( nBar, nHeight );
		break;
	case SB_TOP:
		pWnd->SetScrollPos( nBar, 0 );
		break;
	case SB_THUMBPOSITION:
		pWnd->SetScrollPos( nBar, nPos );
		break;
	case SB_THUMBTRACK:
		pWnd->SetScrollPos( nBar, nPos );
		break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerList::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	UpdateScroll( this, SB_VERT, nSBCode, nPos );
	ArrangeLayers();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
