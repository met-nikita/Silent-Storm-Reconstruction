#include "StdAfx.h"
#include "GSceneUtils.h"
#include "RectLayout.h"
#include "GView.h"
#include "G2DView.h"
#include "A5Script.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\MiscDll\LogStream.h"
#include "..\DBFormat\DataFormat.h"
#include "Interface.h"
#include "UIWrap.h"
#include "UIBaseCtrls.h"
#include "UICommCtrls.h"
#include "Console.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// Время , за которое консоль меняет своё состояние
const int CONSOLE_SPEED = 300;
// длинна промежутка между левым краем экрана и буквами
const int CONSOLE_SPACE_SIZE = 10;
// шаг скрола в пикселях
const int CONSOLE_SCROLL_STEP = 20;
////////////////////////////////////////////////////////////////////////////////////////////////////
static int nCurentCommand = 0;
static list<wstring> consoleCommands;
static const wstring& GetConsoleCommand( int nNum )
{
	ASSERT( nNum >= 0 );
	ASSERT( nNum < consoleCommands.size() );
	list<wstring>::const_iterator iTemp = consoleCommands.begin();
	for ( int nTemp = 0; nTemp < nNum; nTemp++ )
		iTemp++;

	return (*iTemp);
}

namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTextView
////////////////////////////////////////////////////////////////////////////////////////////////////
CTextView::CTextView( const SWindowInfo &sInfo ):
	CWindow( sInfo ), nScroll( 0 ), nLineCount( 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextView::Scroll( int nValue )
{
	nScroll += nValue;
	if ( nScroll < 0 ) nScroll = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextView::ScrollToEnd()
{
	nScroll = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextView::DrawLine( const STime &sTime, NGScene::I2DGameView *pView, int nLine, const wstring &wsText, int *pHeight, vector< CPtr<CTextDraw> > *pNewTextLines )
{
	if ( nScroll > 0 )
	{
		int nLineDelta = consoleLines.size() - nLineCount;
		if ( nLineDelta < 0 )
			nScroll = 0;
		else
			nScroll += nLineDelta;
	}
	nLineCount = consoleLines.size();

	CPtr<CTextDraw> pText;
	if ( nLine < textLines.size() )
	{
		pText = textLines[nLine];
		pText->SetText( wsText );
	}
	else
		pText = new CTextDraw( SPoint( 0, 0 ), SPoint( -1, -1 ), wsText );

	const CVec2 &vScreenRect = pView->GetViewportSize();
	const SPoint &sSize = pText->GetSize( pView );

	(*pHeight) -= sSize.y;
	pText->SetPosition( SPoint( CONSOLE_SPACE_SIZE, *pHeight ) );

	pNewTextLines->push_back( pText );
	pText->Draw( this, sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextView::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	vector< CPtr<CTextDraw> > newTextLines;
	newTextLines.reserve( textLines.size() );

	int nHeight = GetSize().y;
	int nLineCount = 0;
	list<SConsoleLine>::iterator iLine = consoleLines.begin();
	if ( nScroll > 0 )
	{
		for ( int nTemp = 0; nTemp < nScroll; nTemp++ )
		{
			iLine++;
			if ( iLine == consoleLines.end() )
				break;
		}
		DrawLine( sTime, pView, nLineCount, L"<nowrap><font size=16pt>^\t^\t^\t^\t^\t^\t^\t^\t^\t^\t^\t^\t^", &nHeight, &newTextLines );
		nLineCount++;
	}
	while( ( iLine != consoleLines.end() ) && ( nHeight > 0 ) )
	{
		DrawLine( sTime, pView, nLineCount, L"<nowrap><font size=16pt>" + iLine->szText, &nHeight, &newTextLines );

		iLine++;
		nLineCount++;
	}

	textLines = newTextLines;

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Console
////////////////////////////////////////////////////////////////////////////////////////////////////
CConsole::CConsole( const SWindowInfo &sInfo ):
	CWindow( sInfo ), eMode( CON_NONE ), sLastTime( 0 ), fWeight( 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CConsole::SetConsoleState( bool bVisible )
{
	if( !bVisible )
		eMode = CON_SLIDEUP;
	else
		eMode = CON_SLIDEDOWN;

	ShowWindow( SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CConsole::ProcessEvent( const NInput::SEvent &sEvent )
{
	/*
	if ( bindScrollToEnd.ProcessEvent( sEvent ) )
	{
		pTextView->ScrollToEnd();
		return true;
	}
	*/

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CConsole::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_CHAR:
		{
			switch( sEvent.nVal )
			{
			case VK_TAB:
				{
					wstring wsInput( pEdit->GetText() );
					if ( !wsInput.empty() )
					{
						vector<string> varsSet;
						NGlobal::GetIDList( &varsSet );
						sort( varsSet.begin(), varsSet.end() );

						list<wstring> resultList;
						for ( int nTemp = 0; nTemp < varsSet.size(); nTemp++ )
						{
							wstring wsTemp = NStr::ToUnicode( varsSet[nTemp] );
							if ( wsTemp.length() < wsInput.length() )
								continue;

							if ( wsInput == wsTemp.substr( 0, wsInput.length() ) )
								resultList.push_back( wsTemp );
						}

						if ( resultList.size() > 1 )
						{
							csSystem << CC_BLUE << L"Commands( " << (int)resultList.size() << L" ):" << endl;
							for ( list<wstring>::const_iterator iTemp = resultList.begin(); iTemp != resultList.end(); iTemp++ )
								csSystem << L"\t" << (*iTemp) << endl;
						}
						else if ( resultList.size() == 1 )
						{
							wstring wsTemp( resultList.front() + L" " );
							pEdit->SetText( wsTemp );
							pEdit->SetCursorPosition( wsTemp.length() + 1 );
						}
					}
					return true;
				}
			case VK_UP:
				{
					if ( nCurentCommand < consoleCommands.size() )
					{
						nCurentCommand++;
						wstring wsTemp( GetConsoleCommand( nCurentCommand - 1 ) );
						pEdit->SetText( wsTemp );
						pEdit->SetCursorPosition( wsTemp.length() + 1 );
					}
				}
				return true;
			case VK_DOWN:
				{
					if ( nCurentCommand > 0 )
					{
						nCurentCommand--;
						if ( nCurentCommand > 0 )
							pEdit->SetText( GetConsoleCommand( nCurentCommand - 1 ) );
						else
							pEdit->SetText( L"" );
					}
				}
				return true;
			case VK_PRIOR:
				pTextView->Scroll( 1 );
				return true;
			case VK_NEXT:
				pTextView->Scroll( -1 );
				return true;
			}
			break;
		}
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "edit")
			{
				wstring wsEditText( pEdit->GetText() );
				pEdit->SetText( L"" );

				csSystem << wsEditText << endl;

				nCurentCommand = 0;
				consoleCommands.push_front( wsEditText );
				ProcessCommand( wsEditText );

				return true;
			}
			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pTextView = new CTextView( sEvent.pLoader->GetControl( "view" ) );
			pTextView->SetStyle( STYLE_TRANSPARENT, true );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pEdit = GetUIWindow<CEdit>( this, "edit" );
			break;
		}
	}

	if ( CWindow::ProcessMessage( sEvent ) )
		return true;

	switch( sEvent.nEvent )
	{
	case EVENT_LBUTTONUP:
	case EVENT_LBUTTONDOWN:
		{
			if ( HitTest( sEvent.nX, sEvent.nY ) )
				return true;

			break;
		}
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CConsole::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( eMode == CON_NONE )
	{
		sLastTime = GetTickCount();
		CWindow::Draw( sTime, pView );
		return;
	}

	if ( eMode == CON_SLIDEUP )
	{
		fWeight -= (float)( GetTickCount() - sLastTime ) / CONSOLE_SPEED;
		if ( fWeight < 0 )
		{
			eMode = CON_NONE;
			fWeight = 0;
			ShowWindow( SWTYPE_HIDE );
		}
	}
	else if ( eMode == CON_SLIDEDOWN )
	{
		fWeight += (float)( GetTickCount() - sLastTime ) / CONSOLE_SPEED;
		if ( fWeight > 1 )
		{
			eMode = CON_NONE;
			fWeight = 1;
		}
	}

	sLastTime = GetTickCount();
	SetPosition( SPoint( GetPosition().x, GetSize().y * ( fWeight - 1 ) ) );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Commands
////////////////////////////////////////////////////////////////////////////////////////////////////
static void ClearConsole( const string &szID, const vector<wstring> &szParams, void *pContext )
{
	bConsoleUpdated = true;
	
	consoleLines.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(Console)
	REGISTER_CMD( "clear", ClearConsole )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xF2501170, CConsole )
REGISTER_SAVELOAD_CLASS( 0xF25c1171, CTextView )