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
// CCluesUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCluesItem: public CButton
{
	OBJECT_NOCOPY_METHODS(CCluesItem)
private:
	ZDATA_(CButton)
	CPtr<NScenario::CScenarioClue> pClue;
	////
	CObj<CMLText> pText;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CButton*)this); f.Add(2,&pClue); f.Add(3,&pText); return 0; }

protected:
	void OnAction();

public:
	CCluesItem() {}
	CCluesItem( const SWindowInfo &sInfo, NScenario::CScenarioClue *_pClue, const wstring &wsText );

	NScenario::CScenarioClue* Get() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CCluesItem::CCluesItem( const SWindowInfo &sInfo, NScenario::CScenarioClue *_pClue, const wstring &wsText ):
	CButton( sInfo ), pClue( _pClue )
{
	pText = new CMLText( SWindowInfo( this, SPoint( 0, 0 ), GetSize(), "iml-text", STYLE_ENABLED | STYLE_VISIBLE ) );
	pText->SetText( wsText );

	SPoint sSize;
	pText->GetRealSize( &sSize );

	sSize.x = GetSize().x;
	SetSize( sSize );
	pText->SetSize( sSize );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NScenario::CScenarioClue* CCluesItem::Get() const
{
	return pClue;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCluesItem::OnAction()
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
		ZONE_POINT
	};
	ZDATA_(CWindow)
	CPtr<NRPG::CGlobalGame> pGame;
	////
	CObj<CHoverButton> pSortByDate;
	CObj<CHoverButton> pSortByZone;
	CObj<CHoverButton> pSortByPoint;
	////
	CObj<CMLText> pDescription;
	CObj<CScrollWindow<CMLText> > pDescriptionView;
	////
	CObj<CListView> pList;
	CObj<CScrollWindow<CListView> > pListView;
	////
	CPtr<CImage> pSelection;
	CObj<CFlashButton> pCloseButton;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pGame); f.Add(3,&pSortByDate); f.Add(4,&pSortByZone); f.Add(5,&pSortByPoint); f.Add(6,&pDescription); f.Add(7,&pDescriptionView); f.Add(8,&pList); f.Add(9,&pListView); f.Add(10,&pSelection); f.Add(11,&pCloseButton); return 0; }

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
				CPtr<CCluesItem> pItem = dynamic_cast<CCluesItem*>( pList->GetItem( pList->GetSelectedItem() ) );
				if ( IsValid( pItem ) )
				{
					CPtr<NDb::CString> pClueDescription = pGame->pScenarioTracker->GetClueDescriptionFromObjective( pItem->Get() );
					if ( IsValid( pClueDescription ) )
						pDescription->SetText( pClueDescription->szStr );
					else
						pDescription->SetText( L"<color=red>[ERROR]Description not set" );
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

			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pCloseButton = new CFlashButton( sEvent.pLoader->GetControl( "cancel" ) );

			pListView = new CScrollWindow<CListView>( sEvent.pLoader->GetControl( "view" ) );
			pList = pListView->GetClientWindow();

			pDescriptionView = new CScrollWindow<CMLText>( sEvent.pLoader->GetControl( "description" ) );
			pDescription = pDescriptionView->GetClientWindow();

			pSortByDate = new CHoverButton( sEvent.pLoader->GetControl( "sort_bydate" ) );
			pSortByDate->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 7049 ) + GetDBString( 7051 ) );
			pSortByDate->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 7050 ) + GetDBString( 7051 ) );

			pSortByZone = new CHoverButton( sEvent.pLoader->GetControl( "sort_byzone" ) );
			pSortByZone->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 7049 ) + GetDBString( 7052 ) );
			pSortByZone->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 7050 ) + GetDBString( 7052 ) );

			pSortByPoint = new CHoverButton( sEvent.pLoader->GetControl( "sort_bypoint" ) );
			pSortByPoint->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 7049 ) + GetDBString( 7053 ) );
			pSortByPoint->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 7050 ) + GetDBString( 7053 ) );

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
		CPtr<CCluesItem> pItem = dynamic_cast<CCluesItem*>( pList->GetItem( nID ) );
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
				AddSeparator( NStr::ToUnicode( (*iZone)->GetDBZone()->sSmallDescription ), &nCount );
				GenerateListByZone( *iZone, cluesList, &nCount );
			}
			AddSeparator( L"Results", &nCount );
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
				AddSeparator( NStr::ToUnicode( (*iZone)->GetDBZone()->sSmallDescription ), &nCount );
				GenerateListByPoint( *iZone, cluesList, &nCount );
			}
			AddSeparator( L"Results", &nCount );
			GenerateListByPoint( 0, cluesList, &nCount );

			break;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCluesUI::AddSeparator( const wstring &wsText, int *pConut )
{
	int &nCount = (*pConut);

	CPtr<CMLText> pText = new CMLText( SWindowInfo( pList, SPoint( 0, 0 ), SPoint( pList->GetSize().x, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ) );
	pText->SetText( wsText );

	SPoint sSize;
	pText->GetRealSize( &sSize );
	sSize.x = GetSize().x;
	pText->SetSize( sSize );

	pList->AddItem( nCount, pText );
	nCount++;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCluesUI::GenerateListSimple( int nNum, const wstring &wsAdd, const list<CPtr<NScenario::CScenarioClue> > &cluesList, int *pConut )
{
	int &nCount = (*pConut);

	for( list<CPtr<NScenario::CScenarioClue> >::const_iterator iTemp = cluesList.begin(); iTemp != cluesList.end(); iTemp++ )
	{
		WCHAR wsBuffer[1024];
		swprintf( wsBuffer, L"<font face=Courier size=18pt><color=FF2E4051>%d. %s", nNum, GetDBString( (*iTemp)->GetDBClue()->pDescription ).c_str() );

		wstring wsTemp( wsAdd + wsBuffer );
		pList->AddItem( nCount, new CCluesItem( SWindowInfo( pList, SPoint( 0, 0 ), SPoint( pList->GetSize().x, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), (*iTemp), wsTemp ) );
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
REGISTER_SAVELOAD_CLASS( 0xB0820131, CCluesItem )
REGISTER_SAVELOAD_TEMPL_CLASS( 0xB0820132, CScrollWindow<CMLText>, CScrollWindow );
REGISTER_SAVELOAD_TEMPL_CLASS( 0xB0820133, CScrollWindow<CListView>, CScrollWindow );
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB082013A, CCluesInterface );
