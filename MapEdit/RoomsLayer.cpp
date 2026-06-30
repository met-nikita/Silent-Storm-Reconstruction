#include "StdAfx.h"
#include "MapEdit.h"
#include "Layers.h"
#include "RoomsLayer.h"
#include "Placement.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
CRoomsLayer::CRoomsLayer() : CFloorsLayer( FT_ROOM, LID_ROOMS, "Rooms", -1, 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CRoomsLayer::~CRoomsLayer()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRoomsLayer::BrowseBrush()
{
	if ( !pPlacement )
		return;
	CRoomDlg dlg( pPlacement, this );

	dlg.nSelectedRoomID = activeBrush.nItemID;
	if ( IDOK != dlg.DoModal() )
		return;
	SRoom room;
	pPlacement->GetRoom( &room, theApp.GetActiveFloor(), dlg.nSelectedRoomID );
	activeBrush.nTreeID = -1;
	activeBrush.color = room.dwColor;
	activeBrush.nSizeX = 1;
	activeBrush.nSizeY = 1;
	activeBrush.fThickness = 0.1f;
	activeBrush.nItemID = dlg.nSelectedRoomID;
	SetBrush( activeBrush.color, IToA( activeBrush.nItemID ).c_str() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRoomsLayer::Reset()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRoomDlg dialog
CRoomDlg::CRoomDlg(CPlacement *pPl, CWnd* pParent /*=NULL*/)
	: CDialog(CRoomDlg::IDD, pParent)
{
	ASSERT( pPl );
	pPlacement = pPl;
	nSelectedRoomID = -1;
	//{{AFX_DATA_INIT(CRoomDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	m_list.SetPlacement( pPlacement );
}


void CRoomDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CRoomDlg)
	DDX_Control(pDX, IDC_ROOM_LIST, m_list);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CRoomDlg, CDialog)
	//{{AFX_MSG_MAP(CRoomDlg)
	ON_BN_CLICKED(IDC_NEWROOM, OnNewRoom)
	ON_LBN_DBLCLK(IDC_ROOM_LIST, OnDblclkRoomList)
	ON_BN_CLICKED(IDC_ROOM_COLOR, OnRoomColor)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CRoomDlg message handlers
void CRoomDlg::OnNewRoom() 
{
	int id = pPlacement->AddRoom( theApp.GetActiveFloor(), RGB( 0, 0, 0 ) );
	if ( id > 0 )
	{
		int i = m_list.AddString( IToA( id ).c_str() );
		m_list.SetItemData( i, id );
		m_list.SetCurSel( i );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CRoomDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	vector<SRoom> rooms;
	pPlacement->GetRoomParams( theApp.GetActiveFloor(), &rooms );
	
	for ( int i = 0; i < rooms.size(); ++i )
	{
		int ind = m_list.AddString( IToA( rooms[i].nRoomID ).c_str() );
		m_list.SetItemData( ind, rooms[i].nRoomID );
		if ( nSelectedRoomID == rooms[i].nRoomID )
			m_list.SetCurSel( ind );
	}
	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRoomDlg::OnOK()
{
	int ind = m_list.GetCurSel();
	if ( LB_ERR != ind )
		nSelectedRoomID = m_list.GetItemData( ind );
	CDialog::OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRoomDlg::OnDblclkRoomList() 
{
	OnOK();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRoomListBox::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct) 
{
	if ( !pPlacement )
		return;
	ASSERT(lpDrawItemStruct->CtlType == ODT_LISTBOX);
	int nItemID = lpDrawItemStruct->itemID;
	if ( nItemID < 0 )
		return;
	CString str = IToA( lpDrawItemStruct->itemData ).c_str();
	CDC dc;
	
	dc.Attach(lpDrawItemStruct->hDC);
	
	// Save these value to restore them when done drawing.
	COLORREF crOldTextColor = dc.GetTextColor();
	COLORREF crOldBkColor = dc.GetBkColor();
	
	 SRoom room;
	 COLORREF crBk;
	 if ( !pPlacement->GetRoom( &room, theApp.GetActiveFloor(), lpDrawItemStruct->itemData ) )
		 crBk = RGB( 0, 0, 0 );
	 else
		 crBk = room.dwColor;
	 //
	 dc.FillSolidRect( &lpDrawItemStruct->rcItem, crBk );
	 dc.SetTextColor( RGB( 255, 255, 255 ) );
	 dc.SetBkColor( crBk );
   // If this item is selected, set the background color 
   // and the text color to appropriate values. Also, erase
   // rect by filling it with the background color.
   if ((lpDrawItemStruct->itemAction | ODA_SELECT) &&
		 (lpDrawItemStruct->itemState & ODS_SELECTED))
   {
		 CPen pen( PS_SOLID, 1, RGB( 255, 255, 255) );
		 CBrush brush( crBk );
		 CPen *pPenOld = dc.SelectObject( &pen );
		 CBrush *pBrushOld = dc.SelectObject( &brush );
		 dc.Rectangle( &lpDrawItemStruct->rcItem );
		 dc.SelectObject( pPenOld );
		 dc.SelectObject( pBrushOld );
   }
	 
   // Draw the text.
   dc.DrawText( str, &lpDrawItemStruct->rcItem, DT_CENTER|DT_SINGLELINE|DT_VCENTER);
	 
   // Reset the background color and the text color back to their
   // original values.
   dc.SetTextColor(crOldTextColor);
   dc.SetBkColor(crOldBkColor);
	 
   dc.Detach();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRoomListBox::CompareItem(LPCOMPAREITEMSTRUCT lpCompareItemStruct) 
{
	ASSERT(lpCompareItemStruct->CtlType == ODT_LISTBOX);
	int nID1 = lpCompareItemStruct->itemID1;
	int nID2 = lpCompareItemStruct->itemID2;
	
	if ( nID1 < 0 || nID2 < 0 )
		return 0;
	CString str1, str2;
	GetText( nID1, str1 );
	GetText( nID2, str2 );
		
	return str1.Compare( str2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRoomDlg::OnRoomColor() 
{
	int ind = m_list.GetCurSel();
	if ( LB_ERR == ind )
		return;
	int id = m_list.GetItemData( ind );
	SRoom r;
	pPlacement->GetRoom( &r, theApp.GetActiveFloor(), id );
	
	CColorDialog dlg( r.dwColor );
	dlg.m_cc.Flags |= CC_FULLOPEN;
	if ( IDOK != dlg.DoModal() )
		return;
	pPlacement->SetRoomColor( theApp.GetActiveFloor(), id, dlg.GetColor() );	
	Invalidate();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
