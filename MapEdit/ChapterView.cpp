// ChapterView.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "ChapterView.h"
#include "..\Main\ChapterInfo.h"
#include "..\Main\iMain.h"
#include "..\Main\Gfx.h"
#include "..\MiscDll\Commands.h"
#include "..\Input\Bind.h"
#include "Layers.h"
#include "SectorCtrl.h"
#include "Export.h"
#include "CtrlObjectInspector.h"
#include "dbDefs.h"
#include "ChapterSectorDlg.h"
#include "ItemsMgr.h"
#include "..\Image\ImageTGA.h"
#include "GameView.h"
#include "NewSectorDlg.h"
#include "ItemsMgr.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
const CSize viewSize( CHAPTER_SIZE_X, CHAPTER_SIZE_Y );
const int START_POINT_SIZE = 5;
const int ZONE_RADIUS = 35;
const int ZONE_HITTEST = sqr(ZONE_RADIUS);
////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterView
CChapterView::CChapterView()
{
	SetScrollSizes( MM_TEXT, viewSize );
	bModified = false;
	bDrag = false;
	m_popup.CreatePopupMenu();
	m_popup.AppendMenu( MF_STRING, ID_CONTOURPROPS, "Sector properties..." );
	m_popup.AppendMenu( MF_STRING, ID_NEWCONTOUR, "New sector" );
	m_popup.AppendMenu( MF_STRING, ID_DELCONTOUR, "Delete sector" );
	props["Background"] = new CChapterProp( "Background", 0, CVariant::VT_INT, DT_DEC, this );
	props["Background"]->SetGroup( IDC_UITEXTURES_TREE );
	props["Background"]->SetRelation( IDC_UITEXTURES_TREE );
	bmpBackground.CreateBitmap( CHAPTER_SIZE_X, CHAPTER_SIZE_Y, 1, 32, 0 );
	bDrawStartPoint = true;
}

CChapterView::~CChapterView()
{
	bmpBackground.DeleteObject();
}

BOOL CChapterView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CScrollView::PreCreateWindow(cs))
		return FALSE;

	//cs.dwExStyle |= WS_EX_CLIENTEDGE;
	cs.style &= ~WS_BORDER;
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS|CS_BYTEALIGNCLIENT, 
		::LoadCursor(NULL, IDC_ARROW), /*HBRUSH(COLOR_WINDOW+1)*/0, NULL);

	return TRUE;
}

BEGIN_MESSAGE_MAP(CChapterView, CScrollView)
	//{{AFX_MSG_MAP(CChapterView)
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_COMMAND(ID_NEWCONTOUR, OnNewContour)
	ON_COMMAND(ID_DELCONTOUR, OnDelContour)
	ON_COMMAND(ID_CONTOURPROPS, OnContourProps)
	ON_WM_LBUTTONUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterView drawing

void CChapterView::OnInitialUpdate()
{
	CScrollView::OnInitialUpdate();

	SetScrollSizes(MM_TEXT, viewSize);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterView::SetChapter( int nID )
{
	if ( bModified && IsValid( pChapter ) )
	{
		UpdateChapterData();
		SerializeChapter( nChapterID, pChapter );
	}
	CObj<CChapterInfoLoader> pChapterLoader = new CChapterInfoLoader;
	pChapterLoader->SetKey( nID );
	CDGPtr<CPtrFuncBase<CChapterInfo> > pLoader = pChapterLoader;
	pLoader.Refresh();
	pChapter = pLoader->GetValue();
	nChapterID = nID;
	sectors.clear();
	vStartPoint = CVec2(0,0);
	CVariant background;
	if ( IsValid( pChapter ) )
	{
		for ( int i = 0; i < pChapter->sectorsSet.size(); ++i )
		{
			sectors.push_back( new CSectorCtrl( CHAPTER_SIZE_X, CHAPTER_SIZE_Y, SEGMENT_LEN, pChapter->sectorsSet[i].pointsSet ) );
			sectors.back()->nTemplateID = pChapter->sectorsSet[i].nTemplate;
			sectors.back()->nProbability = pChapter->sectorsSet[i].nProbability;
			sectors.back()->nDescrID = pChapter->sectorsSet[i].nDescriptionID;
			sectors.back()->eSectorType = pChapter->sectorsSet[i].eType;
			sectors.back()->szID = pChapter->sectorsSet[i].szID;
			sectors.back()->SetHitAccuracy( sectors.back()->eSectorType == RANDOM ? HIT_ACCURACY : ZONE_HITTEST );
		}
		background = pChapter->nMapID;
		vStartPoint = pChapter->vDeployPos;
		vStartPoint.x = Clamp( vStartPoint.x, 0.0f, (float)CHAPTER_SIZE_X );
		vStartPoint.y = Clamp( vStartPoint.y, 0.0f, (float)CHAPTER_SIZE_Y );
	}
	props["Background"]->SetValue( background, false );
	props["Background"]->SetOwner( SOwner( nID, -1 ) );
	bModified = false;
	Invalidate( FALSE );
	//theApp.SetPropMap( &props );
	nBackgroundID = background;
	const SResTree *pTree = theApp.GetResTree( IDC_CHAPTERS_TREE );
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nID );
	if ( !pProps )
		return;
	CPropMap::const_iterator i = pProps->find( "Background" );
	if ( i != pProps->end() )
		nBackgroundID = i->second->GetValue();
	pTree->pItemsTree->ReleasePropList( pProps );
	SetBackground();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterView::SetBackground()
{
	vector<COLORREF> data( CHAPTER_SIZE_X * CHAPTER_SIZE_Y, 0xffffffff );
	bmpBackground.SetBitmapBits( data.size() * sizeof( COLORREF ), &data[0] );
	//
	const SResTree *pUITexTree = theApp.GetResTree( IDC_UITEXTURES_TREE );
	const SResTree *pTexTree = theApp.GetResTree( IDC_TEXTURES_TREE );
	if ( !pTexTree || !pUITexTree )
		return;
	const CPropMap *pUIProps = pUITexTree->pItemsTree->GetPropList( nBackgroundID );
	if ( !pUIProps )
		return;
	CPropMap::const_iterator itex = pUIProps->find( "R_1024x768" );
	if ( itex == pUIProps->end() )
		return;
	const CPropMap *pTexProps = pTexTree->pItemsTree->GetPropList( itex->second->GetValue() );
	if ( !pTexProps )
		return;
	//
	CPropMap::const_iterator isrc = pTexProps->find( "SrcName" );
	if ( isrc == pTexProps->end() )
		return;
	string szBmp = GetExportSrcDir() + "Textures\\" + (string)isrc->second->GetValue();
	try
	{
		NImage::CImage image;
		CFileStream fsrc;

		fsrc.OpenRead( szBmp.c_str() );
		if ( NImage::LoadImageTGA( &fsrc, &image ) )
		{
			int nX = Min( CHAPTER_SIZE_X, image.GetXSize() );
			int nY = Min( CHAPTER_SIZE_Y, image.GetYSize() );
			vector<COLORREF> data( nX * nY );
			for ( int x = 0; x < nX; ++x )
			{
				int ytga = image.GetYSize() - 1;
				for ( int y = 0; y < nY; ++y, --ytga )
				{
					CVec4 pt = image[ytga][x];
					pt *= 255;
					COLORREF cr = RGB( pt.z, pt.y, pt.x );
					data[y * nX + x] = cr;
				}
			}
			bmpBackground.SetBitmapBits( data.size() * sizeof( COLORREF ), &data[0] );
		}
	} 
	catch (...)
	{
	}
	//
	pTexTree->pItemsTree->ReleasePropList( pTexProps );
	pUITexTree->pItemsTree->ReleasePropList( pUIProps );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterView::SetBackground( const CArray2D<NGfx::SPixel8888> &screen )
{
	{
		vector<COLORREF> data( CHAPTER_SIZE_X * CHAPTER_SIZE_Y, 0xffffffff );
		bmpBackground.SetBitmapBits( data.size() * sizeof( COLORREF ), &data[0] );
	}
	//
	int nX = Min( CHAPTER_SIZE_X, screen.GetXSize() );
	int nY = Min( CHAPTER_SIZE_Y, screen.GetYSize() );
	vector<COLORREF> data( CHAPTER_SIZE_X * nY );
//	for ( int x = 0; x < nX; ++x )
//	{
//		int ytga = image.GetYSize() - 1;
		for ( int y = 0; y < nY; ++y )
		{
			//data[y * nX + x] = screen[y][x].color;
			memcpy( &data[0] + y * CHAPTER_SIZE_X, &screen[y][0], nX * 4 );
		}
//	}
	bmpBackground.SetBitmapBits( data.size() * sizeof( COLORREF ), &data[0] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterView::OnDraw(CDC* pDC)
{
	CRect r, rclient;
  CDC dcBuf, dcBack;
  CBitmap bmp;
	CPoint ptScroll = GetScrollPosition();

  GetClientRect( &rclient );
	CRect rBuf( 0, 0, Max( ptScroll.x + rclient.right, viewSize.cx ), Max( ptScroll.y + rclient.bottom, viewSize.cy ) );
  bmp.CreateCompatibleBitmap( pDC, rBuf.right, rBuf.bottom );
  dcBuf.CreateCompatibleDC( pDC );
	dcBack.CreateCompatibleDC( pDC );
	dcBuf.SetMapMode( pDC->GetMapMode() );
	dcBuf.SetViewportOrg( pDC->GetViewportOrg() );
	dcBuf.SetBkMode( TRANSPARENT );
	dcBuf.SetTextAlign( TA_CENTER );
	dcBuf.SetTextColor( RGB( 70, 60, 70 ) );
	CBitmap *pOldBmp = dcBuf.SelectObject( &bmp );
	CBitmap *pOldBackBmp = dcBack.SelectObject( &bmpBackground );
	r = rclient + ptScroll;
	dcBuf.BitBlt( 0, 0, CHAPTER_SIZE_X, CHAPTER_SIZE_Y, &dcBack, 0, 0, SRCCOPY );
	//
	int oldMode = dcBuf.SetROP2( R2_COPYPEN );
	CPen pen( PS_SOLID, 2, RGB( 0, 200, 0 ) );
	CPen pen1( PS_SOLID, 2, RGB( 100, 100, 255 ) );
	CPen penRed( PS_SOLID, 2, RGB( 150, 0, 0 ) );
	CPen penGreen( PS_SOLID, 5, RGB( 0, 150, 0 ) );
	CPen *pOldPen = dcBuf.SelectObject( &pen );
	for ( int i = 0; i < sectors.size(); ++i )
	{
		const vector<CVec2> &vPolygon = sectors[i]->GetPoints();
		if ( vPolygon.empty() )
			continue;
		CPoint ptStart( vPolygon[0].x, vPolygon[0].y );
		CPoint pt = ptStart;
		if ( sectors[i]->eSectorType == RANDOM )
		{
			dcBuf.MoveTo( pt );
			for ( int j = 1; j < vPolygon.size(); ++j )
			{
				dcBuf.LineTo( CPoint( vPolygon[j].x, vPolygon[j].y ) );
			}
			dcBuf.LineTo( ptStart );
			dcBuf.SelectObject( &pen1 );
			for ( int j = 0; j < vPolygon.size(); ++j )
			{
				PaintCtrlPoint( &dcBuf, CPoint( vPolygon[j].x, vPolygon[j].y ), 5 );
			}
			dcBuf.SelectObject( &pen );
		}
		else
		{
			dcBuf.SelectObject( &penGreen );
			dcBuf.Ellipse( ptStart.x - ZONE_RADIUS, ptStart.y - ZONE_RADIUS, ptStart.x + ZONE_RADIUS, ptStart.y + ZONE_RADIUS );
			dcBuf.SelectObject( pOldPen );
			dcBuf.SetROP2( oldMode );
		}
	}
	if ( bDrawStartPoint )
	{
		dcBuf.SelectObject( &penRed );
		dcBuf.Ellipse( vStartPoint.x - START_POINT_SIZE, vStartPoint.y - START_POINT_SIZE, vStartPoint.x + START_POINT_SIZE, vStartPoint.y + START_POINT_SIZE );
		dcBuf.SelectObject( pOldPen );
		dcBuf.SetROP2( oldMode );
	}
	//
  pDC->BitBlt( 0, 0, rBuf.right, rBuf.bottom, &dcBuf, 0, 0, SRCCOPY );
  dcBuf.SelectObject( pOldBmp );
	dcBack.SelectObject( pOldBackBmp );
  bmp.DeleteObject();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterView diagnostics

#ifdef _DEBUG
void CChapterView::AssertValid() const
{
	CScrollView::AssertValid();
}

void CChapterView::Dump(CDumpContext& dc) const
{
	CScrollView::Dump(dc);
}
#endif //_DEBUG

////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterView message handlers
void CChapterView::PostNcDestroy() 
{
	if ( bModified && IsValid( pChapter ) )
	{
		UpdateChapterData();
		SerializeChapter( nChapterID, pChapter );
	}	
//	CScrollView::PostNcDestroy();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterView::Paint( const CPoint &pt )
{
	CPoint ptScroll = GetScrollPosition();
	bModified = true;
	Invalidate( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChapterView::RegionHitTest( const CVec2 &pt, int *pRegInd, int *pPtInd )
{
	for ( int i = 0; i < sectors.size(); ++i )
	{
		*pPtInd = sectors[i]->RegionHitTest( pt );
		if ( *pPtInd >= 0 )
		{
			*pRegInd = i;
			return true;
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChapterView::StartPointHitTest( const CVec2 &pt )
{
	if ( fabs2( pt - vStartPoint ) < HIT_ACCURACY )
		return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterView::OnLButtonDown(UINT nFlags, CPoint point) 
{
	CPoint pt = GetScrollPosition() + point;
	int nSectorID;
	if ( RegionHitTest( CVec2( pt.x, pt.y ), &nSectorID, &nPointID ) )
	{
		pDrag = sectors[nSectorID];
		bDrag = true;
	}
	if ( StartPointHitTest( CVec2( pt.x, pt.y ) ) )
	{
		bDrag = true;
	}
	if ( bDrag )
	{
		if ( ::GetCapture() != NULL )
		{
			bDrag = false;
			return;
		}
		SetCapture();
		return;
	}
	Paint( point );
	CScrollView::OnLButtonDown(nFlags, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterView::OnMouseMove(UINT nFlags, CPoint point) 
{
	if ( bDrag )
	{
		Invalidate( FALSE );
		if ( CWnd::GetCapture() == this )
		{
			CPoint pt = GetScrollPosition() + point;
			CVec2 vec( pt.x, pt.y ); 
			if ( IsValid( pDrag ) && pDrag->MovePoint( nPointID, vec ) )
			{
				bModified = true;
				if ( pDrag->Rebuild() )
				{
					bDrag = false;
					pDrag = 0;
					ReleaseCapture();
				}
				return;
			}
			else
			{
				vec.x = Clamp( vec.x, 0.0f, (float)CHAPTER_SIZE_X );
				vec.y = Clamp( vec.y, 0.0f, (float)CHAPTER_SIZE_Y );
				vStartPoint = vec;
				bModified = true;
				return;
			}
		}
		bDrag = false;
		pDrag = 0;
		return;
	}
	if ( MK_LBUTTON & nFlags )
		Paint( point );
	CScrollView::OnMouseMove(nFlags, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterView::OnLButtonUp(UINT nFlags, CPoint point) 
{
	if ( bDrag )
	{
		ReleaseCapture();
		bDrag = false;
		if ( IsValid( pDrag ) )
			pDrag->Rebuild();
		pDrag = 0;
		Invalidate( FALSE );
		bModified = true;
	}
	CScrollView::OnLButtonUp(nFlags, point);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterView::OnRButtonDown(UINT nFlags, CPoint point) 
{
	ptRightClick = point;
	ClientToScreen( &point );
	m_popup.TrackPopupMenu( TPM_LEFTBUTTON, point.x, point.y, this );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterView::UpdateChapterData()
{
	if ( !IsValid( pChapter ) )
		return;
	//
	pChapter->sectorsSet.clear();
	for ( int i = 0; i < sectors.size(); ++i )
	{
		CSectorCtrl *p = sectors[i];
		if ( !p )
			continue;
		pChapter->sectorsSet.push_back( SChapterSector() );
		SChapterSector &chap = pChapter->sectorsSet.back();
		chap.pointsSet = p->GetPoints();
		chap.nProbability = p->nProbability;
		chap.nTemplate = p->nTemplateID;
		chap.nDescriptionID = p->nDescrID;
		chap.eType = p->eSectorType;
		chap.szID = p->szID;
	}
	//
	pChapter->nMapID = props["Background"]->GetValue();
	pChapter->vDeployPos = vStartPoint;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CChapterView::SerializeChapter( int nChapterID, CChapterInfo *pChapter )
{
	if ( !pChapter )
		return false;
	try
	{
		char szPath[MAX_PATH];
		sprintf( szPath, "%sChapters\\%d", GetExportDstDir().c_str(), nChapterID );
		CFileStream fp;
		fp.OpenWrite( szPath );
		{
			CStructureSaver file( fp, CStructureSaver::WRITE );
			pChapter->operator&( file ) ;
		}
		return true;
	}
	catch ( ... )
	{
		return false;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterView::OnNewContour()
{
	CNewSectorDlg dlg;

	if ( IDOK != dlg.DoModal() )
		return;
	CPoint pt = GetScrollPosition() + ptRightClick;
	sectors.push_back( new CSectorCtrl( CHAPTER_SIZE_X, CHAPTER_SIZE_Y, SEGMENT_LEN ) );
	CSectorCtrl *p = sectors.back();
	p->SetHitAccuracy( HIT_ACCURACY );
	p->CreateSector( CVec2( pt.x, pt.y ), 20, dlg.m_nSegments );
	p->nProbability = 0;
	p->eSectorType = RANDOM;
	p->nTemplateID = -1;
	p->nDescrID = -1;
	bModified = true;
	Invalidate( FALSE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterView::OnDelContour()
{
	CPoint pt = GetScrollPosition() + ptRightClick;
	int iReg, iPt;
	if ( RegionHitTest( CVec2( pt.x, pt.y ), &iReg, &iPt ) )
	{
		sectors.erase( sectors.begin() + iReg );
		Invalidate( FALSE );
		bModified = true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterView::OnContourProps()
{
	CPoint pt = GetScrollPosition() + ptRightClick;
	int iReg, iPt;
	if ( RegionHitTest( CVec2( pt.x, pt.y ), &iReg, &iPt ) )
	{
		CSectorCtrl *pS = sectors[iReg];
		if ( pS )
		{
			CChapterSectorDlg dlg;

			dlg.m_nProbability = pS->nProbability;
			dlg.m_nTemplteID = pS->nTemplateID;
			dlg.m_nDescrID = pS->nDescrID;
			dlg.eSectorType = pS->eSectorType;
			dlg.m_szID = pS->szID.c_str();
			if ( dlg.DoModal() != IDOK )
				return;
			pS->nTemplateID = dlg.m_nTemplteID;
			pS->nProbability = dlg.m_nProbability;
			pS->nDescrID = dlg.m_nDescrID;
			pS->eSectorType = dlg.eSectorType;
			pS->szID = dlg.m_szID;
			pS->SetHitAccuracy( pS->eSectorType == RANDOM ? HIT_ACCURACY : ZONE_HITTEST );
			bModified = true;
			Invalidate();
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CChapterProp::CChapterProp( const string &szName, int nID, int nType, int nViewType, CChapterView *pView, CVariant _defValue, bool bReadOnly )
: CProp( szName, nID, nType, nViewType, bReadOnly ), defValue( _defValue ), pChapterView( pView )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CChapterProp::SetValue( const CVariant &newValue, bool bModified ) const
{
  *(const_cast<CVariant*>(&value)) = newValue;
	if ( bModified )
		pChapterView->SetModified();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CProp* CChapterProp::Clone() const
{
	CChapterProp *pClone = new CChapterProp( GetName(), GetID(), GetType(), GetViewType(), pChapterView, defValue );
	pClone->value = value;
	return pClone;		
}
////////////////////////////////////////////////////////////////////////////////////////////////////
