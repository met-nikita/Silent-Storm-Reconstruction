#include "StdAfx.h"
#include "GSceneUtils.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "DiscretePos.h"
#include "wInterface.h"
#include "RPGItemInfo.h"
#include "RPGUnitInfo.h"
#include "RWGame.h"
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataSound.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Sound.h"
#include "iMission.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iMissionUI.h"
#include "iUnitPanel.h"
#include "iUnitIconBar.h"
#include "iGameStates.h"
#include "iActionDecorator.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_BASELEVEL = 3,
	N_MAXLEVELS_COUNT = 8,
	N_MAXUNITS_COUNT = 7,
	N_NUM_CRITICALS_ICONS = 6;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitTab
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitTab: public CActionDecorator<CWindow>
{
	OBJECT_BASIC_METHODS(CUnitTab);
private:
	ZDATA_(TBaseClass)
	CPtr<NGame::IMission> pMission;
	////
	CPtr<NGame::IUnitTracker> pUnit;
	CPtr<CText> pAP;
	CPtr<CText> pName;
	CPtr<CImage> pDisable;
	CPtr<CImage> pSelectedTab;
	CPtr<CImage> pUnselectedTab;
	CObj<CLineBar> pLife;
	CObj<CLineBar> pHealedLife;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); f.Add(2,&pMission); f.Add(3,&pUnit); f.Add(4,&pAP); f.Add(5,&pName); f.Add(6,&pDisable); f.Add(7,&pSelectedTab); f.Add(8,&pUnselectedTab); f.Add(9,&pPKLifeBackground); f.Add(10,&pBarBackground); f.Add(11,&pLifeBackground); f.Add(12,&pLife); f.Add(13,&pHealedLife); f.Add(14,&pPKLife); return 0; }
	CObj<CImage> pPKLifeBackground;
	CObj<CImage> pBarBackground;
	CObj<CImage> pLifeBackground;
	CObj<CLineBar> pPKLife;

public:
	CUnitTab() {}
	CUnitTab( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool CanHandleState( NGame::IState *pState ) const;
	CObjectBase* GetTarget();

	NGame::IUnitTracker* Get() const;
	void Set( NGame::IUnitTracker *pUnit );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitTab::CUnitTab( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	TBaseClass( sInfo, _pMission ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitTab::CanHandleState( NGame::IState *pState ) const
{
	CDynamicCast<NGame::CStateMove> pMove(pState);
	if (pMove)
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CUnitTab::GetTarget()
{
	return pUnit->GetUnit();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGame::IUnitTracker* CUnitTab::Get() const
{
	return pUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTab::Set( NGame::IUnitTracker *_pUnit )
{
	pUnit = _pUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitTab::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pPKLife = new CLineBar( sEvent.pLoader->GetControl( "pklife" ) );
			pPKLifeBackground = new CImage( sEvent.pLoader->GetControl( "pklife_background" ) );
			pLife = new CLineBar( sEvent.pLoader->GetControl( "life" ) );
			pHealedLife = new CLineBar( sEvent.pLoader->GetControl( "life_healed" ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pAP = GetUIWindow<CText>( this, "ap" );
			pName = GetUIWindow<CText>( this, "name" );

			pDisable = GetUIWindow<CImage>( this, "disable" );
			pSelectedTab = GetUIWindow<CImage>( this, "ut_selected" );
			pUnselectedTab = GetUIWindow<CImage>( this, "ut_unselected" );

			pBarBackground = GetUIWindow<CImage>( this, "bar_background" );
			pLifeBackground = GetUIWindow<CImage>( this, "life_background" );
			break;
		}
	}

	return TBaseClass::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTab::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !IsValid( pUnit ) )
	{
		SetStyle( STYLE_VISIBLE, false );
		return;
	}

	SetStyle( STYLE_VISIBLE, true );
	pDisable->SetStyle( STYLE_VISIBLE, !pUnit->IsActive() );
	pSelectedTab->SetStyle( STYLE_VISIBLE, pUnit->IsSelected() );
	pUnselectedTab->SetStyle( STYLE_VISIBLE, !pUnit->IsSelected() );

	NRPG::SUnitInfo sUnitInfo;
	pUnit->GetUnit()->GetInfo( &sUnitInfo );

	WCHAR wsText[256];

	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", sUnitInfo.nAP );
	pAP->SetText( wsText );
	pAP->SetStyle( STYLE_VISIBLE, !pMission->IsRealTime() );

	pName->SetText( L"<font face=Courier size=10pt><nowrap>" + pUnit->GetUnit()->GetRPG()->GetName() );

	pLife->Set( float( sUnitInfo.nHP ) / sUnitInfo.nMaxHP );
	pHealedLife->Set( float( sUnitInfo.nHP + sUnitInfo.nHealedHP ) / sUnitInfo.nMaxHP );

	// retail CUnitTab::Draw @0x254810: the PK flag (unit wears a Panzerklein) selects the bar
	// layout. SetImage draws the texture at its natural size, so the texture set is what makes the
	// life bar tall (one bar fills the height of two) or short (paired with the PK-armor bar).
	pPKLife->SetStyle( STYLE_VISIBLE, sUnitInfo.bWearingPK );
	pPKLifeBackground->SetStyle( STYLE_VISIBLE, sUnitInfo.bWearingPK );
	if ( !sUnitInfo.bWearingPK )
	{
		// on-foot: single double-height life bar, PK-armor bar hidden.
		pLife->SetImage( NDb::GetUITexture( 897 ) );
		pHealedLife->SetImage( NDb::GetUITexture( 896 ) );
		pBarBackground->SetImage( NDb::GetUITexture( 895 ) );
		pLifeBackground->SetImage( NDb::GetUITexture( 898 ) );
	}
	else
	{
		// piloting a Panzerklein: short life bar + the PK-armor bar fed from the worn PK's VP.
		pLife->SetImage( NDb::GetUITexture( 893 ) );
		pHealedLife->SetImage( NDb::GetUITexture( 891 ) );
		pBarBackground->SetImage( NDb::GetUITexture( 889 ) );
		pLifeBackground->SetImage( NDb::GetUITexture( 890 ) );
		pPKLife->Set( sUnitInfo.nMaxPKLife ? float( sUnitInfo.nPKLife ) / sUnitInfo.nMaxPKLife : 0.f );
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitsTab
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitsTabBar: public CWindow
{
	OBJECT_BASIC_METHODS(CUnitsTabBar)
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	////
	vector< CPtr<CUnitTab> > tabsSet;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&tabsSet); return 0; }

public:
	CUnitsTabBar() {}
	CUnitsTabBar( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitsTabBar::CUnitsTabBar( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission ), tabsSet( N_MAXUNITS_COUNT )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitsTabBar::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_LBUTTONUP:
		{
			for ( int nTemp = 0; nTemp < tabsSet.size(); nTemp++ )
			{
				NGame::IUnitTracker *pUnit = tabsSet[nTemp]->Get();
				if ( tabsSet[nTemp]->HitTest( sEvent.nX, sEvent.nY ) && IsValid( pUnit ) && pUnit->IsSelected() )
					pMission->FocusCameraOnUnit( pUnit->GetUnit() );
			}

			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			for ( int nTemp = 0; nTemp < tabsSet.size(); nTemp++ )
			{
				tabsSet[nTemp] = new CUnitTab( sEvent.pLoader->GetControl( NStr::Format( "hero_%d", ( nTemp + 1 ) ) ), pMission );
				tabsSet[nTemp]->Set( 0 );
			}

			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitsTabBar::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	vector< CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetUnits( &unitsSet );

	int nCount = 0;
	for ( int nTemp = 0; nTemp < tabsSet.size(); nTemp++ )
	{
		tabsSet[nTemp]->Set( 0 );
		if ( nTemp < unitsSet.size() )
		{
			if ( !unitsSet[nTemp]->GetUnit()->IsDead() )
			{
				tabsSet[nCount]->Set( unitsSet[nTemp] );
				tabsSet[nCount]->SetStyle( STYLE_VISIBLE, true );
				tabsSet[nCount]->SetStyle( STYLE_TOPMOST, unitsSet[nTemp]->IsSelected() );
				nCount++;
			}
		}
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitFace
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitFace: public CActionDecorator<CUnitView>
{
	OBJECT_BASIC_METHODS(CUnitFace)
private:
	ZDATA_(TBaseClass)
	CPtr<NGame::IUnitTracker> pUnit;
	////
	// retail reworked CUnitFace into an ack-effect face animation with NO life bars: retail
	// CUnitFace::ProcessMessage @0x254d20 fetches no life/life_healed and Draw @0x254df0 is the
	// eStage ack-effect state machine. Unit life shows only on the hero tabs (CUnitTab), never on
	// the single-/multi-unit face (retail CInfoPanelSingleUnit @0x25cd30 has no life member), so
	// the dev's pLife/pHealedLife were dropped for retail parity -- they also caused the
	// "face (life,life_healed)" container-not-found errors. The ack-effect ANIMATION behavior
	// (members below, already save-reconciled tags 1-9) is a release-new feature -- deferred.
	enum EStage { ST_NONE=0, ST_SOURCE_HIDE=1, ST_TARGET_SHOW=2, ST_ACK_WAIT=3, ST_TARGET_HIDE=4, ST_SOURCE_SHOW=5, ST_ACK_TERMINATE=6 };
	STime sEventTime;
	EStage eStage = ST_NONE;
	CPtr<NUI::CAckEvent> pEvent;
	CObj<NUI::CImage> pAckEffect;
	bool bDoNotPlayEffect = false;
	CPtr<NWorld::CUnit> pFaceUnit;
	CObj<NUI::CImage> pRedImage;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); f.Add(2,&pUnit); f.Add(3,&sEventTime); f.Add(4,&eStage); f.Add(5,&pEvent); f.Add(6,&pAckEffect); f.Add(7,&bDoNotPlayEffect); f.Add(8,&pFaceUnit); f.Add(9,&pRedImage); return 0; }

public:
	CUnitFace() {}
	CUnitFace( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool CanHandleState( NGame::IState *pState ) const;
	CObjectBase* GetTarget();

	void SetUnit( NGame::IUnitTracker *pUnit );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitFace::CUnitFace( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	TBaseClass( sInfo, _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitFace::CanHandleState( NGame::IState *pState ) const
{
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectBase* CUnitFace::GetTarget()
{
	return pUnit->GetUnit();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitFace::SetUnit( NGame::IUnitTracker *_pUnit )
{
	// Guard on change (retail SetUnit @0x254c80): the dev called this every frame, re-creating the
	// show-unit and resetting the skeletal animation to frame 0 -> the face looked frozen. Build the
	// face model once per unit so its looping POSE_STAND idle (CFakeWorldUnit::Update) can play out.
	if ( pUnit == _pUnit )
		return;

	pUnit = _pUnit;
	if ( IsValid( pUnit ) )
		CUnitView::SetUnit( pUnit->GetUnit() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitFace::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_LBUTTONDOWN:
		return true;
	case EVENT_LBUTTONUP:
		{
			SendMessage( GetParent(), SEvent( EVENT_NOTIFY, GetWindowID() ) );
			return true;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitFace::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	// life bars removed for retail parity (see class comment). The ack-effect Draw state
	// machine (retail @0x254df0) is the deferred behavior; base CUnitView draws the portrait.
	TBaseClass::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSlotReloadImage
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSlotReloadButton: public CButton
{
	OBJECT_NOCOPY_METHODS(CSlotReloadButton)
private:
	ZDATA_(CButton)
	CPtr<NGame::IMission> pMission;
	////
	CObj<CModel> pModel;
	CObj<CToolTip> pToolTip;
	CDBPtr<NDb::CRPGItem> pItem;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CButton*)this); f.Add(2,&pMission); f.Add(3,&pIcon); f.Add(4,&pModel); f.Add(5,&pToolTip); f.Add(6,&pItem); f.Add(7,&nSlot); return 0; }
	CObj<CImage> pIcon;
	int nSlot = 0;

public:
	CSlotReloadButton() {}
	CSlotReloadButton( const SWindowInfo &sInfo, NGame::IMission *pMission );

	void Set( NDb::CRPGItem *pItem );
	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CSlotReloadButton::CSlotReloadButton( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CButton( sInfo ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlotReloadButton::Set( NDb::CRPGItem *_pItem )
{
	if ( _pItem == pItem )
		return;

	pItem = _pItem;
	if ( pItem->pModel )
	{
		const NDb::SCameraParams &sCamera = pItem->sCameras[NDb::CAMERA_RELOADBUTTON];

		CVec3 vForwardDir;
		CQuat q = CQuat( sCamera.fYaw, V3_AXIS_Z ) * CQuat( sCamera.fPitch, V3_AXIS_X );
		q.GetYAxis( &vForwardDir );

		CVec3 vCP( sCamera.vAnchor - vForwardDir * sCamera.fDistance );
		SFBTransform res;
		MakeMatrix( &res, sCamera.fPitch, sCamera.fYaw, sCamera.fRoll, vCP );

		SRand sRnd;
		pModel->SetModel( pItem->pModel->CreateModel( &sRnd ) );
		pModel->SetTransform( new CFBTransform( res ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSlotReloadButton::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATECREATE:
		{
			pModel = new CModel( SWindowInfo( this, SPoint( 0, 0 ), GetSize(), "", STYLE_ENABLED | STYLE_VISIBLE | STYLE_TRANSPARENT | STYLE_TOPMOST ) );
			pToolTip = new CToolTip( SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 0, 0 ), "tooltip", STYLE_ENABLED ) );
			SetToolTip( pToolTip );
			break;
		}
	}

	return CButton::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSlotReloadButton::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	ASSERT( unitsSet.size() == 1 );
	if ( unitsSet.size() != 1 )
		return;

	CPtr<NGame::IUnitTracker> pUnit = unitsSet.front();

	int nActionAP = 0;
	NWorld::EUnitCommandResult eResult = pUnit->GetUnit()->CanDo( new NWorld::CCmdReload, 0, &nActionAP );

	CPtr<NDb::CString> pString;
	if ( ( eResult == NWorld::UCR_OK ) || ( eResult == NWorld::UCR_NOT_ENOUGH_AP ) )
		pString = NDb::GetString( 4313 );
	else
		pString = NDb::GetString( 4314 );

	wstring wsToolTipTemplate( L"%s" );
	if ( IsValid( pString ) )
		wsToolTipTemplate = pString->szStr;

	if ( nActionAP != -1 )
		pToolTip->SetVal( L"ap", nActionAP );
	else
		pToolTip->SetVal( L"ap", L"N/A" );

	NGfx::SPixel8888 sColor( 0xFF, 0xFF, 0xFF, 0xFF );
	if ( eResult == NWorld::UCR_NOT_ENOUGH_AP )
		sColor = NGfx::SPixel8888( 0x5F, 0x5F, 0xBF, 0xFF );

	SetColor( sColor );
	pModel->SetColor( sColor );

	CButton::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInfoPanelSlot
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInfoPanelSlot: public CSlot
{
	OBJECT_NOCOPY_METHODS(CInfoPanelSlot)
private:
	ZDATA_(CSlot)
	CPtr<NGame::IMission> pMission;
	////
	NDb::ESlot eType;
	CPtr<NWorld::CUnit> pUnit;
	////
	CPtr<CWindow> pFade;
	CObj<CMLText> pAmmo;
	CObj<CSlotReloadButton> pReload;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CSlot*)this); f.Add(2,&pMission); f.Add(3,&eType); f.Add(4,&pUnit); f.Add(5,&pFade); f.Add(6,&pAmmo); f.Add(7,&pReload); return 0; }

public:
	CInfoPanelSlot() {}
	CInfoPanelSlot( const SWindowInfo &sInfo, NGame::IMission *pMission, NDb::ESlot eType );

	void Set( NWorld::CUnit *pUnit );

	void Take( int nX, int nY );
	void Place( int nX, int nY, const NWorld::SItem &sItem );
	bool CanPlace( int nX, int nY, const NWorld::SItem &sItem, int *nAP = 0 );
	void GetItemsList( vector<SItem> *pItemsSet );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInfoPanelSlot
////////////////////////////////////////////////////////////////////////////////////////////////////
CInfoPanelSlot::CInfoPanelSlot( const SWindowInfo &sInfo, NGame::IMission *_pMission, NDb::ESlot _eType ):
	CSlot( sInfo, _pMission, 1, 1, NDb::CAMERA_SLOT, false ), pMission( _pMission ), eType( _eType )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelSlot::Set( NWorld::CUnit *_pUnit )
{
	pUnit = _pUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelSlot::Take( int nX, int nY )
{
	NWorld::SItem sSource;
	sSource.eType = NWorld::SItem::SLOT;
	sSource.nSlot = eType;
	sSource.pItem = pUnit->GetRPG()->GetInventoryInfo()->Get( eType );
	sSource.pUnit = pUnit;
	pMission->CommandState( new NGame::CStateMoveItem( pUnit, sSource, NWorld::SItem( pUnit, NWorld::SItem::HAND ) ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelSlot::Place( int nX, int nY, const NWorld::SItem &sItem )
{
	ASSERT( IsValid( pUnit ) );

	NWorld::SItem sTarget;
	sTarget.eType = NWorld::SItem::SLOT;
	sTarget.nSlot = eType;
	sTarget.pUnit = pUnit;

	pMission->Command( pUnit, new NWorld::CCmdMoveInventoryItem( NWorld::SItem( sItem.pUnit, NWorld::SItem::HAND, sItem.pItem.GetPtr() ), sTarget ) );
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInfoPanelSlot::CanPlace( int nX, int nY, const NWorld::SItem &sItem, int *nAP )
{
	NWorld::SItem sTarget;
	sTarget.eType = NWorld::SItem::SLOT;
	sTarget.nSlot = eType;
	sTarget.pUnit = pUnit;

	NWorld::EUnitCommandResult eRes = pUnit->CanDo( new NWorld::CCmdMoveInventoryItem( NWorld::SItem( sItem.pUnit, NWorld::SItem::HAND, sItem.pItem ), sTarget ) );
	if ( eRes != NWorld::UCR_OK )
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelSlot::GetItemsList( vector<SItem> *pItemsSet )
{
	ASSERT( IsValid( pUnit ) );

	CPtr<NRPG::IInventoryInfo> pInfo = pUnit->GetRPG()->GetInventoryInfo();
	CPtr<NRPG::IInventoryItem> pItem = pInfo->Get( eType );
	if ( IsValid( pItem ) )
	{
		SItem &sItem = *pItemsSet->insert( pItemsSet->end(), SItem());
		sItem.sPos = CTPoint<int>( 0, 0 );
		sItem.pItem = pItem;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInfoPanelSlot::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_LBUTTONDOWN:
		{
			NWorld::IPlayer::SItemInfo sInfo;
			if ( !GetDragItem( &sInfo ) && ( pUnit->GetRPG()->GetInventoryInfo()->GetActiveSlot() != eType ) )
			{
				pMission->Command( pUnit, new NWorld::CCmdSetActiveItem( eType ) );
				return true;
			}

			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pAmmo = new CMLText( sEvent.pLoader->GetControl( "ammo" ) );
			pReload = new CSlotReloadButton( sEvent.pLoader->GetControl( "weapon_reload" ), pMission );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pFade = GetUIWindow<CWindow>( this, "fade" );
			break;
		}
	}

	if ( CSlot::ProcessMessage( sEvent ) )
		return true;

	switch( sEvent.nEvent )
	{
	case EVENT_RBUTTONDOWN:
	case EVENT_LBUTTONDOWN:
		{
			NWorld::IPlayer::SItemInfo sInfo;
			if ( GetDragItem( &sInfo ) )
			{
				NWorld::SItem sSource( sInfo.pUnit, NWorld::SItem::HAND, sInfo.pItem );

				CDynamicCast<NRPG::IClipItem> pClip( sInfo.pItem );
				CDynamicCast<NRPG::IWeaponItemInfo> pWeapon( pUnit->GetRPG()->GetInventoryInfo()->Get( eType ) );
				if ( IsValid( pClip ) && IsValid( pWeapon ) )
					pMission->Command( pUnit, new NWorld::CCmdLoadWeapon( pWeapon, sSource ) );
				else
				{
					if ( CanPlace( sEvent.nX, sEvent.nY, sSource ) )
						Place( sEvent.nX, sEvent.nY, sSource );
					else
						PlaySound( NDb::GetSound( NGame::N_SOUND_ERROR ) );
				}
			}
			else if ( pUnit->GetRPG()->GetInventoryInfo()->GetActiveSlot() == eType )
				Take( sEvent.nX, sEvent.nY );
			return true;
		}
	case EVENT_RBUTTONUP:
	case EVENT_LBUTTONUP:
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelSlot::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( !IsValid( pUnit ) )
		return;

	CPtr<NRPG::IInventoryInfo> pInventory = pUnit->GetRPG()->GetInventoryInfo();
	pFade->SetStyle( STYLE_VISIBLE, pInventory->GetActiveSlot() != eType );

	CPtr<NRPG::IInventoryItem> pItem = pInventory->Get( eType );
	CDynamicCast<NRPG::IWeaponItemInfo> pWeapon(pItem);
	if (pWeapon)
	{
		CPtr<NRPG::IClipItem> pRPGClipItem = pWeapon->GetInnerClip();
		pReload->SetStyle( STYLE_VISIBLE, true );
		pReload->Set( pRPGClipItem->GetDBItem() );

		WCHAR wsBuffer[256];
		swprintf( wsBuffer, L"<color=FFB4997C><font face=Impact size=36pt outlinesize=2 outlinecolor=FF513E2B><left>%d/%d", pRPGClipItem->GetIncQuantity(), pRPGClipItem->GetMaxIncQuantity() );
		pAmmo->SetText( wsBuffer );
		pAmmo->SetStyle( STYLE_VISIBLE, true );
	}
	else
	{
		pAmmo->SetStyle( STYLE_VISIBLE, false );
		pReload->SetStyle( STYLE_VISIBLE, false );
	}

	CSlot::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInfoPanelSpecialSlot
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInfoPanelSpecialSlot: public CWindow
{
	OBJECT_NOCOPY_METHODS(CInfoPanelSpecialSlot)
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	////
	CPtr<NWorld::CUnit> pUnit;
	////
	CObj<CText> pWeaponAmmoText;
	CObj<CShowItemModel> pItemModel;
	CObj<CSlotReloadButton> pReload;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pUnit); f.Add(4,&pWeaponAmmoText); f.Add(5,&pItemModel); f.Add(6,&pReload); return 0; }

public:
	CInfoPanelSpecialSlot() {}
	CInfoPanelSpecialSlot( const SWindowInfo &sInfo, NGame::IMission *pMission );

	void Set( NWorld::CUnit *pUnit );
	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInfoPanelSpecialSlot
////////////////////////////////////////////////////////////////////////////////////////////////////
CInfoPanelSpecialSlot::CInfoPanelSpecialSlot( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelSpecialSlot::Set( NWorld::CUnit *_pUnit )
{
	pUnit = _pUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInfoPanelSpecialSlot::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pReload = new CSlotReloadButton( sEvent.pLoader->GetControl( "weapon_reload" ), pMission );
			pItemModel = new CShowItemModel( sEvent.pLoader->GetControl( "view" ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pWeaponAmmoText = GetUIWindow<CText>( this, "ammo" );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelSpecialSlot::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	CPtr<NRPG::IWeaponItemInfo> pItem = pUnit->GetRPG()->GetCannonItemInfo();

	pItemModel->Set( pItem, NDb::CAMERA_SLOT );

	CPtr<NRPG::IClipItem> pRPGClipItem = pItem->GetInnerClip();
	pReload->Set( pRPGClipItem->GetDBItem() );

	WCHAR wsBuffer[1024];
	swprintf( wsBuffer, L"<font face=Courier size=16pt><center>%d/%d", pRPGClipItem->GetIncQuantity(), pRPGClipItem->GetMaxIncQuantity() );
	pWeaponAmmoText->SetText( wsBuffer );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInfoPanelSingleUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInfoPanelCritical: public CImage
{
	OBJECT_BASIC_METHODS(CInfoPanelCritical)
private:
	ZDATA_(CImage)
	CPtr<NRPG::ICriticalInfo> pCritical;
	////
	CObj<CToolTip> pToolTip;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CImage*)this); f.Add(2,&pCritical); f.Add(3,&pToolTip); return 0; }

public:
	CInfoPanelCritical() {}
	CInfoPanelCritical( const SWindowInfo &sInfo );

	void Set( NRPG::ICriticalInfo *pCritical );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CInfoPanelCritical::CInfoPanelCritical( const SWindowInfo &sInfo ):
	CImage( sInfo )
{
	pToolTip = new CToolTip( SWindowInfo( GetInterface(), SPoint( 0, 0 ), SPoint( 0, 0 ), "tooltip", STYLE_ENABLED ) );
	SetToolTip( pToolTip );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelCritical::Set( NRPG::ICriticalInfo *_pCritical )
{
	if ( pCritical == _pCritical )
		return;

	pCritical = _pCritical;

	switch ( pCritical->GetCriticalType() )
	{
	case NDb::C_AP_REDUCTION:
		SetImage( NDb::GetUITexture( 617 ) );
		pToolTip->SetText( GetDBString( 11177 ) );
		break;
	case NDb::C_BLIND:
		SetImage( NDb::GetUITexture( 616 ) );
		pToolTip->SetText( GetDBString( 11178 ) );
		break;
	case NDb::C_WEAPONSKILL_REDUCTION:
		SetImage( NDb::GetUITexture( 615 ) );
		pToolTip->SetText( GetDBString( 11179 ) );
		break;
	case NDb::C_VP:
		SetImage( NDb::GetUITexture( 614 ) );
		pToolTip->SetText( GetDBString( 11180 ) );
		break;
	case NDb::C_MOTIONLESS:
		SetImage( NDb::GetUITexture( 613 ) );
		pToolTip->SetText( GetDBString( 11181 ) );
		break;
	case NDb::C_ENCUMBRANCE:
		SetImage( NDb::GetUITexture( 612 ) );
		pToolTip->SetText( GetDBString( 11182 ) );
		break;
	case NDb::C_ACCIDENTAL_SHOT:
		SetImage( NDb::GetUITexture( 611 ) );
		pToolTip->SetText( GetDBString( 11183 ) );
		break;
	case NDb::C_LOST_WEAPON:
		SetImage( NDb::GetUITexture( 610 ) );
		pToolTip->SetText( GetDBString( 11184 ) );
		break;
	case NDb::C_IDLE_HAND:
		SetImage( NDb::GetUITexture( 609 ) );
		pToolTip->SetText( GetDBString( 11185 ) );
		break;
	case NDb::C_STUN:
		SetImage( NDb::GetUITexture( 608 ) );
		pToolTip->SetText( GetDBString( 11186 ) );
		break;
	case NDb::C_DAMAGE_WEAPON:
		SetImage( NDb::GetUITexture( 607 ) );
		pToolTip->SetText( GetDBString( 11187 ) );
		break;
	case NDb::C_PATIENT:
		SetImage( NDb::GetUITexture( 606 ) );
		pToolTip->SetText( GetDBString( 11188 ) );
		break;
	case NDb::C_DEAF:
		SetImage( NDb::GetUITexture( 605 ) );
		pToolTip->SetText( GetDBString( 11189 ) );
		break;
	case NDb::C_BLEEDING:
		SetImage( NDb::GetUITexture( 604 ) );
		pToolTip->SetText( GetDBString( 11190 ) );
		break;
	}

	switch( pCritical->GetCriticalLocation() )
	{
	case NDb::CL_HEAD:
		pToolTip->SetVal( L"location", GetDBString( 11191 ) );
		break;
	case NDb::CL_TORSO:
		pToolTip->SetVal( L"location", GetDBString( 11192 ) );
		break;
	case NDb::CL_ARMS:
		pToolTip->SetVal( L"location", GetDBString( 11193 ) );
		break;
	case NDb::CL_LEGS:
		pToolTip->SetVal( L"location", GetDBString( 11194 ) );
		break;
	case NDb::CL_ANY:
		pToolTip->SetVal( L"location", GetDBString( 11195 ) );
		break;
	default:
		ASSERT( 0 );
	}

	pToolTip->SetVal( L"difficulty", pCritical->GetDifficultyClass() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelCritical::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	/// UpdateCritical Values
	CImage::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMissionCriticalBleedingFake
////////////////////////////////////////////////////////////////////////////////////////////////////
// A transient stand-in NRPG::ICriticalInfo the single-unit panel uses to draw a permanent
// "bleeding" critical icon (a mission-scripted wound not backed by a real NRPG::CCritical).
// Every accessor is a hard-coded constant except the displayed value, carried as nValue.
// (release iCriticalIcons.obj; nValue@+12, sizeof 16). GetDifficultyClass returns -1 -- in the
// release it was COMDAT-folded with GetRemainingTime (both `return -1`), hence no distinct RVA.
class CMissionCriticalBleedingFake: public NRPG::ICriticalInfo
{
	OBJECT_BASIC_METHODS(CMissionCriticalBleedingFake)
private:
	ZDATA
	int nValue;
	ZEND int operator&( CStructureSaver &f ) { f.Add( 1, &nValue ); return 0; }

public:
	CMissionCriticalBleedingFake(): nValue( 0 ) {}
	CMissionCriticalBleedingFake( int _nValue ): nValue( _nValue ) {}

	int GetRemainingTime() const { return -1; }
	float GetValue() const { return (float)nValue; }

	virtual int GetDifficultyClass() const { return -1; }
	virtual NDb::ECritical GetCriticalType() const { return NDb::C_BLEEDING; }
	virtual NDb::ECriticalLocation GetCriticalLocation() const { return NDb::CL_ANY; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInfoPanelSingleUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInfoPanelSingleUnit: public CWindow
{
	OBJECT_BASIC_METHODS(CInfoPanelSingleUnit)
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	////
	CPtr<NGame::IUnitTracker> pUnit;
	////
	CPtr<CMLText> pAP;
	CObj<CUnitFace> pUnitFace;
	CObj<CInfoPanelSlot> pLeftSlot;
	CObj<CInfoPanelSlot> pRightSlot;
	CObj<CInfoPanelSpecialSlot> pSpecialSlot;
	////
	vector<CObj<CInfoPanelCritical> > criticalIconsSet;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pUnit); f.Add(4,&pAP); f.Add(5,&pUnitFace); f.Add(6,&pLeftSlot); f.Add(7,&pRightSlot); f.Add(8,&pSpecialSlot); f.Add(9,&criticalIconsSet); return 0; }

public:
	CInfoPanelSingleUnit() {}
	CInfoPanelSingleUnit( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CInfoPanelSingleUnit::CInfoPanelSingleUnit( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInfoPanelSingleUnit::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "face" )
			{
				pMission->FocusCameraOnUnit( pUnit->GetUnit() );
				return true;
			}
			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pAP = new CMLText( sEvent.pLoader->GetControl( "ap" ) );
			pUnitFace = new CUnitFace( sEvent.pLoader->GetControl( "face" ), pMission );
			pLeftSlot = new CInfoPanelSlot( sEvent.pLoader->GetControl( "slot_left" ), pMission, NDb::SLOT_1 );
			pRightSlot = new CInfoPanelSlot( sEvent.pLoader->GetControl( "slot_right" ), pMission, NDb::SLOT_2 );
			pSpecialSlot = new CInfoPanelSpecialSlot( sEvent.pLoader->GetControl( "slot_double" ), pMission );

			criticalIconsSet.resize( N_NUM_CRITICALS_ICONS );
			for ( int nTemp = 0; nTemp < N_NUM_CRITICALS_ICONS; nTemp++ )
				criticalIconsSet[nTemp] = new CInfoPanelCritical( sEvent.pLoader->GetControl( NStr::Format( "critical_%d", nTemp ) ) );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelSingleUnit::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetSelectedUnits( &unitsSet );

	pUnit = unitsSet[0];

	int nCount = 0;
	list<CPtr<NRPG::ICriticalInfo> > criticalsList;
	pUnit->GetUnit()->GetRPG()->GetCriticalsList( &criticalsList );
	for( list<CPtr<NRPG::ICriticalInfo> >::const_iterator iTemp = criticalsList.begin(); iTemp != criticalsList.end(); iTemp++ )
	{
		if ( nCount >= criticalIconsSet.size() )
			break;

		CInfoPanelCritical *pIcon = criticalIconsSet[nCount];
		pIcon->Set( *iTemp );
		pIcon->SetStyle( STYLE_VISIBLE, true );

		nCount++;
	}
	for ( int nTemp = nCount; nTemp < criticalIconsSet.size(); nTemp++ )
		criticalIconsSet[nTemp]->SetStyle( STYLE_VISIBLE, false );


	CPtr<NRPG::IWeaponItemInfo> pItem = pUnit->GetUnit()->GetRPG()->GetCannonItemInfo();
	pSpecialSlot->Set( pUnit->GetUnit() );
	pSpecialSlot->SetStyle( STYLE_VISIBLE, IsValid( pItem ) );

	pAP->SetStyle( STYLE_VISIBLE, !pMission->IsRealTime() );
	if ( !pMission->IsRealTime() )
	{
		NRPG::SUnitInfo sUnitInfo;
		pUnit->GetUnit()->GetInfo( &sUnitInfo );

		WCHAR wsBuffer[256];
		swprintf( wsBuffer, L"<color=FFB4997C><font face=Impact size=36pt outlinesize=2 outlinecolor=FF513E2B><wrapright>%d<br><left>AP", sUnitInfo.nAP );
		pAP->SetText( wsBuffer );
	}

	pLeftSlot->Set( pUnit->GetUnit() );
	pRightSlot->Set( pUnit->GetUnit() );
	pSpecialSlot->Set( pUnit->GetUnit() );
	pUnitFace->SetUnit( pUnit );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInfoPanelMultipleUnits
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInfoPanelMultipleUnits: public CWindow
{
	OBJECT_BASIC_METHODS(CInfoPanelMultipleUnits)
private:
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	CPtr<NGame::IUnitTracker> pUnit;
	////
	vector<CPtr<CImage> > selectionsSet;
	vector<CObj<CUnitFace> > facesSet;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pUnit); f.Add(4,&selectionsSet); f.Add(5,&facesSet); return 0; }

public:
	CInfoPanelMultipleUnits() {}
	CInfoPanelMultipleUnits( const SWindowInfo &sInfo, NGame::IMission *pMission );

	void Set( NGame::IUnitTracker *pUnit );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CInfoPanelMultipleUnits::CInfoPanelMultipleUnits( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission ), selectionsSet( N_MAXUNITS_COUNT ), facesSet( N_MAXUNITS_COUNT )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInfoPanelMultipleUnits::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			for ( int nTemp = 0; nTemp < facesSet.size(); nTemp++ )
			{
				facesSet[nTemp] = new CUnitFace( sEvent.pLoader->GetControl( NStr::Format( "hero_%d", ( nTemp + 1 ) ) ), pMission );
				facesSet[nTemp]->SetUnit( 0 );
			}
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			for ( int nTemp = 0; nTemp < facesSet.size(); nTemp++ )
				selectionsSet[nTemp] = GetUIWindow<CImage>( this, NStr::Format( "hero_%d_selection", ( nTemp + 1 ) ) );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CInfoPanelMultipleUnits::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	vector<CPtr<NGame::IUnitTracker> > unitsSet;
	pMission->GetUnits( &unitsSet );
	for ( int nTemp = 0; nTemp < facesSet.size(); nTemp++ )
	{
		if ( nTemp < unitsSet.size() )
		{
			facesSet[nTemp]->SetUnit( unitsSet[nTemp] );
			facesSet[nTemp]->SetStyle( STYLE_VISIBLE, true );
			selectionsSet[nTemp]->SetStyle( STYLE_VISIBLE, unitsSet[nTemp]->IsSelected() );
		}
		else
		{
			facesSet[nTemp]->SetStyle( STYLE_VISIBLE, false );
			selectionsSet[nTemp]->SetStyle( STYLE_VISIBLE, false );
		}
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLevelSwitchBar
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLevelSwitchBar: public CWindow
{
	OBJECT_BASIC_METHODS(CLevelSwitchBar)
private:
	enum
	{
		STATE_VISIBLE,
		STATE_HIDDEN
	};
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	////
	CPtr<CButton> pUp;
	CPtr<CButton> pDown;
	vector<CPtr<CButton> > buttonsSet;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&pUp); f.Add(4,&pDown); f.Add(5,&buttonsSet); return 0; }

public:
	CLevelSwitchBar() {}
	CLevelSwitchBar( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CLevelSwitchBar::CLevelSwitchBar( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission ), buttonsSet( N_MAXLEVELS_COUNT )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLevelSwitchBar::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			int nSelectedFloor = -1;
			if ( sEvent.szID.compare( "up" ) == 0 )
				nSelectedFloor = pMission->GetScene()->GetCutFloor() + 1 + N_BASELEVEL;
			else if ( sEvent.szID.compare( "down" ) == 0  )
				nSelectedFloor = pMission->GetScene()->GetCutFloor() - 1 + N_BASELEVEL;
			else if ( sEvent.szID.compare( "level_1" ) == 0  )
				nSelectedFloor = 0;
			else if ( sEvent.szID.compare( "level_2" ) == 0  )
				nSelectedFloor = 1;
			else if ( sEvent.szID.compare( "level_3" ) == 0  )
				nSelectedFloor = 2;
			else if ( sEvent.szID.compare( "level_4" ) == 0  )
				nSelectedFloor = 3;
			else if ( sEvent.szID.compare( "level_5" ) == 0  )
				nSelectedFloor = 4;
			else if ( sEvent.szID.compare( "level_6" ) == 0  )
				nSelectedFloor = 5;
			else if ( sEvent.szID.compare( "level_7" ) == 0  )
				nSelectedFloor = 6;
			else if ( sEvent.szID.compare( "level_8" ) == 0  )
				nSelectedFloor = 7;

			if ( nSelectedFloor != -1 )
			{
				pMission->GetScene()->SetCutFloor( nSelectedFloor - N_BASELEVEL );
				return true;
			}

			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pUp = GetUIWindow<CButton>( this, "up" );
			pUp->AddImageState( 0, NDb::GetUITexture( 399 ) );
			pDown = GetUIWindow<CButton>( this, "down" );
			pDown->AddImageState( 0, NDb::GetUITexture( 398 ) );

			for ( int nTemp = 0; nTemp < buttonsSet.size(); nTemp++ )
			{
				buttonsSet[nTemp] = GetUIWindow<CButton>( this, NStr::Format( "level_%d", ( nTemp + 1 ) ) );
				if ( nTemp < N_BASELEVEL )
				{
					buttonsSet[nTemp]->AddImageState( STATE_VISIBLE, NDb::GetUITexture( 405 ) );
					buttonsSet[nTemp]->AddImageState( STATE_HIDDEN, NDb::GetUITexture( 407 ) );
				}
				else
				{
					buttonsSet[nTemp]->AddImageState( STATE_VISIBLE, NDb::GetUITexture( 404 ) );
					buttonsSet[nTemp]->AddImageState( STATE_HIDDEN, NDb::GetUITexture( 406 ) );
				}
			}

			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLevelSwitchBar::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	int nSelectedFloor = pMission->GetScene()->GetCutFloor();

	for ( int nTemp = 0; nTemp < buttonsSet.size(); nTemp++ )
	{
		if ( nTemp - N_BASELEVEL <= nSelectedFloor )
			buttonsSet[nTemp]->SetActiveState( STATE_VISIBLE );
		else
			buttonsSet[nTemp]->SetActiveState( STATE_HIDDEN );
	}

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitPanel
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitPanel::CUnitPanel( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
	CWindow( sInfo ), pMission( _pMission )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitPanel::ProcessMessage( const SEvent &sEvent )
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
			pUnitIconsBar = new CUnitIconsBar( sEvent.pLoader->GetControl( "uniticonbar" ), pMission );
			pLevelSwitchBar = new CLevelSwitchBar( sEvent.pLoader->GetControl( "levelswitch" ), pMission );

			pUnitsTabBar = new CUnitsTabBar( sEvent.pLoader->GetControl( "unitstabbar" ), pMission );
			pInfoPanelSingleUnit = new CInfoPanelSingleUnit( sEvent.pLoader->GetControl( "infopanel_singleunit" ), pMission );
			pInfoPanelMultipleUnits = new CInfoPanelMultipleUnits( sEvent.pLoader->GetControl( "infopanel_multipleunits" ), pMission );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pEndOfTurn = GetUIWindow<CButton>( this, "endofturn" );
			pEndOfTurn->AddImageState( 0, NDb::GetUITexture( 379 ) );

			pStartOfTurn = GetUIWindow<CButton>( this, "startofturn" );
			pStartOfTurn->AddImageState( 0, NDb::GetUITexture( 469 ) );

			pBackgroundSingleUnit = GetUIWindow<CImage>( this, "background_single" );
			pBackgroundMultipleUnits = GetUIWindow<CImage>( this, "background_multi" );
			break;
		}
	}

	if ( CWindow::ProcessMessage( sEvent ) )
		return true;

	switch( sEvent.nEvent )
	{
	case EVENT_LBUTTONUP:
	case EVENT_LBUTTONDOWN:
	case EVENT_LBUTTONDBLCLK:
	case EVENT_RBUTTONUP:
	case EVENT_RBUTTONDOWN:
	case EVENT_RBUTTONDBLCLK:
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitPanel::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	int nCountSelected = pMission->CountSelected();

	bool bSignlePanel = false, bMultiPanel = false;
	if ( nCountSelected == 1 )
		bSignlePanel = true;
	else if ( nCountSelected > 1 )
		bMultiPanel = true;

	pEndOfTurn->SetStyle( STYLE_VISIBLE, !pMission->IsRealTime() );
	pStartOfTurn->SetStyle( STYLE_VISIBLE, pMission->IsRealTime() );

	pInfoPanelSingleUnit->SetStyle( STYLE_VISIBLE, bSignlePanel );
	pBackgroundSingleUnit->SetStyle( STYLE_VISIBLE, bSignlePanel );

	pInfoPanelMultipleUnits->SetStyle( STYLE_VISIBLE, bMultiPanel );
	pBackgroundMultipleUnits->SetStyle( STYLE_VISIBLE, bMultiPanel );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0521151, CUnitTab );
REGISTER_SAVELOAD_CLASS( 0xB0521152, CUnitsTabBar );
REGISTER_SAVELOAD_CLASS( 0xB0521153, CUnitFace );
REGISTER_SAVELOAD_CLASS( 0xB0521155, CInfoPanelSlot );
REGISTER_SAVELOAD_CLASS( 0xB0521156, CInfoPanelSingleUnit );
REGISTER_SAVELOAD_CLASS( 0xB0521157, CInfoPanelMultipleUnits );
REGISTER_SAVELOAD_CLASS( 0xB0521158, CUnitPanel );
REGISTER_SAVELOAD_CLASS( 0xB0521159, CSlotReloadButton );
REGISTER_SAVELOAD_CLASS( 0xB052115A, CInfoPanelSpecialSlot );
REGISTER_SAVELOAD_CLASS( 0xB0241943, CLevelSwitchBar );
REGISTER_SAVELOAD_CLASS( 0xB0241944, CInfoPanelCritical );
REGISTER_SAVELOAD_CLASS( 0xB3515150, CMissionCriticalBleedingFake );
