#include "StdAfx.h"
#include "Gfx.h"
#include "iMain.h"
#include "G2DView.h"
#include "RPGGlobal.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "..\DBFormat\DataScenario.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iCluesMenu.h"
#include "UIWrap.h"
#include "scScenarioTracker.h"
#include "scFlowChartItems.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NUI::ConvertLineBreaks @0x31bc30 (same local copy as iShowClue.cpp / iMissionUI.cpp)
static wstring ConvertLineBreaks( const wstring &szStr )
{
	wstring szRet;
	for ( wstring::const_iterator i = szStr.begin(); i != szStr.end(); )
	{
		switch ( wchar_t(*i) )
		{
			case L'\n':
				szRet += L"<br>";
				break;
			case L'\r':
				szRet += L"<br>";
				++i;
				if ( i != szStr.end() && *i == L'\n' )
					++i;
				continue;
			case 133: // ellipsis symbol
				szRet += L"...";
				break;
			default:
				szRet += *i;
				break;
		}
		++i;
	}
	return szRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCluesUI
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NUI::CJournalItem (save id 0xB0820131 unchanged); stores the resolved description CString
// (operator& @0x1bdc30 tag 2 = CDBPtr<NDb::CString>), not the clue itself
class CJournalItem: public CButton
{
	OBJECT_NOCOPY_METHODS(CJournalItem)
private:
	ZDATA_(CButton)
	CDBPtr<NDb::CString> pDescription;
	////
	CObj<CText> pText;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CButton*)this); f.Add(2,&pDescription); f.Add(3,&pText); return 0; }

protected:
	void OnAction();

public:
	CJournalItem() {}
	CJournalItem( const SWindowInfo &sInfo, const wstring &wsText, NDb::CString *_pDescription );

	NDb::CString* Get() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x1bafe0
CJournalItem::CJournalItem( const SWindowInfo &sInfo, const wstring &wsText, NDb::CString *_pDescription ):
	CButton( sInfo ), pDescription( _pDescription )
{
	pText = new CText( SWindowInfo( this, SPoint( 0, 0 ), GetSize(), "iml-text", STYLE_ENABLED | STYLE_VISIBLE ) );
	pText->SetText( wsText );

	SPoint sSize;
	pText->GetRealSize( &sSize );

	sSize.x = GetSize().x;
	SetSize( sSize );
	pText->SetSize( sSize );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CString* CJournalItem::Get() const
{
	return pDescription;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x1ba890
void CJournalItem::OnAction()
{
	SendMessage( GetParent(), SEvent( EVENT_LISTVIEW_ITEMSELECTED, this ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCluesUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCluesUI: public CWindow
{
	OBJECT_NOCOPY_METHODS(CCluesUI)
private:
	enum EFilter
	{
		ORDER,
		ZONE_CLUES,
		ZONE_POINT,
		SHOW_HINTS
	};
	ZDATA_(CWindow)
	CPtr<NRPG::CGlobalGame> pGame;
	//// retail: four CComplexButton icon buttons (@0x1bc020), incl. the 4th hints filter
	CObj<CComplexButton> pSortByDate;
	CObj<CComplexButton> pSortByZone;
	CObj<CComplexButton> pSortByPoint;
	CObj<CComplexButton> pShowHints;
	////
	CObj<CText> pDescription;
	CObj<CScrollWindow<CText> > pDescriptionView;
	////
	CObj<CListView> pList;
	CObj<CScrollWindow<CListView> > pListView;
	////
	CPtr<CImage> pSelection;
	CObj<CFlashButton> pCloseButton;
	// retail operator& @0x1bd9f0: pShowHints is tag 6, shifting the rest to 7..12
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pGame); f.Add(3,&pSortByDate); f.Add(4,&pSortByZone); f.Add(5,&pSortByPoint); f.Add(6,&pShowHints); f.Add(7,&pDescription); f.Add(8,&pDescriptionView); f.Add(9,&pList); f.Add(10,&pListView); f.Add(11,&pSelection); f.Add(12,&pCloseButton); return 0; }

protected:
	void GenerateList( EFilter eFilter );
	void AddSeparator( const wstring &wsText, int *pConut );
	void GenerateListSimple( int nNum, const wstring &wsAdd, const list<CPtr<NScenario::CScenarioClue> > &cluesList, int *pConut );
	void GenerateListByZone( NScenario::CScenarioZone *pZone, const list<CPtr<NScenario::CScenarioClue> > &cluesList, int *pConut );
	void GenerateListByPoint( NScenario::CScenarioZone *pZone, const list<CPtr<NScenario::CScenarioClue> > &cluesList, int *pConut );

public:
	CCluesUI() {}
	CCluesUI( const SWindowInfo &sInfo, NRPG::CGlobalGame *_pGame );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CCluesUI::CCluesUI( const SWindowInfo &sInfo, NRPG::CGlobalGame *_pGame ):
	CWindow( sInfo ), pGame( _pGame )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCluesUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "view" )
			{
				int nID = pList->GetSelectedItem();
				CPtr<CJournalItem> pItem = dynamic_cast<CJournalItem*>( pList->GetItem( pList->GetSelectedItem() ) );
				if ( IsValid( pItem ) )
				{
					// retail @0x1bc020: DB string 20258 ("Clue&Hint Format" markup,
					// <font face=Courier size=16pt><color=0xFF5A5959>) + line-break-converted description
					pDescription->SetText( GetDBString( 20258 ) + ConvertLineBreaks( GetDBString( pItem->Get() ) ), true );

					// retail @0x1bc020: fit the text to its content height, keeping the template width
					SPoint sRealSize;
					pDescription->GetRealSize( &sRealSize );
					sRealSize.x = pDescription->GetSize().x;
					pDescription->SetSize( sRealSize );
				}

				return true;
			}
			else if ( sEvent.szID == "sort_bydate" )
			{
				GenerateList( ORDER );
				return true;
			}
			else if ( sEvent.szID == "sort_byzone" )
			{
				GenerateList( ZONE_CLUES );
				return true;
			}
			else if ( sEvent.szID == "sort_bypoint" )
			{
				GenerateList( ZONE_POINT );
				return true;
			}
			else if ( sEvent.szID == "show_hints" )
			{
				GenerateList( SHOW_HINTS );
				return true;
			}

			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pCloseButton = new CFlashButton( sEvent.pLoader->GetControl( "cancel" ) );

			pListView = new CScrollWindow<CListView>( sEvent.pLoader->GetControl( "view" ) );
			pList = pListView->GetClientWindow();

			pDescriptionView = new CScrollWindow<CText>( sEvent.pLoader->GetControl( "description" ) );
			pDescription = pDescriptionView->GetClientWindow();

			// retail @0x1bc020: icon buttons -- corner frames UITexture 566 "Unselected" / 408 "Selected",
			// icons 757 wristwatch / 758 map / 759 magnifier / 760 lightbulb
			pSortByDate = new CComplexButton( sEvent.pLoader->GetControl( "sort_bydate" ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pSortByDate->Set( NDb::GetUITexture( 757 ), 0, CComplexButton::UNCHECKED, "" );

			pSortByZone = new CComplexButton( sEvent.pLoader->GetControl( "sort_byzone" ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pSortByZone->Set( NDb::GetUITexture( 758 ), 0, CComplexButton::UNCHECKED, "" );

			pSortByPoint = new CComplexButton( sEvent.pLoader->GetControl( "sort_bypoint" ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pSortByPoint->Set( NDb::GetUITexture( 759 ), 0, CComplexButton::UNCHECKED, "" );

			pShowHints = new CComplexButton( sEvent.pLoader->GetControl( "show_hints" ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pShowHints->Set( NDb::GetUITexture( 760 ), 0, CComplexButton::UNCHECKED, "" );

			GenerateList( ORDER );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pSelection = GetUIWindow<CImage>( this, "selection" );
			pSelection->SetStyle( STYLE_VISIBLE, false );

			pListView->SetVScroll( GetUIWindow<CScroll>( this, "list_scroll" ) );
			pDescriptionView->SetVScroll( GetUIWindow<CScroll>( this, "descr_scroll" ) );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCluesUI::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	int nID = pList->GetSelectedItem();
	bool bVisible = false;
	if ( nID != -1 )
	{
		CPtr<CJournalItem> pItem = dynamic_cast<CJournalItem*>( pList->GetItem( nID ) );
		if ( IsValid( pItem ) )
		{
			const SPoint &sSize = pItem->GetSize();
			SRect sWindow;
			SPoint sPosition;
			if ( pItem->ClientToScreen( &sPosition, &sWindow ) )
			{
				const SPoint &sSelectionSize = pSelection->GetSize();
				const SPoint &sSelectionPosition = pSelection->GetPosition();
				pSelection->SetPosition( SPoint( sSelectionPosition.x, sPosition.y + sSize.y - sSelectionSize.y ) );

				bVisible = true;
			}
		}
	}
	pSelection->SetStyle( STYLE_VISIBLE, bVisible );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCluesSort
{
	bool operator()( NScenario::CScenarioClue *p1, NScenario::CScenarioClue *p2 ) const 
	{ 
		return p1->GetOpenOrder() < p2->GetOpenOrder(); 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SZoneSort
{
	bool operator()( NScenario::CScenarioZone *p1, NScenario::CScenarioZone *p2 ) const 
	{
		return p1->GetOpenOrder() < p2->GetOpenOrder(); 
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCluesUI::GenerateList( EFilter eFilter )
{
	pList->RemoveAllItems();

	// retail @0x1bb930: the active filter's button gets the checked (selected-frame) state
	pSortByDate->SetChecked( eFilter == ORDER );
	pSortByZone->SetChecked( eFilter == ZONE_CLUES );
	pSortByPoint->SetChecked( eFilter == ZONE_POINT );
	pShowHints->SetChecked( eFilter == SHOW_HINTS );

	switch( eFilter )
	{
	case ORDER:
		{
			list<CPtr<NScenario::CScenarioClue> > cluesList;
			pGame->pScenarioTracker->GetAvailableClues( &cluesList );
			cluesList.sort( SCluesSort() );

			int nCount = 0;
			GenerateListSimple( 1, L"", cluesList, &nCount );

			break;
		}
	case ZONE_CLUES:
		{
			list< CPtr<NScenario::CScenarioZone> > zonesList;
			pGame->pScenarioTracker->GetAvailableZones( &zonesList );
			zonesList.sort( SZoneSort() );

			list<CPtr<NScenario::CScenarioClue> > cluesList;
			pGame->pScenarioTracker->GetAvailableClues( &cluesList );

			int nCount = 0;
			for( list< CPtr<NScenario::CScenarioZone> >::const_iterator iZone = zonesList.begin(); iZone != zonesList.end(); iZone++ )
			{
				// retail @0x1bb930: separator = the zone's localized pName CString, not sSmallDescription
				AddSeparator( GetDBString( (*iZone)->GetDBZone()->pName ), &nCount );
				GenerateListByZone( *iZone, cluesList, &nCount );
			}
			AddSeparator( GetDBString( 19811 ), &nCount );	// retail @0x1bb930: string 19811 "Separator - Result"
			GenerateListByZone( 0, cluesList, &nCount );

			break;
		}
	case ZONE_POINT:
		{
			list< CPtr<NScenario::CScenarioZone> > zonesList;
			pGame->pScenarioTracker->GetAvailableZones( &zonesList );
			zonesList.sort( SZoneSort() );

			list<CPtr<NScenario::CScenarioClue> > cluesList;
			pGame->pScenarioTracker->GetAvailableClues( &cluesList );

			int nCount = 0;
			for( list< CPtr<NScenario::CScenarioZone> >::const_iterator iZone = zonesList.begin(); iZone != zonesList.end(); iZone++ )
			{
				AddSeparator( GetDBString( (*iZone)->GetDBZone()->pName ), &nCount );
				GenerateListByPoint( *iZone, cluesList, &nCount );
			}
			AddSeparator( GetDBString( 19811 ), &nCount );
			GenerateListByPoint( 0, cluesList, &nCount );

			break;
		}
	case SHOW_HINTS:
		{
			// retail @0x1bb930 case 3: list the campaign's collected UI hints (CGlobalGame::hintsSet);
			// item text = title, stored description = the hint's full pString
			int nCount = 0;
			for( vector< CDBPtr<NDb::CUIHint> >::const_iterator iHint = pGame->hintsSet.begin(); iHint != pGame->hintsSet.end(); ++iHint )
			{
				wstring wsText = GetDBString( 20994 ) + NStr::Format( L"%d. %s", nCount + 1, GetDBString( (*iHint)->pTitle ).c_str() );
				pList->AddItem( nCount, new CJournalItem( SWindowInfo( pList, SPoint( 0, 0 ), SPoint( pList->GetSize().x, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), wsText, (*iHint)->pString ) );
				nCount++;
			}

			break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCluesUI::AddSeparator( const wstring &wsText, int *pConut )
{
	int &nCount = (*pConut);

	CPtr<CText> pText = new CText( SWindowInfo( pList, SPoint( 0, 0 ), SPoint( pList->GetSize().x, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ) );
	// retail @0x1ba9b0: DB string 20993 ("Journal Headers" markup, <font face=Courier size=18pt><color=FF2E4051>)
	pText->SetText( GetDBString( 20993 ) + wsText, true );

	SPoint sSize;
	pText->GetRealSize( &sSize );
	sSize.x = GetSize().x;
	pText->SetSize( sSize );

	pList->AddItem( nCount, pText );
	nCount++;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x1bb1a0: wsAdd is passed by the callers but never used
void CCluesUI::GenerateListSimple( int nNum, const wstring &wsAdd, const list<CPtr<NScenario::CScenarioClue> > &cluesList, int *pConut )
{
	int &nCount = (*pConut);

	for( list<CPtr<NScenario::CScenarioClue> >::const_iterator iTemp = cluesList.begin(); iTemp != cluesList.end(); iTemp++ )
	{
		// DB string 20994 ("Jornal ListItems Format" markup, <font face=Courier size=16pt><color=0xFF5A5959>)
		wstring wsTemp = GetDBString( 20994 ) + NStr::Format( L"%d. %s", nNum, GetDBString( (*iTemp)->GetDBClue()->pDescription ).c_str() );

		// the item stores the resolved objective-description CString for the "view" panel
		pList->AddItem( nCount, new CJournalItem( SWindowInfo( pList, SPoint( 0, 0 ), SPoint( pList->GetSize().x, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), wsTemp, pGame->pScenarioTracker->GetClueDescriptionFromObjective( *iTemp ) ) );
		nCount++;
		nNum++;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCluesUI::GenerateListByZone( NScenario::CScenarioZone *pZone, const list<CPtr<NScenario::CScenarioClue> > &cluesList, int *pConut )
{
	int &nCount = (*pConut);

	list<CPtr<NScenario::CScenarioClue> > zoneCluesList;
	for ( list<CPtr<NScenario::CScenarioClue> >::const_iterator iClue = cluesList.begin(); iClue != cluesList.end(); iClue++ )
	{
		if ( pGame->pScenarioTracker->GetZoneInWhichClueWasFound( *iClue ) != pZone )
			continue;

		zoneCluesList.push_back( *iClue );
	}
	zoneCluesList.sort( SCluesSort() );

	GenerateListSimple( 1, L"\t", zoneCluesList, &nCount );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCluesUI::GenerateListByPoint( NScenario::CScenarioZone *pZone, const list<CPtr<NScenario::CScenarioClue> > &cluesList, int *pConut )
{
	int &nCount = (*pConut);

	list<CPtr<NScenario::CScenarioClue> > zoneCluesList;
	for ( list<CPtr<NScenario::CScenarioClue> >::const_iterator iClue = cluesList.begin(); iClue != cluesList.end(); iClue++ )
	{
		list< CPtr<NScenario::CScenarioZone> > openedZonesList;
		pGame->pScenarioTracker->GetZonesWhichCanBeOpened( *iClue, &openedZonesList );
		if ( IsValid( pZone ) && ( find( openedZonesList.begin(), openedZonesList.end(), pZone ) == openedZonesList.end() ) )
			continue;
		if ( !IsValid( pZone ) )
		{
			list< CPtr<NScenario::CScenarioZone> > zonesList;
			pGame->pScenarioTracker->GetAvailableZones( &zonesList );

			bool bFound = false;
			for( list< CPtr<NScenario::CScenarioZone> >::const_iterator iZone = openedZonesList.begin(); iZone != openedZonesList.end(); iZone++ )
			{
				if ( find( zonesList.begin(), zonesList.end(), *iZone ) == zonesList.end() )
					continue;

				bFound = true;
				break;
			}

			if ( bFound )
				continue;
		}

		zoneCluesList.push_back( *iClue );
	}
	zoneCluesList.sort( SCluesSort() );

	GenerateListSimple( 1, L"\t", zoneCluesList, &nCount );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// COptionsInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCluesInterface: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CCluesInterface);
private:
	NInput::CBind bindClose;

	ZDATA
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	CPtr<NRPG::CGlobalGame> pGame;
	////
	CObj<NUI::CWindow> pCluesUI;
	CObj<NUI::CScreenShot> pScreenShot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pCursor); f.Add(3,&pInterface); f.Add(4,&pGame); f.Add(5,&pCluesUI); f.Add(6,&pScreenShot); return 0; }

public:
	CCluesInterface();

	void Initialize( NRPG::CGlobalGame *pGame );

	void Step();
	void OnGetFocus() {}
	bool ProcessEvent( const NInput::SEvent &eEvent );
	void RenderFrame();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CCluesInterface::CCluesInterface():
	bindClose( "cancel" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCluesInterface::Initialize( NRPG::CGlobalGame *_pGame )
{
	pGame = _pGame;

	pCursor = NUI::ICursor::Create();
	pInterface = new NUI::CInterface( pCursor );

	pCluesUI = new NUI::CCluesUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "clues", NUI::STYLE_ENABLED ), pGame );
	NUI::LoadTemplate( pCluesUI, NDb::GetUIContainer( 181 ) );

	pScreenShot = new NUI::CScreenShot( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "clues", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_BOTTOMMOST ) );
	pScreenShot->SetMode( NUI::CScreenShot::BLACKANDWHITE, CVec4( 0.5f, 0.5f, 0.5f, 1 ) );
	pScreenShot->Generate();

	pCluesUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCluesInterface::Step()
{
	MarkNewDGFrame();
	if ( CanRender() )
	{
		pInterface->UpdateCursor();
		pInterface->Step( GetTime() );
		RenderFrame();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCluesInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	pCursor->ProcessEvent( sEvent );

	if ( pInterface->ProcessEvent( sEvent ) )
		return true;

	if ( bindClose.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NMainLoop::CICExitModal() );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCluesInterface::RenderFrame()
{
	NGScene::ClearScreen( CVec3(0.5f, 0.5f, 0.5f ) );

	pInterface->Draw( GetTime() );

	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICMission
////////////////////////////////////////////////////////////////////////////////////////////////////
CICClues::CICClues( NRPG::CGlobalGame *_pGame ):
	pGame( _pGame )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICClues::Exec()
{
	CCluesInterface *pRes = new CCluesInterface();
	pRes->Initialize( pGame );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0820130, CCluesUI );
REGISTER_SAVELOAD_CLASS( 0xB0820131, CJournalItem )
REGISTER_SAVELOAD_TEMPL_CLASS( 0xB0820132, CScrollWindow<CText>, CScrollWindow );
REGISTER_SAVELOAD_TEMPL_CLASS( 0xB0820133, CScrollWindow<CListView>, CScrollWindow );
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB082013A, CCluesInterface );
