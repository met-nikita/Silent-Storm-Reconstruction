#include "StdAfx.h"
#include "GSceneUtils.h"
#include "GView.h"
#include "G2DView.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\LogStream.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Interface.h"
#include "iLogPanel.h"
#include "UIWrap.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_LINE_TTL = 8000;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLogPanelNotify
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x1f2cc0 -- forward a console line into the panel iff it belongs to the panel's stream type.
void CLogPanelNotify::OnAddConsoleLine( const SConsoleLine &line )
{
	if ( line.eType == eType && IsValid( pPanel ) )
		pPanel->AddLine( line.szText );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLogPanel
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x1f2870 -- build the fixed 10-slot ring, its notify sink, and one empty CTextDraw per storage slot.
CLogPanel::CLogPanel( const SWindowInfo &sInfo, EStreamType _eType ):
	CWindow( sInfo )
{
	// fixed ring of 10 default slots (slot 0 is the permanent sentinel head).
	for ( int i = 0; i < 10; ++i )
		linesSet.push_back( STextLine() );

	// the stream sink: filters console lines by stream type and feeds this panel (weak back-ref).
	pNotify = new CLogPanelNotify( _eType, this );

	// slot 0 is the sentinel/head -- forced used, never scanned.
	linesSet[0].bUsed = true;

	// mint an empty auto-sizing render line for every storage slot.
	for ( int i = 1; i < linesSet.size(); ++i )
		linesSet[i].pText = new CTextDraw( SPoint( 0, 0 ), SPoint( -1, -1 ), L"" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x1f2710 -- link slot nIndex immediately after the sentinel (newest), mark it used, reset its age
// timer, and push the text into its CTextDraw.
void CLogPanel::AddToHead( int nIndex, const wstring &wsText )
{
	const int nOldHead = linesSet[0].nNext;		// old newest (sentinel.nNext)

	linesSet[nIndex].nPrev = 0;					// sentinel
	linesSet[nIndex].nNext = nOldHead;
	linesSet[nOldHead].nPrev = nIndex;
	linesSet[0].nNext = nIndex;

	linesSet[nIndex].bUsed = true;
	linesSet[nIndex].sTime = 0;					// reset display-age (stamped on next Draw)

	// retail AddToHead does GetDBString + operator+ + CTextDraw::SetText; keep the dev's Impact font
	// wrapper (the localized format-string id is not decoded).
	if ( IsValid( linesSet[nIndex].pText ) )
		linesSet[nIndex].pText->SetText( L"<font face=Impact size=20pt>" + wsText );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x1f27c0 -- append one line: claim the first free storage slot, else evict the oldest and reuse it.
void CLogPanel::AddLine( const wstring &wsText )
{
	const int nCount = linesSet.size();

	// pass 1: first free storage slot (indices 1..nCount-1).
	for ( int i = 1; i < nCount; ++i )
	{
		if ( !linesSet[i].bUsed )
		{
			AddToHead( i, wsText );
			return;
		}
	}

	// pass 2: every slot is used -> evict the tail (oldest = sentinel.nPrev) and reuse its index.
	const int nTail = linesSet[0].nPrev;
	linesSet[linesSet[nTail].nPrev].nNext = linesSet[nTail].nNext;
	linesSet[linesSet[nTail].nNext].nPrev = linesSet[nTail].nPrev;
	linesSet[nTail].bUsed = false;

	AddToHead( nTail, wsText );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x1f25e0 -- unlink slot nIndex from the circular ring and free it.
void CLogPanel::Remove( int nIndex )
{
	linesSet[linesSet[nIndex].nPrev].nNext = linesSet[nIndex].nNext;
	linesSet[linesSet[nIndex].nNext].nPrev = linesSet[nIndex].nPrev;
	linesSet[nIndex].bUsed = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// @0x1f2620 -- publish this panel's notify as the active log sink, then walk the ring newest->oldest:
// stamp first-seen lines, expire lines older than N_LINE_TTL (8000ms), draw the rest stacked.
void CLogPanel::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	SetLogNotify( pNotify );

	int nHeight = 0;
	for ( int i = linesSet[0].nNext; i != 0; i = linesSet[i].nNext )
	{
		if ( linesSet[i].sTime == 0 )
			linesSet[i].sTime = sTime;				// first-seen: stamp current time

		if ( ( sTime - linesSet[i].sTime ) > N_LINE_TTL )
		{
			// expired -> unlink from the ring, free the slot (nNext left intact so the walk advances).
			linesSet[linesSet[i].nPrev].nNext = linesSet[i].nNext;
			linesSet[linesSet[i].nNext].nPrev = linesSet[i].nPrev;
			linesSet[i].bUsed = false;
		}
		else if ( IsValid( linesSet[i].pText ) )
		{
			linesSet[i].pText->SetPosition( SPoint( 0, nHeight ) );
			linesSet[i].pText->Draw( this, sTime, pView );
			nHeight += linesSet[i].pText->GetSize( pView ).y;
		}
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0207140, CLogPanel )
REGISTER_SAVELOAD_CLASS( 0xB3714170, CLogPanelNotify )
