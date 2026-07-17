#include "StdAfx.h"
#include "GSceneUtils.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"				// MakeMatrix (camera + spin transforms)
#include "wInterface.h"				// NGame::IUnitTracker, NWorld::CUnit::GetRPG
#include "RPGUnit.h"				// NRPG::CUnit, NRPG::CMedalsGainer::GetGainedMedals / HasNewMedalToShow
#include "RPGUnitInfo.h"			// NRPG::IUnitMissionInfo::GetRPGUnit
#include "..\DBFormat\DataRPG.h"		// NDb::CRPGItem, SCameraParams (medal model + preview camera)
#include "..\DBFormat\DataMisc.h"	// NDb::CMedal (pModel @+0x2c, pName @+0x28)
#include "..\DBFormat\DataFormat.h"	// NDb::CTRndModel::CreateModel, NDb::CModel
#include "..\DBFormat\DataInterface.h"	// NDb::GetUITexture
#include "Interface.h"				// NUI::CInterface (GetInterface / SetCursorInfo / CreateMouseCapture)
#include "UIBaseCtrls.h"			// CModel, CText, CImage
#include "UICommCtrls.h"			// CListView, CButton
#include "iCommonUI.h"				// CHoverButton, CHoverFlashButton, CScrollWindow
#include "iMission.h"				// NGame::IMission::GetSelectedUnits
#include "iMedalsPanel.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release iMedalsPanel.obj convergence -- the "medals" tab of the merc character UI. Reconstructed 1:1
// over the working dev siblings: CMedalsPanelItem mirrors CSaveLoadItem (iSaveLoad.cpp -- a CHoverButton
// list row built from an "iml-text" CText + a "hilight" CImage per state); CMedalsPanelView mirrors
// CEarthView (iSpecialView.cpp -- a CModel 3D-preview with a drag-to-spin state block and the canonical
// "camera from SCameraParams" idiom); CMedalsPanel mirrors CBiographyPanel (the tab-strip panel). All
// layouts + save tags are verbatim from the matched Game.exe + PDB. Retail RVAs noted per function
// (VA = RVA + 0x400000).
//
// DEFERRED SEAM (documented at each call site): the awarded-medal source and the per-unit award flag
// both live on NRPG::CMedalsGainer behind the absent NDb::CSide::medals column. The two gainer methods
// the panel needs (GetGainedMedals / HasNewMedalToShow) are landed in RPGUnit.h as behaviour-neutral
// STUBS (empty list / false), so the panel builds GREEN and runs as the binary's "no awards" path.
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanelItem -- one CHoverButton row of the medals CListView, representing a single awarded medal.
// Structurally identical to CSaveLoadItem (iSaveLoad.cpp): three text states (normal / hover / selected)
// each a "iml-text" CText caption + a "hilight" CImage overlay, plus the pin-on-selection behaviour.
// Layout (PDB NUI::CMedalsPanelItem, size 220, base CHoverButton): pMedal / bSelected / pHilight / pName.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMedalsPanelItem: public CHoverButton
{
	OBJECT_NOCOPY_METHODS(CMedalsPanelItem)
private:
	enum { STATE_SELECTED = STATE_NORMAL + 0xFF };

	ZDATA_(CHoverButton)
	CDBPtr<NDb::CMedal> pMedal;
	////
	bool bSelected;
	CPtr<CImage> pHilight;   // vestigial (per the binary the state hilight images are owned by the state
	CPtr<CWindow> pName;     // windows' child lists; these slots are init-null and never repopulated)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CHoverButton*)this); f.Add(2,&pMedal); f.Add(3,&bSelected); f.Add(4,&pHilight); f.Add(5,&pName); return 0; }

protected:
	void AddTextState( int nID, const wstring &wsText, const NGfx::SPixel8888 &sColor );
	void OnAction();

public:
	CMedalsPanelItem() {}
	CMedalsPanelItem( const SWindowInfo &sInfo, NDb::CMedal *pMedal );

	NDb::CMedal* GetMedal() const { return pMedal; }

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanelItem::CMedalsPanelItem @0x1f9e00 -- caption = medal name + a per-state DB suffix (0x1d85
// normal / 0x1d86 hover & selected), tinted RGB 0x877d4d with rising alpha (0x00 / 0x66 / 0xff). The
// medal name is GetDBString( pMedal->pName ) (CMedal +0x28). Identical shape to CSaveLoadItem's ctor.
////////////////////////////////////////////////////////////////////////////////////////////////////
CMedalsPanelItem::CMedalsPanelItem( const SWindowInfo &sInfo, NDb::CMedal *_pMedal ):
	CHoverButton( sInfo ), pMedal( _pMedal ), bSelected( false )
{
	AddTextState( STATE_NORMAL,   GetDBString( pMedal->pName ) + GetDBString( 0x1D85 ), NGfx::SPixel8888( 0x87, 0x7D, 0x4D, 0x00 ) );
	AddTextState( STATE_HOVER,    GetDBString( pMedal->pName ) + GetDBString( 0x1D86 ), NGfx::SPixel8888( 0x87, 0x7D, 0x4D, 0x66 ) );
	AddTextState( STATE_SELECTED, GetDBString( pMedal->pName ) + GetDBString( 0x1D86 ), NGfx::SPixel8888( 0x87, 0x7D, 0x4D, 0xFF ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanelItem::AddTextState @0x1f9780 -- add the empty state window, lay an "iml-text" CText over
// it, size to {stateWidth, textHeight}, add a same-size "hilight" CImage tinted by sColor, then grow
// self to enclose it. Byte-identical to CSaveLoadItem::AddTextState (iSaveLoad.cpp).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMedalsPanelItem::AddTextState( int nID, const wstring &wsText, const NGfx::SPixel8888 &sColor )
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
// CMedalsPanelItem::ProcessMessage @0x1f9450 -- latch bSelected off the listview selection event, then
// route to the CButton base.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMedalsPanelItem::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_LISTVIEW_ITEMSELECTED:
		{
			bSelected = ( sEvent.nVal == ELV_ITEMSSELECTED_TRUE );
			break;
		}
	}

	return CButton::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanelItem::Draw @0x1f9480 -- pin the hover state to "selected" while this row is selected, then
// paint via the CHoverButton base.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMedalsPanelItem::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	ForceState( bSelected, STATE_SELECTED );
	CHoverButton::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanelItem::OnAction @0x1f9a70 -- notify the parent listview that this row was activated.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMedalsPanelItem::OnAction()
{
	SendMessage( GetParent(), SEvent( EVENT_LISTVIEW_ITEMSELECTED, this ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanelView -- the rotating 3D preview of a single medal's award model. A CModel widget with a
// drag-to-spin state block. Mirrors CEarthView (iSpecialView.cpp) but with a single spin angle. Layout
// (PDB NUI::CMedalsPanelView, size 164, base CModel): bButtonDown / fAngle / fLastAngle / sLastPoint /
// sTimer / pMouseCapture.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMedalsPanelView: public CModel
{
	OBJECT_NOCOPY_METHODS(CMedalsPanelView)
private:
	ZDATA_(CModel)
	bool bButtonDown;
	float fAngle;
	float fLastAngle;
	SPoint sLastPoint;
	CTimeCounter sTimer;
	CObj<CObjectBase> pMouseCapture;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CModel*)this); f.Add(2,&bButtonDown); f.Add(3,&fAngle); f.Add(4,&fLastAngle); f.Add(5,&sLastPoint); f.Add(6,&sTimer); f.Add(7,&pMouseCapture); return 0; }

protected:
	void UpdateMatrix();

public:
	CMedalsPanelView() {}
	CMedalsPanelView( const SWindowInfo &sInfo );

	void Set( NDb::CMedal *pMedal );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanelView::CMedalsPanelView @0x1f9b20 -- zero the spin state. (Unlike CEarthView the binary
// ctor does NOT call SetScene here; matched faithfully.)
////////////////////////////////////////////////////////////////////////////////////////////////////
CMedalsPanelView::CMedalsPanelView( const SWindowInfo &sInfo ):
	CModel( sInfo ), bButtonDown( false ), fAngle( 0 ), fLastAngle( 0 ), sLastPoint( 0, 0 )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanelView::UpdateMatrix @0x1f94b0 -- rebuild the model spin transform from fAngle. The binary
// passes size {1,1,1} / move {0,0,0} to the TRS MakeMatrix overload (the 3x3 collapses; faithful).
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMedalsPanelView::UpdateMatrix()
{
	SFBTransform sTransform;
	MakeMatrix( &sTransform, CVec3( 1, 1, 1 ), CVec3( 0, 0, 0 ), fAngle );
	SetModelTransform( sTransform.forward );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanelView::Set @0x1f95f0 -- aim the preview camera at the medal's award item (sCameras[0]) and
// instance its random model. The camera math is the canonical SCameraParams idiom (cf. CEarthView /
// iCommonUI.cpp): forward dir of a yaw*pitch rotation, camera point = anchor - fwd*distance, then the
// pitch/yaw/roll camera matrix at that point.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMedalsPanelView::Set( NDb::CMedal *pMedal )
{
	NDb::CRPGItem *pItem = pMedal->pModel;
	if ( !IsValid( pItem ) )
		return;

	const NDb::SCameraParams &sCamera = pItem->sCameras[0];

	CVec3 vForwardDir;
	CQuat q = CQuat( sCamera.fYaw, V3_AXIS_Z ) * CQuat( sCamera.fPitch, V3_AXIS_X );
	q.GetYAxis( &vForwardDir );

	CVec3 vCP( sCamera.vAnchor - vForwardDir * sCamera.fDistance );
	SHMatrix sCameraTransform;
	MakeMatrix( &sCameraTransform, sCamera.fPitch, sCamera.fYaw, sCamera.fRoll, vCP );

	SRand sRnd;
	SetModel( pItem->pModel->CreateModel( &sRnd ) );
	SetCameraTransform( sCameraTransform );
	UpdateMatrix();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanelView::Draw @0x1f95b0 -- rebake the spin transform only when fAngle changed, then base draw.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMedalsPanelView::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( fAngle != fLastAngle )
	{
		fLastAngle = fAngle;
		UpdateMatrix();
	}

	CModel::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanelView::ProcessMessage @0x1f9ba0 -- drag-to-spin (single angle; 0.1 rad/pixel). The base
// call goes straight to CWindow (the binary bypasses CModel's EVENT_TEMPLATECREATE handling).
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMedalsPanelView::ProcessMessage( const SEvent &sEvent )
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
			pMouseCapture = 0;
			return CWindow::ProcessMessage( sEvent );
		}
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( SCursorInfo() );
			if ( bButtonDown )
			{
				fAngle += ( sEvent.nX - sLastPoint.x ) * 0.1f;
				sLastPoint = SPoint( sEvent.nX, sEvent.nY );
				return CWindow::ProcessMessage( sEvent );
			}
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanel
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanel::CMedalsPanel @0x1f9da0 -- like the sibling panels, just chains the CWindow base + records
// the mission; all child slots start null (built lazily on EVENT_TEMPLATELOAD).
////////////////////////////////////////////////////////////////////////////////////////////////////
CMedalsPanel::CMedalsPanel( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanel::SetSelected @0x1f9740 -- show the picked row's medal in the preview and reveal it.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMedalsPanel::SetSelected( CMedalsPanelItem *pItem )
{
	if ( IsValid( pItem ) )
	{
		pMedalModelView->Set( pItem->GetMedal() );
		pMedalModelView->SetStyle( STYLE_VISIBLE, true );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanel::Generate @0x1fa680 -- rebuild the medal list for a unit: clear the rows, fetch the unit's
// gained medals, build one CMedalsPanelItem per medal, and show the first live row's medal in the preview.
//
// DEFERRED SEAM: GetGainedMedals is a documented STUB (RPGUnit.h) that returns an empty list because the
// NDb::CSide::medals source column is absent from this tree -- so the loop body and preview-Set are
// presently never reached at runtime (no rows), but compile against the real engine types.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMedalsPanel::Generate( NWorld::CUnit *pUnit )
{
	pMedals->RemoveAllItems();

	vector<CDBPtr<NDb::CMedal> > gainedMedals;
	pUnit->GetRPG()->GetRPGUnit()->GetGainedMedals( &gainedMedals );   // STUB -> empty (see RPGUnit.h)

	CMedalsPanelItem *pSelected = 0;
	for ( int nTemp = 0; nTemp < gainedMedals.size(); nTemp++ )
	{
		CMedalsPanelItem *pItem = new CMedalsPanelItem(
			SWindowInfo( pMedals, SPoint( 0, 0 ), SPoint( pMedals->GetSize().x, 0 ), "item", STYLE_ENABLED | STYLE_VISIBLE ),
			gainedMedals[nTemp] );
		pMedals->AddItem( nTemp, pItem );

		if ( !IsValid( pSelected ) )
			pSelected = pItem;
	}

	if ( IsValid( pSelected ) )
	{
		pMedalModelView->Set( pSelected->GetMedal() );
		pMedalModelView->SetStyle( STYLE_VISIBLE, true );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanel::Draw @0x1fa8e0 -- when exactly one (different) unit is selected, re-generate the list and
// hide the preview; then arm the perks-tab flash off the unit's "new medal" flag, and base-paint.
//
// DEFERRED SEAM: the per-unit "current award" flag (release CMedalsGainer +0x84) belongs to the same
// deferred subsystem; HasNewMedalToShow is a documented STUB (RPGUnit.h) returning false, so the flash is
// never armed -- the binary's "no current award" path.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMedalsPanel::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	if ( ( unitsSet.size() == 1 ) && ( pUnit != unitsSet.front()->GetUnit() ) )
	{
		pUnit = unitsSet.front()->GetUnit();
		Generate( pUnit );
		pMedalModelView->SetStyle( STYLE_VISIBLE, false );
	}

	if ( IsValid( pUnit ) )
		pPerks->SetShowFlash( pUnit->GetRPG()->GetRPGUnit()->HasNewMedalToShow() );   // STUB -> false

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMedalsPanel::ProcessMessage @0x1f9ff0 -- build the child controls on template load, forward the medals
// list selection into SetSelected, and report the active mouse-button band as consumed.
//
// Tab icon ids recovered from raw disasm (@0x1f9ff0 `mov ecx, imm` before GetUITexture -- Ghidra
// mangled them): perks 430/430/948, biography 381/381/429, character 383/383, close("medals") 437.
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMedalsPanel::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_MOUSEMOVE:
		{
			GetInterface()->SetCursorInfo( SCursorInfo() );
			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pMedalsView = new CScrollWindow<CListView>( sEvent.pLoader->GetControl( "view" ) );
			pMedals = pMedalsView->GetClientWindow();

			pPerks = new CHoverFlashButton( sEvent.pLoader->GetControl( "perks" ) );
			pPerks->AddImageState( 1, NDb::GetUITexture( 430 ) );
			pPerks->AddImageState( 0, NDb::GetUITexture( 430 ) );
			pPerks->AddImageState( 2, NDb::GetUITexture( 948 ) );

			pBiography = new CHoverButton( sEvent.pLoader->GetControl( "biography" ) );
			pBiography->AddImageState( 1, NDb::GetUITexture( 381 ) );
			pBiography->AddImageState( 0, NDb::GetUITexture( 381 ) );
			pBiography->AddImageState( 2, NDb::GetUITexture( 429 ) );

			pCharacter = new CHoverButton( sEvent.pLoader->GetControl( "character" ) );
			pCharacter->AddImageState( 1, NDb::GetUITexture( 383 ) );
			pCharacter->AddImageState( 0, NDb::GetUITexture( 383 ) );

			pMedalModelView = new CMedalsPanelView( sEvent.pLoader->GetControl( "medalview" ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pClose = GetUIWindow<CButton>( this, "medals" );
			pClose->AddImageState( 0, NDb::GetUITexture( 437 ) );

			pMedalsView->SetVScroll( GetUIWindow<CScroll>( this, "scroll" ) );
			break;
		}
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "view" )
			{
				CMedalsPanelItem *pItem = dynamic_cast<CMedalsPanelItem*>( pMedals->GetItem( pMedals->GetSelectedItem() ) );
				if ( pItem )
					SetSelected( pItem );
			}
			break;
		}
	}

	// Tail "consumed" predicate (release @0x1f9ff0): base result OR'd with membership of the active
	// mouse-button range (EVENT_FLAG_ACTIVE|EVENT_FLAG_HITTEST | 0x31..0x36 == LBUTTONUP..RBUTTONDBLCLK).
	if ( CWindow::ProcessMessage( sEvent ) || ( sEvent.nEvent > 0x06000030 && sEvent.nEvent < 0x06000037 ) )
		return true;

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NUI
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB3414140, CMedalsPanel );
REGISTER_SAVELOAD_CLASS( 0xB3414141, CMedalsPanelView );
REGISTER_SAVELOAD_CLASS( 0xB3414142, CMedalsPanelItem );
