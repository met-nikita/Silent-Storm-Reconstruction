// ParamsView.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "PiecesInfoView.h"
#include "..\Misc\StrProc.h"
#include "..\Main\BuildingClip.h"
#include "..\Main\GResource.h"
#include "PieceEditDlg.h"
#include "AIGeometryFormat.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int IDC_PIECESINFO_CTRL = 100;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPiecesInfoView

CPiecesInfoView::CPiecesInfoView()
{
	nAIGeometryID = -1;
}

CPiecesInfoView::~CPiecesInfoView()
{
}

void CPiecesInfoView::SetAIGeometry( int nID )
{
	nAIGeometryID = nID;
}

BEGIN_MESSAGE_MAP(CPiecesInfoView, CWnd)
	//{{AFX_MSG_MAP(CPiecesInfoView)
	ON_NOTIFY(NM_CLICK, IDC_PIECESINFO_CTRL, OnClickParamlist)	
	ON_NOTIFY(NM_DBLCLK, IDC_PIECESINFO_CTRL, OnDblClickParamlist)	
	ON_NOTIFY(LVN_KEYDOWN, IDC_PIECESINFO_CTRL, OnLvnKeydownList)
	ON_WM_CREATE()
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

const int N_JUNCS = 6;
char* vszJuncsNames[N_JUNCS] =
{
	"+X",
	"+Y",
	"+Z",
	"-X",
	"-Y",
	"-Z",
};
int N_START_JUNCS_COLUMN = 0;
int N_START_COORDS_COLUMN = 0;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPiecesInfoView message handlers
const int COL_WIDTH = 30;
int CPiecesInfoView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	if ( nAIGeometryID < 0 )
	{
		ASSERT(0);
		return -1;
	}

	CRect rect;

	GetClientRect( &rect );

	DWORD dwStyle = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER;

	m_wndList.Create( dwStyle, rect, this, IDC_PIECESINFO_CTRL );
	m_wndList.SetExtendedStyle( LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT );
	m_wndList.GetClientRect( &rect );
	// вставляем первую колонку
	LV_COLUMN  column;
	memset( &column, 0, sizeof( LV_COLUMN) );
	column.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	column.pszText = "N#";
	column.cx = 40;
	column.iSubItem = 0;
	m_wndList.InsertColumn( column.iSubItem, &column );

	column.pszText = "HashID";
	column.cx = 60;
	m_wndList.InsertColumn( ++column.iSubItem, &column );
	//
	column.pszText = "PartX";
	column.cx = 47;
	m_wndList.InsertColumn( ++column.iSubItem, &column );
	N_START_COORDS_COLUMN = column.iSubItem;
	//
	column.pszText = "PartY";
	m_wndList.InsertColumn( ++column.iSubItem, &column );
	//
	column.pszText = "PartZ";
	m_wndList.InsertColumn( ++column.iSubItem, &column );
	//
	column.pszText = "X";
	column.cx = 30;
	m_wndList.InsertColumn( ++column.iSubItem, &column );
	//
	column.pszText = "Y";
	m_wndList.InsertColumn( ++column.iSubItem, &column );
	//
	column.pszText = "Z";
	m_wndList.InsertColumn( ++column.iSubItem, &column );
	//
	N_START_JUNCS_COLUMN = column.iSubItem + 1;
	for ( int i = 0; i < N_JUNCS; ++i )
	{
		column.pszText = vszJuncsNames[i];
		column.iSubItem = N_START_JUNCS_COLUMN + i;
		m_wndList.InsertColumn( N_START_JUNCS_COLUMN + i, &column );
	}
	//
	try
	{
		NGScene::CResourceOpener file( "AIGeometries", nAIGeometryID );
		CStoredPieceMap fpieces;

		file->Add( N_PIECES_CHUNK, &fpieces );
		for ( CStoredPieceMap::const_iterator i = fpieces.begin(); i != fpieces.end(); ++i )
		{
			const SStoredPiece &p = i->second;
			int x,y,z;
			NBuilding::GetPieceCoords( i->first, &x, &y, &z );
			CVec3 ptCenter(x-1,y-1,z-1);
			SPieceInfo info;
			info.bEditable = false;
			for ( int j = 0; j < p.juncs.size(); ++j )
			{
				const NAI::SJunction &junc = p.juncs[j];
				CVec3 pt = junc.pt - ptCenter;
				if ( pt.x > 0 )
					info.juncs[0] = true;
				else if ( pt.y > 0 )
					info.juncs[1] = true;
				else if ( pt.z > 0 )
					info.juncs[2] = true;
				else if ( pt.x < 0 )
					info.juncs[3] = true;
				else if ( pt.y < 0 )
					info.juncs[4] = true;
				else if ( pt.z < 0 )
					info.juncs[5] = true;
				else
					ASSERT(0);
			}
			AddRecord( i->first, info );
		}
		szOriginalPieces = GetAdditionalPieces( true );
	}
	catch (...)
	{
	}
	//
	vector<string> vszPieces;
	NStr::SplitString( szAdditionalPices, vszPieces, ';' );
	for ( int i = 0; i < vszPieces.size(); ++i )
	{
		vector<string> vszLinks;
		NStr::SplitString( vszPieces[i], vszLinks, ',' );
		if ( vszLinks.size() != 2 )
		{
			ASSERT( vszLinks.size() == 1 );
			continue;
		}
		SPieceInfo info;
		info.bEditable = true;
		int nPieceHashID = atoi( vszLinks[0].c_str() );
		BYTE data = atoi( vszLinks[1].c_str() );
		if ( data & 0x1 ) info.juncs[0] = true;
		if ( data & 0x2 ) info.juncs[1] = true;
		if ( data & 0x4 ) info.juncs[2] = true;
		if ( data & 0x8 ) info.juncs[3] = true;
		if ( data & 0x10 ) info.juncs[4] = true;
		if ( data & 0x20 ) info.juncs[5] = true;
		AddRecord( nPieceHashID, info );
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPiecesInfoView::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);

	m_wndList.MoveWindow( 0, 0, cx, cy );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPiecesInfoView::OnClickParamlist(NMHDR* pNMHDR, LRESULT* pResult)
{
	//ToggleFlag( (NM_LISTVIEW*)pNMHDR );
	*pResult = 0;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPiecesInfoView::OnDblClickParamlist(NMHDR* pNMHDR, LRESULT* pResult)
{
	EditRecord( (NM_LISTVIEW*)pNMHDR );
	*pResult = 0;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static int DlgToPieceInfo( SPieceInfo *pInfo, const CPieceEditDlg &dlg )
{
	int nNewHash = NBuilding::GetPartHashID( dlg.m_nPartX, dlg.m_nPartY, dlg.m_nPartZ );
	nNewHash |= NBuilding::GetPieceHashID( dlg.m_nX, dlg.m_nY, dlg.m_nZ );
	pInfo->juncs[0] = dlg.m_bx;
	pInfo->juncs[1] = dlg.m_by;
	pInfo->juncs[2] = dlg.m_bz;
	pInfo->juncs[3] = dlg.m_b_x;
	pInfo->juncs[4] = dlg.m_b_y;
	pInfo->juncs[5] = dlg.m_b_z;
	return nNewHash;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPiecesInfoView::EditRecord( NM_LISTVIEW *pNMListView )
{
//	if ( pNMListView->iSubItem == 0 )
//		return;

	CPoint ptAction = pNMListView->ptAction;
	ptAction.x = 4;
	int iItem = m_wndList.HitTest( ptAction );
	DWORD nData = -1;
	CPieceEditDlg dlg;

	if ( iItem != -1 )
	{
		nData = m_wndList.GetItemData( iItem );
		unordered_map<int, SPieceInfo>::iterator it = pieces.find( nData );
		if ( it == pieces.end() )
			return;
		SPieceInfo &info = it->second;
		if ( !info.bEditable )
			return;
		//
		int px, py, pz;
		int x, y, z;
		NBuilding::GetPartCoords( nData, &px, &py, &pz );
		NBuilding::GetPieceCoords( nData, &x, &y, &z );

		dlg.m_nPartX = px;
		dlg.m_nPartY = py;
		dlg.m_nPartZ = pz;
		dlg.m_nX = x;
		dlg.m_nY = y;
		dlg.m_nZ = z;
		dlg.m_bx = info.juncs[0];
		dlg.m_by = info.juncs[1];
		dlg.m_bz = info.juncs[2];
		dlg.m_b_x = info.juncs[3];
		dlg.m_b_y = info.juncs[4];
		dlg.m_b_z = info.juncs[5];
	}
	//
	if ( dlg.DoModal() != IDOK )
		return;
	SPieceInfo sNewInfo;
	int nNewHash = DlgToPieceInfo( &sNewInfo, dlg );
	if ( nNewHash == 0 )
		return;

	if ( iItem == -1)
	{
		sNewInfo.bEditable = true;
		AddRecord( nNewHash, sNewInfo );
	}
	else 
	{
		if ( nNewHash != nData )
			pieces.erase( pieces.find( nData ) ); // уже проверено, что элемент существует
		UpdateItem( iItem, nNewHash, sNewInfo );
	}
	pieces[nNewHash] = sNewInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPiecesInfoView::UpdateItem( int nItem, int nHashID, const SPieceInfo &info )
{
	m_wndList.SetItemData( nItem, nHashID );
	m_wndList.SetItemText( nItem, 0, IToA( nItem + 1 ).c_str() );
	string str = info.bEditable ? "[" : "";
	str += IToA( nHashID );
	if ( info.bEditable )
		str += ']';
	m_wndList.SetItemText( nItem, 1, str.c_str() );

	int px, py, pz;
	int x, y, z;
	NBuilding::GetPartCoords( nHashID, &px, &py, &pz );
	NBuilding::GetPieceCoords( nHashID, &x, &y, &z );
	m_wndList.SetItemText( nItem, N_START_COORDS_COLUMN, IToA( px ).c_str() );
	m_wndList.SetItemText( nItem, N_START_COORDS_COLUMN + 1, IToA( py ).c_str() );
	m_wndList.SetItemText( nItem, N_START_COORDS_COLUMN + 2, IToA( pz ).c_str() );
	m_wndList.SetItemText( nItem, N_START_COORDS_COLUMN + 3, IToA( x ).c_str() );
	m_wndList.SetItemText( nItem, N_START_COORDS_COLUMN + 4, IToA( y ).c_str() );
	m_wndList.SetItemText( nItem, N_START_COORDS_COLUMN + 5, IToA( z ).c_str() );
	for ( int i = 0; i < N_JUNCS; ++i )
		if ( info.juncs[i] )
			m_wndList.SetItemText( nItem, N_START_JUNCS_COLUMN + i, "+" );
		else
			m_wndList.SetItemText( nItem, N_START_JUNCS_COLUMN + i, "-" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPiecesInfoView::AddRecord( int nHashID, const SPieceInfo &info )
{
	pieces[nHashID] = info;
	int nItem = m_wndList.InsertItem( m_wndList.GetItemCount(), "" );
	UpdateItem( nItem, nHashID, info );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPiecesInfoView::OnLvnKeydownList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLVKEYDOWN pLVKeyDow = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);

	if ( pLVKeyDow->wVKey == VK_DELETE )
	{
		POSITION pos = m_wndList.GetFirstSelectedItemPosition();
		if (pos == NULL)
			TRACE0("No items were selected!\n");
		else
		{
			int nItem = m_wndList.GetNextSelectedItem(pos);
			int nData = m_wndList.GetItemData( nItem );
			unordered_map<int, SPieceInfo>::iterator it = pieces.find( nData );
			if ( it != pieces.end() && it->second.bEditable )
			{
				pieces.erase( it );
				m_wndList.DeleteItem( nItem );
			}
		}
	}
	else if ( pLVKeyDow->wVKey == VK_INSERT )
	{
		CPieceEditDlg dlg;
		if ( dlg.DoModal() == IDOK )
		{
			SPieceInfo sNewInfo;
			int nNewHash = DlgToPieceInfo( &sNewInfo, dlg );
			if ( nNewHash != 0 )
				AddRecord( nNewHash, sNewInfo );
		}
	}
	*pResult = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string CPiecesInfoView::GetAdditionalPieces( bool bOriginal /* = false  */ )
{
	string szRet;
	for ( unordered_map<int, SPieceInfo>::iterator it = pieces.begin(); it != pieces.end(); ++it )
	{
		const SPieceInfo &info = it->second;
		if ( (bOriginal && info.bEditable ) || (!bOriginal && !info.bEditable ) )
			continue;
		BYTE links = 0;
		if ( info.juncs[0] ) links |= 0x1;
		if ( info.juncs[1] ) links |= 0x2;
		if ( info.juncs[2] ) links |= 0x4;
		if ( info.juncs[3] ) links |= 0x8;
		if ( info.juncs[4] ) links |= 0x10;
		if ( info.juncs[5] ) links |= 0x20;
		szRet += IToA( it->first ) + ',';
		szRet += IToA( links );
		szRet += ';';
	}
	if ( szRet[szRet.size()-1] == ';' )
		szRet.pop_back();
	return szRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
