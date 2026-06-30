// CtrlObjectInspector.cpp : implementation file
//

#include "stdafx.h"
#include "WinUser.h"
#include "CtrlObjectInspector.h"
#include "OIEdit.h"
#include "OICombo.h"
#include "OIBrowEdit.h"
#include "OIColorEdit.h"
#include "OIRelEdit.h"
#include "OISubPartsEdit.h"
#include "OIList.h"
#include "OIParamsEdit.h"
#include "OIPiecesEdit.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

enum
{
	SUB_CTRL_EDIT   = 1100,
	SUB_CTRL_COMBO  = 1101,
	SUB_CTRL_BUTTON = 1102,
  SUB_CTRL_BEDIT  = 1103,
	SUB_CTRL_CEDIT  = 1104,
	SUB_CTRL_REDIT  = 1105,
	SUB_CTRL_SEDIT  = 1106,
	SUB_CTRL_LIST   = 1107,
	SUB_CTRL_PARAMS = 1108,
	SUB_CTRL_PIECES = 1109,
};

const string STR_TRUE  = "true";
const string STR_FALSE = "false";
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCtrlObjectInspector

CCtrlObjectInspector::CCtrlObjectInspector() 
  : m_pEdit( new COIExEdit ), m_pCombo( new COICombo ), 
	  m_pBEdit( new COIBrowseEdit ), m_pCEdit( new COIColorEdit ), 
		m_pREdit( new COIRelEdit ), m_pSEdit( new COISubPartsEdit ),
		m_pRellist( new COIRelList ), m_pParams( new COIParamsEdit ),
		m_pPiecesEdit( new COIGeometryPiecesEdit )
{
	m_nFirstElem = 0;
	m_nLineHeight = -1;
	m_nCurVirtualLine = -1;
	m_nCurGroup = GroupDefault;
  pActiveWnd = 0;
	bDrawZeroGroup = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CCtrlObjectInspector::~CCtrlObjectInspector()
{
  if ( m_pEdit )  delete m_pEdit;
  if ( m_pCombo ) delete m_pCombo;
  if ( m_pBEdit ) delete m_pBEdit;
	if ( m_pCEdit ) delete m_pCEdit;
	if ( m_pREdit)  delete m_pREdit;
	if ( m_pSEdit ) delete m_pSEdit;
	if ( m_pRellist ) delete m_pRellist;
	if ( m_pParams ) delete m_pParams;
	if ( m_pPiecesEdit ) delete m_pPiecesEdit;
  const_cast<COIExEdit*>( m_pEdit ) = 0;
  const_cast<COIBrowseEdit*>( m_pBEdit ) = 0;
  const_cast<COICombo*>( m_pCombo ) = 0;
	const_cast<COIColorEdit*>( m_pCEdit ) = 0;
	const_cast<COIRelEdit*>( m_pREdit ) = 0;	
	const_cast<COISubPartsEdit*>( m_pSEdit ) = 0;	
	const_cast<COIRelList*>( m_pRellist ) = 0;	
}


BEGIN_MESSAGE_MAP(CCtrlObjectInspector, CWnd)
	//{{AFX_MSG_MAP(CCtrlObjectInspector)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_SIZE()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_KILLFOCUS()
	ON_WM_SETFOCUS()
	ON_WM_VSCROLL()
	ON_WM_CLOSE()
	ON_WM_NCLBUTTONDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


////////////////////////////////////////////////////////////////////////////////////////////////////
// CCtrlObjectInspector message handlers
int CCtrlObjectInspector::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	Init();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CRect ScaleRect( const CRect &rect, int nHowMore )
{
	return CRect( rect.left - nHowMore, rect.top - nHowMore, rect.right + nHowMore, rect.bottom + nHowMore );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRect CCtrlObjectInspector::GetPaintColPartRect( int nPaintLine, int nCol )
{
	if ( nCol )
		return CRect( m_nSplitterPos, nPaintLine * m_nLineHeight, m_sizeClient.cx, (nPaintLine+1) * m_nLineHeight );
	else
		return CRect( m_nLineHeight, nPaintLine * m_nLineHeight, m_nSplitterPos, (nPaintLine+1) * m_nLineHeight );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRect CCtrlObjectInspector::GetTextColPartRect( int nPaintLine, int nCol )
{
	CRect rect = GetPaintColPartRect( nPaintLine, nCol );
	rect.top+=1;
	rect.bottom+=1;
	return rect;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRect CCtrlObjectInspector::GetPaintLineRect( int nPaintLine )
{
	return CRect( 0, nPaintLine * m_nLineHeight, m_sizeClient.cx, (nPaintLine+1) * m_nLineHeight );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::DrawPlus( CDC* pDC, int nLine, bool isPlus )
{
	CRect rect = GetPlusRect(nLine);
	pDC->Rectangle(rect);
	CPoint ptCenter = rect.CenterPoint();
	int nTall = rect.Width() / 2 - 2;
	pDC->MoveTo( ptCenter.x - nTall, ptCenter.y );
	pDC->LineTo( ptCenter.x + nTall + 1, ptCenter.y );
	if ( isPlus )
	{
		pDC->MoveTo( ptCenter.x, ptCenter.y - nTall );
		pDC->LineTo( ptCenter.x, ptCenter.y + nTall + 1 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::OnPaint()
{
  ASSERT( m_pEdit );
  ASSERT( m_pBEdit );
  ASSERT( m_pCombo );

  CPaintDC paintDC(this); // device context for painting
  CDC dc; // back buffer DC
	CBitmap bmp;	
  dc.CreateCompatibleDC( &paintDC );

	CRect rectClient;
	GetClientRect( rectClient );
  bmp.CreateCompatibleBitmap( &paintDC, rectClient.Width(), rectClient.Height() );
	CBitmap *pOldBitmap = dc.SelectObject( &bmp );
	dc.FillSolidRect( 0, 0, rectClient.Width(), rectClient.Height(), GetSysColor( COLOR_BTNFACE ) );

	TEXTMETRIC sTextMetrics;
	dc.GetTextMetrics(&sTextMetrics);

	CFont *pOldFont = dc.SelectObject( &m_fntDef );
	dc.SetBkMode( TRANSPARENT );

	// Draw elements
	int nNumber = 1;
	for( CCOIPaintElemVector::const_iterator it = m_aPaintElems.begin() + m_nFirstElem; it != m_aPaintElems.end() && nNumber <= GetLineCount(); ++it, ++nNumber )
	{
		CRect rect = GetPaintLineRect(nNumber);
		rect.bottom = rect.top + 1;
		dc.FillSolidRect( rect, RGB(160,160,160) );

		const SCOIPaintElem &elem = *it;
		if ( elem.pProp )
		{
      const SCOIProperties &prop = *elem.pProp;
      // first col
      rect = GetTextColPartRect( nNumber, 0 );
			dc.DrawText( prop.strName.c_str(), prop.strName.length(), rect, DT_LEFT );

			// second col
			rect = GetTextColPartRect( nNumber, 1 );
			string strVal = prop.varValue;
			if ( DT_COLOR != prop.idDomen )
				dc.DrawText( strVal.c_str(), strVal.length(), rect, DT_RIGHT );
			else
			{
				CBrush brush( atoi( strVal.c_str() ) );
				CRect  r = rect;
				r.DeflateRect( 5, 1, 3, 1 );
				if ( "" != strVal )
					dc.FillRect( &r, &brush );
			}
			const SCOIGroup &group = m_mapGroups[prop.idGroup];
      if ( group.bRadioGroup && group.aPorops.size() > 1 && group.iActiveProp == prop.idProp )
      {
        CBrush brush( RGB( 170, 170, 50 ) );
        CBrush *pBrush = dc.SelectObject( &brush );
        CRect rect = GetPlusRect( nNumber );
        dc.Ellipse( &rect );
        dc.SelectObject( pBrush );
      }
		}
		else
		{
			const SCOIGroup &group = *elem.pGroup;
			DrawPlus( &dc, nNumber, !group.isExpand );
			rect = GetTextColPartRect( nNumber, 0 );
			dc.SelectObject( &m_fntDefBold );
			dc.DrawText( group.strGroupName.c_str(), group.strGroupName.length(), rect, DT_LEFT );
			dc.SelectObject( &m_fntDef );
		}
	}
	CRect rect = GetPaintLineRect( VirtualToPaintLine(m_nCurVirtualLine) );
	rect.left -= N_BORDER;
	rect.right += N_BORDER * 2;
	rect.bottom++;
	if ( m_haveFocus )
		dc.DrawEdge( rect, BDR_SUNKENOUTER, BF_RECT );

	// Draw vertical
	rect = CRect( m_nSplitterPos - 1, 0, m_nSplitterPos + 1, m_sizeClient.cy + 2 );
	dc.DrawEdge( rect, EDGE_ETCHED, BF_RECT );

	// Draw caption
	rect = GetPaintColPartRect( 0, 0 );
	rect.left = 0;
	rect.bottom += 1;
	dc.DrawEdge( rect, EDGE_RAISED, BF_RECT ); // EDGE_ETCHED
	rect.left += 4;
	rect.top++;
	dc.DrawText( "Properties", rect, DT_LEFT );

	rect = GetPaintColPartRect( 0, 1 );
	rect.bottom += 1;
	dc.DrawEdge( rect, EDGE_RAISED, BF_RECT );
	rect.left += 4;
	rect.top++;
	dc.DrawText( "Value", rect, DT_LEFT );
	
	dc.SelectObject(pOldFont);

  paintDC.BitBlt( 0, 0, rectClient.Width(), rectClient.Height(), &dc, 0, 0, SRCCOPY );
  dc.SelectObject( pOldBitmap );
  bmp.DeleteObject();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);
	m_sizeClient.cx = cx;
	m_sizeClient.cy = cy;
	MakePaintList();
	UpdateScrollers();
  if ( pActiveWnd )
  {
    CRect rect;
    pActiveWnd->GetWindowRect( &rect );
    ScreenToClient( &rect );
    rect.left = m_nSplitterPos;
    rect.right = m_sizeClient.cx;
    
    pActiveWnd->MoveWindow( &rect );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	OnLButtonDown( nFlags, point );
	CWnd::OnLButtonDblClk(nFlags, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::ExpandGroup( bool needInverse, bool isExpand )
{
	SCOIPaintElem *pElem = GetVirtualElem( m_nCurVirtualLine );
	if ( !( pElem && pElem->pGroup ) )
		return;
	if ( needInverse )
		pElem->pGroup->isExpand = !pElem->pGroup->isExpand;
	else
		pElem->pGroup->isExpand = isExpand;
	MakePaintList();
	Invalidate( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::CollapseGroup( GroupID idGroup )
{
	CCOIGpoupMap::iterator it = m_mapGroups.find( idGroup );
	if ( m_mapGroups.end() == it )
		return;
	it->second.isExpand = false;
	MakePaintList();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CCtrlObjectInspector::GetNumProps( GroupID idGroup ) const
{
	CCOIGpoupMap::const_iterator it = m_mapGroups.find( idGroup );
	if ( m_mapGroups.end() == it )
		return 0;
	return it->second.aPorops.size();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::SelectRow( int nVirtualLine, bool needHide )
{
	if ( nVirtualLine >= m_aPaintElems.size() )
		nVirtualLine = -1;
	//DebugTrace( "SelectRow nVirt=%d\n", nVirtualLine );
//		return;
//  if ( nVirtualLine < 0 )
//    return;
  
  SCOIPaintElem *pOldElem = GetVirtualElem( m_nCurVirtualLine );
  if ( pOldElem && pOldElem->pProp && !(pOldElem->pProp->idDomen != DT_RELATION && pOldElem->pProp->bReadOnly) )
  {
    if ( pActiveWnd )
      pActiveWnd->ShowWindow( SW_HIDE );
		MSG msg;
		while( PeekMessage( &msg, m_hWnd, WM_USER + 2, WM_USER + 2, PM_REMOVE ) )
			;
    switch( pOldElem->pProp->idDomen )
    {
			case DT_DEC:
			case DT_HEX:
			case DT_STR:
			case DT_PARAMS:
			case DT_GEOMETRYPIECES:
				{
          if ( !pActiveWnd )
            break;
					CString szTemp;
					pActiveWnd->GetWindowText(szTemp);
          CVariant oldVal = pOldElem->pProp->varValue;
					if ( szTemp == "" )
						pOldElem->pProp->varValue = CVariant();
					else
						pOldElem->pProp->varValue.SetNewValue( szTemp.operator LPCTSTR() );
          if ( oldVal != pOldElem->pProp->varValue )
          {
            GetParent()->PostMessage( WM_USER + 1, pOldElem->pProp->idProp );
          }
				}
				break;
      case DT_BROWSE:
        {
          CString szTemp;
          m_pBEdit->GetWindowText(szTemp);
          CVariant oldVal = pOldElem->pProp->varValue;
          pOldElem->pProp->varValue.SetNewValue( szTemp.operator LPCTSTR() );
          if ( oldVal != pOldElem->pProp->varValue )
          {
            GetParent()->PostMessage( WM_USER + 1, pOldElem->pProp->idProp );
          }
        }
        break;
      case DT_COLOR:
        {
          CString szTemp;
          m_pCEdit->GetWindowText(szTemp);
          CVariant oldVal = pOldElem->pProp->varValue;
          pOldElem->pProp->varValue.SetNewValue( szTemp.operator LPCTSTR() );
          if ( oldVal != pOldElem->pProp->varValue )
          {
            GetParent()->PostMessage( WM_USER + 1, pOldElem->pProp->idProp );
          }
        }
        break;
      case DT_SUBPARTS:
        {
          CString szTemp;
          m_pSEdit->GetWindowText(szTemp);
          CVariant oldVal = pOldElem->pProp->varValue;
          pOldElem->pProp->varValue.SetNewValue( szTemp.operator LPCTSTR() );
          if ( oldVal != pOldElem->pProp->varValue )
          {
            GetParent()->PostMessage( WM_USER + 1, pOldElem->pProp->idProp );
          }
        }
        break;
      case DT_RELATION:
        {
					if ( !pOldElem->pProp->bReadOnly && m_pREdit->HasNewValue() )
           GetParent()->PostMessage( WM_USER + 1, pOldElem->pProp->idProp, m_pREdit->GetItemID() );
        }
        break;
      case DT_COMBO:
      case DT_BOOL:
        {
          int nSel = m_pCombo->GetCurSel();
          if ( CB_ERR == nSel )
            break;
          CString szTemp;
          m_pCombo->GetLBText( nSel, szTemp );
          CVariant oldVal = pOldElem->pProp->varValue;
          pOldElem->pProp->varValue.SetNewValue( szTemp.operator LPCTSTR() );
          if ( oldVal != pOldElem->pProp->varValue )
          {
            GetParent()->PostMessage( WM_USER + 1, pOldElem->pProp->idProp );
          }
        }
        break;
      case DT_RELLIST:
        {
					//if ( m_pRellist )
        }
				break;
        
			default:
				//???
				break;
		}
	}
	SCOIPaintElem *pElem = GetVirtualElem( nVirtualLine );
	if ( !pElem || needHide /*|| VirtualToPaintLine(nVirtualLine) == 0*/ )
		return;
	//DebugTrace( "ShowLine\n" );
	if ( pElem->pProp )
		SetActiveProp( pElem->pProp->idProp );
	if ( pElem->pProp || pElem->pGroup )
	{
		if ( pElem->pProp && !(pElem->pProp->idDomen != DT_RELATION && pElem->pProp->bReadOnly) )
		{
			//DebugTrace( "Switch\n" );
			switch( pElem->pProp->idDomen )
			{
				case DT_DEC:
				case DT_HEX:
				case DT_STR:
					{
						//DebugTrace( "DT_DEC\n" );
						if ( pElem->pProp->strName == "CodeText" )
							m_pEdit->SetCheckSyntax( true );
						else
							m_pEdit->SetCheckSyntax( false );
						m_pEdit->SetWindowText( pElem->pProp->varValue );
						m_pEdit->ShowWindow( SW_SHOW );
						CRect rect = GetTextColPartRect( VirtualToPaintLine(nVirtualLine), 1 );
						rect.top--;
						m_pEdit->MoveWindow( rect );
						m_pEdit->SetFocus();
            pActiveWnd = m_pEdit;
					}
					break;
        case DT_BROWSE:
          {
            m_pBEdit->SetDirPrefix( pElem->pProp->szPrefix.c_str() );
            m_pBEdit->SetWindowText( pElem->pProp->varValue );
            m_pBEdit->ShowWindow( SW_SHOW );
            CRect rect = GetTextColPartRect( VirtualToPaintLine(nVirtualLine), 1 );
            rect.top--;
            m_pBEdit->MoveWindow( rect );
            m_pBEdit->SetFocus();
            pActiveWnd = m_pBEdit;
          }
          break;
        case DT_COLOR:
          {
            m_pCEdit->SetWindowText( pElem->pProp->varValue );
            m_pCEdit->ShowWindow( SW_SHOW );
            CRect rect = GetTextColPartRect( VirtualToPaintLine(nVirtualLine), 1 );
            rect.top--;
            m_pCEdit->MoveWindow( rect );
            m_pCEdit->SetFocus();
            pActiveWnd = m_pCEdit;
          }
          break;
        case DT_SUBPARTS:
          {
            m_pSEdit->SetWindowText( pElem->pProp->varValue );
						m_pSEdit->SetItemID( nItemID );
            m_pSEdit->ShowWindow( SW_SHOW );
            CRect rect = GetTextColPartRect( VirtualToPaintLine(nVirtualLine), 1 );
            rect.top--;
            m_pSEdit->MoveWindow( rect );
            m_pSEdit->SetFocus();
            pActiveWnd = m_pSEdit;
          }
          break;
        case DT_RELATION:
          {
						m_pREdit->SetReadOnly( pElem->pProp->bReadOnly );
            m_pREdit->SetWindowText( pElem->pProp->varValue );
						int nSelection = pElem->pProp->varShadowVal.GetType() == CVariant::VT_NULL ? -1 : pElem->pProp->varShadowVal;
						m_pREdit->SetTableItemIDs( pElem->pProp->nRelation, nSelection );
            CRect rect = GetTextColPartRect( VirtualToPaintLine(nVirtualLine), 1 );
            rect.top--;
            m_pREdit->MoveWindow( rect );
            m_pREdit->ShowWindow( SW_SHOW );
            m_pREdit->SetFocus();
            pActiveWnd = m_pREdit;
          }
          break;
        case DT_BOOL:
        case DT_COMBO:
          {
            m_pCombo->ResetContent();
            for ( int i = 0; i < pElem->pProp->szStrs.size(); ++i )
            {
              const string &szItem = pElem->pProp->szStrs[i];
              int n = m_pCombo->AddString( szItem.c_str() );
              if ( szItem == (string)pElem->pProp->varValue )
                m_pCombo->SetCurSel( n );
            }
            m_pCombo->ShowWindow( SW_SHOW );
            CRect rect = GetTextColPartRect( VirtualToPaintLine(nVirtualLine), 1 );
            rect.top--;
            rect.bottom -= 2;
            m_pCombo->MoveWindow( rect );
            m_pCombo->SetItemHeight( -1, rect.Height() );
            m_pCombo->SetFocus();
            pActiveWnd = m_pCombo;
          }
          break;
        case DT_RELLIST:
          {
            m_pRellist->SetList( pElem->pProp->pListProps );
            CRect rect = GetTextColPartRect( VirtualToPaintLine(nVirtualLine), 1 );
            rect.top--;
            m_pRellist->MoveWindow( rect );
            m_pRellist->ShowWindow( SW_SHOW );
            m_pRellist->SetFocus();
            pActiveWnd = m_pRellist;
          }
					break;
				case DT_PARAMS:
					{
						CRect rect = GetTextColPartRect( VirtualToPaintLine(nVirtualLine), 1 );
						rect.top--;
						m_pParams->SetWindowText( pElem->pProp->varValue );
						m_pParams->MoveWindow( rect );
						m_pParams->ShowWindow( SW_SHOW );
						m_pParams->SetFocus();
						pActiveWnd = m_pParams;
					}
					break;
				case DT_GEOMETRYPIECES:
					{
						CRect rect = GetTextColPartRect( VirtualToPaintLine(nVirtualLine), 1 );
						rect.top--;
						m_pPiecesEdit->SetWindowText( pElem->pProp->varValue );
						m_pPiecesEdit->MoveWindow( rect );
						m_pPiecesEdit->ShowWindow( SW_SHOW );
						m_pPiecesEdit->SetFocus();
						m_pPiecesEdit->SetAIGeometryID( nItemID );
						pActiveWnd = m_pPiecesEdit;
					}
					break;
        default:
					//???
					break;
			}
		}
		m_nCurVirtualLine = nVirtualLine;
		Invalidate( FALSE );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::ProcessKeyInput( UINT nChar )
{
	switch( nChar )
	{
		case VK_LEFT:
			ExpandGroup( false, false );
			break;

		case VK_RIGHT:
			ExpandGroup();
			break;

		case VK_DOWN:
		case VK_RETURN:
			if ( m_nCurVirtualLine == m_nFirstElem + GetLineCount() - 1 && m_nCurVirtualLine < m_aPaintElems.size() - 1 )
				UpdateScrollers( m_nCurVirtualLine + 1 );
			SelectRow( m_nCurVirtualLine + 1 );
			break;

		case VK_UP:
			if ( m_nCurVirtualLine == m_nFirstElem && m_nCurVirtualLine > 0 )
				UpdateScrollers( m_nCurVirtualLine - 1 );
			SelectRow( m_nCurVirtualLine - 1 );
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::OnLButtonDown( UINT nFlags, CPoint point )
{
	SetFocus();
	int nPaintLine = GetPaintLine(point);
	int nVirtualLine = PaintLineToVirtual(nPaintLine);
	int nCol = GetCol(point);
	if ( nPaintLine ==  0)
		return; // CRAP

	SCOIPaintElem *pElem = GetVirtualElem( nVirtualLine );
	CRect rectPlus = GetPlusRect( nPaintLine );
	SelectRow( nVirtualLine );
  if ( pElem && pElem->pProp && m_mapGroups[pElem->pProp->idGroup].bRadioGroup )
  {
    m_mapGroups[pElem->pProp->idGroup].iActiveProp = pElem->pProp->idProp;
  }
  if ( pElem && pElem->pGroup && !pElem->pProp && rectPlus.PtInRect(point) )
		ExpandGroup( true );
	CWnd::OnLButtonDown(nFlags, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CCtrlObjectInspector::PreTranslateMessage( MSG* pMsg )
{
	if ( pMsg->message == WM_USER + 1 )
	{
		ProcessKeyInput(pMsg->wParam);
		return true;
	}
	else if ( pMsg->message == WM_USER_LOST_FOCUS )
	{
		LooseFocus();
		return true;
	}
	else if ( pMsg->message == WM_KEYDOWN )
	{
		switch (pMsg->wParam)
		{
			case VK_LEFT:
			case VK_RIGHT:
				if ( GetFocus() != this )
					break;
			case VK_DOWN:
			case VK_RETURN:
			case VK_UP:
				ProcessKeyInput(pMsg->wParam);
				return true;
			case VK_ESCAPE:
			{
				CWnd *pPrnt = GetParent();
				if ( pPrnt )
					pPrnt->PostMessage( pMsg->message, pMsg->wParam, pMsg->lParam );
			}
		}
	}
	else if ( pMsg->message == WM_MOUSEMOVE )
	{
		CPoint pt = pMsg->pt;
		ScreenToClient( &pt );
		int nPaintLine = GetPaintLine( pt );
		if ( nPaintLine >  0 )
		{
			int nVirtualLine = PaintLineToVirtual( nPaintLine );

			SCOIPaintElem *pElem = GetVirtualElem( nVirtualLine );
			if ( pElem && pElem->pProp )
			{
				m_ToolTips.RelayEvent( pMsg );
				CString strProp = pElem->pProp->szToolTip.c_str();
				CString str;
				m_ToolTips.GetText( str, this, 1 );
				if ( str != strProp )
				{
					m_ToolTips.UpdateTipText( strProp, this, 1 );
					m_ToolTips.Pop();
				}
			}
			else
				m_ToolTips.Pop();
		}
	}
	return CWnd::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::LooseFocus()
{
	SelectRow( m_nCurVirtualLine, true );
	Invalidate( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::OnKillFocus(CWnd* pNewWnd)
{
	CWnd::OnKillFocus(pNewWnd);
	m_haveFocus = ( pNewWnd == pActiveWnd );
	if ( !m_haveFocus )
		LooseFocus();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::OnSetFocus(CWnd* pOldWnd)
{
	CWnd::OnSetFocus(pOldWnd);
//	if ( !m_haveFocus )
//		SelectRow( m_nCurVirtualLine );
	m_haveFocus = true;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::Init()
{
	m_nLineHeight = 15;
	if ( !m_fntDef.m_hObject )
	{
		// Create font
		LOGFONT lf;
		memset(&lf, 0, sizeof(LOGFONT));			// zero out structure
		lf.lfHeight = 15;							// request a ?-pixel-height font
		strcpy( lf.lfFaceName, "MS Sans Serif" );	// request a face name "Arial", "Courier", "MS Sans Serif"
		m_fntDef.CreateFontIndirect(&lf);			// create the fonts
		lf.lfWeight = FW_BOLD;
		m_fntDefBold.CreateFontIndirect(&lf);
	}

	CRect rect, rectClient;
	// Create edit
	//m_pEdit->Create( WS_CHILD | ES_WANTRETURN | ES_MULTILINE | ES_LEFT | ES_AUTOHSCROLL, rect, this, SUB_CTRL_EDIT );
	//m_pEdit->ModifyStyleEx( 0, WS_EX_STATICEDGE );
	//m_pEdit->SetFont( &m_fntDef );
	m_pEdit->Create( 0, "Ex Edit", WS_CHILD, rect, this, SUB_CTRL_EDIT );

  // Create browse edit
  m_pBEdit->Create( 0, "Browse Edit", WS_CHILD, rect, this, SUB_CTRL_BEDIT );
	m_pCEdit->Create( 0, "Color Edit", WS_CHILD, rect, this, SUB_CTRL_CEDIT );
	m_pREdit->Create( 0, "Relation Edit", WS_CHILD, rect, this, SUB_CTRL_REDIT );
	m_pSEdit->Create( 0, "SubParts Edit", WS_CHILD, rect, this, SUB_CTRL_SEDIT );
	m_pRellist->Create( 0, "List Edit", WS_CHILD, rect, this, SUB_CTRL_LIST );
	m_pParams->Create( 0, "List Edit", WS_CHILD, rect, this, SUB_CTRL_PARAMS );
	m_pPiecesEdit->Create( 0, "Pieces Edit", WS_CHILD, rect, this, SUB_CTRL_PIECES );
//  m_pBEdit->ModifyStyleEx( 0, WS_EX_STATICEDGE );
  
	// Create combo
  rect.SetRectEmpty();
  rect.right = 80;
  rect.bottom = 220;
//	m_pCombo->Create( WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | CBS_SORT | CBS_NOINTEGRALHEIGHT | WS_VSCROLL, rect, this, SUB_CTRL_COMBO );
  m_pCombo->Create( CBS_DROPDOWNLIST | WS_CHILD | CBS_SORT | WS_VSCROLL | CBS_NOINTEGRALHEIGHT, rect, this, SUB_CTRL_COMBO );
//	m_pCombo->ModifyStyleEx( 0, WS_EX_STATICEDGE );
	m_pCombo->SetFont( &m_fntDef );
  m_pCombo->SetItemHeight( 0, m_nLineHeight - 1 );  

	// Tooltips
	CRect r;
	GetClientRect( &r );
	r.top += 2 * m_nLineHeight;
	m_ToolTips.Create( this );
	m_ToolTips.AddTool( this, "Tip", CRect( 0, 0, 10, 10 ), 1 );
	m_ToolTips.SetDelayTime( TTDT_AUTOPOP, 5000 );
	m_ToolTips.SetDelayTime( TTDT_INITIAL, 200 );
	m_ToolTips.SetDelayTime( TTDT_RESHOW, 100 );   
	m_ToolTips.Activate( true );

	GetClientRect( rectClient );
//	m_nSplitterPos = 11 * rectClient.Width() / 20;
  m_nSplitterPos = m_nLineHeight;
  CDC *pDC = GetDC();
  if ( pDC )
  {
    CSize sz = pDC->GetTextExtent( "Properties", strlen( "Properties" ) );
    m_nSplitterPos = max( m_nSplitterPos, (int)sz.cx + m_nLineHeight );
    ReleaseDC( pDC );
  }
	m_sizeClient = rectClient.Size();

	GetWindowRect( rect );
	rect.bottom = rect.top + m_nLineHeight * GetLineCount() + rect.Height() - rectClient.Height();
	MoveWindow( rect );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// 

bool CCtrlObjectInspector::IsValidDomen( DomenID idDomen )
{
	if ( DT_DEC <= idDomen && idDomen < DT_CUSTOM )
		return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::SetGroup( GroupID idGroup, const string strName, bool bRadioGroup )
{
	m_mapGroups[m_nCurGroup = idGroup].strGroupName = strName;
  m_mapGroups[idGroup].bRadioGroup = bRadioGroup;
  m_mapGroups[idGroup].iActiveProp = 0;
  CDC *pDC = GetDC();
  if ( pDC && (idGroup != 0 || bDrawZeroGroup) )
  {
    CSize sz = pDC->GetTextExtent( strName.c_str(), strlen( strName.c_str() ) );
    m_nSplitterPos = max( m_nSplitterPos, (int)sz.cx + m_nLineHeight );
    ReleaseDC( pDC );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::Update()
{
  MakePaintList();
  Invalidate( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCtrlObjectInspector::AddPropertiesValue( PropID idProp, DomenID idDomen, const string &strName, 
                      const CVariant &var, GroupID idGroup, const string &szToolTip, bool bReadOnly, 
											const char *szDirPrefix, CVariant varShadowVal, bool bRepaint )
{
	if ( IsValidDomen(idDomen) && m_mapProps.find(idProp) == m_mapProps.end() )
	{
		SCOIProperties prop;
    prop.idProp  = idProp;
		prop.idDomen = idDomen;
		prop.idGroup = idGroup;
		prop.strName = strName;
		prop.varValue = var;
		prop.varShadowVal = varShadowVal;
    prop.bReadOnly = bReadOnly;
    prop.szPrefix  = szDirPrefix;
		prop.nRelation = -1;
		prop.szToolTip = szToolTip;
    if ( DT_BOOL == idDomen )
    {
      prop.szStrs.push_back( STR_TRUE );
      prop.szStrs.push_back( STR_FALSE );
      prop.varValue = var ? STR_TRUE : STR_FALSE;
    }
		m_mapProps[idProp] = prop;
		if ( idGroup == GroupDefault )
			idGroup = m_nCurGroup;
		m_mapGroups[idGroup].aPorops.push_back( &m_mapProps[idProp] );
    if ( m_mapGroups[idGroup].bRadioGroup && m_mapGroups[idGroup].aPorops.size() == 1 )
		{
			// делаем активным первый элемент в радио-группе
      m_mapGroups[idGroup].iActiveProp = idProp;
		}
    CDC *pDC = GetDC();
    if ( pDC )
    {
      CSize sz = pDC->GetTextExtent( strName.c_str(), strlen( strName.c_str() ) );
      m_nSplitterPos = max( m_nSplitterPos, (int)sz.cx + m_nLineHeight );
      ReleaseDC( pDC );
    }
		if ( bRepaint )
			Update();
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::SetListProperties( PropID idProp, CListProp *pProp )
{
	if ( m_mapProps.find(idProp) != m_mapProps.end() )
	{
		m_mapProps[idProp].pListProps = pProp;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::ClearAll()
{
	if ( m_mapProps.empty() )
		return;
	SelectRow( m_nCurVirtualLine, true );
  m_mapProps.clear();
  m_mapGroups.clear();
  m_nCurVirtualLine = -1;
	m_nFirstElem = 0;
	m_nCurGroup = GroupDefault;
  pActiveWnd = 0;
  if ( pActiveWnd )
    pActiveWnd->ShowWindow( SW_HIDE );
  m_pBEdit->SetDirPrefix( "" );
  MakePaintList();
  Invalidate( FALSE );
	UpdateWindow();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCtrlObjectInspector::SetPropertiesValue( PropID idProp, const CVariant &var, const CVariant shadowVal )
{
	if ( m_mapProps.find(idProp) != m_mapProps.end() )
	{
		CVariant oldValue = m_mapProps[idProp].varValue;
		m_mapProps[idProp].varValue = var;
		m_mapProps[idProp].varShadowVal = shadowVal;
		if ( DT_BOOL == m_mapProps[idProp].idDomen )
      m_mapProps[idProp].varValue = var ? STR_TRUE : STR_FALSE;
		if ( oldValue != m_mapProps[idProp].varValue )
		{
			SCOIPaintElem *pElem = GetVirtualElem( m_nCurVirtualLine );
			if ( pElem && pElem->pProp && pElem->pProp->idProp == idProp )
			{
				// если редактируемое значение == idProp, то не сохраняем результаты редактирования
				if ( pActiveWnd )
					pActiveWnd->ShowWindow( SW_HIDE );
				m_nCurVirtualLine = -1;
			}
			else
				SelectRow( m_nCurVirtualLine );
			Invalidate( FALSE );
		}
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::MakePaintList()
{
	m_aPaintElems.clear();
	for( CCOIGpoupMap::iterator itGr = m_mapGroups.begin(); itGr != m_mapGroups.end(); ++itGr )
	{
		SCOIGroup &curGroup = itGr->second;
		if ( curGroup.isVisible )
		{
			SCOIPaintElem elem;
			elem.pGroup = &curGroup;
			if ( itGr->first != 0 || bDrawZeroGroup )
				m_aPaintElems.push_back(elem);
			if ( curGroup.isExpand )
			{
				for( CCOIPropPtrs::const_iterator itPr = curGroup.aPorops.begin(); itPr != curGroup.aPorops.end(); ++itPr )
				{
					elem.pProp = *itPr;
					m_aPaintElems.push_back(elem);
				}
			}
		}
	}
	UpdateScrollers();
	//
	CRect r;
	GetClientRect( &r );
	r.top += 2 * m_nLineHeight;
	r.bottom = Min( (int)r.bottom, (int)((m_aPaintElems.size() + 1) * m_nLineHeight) );
	r.right = m_nSplitterPos;
	m_ToolTips.SetToolRect( this, 1, &r );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::UpdateScrollers( int nFirstVirtualLine )
{
	const int nElems = m_aPaintElems.size();

	// CRAP как узнать виден скрол бар или нет ?
	CWnd *pSWnd = GetScrollBarCtrl( SB_VERT );
	if ( pSWnd && ::IsWindow( pSWnd->m_hWnd ) )
	{
		if ( !pSWnd->IsWindowVisible() && nElems * m_nLineHeight < m_sizeClient.cy )
			return;
	}
	SCROLLINFO yScroll;
	memset( &yScroll, 0, sizeof(yScroll) );

	yScroll.cbSize = sizeof(yScroll);
	yScroll.fMask = SIF_ALL;
	yScroll.nMax = m_aPaintElems.size() - 1;
	yScroll.nMin = 0;
	yScroll.nPage = GetLineCount();
	if ( nFirstVirtualLine != -1 )
		m_nFirstElem = nFirstVirtualLine;

	if ( m_nFirstElem > m_aPaintElems.size() - GetLineCount() )
		m_nFirstElem = m_aPaintElems.size() - GetLineCount();
	if ( m_nFirstElem < 0 )
		m_nFirstElem = 0;

	yScroll.nPos = yScroll.nTrackPos = m_nFirstElem;
	SetScrollInfo( SB_VERT, &yScroll );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CCtrlObjectInspector::PreCreateWindow(CREATESTRUCT& cs) 
{
	CWnd::PreCreateWindow(cs);
	cs.style |= WS_VSCROLL | WS_EX_STATICEDGE;
//	cs.dwExStyle |= WS_EX_WINDOWEDGE; //WS_EX_CLIENTEDGE;
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), 0, NULL);
	
	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	SCROLLINFO	yScroll;
	memset( &yScroll, 0, sizeof SCROLLINFO );
	yScroll.cbSize = sizeof SCROLLINFO;
	yScroll.fMask = SIF_ALL;
	GetScrollInfo( SB_VERT, &yScroll );

	switch( nSBCode )
	{
		case SB_LINELEFT:
			UpdateScrollers( m_nFirstElem - 1 );
			break;
		case SB_LINERIGHT:
			UpdateScrollers( m_nFirstElem + 1 );
			break;

		case SB_PAGELEFT:
			UpdateScrollers( m_nFirstElem - int(yScroll.nPage) );
			break;
		case SB_PAGERIGHT:
			UpdateScrollers( m_nFirstElem + yScroll.nPage );
			break;

		case SB_THUMBTRACK:
			yScroll.nPos = yScroll.nTrackPos;

		case SB_THUMBPOSITION:
		case SB_ENDSCROLL:
			UpdateScrollers( yScroll.nPos );
			break;
	}
	SelectRow( m_nCurVirtualLine );
	Invalidate( FALSE );
	CWnd::OnVScroll(nSBCode, nPos, pScrollBar);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVariant CCtrlObjectInspector::GetPropertyValue( PropID idProp )
{
  CCOIPropMap::const_iterator it = m_mapProps.find(idProp);

  if ( it != m_mapProps.end() )
  {
    const SCOIProperties &prop = it->second;
    if ( DT_BOOL == prop.idDomen )
      return STR_TRUE == (string)prop.varValue ? true : false;
    return it->second.varValue;
  }
  return CVariant();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string CCtrlObjectInspector::GetPropertyName( PropID idProp )
{
  CCOIPropMap::const_iterator it = m_mapProps.find(idProp);
  
  if ( it != m_mapProps.end() )
  {
    return it->second.strName;
  }
  return "";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
PropID CCtrlObjectInspector::GetActiveProp( int nGroupID )
{
  CCOIGpoupMap::const_iterator it = m_mapGroups.find( nGroupID );

  if ( it == m_mapGroups.end() || !it->second.bRadioGroup )
    return -1;
  return it->second.iActiveProp;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::AddPropertyString( PropID idProp, const string &szStr )
{
  CCOIPropMap::iterator it = m_mapProps.find(idProp);
  
  if ( it != m_mapProps.end() )
    it->second.szStrs.push_back( szStr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::SetActiveProp( PropID nID )
{
	CCOIPropMap::const_iterator it = m_mapProps.find( nID );
	if ( m_mapProps.end() == it )
		return;
	GroupID idGroup = it->second.idGroup;
	CCOIGpoupMap::iterator itg = m_mapGroups.find( idGroup );
	if ( m_mapGroups.end() == itg )
		return;
	itg->second.iActiveProp = nID;
	Invalidate( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string CCtrlObjectInspector::GetGroupName( GroupID idGroup ) const
{
	CCOIGpoupMap::const_iterator it = m_mapGroups.find( idGroup );
	if ( m_mapGroups.end() == it )
		return "";
	else
		return it->second.strGroupName;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::SetPropertyRelation( PropID idProp, int nRelationID )
{
	CCOIPropMap::iterator it = m_mapProps.find( idProp );
	if ( m_mapProps.end() == it )
		return;
	it->second.nRelation = nRelationID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::OnClose() 
{
	m_fntDef.DeleteObject();
	m_fntDefBold.DeleteObject();	
	CWnd::OnClose();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
extern bool bWYSIWYGActive; // CRAP
void CCtrlObjectInspector::OnNcLButtonDown(UINT nFlags, CPoint point)
{
	bWYSIWYGActive = false;
	CWnd::OnNcLButtonDown( nFlags, point );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCtrlObjectInspector::DrawZeroGroupName( bool bDraw )
{
	bDrawZeroGroup = bDraw;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CCtrlObjectInspector::GetPropertyLine( PropID idProp )
{
	int i = 0;
	for ( CCOIPaintElemVector::const_iterator it = m_aPaintElems.begin(); it != m_aPaintElems.end(); ++it, ++i )
	{
		const SCOIPaintElem &e = *it;
		if ( !e.pProp )
			continue;
		if ( e.pProp->idProp == idProp )
			return i;
	}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
