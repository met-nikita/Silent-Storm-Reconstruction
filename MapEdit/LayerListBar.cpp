#include "StdAfx.h"
#include "MapEdit.h"
#include "LayerList.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// CLayerListBar dialog
////////////////////////////////////////////////////////////////////////////////////////////////////
CLayerListBar::CLayerListBar(CWnd* pParent /*=NULL*/)
	: CDialog(CLayerListBar::IDD, pParent)
{
	//{{AFX_DATA_INIT(CLayerListBar)
	m_floor_1 = FALSE;
	m_floor_2 = FALSE;
	m_floor0 = FALSE;
	m_floor1 = FALSE;
	m_floor2 = FALSE;
	m_floor3 = FALSE;
	m_floor4 = FALSE;
	//}}AFX_DATA_INIT
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerListBar::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLayerListBar)
	DDX_Control(pDX, IDC_LAYER_DEL, m_del);
	DDX_Control(pDX, IDC_LAYER_ADD, m_add);
	DDX_Control(pDX, IDC_LAYER_UP, m_up);
	DDX_Control(pDX, IDC_LAYER_DOWN, m_down);
	DDX_Check(pDX, IDC_CHECK_FLOOR_1, m_floor_1);
	DDX_Check(pDX, IDC_CHECK_FLOOR_2, m_floor_2);
	DDX_Check(pDX, IDC_CHECK_FLOOR0, m_floor0);
	DDX_Check(pDX, IDC_CHECK_FLOOR1, m_floor1);
	DDX_Check(pDX, IDC_CHECK_FLOOR2, m_floor2);
	DDX_Check(pDX, IDC_CHECK_FLOOR3, m_floor3);
	DDX_Check(pDX, IDC_CHECK_FLOOR4, m_floor4);
	//}}AFX_DATA_MAP
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CLayerListBar, CDialog)
	//{{AFX_MSG_MAP(CLayerListBar)
	ON_BN_CLICKED(IDC_LAYER_DOWN, OnLayerDown)
	ON_BN_CLICKED(IDC_LAYER_UP, OnLayerUp)
	ON_BN_CLICKED(IDC_LAYER_ADD, OnLayerAdd)
	ON_BN_CLICKED(IDC_LAYER_DEL, OnLayerDel)
	ON_BN_CLICKED(IDC_CHECK_FLOOR_1, OnCheckFloor)
	ON_BN_CLICKED(IDC_CHECK_FLOOR_2, OnCheckFloor)
	ON_BN_CLICKED(IDC_CHECK_FLOOR0, OnCheckFloor)
	ON_BN_CLICKED(IDC_CHECK_FLOOR1, OnCheckFloor)
	ON_BN_CLICKED(IDC_CHECK_FLOOR2, OnCheckFloor)
	ON_BN_CLICKED(IDC_CHECK_FLOOR3, OnCheckFloor)
	ON_BN_CLICKED(IDC_CHECK_FLOOR4, OnCheckFloor)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CLayerListBar message handlers
BOOL CLayerListBar::OnInitDialog() 
{
	CDialog::OnInitDialog();
		
	m_up.SetIcon( theApp.LoadIcon( IDI_UP ) );
	m_down.SetIcon( theApp.LoadIcon( IDI_DOWN ) );
	m_del.SetIcon( theApp.LoadIcon( IDI_LAYER_DEL ) );
	m_add.SetIcon( theApp.LoadIcon( IDI_LAYER_ADD ) );

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerListBar::OnLayerDown() 
{
	GetParent()->PostMessage( WM_USER_LAYERDOWN, -1);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerListBar::OnLayerUp() 
{
	GetParent()->PostMessage( WM_USER_LAYERUP, -1);	
}
////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CLayersPlace, CWnd)
//{{AFX_MSG_MAP(CLayersPlace)
ON_WM_PAINT()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayersPlace::OnPaint() 
{	
	CRect r;
	CPaintDC dc( this );

	GetClientRect( &r );
	dc.FillSolidRect( &r, GetSysColor( COLOR_BTNFACE ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerListBar::OnLayerAdd() 
{
	CWnd *pWnd = GetParent();
	if ( pWnd )
		pWnd->PostMessage( WM_USER_NOTIFY, LLN_ADD );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerListBar::OnLayerDel() 
{
	CWnd *pWnd = GetParent();
	if ( pWnd )
		pWnd->PostMessage( WM_USER_NOTIFY, LLN_DEL );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
inline BOOL& CLayerListBar::Floor( int nFloor )
{
	switch ( nFloor )
	{
		case -2:
			return m_floor_2;
		case -1:
			return m_floor_1;
		case 0:
			return m_floor0;
		case 1:
			return m_floor1;
		case 2:
			return m_floor2;
		case 3:
			return m_floor3;
		case 4:
			return m_floor4;
		default:
			ASSERT(0);
			return m_floor_2;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLayerListBar::OnCheckFloor() 
{
	UpdateData();
	CWnd *pWnd = GetParent();
	if ( pWnd )
		pWnd->PostMessage( WM_USER_FLOORLINK );
}
