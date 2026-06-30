#include "StdAfx.h"
#include "MapEdit.h"
#include "PropView.h"

extern bool bWYSIWYGActive;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPropView

BEGIN_MESSAGE_MAP(CPropView, SECControlBar)
//{{AFX_MSG_MAP(CPropView)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_MESSAGE( (WM_USER + 1), OnChangeProp )
	ON_WM_LBUTTONDOWN()
	ON_WM_NCLBUTTONDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CPropView construction/destruction

CPropView::CPropView() //: pPropMap( 0 )
{
}

CPropView::~CPropView()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPropView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
  if (SECControlBar::OnCreate(lpCreateStruct) == -1)
    return -1;
  
  // Init object inspector
  m_OIDlg.Create( IDD_OBJINSPECTOR, this );

  return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
LRESULT CPropView::OnChangeProp( WPARAM wParam, LPARAM lParam )
{
/*
   string szName = m_OIDlg.m_wndOI.GetPropertyName( wParam );
   CPropMap::const_iterator it = pPropMap->find( szName );
   if ( it == pPropMap->end() )
     return TRUE;
   it->second->SetValue( m_OIDlg.m_wndOI.GetPropertyValue( wParam ) );
*/
	 return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPropView::OnSize(UINT nType, int cx, int cy) 
{
  SECControlBar::OnSize(nType, cx, cy);
  
  if ( ::IsWindow( m_OIDlg.m_hWnd ) )
  {
    CRect rectInside;
    GetInsideRect(rectInside);
    ::SetWindowPos(m_OIDlg.m_hWnd, NULL, rectInside.left, rectInside.top,
      rectInside.Width(), rectInside.Height(),
      SWP_NOZORDER|SWP_NOACTIVATE);
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPropView::SetPropMap( int nTableID, const CPropMap *pPropMap )
{ 
	// если изменим pPropMap до ClearAll, то можем потерять последнее измененное значение
	m_OIDlg.SetPropMap( nTableID, pPropMap );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CPropView::GetActiveProp( int nGroupID )
{
  return m_OIDlg.GetActiveProp( nGroupID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPropView::UpdateProperty( int nPropID )
{
	m_OIDlg.UpdateProperty( nPropID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPropView::SetActiveProp( int nPropID )
{
	m_OIDlg.SetActiveProp( nPropID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPropView::OnLButtonDown(UINT nFlags, CPoint point)
{
	bWYSIWYGActive = false;
	SECControlBar::OnLButtonDown( nFlags, point );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPropView::OnNcLButtonDown(UINT nFlags, CPoint point)
{
	bWYSIWYGActive = false;
	SECControlBar::OnNcLButtonDown( nFlags, point );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
