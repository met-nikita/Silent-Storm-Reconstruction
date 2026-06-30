#include "StdAfx.h"
#include "MapEdit.h"
#include "Layers.h"
#include "dbDefs.h"
#include "MEUserSettings.h"
#include "..\Input\Bind.h"
#include "..\Main\iMain.h"

const int TRANSPARENT_TILE = 11;
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTilesLayer::SetImage( const CArray2D<BYTE> *pColors )
{
	if ( !pColors )
		return;
	int nWidth  = pColors->GetXSize();
	int nHeight = pColors->GetYSize();
	editor.SetImage( nWidth, nHeight );
	for ( int j = 0; j < nWidth; ++j )
		for ( int i = 0; i < nHeight; ++i )
			//editor.SetTile( j, i, (*pColors)[nHeight - 1 - i][j] );
			editor.SetTileV( j, i, (*pColors)[i][j] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTilesLayer::CreateImage( int nWidth, int nHeight )
{
	editor.SetImage( nWidth, nHeight );
	editor.FillTile( 0, 0, TRANSPARENT_TILE );
	CWnd *pPrnt = GetParent();
	if ( pPrnt )
		pPrnt->PostMessage( WM_USER_NOTIFY, LLN_TERR_SERIALIZE );				
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTilesLayer::Paint( ITemplateView *pView, float fBrightness, bool bGrayed )
{
	ASSERT( pView );
	CDC *pDC = pView->GetPaintDC();
	CPoint ptLT( 0, editor.GetHeight() );
	int nSpacing = pView->GetSpacing();
	
	pView->TemplateToScreen( &ptLT, ptLT );
	int nWidth  = editor.GetWidth() * nSpacing;
	int nHeight = editor.GetHeight() * nSpacing;
	CPoint ptShift = bImageShift ? CPoint( nSpacing/2, nSpacing/2 ) : CPoint(0,0);
//	pDC->StretchBlt( ptLT.x - ptShift.x, ptLT.y + ptShift.y, nWidth, nHeight, editor.GetDC(), 0, 0, editor.GetWidth(), editor.GetHeight(), SRCCOPY );	
	BLENDFUNCTION blend;
	blend.BlendOp = AC_SRC_OVER;
	blend.BlendFlags = 0;
	blend.SourceConstantAlpha = 192;
	blend.AlphaFormat = 0;
	AlphaBlend( pDC->m_hDC, ptLT.x - ptShift.x, ptLT.y + ptShift.y, nWidth, nHeight, editor.GetDC()->m_hDC, 0, 0, editor.GetWidth(), editor.GetHeight(), blend );

}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTilesLayer::OnLButtonDown( UINT nFlags, CPoint pt, ITemplateView *pView )
{
	ASSERT( pView );

	float x, y;
	pView->ScreenToTemplate( pt, &x, &y );
	int nTileID = editor.GetTileID( x, y );
	const SResTree *pTree = theApp.GetResTree( IDC_TERRAINTILES_TREE );
	if ( pTree )
	{
		pView->SelectedItem( IDC_TERRAINTILES_TREE, nTileID, -1 );
		//pTree->pItemsTree->GetItemPath( nTileID );
	}

	Track( nFlags, pt, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTilesLayer::OnMouseMove(UINT nFlags, CPoint pt, ITemplateView *pView )
{
	ASSERT( pView );
	if ( nFlags & MK_LBUTTON )
		Track( nFlags, pt, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTilesLayer::Track( UINT nFlags, CPoint pt, ITemplateView *pView )
{
	ITemplateView::EPaintMode penWidth;
	EEditMode mode = pView->GetPaintMode( (int*)&penWidth );
	float x, y;

	if ( GetBrush().nItemID < 0 && mode != EM_ERASE )
		return;
	pView->ScreenToTemplate( pt, &x, &y );
	x += 0.5f;
	y += 0.5f;
	int nBrush = EM_ERASE == mode ? TRANSPARENT_TILE : GetBrush().nItemID;
	switch ( mode )
	{
		case EM_FILL:
			editor.FillTile( x, y, GetBrush().nItemID );
			pView->Repaint();
			pView->SetModifiedFlag();
			break;
		case EM_SELECT:
		case EM_ERASE:
			switch ( penWidth )
			{
			case ITemplateView::PM_PEN_W1:
				if ( editor.SetTile( x, y, nBrush ) )
				{
					pView->SetModifiedFlag();
					pView->Repaint();
				}
				break;
			case ITemplateView::PM_PEN_W2:
				{
					x -= 0.5f;
					y -= 0.5f;
					bool bch = editor.SetTile( x, y, nBrush );
					bch = editor.SetTile( x + 1, y + 1, nBrush ) || bch;
					bch = editor.SetTile( x + 1, y, nBrush ) || bch;
					bch = editor.SetTile( x, y + 1, nBrush ) || bch;
					if ( bch )
					{
						pView->Repaint();
						pView->SetModifiedFlag();
					}
				}
				break;
			case ITemplateView::PM_PEN_W3:
				{
					x -= 2.0f;
					y -= 2.0f;
					bool bch = false;
					for ( int j = 0; j < 5; ++j )
						for ( int i = 0; i < 5; ++i )
							bch = editor.SetTile( x + i, y + j, nBrush ) || bch;
						if ( bch )
						{
							pView->Repaint();
							pView->SetModifiedFlag();
						}
				}
				break;
			}	
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTilesLayer::GetTiles( CArray2D<BYTE> *pTiles )
{
	int nw = editor.GetWidth();
	int nh = editor.GetHeight();
	int i, j;

	pTiles->SetSizes( nw, nh );
	for ( j = 0; j < nh; ++j )
		for ( i = 0; i < nw; ++i )
			//(*pTiles)[nh - 1 - j][i] = editor.GetTileID( i, j );
			(*pTiles)[j][i] = editor.GetTileID( i, j );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTilesLayer::Export( const string &szExportDir, const string &szPrefix )
{
	string szFile = szExportDir + "\\" + szPrefix + GetLayerName() + ".bmp";

	CFileStream f;
	try
	{
		f.OpenWrite( szFile.c_str() );
		BITMAPFILEHEADER head;
		BITMAPINFOHEADER info;
		Zero( head );
		Zero( info );
		head.bfType = 0x4d42;
		head.bfSize = sizeof( head ) + sizeof( info ) + editor.GetWidth() * editor.GetWidth() * 4;
		head.bfOffBits = sizeof( head ) + sizeof( info );
		info.biSize = sizeof( info );
		info.biWidth = editor.GetWidth();
		info.biHeight = editor.GetHeight();
		info.biPlanes = 1;
		info.biBitCount = 32;
		info.biCompression = BI_RGB;
		info.biSizeImage = 0;
		info.biXPelsPerMeter = 1;
		info.biYPelsPerMeter = 1;
		info.biClrUsed = 0;
		info.biClrImportant = 0;

		f.Write( &head, sizeof( head ) );
		f.Write( &info, sizeof( info ) );
		for ( int y = 0; y < editor.GetHeight(); ++y )
			for ( int x = 0; x < editor.GetWidth(); ++x )
			{
				DWORD cr = editor.GetPixel( x, y );
				cr = RGB( GetBValue(cr), GetGValue(cr), GetRValue(cr) );
				f.Write( &cr, sizeof(cr) );
			}
	}
	catch (...)
	{
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTilesLayer::OnVisible()
{
	CLayerCtrl::OnVisible();
	NInput::PostEvent( "update_terrain" );
	NMainLoop::StepApp(true, true);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
