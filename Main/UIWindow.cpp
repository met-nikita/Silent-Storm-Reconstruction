#include "StdAfx.h"
#include "Sound.h"
#include "G2DView.h"
#include "RectLayout.h"
#include "G2DView.h"
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Interface.h"
#include "UIWindow.h"
#include "UICommCtrls.h"	// CToolTip -- CWindow::pToolTip is the typed retail CObj<CToolTip>
#include "A5Script.h"		// NScript::CScript -- retail CWindow window-scripting members (eventsMap/pScript)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_TOOLTIP_TIME = 500;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWindow
////////////////////////////////////////////////////////////////////////////////////////////////////
CWindow::CWindow( const SWindowInfo &sInfo ):
	pParent( sInfo.pParent ), nStyle( sInfo.nStyle ), szID( sInfo.szID ), sSize( sInfo.sSize ), sPosition( sInfo.sPosition ), bActive( false ),
	bRequireUpdate( true ), sToolTipAnchor( 0, 0 ), eToolTipAnchorType( NDb::UIA_NONE )	// retail ctor defaults (oracle @s2_imissionui.h:1126)
{
	if ( IsValid( pParent ) )
	{
		pParent->AddChild( this );
		pInterface = pParent->GetInterface();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NUI::CWindow::operator& @0xd4220 -- 2=nStyle, 3=bActive, 4=bRequireUpdate, 5=szID, 6=sSize,
// 7=sPosition, 8=sInfo, 9=pParent, 10=pMouseFocus, 11=pToolTip, 12=pInterface, 13=listChildren,
// 14=sToolTipAnchor, 15=eToolTipAnchorType, 16=eventsMap, 17=pScript. Out of line so CToolTip and
// NScript::CScript are complete types here.
int CWindow::operator&( CStructureSaver &f )
{
	f.Add( 2, &nStyle );
	f.Add( 3, &bActive );
	f.Add( 4, &bRequireUpdate );
	f.Add( 5, &szID );
	f.Add( 6, &sSize );
	f.Add( 7, &sPosition );
	f.Add( 8, &sInfo );
	f.Add( 9, &pParent );
	f.Add( 10, &pMouseFocus );
	f.Add( 11, &pToolTip );
	f.Add( 12, &pInterface );
	f.Add( 13, &listChildren );
	f.Add( 14, &sToolTipAnchor );
	f.Add( 15, &eToolTipAnchorType );
	f.Add( 16, &eventsMap );
	f.Add( 17, &pScript );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWindow::~CWindow()
{
	// retail @0x327920: RemoveChild(pParent, this) iff pParent is set and not ref-invalid (== IsValid).
	if ( IsValid( pParent ) )
		pParent->RemoveChild( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const string& CWindow::GetWindowID() const
{
	return szID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CInterface* CWindow::GetInterface() const
{
	return pInterface;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::SetInterface( CInterface *_pInterface )
{
	pInterface = _pInterface;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWindow* CWindow::GetParent() const
{
	return pParent;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::SetParent( CWindow *_pParent )
{
	pParent = _pParent;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::AddChild( CWindow *pWindow )
{
	listChildren.push_back( pWindow );
	if ( *listChildren.begin() == pWindow )
		SendMessage( *listChildren.begin(), SEvent( EVENT_ACTIVATE, EAF_ACTIVATE ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::RemoveChild( CWindow *pWindow )
{
	// retail NUI::CWindow::RemoveChild @0x327ac0: erase pWindow from the (vector) listChildren, then clear
	// pWindow->pParent (releasing its ref). The find==end() guard is defensive: on a save whose deserialize
	// left a child's pParent pointing at a window that does not actually list it, retail's raw vector erase
	// would pop an unrelated last element; skipping keeps a live parent's container intact. (The former
	// std::list here made the not-found erase(end()) free the list sentinel -> teardown heap-use-after-free;
	// ASan 2026-07-14. Matching retail's vector removes that failure mode.)
	vector< CMObj<CWindow> >::iterator iTemp = find( listChildren.begin(), listChildren.end(), pWindow );
	ASSERT( iTemp != listChildren.end() );
	if ( iTemp != listChildren.end() )
		listChildren.erase( iTemp );
	if ( pWindow )
		pWindow->pParent = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWindow* CWindow::GetChildByID( const string &szID )
{
	for( vector< CMObj<CWindow> >::iterator iTemp = listChildren.begin(); iTemp != listChildren.end(); iTemp++ )
	{
		if ( (*iTemp)->GetWindowID() == szID )
			return (*iTemp);
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::GetChildrenList( list<CPtr<CWindow> > *pList )
{
	ASSERT( pList );
	FormChildrenList( pList );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWindow::GetStyle( int _nStyle ) const
{
	return ( ( nStyle & _nStyle ) == _nStyle );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::SetStyle( int _nStyle, bool bOn )
{
	if ( bOn )
		nStyle |= _nStyle;
	else
		nStyle &= (~_nStyle);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SPoint& CWindow::GetSize() const
{
	return sSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::SetSize( const SPoint &_sSize )
{
	sSize = _sSize;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SPoint& CWindow::GetPosition() const
{
	return sPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::SetPosition( const SPoint &_sPosition )
{
	sPosition = _sPosition;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWindow::IsActive() const
{
	return bActive;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::ShowWindow( int nCmdShow )
{
	switch( nCmdShow )
	{
		case SWTYPE_HIDE:
			SetStyle( STYLE_VISIBLE, false );
			break;
		case SWTYPE_SHOW:
			SetStyle( STYLE_VISIBLE, true );
			SendMessage( GetParent(), SEvent( EVENT_ACTIVATEREQ, this ) );
			break;
		case SWTYPE_SHOWNA:
			SetStyle( STYLE_VISIBLE, true );
			break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWindow::HitTest( int nX, int nY ) const
{
	if ( !GetStyle( STYLE_VISIBLE ) || GetStyle( STYLE_TRANSPARENT ) )
		return false;

	SRect sWindow;
	SPoint sPosition;
	if ( !ClientToScreen( &sPosition, &sWindow ) )
		return false;

	if ( ( nX >= sWindow.x1 ) && ( nX < sWindow.x2 ) && ( nY >= sWindow.y1 ) && ( nY < sWindow.y2 ) )
		return true;

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWindow::ClientToScreen( SPoint *pPosition, SRect *pWindow, bool bSelf ) const
{
	if ( bSelf )
	{
		pPosition->x = 0;
		pPosition->y = 0;

		pWindow->x1 = 0;
		pWindow->y1 = 0;
		pWindow->x2 = sSize.x;
		pWindow->y2 = sSize.y;
	}

	if ( pWindow->x1 < 0 )
		pWindow->x1 = 0;
	if ( pWindow->x2 > sSize.x )
		pWindow->x2 = sSize.x;
	if ( pWindow->y1 < 0 )
		pWindow->y1 = 0;
	if ( pWindow->y2 > sSize.y )
		pWindow->y2 = sSize.y;

	pPosition->x += sPosition.x;
	pPosition->y += sPosition.y;

	pWindow->x1 += sPosition.x;
	pWindow->y1 += sPosition.y;
	pWindow->x2 += sPosition.x;
	pWindow->y2 += sPosition.y;

	if ( IsValid( pParent ) )
		pParent->ClientToScreen( pPosition, pWindow, false );

	if ( bSelf && ( ( pWindow->x1 >= pWindow->x2 ) || ( pWindow->y1 >= pWindow->y2 ) ) )
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::ScreenToClient( const SPoint &sScreenPos, SPoint *pPosition ) const
{
	SRect sWindow;
	SPoint sPosition;

	ClientToScreen( &sPosition, &sWindow );

	pPosition->x = sScreenPos.x - sPosition.x;
	pPosition->y = sScreenPos.y - sPosition.y;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::VirtualToScreen( SPoint *pPosition, SRect *pWindow )
{
	SRect sRect;
	const CVec2 &vScreenRect = GetInterface()->GetView()->GetViewportSize();

	if ( pPosition )
	{
		pPosition->x = pPosition->x * vScreenRect.x / 1024;
		pPosition->y = pPosition->y * vScreenRect.y / 768;
	}
	if ( pWindow )
	{
		pWindow->x1 = pWindow->x1 * vScreenRect.x / 1024;
		pWindow->x2 = pWindow->x2 * vScreenRect.x / 1024;
		pWindow->y1 = pWindow->y1 * vScreenRect.y / 768;
		pWindow->y2 = pWindow->y2 * vScreenRect.y / 768;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x3275c0
void CWindow::CreateClearRect( NGScene::I2DGameView *pView, float fZ )
{
	SRect sWindow;
	SPoint sPosition;
	if ( !ClientToScreen( &sPosition, &sWindow ) )
		return;

	SRect s2DWindow( sWindow );
	SPoint s2DPosition( sPosition );
	VirtualToScreen( &s2DPosition, &s2DWindow );

	SPoint sRealSize( GetSize() );
	VirtualToScreen( &sRealSize, 0 );

	CRectLayout sLayout;
	sLayout.AddRect( 0, 0, sRealSize.x, sRealSize.y, CTRect<float>( 0, 0, 0, 0 ) );
	pView->CreateDynamicClearRects( sLayout, s2DPosition, s2DWindow, fZ );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CToolTip* CWindow::GetToolTip() const
{
	return pToolTip;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::SetToolTip( CToolTip *pWindow )
{
	ASSERT( pWindow->GetParent() == GetInterface() );
	pToolTip = pWindow;

	if ( !IsValid( pToolTip ) )
		return;

	pToolTip->SetStyle( STYLE_VISIBLE, false );
	pToolTip->SetStyle( STYLE_TRANSPARENT | STYLE_TOPMOST, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SCursorInfo& CWindow::GetCursorInfo() const
{
	return sInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::SetCursorInfo( const SCursorInfo &_sInfo )
{
	sInfo = _sInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::PlaySound( NDb::CSound *pSound )
{
	if ( !IsValid ( pSound ) )
		return;

	// retail @0x327380: route through the OWNING interface's sound scene. Retail ALWAYS has one --
	// CInterface self-creates a scene when none is handed in (ctor @0x31dbd0, now mirrored in
	// UIInterface.cpp), which is why the retail recruit-menu click is audible.
	NSound::ISoundScene *pSoundScene = GetInterface() ? GetInterface()->GetSound() : 0;
	if ( !IsValid( pSoundScene ) )
	{
		return;
	}
	NSound::ISound2D *p2D = pSoundScene->Add2DSound( pSound );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWindow::SendMessage( CWindow *_pTarget, const SEvent &sEvent )
{
	CPtr<CWindow> pTarget( _pTarget );
	if ( !IsValid( pTarget ) )
		return false;

	return pTarget->ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWindow::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_ACTIVATE:
		{
			if ( GetStyle( STYLE_NOACTIVATE ) )
				return true;

			if ( sEvent.nVal == EAF_ACTIVATE )
				bActive = true;
			else if ( sEvent.nVal == EAF_DEACTIVATE )
				bActive = false;

			if ( !listChildren.empty() )
			{
				if ( ( sEvent.nVal & EAF_ACTIVATE ) && !( *listChildren.begin() )->IsActive() )
					SendMessage( *listChildren.begin(), SEvent( EVENT_ACTIVATE, EAF_ACTIVATENOTIFY ) );
				else if ( ( sEvent.nVal & EAF_DEACTIVATE ) && ( *listChildren.begin() )->IsActive() )
					SendMessage( *listChildren.begin(), SEvent( EVENT_ACTIVATE, EAF_DEACTIVATENOTIFY ) );
			}

			return true;
		}
	case EVENT_ACTIVATEREQ:
		{
			if ( GetStyle( STYLE_NOACTIVATE ) )
				return true;

			if ( !sEvent.pWindow->IsActive() )
			{
				if ( sEvent.pWindow.GetPtr() != *listChildren.begin() )
				{
					SendMessage( *listChildren.begin(), SEvent( EVENT_ACTIVATE, EAF_DEACTIVATE ) );
					BringWindowToTop( sEvent.pWindow );
				}
				SendMessage( *listChildren.begin(), SEvent( EVENT_ACTIVATE, EAF_ACTIVATE ) );
			}

			SendMessage( GetParent(), SEvent( EVENT_ACTIVATEREQ, this ) );

			return true;
		}
	case EVENT_MOUSEMOVE:
		{
			CPtr<CWindow> pHitWnd;
			for ( vector<CMObj<CWindow> >::iterator iTemp = listChildren.begin(); iTemp != listChildren.end(); iTemp++ )
			{
				if ( !(*iTemp)->HitTest( sEvent.nX, sEvent.nY ) )
					continue;

				pHitWnd = (*iTemp);
				break;
			}

			if ( pMouseFocus != pHitWnd )
			{
				SendMessage( pHitWnd, SEvent( EVENT_MOUSEENTER, sEvent.nX, sEvent.nY ) );
				SendMessage( pMouseFocus, SEvent( EVENT_MOUSEEXIT, sEvent.nX, sEvent.nY ) );
				pMouseFocus = pHitWnd;
			}

			if ( sInfo.pCursor )
				pInterface->SetCursorInfo( sInfo );

			break;
		}
	case EVENT_LBUTTONUP:
	case EVENT_RBUTTONUP:
	case EVENT_LBUTTONDOWN:
	case EVENT_RBUTTONDOWN:
	case EVENT_LBUTTONDBLCLK:
	case EVENT_RBUTTONDBLCLK:
		{
			ActivateTest( sEvent.nX, sEvent.nY );
			break;
		}
	case EVENT_MOUSEEXIT:
		{
			SendMessage( pMouseFocus, SEvent( EVENT_MOUSEEXIT, sEvent.nX, sEvent.nY ) );
			pMouseFocus = 0;
			break;
		}
	case EVENT_TEMPLATECREATE:
		{
			if ( IsValid( sEvent.pControl ) )
			{
				if ( IsValid( sEvent.pControl->pToolTip ) )
				{
					CToolTip *pToolTip = new CToolTip( SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 0, 0 ), "", STYLE_ENABLED | STYLE_TRANSPARENT | STYLE_TOPMOST ) );
					pToolTip->SetText( sEvent.pControl->pToolTip->szStr );
					SetToolTip( pToolTip );
				}

				// retail @0x328250 TEMPLATECREATE tail (decomp lines control+0x90/0x94/0x98): the window
				// ALWAYS copies the tooltip anchor from the originating control -- even without a tooltip.
				sToolTipAnchor = sEvent.pControl->sToolTipAnchor;
				eToolTipAnchorType = sEvent.pControl->toolTipAnchorType;
			}

			break;
		}
	}

	if ( sEvent.nEvent & EVENT_FLAG_NOTIFY )
		return false;
	if ( sEvent.nEvent & EVENT_FLAG_PARENTNOTIFY )
		return SendMessage( GetParent(), sEvent );

	list< CPtr<CWindow> > windowsList;
	FormChildrenList( &windowsList );
	for( list< CPtr<CWindow> >::iterator iTemp = windowsList.begin(); iTemp != windowsList.end(); iTemp++ )
	{
		if ( (*iTemp) == 0 )
		{
			ASSERT( 0 );
			continue;
		}

		if ( !(*iTemp)->GetStyle( STYLE_VISIBLE ) || (*iTemp)->GetStyle( STYLE_TRANSPARENT ) )
			continue;

		if ( ( sEvent.nEvent & EVENT_FLAG_ACTIVE ) && !(*iTemp)->IsActive() )
			continue;
		if ( ( sEvent.nEvent & EVENT_FLAG_HITTEST ) && !(*iTemp)->HitTest( sEvent.nX, sEvent.nY ) ) 
			continue;

		if ( (*iTemp)->ProcessMessage( sEvent ) )
			return true;

		if ( sEvent.nEvent & EVENT_FLAG_HITTEST ) // Only topmost window receive message w hittest
			break;
	}
		
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			// retail @0x328250 MOUSEMOVE tail: re-target the tooltip owner ONLY when this window
			// actually owns a live tooltip (decomp gates on this->pToolTip before SetToolTipOwner).
			if ( IsValid( pToolTip ) )
			{
				pInterface->SetToolTipOwner( this );
				return true;
			}
			break;
		}
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::Update( const STime &sTime, NGScene::I2DGameView *pView )
{
	// retail @0x3273d0 walks listChildren (a vector) BY INDEX, re-reading the bounds each step so a child's
	// Update mutating the vector cannot invalidate the walk. The guard below is defensive: a rare deserialize
	// can leave a wild/dangling child entry; retail has no guard, but keep it (verify the object AND its
	// vtable are mapped before calling a virtual through the pointer -- IsValid alone would deref a wild ptr).
	for ( int i = 0; i < (int)listChildren.size(); i++ )
	{
		CWindow *pChild = listChildren[i].GetPtr();
		unsigned u = (unsigned)pChild;
		if ( pChild == 0 || u < 0x00010000 || u >= 0x7f000000 || ( u & 3 ) )
			continue;
		if ( IsBadReadPtr( pChild, 4 ) || IsBadReadPtr( *(void**)pChild, 4 ) || !IsValid( listChildren[i] ) )
			continue;
		pChild->Update( sTime, pView );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	list< CPtr<CWindow> > windowsList;
	FormChildrenList( &windowsList );
	for ( list< CPtr<CWindow> >::reverse_iterator iTemp = windowsList.rbegin(); iTemp != windowsList.rend(); iTemp++ )
	{
		if ( !(*iTemp)->GetStyle( STYLE_VISIBLE ) )
			continue;

		(*iTemp)->Draw( sTime, pView );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CWindow internal
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::ActivateTest( int nX, int nY )
{
	if ( listChildren.empty() )
		return;
	if ( ( listChildren.size() > 0 ) && ( *listChildren.begin() )->GetStyle( STYLE_MODAL | STYLE_VISIBLE ) )
		return;

	list< CPtr<CWindow> > windowsList;
	FormChildrenList( &windowsList );
	for ( list< CPtr<CWindow> >::iterator iTemp = windowsList.begin(); iTemp != windowsList.end(); iTemp++ )
	{
		if ( (*iTemp)->GetStyle( STYLE_NOACTIVATE ) )
			continue;
		if ( !(*iTemp)->HitTest( nX, nY ) )
			continue;

		if ( !(*iTemp)->IsActive() )
		{
			if ( iTemp->GetPtr() != listChildren.begin()->GetPtr() )
			{
				SendMessage( *listChildren.begin(), SEvent( EVENT_ACTIVATE, EAF_DEACTIVATE ) );
				BringWindowToTop( (*iTemp) );
			}
			SendMessage( *listChildren.begin(), SEvent( EVENT_ACTIVATE, EAF_ACTIVATE ) );
		}
		break;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::BringWindowToTop( CWindow *pWindow )
{
	ASSERT( find( listChildren.begin(), listChildren.end(), pWindow ) != listChildren.end() );

	// retail @0x327c20: hold a temp M-ref on pWindow, erase it from the (vector) listChildren, reinsert at
	// front. Vector form of the former list remove()/push_front().
	CMObj<CWindow> pKeepPointer( pWindow );
	vector< CMObj<CWindow> >::iterator iTemp = find( listChildren.begin(), listChildren.end(), pWindow );
	if ( iTemp != listChildren.end() )
		listChildren.erase( iTemp );
	listChildren.insert( listChildren.begin(), pWindow );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWindow::FormChildrenList( list< CPtr<CWindow> > *pList )
{
	pList->clear();
	for ( vector< CMObj<CWindow> >::iterator iTemp = listChildren.begin(); iTemp != listChildren.end(); iTemp++ )
	{
		if ( !(*iTemp)->GetStyle( STYLE_TOPMOST ) )
			continue;
		pList->push_back( (*iTemp).GetPtr() );
	}
	for ( vector< CMObj<CWindow> >::iterator iTemp = listChildren.begin(); iTemp != listChildren.end(); iTemp++ )
	{
		if ( (*iTemp)->GetStyle( STYLE_TOPMOST ) || (*iTemp)->GetStyle( STYLE_BOTTOMMOST ) )
			continue;
		pList->push_back( (*iTemp).GetPtr() );
	}
	for ( vector< CMObj<CWindow> >::iterator iTemp = listChildren.begin(); iTemp != listChildren.end(); iTemp++ )
	{
		if ( !(*iTemp)->GetStyle( STYLE_BOTTOMMOST ) || (*iTemp)->GetStyle( STYLE_TOPMOST ) )
			continue;
		pList->push_back( (*iTemp).GetPtr() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0241990, CWindow )
