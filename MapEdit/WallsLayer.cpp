#include "StdAfx.h"
#include "MapEdit.h"
#include "TemplateView.h"
#include "Layers.h"
#include "WallsLayer.h"
#include "Walls.h"
#include "Placement.h"
#include "dbDefs.h"
#include "ItemsMgr.h"
#include "..\Input\Bind.h"
#include "..\Main\iMain.h"

// “екущий активный этаж получаем у theApp

const float MAX_SELECTION_DIST = 1.0f;

inline POINT ToPOINT( const CTPoint<int> &pt ) { POINT p = { pt.x, pt.y }; return p; }

BEGIN_MESSAGE_MAP(CWallsLayer, CLayerCtrl)
//{{AFX_MSG_MAP(CWallsLayer)
	ON_COMMAND(ID_WALL_FLIP, OnWallFlip)
	ON_COMMAND(ID_DELETE_WALL, OnDeleteWall)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
CWallsLayer::CWallsLayer() : CBaseLayer( LID_WALLS, 0, "Walls", IDC_CONSTRUCTIONPARTS_TREE ), pPlacement( 0 )
{
	wallMenu.LoadMenu( IDR_WALL_MENU );
	hLayerCursor = ::CreateCursor( IDC_ARROW, IDC_WALL, 0.5f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWallsLayer::~CWallsLayer()
{
	wallMenu.DestroyMenu();
	if ( hLayerCursor )
		DestroyCursor( hLayerCursor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsLayer::BrowseBrush()
{
	CBaseLayer::BrowseBrush();
	SBrush br = GetBrush();
	if ( br.nSizeY > 0 )
	{
		MessageBox( "Only wall can be selected", "Warring", MB_OK | MB_ICONEXCLAMATION );
		activeBrush.nItemID = -1;
		Reset();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsLayer::OnLButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView )
{
	if ( !CanDraw() )
		return;
	Track( pView );
	pView->Repaint();

	const SWall *pWall = SelectWall( pt, pView );
	if ( pWall )
		pView->SelectedItem( IDC_CONSTRUCTIONPARTS_TREE, pWall->fr.nConstructionPartID, -1/*pWall->nID*/ );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsLayer::OnMouseMove(UINT nFlags, CPoint pt, ITemplateView *pView )
{
	if ( !CanDraw() )
		return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsLayer::OnRButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView )
{
	ASSERT( pView );
	CMenu *pMenu = wallMenu.GetSubMenu( 0 );
	if ( pMenu )
	{
		if ( IsSelectionEmpty() )
			SetActiveWall( pt, pView );
		pView->GetWnd()->ClientToScreen( &pt );
		pMenu->TrackPopupMenu( TPM_LEFTBUTTON, pt.x, pt.y, this );
	}	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsLayer::Reset()
{
	ClearColors();
	ClearSelection();
	CBaseLayer::Reset();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ¬озвратить указатель на активную планировку этажа
// ¬озвр. 0, если планировка не существует или ошибка
inline const CWallsPlan* CWallsLayer::GetFloor() const
{
  int nFloor = theApp.GetActiveFloor();
  
  if ( !pPlacement )
    return 0;
  return ((const CPlacement*)pPlacement)->GetFloorWalls( nFloor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ¬озвратить указатель на активную планировку этажа
// ¬озвр. 0, если планировка не существует  или ошибка
inline CWallsPlan* CWallsLayer::GetFloor()
{
  int nFloor = theApp.GetActiveFloor();
  
  if ( !pPlacement /*|| !pPlacement->HasWalls( nFloor )*/ )
    return 0;
  return pPlacement->GetFloorWalls( nFloor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsLayer::Paint( ITemplateView *pView, float fBrightness, bool bGrayed )
{
  ASSERT( pView );
	CDC *pDC = pView->GetPaintDC();
  CWallsPlan *pFloor = GetFloor();
  if ( !pFloor )
    return;
	pFloor->Update();
	int width = Max( 1, nWallWidth );
  CPen  pen( PS_SOLID, width, DEF_WALL_COLOR );
  CPen *pOldPen = pDC->SelectObject( &pen );

	COLORREF color;
  CPoint pt;
	//
  CPoint pt2;
  const_cast<CWallsPlan*>( pFloor )->MoveFirst();
	if ( 1 == width )
	{
		LOGBRUSH logBrush;
		logBrush.lbHatch = HS_BDIAGONAL;
		logBrush.lbStyle = BS_SOLID;
		DWORD patern[] = { 1, 1 };
		while ( const_cast<CWallsPlan*>( pFloor )->MoveNext() )
		{
			if ( !pFloor->GetWall() )
				continue;
			color = GetColor( pFloor->GetWall()->fr.nConstructionPartID, bGrayed );
			pDC->SelectObject( pOldPen );
			pen.DeleteObject();
			logBrush.lbColor = color;
			pen.CreatePen( PS_USERSTYLE, width, &logBrush, ARRAY_SIZE( patern ), patern );
			pDC->SelectObject( &pen );
			//
			int x, y;
			const SWall *pW = pFloor->GetWall();
			CPoint ptStart( pW->fr.ptPos.x, pW->fr.ptPos.y );
			CPoint ptEnd = ptStart + CWallsPlan::GetDirectionPt( pW );
			pView->TemplateToScreen( &pt, ptStart );
			pView->TemplateToScreen( &pt2, ptEnd );
			GetInternalWall( &x, &y, pt, pt2, width );
			pDC->MoveTo( pt.x + x, pt.y + y );
			pDC->LineTo( pt2.x + x, pt2.y + y );
		}
	}
	else
	{
		while ( const_cast<CWallsPlan*>( pFloor )->MoveNext() )
		{
			if ( !pFloor->GetWall() )
				continue;
			color = GetColor( pFloor->GetWall()->fr.nConstructionPartID, bGrayed );
			CBrush brush( HS_DIAGCROSS, color );
			//
			int x1, y1;
			int x2, y2;
			const SWall *pW = pFloor->GetWall();
			CPoint ptStart( pW->fr.ptPos.x, pW->fr.ptPos.y );
			CPoint ptEnd = ptStart + CWallsPlan::GetDirectionPt( pW );
			pView->TemplateToScreen( &pt, ptStart );
			pView->TemplateToScreen( &pt2, ptEnd );
			int dist = width - (width >> 1);
			GetInternalWall( &x1, &y1, pt, pt2, dist );
			GetInternalWall( &x2, &y2, pt, pt2, dist + width );
			CRect r( pt + CPoint( x1, y1 ), pt2 + CPoint( x2, y2 ) );
			pDC->FillRect( &r, &brush );
		}
	}
	//
  const_cast<CWallsPlan*>( pFloor )->MoveFirst();
  while ( const_cast<CWallsPlan*>( pFloor )->MoveNext() )
  {
		if ( !pFloor->GetWall() )
			continue;
		color = GetColor( pFloor->GetWall()->fr.nConstructionPartID, bGrayed );
		pDC->SelectObject( pOldPen );
		pen.DeleteObject();
		pen.CreatePen( PS_SOLID, width, color );
		pDC->SelectObject( &pen );

		const SWall *pW = pFloor->GetWall();
		CPoint ptStart( pW->fr.ptPos.x, pW->fr.ptPos.y );
		CPoint ptEnd = ptStart + CWallsPlan::GetDirectionPt( pW );

    pView->TemplateToScreen( &pt, ptStart );
    pDC->MoveTo( pt );
    pView->TemplateToScreen( &pt, ptEnd );
    pDC->LineTo( pt );
  }
  pDC->SelectObject( pOldPen );
	pen.DeleteObject();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool OnTheSameLine( const CPoint &pt1, const CPoint &pt2, const CPoint &pt3 )
{
  CPoint ptL = pt2 - pt1;
  CPoint ptX = pt3 - pt1;

  return ptX.x * ptL.y == ptX.y * ptL.x;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWallsLayer::WallMove( const CPoint &pt, ITemplateView *pView )
{
  CPoint ptTemp;

  pView->ScreenToTemplate( &ptTemp, pt );
  if ( CrossNearestBorder( &ptTemp, GetBrush().nSizeX ) )
  {
    ptLast = ptTemp;
    const int n = chain.size();
    if ( n > 1 && chain[n-2] == ptLast)
      chain.pop_back();
    else
      chain.push_back( ptTemp );
    return true;
  }
  return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsLayer::WallEnd( const CPoint &pt, ITemplateView *pView )
{
  ASSERT( pPlacement );

  if ( chain.size() < 2 )
    return;
  CWallsPlan *pFloor = pPlacement->GetFloorWalls( theApp.GetActiveFloor() );
  if ( !pFloor )
    return;
  CPoint ptStart = chain[0];
  ptLast = chain[1];
  vector<pair<CTPoint<int>, CTPoint<int> > > points;

  for ( int i = 2; i < chain.size(); ++i )
  {
    if ( OnTheSameLine( ptStart, ptLast, chain[i] ) )
    {
      ptLast = chain[i];
      continue;
    }
    pair<CTPoint<int>, CTPoint<int> > p( CTPoint<int>( ptStart.x, ptStart.y ), CTPoint<int>( ptLast.x, ptLast.y ) );
    points.push_back( p );
    ptStart = ptLast;
    ptLast = chain[ i];
  }
	pair<CTPoint<int>, CTPoint<int> > p( CTPoint<int>( ptStart.x, ptStart.y ), CTPoint<int>( ptLast.x, ptLast.y ) );
  points.push_back( p );
	const SBrush &wbrush = GetBrush();
  pFloor->AddWalls( wbrush.nItemID, points );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsLayer::DrawChain( CDC *pDC, ITemplateView *pView ) const
{
  if ( chain.empty() )
    return;
  int oldMode = pDC->SetROP2( R2_XORPEN );
  CPen pen( PS_SOLID, 2, RGB( 150, 155, 0 ) );
  CPen *pOldPen = pDC->SelectObject( &pen );

  CPoint pt;
  pView->TemplateToScreen( &pt, chain[0] );
  pDC->MoveTo( pt );
  for ( int i = 1; i < chain.size(); ++i )
  {
    pView->TemplateToScreen( &pt, chain[i] );
    pDC->LineTo( pt );
  }
  
  pDC->SelectObject( pOldPen );
  pDC->SetROP2( oldMode );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ќпределить, по€вилс€ ли новый кусок в цепочке элементов стен
// pPt - координата ptEnd нового элемента сенки
// nLength - длина элемента стенки
bool CWallsLayer::CrossNearestBorder( CPoint *pPt, int nLength )
{
	if ( 0 == nLength )
		return false;
  if ( ptLast.x != pPt->x && 0 == ( ptLast.x - pPt->x ) % nLength )
  {
    pPt->y = ptLast.y;
    return true;
  }
  if ( ptLast.y != pPt->y && 0 == ( ptLast.y - pPt->y ) % nLength )
  {
    pPt->x = ptLast.x;
    return true;
  }
  return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// вектор стенки: pt1 - начальна€ точка, pt2 - конечна€
// внутренн€€ стенка справа
void CWallsLayer::GetInternalWall( int *pX, int *pY, const CPoint &pt1, const CPoint &pt2, int nWidth ) const
{
  // ось у направлена вниз при рисовании
  if ( pt1.x != pt2.x )
  {
    *pX = 0;
    if ( pt1.x < pt2.x )
      *pY = nWidth;
    else
      *pY = -nWidth;
    return;
  }
  // не должно быть диагональных стенок
  ASSERT( pt1.y != pt2.y );
  *pY = 0;
  if ( pt1.y < pt2.y )
    *pX = -nWidth;
  else
    *pX = nWidth;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ¬озвр. true, если была удалена хот€ бы одна активна€ стенка
bool CWallsLayer::Delete()
{
  if ( activeWalls.empty() )
    return false;
  CWallsPlan *pFloor = GetFloor();
  if ( !pFloor )
    return false;
  bool ret = false;
  for ( int i = 0; i < activeWalls.size(); ++i )
    if ( pFloor->DeleteWall( activeWalls[i] ) )
      ret = true;
		activeWalls.clear();
		return ret;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsLayer::SetPlacement( CPlacement *pPl )
{
  pPlacement = pPl;
  ClearSelection();
  chain.clear();
	ClearColors();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SWall* CWallsLayer::SelectWall( const CPoint &pt, ITemplateView *pView )
{
  const CWallsPlan *pFloor = GetFloor();
  if ( !pFloor )
    return 0;
  
  float x, y;
  pView->ScreenToTemplate( pt, &x, &y );
  
  return pFloor->GetNearestWall( CVec2( x, y ), MAX_SELECTION_DIST );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsLayer::SetActiveWall( const CPoint &pt, ITemplateView *pView )
{
  ClearSelection();
  
  const SWall *pWall = SelectWall( pt, pView );
  if ( pWall )
    activeWalls.push_back( pWall );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// "¬ывернуть наизнанку" активные стенки
bool CWallsLayer::Flip()
{
  CWallsPlan *pFloor = GetFloor();
  if ( !pFloor )
    return false;
  for ( int i = 0; i < activeWalls.size(); ++i )
  {
		pFloor->Flip( activeWalls[i] );
  }
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// ѕолучить цвет стенки
// если цвет дл€ данной модели неизвестен, то он читаетс€ из базы
COLORREF CWallsLayer::GetColor( int nWallModelID, bool bGrayed  ) const
{
	unordered_map<int, DWORD>::const_iterator it = colorMap.find( nWallModelID );
	COLORREF color;

	if ( colorMap.end() == it )
	{
		const SResTree *pTree = theApp.GetResTree( IDC_CONSTRUCTIONPARTS_TREE );
		if ( !pTree || !pTree->pItemsTree )
		{
			const_cast<CWallsLayer*>(this)->colorMap[nWallModelID] = DEF_WALL_COLOR;
			return DEF_WALL_COLOR;
		}
		const CPropMap *pProps = pTree->pItemsTree->GetPropList( nWallModelID );
		if ( !pProps )
		{
			const_cast<CWallsLayer*>(this)->colorMap[nWallModelID] = DEF_WALL_COLOR;
			return DEF_WALL_COLOR;
		}
		CPropMap::const_iterator itc = pProps->find( "UserColor" );
		if ( pProps->end() == itc )
		{
			const_cast<CWallsLayer*>(this)->colorMap[nWallModelID] = DEF_WALL_COLOR;
			return DEF_WALL_COLOR;
		}
		color = (int)itc->second->GetValue();
		const_cast<CWallsLayer*>(this)->colorMap[nWallModelID] = color;
		pTree->pItemsTree->ReleasePropList( pProps );
	}
	else
		color = it->second;
	// 
	if ( bGrayed )
	{
		DWORD mean = 0.333f * ( GetRValue( color ) + GetGValue( color ) + GetBValue( color ) );
		color = RGB( mean, mean, mean );
	}
	return color;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsLayer::ClearColors()
{
	colorMap.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWallsLayer::Track( ITemplateView *pView )
{
	if ( !pPlacement )
		return false;
	CWnd *pWnd = pView->GetWnd();

	// don't handle if capture already set
	if (::GetCapture() != NULL)
		return false;		
	// set capture to the window which received this message
	pWnd->SetCapture();
		
	// 
	CPoint ptClip( pPlacement->GetWidth(), 0 );
	CPoint ptClipLT( 0, pPlacement->GetHeight() );
	pView->TemplateToScreen( &ptClip, ptClip );
	pView->TemplateToScreen( &ptClipLT, ptClipLT );
	CRect rClip( ptClipLT, ptClip );
	rClip.InflateRect( 1, 0, 1, 1 );
	pWnd->ClientToScreen( &rClip );
	::ClipCursor( &rClip );
		
	CPoint pt;
	
	GetCursorPos( &pt );
	pWnd->ScreenToClient( &pt );
  chain.clear();
  pView->ScreenToTemplate( &ptLast, pt );
  chain.push_back( ptLast );
	
	// get DC for drawing
	CDC* pDC = pWnd->GetDC();
	ASSERT_VALID(pDC);
		
	// get messages until capture lost or cancelled/accepted
	for (;;)
	{
		MSG msg;
		VERIFY(::GetMessage(&msg, NULL, 0, 0));
		
		if ( CWnd::GetCapture() != pWnd )
		{
			pWnd->ReleaseDC(pDC);
			::ClipCursor( 0 );
			chain.clear();
			return false;
		}
		if ( WM_LBUTTONUP == msg.message )
			break;
		
		switch (msg.message)
		{
			// handle movement/accept messages
		case WM_MOUSEMOVE:
			pt.y = (int)(short)HIWORD(msg.lParam);
			pt.x = (int)(short)LOWORD(msg.lParam);
			DrawChain( pDC, pView );
			WallMove( pt, pView );
			DrawChain( pDC, pView );
			break;
			// handle cancel messages
		case WM_KEYDOWN:
			if (msg.wParam != VK_ESCAPE)
				break;
		case WM_RBUTTONDOWN:
			pWnd->ReleaseDC(pDC);
			ReleaseCapture();
			::ClipCursor( 0 );
			chain.clear();
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
	WallEnd( pt, pView );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsLayer::Selection( const CRect &rect, ITemplateView *pView )
{
	ClearSelection();
	CWallsPlan *pFloor = GetFloor();
  if ( !pFloor )
    return;
  
  float xmin, ymin;
	float xmax, ymax;
  pView->ScreenToTemplate( CPoint( rect.left, rect.bottom ), &xmin, &ymin );
	pView->ScreenToTemplate( CPoint( rect.right, rect.top ), &xmax, &ymax );
  
	pFloor->MoveFirst();
	while ( pFloor->MoveNext() ) 
	{
		const SWall *pWall = pFloor->GetWall();
		const CPoint ptStart( pWall->fr.ptPos.x, pWall->fr.ptPos.y );
		const CPoint ptEnd = ptStart + CWallsPlan::GetDirectionPt( pWall );
		if ( ptStart.x > xmin && ptStart.x < xmax && ptStart.y > ymin && ptStart.y < ymax &&
				 ptEnd.x > xmin   && ptEnd.x < xmax     && ptEnd.y > ymin && ptEnd.y < ymax 
			 )
			activeWalls.push_back( pWall );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsLayer::ClearSelection()
{
	activeWalls.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsLayer::OnWallFlip() 
{
	if ( Flip() )
		Repaint();
	ClearSelection();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsLayer::OnDeleteWall()
{
	if ( Delete() )
		Repaint();
	ClearSelection();	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWallsLayer::OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message, ITemplateView *pView )
{
	if ( hLayerCursor )
	{
		SetCursor( hLayerCursor );
		return true;
	}
	return false;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWallsLayer::OnVisible() 
{
	CLayerCtrl::OnVisible();
	NInput::PostEvent( "update" );
	NMainLoop::StepApp( true, true );
}
