#include "StdAfx.h"
#include "MapEdit.h"
#include "Layers.h"
#include "FloorLayer.h"
#include "TemplateView.h"
#include "Placement.h"
#include "dbDefs.h"
#include "ItemsMgr.h"
#include "Floor.h"
#include "..\Main\DiscretePos.h"
#include "iWysiwyg.h"
#include "MEUserSettings.h"
#include "..\Input\Bind.h"

const int ERASE_BRUSH = 3;
const int ERASE_MODEL_ID = -10;
// Текущий активный этаж получаем у theApp
////////////////////////////////////////////////////////////////////////////////////////////////////
CFloorsLayer::CFloorsLayer( EFloorType type, int nLayerID, CString szName, int nBrushesTreeID, int nFloorLayerID ) 
 : CBaseLayer( nLayerID, nFloorLayerID, szName, nBrushesTreeID ), pPlacement( 0 ), floorType( type ), nFloorLayer( nFloorLayerID )
{
	switch ( type )
	{
		case FT_FLOOR:
		case FT_FLOOR_INTERMEDIATE:
			hLayerCursor = ::CreateCursor( IDC_ARROW, IDC_FLOORS );
			break;			
		case FT_SOLID_:
		case FT_SOLID_INTERMEDIATE:
			hLayerCursor = ::CreateCursor( IDC_ARROW, IDC_SOLIDS );
			break;			
	}
	NInput::PostEvent( "update" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CFloorsLayer::~CFloorsLayer()
{
	if ( hLayerCursor )
		DestroyCursor( hLayerCursor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFloorsLayer::OnLButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView )
{
	ASSERT( pView );
	if ( !CanDraw() )
		return;
	TrackFloor( pView );
	pView->Repaint();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFloorsLayer::OnMouseMove(UINT nFlags, CPoint pt, ITemplateView *pView )
{
	ASSERT( pView );
	if ( !CanDraw() )
		return;
	if ( pView->GetPaintMode() == EM_SELECT )
		DrawBrush( pt, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвратить указатель на активную планировку этажа
// Возвр. 0, если планировка не существует или ошибка
inline const CFloorPlan* CFloorsLayer::GetFloor() const
{
  int nFloor = theApp.GetActiveFloor();
  
  if ( !pPlacement )
    return 0;
  return ((const CPlacement*)pPlacement)->GetFloor( floorType, nFloor, nFloorLayer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Возвратить указатель на активную планировку этажа
// Возвр. 0, если планировка не существует  или ошибка
inline CFloorPlan* CFloorsLayer::GetFloor()
{
  int nFloor = theApp.GetActiveFloor();
  
  if ( !pPlacement || !pPlacement->HasFloor( floorType, nFloor, nFloorLayer ) )
    return 0;
  return pPlacement->GetFloor( floorType, nFloor, nFloorLayer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFloorsLayer::DrawCells( CDC *pDC, int nModelID, const vector<CFloorPlan::SCell> &cells, ITemplateView *pView ) const
{
	COLORREF cr = ERASE_MODEL_ID == nModelID ? RGB( 255, 255, 255 ) : GetColor( nModelID );
	CBrush brush( HS_CROSS, cr );
	CPen pen( PS_SOLID, 1, cr );
	CBrush *pOld = pDC->SelectObject( &brush );
	CPen *pPenOld = pDC->SelectObject( &pen );
	CSize sz = GetSize( nModelID );
		
	for ( int i = 0; i < cells.size(); ++i )
	{
		CPoint ptBL, ptRT;
		if ( cells[i].x < 0 )
			continue; // ячейка была удалена
		pView->TemplateToScreen( &ptBL, CPoint( cells[i].x, cells[i].y ) );
		pView->TemplateToScreen( &ptRT, CPoint( cells[i].x + sz.cx, cells[i].y + sz.cy ) );
		CRect r( ptBL.x, ptRT.y, ptRT.x, ptBL.y );
		pDC->FillRect( &r, &brush );
		pDC->Rectangle( &r );
		//pDC->PatBlt( r.left, r.top, r.Width(), r.Height(), PATCOPY );
	}
	pDC->SelectObject( pOld );
	pDC->SelectObject( pPenOld );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFloorsLayer::Paint( ITemplateView *pView, float fBrightness, bool bGrayed )
{
  ASSERT( pView );
	CDC *pDC = pView->GetPaintDC();

	CFloorPlan* pFloor = GetFloor();
	if ( !pFloor )
		return;
	pFloor->Update();
	const CFloorPlan::CCellInfo *pInfo = pFloor->GetFloor();
	//
	for ( CFloorPlan::CCellInfo::const_iterator it = pInfo->begin(); it != pInfo->end(); ++it )
	{
		DrawCells( pDC, it->first, it->second, pView );
	}
	rOldBrush.SetRectEmpty();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFloorsLayer::Reset()
{
	CBaseLayer::Reset();
	sizeMap.clear();
	ClearColors();
	ClearSelection();		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFloorsLayer::SetPlacement( CPlacement *pPl )
{
  pPlacement = pPl;
	Reset();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить цвет пола
// если цвет для данной модели неизвестен, то он читается из базы
DWORD CFloorsLayer::GetColor( int nFloorModelID ) const
{
	unordered_map<int, DWORD>::const_iterator it = colorMap.find( nFloorModelID );

	if ( colorMap.end() == it )
	{
		int cr = DEF_FLOOR_COLOR;
		const SResTree *pTree = 0;
		switch( floorType )
		{
			case FT_FLOOR:
			case FT_FLOOR_INTERMEDIATE:
				pTree = theApp.GetResTree( IDC_CONSTRUCTIONPARTS_TREE );
				break;
			case FT_SOLID_:
			case FT_SOLID_INTERMEDIATE:
				pTree = theApp.GetResTree( IDC_CONSTRUCTIONPARTS_TREE );
				break;
			case FT_ROOM:
				{
					SRoom room;
					if ( !pPlacement->GetRoom( &room, theApp.GetActiveFloor(), nFloorModelID ) )
						return cr;
					return room.dwColor;
				}
		}
		if ( pTree && pTree->pItemsTree )
		{
			const CPropMap *pProps = pTree->pItemsTree->GetPropList( nFloorModelID );
			if ( pProps )
			{
				CPropMap::const_iterator itc = pProps->find( "UserColor" );
				if ( pProps->end() != itc )
					cr = itc->second->GetValue();
				pTree->pItemsTree->ReleasePropList( pProps );
			}
		}
		const_cast<CFloorsLayer*>(this)->colorMap[nFloorModelID] = cr;
		return cr;
	}
	return it->second;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Получить размер модельки
// если размер для данной модели неизвестен, то он читается из базы
CSize CFloorsLayer::GetSize( int nFloorModelID ) const
{
	unordered_map<int, CSize>::const_iterator it = sizeMap.find( nFloorModelID );
	
	if ( sizeMap.end() == it )
	{
		const SResTree *pTree = 0;
		switch( floorType )
		{
		case FT_FLOOR:
		case FT_FLOOR_INTERMEDIATE:
			pTree = theApp.GetResTree( IDC_CONSTRUCTIONPARTS_TREE );
			break;
		case FT_SOLID_:
		case FT_SOLID_INTERMEDIATE:
			pTree = theApp.GetResTree( IDC_CONSTRUCTIONPARTS_TREE );
			break;
		}
		CSize sz( 1, 1 );
		if ( pTree && pTree->pItemsTree )
		{
			const CPropMap *pProps = pTree->pItemsTree->GetPropList( nFloorModelID );
			if ( pProps )
			{
				CPropMap::const_iterator itw = pProps->find( "SizeY" );
				CPropMap::const_iterator itl = pProps->find( "SizeX" );
				if ( pProps->end() != itw && pProps->end() != itl )
				{
					int nRotation = SDiscretePos::TURN_0;//itr->second->GetValue();
					if ( SDiscretePos::TURN_90 == nRotation || SDiscretePos::TURN_270 == nRotation )
						sz = CSize( int( itw->second->GetValue() ) * 2, int( itl->second->GetValue() ) * 2 );
					else
						sz = CSize( int( itl->second->GetValue() ) * 2, int( itw->second->GetValue() ) * 2 );
				}
				pTree->pItemsTree->ReleasePropList( pProps );
			}
		}
		const_cast<CFloorsLayer*>(this)->sizeMap[nFloorModelID] = sz;
		return sz;
	}
	return it->second;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFloorsLayer::ClearColors()
{
	colorMap.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFloorsLayer::TrackFloor( ITemplateView *pView )
{
	if ( !pPlacement )
		return false;
	int nFloorModelID = GetBrush().nItemID;
	int nRotationID = GetUserSettings().GetActiveRotationID();
	if ( EM_ERASE != pView->GetPaintMode() )
	{
		if ( nFloorModelID < 0 )
			return false;
	}
	else
		nFloorModelID = ERASE_MODEL_ID;
	CWnd *pWnd = pView->GetWnd();
  CFloorPlan *pFloor = pPlacement->GetFloor( floorType, theApp.GetActiveFloor(), nFloorLayer );
  if ( !pFloor )
    return false;
	// don't handle if capture already set
  if (::GetCapture() != NULL)
    return false;
  
	CSize size = GetSize( nFloorModelID );
	const int nMaxW = pPlacement->GetWidth();
	const int nMaxH = pPlacement->GetHeight();
  // set capture to the window which received this message
  pWnd->SetCapture();

	// 
	CPoint ptClip( pPlacement->GetWidth(), 0 );
	CPoint ptClipLT( 0, pPlacement->GetHeight() - 1 );
	pView->TemplateToScreen( &ptClip, ptClip );
	pView->TemplateToScreen( &ptClipLT, ptClipLT );
	CRect rClip( ptClipLT, ptClip );
	pWnd->ClientToScreen( &rClip );
	::ClipCursor( &rClip );
  
  CPoint pt;
	float  fx, fy;
	CFloorPlan::SCell cellOld, cellNew;
	vector<CFloorPlan::SCell> cells;
	
  GetCursorPos( &pt );
  pWnd->ScreenToClient( &pt );
	pView->ScreenToTemplate( pt, &fx, &fy );
	cellOld.x = int( fx );
	cellOld.y = int( fy );
	cellOld.nRotationID = nRotationID;
	if ( !(cellOld.x + size.cx <= nMaxW && cellOld.y + size.cy <= nMaxH) )
	{
		::ClipCursor( 0 );
		ReleaseCapture();
		return false;
	}
	cells.push_back( cellOld );

  // get DC for drawing
  CDC* pDC = pWnd->GetDC();
  ASSERT_VALID(pDC);
	DrawCells( pDC, nFloorModelID, cells, pView );
	
  // get messages until capture lost or cancelled/accepted
  for (;;)
  {
    MSG msg;
    VERIFY(::GetMessage(&msg, NULL, 0, 0));
    
    if ( CWnd::GetCapture() != pWnd )
		{
			pWnd->ReleaseDC(pDC);
			::ClipCursor( 0 );
      return false;
		}
    if ( WM_LBUTTONUP == msg.message )
			break;
		
    switch (msg.message)
    {
      // handle movement/accept messages
    case WM_MOUSEMOVE:
			{
				pt.y = (int)(short)HIWORD(msg.lParam);
				pt.x = (int)(short)LOWORD(msg.lParam);
				pView->ScreenToTemplate( pt, &fx, &fy );
				cellNew.x = int( fx );
				cellNew.y = int( fy );
				cellNew.nRotationID = nRotationID;
				int dx = abs( cellNew.x - cellOld.x );
				int dy = abs( cellNew.y - cellOld.y );
				
				if ( (dx || dy) && dx % size.cx == 0 && dy % size.cy == 0
					&& cellNew.x + size.cx <= nMaxW && cellNew.y + size.cy <= nMaxH )
				{
					cells.push_back( cellNew );
					DrawCells( pDC, nFloorModelID, cells, pView );
					cellOld = cellNew;
				}
			}
      break;
      // handle cancel messages
    case WM_KEYDOWN:
      if (msg.wParam != VK_ESCAPE)
        break;
    case WM_RBUTTONDOWN:
			pWnd->ReleaseDC(pDC);
			ReleaseCapture();
			::ClipCursor( 0 );
      return false;
      // just dispatch rest of the messages
    default:
      DispatchMessage(&msg);
      break;
    }
  }
  pWnd->ReleaseDC(pDC);
  ReleaseCapture();
	::ClipCursor( 0 );

	if ( EM_ERASE == pView->GetPaintMode() )
		pFloor->DeleteCells( cells );
	else
		pFloor->AddCells( nFloorModelID, cells );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFloorsLayer::SetActiveCells( const CRect &rect, ITemplateView *pView )
{
	ASSERT( pView );
	ClearSelection();

	CPoint ptMin, ptMax;
  pView->ScreenToTemplate( &ptMin, CPoint( rect.left, rect.bottom ) );
	pView->ScreenToTemplate( &ptMax, CPoint( rect.right, rect.top ) );

	int i, j;
	for ( j = ptMin.y; j < ptMax.y; ++j )
		for ( i = ptMin.x; i < ptMax.x; ++i )
		{
			CFloorPlan::SCell cell;
			cell.x = i;
			cell.y = j;
			activeCells.push_back( cell );
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFloorsLayer::ClearSelection()
{
	activeCells.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// удаление активных ячеек
void CFloorsLayer::Delete()
{
	if ( !pPlacement )
		return;
  CFloorPlan *pFloor = pPlacement->GetFloor( floorType, theApp.GetActiveFloor(), nFloorLayer );
  if ( !pFloor )
    return;
	pFloor->DeleteCells( activeCells );
	ClearSelection();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFloorsLayer::DrawBrush( const CPoint &pt, ITemplateView *pView )
{
	const SBrush &br = GetBrush();
	if ( br.nItemID < 0 )
		return;
	CSize sz = GetSize( br.nItemID );	
	float x, y;

	pView->ScreenToTemplate( pt, &x, &y );
	CPoint ptR( x, y );
	int dl =pView->GetSpacing();
	pView->TemplateToScreen( &ptR, ptR );
	CRect r( ptR.x, ptR.y - dl * sz.cy, ptR.x + dl * sz.cx, ptR.y );
	CDC *pDC = pView->GetWnd()->GetDC();
	int nOldMode = pDC->SetROP2( R2_XORPEN );
	pDC->Rectangle( &rOldBrush );
	pDC->Rectangle( &r );
	rOldBrush = r;
	pDC->SetROP2( nOldMode );
	pView->GetWnd()->ReleaseDC( pDC );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CFloorsLayer::OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message, ITemplateView *pView )
{
	if ( hLayerCursor )
	{
		SetCursor( hLayerCursor );
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFloorsLayer::OnVisible() 
{
	NInput::PostEvent( "update" );
	NMainLoop::StepApp( true, true );
	CLayerCtrl::OnVisible();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
