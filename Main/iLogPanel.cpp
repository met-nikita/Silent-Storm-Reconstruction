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
// CTextView
////////////////////////////////////////////////////////////////////////////////////////////////////
CLogPanel::CLogPanel( const SWindowInfo &sInfo, EStreamType _eType ):
	CWindow( sInfo ), eType( _eType )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLogPanel::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	{
		list<SConsoleLine>::reverse_iterator iLine = consoleLines.rbegin();

		list<SConsoleLine>::reverse_iterator iTemp = iLine;
		while( iTemp != consoleLines.rend() )
		{
			if ( iTemp->nID == nLastID )
			{
				iTemp++;
				iLine = iTemp;
				break;
			}

			iTemp++;
		}

		for( ; iLine != consoleLines.rend(); iLine++ )
		{
			nLastID = iLine->nID;

			if ( iLine->eType != eType )
				continue;

			STextLine &sLine = *linesList.insert( linesList.end(), STextLine());
			sLine.bUsed = true;
			sLine.sTime = sTime;
			sLine.wsText = iLine->szText;
		}

		for( list<STextLine>::iterator iTemp = linesList.begin(); iTemp != linesList.end(); )
		{
			if ( ( sTime - iTemp->sTime ) > N_LINE_TTL )
				iTemp = linesList.erase( iTemp );
			else
				iTemp++;
		}
	}

	vector< CObj<CTextDraw> > newTextDrawSet;
	newTextDrawSet.reserve( textDrawSet.size() );

	int nHeight = 0;
	int nLineCount = 0;
	list<STextLine>::iterator iLine = linesList.begin();
	while( ( iLine != linesList.end() ) && ( nHeight < GetSize().y ) )
	{
		DrawLine( sTime, pView, nLineCount, L"<font face=Impact size=20pt>" + iLine->wsText, &nHeight, &newTextDrawSet );
		iLine++;
		nLineCount++;
	}

	textDrawSet = newTextDrawSet;

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLogPanel::DrawLine( const STime &sTime, NGScene::I2DGameView *pView, int nLine, const wstring &wsText, int *pHeight, vector<CObj<CTextDraw> > *pNewTextLines )
{
	CObj<CTextDraw> pText;

	if ( nLine < textDrawSet.size() )
	{
		pText = textDrawSet[nLine];
		pText->SetText( wsText );
	}
	else
		pText = new CTextDraw( SPoint( 0, 0 ), SPoint( -1, -1 ), wsText );

	CVec2 vScreenRect = pView->GetViewportSize();
	const SPoint &sSize = pText->GetSize( pView );

	pText->SetPosition( SPoint( 0, *pHeight ) );
	(*pHeight) += sSize.y;

	pNewTextLines->push_back( pText );
	pText->Draw( this, sTime, pView );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0207140, CLogPanel )