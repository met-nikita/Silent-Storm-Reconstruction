#include "StdAfx.h"
#include "MapEdit.h"
#include "Layers.h"
#include "dbDefs.h"
#include "Placement.h"
#include "RectsLayer.h"
#include "RectTrack.h"
#include "Unit_ObjDraw.h"
#include "TemplMgr.h"
#include "Templ.h"
#include "ItemsMgr.h"
#include "TreeSelItemDlg.h"
#include "RouteDlg.h"
#include "..\Input\Bind.h"
#include "..\Main\iMain.h"

BEGIN_MESSAGE_MAP(CRectsLayer, CLayerCtrl)
//{{AFX_MSG_MAP(CRectsLayer)
ON_COMMAND(ID_DELRECT, OnDelObject)
ON_COMMAND(ID_ADDTEMPLATE, OnAddTemplate)
ON_COMMAND(ID_ADDUNIT, OnAddUnit)
ON_COMMAND(ID_ADDMODEL, OnAddModel)
ON_COMMAND(ID_ADDEXPLOSION, OnAddExplosion)
ON_COMMAND(ID_ADDCONTAINER, OnAddContainer)
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
EMapObjType TreeID2MapObjType( int nTreeID )
{
	switch( nTreeID )
	{
	case IDC_OBJECTS_TREE:
		return MO_OBJECT;
	case IDC_RPG_PERS_TREE:
		return MO_UNIT;
	case IDC_TEMPLATE_TREE:
		return MO_TEMPLATE;
	case IDC_EXPLOSIONS_TREE:
		return MO_EXPLOSION;
	}		
	return MO_EMPTY;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int MapObjType2TreeID( EMapObjType type )
{
	switch( type )
	{
	case MO_OBJECT:
		return IDC_OBJECTS_TREE;
	case MO_UNIT:
		return IDC_RPG_PERS_TREE;
	case MO_TEMPLATE:
		return IDC_TEMPLATE_TREE;
	case MO_EXPLOSION:
		return IDC_EXPLOSIONS_TREE;
	}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_FONTH = 14;
////////////////////////////////////////////////////////////////////////////////////////////////////
void RotatePt( CPoint &pt, int nAngle )
{
  const float fAng = -ToRadian( (float) nAngle );
  double fc = cos( fAng );
  double fs = sin( fAng );
	
  long x = long(  fc * pt.x + fs * pt.y );
  pt.y   = long( -fs * pt.x + fc * pt.y );
  pt.x   = x;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void RotatePt( CVec2 *pVec, int nAngle )
{
  const float fAng = -ToRadian( (float) nAngle );
  float fc = cos( fAng );
  float fs = sin( fAng );
	
  float x = fc * pVec->x + fs * pVec->y;
  pVec->y = -fs * pVec->x + fc * pVec->y;
  pVec->x = x;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRectsLayer::CRectsLayer( int nLayerType, const string &szName ) : CLayerCtrl( nLayerType, 0, szName.c_str() )
{
	pPlacement = 0;
	nLastFloor = -1000;
	crFont = RGB( 90, 100, 140 );
  m_popup.CreatePopupMenu();
	CString szDelObj, szAddUnit, szAddTemplate, szAddContainer, szAddExplosion;
	szDelObj.LoadString( IDS_DELETE_OBJECT );
	szAddUnit.LoadString( IDS_ADD_UNIT );
	szAddTemplate.LoadString( IDS_ADD_TEMPLATE );
	szAddContainer.LoadString( IDS_ADD_CONTAINER );
	szAddExplosion.LoadString( IDS_ADD_EXPLOSION );
	bmpMenu.LoadBitmap( IDB_TEMPLATE_VIEW );
	m_popup.AppendMenu( MF_STRING | MF_DISABLED | MF_UNHILITE, 0, &bmpMenu );
	m_popup.AppendMenu( MF_SEPARATOR );
	switch ( nLayerType )
	{
		case LID_SUBTEMPLATES:
			m_popup.AppendMenu( MF_STRING | MF_GRAYED, ID_ADDTEMPLATE, szAddTemplate );
			break;
		case LID_UNITS:
			m_popup.AppendMenu( MF_STRING | MF_GRAYED, ID_ADDUNIT, szAddUnit );
			break;
		case LID_OBJECTS:
			m_popup.AppendMenu( MF_STRING | MF_GRAYED, ID_ADDCONTAINER, szAddContainer );
			m_popup.AppendMenu( MF_STRING, ID_ADDEXPLOSION, szAddExplosion );
			break;
	}
//	m_popup.AppendMenu( MF_STRING, ID_ADDTERRSPOT, szAddExplosion );
	m_popup.AppendMenu( MF_SEPARATOR );
  m_popup.AppendMenu( MF_STRING, ID_DELRECT, szDelObj );
	hLayerCursor = ::CreateCursor( IDC_ARROW, IDC_RECTS );
	bDirty = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRectsLayer::~CRectsLayer()
{
	DeleteRects();
	if ( hLayerCursor )
		DestroyCursor( hLayerCursor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::OnLButtonDblClk( UINT nFlags, CPoint pt, ITemplateView *pView )
{
	ASSERT( pView );

	if ( !pPlacement )
		return;
  float x, y;
  pView->ScreenToTemplate( pt, &x, &y );
  int id;
	EMapObjType type = pPlacement->GetObjID( &id, theApp.GetActiveFloor(), x, y, SELECTION_ACCURACY );
	
	switch ( type )
	{
		case MO_TEMPLATE:
			{
				int nextTemplID = pPlacement->GetRectTemplID( theApp.GetActiveFloor(), id );
				theApp.SetActiveItem( IDC_TEMPLATE_TREE, nextTemplID );
			}
			break;
		case MO_UNIT:
			{
				CRouteDlg dlg( id );

				dlg.DoModal();
			}
			break;
	}	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::OnRButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView )
{
	ASSERT( pView );

	pView->ScreenToTemplate( pt, &ptLastRClick.x, &ptLastRClick.y );
	if ( actvObjs.empty() )
	{
		int id;
		EMapObjType type = pPlacement->GetObjID( &id, theApp.GetActiveFloor(), ptLastRClick.x, ptLastRClick.y, SELECTION_ACCURACY );
		if ( type != MO_EMPTY )
		{
			DeactivateObjs();
			SetActiveObj( type, id );
		}	
	}
	//
	pView->GetWnd()->ClientToScreen( &pt );
	m_popup.TrackPopupMenu( TPM_LEFTBUTTON, pt.x, pt.y, this );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::OnLButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView )
{
	ASSERT( pView );
	const UINT RECURSION = 0x100000;
	
	float x, y;
	pView->ScreenToTemplate( pt, &x, &y );
	int idSelect;
	EMapObjType objType = pPlacement->GetObjID( &idSelect, theApp.GetActiveFloor(), x, y, SELECTION_ACCURACY );
	if ( !(nFlags & RECURSION) )
		SelectedItem( pView, objType, idSelect );
	bool bStartDrag = false;

	// решаем что это - начало перетаскивания объекта 
	// или выделение объекта находящегося внутри поселекченного объекта
	if ( objType == MO_EMPTY && !actvObjs.empty() )
		bStartDrag = true;
	else
	{
		for ( int i = 0; i< actvObjs.size(); ++i )
			if ( actvObjs[i].type == objType && actvObjs[i].id == idSelect )
			{
				bStartDrag = true;
				break;
			}
	}
	//
	if ( bStartDrag )
	{
		// операции над поселекченными объектами
		for ( int i = 0; i < (int)actvObjs.size(); ++i )
		{
			const STemplTrack &track = trackRects[actvObjs[i].ind];
			if ( track.pTrack->HitTest( pt ) != -1 )
			{
				// нашелся активный объект, в пределах которого был клик
				nFlags &= MK_SHIFT | MK_CONTROL;
				switch ( nFlags )
				{
					case MK_SHIFT:
					{
						// вращение
						int ang = track.pTrack->TrackRotate( pView->GetWnd(), 45 );
						Rotate( pPlacement, actvObjs[i].type, actvObjs[i].id, ang, pView );
						return;
					}
					case MK_CONTROL: //if ( nFlags & MK_CONTROL )
					{
						// клик с нажатым CTRL - деселектим объект 
						DeactivateObj( actvObjs[i].id );
						return;
					}
				}
				// если ничего из вышеперечисленного, значит будем драгать объект
				
				// получить bound область
				GetCurTrackRect( &curTrackRect );
				CPoint ptOld = track.pTrack->GetCeneter();
				curTrackDCenter = curTrackRect.CenterPoint() - ptOld;
				if ( track.pTrack->Track( pView->GetWnd(), pt, false ) )
				{
					CPoint dv = track.pTrack->GetCeneter() - ptOld;
					trackRects[actvObjs[i].ind].pTrack->Move( -dv );
					for ( int j=0; j < (int)actvObjs.size(); ++j )
					{
						const STemplTrack &tr = trackRects[actvObjs[j].ind];
						tr.pTrack->Move( dv );
						float  x, y;
						pView->ScreenToTemplate( tr.pTrack->GetCeneter(), &x, &y );
						pPlacement->MoveObj( theApp.GetActiveFloor(), tr.objType, tr.objID, CVec3( x, y, tr.fDZ ) );
					}
					SetupRects( pView, true );
				}
				pView->Repaint();
				return;
			}
		}
	}
	// Select объектов
	{
		if ( idSelect >= 0 )
		{
			if ( nFlags & MK_CONTROL )
				AddActiveObj( objType, idSelect );
			else
			{
				DeactivateObjs();
				if ( SetActiveObj( objType, idSelect ) && !(RECURSION & nFlags) )
				{
					// чтобы не нужно было повторно кликать на объект для трэкинга
					// RECURSION - защита от переполнения стека
				//	OnLButtonDown( nFlags | RECURSION, pt, pView );
				}
			}
			return;
		}
		else
			DeactivateObjs();
	}	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::Paint( ITemplateView *pView, float fBrightness, bool bGrayed )
{
	ASSERT( pView );
  if ( !pPlacement )
    return;

	if ( nLastFloor != theApp.GetActiveFloor() )
		SetupRects( pView );
	//
	CDC *pDC = pView->GetPaintDC();
  CRect  r;
  CBrush brush( GetSysColor( COLOR_WINDOW ) );
  CBrush *pOldBrush = pDC->SelectObject(&brush);
	int nThickness = pView->GetSpacing() == 1 ? 1 : 2;
	int i;
  
  for( i = 0; i < (int)trackRects.size(); ++i )
  {
    CTemplate *pNested = 0;
		if ( MO_TEMPLATE == trackRects[i].objType ) 
			pNested = theTemplMgr.GetTempl( pPlacement->GetRectTemplID( theApp.GetActiveFloor(), trackRects[i].objID ) );
		CMERectTracker *pTr = trackRects[i].pTrack;
		
    if ( pNested )
		{
			CString szStr = pNested->GetName();
			CSize size = pDC->GetTextExtent( szStr );
			CPoint ptPos( -0.0f * size.cx, 2 - 0.5f * size.cy );
			RotatePt( ptPos, -pTr->GetRotation() );
			ptPos += pTr->GetCeneter();
			CFont font;
			font.CreateFont( N_FONTH, 
				0, 
				pTr->GetRotation() * 10, 
				0, 
				FW_NORMAL, 
				false, 
				false, 
				0, 
				ANSI_CHARSET, 
				OUT_DEFAULT_PRECIS, 
				CLIP_DEFAULT_PRECIS, 
				PROOF_QUALITY, 
				DEFAULT_PITCH | FF_SWISS | TMPF_TRUETYPE, 
				"Sans Serif" );
			CFont* def_font = pDC->SelectObject(&font);			
			COLORREF crOld = pDC->SetTextColor( crFont );
			pDC->SetTextAlign(TA_CENTER);
			pDC->SetBkMode(TRANSPARENT);
			pTr->GetBoundsRect( &r );
      pDC->ExtTextOut( ptPos.x, ptPos.y, ETO_CLIPPED, r, szStr,	0 );
			pDC->SelectObject( def_font );
			pDC->SetTextColor( crOld );
			font.DeleteObject(); 			
		}
    pTr->Draw( pDC, nThickness, bGrayed );
  }	
	if ( !pView->IsZooming() )
	{
		int w = pView->GetSpacing() > 5 ? 2 : 1;
		for( i = 0; i < (int)internalRects.size(); ++i )
			internalRects[i].pTrack->Draw( pDC, w, true );
	}
	
  pDC->SelectObject(pOldBrush);
  brush.DeleteObject();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::SetPlacement( CPlacement *pPl, CPlacementCache *pCache, ITemplateView *pView )
{
	ASSERT( pView );
	pPlacement = pPl;
	pPlacementCache = pCache;
	ptLastRClick = CVec2( 0, 0 );
	SetupRects( pView );
	bDirty = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::Reset()
{
	bDirty = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::DeleteRects()
{
	ClearSelection();
	int i;
  for( i = 0; i < (int)trackRects.size(); ++i )
    delete trackRects[i].pTrack;
  for( i = 0; i < (int)internalRects.size(); ++i )
    delete internalRects[i].pTrack;
  trackRects.clear();
	internalRects.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CPlacement* CRectsLayer::GetFirstPlacement( int nTemplateID )
{
	if ( !pPlacementCache )
		return 0;
	const SResTree *pTree = theApp.GetResTree( IDC_TEMPLATE_TREE );
	if ( !pTree )
		return 0;
	vector<int> vars;
	if ( !pTree->pItemsTree->GetItemVariants( nTemplateID, &vars ) )
		return false;

	if ( !vars.empty() )
	{
		int nPlID = vars[0];
		CPlacement *pPl = (*pPlacementCache)[nPlID];
		if ( !pPl )
			(*pPlacementCache)[nPlID] = theTemplMgr.GetPlacement( nPlID );
		return (*pPlacementCache)[nPlID];
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::AddTracker( ITemplateView *pView, EMapObjType type, int nObjID, const CVec3 &ptCenter, const CVec2 &ptSize, int nRotation )
{
	const int nFloor = theApp.GetActiveFloor();
	const int nDelta = pView->GetSpacing();
	CPoint ptScreen;
	//
  switch ( type )
  {
		case MO_TEMPLATE:
		{
			pView->TemplateToScreen( &ptScreen, ptCenter.x, ptCenter.y );
			STemplTrack tr;
			//
			const int nw = ptSize.x * nDelta;
			const int nh = ptSize.y * nDelta;
			tr.pTrack  = new CMERectTracker( pView, this, ptScreen, nw, nh, nRotation, nDelta );
			tr.objType = MO_TEMPLATE;
			tr.objID   = nObjID;
			tr.fDZ = ptCenter.z;
			trackRects.push_back( tr );

			CVec2 ptc( ptCenter.x, ptCenter.y );
			if ( !pView->IsZooming() && theApp.GetTemplateMaxDepth() > 0 )
			{
				CPlacement *pTmpPl = GetFirstPlacement( pPlacement->GetRectTemplID( nFloor, nObjID ) );
				SetupInternalRects( pView, pTmpPl, ptc, nRotation, 0 );
			}
			break;
		}
		case  MO_UNIT:
		{
			pView->TemplateToScreen( &ptScreen, ptCenter.x, ptCenter.y );
			//
			STemplTrack tr;
			tr.pTrack  = new CUnitTracker( pView, this, ptScreen, nRotation, nDelta );
			tr.objType = MO_UNIT;
			tr.objID   = nObjID;
			tr.fDZ = 0;
			trackRects.push_back( tr );
			break;
	  }
		case MO_OBJECT:
		{
			pView->TemplateToScreen( &ptScreen, ptCenter.x, ptCenter.y );
			//
			STemplTrack tr;
			tr.pTrack  = new CObjTracker( pView, this, ptScreen, ptSize * nDelta, nRotation, nDelta );
			tr.objType = MO_OBJECT;
			tr.objID   = nObjID;
			tr.fDZ = ptCenter.z;
			trackRects.push_back( tr );
			break;
		}
		case MO_EXPLOSION:
		{
			pView->TemplateToScreen( &ptScreen, ptCenter.x, ptCenter.y );
			//
			STemplTrack tr;
			tr.pTrack  = new CObjTracker( pView, this, ptScreen, ptSize* nDelta, nRotation, nDelta, RGB( 200, 200, 0 ) );
			tr.objType = MO_EXPLOSION;
			tr.objID   = nObjID;
			tr.fDZ = ptCenter.z;
			trackRects.push_back( tr );
			break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::SetupRects( ITemplateView *pView, bool bKeepActiveObjs )
{
	vector<SActvObj> objs2activate;

	if ( bKeepActiveObjs )
		objs2activate = actvObjs;
	else
	{
		//SendNotify( LLN_PROPSRESET );
	}

	actvObjs.clear();
  DeleteRects();
	
  if ( !pPlacement )
    return;
	nLastFloor = theApp.GetActiveFloor();
	const int nDelta = pView->GetSpacing();
	CVec3 ptCenter;
	CVec2 ptSize;
  int rotation = 0;
	//
	switch ( GetLayerType() )
	{
		case LID_SUBTEMPLATES:
			pPlacement->MoveFirst( nLastFloor );
			while ( pPlacement->MoveNext() )
			{
				CVec3 ptCenter;
				pPlacement->GetRect( &ptCenter, &ptSize.x, &ptSize.y, &rotation );
				AddTracker( pView, MO_TEMPLATE, pPlacement->GetRectID(), ptCenter, ptSize, rotation );
			}
			break;
		case LID_UNITS:
			pPlacement->MoveFirstUnit( nLastFloor );
			while ( pPlacement->MoveNextUnit() )
			{
				int id = pPlacement->GetUnitID();
				pPlacement->GetObjPos( nLastFloor, MO_UNIT, id, &ptCenter, &rotation );
				//
				AddTracker( pView, MO_UNIT, id, ptCenter, CVec2( 0, 0 ), rotation );
			}
			break;
		case LID_OBJECTS:
			pPlacement->MoveFirstObj( nLastFloor );
			while ( pPlacement->MoveNextObj() )
			{
				int id = pPlacement->GetObjID();
				pPlacement->GetObjPos( nLastFloor, MO_OBJECT, id, &ptCenter, &rotation );
				pPlacement->GetObjectSize( nLastFloor, MO_OBJECT, id, &ptSize, 0 );
				//
				AddTracker( pView, MO_OBJECT, id, ptCenter, ptSize, rotation );
			}
			//
			pPlacement->MoveFirstExplosion( nLastFloor );
			while ( pPlacement->MoveNextExplosion() )
			{
				int id = pPlacement->GetExplosionID();
				pPlacement->GetObjPos( nLastFloor, MO_EXPLOSION, id, &ptCenter, &rotation );
				pPlacement->GetObjectSize( nLastFloor, MO_EXPLOSION, id, &ptSize, 0 );
				//
				AddTracker( pView, MO_EXPLOSION, id, ptCenter, ptSize, rotation );
			}
			break;
		default:
			ASSERT(0);
			break;
	}
	//
	if ( bKeepActiveObjs )
	{
		for ( int i = 0; i < objs2activate.size(); ++i )
			AddActiveObj( objs2activate[i].type, objs2activate[i].id );
	}
	Repaint();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Определить куда можно поместить текущий трэкаемый прямоугольник,
// поближе к желаемой юзером точке
// ppt - центр трэкаемого прямоугольника
void CRectsLayer::GetNearestPos( CPoint *ppt, ITemplateView *pView )
{
  if ( !pPlacement )
    return;
  CPoint pt = *ppt + curTrackDCenter;
	
	CVec2 ptDesired;
	pView->ScreenToTemplate( pt, &ptDesired.x, &ptDesired.y );
  CVec2 ptNearest;
	int nDelta = pView->GetSpacing();
  pPlacement->GetNearestPos( &ptNearest, ptDesired, 
    float( curTrackRect.Width() ) / nDelta, 
    float( curTrackRect.Height() ) / nDelta );
  ptDesired = ptNearest - ptDesired;
	CPoint delta( ptDesired.x * nDelta, -ptDesired.y * nDelta );
  (*ppt) = pt + delta - curTrackDCenter;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::MoveRects( const CPoint &pt )
{
	int i;

  for ( i = 0; i < (int)trackRects.size(); ++i )
    trackRects[i].pTrack->Move( pt );
  for ( i = 0; i < (int)internalRects.size(); ++i )
    internalRects[i].pTrack->Move( pt );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRectsLayer::SetActiveObj( EMapObjType type, int id )
{
  actvObjs.clear();
	
  return AddActiveObj( type, id );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRectsLayer::AddActiveObj( EMapObjType type, int id )
{
	for( int i = 0; i < (int)trackRects.size(); ++i )
		if ( trackRects[i].objType == type && trackRects[i].objID == id )
		{
			trackRects[i].pTrack->SetStyle( CRectTracker::hatchedBorder | CRectTracker::resizeOutside );
			SActvObj obj = { type, id, i };
			trackRects[i].pTrack->SetStyle( CRectTracker::hatchedBorder | CRectTracker::resizeOutside );
			trackRects[i].pTrack->GetBoundsRect( &curTrackRect );
			actvObjs.push_back( obj );
			return true;
		}
		return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::DeactivateObjs()
{
  if ( actvObjs.empty() )
    return;
  for ( int i=0; i < (int)actvObjs.size(); ++i )
		if ( -1 != actvObjs[i].ind )
			trackRects[actvObjs[i].ind].pTrack->SetStyle( CRectTracker::solidLine );
	actvObjs.clear();
	Repaint();
	SendNotify( LLN_PROPSRESET );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::DeactivateObj( int id )
{
  if ( actvObjs.empty() )
    return;
  for ( int i=0; i < (int)actvObjs.size(); ++i )
    if ( actvObjs[i].id == id )
    {
			if ( -1 != actvObjs[i].ind )
				trackRects[actvObjs[i].ind].pTrack->SetStyle( CRectTracker::solidLine );
      actvObjs.erase( actvObjs.begin() + i );
			Repaint();
    }
	SendNotify( LLN_PROPSRESET );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::Rotate( CPlacement *pPl, EMapObjType objType, int objID, int ang, ITemplateView *pView )
{
  if ( pPl->RotateObj( theApp.GetActiveFloor(), objType, objID, ang ) )
	{
    SetActiveObj( objType, objID );
		SetPlacement( pPl, pPlacementCache, pView );
		pView->Repaint();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//! получить ограничивающий прямоугольник для активных объектов
void CRectsLayer::GetCurTrackRect( CRect *pRect )
{
  if ( actvObjs.empty() )
    return;
  trackRects[actvObjs[0].ind].pTrack->GetBoundsRect( pRect );
  for ( int i=1; i < (int)actvObjs.size(); ++i )
  {
		CRect r;
		trackRects[actvObjs[i].ind].pTrack->GetBoundsRect( &r );
    pRect->left   = Min( pRect->left, r.left );
    pRect->right  = Max( pRect->right, r.right );
    pRect->top    = Min( pRect->top, r.top );
    pRect->bottom = Max( pRect->bottom, r.bottom );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::OnDelObject() 
{
  if ( !pPlacement || actvObjs.empty() )
    return;

	int i;
  for ( i = 0; i < (int)actvObjs.size(); ++i )
	{
    pPlacement->DeleteObj( theApp.GetActiveFloor(), actvObjs[i].type, actvObjs[i].id );
		trackRects[actvObjs[i].ind].objID = -1;
	}
  for ( i = 0; i < trackRects.size(); ++i )
		if ( -1 == trackRects[i].objID )
		{
			delete trackRects[i].pTrack;
			trackRects.erase( trackRects.begin() + i );
			i = 0;
		}

	actvObjs.clear();
	SendNotify( LLN_SETUP );
	Repaint();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags, ITemplateView *pView )
{
	switch ( nChar )
	{
	case VK_DELETE:
		OnDelObject();
		break;
	}		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRectsLayer::DropObject( EMapObjType type, int nObjID, ITemplateView *pView, CVec3 ptPos, int nRotation )
{
	ASSERT( pView );

	if ( !pPlacement || !IsVisible() )
		return -1;

	CPoint pt;
	float  x = ptPos.x, y = ptPos.y;
	bool bSuccess = false;
	int id = -1;
	const int nFloor = theApp.GetActiveFloor();

	if ( CVec3( -1, -1, -1 ) == ptPos )
	{
		GetCursorPos( &pt );
		pView->GetWnd()->ScreenToClient( &pt );
		pView->ScreenToTemplate( pt, &x, &y );
		ptPos = CVec3( x, y, 0 );
	}
	CVec2 ptNearest( ptPos.x, ptPos.y );
	pPlacement->GetNearestPos( &ptNearest, ptNearest, 0, 0 );
	ptPos.x = ptNearest.x, ptPos.y = ptNearest.y;
	
	switch ( type )
	{
		case MO_TEMPLATE:
		{
			CTemplate *pTempl = theTemplMgr.GetTempl( nObjID );
			if ( !pTempl )
				return -1;
			
			id = pPlacement->AddRect( nFloor, CVec2( ptPos.x, ptPos.y ), pTempl->GetWidth(), pTempl->GetHeight(), nObjID );
			break;
		}
		case MO_UNIT:
				id = pPlacement->AddUnit( nFloor, CTPoint<int>( ptPos.x + 0.5f, ptPos.y + 0.5f ), nObjID );
				break;
		case MO_OBJECT:
			id = pPlacement->AddObj( nFloor, CVec2( ptPos.x, ptPos.y ), IDC_OBJECTS_TREE, nObjID );
			break;
		case MO_EXPLOSION:
			id = pPlacement->AddExplosion( nFloor, CVec2( ptPos.x, ptPos.y ), nObjID );
			break;
	}
	if ( id != -1 )
	{
		CVec2 ptCenter, ptSize = VNULL2;
		int nr;
		float z = ptPos.z;

		pPlacement->GetObjPos( nFloor, type, id, &ptPos, &nr ); // берем реальное положение ptPos, куда смог поместиться объект
		pPlacement->GetObjectSize( nFloor, type, id, &ptSize, &ptCenter );
		ptPos.z = z;
		pPlacement->RotateObj( nFloor, type, id, nRotation );
		pPlacement->MoveObj( nFloor, type, id, ptPos );
		AddTracker( pView, type, id, ptPos, ptSize, nRotation );
		SetActiveObj( type, id );
		Repaint();
	}
	else
		MessageBox( "Cant place here", "Note" );

	return id;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::OnTimer( ITemplateView *pView )
{
	ASSERT( pView );

	if ( actvObjs.empty() || !pView->GetWnd()->IsWindowVisible() || !IsActive() )
		return;
	//
	CDC *pDC = pView->GetWnd()->GetDC();
	for ( int i = 0; i < actvObjs.size(); ++i )
	{
		if ( -1 == actvObjs[i].ind )
			continue;
		const STemplTrack &tr = trackRects[ actvObjs[i].ind ];
		if ( tr.objType != MO_TEMPLATE )
			tr.pTrack->Draw( pDC, 1, false );
	}
	pView->GetWnd()->ReleaseDC( pDC );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRectsLayer::OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message, ITemplateView *pView )
{
	if ( !actvObjs.empty() )
		for ( int i=0; i < (int)actvObjs.size(); ++i )
		{
			if ( trackRects[actvObjs[i].ind].pTrack->SetCursor(this, nHitTest))
				return true;
		}
	if ( hLayerCursor )
	{
		SetCursor( hLayerCursor );
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::ClearSelection()
{
	DeactivateObjs();
	actvObjs.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::SetupInternalRects( ITemplateView *pView, CPlacement *pPl, const CVec2 &ptCenter, int nRotation, int nDepth )
{
	if ( nDepth == theApp.GetTemplateMaxDepth() || !pPl )
		return;
	const int nFloor = theApp.GetActiveFloor();
	const int nDelta = pView->GetSpacing();
	CVec2 ptOffset( 0.5f * pPl->GetWidth(), 0.5f * pPl->GetHeight() );
	
	pPl->MoveFirst( nFloor );
	while ( pPl->MoveNext() )
	{
    int rotation;
		float w, h;
		CVec3 ptGet;
    pPl->GetRect( &ptGet, &w, &h, &rotation );
		CVec2 pt( ptGet.x, ptGet.y );
		pt -= ptOffset;
		RotatePt( &pt, nRotation );
		pt += ptCenter;
		rotation += nRotation;
		//
		CPoint ptScreen;
    pView->TemplateToScreen( &ptScreen, pt.x, pt.y );
    STemplTrack tr;
		//
    tr.pTrack  = new CMERectTracker( pView, this, ptScreen, w * nDelta, h * nDelta, rotation, nDelta );
		tr.objType = MO_TEMPLATE;
    tr.objID   = pPl->GetRectID();
    internalRects.push_back( tr );

		CPlacement *pTmpPl = GetFirstPlacement( pPl->GetRectTemplID() );
		SetupInternalRects( pView, pTmpPl, pt, rotation, nDepth + 1 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::SelectedItem( ITemplateView *pView, EMapObjType type, int nItemID )
{
	switch( type )
	{
		case MO_OBJECT:
			pView->SelectedItem( IDC_OBJECTS_TREE, pPlacement->GetObjModelID( theApp.GetActiveFloor(), nItemID ), nItemID );
			break;
		case MO_UNIT:
			pView->SelectedItem( IDC_RPG_PERS_TREE, pPlacement->GetUnitMonsterID( theApp.GetActiveFloor(), nItemID ), nItemID );
			break;
		case MO_EXPLOSION:
			pView->SelectedItem( IDC_EXPLOSIONS_TREE, pPlacement->GetExplosionPower( theApp.GetActiveFloor(), nItemID ), nItemID );
			break;
		case MO_TEMPLATE:
			pView->SelectedItem( IDC_TEMPLATE_TREE, pPlacement->GetRectTemplID( theApp.GetActiveFloor(), nItemID ), nItemID );
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRectsLayer::ChangeObject( int nTreeID, int nObjectID, int nNewRelation, ITemplateView *pView )
{
  if ( !pPlacement )
    return false;
	
	EMapObjType type = TreeID2MapObjType( nTreeID );
	const int nFloor = theApp.GetActiveFloor();
	CVec3 pt;
	int nRotation;
	if ( !pPlacement->GetObjPos( nFloor, type, nObjectID, &pt, &nRotation ) )
		return false;
	const BYTE nRoomID = type == MO_OBJECT ? pPlacement->GetObjRoomID( nFloor, nObjectID ) : 0;
  if ( !pPlacement->DeleteObj( nFloor, type, nObjectID ) )
		return false;
	int id = DropObject( type, nNewRelation, pView, pt, nRotation );
	if ( -1 == id )
		return false;
	if ( MO_OBJECT == type )
		pPlacement->SetObjRoomID( nFloor, id, nRoomID );
//	SelectedItem( pView, type, id );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRectsLayer::AddObject( int nTableID )
{
	const SResTree *pTree = theApp.GetResTree( nTableID );
	if ( !pTree )
		return -1;
	CTreeSelItemDlg dlg( vector<SResTree>( 1, *pTree ) );
	
	if ( IDOK != dlg.DoModal() )
		return -1;
	int nTree, nItem;
	dlg.GetSelectedItemID( &nTree, &nItem );
	return nItem;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::OnAddExplosion() 
{
	if ( !pPlacement )
		return;
	CExplosionDlg dlg;

	if ( IDOK != dlg.DoModal() )
		return;
	int id = pPlacement->AddExplosion( theApp.GetActiveFloor(), ptLastRClick, dlg.m_fPower );
	if ( -1 != id )
		SendNotify( LLN_SETUP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::OnAddTemplate() 
{
//	DropObject( MO_TEMPLATE, AddObject( IDC_TEMPLATE_TREE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::OnAddUnit() 
{
//	AddObject( IDC_RPG_PERS_TREE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::OnAddModel() 
{
//	AddObject( IDC_OBJECTS_TREE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::OnAddContainer() 
{
//	AddObject( IDC_PARTICLES_TREE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRectsLayer::OnVisible() 
{
	CLayerCtrl::OnVisible();
	switch ( GetLayerType() )
	{
		case LID_SUBTEMPLATES:
			NInput::PostEvent( "update_subtemplates" );
			break;
		case LID_OBJECTS:
			NInput::PostEvent( "update_objects" );
			break;
		case LID_UNITS:
			NInput::PostEvent( "update_units" );
			break;
	}
	NMainLoop::StepApp( true, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExplosionDlg dialog

CExplosionDlg::CExplosionDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CExplosionDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CExplosionDlg)
	m_fPower = 0.0f;
	//}}AFX_DATA_INIT
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplosionDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CExplosionDlg)
	DDX_Text(pDX, IDC_EXPLPOWER, m_fPower);
	//}}AFX_DATA_MAP
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CExplosionDlg, CDialog)
	//{{AFX_MSG_MAP(CExplosionDlg)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
////////////////////////////////////////////////////////////////////////////////////////////////////
// CExplosionDlg message handlers
