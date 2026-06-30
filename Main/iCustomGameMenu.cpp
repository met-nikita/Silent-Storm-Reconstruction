#include "StdAfx.h"
#include "iMain.h"
#include "G2DView.h"
#include "..\Misc\StrProc.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Interface.h"
#include "Cursor.h"
#include "UIBaseCtrls.h"
#include "UICommCtrls.h"
#include "iCommonUI.h"
#include "iMainMenu.h"
#include "iCustomGameMenu.h"
#include "ModManager.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// iCustomGameMenu -- the "custom game" (mods browser) screen. Reconstructed from
// .\release\iCustomGameMenu.obj (Game.exe). Structurally the twin of iSaveLoad.cpp:
//   CCustomGameItem        ~ CSaveLoadItem        (a CHoverButton list row carrying one SModInfo)
//   CCustomGameView        ~ CBaseView            (a two-CListView mod picker: active | available)
//   CCustomGameUI          ~ CSaveLoadUI          (the in-screen CWindow owning view + button row)
//   CCustomGameMenuInterface ~ CSaveLoadMenuInterface (the modal IInterfaceBase screen)
//   CICCustomGameMenu      ~ CICSaveLoadMenu       (the queued command that opens it)
// SModInfo + CModManager (the mod enumerator/activator) already live in ModManager.{h,cpp}.
//
// Nothing in the dev tree issues CICCustomGameMenu yet -> the entire module is behaviour-neutral.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
void MoveItem( CListView *pSrc, CListView *pDst );
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameItem -- one mod row (a CHoverButton carrying its SModInfo). @0xCC mod / @0xE4 bSelected
// / @0xE8 pHilight / @0xEC pName -> sizeof 0xF0, matching operator_new(0xf0) in MoveItem.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCustomGameItem: public CHoverButton
{
	OBJECT_NOCOPY_METHODS(CCustomGameItem)
private:
	enum { STATE_SELECTED = STATE_NORMAL + 0xFF };

	ZDATA_(CHoverButton)
	SModInfo mod;
	////
	bool bSelected;
	CPtr<CImage> pHilight;
	CPtr<CWindow> pName;
	// SModInfo has no operator& (plain POD in ModManager.h) -> serialize its two strings directly.
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CHoverButton*)this); f.Add(2,&mod.szName); f.Add(3,&mod.szDirectory); f.Add(4,&bSelected); f.Add(5,&pHilight); f.Add(6,&pName); return 0; }

protected:
	void AddTextState( int nID, const wstring &wsText, const NGfx::SPixel8888 &sColor );
	void OnAction();

public:
	CCustomGameItem() {}
	CCustomGameItem( const SWindowInfo &sInfo, const SModInfo &mod );

	const SModInfo* Get() const;

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameItem::CCustomGameItem @0x1cf060
// NOTE: the three captions are `GetDBString(markup) + NStr::ToUnicode( mod.szName )` (decomp shows
// GetDBString + NStr::ToUnicode + nstl::operator+<unsigned short>). The caption markup DB ids are
// garbled in the decomp; 0x2b79 (normal) / 0x2b7a (hover) are the main-menu hover-button markup ids
// (iMainMenu.cpp) and are reused here. The per-state tint RGB 0x877d4d with rising alpha
// 0x00/0x66/0xff IS recovered from the decomp (local_4=0x66877d4d, local_8=0xff877d4d).
CCustomGameItem::CCustomGameItem( const SWindowInfo &sInfo, const SModInfo &_mod ):
	CHoverButton( sInfo ), mod( _mod ), bSelected( false )
{
	AddTextState( STATE_NORMAL,   GetDBString( 0x2b79 ) + NStr::ToUnicode( mod.szName ), NGfx::SPixel8888( 0x87, 0x7D, 0x4D, 0x00 ) );
	AddTextState( STATE_HOVER,    GetDBString( 0x2b7a ) + NStr::ToUnicode( mod.szName ), NGfx::SPixel8888( 0x87, 0x7D, 0x4D, 0x66 ) );
	AddTextState( STATE_SELECTED, GetDBString( 0x2b7a ) + NStr::ToUnicode( mod.szName ), NGfx::SPixel8888( 0x87, 0x7D, 0x4D, 0xFF ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameItem::Get @0x1ce000 -- &this->mod.
const SModInfo* CCustomGameItem::Get() const
{
	return &mod;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameItem::ProcessMessage @0x1ce010 -- sniff the listview-item-selected notify, then chain
// to the CButton base (decomp tail-calls CButton::ProcessMessage @0x714f60).
bool CCustomGameItem::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_LISTVIEW_ITEMSELECTED:
		{
			if ( sEvent.nVal == ELV_ITEMSSELECTED_TRUE )
				bSelected = true;
			else
				bSelected = false;

			break;
		}
	}

	return CButton::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameItem::Draw @0x1ce040 -- pin the hover state to "selected" iff this row is selected.
void CCustomGameItem::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	ForceState( bSelected, STATE_SELECTED );
	CHoverButton::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameItem::AddTextState @0x1ce240 -- byte-identical chain to CSaveLoadItem::AddTextState:
// add the empty state window, lay an "iml-text" CMLText over it, size to {stateWidth, textHeight},
// add a "hilight" CImage of the same size tinted by sColor, then grow self to enclose it.
void CCustomGameItem::AddTextState( int nID, const wstring &wsText, const NGfx::SPixel8888 &sColor )
{
	CWindow *pWindow = AddState( nID );

	CPtr<CMLText> pText = new CMLText( SWindowInfo( pWindow, SPoint( 0, 0 ), pWindow->GetSize(), "iml-text", STYLE_ENABLED | STYLE_VISIBLE ) );
	pText->SetText( wsText );

	SPoint sSize;
	pText->GetRealSize( &sSize );

	sSize.x = pWindow->GetSize().x;
	pWindow->SetSize( sSize );
	pText->SetSize( sSize );

	CPtr<CImage> pImage = new CImage( SWindowInfo( pWindow, SPoint( 0, 0 ), sSize, "hilight", STYLE_ENABLED | STYLE_VISIBLE | STYLE_BOTTOMMOST ) );
	pImage->SetColor( sColor );

	SetSize( SPoint( Max( sSize.x, GetSize().x ), Max( sSize.y, GetSize().y ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameItem::OnAction @0x1ce530 -- notify the owning list that this row was selected.
void CCustomGameItem::OnAction()
{
	SendMessage( GetParent(), SEvent( EVENT_LISTVIEW_ITEMSELECTED, this ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// NUI::MoveItem @0x1d0680 -- move the selected CCustomGameItem out of pSrc into pDst, preserving its
// list id. (Add: src=right/available, dst=left/active; Remove: the reverse.) The new row is a fresh
// CCustomGameItem built from the moved row's SModInfo (the engine never copies the window itself).
void MoveItem( CListView *pSrc, CListView *pDst )
{
	int nID = pSrc->GetSelectedItem();
	if ( nID == -1 )
		return;

	CCustomGameItem *pItem = dynamic_cast<CCustomGameItem*>( pSrc->GetItem( nID ) );
	if ( IsValid( pItem ) )
	{
		pDst->AddItem( nID, new CCustomGameItem( SWindowInfo( pDst, SPoint( 0, 0 ), SPoint( pDst->GetSize().x, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), *pItem->Get() ) );
		pSrc->RemoveItem( nID );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameView -- the two-list mod picker. @0x80 pAdd / @0x84 pRemove / @0x88 pLeftList /
// @0x8c pLeftListView / @0x90 pRightList / @0x94 pRightListView -> sizeof 0x98 (operator_new 0x98).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCustomGameView: public CWindow
{
	OBJECT_BASIC_METHODS(CCustomGameView);
private:
	ZDATA_(CWindow)
	CObj<CComplexButton> pAdd;
	CObj<CComplexButton> pRemove;
	CObj<CListView> pLeftList;
	CObj<CScrollWindow<CListView> > pLeftListView;
	CObj<CListView> pRightList;
	CObj<CScrollWindow<CListView> > pRightListView;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pAdd); f.Add(3,&pRemove); f.Add(4,&pLeftList); f.Add(5,&pLeftListView); f.Add(6,&pRightList); f.Add(7,&pRightListView); return 0; }

protected:
	void RefreshModsList();

public:
	CCustomGameView() {}
	CCustomGameView( const SWindowInfo &sInfo );

	void GetModsList( vector<SModInfo> *pMods );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameView::CCustomGameView @0x1ce5e0 -- base ctor; the six CObj members null-init themselves.
CCustomGameView::CCustomGameView( const SWindowInfo &sInfo ):
	CWindow( sInfo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameView::GetModsList @0x1cedb0 -- harvest the SModInfo of every CCustomGameItem currently
// in the LEFT (active) list. APPENDS to *pMods (the decomp never clears it first).
void CCustomGameView::GetModsList( vector<SModInfo> *pMods )
{
	list<CPtr<CWindow> > itemsList;
	if ( IsValid( pLeftList ) )
		pLeftList->GetItemsList( &itemsList );

	for ( list<CPtr<CWindow> >::const_iterator iTemp = itemsList.begin(); iTemp != itemsList.end(); iTemp++ )
	{
		CCustomGameItem *pItem = dynamic_cast<CCustomGameItem*>( iTemp->GetPtr() );
		if ( pItem )
			pMods->push_back( *pItem->Get() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameView::ProcessMessage @0x1cf200
bool CCustomGameView::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pAdd = new CComplexButton( sEvent.pLoader->GetControl( "add" ), 0, 0, 0, 0 );
			pRemove = new CComplexButton( sEvent.pLoader->GetControl( "remove" ), 0, 0, 0, 0 );
			// pAdd/pRemove->Set( GetUITexture(..), GetUITexture(..), NORMAL, "" ): the two arrow-icon DB
			// texture ids are garbled in the decomp -> Set() with no icons (the template control art stands).
			pAdd->Set();
			pRemove->Set();

			pLeftListView = new CScrollWindow<CListView>( sEvent.pLoader->GetControl( "left" ) );
			pLeftList = pLeftListView->GetClientWindow();
			pLeftList->SetStyle( LVSTYLE_SHOWSELALWAYS, true );

			pRightListView = new CScrollWindow<CListView>( sEvent.pLoader->GetControl( "right" ) );
			pRightList = pRightListView->GetClientWindow();
			pRightList->SetStyle( LVSTYLE_SHOWSELALWAYS, true );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pLeftListView->SetVScroll( GetUIWindow<CScroll>( this, "left_scroll" ) );
			pRightListView->SetVScroll( GetUIWindow<CScroll>( this, "right_scroll" ) );
			RefreshModsList();
			break;
		}
	case EVENT_NOTIFY:
		{
			if ( IsValid( pAdd ) && ( sEvent.szID == pAdd->GetWindowID() ) )
			{
				MoveItem( pRightList, pLeftList );	// available -> active
				return true;
			}
			else if ( IsValid( pRemove ) && ( sEvent.szID == pRemove->GetWindowID() ) )
			{
				MoveItem( pLeftList, pRightList );	// active -> available
				return true;
			}

			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameView::RefreshModsList -- the EVENT_TEMPLATELOADCOMPLETE mod-enumeration tail. The release
// decomp here is GARBLED (31 unreachable-block warnings; only the CModManager::GetAvailableMods /
// GetActiveMods calls survive, the per-row list-building lost). HAND-AUTHORED to the documented intent
// (verdict + answer key both delegate this whole tail): the active (already-selected) mods fill the
// LEFT list; the remaining available mods fill the RIGHT list. Behaviour-neutral here -- nothing opens
// this screen, and CModManager::Activate is a stub.
void CCustomGameView::RefreshModsList()
{
	if ( !IsValid( pLeftList ) || !IsValid( pRightList ) )
		return;

	vector<SModInfo> availableMods;
	CModManager::GetAvailableMods( &availableMods );
	vector<SModInfo> *pActiveMods = CModManager::GetActiveMods();

	int nID = 0;
	for ( vector<SModInfo>::const_iterator iActive = pActiveMods->begin(); iActive != pActiveMods->end(); iActive++ )
	{
		pLeftList->AddItem( nID, new CCustomGameItem( SWindowInfo( pLeftList, SPoint( 0, 0 ), SPoint( pLeftList->GetSize().x, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), (*iActive) ) );
		nID++;
	}

	nID = 0;
	for ( vector<SModInfo>::const_iterator iAvail = availableMods.begin(); iAvail != availableMods.end(); iAvail++ )
	{
		bool bAlreadyActive = false;
		for ( vector<SModInfo>::const_iterator iActive = pActiveMods->begin(); iActive != pActiveMods->end(); iActive++ )
		{
			if ( iActive->szDirectory == iAvail->szDirectory )
			{
				bAlreadyActive = true;
				break;
			}
		}
		if ( !bAlreadyActive )
		{
			pRightList->AddItem( nID, new CCustomGameItem( SWindowInfo( pRightList, SPoint( 0, 0 ), SPoint( pRightList->GetSize().x, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), (*iAvail) ) );
			nID++;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameView::Draw @0x1ce150 -- enable "Add" only when the RIGHT (available) list has a selection,
// "Remove" only when the LEFT (active) list has one, then paint the child tree. (IsValid guards added
// defensively: before the template loads the children are null; behaviour-equivalent in the live flow.)
void CCustomGameView::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( IsValid( pAdd ) )
		pAdd->SetStyle( STYLE_ENABLED, IsValid( pRightList ) && ( pRightList->GetSelectedItem() != -1 ) );
	if ( IsValid( pRemove ) )
		pRemove->SetStyle( STYLE_ENABLED, IsValid( pLeftList ) && ( pLeftList->GetSelectedItem() != -1 ) );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameUI -- the in-screen widget window. @0x80 pButtonsLine / @0x84 pView -> sizeof 0x88.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCustomGameUI: public CWindow
{
	OBJECT_BASIC_METHODS(CCustomGameUI);
private:
	ZDATA_(CWindow)
	CObj<CButtonsLine> pButtonsLine;
	CObj<CCustomGameView> pView;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pButtonsLine); f.Add(3,&pView); return 0; }

public:
	CCustomGameUI() {}
	CCustomGameUI( const SWindowInfo &sInfo );

	void GetModsList( vector<SModInfo> *pMods );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameUI::CCustomGameUI @0x1ce620 -- base ctor; the two CObj members null-init themselves.
CCustomGameUI::CCustomGameUI( const SWindowInfo &sInfo ):
	CWindow( sInfo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameUI::GetModsList @0x1ceed0 -- forward to the view's mod-list accessor.
void CCustomGameUI::GetModsList( vector<SModInfo> *pMods )
{
	if ( IsValid( pView ) )
		pView->GetModsList( pMods );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameUI::ProcessMessage @0x1ce650 -- on template load (re)build the view + the bottom button
// row, wiring a cancel (tooltip 0x4ded) and an apply (tooltip 0x4db6) hover button; every message
// chains to the CWindow base whose result is returned.
// NOTE: the cancel/apply CAPTION DB ids are garbled in the decomp; the markup 0x2b79/0x2b7a are the
// main-menu hover-button markup ids, and the tooltip id doubles as the caption id (as the main menu's
// customgame button does). The tooltip ids 0x4ded/0x4db6 ARE recovered from the decomp.
bool CCustomGameUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pView = new CCustomGameView( sEvent.pLoader->GetControl( "view" ) );
			pButtonsLine = new CButtonsLine( sEvent.pLoader->GetControl( "line" ) );

			pButtonsLine->AddHoverButton( "", 0x4ded, GetDBString( 0x2b79 ) + GetDBString( 0x4ded ), GetDBString( 0x2b7a ) + GetDBString( 0x4ded ), L"" );
			pButtonsLine->AddHoverButton( "", 0x4db6, GetDBString( 0x2b79 ) + GetDBString( 0x4db6 ), GetDBString( 0x2b7a ) + GetDBString( 0x4db6 ), L"" );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameUI::Draw @0x1ce070 -- paint the visible children back-to-front (the override differs from
// CWindow::Draw only in REVERSE child order). The decomp's (w+7)&0x80==0 test is the not-being-deleted
// bit (== IsValid); (w+0xc)&2 is nStyle & STYLE_VISIBLE (== GetStyle( STYLE_VISIBLE )).
void CCustomGameUI::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	list<CPtr<CWindow> > childrenList;
	FormChildrenList( &childrenList );

	for ( list<CPtr<CWindow> >::reverse_iterator iChild = childrenList.rbegin(); iChild != childrenList.rend(); iChild++ )
	{
		CWindow *pChild = iChild->GetPtr();
		if ( IsValid( pChild ) && pChild->GetStyle( STYLE_VISIBLE ) )
			pChild->Draw( sTime, pView );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NUI
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// PLACEHOLDER: the custom-game UI container (template) id is GARBLED in the Initialize decomp
// (DAT_bf800000 register-tracking loss). The window id is "chargenUI" (shared with chargen). The real
// container id = 441 (0x1b9): raw disasm at RVA 0x1ced05 `mov ecx,0x1b9` feeds NDb::GetUIContainer (the decompiler
// lost the arg to register tracking, rendering it as nCmdShow). 441 is distinct from chargen=345 and mainmenu=347,
// so the old 345 placeholder (== chargen's container) was wrong. The screen is now reached via the main-menu button.
const int N_CUSTOMGAMEMENU_CONTAINER = 441;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameMenuInterface -- the modal custom-game screen. @0x0c bindClose / @0x14 bindApply /
// @0x1c pCursor / @0x20 pInterface / @0x24 pMenuUI -> sizeof 0x28 (operator_new 0x28).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCustomGameMenuInterface: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CCustomGameMenuInterface);
private:
	NInput::CBind bindClose;
	NInput::CBind bindApply;

	ZDATA_(IInterfaceBase)
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	CObj<NUI::CCustomGameUI> pMenuUI;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pCursor); f.Add(3,&pInterface); f.Add(4,&pMenuUI); return 0; }

public:
	CCustomGameMenuInterface();

	void Initialize();

	void Step();
	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &sEvent );
	void RenderFrame();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameMenuInterface::CCustomGameMenuInterface @0x1cea90 -- bind "cancel"/"apply".
CCustomGameMenuInterface::CCustomGameMenuInterface():
	bindClose( "cancel" ), bindApply( "apply" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameMenuInterface::Initialize @0x1ceb40
void CCustomGameMenuInterface::Initialize()
{
	pCursor = NUI::ICursor::Create( false );		// Create( false, { -1, -1 } )
	pInterface = new NUI::CInterface( pCursor );

	pMenuUI = new NUI::CCustomGameUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "chargenUI", NUI::STYLE_ENABLED ) );
	NUI::LoadTemplate( pMenuUI, NDb::GetUIContainer( N_CUSTOMGAMEMENU_CONTAINER ) );	// container id placeholder (see note)
	pMenuUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameMenuInterface::Step @0x1ce1e0
void CCustomGameMenuInterface::Step()
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
// CCustomGameMenuInterface::OnGetFocus @0x1ce080 -- empty body (decomp: ret).
void CCustomGameMenuInterface::OnGetFocus()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameMenuInterface::ProcessEvent @0x1ceee0
//   cancel -> Command( new CICExitModal() );  apply -> Activate( GetModsList() ) + Command( new CICMainMenu() ).
bool CCustomGameMenuInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	NInput::SetSection( "menu" );

	pCursor->ProcessEvent( sEvent );

	if ( pInterface->ProcessEvent( sEvent ) )
		return true;

	if ( bindClose.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NMainLoop::CICExitModal() );
		return true;
	}

	if ( bindApply.ProcessEvent( sEvent ) )
	{
		vector<SModInfo> mods;
		if ( IsValid( pMenuUI ) )
			pMenuUI->GetModsList( &mods );
		CModManager::Activate( mods );				// stub today (sibling deferred); behaviour-neutral
		NMainLoop::Command( new CICMainMenu() );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCustomGameMenuInterface::RenderFrame @0x1ce1b0
// NOTE: release NGScene::ClearScreenZBuffer() is ABSENT from this tree (same as iIntroScreen.cpp's
// convergence note) -> black ClearScreen stand-in.
void CCustomGameMenuInterface::RenderFrame()
{
	NGScene::ClearScreen( CVec3( 0, 0, 0 ) );	// release ClearScreenZBuffer() (absent) -> black clear
	pInterface->Draw( GetTime() );
	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICCustomGameMenu::Exec @0x1ced40 -- build, initialise, and push the custom-game screen (no validity
// gate; the decomp unconditionally news + pushes, and Initialize takes no args).
void CICCustomGameMenu::Exec()
{
	CCustomGameMenuInterface *pRes = new CCustomGameMenuInterface();
	pRes->Initialize();
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NGame
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB3530160, CCustomGameUI );
REGISTER_SAVELOAD_CLASS( 0xB3530162, CCustomGameView );
REGISTER_SAVELOAD_CLASS( 0xB3530163, CCustomGameItem );
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB3530161, CCustomGameMenuInterface );
////////////////////////////////////////////////////////////////////////////////////////////////////
