#include "StdAfx.h"
/*
#include "GSceneUtils.h"
#include "RectLayout.h"
#include "GView.h"
#include "G2DView.h"
#include "Interface.h"
#include "iPopupMenu.h"
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataFormat.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const NGfx::SPixel8888
	sFrame				= NGfx::SPixel8888( 157, 218, 212, 0xB2 ),
	sBackground		= NGfx::SPixel8888( 41, 51, 51, 0x7F );
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDynamicBackground
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDynamicBackground: public CDecorator<CWindow>
{
	OBJECT_BASIC_METHODS(CDynamicBackground)
private:
	ZDATA_(TBaseClass)
	int nWidth;
	NGfx::SPixel8888 sColor;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); f.Add(2,&nWidth); f.Add(3,&sColor); return 0; }

public:
	CDynamicBackground() {}
	CDynamicBackground( IWindow *pWindow, int nWidth, const NGfx::SPixel8888 &sColor );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CDynamicBackground::CDynamicBackground( IWindow *pWindow, int _nWidth, const NGfx::SPixel8888 &_sColor ):
	TBaseClass( pWindow ), nWidth( _nWidth ), sColor( _sColor )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDynamicBackground::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
		case EVENT_TEMPLATELOAD:
		{
			list<CPtr<IWindow> > childrenList;
			GetChildrenList( &childrenList );

			int nMidSize = 0;
			for ( list<CPtr<IWindow> >::iterator iTemp = childrenList.begin(); iTemp != childrenList.end(); iTemp++ )
			{
				CPtr<IImage> pImage = dynamic_cast<IImage*>( iTemp->GetPtr() );
				if ( IsValid( pImage ) )
				{
					const string &szID = pImage->GetWindowID();
					if ( szID.compare( "mid" ) == 0 )
						nMidSize = Max( nMidSize, pImage->GetSize().x );
				}
			}

			SetSize( SPoint( GetSize().x + nWidth - nMidSize, GetSize().y ) );

			int nDelta = nWidth - nMidSize;
			for ( list<CPtr<IWindow> >::iterator iTemp = childrenList.begin(); iTemp != childrenList.end(); iTemp++ )
			{
				CPtr<IImage> pImage = dynamic_cast<IImage*>( iTemp->GetPtr() );
				if ( IsValid( pImage ) )
				{
					const string &szID = pImage->GetWindowID();
					if ( szID.compare( "mid" ) == 0 )
						pImage->SetSize( SPoint( pImage->GetSize().x + nDelta, pImage->GetSize().y ) );
					else if ( szID.compare( "right" ) == 0 )
						pImage->SetPosition( SPoint( pImage->GetPosition().x + nDelta, pImage->GetPosition().y ) );

					pImage->SetColor( sColor );
				}
			}
			break;
		}
	}

	return TBaseClass::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPopupBackground
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPopupBackground: public CDecorator<CWindow>
{
	OBJECT_BASIC_METHODS(CPopupBackground)
private:
	ZDATA_(TBaseClass)
	int nWidth;
	SPoint sSize;
	CPtr<CDynamicBackground> pFrame;
	CPtr<CDynamicBackground> pBackground;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); f.Add(2,&nWidth); f.Add(3,&sSize); f.Add(4,&pFrame); f.Add(5,&pBackground); return 0; }

public:
	CPopupBackground() {}
	CPopupBackground( IWindow *pWindow, int nWidth );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CPopupBackground::CPopupBackground( IWindow *pWindow, int _nWidth ):
	TBaseClass( pWindow ), nWidth( _nWidth )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPopupBackground::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
		case EVENT_TEMPLATELOAD:
		{
			pFrame = new CDynamicBackground( GetUIWindow<IWindow>( this, "frame", sEvent.pLoader ), nWidth - 20, sFrame );
			pBackground = new CDynamicBackground( GetUIWindow<IWindow>( this, "background", sEvent.pLoader ), nWidth - 20, sBackground );

			break;
		}
		case EVENT_TEMPLATELOADCOMPLETE:
		{
			SetSize( SPoint( Max( pFrame->GetSize().x, pBackground->GetSize().x ), GetSize().y ) );
			break;
		}
	}

	return TBaseClass::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPopupItem
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPopupItem: public CPopupBackground
{
	OBJECT_BASIC_METHODS(CPopupItem)
private:
	ZDATA_(CPopupBackground)
	int nWidth;
	wstring wsText;
	CPtr<IText> pText;
	CPtr<IImage> pIcon;
	CObj<CObjectBase> pMouseCapture;
	CDBPtr<NDb::CUITexture> pIconTexture;
	CObj<CDynamicBackground> pSelection;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CPopupBackground*)this); f.Add(2,&nWidth); f.Add(3,&wsText); f.Add(4,&pText); f.Add(5,&pIcon); f.Add(6,&pMouseCapture); f.Add(7,&pIconTexture); f.Add(8,&pSelection); return 0; }

public:
	CPopupItem() {}
	CPopupItem( IWindow *pWindow, const wstring &wsText, NDb::CUITexture *pIconTexture, int nWidth );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CPopupItem::CPopupItem( IWindow *pWindow, const wstring &_wsText, NDb::CUITexture *_pIconTexture, int _nWidth ):
	CPopupBackground( pWindow, _nWidth ), wsText( _wsText ), pIconTexture( _pIconTexture ), nWidth( _nWidth )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPopupItem::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
		case EVENT_LBUTTONDOWN:
		{
			return true;
		}
		case EVENT_LBUTTONUP:
		{
			SendMessage( GetParent(), SEvent( EVENT_NOTIFY, GetWindowID() ) );
			return true;
		}
		case EVENT_MOUSEMOVE:
		{
			if ( HitTest( sEvent.nX, sEvent.nY) )
			{
				if ( !IsValid( pMouseCapture ) )
					pMouseCapture = GetInterface()->CreateMouseCapture( this );

				pSelection->SetStyle( STYLE_VISIBLE, true );
			}
			else
			{
				pMouseCapture = 0;
				pSelection->SetStyle( STYLE_VISIBLE, false );
			}

			break;
		}
		case EVENT_TEMPLATELOAD:
		{
			pText = GetUIWindow<IText>( this, "text", sEvent.pLoader );
			pText->SetText( wsText );
			pText->SetSize( SPoint( nWidth, pText->GetSize().y ) );

			pIcon = GetUIWindow<IImage>( this, "icon", sEvent.pLoader );
			if ( pIconTexture )
				pIcon->SetImage( pIconTexture );
			else
				pIcon->SetStyle( STYLE_VISIBLE, false );

			pSelection = new CDynamicBackground( GetUIWindow<IWindow>( this, "selection", sEvent.pLoader ), nWidth, NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF ) );
			pSelection->SetStyle( STYLE_VISIBLE, false );
			break;
		}
	}

	return CPopupBackground::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPopupMenu
////////////////////////////////////////////////////////////////////////////////////////////////////
CPopupMenu::CPopupMenu( IWindow *pContainer ): 
	TBaseClass( pContainer ), bUpdated( false ), bComplete( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPopupMenu::AddItem( const string &szID, const wstring &wsText, NDb::CUITexture *pIcon )
{
	SItem &sItem = *itemsSet.insert( itemsSet.end() );

	bUpdated = true;
	sItem.szID = szID;
	sItem.pIcon = pIcon;
	sItem.wsText = L"<font size=18pt face=Courier><left>" + wsText;
	sItem.bSeparator = false;

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPopupMenu::AddSeparator()
{
	SItem &sItem = *itemsSet.insert( itemsSet.end() );

	bUpdated = true;
	sItem.bSeparator = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPopupMenu::RemoveAll()
{
	bUpdated = true;
	itemsSet.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPopupMenu::Do()
{
	bComplete = false;
	szReturnID = "!!!not yet!!!";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPopupMenu::IsComplete()
{
	return bComplete;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const string& CPopupMenu::GetReturnID()
{
	return szReturnID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CPopupMenu::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
		case EVENT_ACTIVATE:
		{
			if ( sEvent.nVal & EAF_DEACTIVATE )
			{
				bComplete = true;
				szReturnID = sEvent.szID;
			}
			break;
		}
		case EVENT_NOTIFY:
		{
			bComplete = true;
			szReturnID = sEvent.szID;
			return true;
		}
	}

	return TBaseClass::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPopupMenu::Update( const SScene &sScene )
{
	if ( bUpdated )
	{
		int nWidth = 0;
		for ( int nTemp = 0; nTemp < itemsSet.size(); nTemp++ )
		{
			SItem &sItem = itemsSet[nTemp];
			if ( !sItem.bSeparator )
			{
				CPtr<IDynamicTextDraw> pText = IDynamicTextDraw::Create( SPoint( 0, 0 ), sItem.wsText );
				const SPoint &sSize = pText->CalcSize( sScene );
				nWidth = Max( nWidth, sSize.x );
			}
		}

		NGfx::SPixel8888 sFrame( 157, 218, 212, 0xB2 ), sBackground( 41, 51, 51, 0x7F );

		int nX = 0, nY = 0;
		pUp = new CPopupBackground( IWindow::Create( this, SRect( 0, 0, 0, 0 ), "", STYLE_ENABLED | STYLE_VISIBLE ), nWidth );
		LoadTemplate( pUp, NDb::GetUIContainer( 58 ) );
		nY += pUp->GetSize().y;
		nX = Max( nX, pUp->GetSize().x );

		for ( int nTemp = 0; nTemp < itemsSet.size(); nTemp++ )
		{
			SItem &sItem = itemsSet[nTemp];
			CPtr<IWindow> pLine = IWindow::Create( this, SRect( 0, nY, 0, nY ), sItem.szID, STYLE_ENABLED | STYLE_VISIBLE );
			if ( !sItem.bSeparator )
			{
				sItem.pLine = new CPopupItem( pLine, sItem.wsText, sItem.pIcon, nWidth );
				LoadTemplate( sItem.pLine, NDb::GetUIContainer( 61 ) );
			}
			else
			{
				sItem.pLine = new CPopupBackground( pLine, nWidth );
				LoadTemplate( sItem.pLine, NDb::GetUIContainer( 60 ) );
			}

			nY += sItem.pLine->GetSize().y;
			nX = Max( nX, sItem.pLine->GetSize().x );
		}

		pDown = new CPopupBackground( IWindow::Create( this, SRect( 0, nY, 0, nY ), "", STYLE_ENABLED | STYLE_VISIBLE ), nWidth );
		LoadTemplate( pDown, NDb::GetUIContainer( 59 ) );
		nY += pDown->GetSize().y;
		nX = Max( nX, pDown->GetSize().x );

		bUpdated = false;
		SetSize( SPoint( nX, nY ) );

		SPoint sSize = GetSize();
		SPoint sPosition = GetPosition();
		if ( sPosition.x + sSize.x > 1024 )
			sPosition.x -= sSize.x;
		if ( sPosition.y + sSize.y > 768 )
			sPosition.y -= sSize.y;

		SetPosition( sPosition );
	}

	TBaseClass::Update( sScene );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0118100, CPopupMenu );
REGISTER_SAVELOAD_CLASS( 0xB0118101, CPopupItem );
REGISTER_SAVELOAD_CLASS( 0xB0118102, CPopupBackground );
REGISTER_SAVELOAD_CLASS( 0xB0118103, CDynamicBackground );
*/