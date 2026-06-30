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
void CSaveLoadItem::OnAction()
{
	SendMessage( GetParent(), SEvent( EVENT_LISTVIEW_ITEMSELECTED, this ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBaseView
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBaseView: public CWindow
{
	OBJECT_BASIC_METHODS(CBaseView);
private:
	ZDATA_(CWindow)
	bool bDefaultSelection;
	CObj<CListView> pList;
	CObj<CScrollWindow<CListView> > pListView;
	////
	CPtr<CScroll> pScroll;
	CPtr<CScreenShot> pScreenShot;
	CObj<CHoverButton> pDelete;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&bDefaultSelection); f.Add(3,&pList); f.Add(4,&pListView); f.Add(5,&pScroll); f.Add(6,&pScreenShot); f.Add(7,&pDelete); return 0; }

protected:
	void GenerateList();

public:
	CBaseView() {}
	CBaseView( const SWindowInfo &sInfo, bool bDefaultSelection, CScreenShot *pScreenShot );

	virtual void SetSelected( CSaveLoadItem *pItem );
	virtual void SaveSlot();
	virtual void LoadSlot( CSaveLoadItem *pItem );
	virtual void DeleteSlot( CSaveLoadItem *pItem );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CBaseView::CBaseView( const SWindowInfo &sInfo, bool _bDefaultSelection, CScreenShot *_pScreenShot ): 
	CWindow( sInfo ), bDefaultSelection( _bDefaultSelection ), pScreenShot( _pScreenShot )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBaseView::SetSelected( CSaveLoadItem *pItem )
{
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
void CBaseView::DeleteSlot( CSaveLoadItem *pItem )
{
	NMainLoop::CSaveManager *pSaveManager = NMainLoop::GetSaveManager();
	pSaveManager->DeleteSlot( pItem->Get() );

	GenerateList();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
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
			else
			{
				int nID = pList->GetSelectedItem();
				if ( nID != -1 )
				{
					CPtr<CSaveLoadItem> pItem = dynamic_cast<CSaveLoadItem*>( pList->GetItem( pList->GetSelectedItem() ) );
					if ( IsValid( pItem ) )
					{
						if ( sEvent.szID == "load_slot" )
						{
							LoadSlot( pItem );
							return true;
						}
						else if ( sEvent.szID == "delete_slot" )
						{
							DeleteSlot( pItem );
							return true;
						}
						else if ( sEvent.szID == "view" )
						{
							SetSelected( pItem );
							return true;
						}
					}
				}
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

	return CWindow::ProcessMessage( sEvent );
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
	ZDATA_(CBaseView)
	CObj<CHoverButton> pLoad;
	CObj<CHoverButton> pSaveTabText;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CBaseView*)this); f.Add(2,&pLoad); f.Add(3,&pSaveTabText); return 0; }

public:
	CLoadView() {}
	CLoadView( const SWindowInfo &sInfo, CScreenShot *pScreenShot );

	void LoadSlot( CSaveLoadItem *pItem );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CLoadView::CLoadView( const SWindowInfo &sInfo, CScreenShot *pScreenShot ): 
	CBaseView( sInfo, true, pScreenShot )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLoadView::LoadSlot( CSaveLoadItem *pItem )
{
	NMainLoop::Command( new NMainLoop::CICLoad( pItem->Get() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
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

			pSaveTabText = new CHoverButton( sEvent.pLoader->GetControl( "tab_save_text" ) );
			pSaveTabText->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 7560 ) + GetDBString( 7565 ) );
			pSaveTabText->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 7559 ) + GetDBString( 7565 ) );
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
	ZDATA_(CBaseView)
	CPtr<CEdit> pEdit;
	CObj<CHoverButton> pSave;
	CObj<CHoverButton> pLoadTabText;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CBaseView*)this); f.Add(2,&pEdit); f.Add(3,&pSave); f.Add(4,&pLoadTabText); return 0; }

public:
	CSaveView() {}
	CSaveView( const SWindowInfo &sInfo, CScreenShot *pScreenShot );

	void SetSelected( CSaveLoadItem *pItem );
	void SaveSlot();

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CSaveView::CSaveView( const SWindowInfo &sInfo, CScreenShot *pScreenShot ): 
	CBaseView( sInfo, false, pScreenShot )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveView::SetSelected( CSaveLoadItem *pItem )
{
	pEdit->SetText( NStr::ToUnicode( pItem->Get() ) );

	CBaseView::SetSelected( pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveView::SaveSlot()
{
	wstring wsTemp( pEdit->GetText() );
	if ( wsTemp.empty() )
		return;

	int nCount = NMainLoop::GetInterfaceStackDepth();
	for ( int nTemp = 1; nTemp < nCount; nTemp++ )
		NMainLoop::Command( new NMainLoop::CICExitModal() );

	NMainLoop::Command( new NMainLoop::CICSave( NStr::ToAscii( wsTemp ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
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

			pLoadTabText = new CHoverButton( sEvent.pLoader->GetControl( "tab_load_text" ) );
			pLoadTabText->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 7560 ) + GetDBString( 7566 ) );
			pLoadTabText->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 7559 ) + GetDBString( 7566 ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pEdit = GetUIWindow<CEdit>( this, "edit" );
			pEdit->SetMode( CEdit::FILENAME );
			pEdit->SetEditSize( 32 );
			pEdit->SetTextFormat( GetDBString( 10059 ) );
			break;
		}
	}

	return CBaseView::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSaveLoadUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSaveLoadUI: public CWindow
{
	OBJECT_BASIC_METHODS(CSaveLoadUI);
private:
	ZDATA_(CWindow)
	CPtr<CWindow> pView;
	CObj<CWindow> pModeView;
	CPtr<CScreenShot> pScreenShot;
	CObj<CFlashButton> pCloseButton;
	CArray2D<NGfx::SPixel8888> sScreenShot;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pView); f.Add(3,&pModeView); f.Add(4,&pScreenShot); f.Add(5,&pCloseButton); f.Add(6,&sScreenShot); return 0; }

public:
	CSaveLoadUI() {}
	CSaveLoadUI( const SWindowInfo &sInfo, const CArray2D<NGfx::SPixel8888> &sScreenShot );

	void SetMode( NGame::ESaveLoadType eType );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CSaveLoadUI::CSaveLoadUI( const SWindowInfo &sInfo, const CArray2D<NGfx::SPixel8888> &_sScreenShot ): 
	CWindow( sInfo ), sScreenShot( _sScreenShot )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSaveLoadUI::SetMode( NGame::ESaveLoadType eType )
{
	switch( eType )
	{
	case NGame::SAVE:
		pScreenShot->Set( sScreenShot );
		pScreenShot->SetStyle( STYLE_VISIBLE, true );
		pModeView = new CSaveView( SWindowInfo( pView, SPoint( 0, 0 ), pView->GetSize(), "", STYLE_VISIBLE | STYLE_ENABLED ), pScreenShot );
		LoadTemplate( pModeView, NDb::GetUIContainer( 340 ) );
		break;
	case NGame::LOAD:
		pScreenShot->SetStyle( STYLE_VISIBLE, false );
		pModeView = new CLoadView( SWindowInfo( pView, SPoint( 0, 0 ), pView->GetSize(), "", STYLE_VISIBLE | STYLE_ENABLED ), pScreenShot );
		LoadTemplate( pModeView, NDb::GetUIContainer( 341 ) );
		break;
	}
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

	void Initialize( ESaveLoadType eType, NGScene::CScreenshotTexture *pScreenShotTexture = 0 );

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
void CSaveLoadMenuInterface::Initialize( ESaveLoadType _eType, NGScene::CScreenshotTexture *pScreenShotTexture )
{
	eType = _eType;

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

	pMenuUI = new NUI::CSaveLoadUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "saveloadmenu", NUI::STYLE_ENABLED ), sScreenShot320x200 );
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
CICSaveLoadMenu::CICSaveLoadMenu( ESaveLoadType _eType, NGScene::CScreenshotTexture *_pScreenShotTexture ):
	eType( _eType ), pScreenShotTexture( _pScreenShotTexture )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICSaveLoadMenu::Exec()
{
	CSaveLoadMenuInterface *pRes = new CSaveLoadMenuInterface();
	pRes->Initialize( eType, pScreenShotTexture );
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
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB1014130, CSaveLoadMenuInterface );
