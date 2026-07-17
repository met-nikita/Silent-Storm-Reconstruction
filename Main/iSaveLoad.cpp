#include "StdAfx.h"
#include "Gfx.h"
#include "SWTexture.h"
#include "ScreenShot.h"
#include "iMain.h"
#include "G2DView.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iSaveLoad.h"
#include "iSaveManager.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSaveLoadItem
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSaveLoadItem: public CHoverButton
{
	OBJECT_NOCOPY_METHODS(CSaveLoadItem)
private:
	enum { STATE_SELECTED = STATE_NORMAL + 0xFF };

	ZDATA_(CHoverButton)
	string szName;
	////
	bool bSelected;
	CPtr<CImage> pHilight;
	CPtr<CWindow> pName;
	CPtr<CWindow> pDate;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CHoverButton*)this); f.Add(2,&szName); f.Add(3,&bSelected); f.Add(4,&pHilight); f.Add(5,&pName); f.Add(6,&pDate); return 0; }

protected:
	void AddTextState( int nID, const wstring &wsText, const NGfx::SPixel8888 &sColor );
	void OnAction();

public:
	CSaveLoadItem() {}
	CSaveLoadItem( const SWindowInfo &sInfo, const string &szName );

	const string& Get() const;

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CSaveLoadItem::CSaveLoadItem( const SWindowInfo &sInfo, const string &_szName ):
	CHoverButton( sInfo ), szName( _szName ), bSelected( false )
{
	NMainLoop::CSaveManager *pSaveManager = NMainLoop::GetSaveManager();

	wstring wsTemp;
	pSaveManager->GetSlotTime( szName, &wsTemp );

	AddTextState( STATE_NORMAL, GetDBString( 7557 ) + L"<wrapright>" + wsTemp + L"<br><left>" + NStr::ToUnicode( szName ), NGfx::SPixel8888( 0x87, 0x7D, 0x4D, 0x0 ) );
	AddTextState( STATE_HOVER, GetDBString( 7558 ) + L"<wrapright>" + wsTemp + L"<br><left>" + NStr::ToUnicode( szName ), NGfx::SPixel8888( 0x87, 0x7D, 0x4D, 0x66 ) );
	AddTextState( STATE_SELECTED, GetDBString( 7558 ) + L"<wrapright>" + wsTemp + L"<br><left>" + NStr::ToUnicode( szName ), NGfx::SPixel8888( 0x87, 0x7D, 0x4D, 0xFF ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const string& CSaveLoadItem::Get() const
{
	return szName;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSaveLoadItem::ProcessMessage( const SEvent &sEvent )
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
void CSaveLoadItem::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	ForceState( bSelected, STATE_SELECTED );
	CHoverButton::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveLoadItem::AddTextState( int nID, const wstring &wsText, const NGfx::SPixel8888 &sColor )
{
	CWindow *pWindow = AddState( nID );

	CPtr<CText> pText = new CText( SWindowInfo( pWindow, SPoint( 0, 0 ), pWindow->GetSize(), "iml-text", STYLE_ENABLED | STYLE_VISIBLE ) );
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
void CSaveLoadItem::OnAction()
{
	SendMessage( GetParent(), SEvent( EVENT_LISTVIEW_ITEMSELECTED, this ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Save-manager command helpers -- retail NUI free fns (iSaveLoad.obj), convergence W4.
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NUI::SM_Save @0x22ff20: unwind every modal interface above the game (depth-1 CICExitModal)
// then post the save command. NOTE retail's CICSave carries (name, CScreenshotTexture*, false) --
// the dev CICSave has no texture parameter yet (the save-machinery convergence leg owns iMain); the
// texture is accepted here so the call sites match retail and is forwarded once CICSave grows it.
static void SM_Save( const string &szName, NGScene::CScreenshotTexture *pScreenShotTexture )
{
	int nCount = NMainLoop::GetInterfaceStackDepth();
	for ( int nTemp = 1; nTemp < nCount; nTemp++ )
		NMainLoop::Command( new NMainLoop::CICExitModal() );

	NMainLoop::Command( new NMainLoop::CICSave( szName ) );   // retail: CICSave( szName, pScreenShotTexture, false )
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NUI::SM_Load @0x22fd30: post a CICLoad for the slot.
static void SM_Load( const string &szName )
{
	NMainLoop::Command( new NMainLoop::CICLoad( szName ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CSaveDeleteDlg::OnOK tail-calls NMainLoop::DeleteSlot @0x237470 (remove the slot dir).
static void SM_Delete( const string &szName )
{
	NMainLoop::GetSaveManager()->DeleteSlot( szName );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBaseDlg -- retail NUI::CBaseDlg (iSaveLoad.obj; UNREGISTERED in retail -- serialized only inline
// through CBaseView::pDialog): the generic modal OK/Cancel confirmation dialog. operator& @0x235500:
// 1=CWindow base, 2=bComplete, 3=pText, 4=pOK, 5=pCancel. OnOK/OnCancel @0x233840 share one body
// (bComplete = true -- the CBaseDlg vftable points both +0x34 and +0x38 slots at it); GetText
// (vtbl+0x3c) defaults to an empty caption and is overridden by the concrete dialogs.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBaseDlg: public CWindow
{
	OBJECT_BASIC_METHODS(CBaseDlg)
protected:
	ZDATA_(CWindow)
	bool bComplete;
	CPtr<CText> pText;
	CObj<CHoverButton> pOK;
	CObj<CHoverButton> pCancel;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&bComplete); f.Add(3,&pText); f.Add(4,&pOK); f.Add(5,&pCancel); return 0; }

public:
	CBaseDlg(): bComplete( false ) {}                                    // retail @0x2346a0
	CBaseDlg( const SWindowInfo &sInfo ): CWindow( sInfo ), bComplete( false ) {}   // retail @0x22ffc0

	bool IsComplete() const { return bComplete; }

	virtual void OnOK() { bComplete = true; }       // retail @0x233840 (vtbl+0x34)
	virtual void OnCancel() { bComplete = true; }   // retail vtbl+0x38 == the same body
	virtual void GetText( wstring *pRes ) {}        // retail vtbl+0x3c default: empty caption

	bool ProcessMessage( const SEvent &sEvent );    // retail @0x2318b0
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CBaseDlg::ProcessMessage @0x2318b0 (disasm 0x6318f5..): TEMPLATELOAD builds the ok/cancel
// hover buttons (hover prefix DB 11197 / normal prefix DB 11198 + the "OK" DB 19055 / "Cancel" DB
// 19056 labels); TEMPLATELOADCOMPLETE grabs the "text" CText label and fills it from the virtual
// GetText; a NOTIFY dispatches ok/cancel (and is swallowed either way); Enter == OK.
bool CBaseDlg::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pOK = new CHoverButton( sEvent.pLoader->GetControl( "ok" ) );
			pOK->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11197 ) + GetDBString( 19055 ) );
			pOK->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11198 ) + GetDBString( 19055 ) );

			pCancel = new CHoverButton( sEvent.pLoader->GetControl( "cancel" ) );
			pCancel->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11197 ) + GetDBString( 19056 ) );
			pCancel->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11198 ) + GetDBString( 19056 ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pText = GetUIWindow<CText>( this, "text" );

			wstring wsText;
			GetText( &wsText );          // virtual (vtbl+0x3c): the concrete dialog's prompt
			pText->SetText( wsText, true );
			break;
		}
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "ok" )
			{
				OnOK();
				return true;
			}
			if ( sEvent.szID == "cancel" )
			{
				OnCancel();
				return true;
			}
			return true;   // the modal dialog swallows every other notify (retail)
		}
	case EVENT_CHAR:
		{
			if ( sEvent.nVal == VK_RETURN )
			{
				OnOK();
				return true;
			}
			break;   // other keys fall through to the base
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSaveDeleteDlg -- retail NUI::CSaveDeleteDlg (unregistered; held by CBaseView::pDialog): the
// "delete this saved game?" confirmation. operator& @0x2358b0: 1=CBaseDlg base, 2=szName (string
// chunk). GetText @0x230070 = DB string 0x4a71 (19057); OnOK @0x22fc80 deletes the slot, completes.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSaveDeleteDlg: public CBaseDlg
{
	OBJECT_BASIC_METHODS(CSaveDeleteDlg)
private:
	ZDATA_(CBaseDlg)
	string szName;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CBaseDlg*)this); f.Add(2,&szName); return 0; }

public:
	CSaveDeleteDlg() {}
	CSaveDeleteDlg( const SWindowInfo &sInfo, const string &_szName ):   // retail @0x230000
		CBaseDlg( sInfo ), szName( _szName ) {}

	void GetText( wstring *pRes ) { *pRes = GetDBString( 19057 ); }   // retail @0x230070 (0x4a71)

	void OnOK()   // retail @0x22fc80: NMainLoop::DeleteSlot(szName); bComplete = true
	{
		SM_Delete( szName );
		bComplete = true;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSaveOverwriteDlg -- retail NUI::CSaveOverwriteDlg (unregistered; held by CBaseView::pDialog):
// the "a save with this name exists -- overwrite?" confirmation. operator& @0x235930: 1=CBaseDlg
// base, 2=szName (string chunk), 3=pScreenShotTexture (CDGPtr<CScreenshotTexture>). GetText
// @0x230190 = DB string 0x4a03 (18947); OnOK @0x230170 saves over the slot, completes.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSaveOverwriteDlg: public CBaseDlg
{
	OBJECT_BASIC_METHODS(CSaveOverwriteDlg)
private:
	ZDATA_(CBaseDlg)
	string szName;
	CDGPtr<NGScene::CScreenshotTexture> pScreenShotTexture;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CBaseDlg*)this); f.Add(2,&szName); f.Add(3,&pScreenShotTexture); return 0; }

public:
	CSaveOverwriteDlg() {}
	CSaveOverwriteDlg( const SWindowInfo &sInfo, const string &_szName, NGScene::CScreenshotTexture *pTexture ):   // retail @0x2300e0
		CBaseDlg( sInfo ), szName( _szName ), pScreenShotTexture( pTexture ) {}

	void GetText( wstring *pRes ) { *pRes = GetDBString( 18947 ); }   // retail @0x230190 (0x4a03)

	void OnOK()   // retail @0x230170: SM_Save(szName, pScreenShotTexture); bComplete = true
	{
		SM_Save( szName, pScreenShotTexture );
		bComplete = true;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHoverTabMustDie -- retail NUI::CHoverTabMustDie (iSaveLoad.obj, saveload id 0xB3619190): the
// "Save"/"Load" tab header of the save/load screen -- a CButton owning a live hover sub-button plus
// a DB string label. operator& @0x235990: 1=CButton base, 2=pButton, 3=pText. Ctor @0x2322e0;
// ProcessMessage @0x230a10.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHoverTabMustDie: public CButton
{
	OBJECT_BASIC_METHODS(CHoverTabMustDie)
private:
	ZDATA_(CButton)
	CObj<CHoverButton> pButton;
	CDBPtr<NDb::CString> pText;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CButton*)this); f.Add(2,&pButton); f.Add(3,&pText); return 0; }

public:
	CHoverTabMustDie() {}
	CHoverTabMustDie( const SWindowInfo &sInfo, NDb::CString *_pText ):   // retail @0x2322e0
		CButton( sInfo ), pText( _pText ) {}

	// retail @0x230a10: TEMPLATELOAD builds the inner hover button from the "text" control with the
	// hover/normal prefixes (DB 7560/7559) + this tab's DB label (disasm 0x630b35: GetDBString(pText),
	// NOT the doubled-prefix the first reconstruction pass guessed); a notify matching the inner
	// button's window id fires OnAction. Everything else -> CButton.
	bool ProcessMessage( const SEvent &sEvent )
	{
		if ( sEvent.nEvent == EVENT_TEMPLATELOAD )
		{
			pButton = new CHoverButton( sEvent.pLoader->GetControl( "text" ) );
			pButton->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 7560 ) + GetDBString( pText ) );
			pButton->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 7559 ) + GetDBString( pText ) );
		}
		else if ( sEvent.nEvent == EVENT_NOTIFY )
		{
			if ( IsValid( pButton ) && ( sEvent.szID == pButton->GetWindowID() ) )
			{
				OnAction();   // CButton::OnAction (vtbl+0x34): notify the parent with our id
				return true;
			}
		}

		return CButton::ProcessMessage( sEvent );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBaseView
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBaseView: public CWindow
{
	OBJECT_BASIC_METHODS(CBaseView);
protected:
	// retail operator& @0x235580: 1=CWindow base, 2=bDefaultSelection, 3=pList, 4=pListView,
	// 5=pScroll, 6=pDialog, 7=pScreenShot, 8=pDelete, 9=pZoneScreenShotTexture (convergence W4:
	// dev lacked pDialog -- which shifted pScreenShot/pDelete -- and pZoneScreenShotTexture).
	ZDATA_(CWindow)
	bool bDefaultSelection;
	CObj<CListView> pList;
	CObj<CScrollWindow<CListView> > pListView;
	////
	CPtr<CScroll> pScroll;
	CObj<CBaseDlg> pDialog;   // the modal delete/overwrite confirmation (retail tag 6)
	CPtr<CScreenShot> pScreenShot;
	CObj<CHoverButton> pDelete;
	CDGPtr<NGScene::CScreenshotTexture> pZoneScreenShotTexture;   // the zone screenshot the save writes (retail tag 9)
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&bDefaultSelection); f.Add(3,&pList); f.Add(4,&pListView); f.Add(5,&pScroll); f.Add(6,&pDialog); f.Add(7,&pScreenShot); f.Add(8,&pDelete); f.Add(9,&pZoneScreenShotTexture); return 0; }

protected:
	void GenerateList();

public:
	CBaseView() {}
	CBaseView( const SWindowInfo &sInfo, bool bDefaultSelection, CScreenShot *pScreenShot, NGScene::CScreenshotTexture *pZoneScreenShotTexture );   // retail @0x230740

	// retail dialog plumbing (convergence W4)
	bool IsDialogMode();                    // retail @0x22fda0: a live, not-yet-complete dialog is open
	void SetDialog( CBaseDlg *pDialog );    // retail @0x233a40
	void ResetDialog();                     // retail @0x2307b0: drop the dialog, re-show
	virtual CSaveLoadItem* GetActiveItem(); // retail @0x2307f0 (vtbl+0x34)

	virtual void SetSelected( CSaveLoadItem *pItem );
	virtual void SaveSlot();
	virtual void LoadSlot( CSaveLoadItem *pItem );
	virtual void DeleteSlot( CSaveLoadItem *pItem );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );   // retail @0x232bd0
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x230740: the binary added the CScreenshotTexture param the Jan03/dev ctor lacked.
CBaseView::CBaseView( const SWindowInfo &sInfo, bool _bDefaultSelection, CScreenShot *_pScreenShot, NGScene::CScreenshotTexture *_pZoneScreenShotTexture ):
	CWindow( sInfo ), bDefaultSelection( _bDefaultSelection ), pScreenShot( _pScreenShot ), pZoneScreenShotTexture( _pZoneScreenShotTexture )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x22fda0: true iff a dialog is attached, alive, and NOT yet complete.
bool CBaseView::IsDialogMode()
{
	return IsValid( pDialog ) && !pDialog->IsComplete();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x233a40: plain CObj swap.
void CBaseView::SetDialog( CBaseDlg *_pDialog )
{
	pDialog = _pDialog;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2307b0: drop the dialog and re-show this view.
void CBaseView::ResetDialog()
{
	pDialog = 0;
	ShowWindow( SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2307f0: the selected list row as a CSaveLoadItem (null when nothing is selected or the
// row is not a save item / flagged dead).
CSaveLoadItem* CBaseView::GetActiveItem()
{
	int nID = pList->GetSelectedItem();
	if ( nID == -1 )
		return 0;

	CPtr<CSaveLoadItem> pItem = dynamic_cast<CSaveLoadItem*>( pList->GetItem( pList->GetSelectedItem() ) );
	if ( !IsValid( pItem ) )
		return 0;
	return pItem;   // borrowed (non-owning), exactly the retail temp-ref dance
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x22fdc0: live-item guard, then screenshot + button styles.
void CBaseView::SetSelected( CSaveLoadItem *pItem )
{
	if ( !IsValid( pItem ) )
		return;

	NMainLoop::CSaveManager *pSaveManager = NMainLoop::GetSaveManager();
	CArray2D<NGfx::SPixel8888> sScreenShot;
	pSaveManager->GetSlotScreenShot( pItem->Get(), &sScreenShot );
	pScreenShot->Set( sScreenShot );
	pScreenShot->SetStyle( STYLE_VISIBLE, true );

	pDelete->SetStyle( STYLE_ENABLED, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseView::SaveSlot()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseView::LoadSlot( CSaveLoadItem *pItem )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x230890 (convergence W4): the delete is CONFIRMED first -- pop a modal CSaveDeleteDlg
// over the whole interface (template container 420, style 0xe = VISIBLE|ENABLED|TOPMOST, 1024x768);
// the actual erase happens in the dialog's OnOK, and CBaseView::Draw regenerates the list once the
// dialog completes. (Jan03/dev deleted immediately.)
void CBaseView::DeleteSlot( CSaveLoadItem *pItem )
{
	if ( !IsValid( pItem ) )
		return;

	CSaveDeleteDlg *pDlg = new CSaveDeleteDlg(
		SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 1024, 768 ), "deletedlg", STYLE_VISIBLE | STYLE_ENABLED | STYLE_TOPMOST ),
		pItem->Get() );
	LoadTemplate( pDlg, NDb::GetUIContainer( 420 ) );
	pDlg->ShowWindow( SWTYPE_SHOW );
	pDialog = pDlg;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x232bd0: once the attached dialog completes, drop it and regenerate the list (a finished
// delete/overwrite changed the slots); then the base paint.
void CBaseView::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( IsValid( pDialog ) && pDialog->IsComplete() )
	{
		pDialog = 0;
		GenerateList();
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2326f0. ORIGINAL BUG (confirmed @0x6326f0): the load/view/delete NOTIFY arms return
// true even when GetActiveItem() is null (no live selection) -- Jan03/dev instead fell through to
// CWindow::ProcessMessage; the action bodies are null-guarded so this is harmless. The VK_DELETE
// key shares the delete arm AFTER the base router declined the event.
bool CBaseView::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "save_slot" )
			{
				SaveSlot();
				return true;
			}
			if ( sEvent.szID == "load_slot" )
			{
				LoadSlot( GetActiveItem() );
				return true;
			}
			if ( sEvent.szID == "delete_slot" )
			{
				DeleteSlot( GetActiveItem() );
				return true;
			}
			if ( sEvent.szID == "view" )
			{
				SetSelected( GetActiveItem() );
				return true;
			}

			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pDelete = new CHoverButton( sEvent.pLoader->GetControl( "delete_slot" ) );
			pDelete->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 7560 ) + GetDBString( 7564 ) );
			pDelete->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 7559 ) + GetDBString( 7564 ) );
			pDelete->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 7561 ) + GetDBString( 7564 ) );
			pDelete->SetStyle( STYLE_ENABLED, false );

			pListView = new CScrollWindow<CListView>( sEvent.pLoader->GetControl( "view" ) );
			pList = pListView->GetClientWindow();
			if ( bDefaultSelection )
				pList->SetStyle( LVSTYLE_SHOWSELALWAYS, true );

			GenerateList();
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pScroll = GetUIWindow<CScroll>( this, "scroll" );
			pListView->SetVScroll( pScroll );
			break;
		}
	}

	if ( CWindow::ProcessMessage( sEvent ) )
		return true;

	// retail tail: an unclaimed DirectInput Delete keypress deletes the selected slot.
	if ( ( sEvent.nEvent != EVENT_CHAR ) || ( sEvent.nVal != VK_DELETE ) )
		return false;
	DeleteSlot( GetActiveItem() );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseView::GenerateList()
{
	NMainLoop::CSaveManager *pSaveManager = NMainLoop::GetSaveManager();

	list<string> slotsList;
	pSaveManager->GetSlotsList( &slotsList );

	pList->RemoveAllItems();

	int nCount = 0;
	for ( list<string>::const_iterator iTemp = slotsList.begin(); iTemp != slotsList.end(); iTemp++ )
	{
		pList->AddItem( nCount, new CSaveLoadItem( SWindowInfo( pList, SPoint( 0, 0 ), SPoint( pList->GetSize().x, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), (*iTemp) ) );
		nCount++;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLoadView
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLoadView: public CBaseView
{
	OBJECT_BASIC_METHODS(CLoadView);
private:
	// retail operator& @0x2359e0: 1=CBaseView base, 2=bAllowSave, 3=pLoad, 4=pSaveTab (convergence
	// W4: dev lacked bAllowSave and built the save tab as a plain CHoverButton "tab_save_text").
	ZDATA_(CBaseView)
	bool bAllowSave;
	CObj<CHoverButton> pLoad;
	CObj<CHoverTabMustDie> pSaveTab;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CBaseView*)this); f.Add(2,&bAllowSave); f.Add(3,&pLoad); f.Add(4,&pSaveTab); return 0; }

public:
	CLoadView(): bAllowSave( true ) {}
	CLoadView( const SWindowInfo &sInfo, bool bAllowSave, CScreenShot *pScreenShot, NGScene::CScreenshotTexture *pZoneScreenShotTexture );   // retail @0x230c40

	void LoadSlot( CSaveLoadItem *pItem );   // retail @0x22fe80

	bool ProcessMessage( const SEvent &sEvent );   // retail @0x232c30
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CLoadView::CLoadView( const SWindowInfo &sInfo, bool _bAllowSave, CScreenShot *pScreenShot, NGScene::CScreenshotTexture *pZoneScreenShotTexture ):
	CBaseView( sInfo, true, pScreenShot, pZoneScreenShotTexture ), bAllowSave( _bAllowSave )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x22fe80: live-item guard, then SM_Load (post the CICLoad).
void CLoadView::LoadSlot( CSaveLoadItem *pItem )
{
	if ( !IsValid( pItem ) )
		return;
	SM_Load( pItem->Get() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x232c30: the save tab is a CHoverTabMustDie over the "tab_save" control with the DB 7565
// label, shown only when saving is allowed; Enter loads the selected slot.
bool CLoadView::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pLoad = new CHoverButton( sEvent.pLoader->GetControl( "load_slot" ) );
			pLoad->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 7560 ) + GetDBString( 7563 ) );
			pLoad->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 7559 ) + GetDBString( 7563 ) );
			pLoad->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 7561 ) + GetDBString( 7563 ) );

			pSaveTab = new CHoverTabMustDie( sEvent.pLoader->GetControl( "tab_save" ), NDb::GetString( 7565 ) );
			pSaveTab->SetStyle( STYLE_VISIBLE, bAllowSave );   // hide the Save tab when saving is disallowed (retail)
			break;
		}
	case EVENT_CHAR:
		{
			if ( sEvent.nVal == VK_RETURN )
			{
				LoadSlot( GetActiveItem() );
				return true;
			}
			break;
		}
	}

	return CBaseView::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSaveView
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSaveView: public CBaseView
{
	OBJECT_BASIC_METHODS(CSaveView);
private:
	// retail operator& @0x235b10: 1=CBaseView base, 2=pEdit, 3=pSave, 4=pLoadTab (CHoverTabMustDie),
	// 5=szLastEditString (string chunk -- the last typed name, gating the Save button refresh).
	ZDATA_(CBaseView)
	CPtr<CEdit> pEdit;
	CObj<CHoverButton> pSave;
	CObj<CHoverTabMustDie> pLoadTab;
	string szLastEditString;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CBaseView*)this); f.Add(2,&pEdit); f.Add(3,&pSave); f.Add(4,&pLoadTab); f.Add(5,&szLastEditString); return 0; }

public:
	CSaveView() {}
	CSaveView( const SWindowInfo &sInfo, CScreenShot *pScreenShot, NGScene::CScreenshotTexture *pZoneScreenShotTexture );   // retail @0x230c80

	void SetSelected( CSaveLoadItem *pItem );   // retail @0x22fea0
	void SaveSlot();                            // retail @0x231690

	bool ProcessMessage( const SEvent &sEvent );   // retail @0x232fd0
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );   // retail @0x233420
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CSaveView::CSaveView( const SWindowInfo &sInfo, CScreenShot *pScreenShot, NGScene::CScreenshotTexture *pZoneScreenShotTexture ):
	CBaseView( sInfo, false, pScreenShot, pZoneScreenShotTexture )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x22fea0: live-item guard, load the slot name into the edit, then the base behaviour.
void CSaveView::SetSelected( CSaveLoadItem *pItem )
{
	if ( !IsValid( pItem ) )
		return;

	pEdit->SetText( NStr::ToUnicode( pItem->Get() ) );

	CBaseView::SetSelected( pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x231690 (convergence W4): validate the typed name (non-empty AND not a reserved slot
// name); a NEW name saves immediately (SM_Save = modal unwind + CICSave), an EXISTING name pops the
// modal CSaveOverwriteDlg (template container 420) whose OnOK performs the overwrite.
void CSaveView::SaveSlot()
{
	wstring wsTemp( pEdit->GetText() );
	string szName( NStr::ToAscii( wsTemp ) );
	if ( szName.empty() )
		return;
	if ( !NMainLoop::IsValidCustomName( szName ) )
		return;

	list<string> slotsList;
	NMainLoop::GetSaveManager()->GetSlotsList( &slotsList );

	if ( find( slotsList.begin(), slotsList.end(), szName ) == slotsList.end() )
	{
		SM_Save( szName, pZoneScreenShotTexture );
	}
	else
	{
		CSaveOverwriteDlg *pDlg = new CSaveOverwriteDlg(
			SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 1024, 768 ), "overwritedlg", STYLE_VISIBLE | STYLE_ENABLED | STYLE_TOPMOST ),
			szName, pZoneScreenShotTexture );
		LoadTemplate( pDlg, NDb::GetUIContainer( 420 ) );
		pDlg->ShowWindow( SWTYPE_SHOW );
		SetDialog( pDlg );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x232fd0: the load tab is a CHoverTabMustDie over "tab_load" with the DB 7566 label; the
// edit is seeded with DB 20240 + format DB 10059; Enter saves.
bool CSaveView::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pSave = new CHoverButton( sEvent.pLoader->GetControl( "save_slot" ) );
			pSave->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 7560 ) + GetDBString( 7562 ) );
			pSave->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 7559 ) + GetDBString( 7562 ) );
			pSave->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 7561 ) + GetDBString( 7562 ) );

			pLoadTab = new CHoverTabMustDie( sEvent.pLoader->GetControl( "tab_load" ), NDb::GetString( 7566 ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pEdit = GetUIWindow<CEdit>( this, "edit" );
			pEdit->SetMode( CEdit::FILENAME );
			pEdit->SetEditSize( 32 );
			pEdit->SetText( GetDBString( 20240 ) );   // retail @0x6330a2: the default save-name seed (0x4f10)
			pEdit->SetTextFormat( GetDBString( 10059 ) );
			break;
		}
	case EVENT_CHAR:
		{
			if ( sEvent.nVal == VK_RETURN )
			{
				SaveSlot();
				return true;
			}
			break;
		}
	}

	return CBaseView::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x233420: when the typed name changed since the last frame, cache it and enable the Save
// button iff the name is a valid custom name; then the base paint (which also reaps a completed
// confirmation dialog).
void CSaveView::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	string szName( NStr::ToAscii( pEdit->GetText() ) );
	if ( szName != szLastEditString )
	{
		szLastEditString = szName;
		pSave->SetStyle( STYLE_ENABLED, NMainLoop::IsValidCustomName( szName ) );
	}

	CBaseView::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSaveLoadUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSaveLoadUI: public CWindow
{
	OBJECT_BASIC_METHODS(CSaveLoadUI);
private:
	ZDATA_(CWindow)
	bool bAllowSave;   // retail tag 2 (save-allowed gate; wiring of the writer = W3)
	CPtr<CWindow> pView;
	CObj<CBaseView> pModeView;   // retail type: CObj<NUI::CBaseView> (op& @0x235770 serializes CallObjectSerialize<CObj<CBaseView>>)
	CPtr<CScreenShot> pScreenShot;
	CObj<CFlashButton> pCloseButton;
	CArray2D<NGfx::SPixel8888> sScreenShot;   // dev-only pixel copy, NOT in the retail format (dropped from operator&, kept for the dev draw path)
	CDGPtr<NGScene::CScreenshotTexture> pScreenShotTexture;   // retail tag 7: the save-slot screenshot texture
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&bAllowSave); f.Add(3,&pView); f.Add(4,&pModeView); f.Add(5,&pScreenShot); f.Add(6,&pCloseButton); f.Add(7,&pScreenShotTexture); return 0; }   // retail @0x235770 (convergence W2; dev was -1-shifted from tag 2 and wrote the raw pixel array)

public:
	CSaveLoadUI(): bAllowSave( true ) {}
	CSaveLoadUI( const SWindowInfo &sInfo, const CArray2D<NGfx::SPixel8888> &sScreenShot, NGScene::CScreenshotTexture *pScreenShotTexture = 0, bool bAllowSave = true );   // retail 3-arg ctor carries the save gate (@0x2310b0 caller)

	void SetMode( NGame::ESaveLoadType eType );

	bool IsDialogMode() { return IsValid( pModeView ) && pModeView->IsDialogMode(); }   // used by the close-bind gate (retail reads pModeView->pDialog directly)
	void ResetDialog();   // retail @0x233bf0

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CSaveLoadUI::CSaveLoadUI( const SWindowInfo &sInfo, const CArray2D<NGfx::SPixel8888> &_sScreenShot, NGScene::CScreenshotTexture *_pScreenShotTexture, bool _bAllowSave ):
	CWindow( sInfo ), sScreenShot( _sScreenShot ), pScreenShotTexture( _pScreenShotTexture ), bAllowSave( _bAllowSave )   // retail tag 7 + tag 2 writers (W3)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail SetMode @0x230d60 (convergence W4): an outer `if (bAllowSave || eType != SAVE)` guard --
// when saving is disallowed SetMode(SAVE) is a complete no-op; the zone screenshot texture is
// forwarded into the mode view (it ends up in the save via the overwrite dialog / SM_Save), and
// LOAD receives the bAllowSave gate for its Save tab.
void CSaveLoadUI::SetMode( NGame::ESaveLoadType eType )
{
	if ( !bAllowSave && ( eType == NGame::SAVE ) )
		return;

	switch( eType )
	{
	case NGame::SAVE:
		// retail: SetStyle(VISIBLE) + pScreenShot->SetTexture(pScreenShotTexture); the pixel-array
		// Set() is the dev fallback for the no-texture path (see CSaveLoadMenuInterface::Initialize).
		if ( IsValid( pScreenShotTexture ) )
			pScreenShot->SetTexture( pScreenShotTexture );
		else
			pScreenShot->Set( sScreenShot );
		pScreenShot->SetStyle( STYLE_VISIBLE, true );
		pModeView = new CSaveView( SWindowInfo( pView, SPoint( 0, 0 ), pView->GetSize(), "", STYLE_VISIBLE | STYLE_ENABLED ), pScreenShot, pScreenShotTexture );
		LoadTemplate( pModeView, NDb::GetUIContainer( 340 ) );
		break;
	case NGame::LOAD:
		pScreenShot->SetStyle( STYLE_VISIBLE, false );
		pModeView = new CLoadView( SWindowInfo( pView, SPoint( 0, 0 ), pView->GetSize(), "", STYLE_VISIBLE | STYLE_ENABLED ), bAllowSave, pScreenShot, pScreenShotTexture );
		LoadTemplate( pModeView, NDb::GetUIContainer( 341 ) );
		break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x233bf0: dismiss the active view's confirmation dialog and re-show.
void CSaveLoadUI::ResetDialog()
{
	if ( IsValid( pModeView ) )
		pModeView->ResetDialog();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSaveLoadUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			if ( ( sEvent.szID == "tab_save" ) || ( sEvent.szID == "tab_save_text" ) )
			{
				SetMode( NGame::SAVE );
				return true;
			}
			else if ( ( sEvent.szID == "tab_load" ) || ( sEvent.szID == "tab_load_text" ) )
			{
				SetMode( NGame::LOAD );
				return true;
			}

			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pScreenShot = new CScreenShot( sEvent.pLoader->GetControl( "screenshot" ) );
			pCloseButton = new CFlashButton( sEvent.pLoader->GetControl( "cancel" ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pView = GetUIWindow<CWindow>( this, "view" );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSaveLoadMenuInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSaveLoadMenuInterface: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CSaveLoadMenuInterface);
private:
	NInput::CBind bindClose;

	ZDATA
	ESaveLoadType eType;
	////
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	////
	CObj<NUI::CSaveLoadUI> pMenuUI;
	CObj<NUI::CScreenShot> pScreenShot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bAllowSave); f.Add(3,&eType); f.Add(4,&pCursor); f.Add(5,&pInterface); f.Add(6,&pMenuUI); f.Add(7,&pScreenShot); return 0; }
	bool bAllowSave;  // place as FIRST member inside the ZDATA block, before eType, to land at offset 20 (after NInput::CBind bindClose @12) and match release layout = false;

public:
	CSaveLoadMenuInterface();

	void Initialize( ESaveLoadType eType, NGScene::CScreenshotTexture *pScreenShotTexture = 0, bool bAllowSave = true );   // retail @0x2310b0 stores the gate + forwards it into the UI

	void Step();
	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &eEvent );
	void RenderFrame();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CSaveLoadMenuInterface::CSaveLoadMenuInterface():
	bindClose( "cancel" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveLoadMenuInterface::Initialize( ESaveLoadType _eType, NGScene::CScreenshotTexture *pScreenShotTexture, bool _bAllowSave )
{
	eType = _eType;
	bAllowSave = _bAllowSave;   // retail @0x2310b0: stored, then forwarded into the CSaveLoadUI ctor

	pCursor = NUI::ICursor::Create();
	pInterface = new NUI::CInterface( pCursor );

	CArray2D<NGfx::SPixel8888> sScreenShot;

	pScreenShot = new NUI::CScreenShot( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "screenshot", NUI::STYLE_ENABLED | NUI::STYLE_VISIBLE | NUI::STYLE_BOTTOMMOST ) );
	pScreenShot->SetMode( NUI::CScreenShot::BLACKANDWHITE, CVec4( 0.5f, 0.5f, 0.5f, 1 ) );
	if ( !IsValid( pScreenShotTexture ) )
	{
		NGfx::MakeScreenShot( &sScreenShot, true );
		pScreenShot->Set( sScreenShot );
	}
	else
	{
		pScreenShotTexture->Get( &sScreenShot );
		pScreenShot->SetTexture( pScreenShotTexture );
	}

	CDGPtr<NGScene::CBilinearTexture> pTexture = new NGScene::CBilinearTexture( sScreenShot, NMainLoop::N_SAVE_SCREENSHOT_X, NMainLoop::N_SAVE_SCREENSHOT_Y );
	pTexture.Refresh();
	CObj<NGScene::CSWTextureData> pData = pTexture->GetValue();
	CArray2D<NGfx::SPixel8888> &sScreenShot320x200 = pData->mips.front();

	pMenuUI = new NUI::CSaveLoadUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "saveloadmenu", NUI::STYLE_ENABLED ), sScreenShot320x200, pScreenShotTexture, bAllowSave );
	NUI::LoadTemplate( pMenuUI, NDb::GetUIContainer( 335 ) );
	pMenuUI->ShowWindow( NUI::SWTYPE_SHOW );

	pMenuUI->SetMode( eType );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveLoadMenuInterface::Step()
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
void CSaveLoadMenuInterface::OnGetFocus()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSaveLoadMenuInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	NInput::SetSection( "menu" );

	pCursor->ProcessEvent( sEvent );

	if ( pInterface->ProcessEvent( sEvent ) )
		return true;

	if ( bindClose.ProcessEvent( sEvent ) )
	{
		// retail CSaveLoadMenuInterface::ProcessEvent @0x2314c0 (convergence W4): if the active
		// view's confirmation dialog is still open, Escape DISMISSES the dialog instead of tearing
		// the whole save/load screen down.
		if ( IsValid( pMenuUI ) && pMenuUI->IsDialogMode() )
		{
			pMenuUI->ResetDialog();
			return true;
		}

		NMainLoop::Command( new NMainLoop::CICExitModal() );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveLoadMenuInterface::RenderFrame()
{
	NGScene::ClearScreen( CVec3(0.5f, 0.5f, 0.5f ) );
	pInterface->Draw( GetTime() );
	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICMission
////////////////////////////////////////////////////////////////////////////////////////////////////
CICSaveLoadMenu::CICSaveLoadMenu( ESaveLoadType _eType, NGScene::CScreenshotTexture *_pScreenShotTexture, bool _bAllowSave ):
	eType( _eType ), pScreenShotTexture( _pScreenShotTexture ), bAllowSave( _bAllowSave )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICSaveLoadMenu::Exec()
{
	CSaveLoadMenuInterface *pRes = new CSaveLoadMenuInterface();
	pRes->Initialize( eType, pScreenShotTexture, bAllowSave );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB1014131, CSaveLoadUI );
REGISTER_SAVELOAD_CLASS( 0xB1014132, CSaveLoadItem );
REGISTER_SAVELOAD_CLASS( 0xB1014133, CBaseView );
REGISTER_SAVELOAD_CLASS( 0xB1014134, CLoadView );
REGISTER_SAVELOAD_CLASS( 0xB1014135, CSaveView );
REGISTER_SAVELOAD_CLASS( 0xB3619190, CHoverTabMustDie );   // retail NUI::CHoverTabMustDie id (gen/classreg.json)
// NOTE: CBaseDlg / CSaveDeleteDlg / CSaveOverwriteDlg are deliberately NOT registered -- retail
// leaves them unregistered too (gen/classreg.json has no ids for them; they serialize only inline
// through CBaseView::pDialog).
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB1014130, CSaveLoadMenuInterface );
