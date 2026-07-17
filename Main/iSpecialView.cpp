#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "GSceneUtils.h"
#include "RPGGlobal.h"
#include "GlobalInfo.h"
#include "ChapterInfo.h"
#include "Interface.h"
#include "iMission.h"
#include "iCommonUI.h"
#include "iDesktopWindow.h"
#include "iSpecialView.h"
#include "..\Misc\BasicShare.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataScenario.h"
#include "..\DBFormat\DataInterface.h"
#include "scScenarioTracker.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
extern CBasicShare<int, CChapterInfoLoader> shareChapterInfo;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSlideButton
////////////////////////////////////////////////////////////////////////////////////////////////////
CSlideButton::CSlideButton( const SWindowInfo &sInfo ):
	CHoverButton( sInfo ), bLastState( false ), fValue( 0 ), sLastTime( 0 )
{
	// ORIGINAL BUG (confirmed @0x23fbb0): bActive is intentionally NOT initialised --
	// the inlined release ctor only zeroes bLastState/fValue/sLastTime. Left
	// indeterminate to match the binary; do not "fix".
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSlideButton::Draw @0x23f020 -- edge-triggered off CButton::IsPushed vs the cached bLastState.
// ORIGINAL BUG (confirmed): the active/ramp path is effectively dead in normal frame sequencing --
// sLastTime is forced to 0 every up-frame, so the > 500ms warm-up never fires. Reproduced verbatim.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlideButton::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( IsPushed() != bLastState )
	{
		if ( IsPushed() )
		{
			if ( sLastTime == 0 )
				sLastTime = sTime;

			if ( !bActive )
			{
				if ( (unsigned int)( sTime - sLastTime ) > 500 )
					bActive = true;
			}
			else
			{
				fValue += (float)(unsigned int)( sTime - sLastTime ) * 0.1f;
				sLastTime = sTime;
			}
		}
		else
		{
			if ( !bActive )
				fValue += 10.0f;
		}
	}

	bLastState = IsPushed();
	if ( !IsPushed() )
		sLastTime = 0;

	CHoverButton::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEarthView
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEarthView::CEarthView( const SWindowInfo & ) @0x23f280 -- the live ctor. Zeroes the spin state,
// clears the parented scene, then for the fixed globe RPG item ( NDb::GetRPGItem(0x1E7) ) builds the
// camera from sCameras[2] (the canonical "camera from SCameraParams" idiom, cf. iCommonUI.cpp:858)
// and installs the model + camera transform.
////////////////////////////////////////////////////////////////////////////////////////////////////
CEarthView::CEarthView( const SWindowInfo &sInfo ):
	CModel( sInfo ), bButtonDown( false ), fAngleX( 0 ), fLastAngleX( 0 ), fAngleY( 0 ), fLastAngleY( 0 ), sLastPoint( 0, 0 )
{
	SetScene( 0, false );

	CPtr<NDb::CRPGItem> pRPGItem( NDb::GetRPGItem( 0x1E7 ) );
	if ( IsValid( pRPGItem ) )
	{
		const NDb::SCameraParams &sCamera = pRPGItem->sCameras[2];

		CVec3 vForwardDir;
		CQuat q = CQuat( sCamera.fYaw, V3_AXIS_Z ) * CQuat( sCamera.fPitch, V3_AXIS_X );
		q.GetYAxis( &vForwardDir );

		CVec3 vCP( sCamera.vAnchor - vForwardDir * sCamera.fDistance );
		SHMatrix sCameraTransform;
		MakeMatrix( &sCameraTransform, sCamera.fPitch, sCamera.fYaw, sCamera.fRoll, vCP );

		SRand sRnd;
		SetModel( pRPGItem->pModel->CreateModel( &sRnd ) );
		SetCameraTransform( sCameraTransform );
		UpdateMatrix();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEarthView::UpdateMatrix @0x23f100 -- rebuild the model transform from the current spin angles.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEarthView::UpdateMatrix()
{
	SHMatrix sMatrix;
	MakeMatrix( &sMatrix, fAngleX, fAngleY, 0, CVec3( 0, 0, 0 ) );
	SetModelTransform( sMatrix );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEarthView::Draw @0x23f210 -- rebake the matrix only when a spin angle changed, then base draw.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEarthView::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( ( fLastAngleX != fAngleX ) || ( fLastAngleY != fAngleY ) )
	{
		fLastAngleY = fAngleY;
		fLastAngleX = fAngleX;
		UpdateMatrix();
	}

	CModel::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEarthView::ProcessMessage @0x23f4b0 -- drag to free-spin; the base call goes straight to CWindow
// (decomp confirms CWindow::ProcessMessage, bypassing CModel's EVENT_TEMPLATECREATE handling).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CEarthView::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_LBUTTONDOWN:
		{
			sLastPoint = SPoint( sEvent.nX, sEvent.nY );
			bButtonDown = true;
			pMouseCapture = GetInterface()->CreateMouseCapture( this );
			return true;
		}
	case EVENT_LBUTTONUP:
		{
			bButtonDown = false;
			pMouseCapture = 0;
			return true;
		}
	case EVENT_MOUSECAPTURELOSE:
		{
			bButtonDown = false;
			if ( pMouseCapture )
			{
				pMouseCapture = 0;
				return CWindow::ProcessMessage( sEvent );
			}
			break;
		}
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( SCursorInfo() );
			if ( bButtonDown )
			{
				fAngleX += ( sEvent.nX - sLastPoint.x ) * 0.1f;
				fAngleY += ( sEvent.nY - sLastPoint.y ) * 0.1f;
				sLastPoint = SPoint( sEvent.nX, sEvent.nY );
				return CWindow::ProcessMessage( sEvent );
			}
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEarthControl
////////////////////////////////////////////////////////////////////////////////////////////////////
CEarthControl::CEarthControl( const SWindowInfo &sInfo ):
	CWindow( sInfo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEarthControl::Draw @0x23f160 -- reverse z-order, valid + visible-filtered child dispatch only
// (no base CWindow::Draw, no own chrome). Mirrors the CWindow::Draw idiom plus the valid-bit filter.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEarthControl::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	list< CPtr<CWindow> > windowsList;
	FormChildrenList( &windowsList );
	for ( list< CPtr<CWindow> >::reverse_iterator iTemp = windowsList.rbegin(); iTemp != windowsList.rend(); iTemp++ )
	{
		CWindow *pChild = *iTemp;
		if ( !IsValid( pChild ) )
			continue;
		if ( !pChild->GetStyle( STYLE_VISIBLE ) )
			continue;

		pChild->Draw( sTime, pView );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEarthControl::ProcessMessage @0x23fbe0 -- on template-load build the six rotate/zoom slide
// buttons from the loader's named controls (name->member order fixed by the decode).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CEarthControl::ProcessMessage( const SEvent &sEvent )
{
	if ( sEvent.nEvent == EVENT_TEMPLATELOAD )
	{
		pRXPlus    = new CSlideButton( sEvent.pLoader->GetControl( "right" ) );
		pRYPlus    = new CSlideButton( sEvent.pLoader->GetControl( "down" ) );
		pRXMinus   = new CSlideButton( sEvent.pLoader->GetControl( "left" ) );
		pRYMinus   = new CSlideButton( sEvent.pLoader->GetControl( "up" ) );
		pZoomPlus  = new CSlideButton( sEvent.pLoader->GetControl( "plus" ) );
		pZoomMinus = new CSlideButton( sEvent.pLoader->GetControl( "minus" ) );
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CXComMapUI
////////////////////////////////////////////////////////////////////////////////////////////////////
CXComMapUI::CXComMapUI( const SWindowInfo &sInfo, NGame::IMission *_pGlobal ):
	CDesktopWindow( sInfo ), pGlobal( _pGlobal )
{
	// retail CXComMapUI ctor @0x23f720: disasm @0x63f7e0 `mov ecx,1` -> NDb::GetUICursor(1) =
	// UICursors row 1 "xz" (UITexture 295, NormalPen.cur) -- same map-screen default cursor as
	// the global/chapter maps. (492 was the pre-remap arbitrary id = HitLocationLeftArm.cur.)
	sCursor = SCursorInfo( NDb::GetUICursor( 1 ) );

	// NOTE (dev-API divergence): retail @0x23f720 finishes by resolving
	//   pInfo = pGlobal->GetGlobalInfo()   ( an IMission vtbl slot the SHIPPED IMission added ).
	// This predecessor's NGame::IMission has no GetGlobalInfo accessor, and pInfo is vestigial in the
	// reworked CXComMapUI ( the per-sector window build of CGlobalMapUI was removed and nothing here
	// reads pInfo ), so it is left null. Re-adding GetGlobalInfo to IMission would be a load-bearing
	// interface change across every mission impl -- out of scope for this additive parity leg.
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CXComMapUI::Update @0x23f170 -- a stub that just reports "handled" ( the per-frame sector hit-test
// of the CGlobalMapUI predecessor was removed ). CWindow::Update is void here, so the shipped
// `return true` is observably a no-op.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CXComMapUI::Update( const STime &sTime, NGScene::I2DGameView *pView )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CXComMapUI::ProcessMessage @0x240040 -- build the owned children / push the cursor, then defer to
// the base; any mouse-button event ( EVENT_LBUTTONUP .. EVENT_RBUTTONDBLCLK ) is force-consumed.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CXComMapUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pEarthControl = new CEarthControl( sEvent.pLoader->GetControl( "earthcontrol" ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pMapView = GetUIWindow<CWindow>( this, "view" );
			pEarth = new CEarthView( SWindowInfo( pMapView, SPoint( 0, 0 ), pMapView->GetSize(), "earth", STYLE_VISIBLE | STYLE_ENABLED | STYLE_TOPMOST ) );
			break;
		}
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( sCursor );
			break;
		}
	}

	if ( CDesktopWindow::ProcessMessage( sEvent ) )
		return true;

	if ( ( sEvent.nEvent >= EVENT_LBUTTONUP ) && ( sEvent.nEvent <= EVENT_RBUTTONDBLCLK ) )
		return true;

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CXComMapUI::GetGlobalSectorInfo @0x23f940 -- VISIBLE if any ZONE-type chapter sector maps to a
// currently-available scenario zone, RECOMMENDED if that zone is the tracker's recommended one for
// the first player. Same algorithm as CGlobalMapUI::GetGlobalSectorInfo (iGlobalMapUI.cpp:295) with
// the mission's CGlobalGame fetched via GetRPGGame (this tree's IMission accessor).
// ORIGINAL BUG (confirmed): the chapter-info handle is dereferenced with NO null check after
// shareChapterInfo.Get -- a missing chapter faults the original. Reproduced verbatim.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CXComMapUI::GetGlobalSectorInfo( const SGlobalSector &sSector, bool *pbVisible, bool *pRecommended )
{
	*pbVisible = true;
	*pRecommended = false;

	if ( sSector.nTemplate == -1 )
		return;

	CPtr<NRPG::CGlobalGame> pGame = pGlobal->GetRPGGame();

	*pbVisible = false;
	list<CPtr<NScenario::CScenarioZone> > zonesList;
	pGame->pScenarioTracker->GetAvailableZones( &zonesList );

	CDGPtr<CPtrFuncBase<CChapterInfo> > pChapterInfo = shareChapterInfo.Get( sSector.nTemplate );
	pChapterInfo.Refresh();
	CObj<CChapterInfo> pInfo = pChapterInfo->GetValue();

	for( int nTemp = 0; nTemp < pInfo->sectorsSet.size(); nTemp++ )
	{
		const SChapterSector &sChapterSector = pInfo->sectorsSet[nTemp];
		if ( sChapterSector.eType != ZONE )
			continue;

		CPtr<NScenario::CScenarioZone> pZone = pGame->pScenarioTracker->GetZoneByDBZone( NDb::GetDBScenarioZone( sChapterSector.nTemplate ) );
		if ( find( zonesList.begin(), zonesList.end(), pZone ) != zonesList.end() )
		{
			*pbVisible = true;
			if ( pGame->pScenarioTracker->GetRecommendedZone( pGame->players.front() ) == pZone )
				*pRecommended = true;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NUI
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB3717180, CXComMapUI );
REGISTER_SAVELOAD_CLASS( 0xB3717181, CEarthView );
REGISTER_SAVELOAD_CLASS( 0xB3717182, CEarthControl );
